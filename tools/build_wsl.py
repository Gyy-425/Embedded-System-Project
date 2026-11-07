"""Build AAI05 diagnostics in Ubuntu. Does not flash or contact a broker."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
from network_config import validate_network_config

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sdk', default=os.environ.get('PICO_SDK_PATH'))
parser.add_argument('--label', help='unique prefix for evidence logs')
parser.add_argument('--motor-probe', action='store_true', help='coast-only PWM/timer diagnostic; no motor drive')
parser.add_argument('--network-probe', action='store_true', help='Wi-Fi/MQTT transport probe; forces coast-only motor mode')
parser.add_argument('--comms-app', action='store_true', help='binary target/ACK/raw-IR application; no motor output')
parser.add_argument('--build-only', action='store_true', help='compile/static checks only; defer executable functional tests')
parser.add_argument('--motor-focused', action='store_true', help='run only affected PWM/probe host checks and motor builds')
args = parser.parse_args()
args.network_probe = args.network_probe or args.comms_app
args.motor_probe = args.motor_probe or args.network_probe
if args.motor_focused and args.motor_probe:
    parser.error('--motor-focused uses the normal motor-output profile')
args.label = args.label or ('2026-11-07-v0_5' if args.comms_app else
                           '2026-11-06-v0_4' if args.network_probe else
                           '2026-11-06-v0_3p' if args.motor_probe else '2026-11-07-v0_6')
if re.fullmatch(r'[A-Za-z0-9_-]+', args.label) is None:
    parser.error('--label must contain only letters, digits, underscores or hyphens')
if not args.sdk:
    parser.error('pass --sdk /path/to/pico-sdk or set PICO_SDK_PATH')
sdk = Path(args.sdk).resolve()
for required in ['pico_sdk_init.cmake', 'lib/tinyusb/src/tusb.c',
                 'lib/cyw43-driver/src/cyw43_ctrl.c']:
    if not (sdk / required).is_file():
        parser.error(f'SDK dependency missing: {sdk / required}')
repo = Path(__file__).resolve().parents[1]
if args.network_probe:
    if not (sdk / 'lib/lwip/src/core/init.c').is_file():
        parser.error('SDK lwIP submodule is missing')
    try:
        validate_network_config(repo / 'config/wifi_secrets.h')
    except OSError:
        parser.error('create the private config/wifi_secrets.h before building')
    except ValueError as error:
        parser.error(str(error))
alias = Path('/tmp') / ('aai05-sdk-' + hashlib.sha256(str(sdk).encode()).hexdigest()[:10])
if alias.is_symlink():
    if alias.resolve() != sdk:
        raise SystemExit('SDK alias collision; no existing path was overwritten')
elif alias.exists():
    raise SystemExit('SDK alias is occupied; no existing path was overwritten')
else:
    alias.symlink_to(sdk, target_is_directory=True)
env = dict(os.environ, PICO_SDK_PATH=str(alias), LC_ALL='C')
logs = repo / 'testing/integration/logs'
logs.mkdir(parents=True, exist_ok=True)
artifacts = repo / 'output/firmware'
artifacts.mkdir(parents=True, exist_ok=True)
host_binary = Path('/tmp/aai05_encoder_test')
summary = {'baseline_commit': 'c3b09544cb1fd6dac6ad4b8f523be0af59b44482',
           'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=repo, text=True).strip(),
           'source_dirty': bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=repo, text=True).strip()),
           'application_version': '0.5' if args.comms_app else '0.4' if args.network_probe else '0.3p' if args.motor_probe else '0.6',
           'motor_pulse_duty_percent': 50, 'motor_pulse_duration_ms': 400,
           'motor_mode': 'NO_DRIVE_PROBE' if args.motor_probe else 'BUTTON_TRIGGERED_DRIVE',
           'network_mode': 'BINARY_COMMS_APP' if args.comms_app else 'WIFI_MQTT_TRANSPORT_PROBE' if args.network_probe else 'RADIO_ONLY',
           'functional_tests': 'DEFERRED_AT_USER_REQUEST' if args.build_only else 'HOST_ONLY',
           'test_scope': 'AFFECTED_MOTOR_PWM_AND_PROBE' if args.motor_focused else 'ALL',
           'running_pwm': 'DIRECT_DUTY_FIXED_LOW',
           'sdk': str(sdk), 'hardware_tests': 'NOT RUN', 'results': []}

def run(command, label, cwd=repo):
    log = logs / f'{args.label}-{label}.log'
    motor_checks = {'host-pulse-compile', 'host-pulse',
                    'host-probe-compile', 'host-probe'}
    if args.motor_focused and label.startswith('host-') and label not in motor_checks:
        summary['results'].append({'label': label, 'command': command,
                                   'exit_code': None, 'status': 'SKIPPED_UNCHANGED_SCOPE',
                                   'warnings': []})
        (logs / f'{args.label}-build-result.json').write_text(json.dumps(summary, indent=2))
        print(f'{label}: SKIPPED (unchanged subsystem)', flush=True)
        return
    if args.build_only and label.startswith('host-'):
        summary['results'].append({'label': label, 'command': command,
                                   'exit_code': None, 'status': 'DEFERRED',
                                   'warnings': []})
        (logs / f'{args.label}-build-result.json').write_text(json.dumps(summary, indent=2))
        print(f'{label}: DEFERRED (build-only request)', flush=True)
        return
    print(f'{label}: running; log={log}', flush=True)
    with log.open('w') as stream:
        result = subprocess.run(command, cwd=cwd, env=env, stdout=stream, stderr=subprocess.STDOUT)
    lines = log.read_text(errors='replace').splitlines()
    summary['results'].append({'label': label, 'command': command, 'exit_code': result.returncode,
                               'log': str(log.relative_to(repo)),
                               'warnings': [line for line in lines if 'warning:' in line]})
    (logs / f'{args.label}-build-result.json').write_text(json.dumps(summary, indent=2))
    if result.returncode:
        print('\n'.join(lines[-16:]), flush=True)
        raise SystemExit(result.returncode)
    print(f'{label}: PASS', flush=True)

run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined,address',
     '-Iapp_program/subsystem2', 'testing/host/test_encoder.c',
     'app_program/subsystem2/encoder_logic.c', '-o', str(host_binary)], 'host-compile')
run([str(host_binary)], 'host-encoder')
register_binary = Path('/tmp/aai05_rp2040_register_test')
run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined,address',
     'testing/host/test_rp2040_registers.c', '-o', str(register_binary)], 'host-register-compile')
run([str(register_binary)], 'host-register')
motor_binary = Path('/tmp/aai05_motor_test')
run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined,address',
     '-Iapp_program/subsystem2', 'testing/host/test_motor_startup.c',
     'app_program/subsystem2/drv_motor.c', '-o', str(motor_binary)], 'host-motor-compile')
run([str(motor_binary)], 'host-motor-startup')
capture_binary = Path('/tmp/aai05_capture_test')
run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined,address',
     '-Iapp_program/subsystem2', 'testing/host/test_encoder_capture.c',
     'app_program/subsystem2/encoder_capture.c', 'app_program/subsystem2/encoder_logic.c',
     '-o', str(capture_binary)], 'host-capture-compile')
run([str(capture_binary)], 'host-capture')
capture_startup_binary = Path('/tmp/aai05_capture_startup_test')
run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined,address',
     '-Iapp_program/subsystem2', 'testing/host/test_encoder_startup.c',
     '-o', str(capture_startup_binary)], 'host-capture-startup-compile')
run([str(capture_startup_binary)], 'host-capture-startup')
control_binary = Path('/tmp/aai05_motor_control_test')
run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined,address',
     '-Iapp_program/subsystem2', 'testing/host/test_motor_control.c',
     'app_program/subsystem2/motor_test.c', '-o', str(control_binary)], 'host-control-compile')
run([str(control_binary)], 'host-control')
pulse_binary = Path('/tmp/aai05_motor_pulse_test')
run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined,address',
     '-Iapp_program/subsystem2', 'testing/host/test_motor_pulse.c',
     '-o', str(pulse_binary)], 'host-pulse-compile')
run([str(pulse_binary)], 'host-pulse')
probe_binary = Path('/tmp/aai05_motor_probe_test')
run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined,address',
     '-DMOTOR_TEST_PROBE=1', '-Iapp_program/subsystem2', 'testing/host/test_motor_pulse.c',
     '-o', str(probe_binary)], 'host-probe-compile')
run([str(probe_binary)], 'host-probe')
run([sys.executable, '-m', 'unittest', 'discover', '-s', 'testing/host', '-p', 'test_network_config.py'],
    'host-network-config')
if args.comms_app:
    run(['gcc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-Wconversion',
         '-Wshadow', '-Wstrict-prototypes', '-c',
         'app_program/subsystem3/comm_protocol.c', '-o',
         '/tmp/aai05_comm_protocol.o'], 'comm-static-compile')
    run(['gcc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-Wconversion',
         '-Wshadow', '-Wstrict-prototypes', '-Iapp_program/subsystem3', '-c',
         'testing/host/test_comm_protocol.c', '-o',
         '/tmp/aai05_comm_checks.o'], 'comm-checks-static-compile')
    run([sys.executable, '-m', 'py_compile', 'tools/mqtt/robot_codec.py',
         'tools/mqtt/robot_cli.py', 'testing/host/test_comm_cli.py'],
        'cli-syntax-compile')
    run(['gcc', '-std=c99', '-Wall', '-Wextra', '-Werror',
         '-fsanitize=undefined,address', '-Iapp_program/subsystem3',
         'testing/host/test_comm_protocol.c',
         'app_program/subsystem3/comm_protocol.c', '-o',
         '/tmp/aai05_comm_test'], 'host-comm-compile')
    run(['/tmp/aai05_comm_test'], 'host-comm')
    env['COMM_VECTOR_FILE'] = str(logs / f'{args.label}-host-comm.log')
    run([sys.executable, '-m', 'unittest', 'discover', '-s', 'testing/host',
         '-p', 'test_comm_cli.py'], 'host-comm-cli')
profile = ['SMP=1', 'CONSOLE=usb_cdc', 'WIFI=cyw43',
           'NET=lwip' if args.network_probe else 'NET=none',
           'MQTT=1' if args.network_probe else 'MQTT=0', 'CRASH_REPORT=0',
           f'MOTOR_PROBE={int(args.motor_probe)}', f'COMM_APP={int(args.comms_app)}']
artifact_base = ('AAI05_Comms_v0_5_NO_DRIVE' if args.comms_app else
                 'AAI05_Network_Probe_v0_4_NO_DRIVE' if args.network_probe else
                 'AAI05_Motor_Probe_v0_3p_NO_DRIVE' if args.motor_probe else 'AAI05_Motor_PWM_v0_6')
for test in [0, 1]:
    run(['make', '-j8', *profile, f'TEST={test}'], f'firmware-test{test}', repo / 'build_make')
    stem = ('mtk3pico_smp1_usb_cdc_wifi' + ('_lwip_mqtt' if args.network_probe else '') +
            ('_probe' if args.motor_probe else '') +
            ('_comm' if args.comms_app else '') + ('_test' if test else ''))
    source = repo / 'build_make' / (stem + '.uf2')
    name = artifact_base + ('_TEST' if test else '')
    for extension in ['uf2', 'elf', 'map']:
        shutil.copy2(repo / 'build_make' / f'{stem}.{extension}', artifacts / f'{name}.{extension}')
    summary['results'][-1]['uf2'] = str((artifacts / f'{name}.uf2').relative_to(repo))
    summary['results'][-1]['sha256'] = hashlib.sha256(source.read_bytes()).hexdigest()
    summary['results'][-1]['size'] = subprocess.check_output(
        ['arm-none-eabi-size', str(repo / 'build_make' / f'{stem}.elf')], text=True).strip()
summary['compiler'] = subprocess.check_output(['arm-none-eabi-gcc', '--version'], text=True).splitlines()[0]
(logs / f'{args.label}-build-result.json').write_text(json.dumps(summary, indent=2))
print(f'Builds complete. {summary["motor_mode"]} image: {artifacts / (artifact_base + ".uf2")}', flush=True)
print('On-target tests remain pending; no firmware was flashed.', flush=True)
