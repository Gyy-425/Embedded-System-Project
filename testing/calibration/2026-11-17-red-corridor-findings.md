# Red lane calibration and geometry

User file `red-center.txt` contains three labelled sections, not just centre.
Original bytes and extracted sections are archived as red-corridor files here;
source hashes, exact142-row transcription and bounds are in
`../host/fixtures/rgb_2026_11_17_red_corridor_provenance.json`.

| Robot position | Paired rows | Left IR | Right IR | Clear |
|---|---:|---:|---:|---:|
| Centre |39|243–283|229–261|706–707|
| Left |53|510–540|203–240|703–710|
| Right |50|204–244|494–520|699–700|

All142 RGB rows are valid, unsaturated, current statusOK with no accumulated
errors, id0x44, gain4x,50400us integration and fresh at report time. Capture
modelR2 reports UNKNOWN as expected: that model has no calibrated red class.
All FOLLOW reports are IDLE/DRIVEOFF, PIO_BAD1; these are not powered red tests.

Normalized fractions across142 rows: R1681–1696, G1220–1227, B1178–1188.
The existing32/4095 margin yields R1649–1728/G1188–1259/B1146–1220.
Measured clear699–710 with25% margin yields524–888. The red fraction region
is separate from all four R3 profiles, whose bounds remain unchanged.
Previous unsupported-red rows retain their original expected UNKNOWN: those
earlier optical samples lie outside this new assembled-lane red calibration.
Recognition describes these calibrated tape/setup responses, not every red
material or lighting condition. This is calibration evidence, not held-out
accuracy or a guarantee of moving recognition.

The same left-border/right-border sign as yellow is demonstrated. Shared
yellow IR normalization gives zero integer steering for every centre row,
positive/right for every robot-left row and negative/left for every robot-right
row. Keep this scale instead of introducing an unrelated threshold change.
The user's35% duty request is applied independently, with gentle turn8.

User reports yellow following passed and smoother than the black stripe.
No new yellow moving log, route-error measurement or speed measurement was
supplied with this report. Red motion and the new yellow35 build remain to test.
