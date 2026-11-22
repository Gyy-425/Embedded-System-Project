# Additional blue/yellow IR capture — findings

Source: blue.txt, received 14 November 2026. Height, lighting and power are not recorded in this file; do not silently inherit previous conditions. The user describes these as different-coloured tapes of the same material; composition is not independently verified.

| Colour | Records | Left range | Left mean | Left sample SD | Right range | Right mean | Right sample SD |
|---|---:|---|---:|---:|---|---:|---:|
| Blue | 118 | 154–201 | 178.31 | 14.71 | 166–198 | 182.14 | 11.25 |
| Yellow | 113 | 108–188 | 162.35 | 15.99 | 148–189 | 166.71 | 11.77 |

All 231 records have valid, error-free IR data, age at most 100ms, and DRIVE OFF. No malformed timestamp lines or duplicate timestamps were found.

Blue minus yellow means: left 15.95, right 15.44 ADC counts. Overlap: left 154–188, right 166–189. Exact pairs 173/171 and 184/166 occur under both labels. No deterministic function of a single instantaneous pair can correctly label every recorded occurrence.

A single threshold per sensor, classifying higher readings as blue and fitted to maximise balanced accuracy on this same dataset, correctly labels 163/231 (70.56%) using left alone and 175/231 (75.76%) using right alone. These are descriptive in-sample results, not independent validation and not a limit on other classifiers. Thresholds were not deployed.

Conclusion: different average responses are present, but clean instantaneous separation is absent. Reliable moving recognition remains unvalidated. These data justify discussing feasibility and agreeing performance criteria; they do not establish universal impossibility. There is one block per colour and no white/background or moving validation in this file.
