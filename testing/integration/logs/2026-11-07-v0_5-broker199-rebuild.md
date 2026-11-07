# v0.5 broker-address rebuild

The coordinator requested a rebuild for actual communication after the successful host checks. The current WLAN IPv4 was rechecked using `ipconfig`:192.168.1.199. The private firmware setting still contained192.168.1.196.

Only `MQTT_BROKER_IP` in ignored `config/wifi_secrets.h` was changed to192.168.1.199. The edit verified an exact encoding round trip and that every other configuration byte was preserved. No application, port, build-script or host-test source was changed. Passwords were not printed or committed.

All six previous normal/TEST UF2/ELF/map artifacts were copied to `output/firmware/archive/v0_5-broker-192_168_1_196` and each copy was hash-verified before rebuilding. The archive and new binaries remain ignored/local.

Build command in existing Ubuntu WSL:

```sh
python3 tools/build_wsl.py --sdk "/mnt/c/Users/Guo YouYan/.pico-sdk/sdk/2.3.0" --comms-app --build-only --label 2026-11-07-v0_5-broker199-rebuild
```

Actual result: runner exit0. Strict C99 codec compile, prepared C test-source compile, Python syntax compile and both firmware compile variants passed, with no reported warnings. The earlier25-stage host run already passed against unchanged application code; executable host tests were not repeated for this address-only rebuild. Build-only DEFERRED entries describe skipped execution, not failed tests or a replacement for that recorded pass.

| Variant | Current artifact | SHA256 |
|---|---|---|
| Normal, use for Pico | `output/firmware/AAI05_Comms_v0_5_NO_DRIVE.uf2` | `c9d6e1c45331a83d9c928f436f6cbf8c37c6bbf5539fe9ca898d0f10788d8189` |
| TEST compile variant | `output/firmware/AAI05_Comms_v0_5_NO_DRIVE_TEST.uf2` | `61cd30108754d4f87be228328178c55f56cfdb8d7ddeab7bcd097955f2aef79c` |

Independent post-build verification checked each UF2 block's magic and payload size, reconstructed its payload bytes, confirmed192.168.1.199 present and192.168.1.196 absent, and matched the current file hash to the runner result. Evidence is in `2026-11-07-v0_5-broker199-artifact-check.json`; compiler logs/result JSON share the rebuild label.

No broker was started and no Pico was flashed or operated in this task. Actual Wi-Fi association, MQTT readiness/exchange and reconnect behavior remain pending. Continue at broker startup in [COMMS_TEST_V0_5.md](../../../docs/COMMS_TEST_V0_5.md), using the current normal image. Recheck the laptop address if its network connection changes.
