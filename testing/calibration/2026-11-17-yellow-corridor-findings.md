# Yellow bordered-lane captures

Raw centre, robot-left and robot-right files are archived beside this note.
Exact source and fixture hashes, extrema and model derivation are in
`../host/fixtures/rgb_2026_11_17_yellow_corridor_provenance.json`.

| Robot position | IR pairs | Left raw range | Right raw range | R2 YELLOW / UNKNOWN |
|---|---:|---:|---:|---:|
| Centre |29|240–274|224–256|30 /0 RGB rows|
| Left |40|470–511|209–242|29 /11|
| Right |39|203–238|480–511|21 /18|

Centre has one extra RGB-only row before its first IR report. Thus there are
109 RGB samples and108 timestamp-paired IR positions, not109 paired positions.
User labels describe the robot moving relative to the fixed path. This is
the reverse of plain-black-stripe response and requires a separate geometry.
The supplied values near500 are offset observations, not measured stability
or recovery boundaries. Proportional steering begins below500.

All109 RGB records are fresh, valid, unsaturated, id0x44, gain4x and50400us.
R2 matches80 and rejects29. Accumulated yellow data comprise101 original plus
109 new rows, with normalized fraction extrema R1552–1588/G1425–1433/B1075–1117.
Applying the previous32-count margin gives R1520–1620/G1393–1465/B1043–1149.
Keep clear737–1230 and every other class bound unchanged. The normalized
yellow region remains separated from other profiles; overlapping/nonmatching
inputs still reject rather than force a colour.

R3 replay passes all109 new rows and584 previous colour cases. This is model
adaptation evidence. The original yellow validation rows were reused and no
longer represent held-out accuracy for the revised yellow profile. Stationary
replay does not measure moving recognition or control quality.

All FOLLOW reports show IDLE, DRIVE=OFF and sticky PIO_BAD2. The old black
target gate explains rejection of correctly classified yellow; these logs do
not demonstrate a powered yellow run. Encoder warning timing cannot be
determined from these excerpts. Check a clean stationary reboot before motion.
