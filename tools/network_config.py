"""Private-header checks; errors identify fields without echoing their values."""
import ipaddress
import json
import re


def validate_network_config(path):
    text = path.read_text(encoding='utf-8-sig')

    def token(name):
        values = re.findall(r'^[ \t]*#[ \t]*define[ \t]+' + name + r'[ \t]+([^\r\n]+)', text, re.MULTILINE)
        if len(values) != 1:
            raise ValueError(f'{name}: define exactly once in the private configuration')
        return values[0].strip()

    def string(name):
        try:
            literal = token(name)
            # JSON's \/ and \u escapes are not generally valid C string escapes.
            # Use literal UTF-8 text for non-ASCII characters instead.
            if any(escape not in {'"', '\\', 'b', 'f', 'n', 'r', 't'}
                   for escape in re.findall(r'\\(.)', literal)):
                raise ValueError
            value = json.loads(literal)
        except (ValueError, TypeError):
            raise ValueError(f'{name}: use a valid quoted string; escape backslashes and quotes') from None
        if not isinstance(value, str) or not value or '\0' in value or '\n' in value or '\r' in value:
            raise ValueError(f'{name}: requires a nonempty single-line string')
        if value in {'FILL_IN_LOCALLY', 'your-network-name', 'your-network-password'}:
            raise ValueError(f'{name}: replace the local placeholder before building')
        return value

    string('WIFI_SSID')
    string('WIFI_PASSWORD')
    broker = string('MQTT_BROKER_IP')
    try:
        address = ipaddress.IPv4Address(broker)
        if address.is_loopback or address.is_unspecified or address.is_multicast:
            raise ValueError
    except ValueError:
        raise ValueError('MQTT_BROKER_IP: use the laptop LAN IPv4, not localhost') from None
    port = token('MQTT_BROKER_PORT')
    if not port.isascii() or not port.isdecimal() or not 1 <= int(port) <= 65535:
        raise ValueError('MQTT_BROKER_PORT: use an integer from 1 to 65535')
    string('MQTT_CLIENT_ID')
    if any(char in string('MQTT_TOPIC') for char in '+#'):
        raise ValueError('MQTT_TOPIC: heartbeat publish topic cannot contain wildcards')
