# Stationary tape calibration capture

This is the next prepared coding case after the recorded MQTT movement checks.
It uses the existing v0.8 REMOTE firmware and two AO/ADC channels. No reflash,
wiring change or Python package installation is required. The tool is prepared
and host-tested; actual tape capture/colour recognition has not passed yet.

The capture tool sends only STATUS requests. It never sends START, STOP, ARM
or movement renewals. It requires ready/output-off state, rejects movement,
faults or session changes and stops recording if fresh telemetry disappears
for2s. Do not run a drive client or operate motor buttons during capture. This
software guard checks reported output state; it is not a hardware interlock.
Device elapsed time is also compared with host monotonic elapsed time from
the initial STATUS: divergence over1s aborts rather than accepting an ordered
backlog. Deadline/gap checks run again after receive. This is a relative timing
guard, not a measured absolute network/sample-age guarantee. A delayed initial
STATUS or heavy host scheduling can conservatively abort; use a new capture.

## When the samples are available

1. Use the actual black/white/five selected tape samples and intended background.
   Put the car stationary at its normal installed sensor height over that surface.
   For each capture, both sensors must fully cover the same tape patch. Keep
   mounting, height, lighting and module adjustments consistent. Do not hold
   the car at a different height for each sample.
2. Start the usual broker, using the laptop's current address configured in
   firmware. Commands below use the established192.168.1.199 example. The
   Pico's own address may differ. Use v0.8/updated client; no drive process.
3. From PowerShell in `C:\Users\Guo YouYan\Desktop\INF2004-main\AAI05_Firmware`,
   place black material under both sensors and run:

```powershell
python .\tools\mqtt\calibrate_cli.py capture --host 192.168.1.199 --label black --name black --setup "installed bracket; room lighting; actual cardboard" --out .\testing\calibration\black-01
```

Replace the setup description with the actual conditions. Keep it identical
within one experiment. If height is actually measured, add `--height-mm` with
that measurement on every capture; otherwise omit it. No height is assumed.

Default capture:500ms settling, then40 fresh valid telemetry samples,30s total
deadline. The stream is nominally one packet per250ms, so capture commonly
takes about11s; actual timing depends on delivery/valid samples. These are tool
defaults, not professor acceptance targets or ADC sampling-rate measurements.
The deadline bounds accepting samples; file flushing/cleanup can finish later.

On success, the directory contains `samples.csv` and `metadata.json` marked
COMPLETE. CSV is written/flushed per accepted sample. Ctrl+C or a link/state
failure preserves partial data with an incomplete state. An abrupt process
termination can leave IN_PROGRESS. Existing output names are never overwritten;
use `black-02` for a separate repeat. Do not relabel one saved capture as another.

4. Repeat with white (`--label white --name white --out ...\white-01`) and
   the five selected colours (`--label colour1` through `colour5`). Give each
   its actual name, such as `--name "your selected tape name"`, and a separate
   output directory. IDs0/1 remain black/white; IDs2..6 are colour1..5. Names
   are your labels; readings have not established those colours automatically.
5. Produce an offline report in a new file:

```powershell
python .\tools\mqtt\calibrate_cli.py report --root .\testing\calibration --out .\testing\calibration\range-report-01.json
```

The report reads immediate capture directories under that root, combines
complete repeats of the same class/setup and lists excluded incomplete captures.
Different setup/height/device or inconsistent names for a class are rejected;
keep different physical experiments in different root folders. Reports are
exclusive files too: use a new name for each report.

## Reading the report

- INCOMPLETE: one or more of the seven classes has no complete capture.
- OVERLAP: all seven are present, but observed class intervals overlap on at
  least one sensor. The report names each pair and sensor; do not invent a
  threshold or commit to those tapes without investigating the measured overlap.
- CANDIDATE_RANGES: observed intervals are disjoint on both sensors. This is
  a candidate for a later classifier, not recognition accuracy or proof under
  changing height/light/movement. Exact ADC0/4095 hits are counted for inspection;
  zero is valid data, and a rail hit alone is not diagnosed as a hardware fault.

The files retain raw12-bit readings, sequence/time/session and physical setup.
They do not export a deployed calibration model or alter firmware. Captures
cover reported telemetry samples, not every ADC conversion. The later C
classifier still needs independent validation, unknown/uncertain handling,
background/edge data and moving transitions before path following. No tape
measurements, shortest-segment performance or classification accuracy are
invented by the supplied synthetic host tests.
