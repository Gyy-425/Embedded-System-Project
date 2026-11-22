# Initial six-colour separability findings

Measured source: 2026-11-12-six-colour-serial.txt, SHA256
8b37e56343c03e1b8a79d25d6c16a9e0014c5da899cf3f5d360221398121826f.
The source's old PENDING heading is obsolete; its recorded measurements are
preserved unchanged. Statistics are in 2026-11-12-six-colour-analysis.json.

680 parsed serial records in14 sections,41..60 samples per section. No
malformed/duplicate records, invalid IR/error/freshness failures, or DRIVE ON
records. All phases READY with duties0; READY is acceptable for this stationary
output-off analysis. No ADC0/4095 hits. Setup reports left17mm/right18mm,
room light with possible chassis shadow, USB-only power, fixed front bracket.
The height measurement reference (sensor face versus board/bracket) is not
specified. Future bank-powered/moving behaviour is not qualified here.

## Observed ranges, both repeats pooled

|Sample|Left ADC|Right ADC|
|---|---|---|
|Black|2492–2592|2514–2707|
|Red|173–228|196–231|
|Green|181–249|205–249|
|Blue|189–265|216–253|
|Orange|174–260|198–246|
|Yellow|140–228|195–227|
|Cardboard|187–260|209–261|

Black separates from all other recorded surfaces. All15 pairs among the five
non-black colours plus cardboard have overlapping intervals on each sensor.
There are39 distinct exact (left,right) pairs assigned to multiple surfaces.
Example:192/220 appears under both RED and YELLOW. A deterministic classifier
using only that instantaneous pair cannot always identify the recorded label.
This is not proof every possible material/height/classifier is infeasible.

Red/yellow pooled means differ by2.46 left and0.91 right counts, versus pooled
sample SD around16..18 left and10 right counts. No six-class thresholds should
be installed from this dataset. Strong black contrast does not establish
multi-colour recognition or coloured-path contrast against bare cardboard.

Right black mean shifts2688.17→2530.57 between repeats (157.60 counts), while
within-repeat SD is10.57/11.11. Left black mean shifts2532.76→2521.19. This
shows a repeatability concern, not a proven cause; geometry/patch/lighting or
other conditions require checking. The1mm left/right nominal height difference
is not by itself an explanation for class overlap; models are per-sensor.

## Next controlled experiment

1. Verify heights are sensor-face-to-tape distances; secure bracket and flat
   board, stable indoor illumination and consistent sample-centre positioning.
2. Screen red/yellow/black/cardboard at about3mm closer, then3mm farther than
   the recorded setup, moving both sensors consistently.20 consecutive raw
   records per surface/height suffice for an exploratory screen, not final
   calibration. Keep power/lighting/potentiometers fixed. These are engineering
   trial offsets, not a manufacturer-specified optimum or assured solution.
3. Select a promising common height only if gaps improve relative to noise
   and repositioning variation; then repeat full six-class capture and reserve
   independent validation captures. If still overlapping, compare alternate
   tape/material samples (white is an optional measured candidate), or consult
   the professor about achievable reflectance classes under the IR-only scope.
   Do not invent ranges, reduce the scope silently or substitute an RGB sensor
   as if the existing requirement had changed.

Separate diagnostics: all680 records show right PIO_BAD1/ENC_UNCERTAIN1; this
does not invalidate the IR ADC readings, but powered encoder qualification
must not be claimed from this capture. Wi-Fi never reaches LINK3; this is not
a reason to discard the deliberately USB-only IR experiment. Neither issue
has been diagnosed or fixed by this analysis. No calibration/firmware edited.
