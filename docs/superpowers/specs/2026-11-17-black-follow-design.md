# Bounded local black-stripe following

User authorises implementation and confirms geometry: both IR heads see
cardboard when centred; robot-left puts right IR on black and requires right
correction; robot-right puts left IR on black and requires left correction.
Keep35mm stripe, current mounting and BWY_20261117 RGB profiles.

Deliver a focused local prototype first: no network app, boot stationary,
fresh debounced GP20 starts at most1000ms of following, GP21 stops immediately.
Every stop requires a new stable release/press; no recovery/search/reverse or
automatic restart. Provide distinct NO_DRIVE and powered normal/TEST images.
MOTOR_PROBE enforces physical coast in the no-drive image, not just zero
commands. Radio infrastructure remains for SDK timing; MQTT profiles preserved.

Portable C core: centre baselines L268/R304; positive spans L1100/R1140.
Normalize max(0,raw-baseline)*1000/span, clamp1000. Error=right-left;
abs(error)<=80 -> neutral; else turn=error*20/1000, bounded20. Forward electrical
duty50 (the successfully tested rolling duty), mapped through existing chassis
polarity conversion. Duty is not speed. Two normalized channels >=600 is
ambiguous and stops. Raw bounds0..4095, valid IR age<=100ms, fresh healthy RGB
classification BLACK age<=200ms, and motor/encoder capture readiness required.
Healthy centered low/low requires RGB presence: low/low alone is not track loss
nor proof of line presence. New explicit invalid samples stop immediately.

Motion task remains sole motor writer. A new owner adapter caches only coherent
successful IR/RGB snapshots; a busy read retains original timestamp and must
expire normally, never refresh validity. Classification uses the existing core
and fixed exposure. Owner applies existing motion_control/chassis and motor
driver100ms hardware lease, checks independent driver cutoff and refuses silent
restart after stall/expiry. Only a fresh start can rearm coast outputs.

Review refinement: absolute raw-timer deadlines are validated inside protected
driver start/renew. Anchor hardware time before kernel time, subtract10ms for
clock quantization, and cap run/IR/RGB/command expiry. Existing relative driver
API remains compatible. Nominal1000ms may finish early; actual timing, coast
distance and WCET require measurement.

Separate FOLLOW_APP profile is mutually exclusive with existing Motion/remote,
RGB diagnostic, comm apps and networking. PIO encoders permitted with this
local profile. RGB acquisition enabled by RGB_DIAG or FOLLOW_APP, never two
owners. Driver I2C1 and kernel tick guards preserved. No shared kernel changes.

New FOLLOW snapshot copy uses a short interrupt-preserving SMP spinlock,
covering only copying. This avoids polling-Motion snapshot failures seen in
the diagnostic mutex reader. Existing legacy snapshot policy stays unchanged.
Report IR availability independently; do not suppress healthy IR data because
another snapshot is unavailable. RGB reporting must not say DRIVE=OFF in a
powered profile; actual output state comes from Motion's copied motor status.

Acceptance: TDD core steering/sign/stop/debounce/deadline/wrap tests, owner
integration with real motion/chassis policies and substituted hardware/kernel
boundaries, snapshot concurrency/copy checks, actual-record replay, strict
sanitizers, prior host regression, guarded profile rejection, distinct firmware
builds and preserved prior UF2 hashes. Wheel-raised and ground tests NOT RUN.
One-second limitation and no-MQTT status must be clear in guide and boot banner.
