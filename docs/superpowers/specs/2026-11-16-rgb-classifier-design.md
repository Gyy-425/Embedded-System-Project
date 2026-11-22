# RGB classifier: stationary recognition milestone

The user authorised building the RGB classifier using black, white tape 1 and
yellow captures, while a teammate prepares a black steering track. Work stays
in the existing solo checkout. No navigation, motor, MQTT, kernel or wiring
behaviour changes belong to this milestone.

Use a portable C11 core with immutable, configurable profiles. Each profile
accepts measured clear intensity and three integer colour fractions
round(channel * 4095 / (R+G+B)). A unique matching profile yields BLACK,
WHITE, YELLOW or BACKGROUND. No match yields UNKNOWN; multiple matches yield
UNKNOWN/AMBIGUOUS. This approach rejects unsupported colours rather than always
choosing the nearest centroid. Raw RGBC centroids were considered but rejected
for deployment because the fresh cardboard capture changes brightness.

Generate provisional bounds from the 205 first-pass calibration records only:
fraction min/max with 32/4095 absolute margin on each channel, and clear
min/max with 25 percent outward margin (integer rounding outwards). These are
engineering starting tolerances, not statistically established confidence
limits. Validation and red-strip captures are replay evidence, never training.
Keep gain 4x and integration 50400us; refuse mismatched exposure, stale/invalid
data, bad ID, saturation and malformed models. Zero RGB sum yields NO_MATCH.

Result is one byte: class bits 0..2 (black0, white1, yellow2, background3,
unknown7), reason bits 3..5, reserved bits 6..7 zero. These RGB IDs are local;
they are not a change to the old IR or MQTT protocols. Model bounds use native
uint16_t for 12-bit fractions and raw clear counts, with uint32_t timestamps.
No packed-struct wire format, heap, floating point or mutable shared model.

Attach classification to a copied RGB diagnostic snapshot outside the mutex.
Preserve the raw RGB line and add a separate readable RGB_CLASS line; no
temporal filtering or confirmed transition claims yet. New v0.12 normal/TEST
images remain USB-only and motor-disabled under existing profile guards.

Acceptance: strict/sanitized C edge tests, source-labelled replay statistics,
bad-model/ambiguous/unknown/dark/exposure/freshness/wrap tests, target compile,
existing host regression suite and normal/TEST diagnostic builds plus remote
PIO compatibility build. Preserve all earlier UF2 files. New live stationary
recognition and moving use remain NOT RUN until the user tests the image.
