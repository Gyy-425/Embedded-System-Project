# v0.8 MQTT Motion preparation,7 November2026

Software prepared and host-verified; **hardware tests NOT RUN**. No firmware
was uploaded, broker started or robot actuated during this development task.
Prior physical evidence remains the recorded v0.7 LOCAL pass.

## Implementation and review

v2 binary START/KEEPALIVE/STOP connects the network owner to typed Motion
ARM/submit/STOP. Owner-only rearm waits for coast, healthy hardware, released
GP21 and an epoch acknowledgement. GP20 is disabled in REMOTE. Session/grant
and sequence checks reject stale/replayed mutations; duplicates cannot renew
the 750ms remote lease. Internal Motion/driver lease remains100ms. Reconnect
clears authorization and cannot start motion. C uses fixed-width types,
explicit byte encoding and the scoped BARR-C conventions/strict checks.

One fresh whole-feature review found two Important issues; no Critical or Minor
findings. The network loop lacked periodic REMOTE service and did not service
observed DOWN. Actual port-helper fixtures reproduced both failures, then
passed after wiring service for both application profiles and link states.
The client started its duration budget after handshake and could exceed it
while waiting for renewal; two fake-clock regressions reproduced the overrun.
The budget now starts before START and caps handshake/renewal waits. All final
regressions pass. Cleanup/network/physical stop latency remains a separate
limit; no hard physical duration bound is claimed.

Review RED evidence:
- `2026-11-07-v0_8-review-owner-poll-red.log`
- `2026-11-07-v0_8-review-observed-down-red.log`
- `2026-11-07-v0_8-review-duration-red.log`

Earlier failing tests also cover the new compact codec, controller, owner and
driver rearm, client cleanup and short-lease renewal. Fixture compile/setup
corrections are not counted as feature-failure evidence.

## Final reproducible verification

From Windows PowerShell in the firmware repository:

```powershell
wsl.exe -d Ubuntu --cd '/mnt/c/Users/Guo YouYan/Desktop/INF2004-main/AAI05_Firmware' -- python3 tools/build_wsl.py --sdk '/mnt/c/Users/Guo YouYan/.pico-sdk/sdk/2.3.0' --remote-app --motor-focused --label NEW_UNIQUE_LABEL
```

Actual final label: `2026-11-07-v0_8-remote-final`.
[Machine-readable result](2026-11-07-v0_8-remote-final-build-result.json).
Source base `0660fdd3a634cc14e56ac8856f1090f512f4e5d7` with working-tree v0.8 changes
(`source_dirty=true`); this evidence is saved with the implementation commit.
Compiler: `arm-none-eabi-gcc (15:13.2.rel1-2) 13.2.1 20231009`. SDK2.3.0 parser hash is pinned by the runner
before REMOTE build; no external SDK edits were made.

**31 executed stages passed, zero compiler warnings.** These include actual
Motion runtime in LOCAL and REMOTE configurations, real driver MMIO fixtures,
portable v2 codec/control/service, actual MQTT port metadata/fragment handling
with SDK types in both profiles, cyclic owner service,14 v2 Python tests with
C byte vectors,8 legacy v1 Python tests with C vectors, and both firmware builds.
C functional fixtures run with ASAN/UBSAN; new portable modules use strict
conversion/shadow/prototype checks. These are host fixtures, not real radio,
interrupt timing or robot measurements.11 unchanged-scope stages were explicitly
skipped; they are not counted as passes. Earlier preliminary logs accidentally
skipped the legacy communication group; the final runner includes and passes it.

Payload sizes are14-byte commands,23-byte ACKs and 26-byte telemetry. The final
normal ELF reports text 340640, data 100, bss 82576; TEST reports text 341196, data 100,
bss 82576. These are current linked sizes, not equivalent-function RAM savings.
Week13 representation savings and measured correctness/performance remain open.

## Local artifacts

| Variant | File | SHA256 |
|---|---|---|
| Normal | `output/firmware/AAI05_MQTT_Motion_v0_8_REMOTE.uf2` | `d80d95ad4b7c364198e293942cef1bb8a46bf8202fb138b580460092dae5ba0a` |
| TEST | `output/firmware/AAI05_MQTT_Motion_v0_8_REMOTE_TEST.uf2` | `0b002d4b11266aedf3ec4a1fd947e63e20ec38cd465e409b3c9d0999ca0b7728` |

All 22 pre-existing UF2 files match their pre-development hashes.
[Preservation manifest](2026-11-07-v0_8-preserved-artifacts.json).
Firmware binaries, real Wi-Fi credentials and runtime broker files stay local
and ignored. This task does not push or merge the feature branch.

## Next evidence

Use [the focused car guide](../../../docs/MQTT_MOTION_V0_8.md): bounded MQTT
left-wheel movement and explicit rearm, GP21 during motion, then one broker-loss
actuation check with no automatic restart. Do not replay the old basic tests.
No physical v0.8 result, accurate powered encoder feedback, forward-direction
calibration, simultaneous two-wheel load, tape classification, measured WCET
or network/physical STOP latency is claimed. Trusted-LAN session/grants are not
authentication. No additional hardware photos are needed for this case.
