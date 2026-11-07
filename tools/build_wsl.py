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
parser.add_argument('--motion-app', action='store_true', help='Motion interface with bounded local exercise; no MQTT actuation')
parser.add_argument('--remote-app', action='store_true', help='v2 MQTT Motion with explicit fresh START/STOP; disables local producer')
parser.add_argument('--build-only', action='store_true', help='compile/static checks only; defer executable functional tests')
parser.add_argument('--motor-focused', action='store_true', help='run only affected controller/PWM/probe checks and motor builds')
args = parser.parse_args()
if args.remote_app and (args.motion_app or args.comms_app or args.network_probe or args.motor_probe):
    parser.error('--remote-app selects its own Motion/network profile; do not combine app selectors')
if args.remote_app:
    args.motion_app = True
    args.network_probe = True
if args.motion_app and not args.remote_app and (args.comms_app or args.network_probe or args.motor_probe):
    parser.error('--motion-app cannot be combined with no-drive/network profiles')
args.network_probe = args.network_probe or args.comms_app
args.motor_probe = args.motor_probe or (args.network_probe and not args.remote_app)
if args.motor_focused and args.motor_probe:
    parser.error('--motor-focused uses the normal motor-output profile')
args.label = args.label or ('2026-11-07-v0_8-remote' if args.remote_app else
                           '2026-11-07-v0_7-motion' if args.motion_app else
                           '2026-11-07-v0_5' if args.comms_app else
                           '2026-11-06-v0_4' if args.network_probe else
                           '2026-11-06-v0_3p' if args.motor_probe else '2026-11-07-v0_6_1')
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
parser_contract = None
if args.remote_app:
    mqtt_parser = sdk / 'lib/lwip/src/apps/mqtt/mqtt.c'
    parser_contract = hashlib.sha256(mqtt_parser.read_bytes()).hexdigest()
    if parser_contract != '71bb51547bf2f81b324d5fa796df367076fb98bc8a0eb687a1450f7eedfcf98a':
        parser.error('SDK MQTT parser changed; review the retained-header callback contract before building REMOTE')
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
           'application_version': '0.8' if args.remote_app else '0.7' if args.motion_app else '0.5' if args.comms_app else '0.4' if args.network_probe else '0.3p' if args.motor_probe else '0.6.1',
           'motor_pulse_duty_percent': 50, 'motor_pulse_duration_ms': None if args.motion_app else 400,
           'motor_mode': 'REMOTE_LEASED_MOTION' if args.remote_app else 'LOCAL_LEASED_MOTION' if args.motion_app else 'NO_DRIVE_PROBE' if args.motor_probe else 'BUTTON_TRIGGERED_DRIVE',
           'motion_command_ttl_ms': 100 if args.motion_app else None,
           'network_mode': 'BINARY_REMOTE_MOTION_V2' if args.remote_app else 'BINARY_COMMS_APP' if args.comms_app else 'WIFI_MQTT_TRANSPORT_PROBE' if args.network_probe else 'RADIO_ONLY',
           'mqtt_parser_contract_sha256': parser_contract,
           'remote_lease_ms': 750 if args.remote_app else None,
           'remote_grant_ms': 1000 if args.remote_app else None,
           'remote_wire_sizes': [14, 23, 26] if args.remote_app else None,
           'functional_tests': 'DEFERRED_AT_USER_REQUEST' if args.build_only else 'HOST_ONLY',
           'test_scope': 'AFFECTED_MQTT_MOTION_AND_LEGACY' if args.remote_app and args.motor_focused else 'AFFECTED_MOTOR_CONTROLLER_PWM_AND_PROBE' if args.motor_focused else 'ALL',
           'encoder_warning_policy': 'NONFATAL_BOUNDED_OPEN_LOOP_ONLY',
           'running_pwm': 'DIRECT_DUTY_FIXED_LOW',
           'sdk': str(sdk), 'hardware_tests': 'NOT RUN', 'results': []}

def run(command, label, cwd=repo):
    log = logs / f'{args.label}-{label}.log'
    motor_checks = {'host-control-compile', 'host-control',
                    'host-motion-compile', 'host-motion',
                    'host-motion-runtime-compile', 'host-motion-runtime',
                    'host-pulse-compile', 'host-pulse',
                    'host-probe-compile', 'host-probe'}
    is_remote_check = args.remote_app and label.startswith(('host-drive-', 'host-remote-', 'host-mqtt-', 'host-comm'))
    if args.motor_focused and label.startswith('host-') and label not in motor_checks and not is_remote_check:
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
motion_binary = Path('/tmp/aai05_motion_test')
run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wconversion',
     '-Wshadow', '-Wstrict-prototypes', '-fsanitize=undefined,address',
     '-Iapp_program/subsystem2', 'testing/host/test_motion_control.c',
     'app_program/subsystem2/motion_control.c',
     'app_program/subsystem2/motion_button.c', '-o', str(motion_binary)],
    'host-motion-compile')
run([str(motion_binary)], 'host-motion')
runtime_binary = Path('/tmp/aai05_motion_runtime_test')
run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wconversion',
     '-Wshadow', '-Wstrict-prototypes', '-fsanitize=undefined,address',
     '-pthread', '-Iapp_program/subsystem2', '-Iinclude',
     'testing/host/test_motion_runtime.c',
     'app_program/subsystem2/motion_control.c',
     'app_program/subsystem2/motion_button.c', '-o', str(runtime_binary)],
    'host-motion-runtime-compile')
run([str(runtime_binary)], 'host-motion-runtime')
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
if args.remote_app:
    strict_host = ['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                   '-Wconversion', '-Wshadow', '-Wstrict-prototypes',
                   '-fsanitize=undefined,address']
    run([*strict_host, '-pthread', '-DAAI05_REMOTE_APP=1',
         '-Iapp_program/subsystem2', '-Iinclude',
         'testing/host/test_motion_runtime.c',
         'app_program/subsystem2/motion_control.c',
         'app_program/subsystem2/motion_button.c', '-o', '/tmp/aai05_remote_runtime'],
        'host-remote-runtime-compile')
    run(['/tmp/aai05_remote_runtime'], 'host-remote-runtime')
    run([*strict_host, '-Iapp_program/subsystem3',
         'testing/host/test_drive_protocol.c',
         'app_program/subsystem3/drive_protocol.c',
         'app_program/subsystem3/comm_protocol.c', '-o', '/tmp/aai05_drive_protocol'],
        'host-drive-protocol-compile')
    run(['/tmp/aai05_drive_protocol'], 'host-drive-protocol')
    env['DRIVE_VECTOR_FILE'] = str(logs / f'{args.label}-host-drive-protocol.log')
    run([*strict_host, '-Iapp_program/subsystem3',
         'testing/host/test_drive_control.c',
         'app_program/subsystem3/drive_control.c', '-o', '/tmp/aai05_drive_control'],
        'host-drive-control-compile')
    run(['/tmp/aai05_drive_control'], 'host-drive-control')
    run([*strict_host, '-DAAI05_REMOTE_APP=1', '-Iapp_program/subsystem3',
         '-Iapp_program/subsystem2', 'testing/host/test_drive_service.c',
         'app_program/subsystem3/mqtt_drive_app.c',
         'app_program/subsystem3/drive_control.c',
         'app_program/subsystem3/drive_protocol.c',
         'app_program/subsystem3/comm_protocol.c', '-o', '/tmp/aai05_drive_service'],
        'host-drive-service-compile')
    run(['/tmp/aai05_drive_service'], 'host-drive-service')
    for remote in [0, 1]:
        name = 'remote' if remote else 'legacy'
        run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror',
             '-fsanitize=undefined,address', f'-DAAI05_REMOTE_APP={remote}',
             f'-DAAI05_COMMS_APP={1-remote}', '-Itesting/host/mqtt_fixture',
             '-Ilib/liblwip/sysdepend/pico_rp2040',
             '-I' + str(alias / 'lib/lwip/src/include'),
             'testing/host/test_mqtt_ingress.c', '-o', f'/tmp/aai05_mqtt_{name}'],
            f'host-mqtt-{name}-compile')
        run([f'/tmp/aai05_mqtt_{name}'], f'host-mqtt-{name}')
    run([sys.executable, '-m', 'unittest', 'discover', '-s', 'testing/host',
         '-p', 'test_drive_*.py'], 'host-drive-python')
run([sys.executable, '-m', 'unittest', 'discover', '-s', 'testing/host', '-p', 'test_network_config.py'],
    'host-network-config')
if args.comms_app or args.remote_app:
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
           f'MOTOR_PROBE={int(args.motor_probe)}', f'COMM_APP={int(args.comms_app)}',
           f'MOTION_APP={int(args.motion_app)}', f'REMOTE_APP={int(args.remote_app)}']
artifact_base = ('AAI05_MQTT_Motion_v0_8_REMOTE' if args.remote_app else
                 'AAI05_Motion_v0_7_LOCAL' if args.motion_app else
                 'AAI05_Comms_v0_5_NO_DRIVE' if args.comms_app else
                 'AAI05_Network_Probe_v0_4_NO_DRIVE' if args.network_probe else
                 'AAI05_Motor_Probe_v0_3p_NO_DRIVE' if args.motor_probe else 'AAI05_Motor_PWM_v0_6_1')
for test in [0, 1]:
    run(['make', '-j8', *profile, f'TEST={test}'], f'firmware-test{test}', repo / 'build_make')
    stem = ('mtk3pico_smp1_usb_cdc_wifi' + ('_lwip_mqtt' if args.network_probe else '') +
            ('_probe' if args.motor_probe else '') +
            ('_comm' if args.comms_app else '') + ('_motion' if args.motion_app else '') +
            ('_remote' if args.remote_app else '') +
            ('_test' if test else ''))
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
