# Yellow corridor following

User-authorized scope: the same bounded local GP20/GP21 trial as the black
follower, on a yellow lane with black borders, using the supplied centre,
robot-left and robot-right captures. No new wiring or live MQTT mode switching.

## Approach

Reuse the single Motion owner, snapshot acquisition, STOP/rearm policy and
absolute motor cutoff. Select the geometry at build time; default remains
plain black. Copying an entire owner would duplicate the timing fixes, while
selecting steering polarity from each noisy RGB frame could reverse control.
An explicit yellow build is the smallest suitable extension.

Yellow mode uses per-channel centre baselines L255/R236 and spans245/264,
placing full normalized response at raw500 for either sensor. This implements
the user's approximate500 observation as gradual steering, not a binary jump.
Left IR rising means robot left of the corridor: positive turn/right; right
rising means negative turn/left. The existing black mode retains its opposite
sign and its original baselines, spans and ambiguity threshold.

Yellow both-normalized1000 is ambiguous and stops. This is a conservative
prototype assumption for simultaneous border contact; the supplied stationary
data do not prove that every such position is unrecoverable. Both-low is only
allowed while fresh RGB matches YELLOW; it cannot prove lane position alone.
Keep D40/T8, one-second trial, no reverse/search and no automatic restart.

Adapt only YELLOW chromatic bounds using all labelled yellow rows from the
original capture plus the new109 rows and the existing32/4095 margin. Keep
yellow clear bounds and all other colour bounds unchanged. UNKNOWN/ambiguity,
health, gain/integration and freshness rejection remain. Mark reused rows as
adaptation; they are not independent accuracy evidence. Report modelR3 and
explicit track/target to prevent flashing or interpreting the wrong mode.

## Validation and limits

Replay every new RGB row and108 paired IR positions, including measured
left/right steering signs; rerun584 prior colour rows. Exercise GP21, release
and rearm, wrong colour, stale/invalid sensors, both-high, deadline, timestamp
wrap and delayed output. Compile black and yellow owners; build powered and
NO_DRIVE yellow firmware and compatibility images. Preserve old UF2 hashes.
All supplied captures show sticky PIO_BAD2; a fresh stationary reboot must be
checked before powered testing. No moving yellow result, physical speed,
STOP distance or sustained curve performance is claimed.
