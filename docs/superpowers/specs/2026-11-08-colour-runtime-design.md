# IR classifier runtime and reporting

The user requested runtime integration while actual tapes are unavailable,
after basic ground motion bring-up. Continue directly in the existing
aai05/bring-up checkout; no hardware actuation, upload, push or merge.
This is one reporting case, not target-following or a complete robot controller.

## Approach

Use the existing IR task as acquisition/classification owner. Copy a public
boot calibration into its private runtime once; no live setters or MQTT model
mutation. Default calibration is explicitly missing. Keep the tested v2
commands/raw telemetry unchanged; add a separate read-only colour topic.
Classifying at 250ms telemetry rate could erase short segments, so classify
every acquired sample and queue transitions. Adding a new task or modifying
the proven motion protocol would add scheduling/compatibility work without
helping this case. These are engineering decisions, not professor targets.
Direct execution follows the user's selected 'prepare runtime integration'
and prior request for focused coding with fewer redundant handoffs.

## Contract

- C11/BARR-C style, integers, no heap. IR owns the model and sample updates.
  Existing IR mutex protects runtime snapshot and transition queue across
  cores. Network readers use TMO_POL; no network calls while holding the lock.
- Model max_age_ms=100 by default (engineering setting). Complete disjoint
  models on both sensors are accepted; completely absent models are allowed.
  Partial, overlapping or invalid deployment models yield INVALID_MODEL and
  never guessed colours. Store no synthetic bounds in firmware.
- Per-sensor results remain distinct: neither voting nor a fused ground-truth
  colour is justified before track geometry/calibration. Sample errors stay
  invalid. Unsigned timestamp subtraction handles wrap for short lifetimes.
- Classify each acquisition, no time debounce. First result and any change in
  either packed class/reason generate an event; unchanged readings update the
  latest snapshot only. Event sequence is uint32 modulo; sample timestamp/raw
  values are retained with each transition. A 16-event ring retains newest
  events; overwriting oldest increments a saturating uint32 lost-event count.
  This finite queue cannot guarantee arbitrary transition rates/downtime.
- Latest snapshots are reclassified at read time so stalled sampling reports
  stale rather than a permanently cached match. Historical events remain
  historical, with their original sample timestamps and results.
- Network owner uses the existing REMOTE MQTT session. On a new link it first
  clears pre-session queued events, then publishes a latest snapshot. It
  drains at most one event per poll, holds one pending event on publication
  rejection, and emits a snapshot every 1000 ms when idle. Retry is bounded per
  poll; link loss clears pending data. Colour publication runs after motion
  service so it cannot bypass STOP/expiry. No movement requests originate here.
- Topic: aai05/colour/v1/robot01/telemetry, QoS0/non-retained. Payload23 bytes:
  byte0 D0 event/D1 snapshot; bytes1..4 session;5..8 transition sequence;
  9..12 sample timestamp;13..16 queue drops;17..19 two12-bit raw values;
  20 left packed result;21 right packed result;22 existing CRC8.
  Integers are big-endian. Result bits7..6 zero; MATCH has class0..6 and all
  other reasons have unknown7. No native struct is sent.
- A read-only laptop watch decodes strictly, prints per-sensor class/reason,
  identifies event gaps/session changes and warns of absent fresh arrivals.
  It never publishes START, STATUS, STOP or calibration commands. Raw v2 watch
  remains supported and still labels its own unavailable detected field.
- Serial reports include current per-sensor results/reasons/drop count.
  These are diagnostic observations, not accuracy/WCET/short-segment proof.

## Validation and limits

Use synthetic fixtures only in host tests: no-model, valid and rejected models,
two-sensor disagreement, one-sample intermediate colour, sample failure/stale,
wrap, queue overflow/saturation, publication failure and session reset. Exercise
the real IR adapter with kernel/ADC substitutes and the real reporting service
with publication substitutes. Cross-check C frames with Python decoding and
malformed input rejection. Run full existing host suite and PIO/GPIO normal/
TEST firmware builds under distinct candidate paths, preserving old UF2 hashes.
No actual calibration, recognition accuracy, moving tape run, Wi-Fi timing,
loss-free delivery or integrated control acceptance is claimed.
