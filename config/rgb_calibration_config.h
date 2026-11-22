/* Provisional RGB profiles, with R4 bounds retained and blue added in R5. */
#ifndef AAI05_RGB_CALIBRATION_CONFIG_H
#define AAI05_RGB_CALIBRATION_CONFIG_H

#include "../app_program/subsystem1/rgb_classifier.h"

#ifndef AAI05_RGB_HOME_PROFILE
#define AAI05_RGB_HOME_PROFILE 0
#endif
#if FOLLOW_HOME_TRACK && !AAI05_RGB_HOME_PROFILE
#error The home track calibration requires the home RGB profile
#endif
#ifndef AAI05_RGB_ROOM2
#define AAI05_RGB_ROOM2 0
#endif
#if (AAI05_RGB_ROOM2 != 0) && (AAI05_RGB_ROOM2 != 1)
#error AAI05_RGB_ROOM2 must be 0 or 1
#endif
#if AAI05_RGB_ROOM2 && !FOLLOW_HOME_TRACK
#error The ROOM2 lighting profile applies only to the home track
#endif

/* Source SHA256: 0a27fc36c62323a577baffe65ac7ad35041e9a84d4437a30ce989376f3baff66
 * Base profiles:205 first-pass rows; background red upper1495 adapted17-Nov.
 * R2 BLACK chromatic box uses177 labelled black rows:104 original,
 * 51 actual-track and22 flicker captures, with32/4095 margin on each channel.
 * R3 YELLOW chromatic box uses210 labelled rows:101 original and109 assembled
 * corridor positions, with32/4095 margin. Other class bounds and every clear
 * bound remain unchanged. Original reused yellow validation rows are adaptation.
 * See rgb_2026_11_17_yellow_corridor_provenance.json for sources/hash/bounds.
 * Reused original repositioned rows are now adaptation, not held-out accuracy.
 * See rgb_2026_11_17_black_flicker_provenance.json for sources/hash/bounds.
 * R4 RED uses142 labelled assembled-lane rows,32/4095 chromatic margin and
 * 25% clear margin. All four R3 profiles remain unchanged. Sources/bounds:
 * rgb_2026_11_17_red_corridor_provenance.json. Not held-out moving accuracy.
 * R5 BLUE uses196 labelled assembled-lane rows,32/4095 chromatic margin and
 * 25% clear margin. See rgb_2026_11_19_blue_corridor_provenance.json.
 * HOME_R1 extends R5 red and yellow around their labelled home captures
 * with16/4095 chromatic and5% clear margins. Other profiles use R5.
 * See rgb_2026_11_19_home_provenance.json for source and observed ranges.
 * HOME_ROOM2 (22-Nov, AAI05_RGB_ROOM2=1): new room lighting. BLUE and
 * BACKGROUND home boxes are translated by the shift between the old home
 * captures and the ROOM2 captures (blue +50/-41/-11, clear x1.314;
 * cardboard +65/-35/-30, clear x1.438), rounded outward, so the blue/UNKNOWN
 * edge sits at the same relative place the home-track logic was tuned on.
 * RED/YELLOW/BLACK/WHITE are NOT recaptured in ROOM2 and are unchanged.
 * See rgb_2026_11_22_room2_provenance.json.
 * Engineering margins, not established statistical confidence. */
static const rgb_classifier_model_t g_rgb_calibration = {
    .gain = 4U, .integration_us = 50400U, .max_age_ms = 200U,
    .present_mask = 0x3FU,
    .profiles = {
        {.lower = {1319U, 1366U, 1255U},
         .upper = {1432U, 1461U, 1352U},
         .clear_lower = 495U, .clear_upper = 828U},
        {.lower = {1337U, 1372U, 1286U},
         .upper = {1404U, 1438U, 1352U},
         .clear_lower = 1071U, .clear_upper = 1787U},
#if AAI05_RGB_HOME_PROFILE
        {.lower = {1503U, 1393U, 1043U},
         .upper = {1620U, 1465U, 1177U},
         .clear_lower = 606U, .clear_upper = 1230U},
#else
        {.lower = {1520U, 1393U, 1043U},
         .upper = {1620U, 1465U, 1149U},
         .clear_lower = 737U, .clear_upper = 1230U},
#endif
#if AAI05_RGB_ROOM2
        /* ROOM2 cardboard measured 1491..1493/1337..1338/1265..1267, clear 861..866. */
        {.lower = {1479U, 1313U, 1195U},
         .upper = {1560U, 1383U, 1270U},
         .clear_lower = 838U, .clear_upper = 1477U},
#else
        {.lower = {1414U, 1348U, 1225U},
         .upper = {1495U, 1417U, 1300U},
         .clear_lower = 583U, .clear_upper = 1027U},
#endif
#if AAI05_RGB_HOME_PROFILE
        {.lower = {1574U, 1188U, 1146U},
         .upper = {1728U, 1277U, 1262U},
         .clear_lower = 524U, .clear_upper = 888U},
#else
        {.lower = {1649U, 1188U, 1146U},
         .upper = {1728U, 1259U, 1220U},
         .clear_lower = 524U, .clear_upper = 888U},
#endif
#if AAI05_RGB_ROOM2
        /* ROOM2 blue measured 1289..1294/1358..1363/1441..1444, clear 628..631. */
        {.lower = {1253U, 1330U, 1399U},
         .upper = {1324U, 1406U, 1494U},
         .clear_lower = 599U, .clear_upper = 1021U},
#else
        {.lower = {1203U, 1371U, 1410U},
#if FOLLOW_HOME_TRACK
         /* Blue tape on the new home track measured 1492..1496 on B. */
         .upper = {1273U, 1446U, 1504U},
#else
         .upper = {1273U, 1446U, 1488U},
#endif
         .clear_lower = 456U, .clear_upper = 777U},
#endif
    }
};

#endif
