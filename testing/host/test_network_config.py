"""Validate private configuration without leaking it into a compiler diagnostic."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from network_config import validate_network_config

SAMPLE = '''#define WIFI_SSID "synthetic-network"
#define WIFI_PASSWORD "SYNTHETIC_TEST_SECRET"
#define MQTT_BROKER_IP "192.0.2.1"
#define MQTT_BROKER_PORT 1883
#define MQTT_CLIENT_ID "aai05-test"
#define MQTT_TOPIC "aai05/bringup/heartbeat"
'''


class NetworkConfigTests(unittest.TestCase):
    def check_config(self, content):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'wifi_secrets.h'
            path.write_text(content, encoding='utf-8')
            return validate_network_config(path)

    def test_valid_and_escaped_password(self):
        self.assertIsNone(self.check_config(SAMPLE))
        self.assertIsNone(self.check_config(SAMPLE.replace('SYNTHETIC_TEST_SECRET', r'a\\b\"c')))

    def test_missing_placeholder_and_bad_quoting_rejected_without_secret(self):
        for replacement in ['', 'FILL_IN_LOCALLY', 'your-network-password', 'SYNTHETIC_TEST_SECRET"broken']:
            with self.subTest(replacement=replacement), self.assertRaises(ValueError) as caught:
                self.check_config(SAMPLE.replace('SYNTHETIC_TEST_SECRET', replacement))
            self.assertNotIn('SYNTHETIC_TEST_SECRET', str(caught.exception))
        with self.assertRaises(ValueError):
            self.check_config(SAMPLE.replace('#define WIFI_PASSWORD', '#define OTHER_VALUE'))

    def test_invalid_broker_port_and_topic_rejected(self):
        for old, new in [('192.0.2.1', '999.1.1.1'), ('1883', '0'), ('1883', '65536'),
                         ('aai05/bringup/heartbeat', 'aai05/#')]:
            with self.subTest(value=new), self.assertRaises(ValueError):
                self.check_config(SAMPLE.replace(old, new))

    def test_json_only_escapes_do_not_reach_c_compiler(self):
        for escape in [r'\ud800', r'\u0041', r'\/', r'\x41']:
            with self.subTest(escape=escape), self.assertRaises(ValueError):
                self.check_config(SAMPLE.replace('SYNTHETIC_TEST_SECRET', 'synthetic' + escape))


if __name__ == '__main__':
    unittest.main()
