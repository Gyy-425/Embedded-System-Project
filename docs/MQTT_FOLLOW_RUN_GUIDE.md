# MQTT colour follow and distance: build and run guide

This is the current release path: colour-lane following between black borders, started and stopped over MQTT, with encoder distance reported per colour.

## 1. Build

Build in WSL (Ubuntu) with the pinned Pico SDK 2.3.0:

- `config/wifi_secrets.h` must hold your Wi-Fi network and the laptop broker IP (copy `config/wifi_secrets.example.h`).
- That file is git-ignored. **The UF2 contains these credentials**, so never commit or share the image.

```powershell
wsl.exe -d Ubuntu -- bash -lc "cd '/mnt/c/Users/Guo YouYan/Desktop/INF2004-main/AAI05_Firmware/tools' && python3 build_follow_mqtt.py --sdk '/mnt/c/Users/Guo YouYan/.pico-sdk/sdk/2.3.0' --label <new_label>"
```

- **Output:** `output/firmware/<new_label>/AAI05_MQTT_Follow_Distance_v1.uf2`, plus `build.log`.
- **Steering profile:** the build uses the frozen v0.87 profile listed in `tools/build_follow_mqtt.py` (`STEERING`, `PROFILE`).
- **Existing images are safe:** the script refuses to overwrite an existing label.

## 2. Start the broker and the client

Check the laptop's LAN IPv4 with `ipconfig`; it must match `MQTT_BROKER_IP` in the image. Leave the broker terminal open:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\mqtt\Start-Broker.ps1 -BindAddress <laptop-ip>
```

In a second terminal:

```powershell
python .\tools\mqtt\drive_cli.py --host <laptop-ip> menu
```

## 3. Menu commands (type, then press Enter)

| Key | Action |
|---|---|
| `f` | Follow the whole multicolour route, joining each new lane automatically (frozen v0.87 behaviour) |
| `fb` / `fr` / `fy` | Follow only the blue / red / yellow lane, selected over Wi-Fi. The car must start on that lane or the start is refused (`TARGET_MISMATCH`). It stops when another lane colour is confirmed (`TARGET_END`). |
| `l` | Straight-track distance measurement (no bend manoeuvres) |
| `s` | Stop |
| `?` | Status |
| `r` | Run report: colour sections and their distances |
| `q` | Stop and quit |

**Safety:**
- GP21 on the car always stops.
- The car stops within 100 ms if the network owner stops renewing motor permission.
- It stops within 750 ms if the laptop stops renewing the run.
- Reconnecting never restarts the car; a new run needs a new `f`, `fb`, `fr`, `fy` or `l`.

## 4. What to record for each run

Keep these with the run so `testing/system/TEST_LOG.md` can link them:

1. The USB serial log (`FOLLOW[...]`, `BEND[...]` and `NET[...]` lines). Save it to a new file in `testing/integration/logs/`. Do not overwrite earlier logs.
2. The client output, especially the run report from `r`.
3. The commit and image label, the start lane, the command used, and the outcome.
