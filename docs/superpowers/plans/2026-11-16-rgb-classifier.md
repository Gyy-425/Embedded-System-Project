# RGB classifier implementation plan

Spec: ../specs/2026-11-16-rgb-classifier-design.md
Execution: inline solo, as requested; current aai05/bring-up checkout.

## Global constraints
C SDK / micro T-Kernel integration; BARR-C style for new C; no motor output,
network, wire-protocol or shared infrastructure changes; no hardware claims.

## Review focus
Fresh cardboard drift; unseen red rejection; overlapping configurable profiles;
uint32 timestamp wrap/future timestamps; snapshot failure cannot reuse a label.

## Tasks and ledger
- [x] 1. Capture source-labelled replay fixtures and immutable model provenance.
- [x] 2. Write classifier behaviour tests; observe RED, implement portable core,
  then strict/sanitized GREEN and all recorded-sample replay.
- [x] 3. Test diagnostic output and failure handling first; add RGB_CLASS output
  after the unchanged raw line, outside any mutex. Add strict target rules.
- [x] 4. Run existing host regressions and distinct normal/TEST v0.12 plus
  remote PIO builds. Hash old UF2s, review changes and document user test steps.

Ruling: the explicit build request authorises this already-discussed classifier
milestone; proceed without another permission round. Preserve the established
solo checkout and uncommitted work; no automatic commit or push.

Interface: rgb_classify(model, snapshot, now_ms, gain, integration_us) returns
rgb_classification_t uint8_t; rgb_classification_class/reason decode it.
rgb_default_calibration() returns a const model compiled from labelled data.
Tests use literal expected classes and reasons, never a second classifier.

Pre-flight: diagnostic snapshot contains RGBC/time/health but not exposure;
the consumer supplies the fixed driver constants explicitly. Model is const,
so task readers need no new mutex. Classification performs no I/O.

Task 1: complete. Fixture header parsing was checked against all eight source
block headings; exactly205 first-pass rows and200 repositioned rows retained.
Independent review recomputed all four model bounds and verified hashes.
Task 2: complete. Initial test failed because classifier header was absent;
strict/ASan/UBSan GREEN and489-row replay passed. No training rows filtered.
Task 3: complete. Reporting test first failed because RGB_CLASS was absent.
GREEN verified raw output, label, busy UNKNOWN and no mutex held during output.
Review minor fixes: retain exposure metadata in replay vectors; add a failing
stale-validity regression, then make otherwise healthy expired samples report
STALE consistently even if the producer already cleared valid.
Task 4: complete. Final normal/TEST v0.12 and remote PIO builds, expanded
boundary tests and69-stage existing regression passed, zero compiler warnings.
73 earlier UF2s unchanged. Independent review plus minor-fix re-review found no
Critical/Important defect. See final evidence and manifest. Hardware NOT RUN;
no flash, commit or push performed.
