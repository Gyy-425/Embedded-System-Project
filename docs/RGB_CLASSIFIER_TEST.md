# RGB classifier v0.12.1: stationary check

Historical first-classifier guide: the current following firmware v0.13.3 uses
the R2 black profile. See BLACK_PROFILE_R2.md for the latest recognition check.
The original v0.12.1 UF2 is preserved and does not contain that later correction.

This milestone recognises the specific BLACK, WHITE TAPE 1 and YELLOW samples,
with BACKGROUND and UNKNOWN outcomes. It does not enable movement, MQTT RGB
telemetry or path following. Use the normal `AAI05_RGB_Classifier_v0_12_1.uf2`,
not the `_TEST` or remote compatibility images.

## Upload and observe

1. Close Serial Monitor. Switch the car off and disconnect bank VIN and Pico
   USB before handling it. Keep the established GP2/GP3 RGB and IR wiring.
2. Hold Pico BOOTSEL while connecting laptop USB, release it once RPI-RP2
   appears, and copy the normal v0.12.1 UF2 to that drive. Allow it to reboot.
3. Switch the Robo Pico ON if needed and open its serial port at 115200.
   Use laptop USB only for this stationary check, as for calibration.
4. Keep optical height, exposure, LED state and indoor lighting unchanged.
   Cover the RGB chip and its illumination area with the same uniform sample.
   Wait about one second after each repositioning; keep hands out of the view.
5. Check BLACK, WHITE, YELLOW, then bare matching CARDBOARD. Save 5–10 complete
   raw RGB and RGB_CLASS line pairs for each, labelled with the sample name.
6. Optional red strip should yield UNKNOWN/NO_MATCH. Red is not a trained class;
   do not reinterpret it as black just because raw clear intensity is similar.

Example (values are illustrative, not a new measurement):

```text
RGB[t=... ready=1 id=0x44 seq=... R=... G=... B=... C=... has=1 valid=1 sat=0 age=... status=0 errors=0 gain=4x integration_us=50400 DRIVE=OFF]
RGB_CLASS[t=... sample=... seq=... class=BLACK reason=MATCH code=0x00 read=1 age=... model=BWY_20261117 DRIVE=OFF]
```

The raw line remains unchanged. The legacy `COLOUR[L=7/1 R=7/1]` on IR status
lines still refers to the uncalibrated IR classifier, not this RGB result.
RGB_CLASS is a report-time classification of a copied observation, not a
confirmed colour-transition event or one label for every acquisition. There is
no temporal smoothing; boundary mixtures may return UNKNOWN/AMBIGUOUS.

Do not press the carrier's four hardware motor-test buttons: they can drive
the motors independently of software. No motor test is part of this check.

## Meaning and limits

- MATCH: exactly one enabled profile accepts all three colour fractions and
  clear intensity. BACKGROUND is the cardboard reference, not a fourth tape.
- NO_MATCH: no profile accepts the observation, including zero RGB sum.
- AMBIGUOUS: two or more profiles accept it. Report UNKNOWN; do not select one.
- INVALID: missing/failed/saturated/bad-ID acquisition, absent snapshot or
  invalid fresh data. Historical acquisition errors do not prevent recovery.
- STALE: otherwise healthy observation is older than the configured 200ms.
- EXPOSURE / BAD_MODEL / UNCALIBRATED: mismatched exposure, malformed model or
  empty model respectively; all yield UNKNOWN.
- `read=0` means the snapshot was unavailable. No previous label is reused.

If a uniform sample repeatedly gives UNKNOWN or a wrong label, send a short
labelled excerpt with raw values. Do not repeatedly collect hundreds of rows,
change height or expand ranges until every colour passes: that can erase
separation and conceal ambiguity. Tape edges, a different background, new
lighting and motor-powered motion still require validation.

## Calibration and representation

`config/rgb_calibration_config.h` is an immutable, locally configurable native
model. Profiles are indexed BLACK0, WHITE1, YELLOW2, BACKGROUND3. For each
channel, fraction = round(channel*4095/(R+G+B)). Base bounds come from205
first-pass records, with provisional padding32/4095 per fraction and25% on
clear intensity. v0.12.1 adjusts only BLACK red lower1367→1352 and BACKGROUND
red upper1486→1495 using the labelled17-Nov track captures. Each changed bound
includes8/4095 beyond the new observed extremum. These engineering margins
are not a confidence percentage; other bounds remain unchanged.
Gain4x and nominal50400us exposure are required. The classifier explicitly
rejects data at or beyond the driver saturation count21504.

Model validation precedes exposure, acquisition health, freshness and matching.
The producer may already clear valid on expiry; otherwise healthy expired data
still reports STALE. Timestamp ages use unsigned32 subtraction across wrap,
assuming observations are from the current acquisition timeline. Future or
normally old timestamps reject. Sequence0 can be a legitimate wrapped sequence.

The result uses one uint8_t: class bits0..2, reason bits3..5, two reserved zero
bits. The model uses uint16_t for fraction/count bounds and uint32_t time,
without C bitfields or a packed shared structure. This is not a changed MQTT
format, nor completion of the Week13 equivalent-function size comparison.
The core has no heap, floating point, blocking, I/O or mutable global model.

## Replay evidence and reproduction

`testing/host/fixtures/rgb_2026_11_16.csv` retains the labelled205 calibration,
200 repositioned,61 unsupported red and23 fresh cardboard observations.
Its adjacent provenance JSON records source hashes and exact model bounds.
The separate `rgb_2026_11_17_track.csv` and provenance preserve26 initial black,
25 repositioned black and22 cardboard records used for adaptation. These73
records are regression/adaptation evidence, not held-out accuracy evidence.
The tolerances were chosen after exploratory inspection; replay is not a blind
assessment, hundreds of independent placements or a moving accuracy result.

Run in Ubuntu/WSL with a new unique label:

```text
python3 tools/check_rgb_diag.py --label NEW_LABEL --firmware --sdk /path/to/pico-sdk
```

This runs strict/sanitized C tests, recorded-data replay, ARM compilation and
distinct v0.12.1 normal/TEST plus remote PIO compatibility builds. It never
flashes hardware or modifies earlier images. The shared kernel is unchanged.
