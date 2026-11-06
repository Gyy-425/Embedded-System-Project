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

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sdk', default=os.environ.get('PICO_SDK_PATH'))
parser.add_argument('--label', default='2026-11-06-v0_3', help='unique prefix for evidence logs')
args = parser.parse_args()
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
           'application_version': '0.3',
           'sdk': str(sdk), 'hardware_tests': 'NOT RUN', 'results': []}

def run(command, label, cwd=repo):
    log = logs / f'{args.label}-{label}.log'
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
profile = ['SMP=1', 'CONSOLE=usb_cdc', 'WIFI=cyw43', 'NET=none', 'MQTT=0', 'CRASH_REPORT=0']
for test in [0, 1]:
    run(['make', '-j8', *profile, f'TEST={test}'], f'firmware-test{test}', repo / 'build_make')
    stem = 'mtk3pico_smp1_usb_cdc_wifi' + ('_test' if test else '')
    source = repo / 'build_make' / (stem + '.uf2')
    name = 'AAI05_Motor_Test_v0_3' + ('_TEST' if test else '')
    for extension in ['uf2', 'elf', 'map']:
        shutil.copy2(repo / 'build_make' / f'{stem}.{extension}', artifacts / f'{name}.{extension}')
    summary['results'][-1]['uf2'] = str((artifacts / f'{name}.uf2').relative_to(repo))
    summary['results'][-1]['sha256'] = hashlib.sha256(source.read_bytes()).hexdigest()
    summary['results'][-1]['size'] = subprocess.check_output(
        ['arm-none-eabi-size', str(repo / 'build_make' / f'{stem}.elf')], text=True).strip()
summary['compiler'] = subprocess.check_output(['arm-none-eabi-gcc', '--version'], text=True).splitlines()[0]
(logs / f'{args.label}-build-result.json').write_text(json.dumps(summary, indent=2))
print(f'Builds complete. Button-triggered motor-test image: {artifacts / "AAI05_Motor_Test_v0_3.uf2"}', flush=True)
print('On-target tests remain pending; no firmware was flashed.', flush=True)
