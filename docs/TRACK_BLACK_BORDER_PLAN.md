# Black guide borders and RGB identification: prototype plan

Proposed on16 November2026 for the next physical session. This is a prototype
layout and test plan, not demonstrated following or a claim of professor approval.
Current v0.12 firmware remains motor-disabled; a following controller and its
integration with Motion/MQTT are still required.

## Geometry to build first

On the matching cardboard, make a removable30–50cm straight white or yellow
lane, with a nominal35mm clear coloured width BETWEEN the inner edges of two
black borders. Each black border should initially be10mm wide, giving about
55mm total width. These dimensions are engineering starting values derived
from34mm measured IR-head spacing, not a sensor-manufacturer specification.

Keep the two inner boundaries parallel. When centred, each IR optical head
should be close to its respective inner boundary, while the RGB chip looks at
the middle. Do not centre both IR heads on the middles of broad black rails:
both readings could then saturate at their black response and lose small-offset
information. The complete44.63mm board assembly width is not the guide spacing.

Keep all mounting and heights unchanged initially. Verify RGB on the ASSEMBLED
lane: black borders may contaminate its field of view at the25.37mm optical gap.
The RGB chip is13.5mm behind the IR row, so its colour-transition time differs.
If geometry must change, change one thing at a time and recheck both signals.

Do not construct the full board before the short straight passes. Then extend
to about1m and add one broad curve without intersections. Start with white and
yellow, which have current RGB profiles; red remains unsupported/UNKNOWN.

## Black is a special case

Black centre plus black borders forms one wide black area. It removes the
inner black/colour contrast that makes this corridor work. Use a separate
approximately35mm plain black stripe on cardboard for the first black-follow
test. Stripe and corridor modes require opposite steering polarity and must
be explicitly configured while stopped; do not silently switch mode from
noisy per-frame colour labels.

Do not put black centre segments into the first white/yellow corridor and
assume continuous guidance is preserved. A later common all-colour layout
could use contrasting guard bands, but needs new geometry/optical tests.

## Physical check before motion

With read-only v0.12 and DRIVE=OFF:
1. Record5–10 paired IR/RGB_CLASS observations with the lane centred.
2. Move the lane about5mm left; record5–10. Repeat about5mm right.
3. For a corridor shifted left relative to the robot, the right sensor should
   see more black and the left less; the required correction is left. Reverse
   the lane offset and check the opposite response. Verify this experimentally
   rather than assuming raw ADC equality or identical channel sensitivities.
4. Confirm the RGB label is the centre colour, not BLACK/UNKNOWN, at the
   intended centred position. Covering the chip with a loose uniform sample
   alone is not acceptance of this assembled lane.
5. Repeat the short check for the other supported centre colour. Stop here if
   opposite offsets cannot be separated or borders disrupt colour recognition.

## Software work after geometry passes

1. Calibrate each IR channel's black/light response from the actual lane.
   Implement a bounded corridor steering controller from normalized readings,
   with explicit track-loss/invalid/stale handling. Keep plain-stripe polarity
   separate. No unbounded spinning, reversal or search in the first version.
2. Integrate the RGB acquisition/classifier with the existing motion and MQTT
   image; the current RGB diagnostic build explicitly rejects powered use.
   Retain one motor writer, command expiry and latched STOP protection.
3. Define two behaviours: report successive centre colours while guiding; and
   follow a specified target colour on a manually aligned lane, with bounded
   confirmation of a mismatch/loss before stopping. Selecting a target does
   not yet locate an arbitrary lane or choose a branch.
4. Prove steering sign with the drive wheels raised, then make a short low-duty
   straight run on bank power, without a dragging laptop USB cable. Duty is not
   calibrated speed. Verify STOP, then add distance and one broad curve.
5. Show the selected colour, fresh reports and observed automatic corrections.
   Record actual behaviour and remaining limits; do not claim full following
   from a timed open-loop straight run.

## Basis and uncertainty

User logs establish strong black IR contrast and poor red/cardboard positional
contrast. Labelled RGB replay supports black/white/yellow/background recognition,
but live classifier and moving performance remain unaccepted. Reflectance
edge contrast and height sensitivity are general principles also illustrated
in Pololu's application note https://www.pololu.com/docs/0J13/3 ; its specific
voltages/distances do not describe these blue IR modules.
