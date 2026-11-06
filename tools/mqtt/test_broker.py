"""Check the real workspace broker locally; does not flash or contact the Pico."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import socket
import subprocess
import time
import uuid

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--label', required=True, help='unique evidence filename prefix')
args = parser.parse_args()
if re.fullmatch(r'[A-Za-z0-9_-]+', args.label) is None:
    parser.error('label must use letters, digits, hyphens or underscores')
root = Path(__file__).resolve().parent
repo = root.parents[1]
runtime = root / '.runtime' / 'mosquitto-2.1.2'
logs = repo / 'testing' / 'integration' / 'logs'
logs.mkdir(parents=True, exist_ok=True)
log_path = logs / f'{args.label}.log'
json_path = logs / f'{args.label}.json'
if log_path.exists() or json_path.exists():
    raise SystemExit('Evidence label exists; choose a new label. Nothing overwritten.')
for name in ['mosquitto.exe', 'mosquitto_pub.exe', 'mosquitto_sub.exe']:
    if not (runtime / name).is_file():
        raise SystemExit(f'Missing workspace runtime: {name}')
with socket.socket() as available:
    available.bind(('127.0.0.1', 1883))  # Do not interfere with an existing listener.
config = root / '.runtime' / 'host-check.conf'
config.write_text('listener 1883 127.0.0.1\nallow_anonymous true\npersistence false\nlog_dest stdout\n', encoding='ascii')
topic = 'aai05/bringup/hostcheck'
token = 'aai05-local-' + uuid.uuid4().hex
flags = getattr(subprocess, 'CREATE_NO_WINDOW', 0)
result = {'test': 'INT-108', 'scope': 'laptop loopback only; Pico NOT TESTED',
          'protocol': 'MQTT3.1.1', 'address': '127.0.0.1:1883',
          'topic': topic, 'status': 'FAIL',
          'broker_sha256': hashlib.sha256((runtime / 'mosquitto.exe').read_bytes()).hexdigest()}
with log_path.open('w', encoding='utf-8') as stream:
    broker = subprocess.Popen([str(runtime / 'mosquitto.exe'), '-c', str(config), '-v'],
                              stdout=stream, stderr=subprocess.STDOUT, creationflags=flags)
    try:
        deadline = time.monotonic() + 5
        while True:
            if broker.poll() is not None:
                raise RuntimeError('Broker exited before readiness; see log')
            try:
                with socket.create_connection(('127.0.0.1', 1883), timeout=0.2):
                    break
            except OSError:
                if time.monotonic() >= deadline:
                    raise RuntimeError('Broker did not become ready within five seconds')
                time.sleep(0.05)
        common = ['-h', '127.0.0.1', '-p', '1883', '-V', 'mqttv311', '-q', '1', '-t', topic]
        subprocess.run([str(runtime / 'mosquitto_pub.exe'), *common, '-r', '-m', token],
                       check=True, timeout=8, stdout=stream, stderr=stream, creationflags=flags)
        received = subprocess.run([str(runtime / 'mosquitto_sub.exe'), *common, '-C', '1', '-W', '5', '-v'],
                                  check=True, timeout=8, capture_output=True, text=True, creationflags=flags)
        if received.stdout.strip() != f'{topic} {token}':
            raise RuntimeError('Subscriber did not receive the exact published payload')
        subprocess.run([str(runtime / 'mosquitto_pub.exe'), *common, '-r', '-n'],
                       check=True, timeout=8, stdout=stream, stderr=stream, creationflags=flags)
        result.update(status='PASS', received=received.stdout.strip(),
                      retained_test_message='received and then cleared; not a robot command')
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        broker.terminate()
        try:
            broker.wait(timeout=3)
        except subprocess.TimeoutExpired:
            broker.kill()
            broker.wait(timeout=3)
        result['test_broker_stopped'] = broker.poll() is not None
        json_path.write_text(json.dumps(result, indent=2), encoding='utf-8')
print(f'PASS: real MQTT3.1.1 local publish/subscribe; evidence: {json_path}')
print('Test broker stopped. Start-Broker.ps1 starts a fresh broker; Pico/LAN tests remain pending.')
