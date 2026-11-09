# Colour runtime broker-address rebuild

9 November 2026. While preparing requested test instructions, ipconfig showed
the laptop WLAN IPv4 is192.168.1.210; the preceding candidate used206.
Changed only MQTT_BROKER_IP in the ignored private header, preserving all
other private settings without printing credentials. Application source stays
at c1cc45f. Calibration remains absent. No hardware/network test was started.

Rebuilt only the required normal PIO profile, using the same flags as the
earlier fully verified runtime. Full host/matrix tests were not replayed for
this private-address-only change. The private header validator passed and the
normal build exited0 with zero compiler warnings. All50 earlier UF2 hashes
were unchanged. See the build log and -result.json beside this file for exact
arguments, source commit, image hash and preservation manifest.

Image: output/firmware/AAI05_Colour_Runtime_v0_10_BROKER210.uf2.
No upload performed. Broker, firmware and CLI must all use the laptop's actual
LAN address; changing only the CLI cannot repair an older compiled broker IP.
[Stationary test procedure](../../../docs/COLOUR_RUNTIME_CAR_CHECK.md).
