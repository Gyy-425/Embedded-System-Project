################################################################################
# micro T-Kernel 3.0 BSP makefile
#     Target Board: Raspberry Pi Pico
################################################################################

################################################################################
# Execution model
#
#   SMP=1  (default)  dual core.  TK_MAX_CORE is 2.
#   SMP=0             single core.  TK_MAX_CORE is 1, the per-core arrays
#                     collapse and the current-core index folds to a constant
#                     zero, so the generated code is equivalent to the
#                     pre-SMP port.
#
# Passed to the assembler as well: the dispatcher indexes the per-core arrays
# by SIO CPUID and needs the same profile.
################################################################################

SMP ?= 1

ifneq ($(SMP),0)
ifneq ($(SMP),1)
$(error Unknown SMP '$(SMP)'; use 0 or 1)
endif
endif

# Console default is needed here because the image name includes it.
#
# Defaults to usb_cdc so the console appears on the Pico's own USB cable.
# usb_cdc still mirrors every line to UART0 (GP0 TX / GP1 RX) for early boot
# and panic messages, so GP0/GP1 stay reserved either way.  CONSOLE=uart drops
# the USB console and its TinyUSB dependency.
CONSOLE ?= usb_cdc

# Pico W radio.  Defaults to cyw43: the radio is powered up and owned by a
# core-0 polling service task, using the Pico SDK's cyw43 driver.  WIFI=none
# leaves the radio powered down.
WIFI ?= cyw43
ifneq ($(filter $(WIFI),none cyw43),$(WIFI))
$(error Unknown WIFI '$(WIFI)'; use none or cyw43)
endif

ifeq ($(WIFI),cyw43)
WIFI_SUFFIX := _wifi
# SMP=0 is supported: the radio service task simply drops its TP_PRC1 affinity
# request when TK_SUPPORT_SMP is off (see cyw43_utk_kernel.c), the same way the
# USB console task already did. Everything touching the radio and lwIP still
# runs on that one task, which is what docs/RELEASE.md safety rule 6 actually
# requires -- single-core satisfies it trivially.
endif

# Optional lwIP network stack on top of WIFI=cyw43. NO_SYS=1 (raw API, no
# tcpip thread): everything that calls into lwIP -- cyw43_poll(),
# sys_check_timeouts(), and any application tcp_*/udp_*/dns_* call -- runs
# on the same processor-1-owned task that already services the radio, per
# docs/RELEASE.md safety rule 6. See lib/liblwip/sysdepend/pico_rp2040/.
NET ?= none
ifneq ($(filter $(NET),none lwip),$(NET))
$(error Unknown NET '$(NET)'; use none or lwip)
endif
ifeq ($(NET),lwip)
ifneq ($(WIFI),cyw43)
$(error NET=lwip requires WIFI=cyw43)
endif
NET_SUFFIX := _lwip
endif

WIFI_AUTH ?= default
ifneq ($(filter $(WIFI_AUTH),default wpa3),$(WIFI_AUTH))
$(error Unknown WIFI_AUTH '$(WIFI_AUTH)'; use default or wpa3)
endif
ifeq ($(WIFI_AUTH),wpa3)
ifneq ($(NET),lwip)
$(error WIFI_AUTH=wpa3 requires NET=lwip)
endif
WIFI_AUTH_SUFFIX := _wpa3
endif

# Optional MQTT client on top of NET=lwip. Connects and publishes from the
# same processor-1-owned task as everything else above -- see
# lib/liblwip/sysdepend/pico_rp2040/mqtt_utk.c. Broker address and topic
# come from config/wifi_secrets.h (gitignored; copy from
# config/wifi_secrets.example.h), same as the WiFi credentials.
MQTT ?= 0
ifneq ($(filter $(MQTT),0 1),$(MQTT))
$(error Unknown MQTT '$(MQTT)'; use 0 or 1)
endif
ifeq ($(MQTT),1)
ifneq ($(NET),lwip)
$(error MQTT=1 requires NET=lwip)
endif
MQTT_SUFFIX := _mqtt
endif

# TEST=1 builds the subsystem self-tests into the image (APP_TEST=1 in C).
# The image gets a _test suffix so it cannot be mistaken for the normal one.
TEST ?= 0
ifneq ($(filter $(TEST),0 1),$(TEST))
$(error Unknown TEST '$(TEST)'; use 0 or 1)
endif
ifeq ($(TEST),1)
TEST_SUFFIX := _test
endif

# MOTOR_PROBE=1 keeps motor inputs in coast while exercising the pulse timer.
# Distinct image/profile names prevent accidentally reusing powered objects.
MOTOR_PROBE ?= 0
ifneq ($(filter $(MOTOR_PROBE),0 1),$(MOTOR_PROBE))
$(error Unknown MOTOR_PROBE '$(MOTOR_PROBE)'; use 0 or 1)
endif
ifeq ($(MOTOR_PROBE),1)
MOTOR_PROBE_SUFFIX := _probe
endif

# Communication-only application: never permit a motion-enabled build.
COMM_APP ?= 0
ifneq ($(filter $(COMM_APP),0 1),$(COMM_APP))
$(error Unknown COMM_APP '$(COMM_APP)'; use 0 or 1)
endif
ifeq ($(COMM_APP),1)
ifneq ($(NET)$(MQTT)$(MOTOR_PROBE),lwip11)
$(error COMM_APP=1 requires NET=lwip MQTT=1 MOTOR_PROBE=1)
endif
COMM_APP_SUFFIX := _comm
endif

# Opt-in open-loop Motion/local exercise, separate from legacy pulse/MQTT.
REMOTE_APP ?= 0
ifneq ($(filter $(REMOTE_APP),0 1),$(REMOTE_APP))
$(error Unknown REMOTE_APP '$(REMOTE_APP)'; use 0 or 1)
endif
ifeq ($(REMOTE_APP),1)
ifneq ($(NET)$(MQTT)$(MOTOR_PROBE)$(COMM_APP)$(MOTION_APP),lwip1001)
$(error REMOTE_APP=1 requires NET=lwip MQTT=1 MOTOR_PROBE=0 COMM_APP=0 MOTION_APP=1)
endif
REMOTE_APP_SUFFIX := _remote
endif
MOTION_APP ?= 0
ifneq ($(filter $(MOTION_APP),0 1),$(MOTION_APP))
$(error Unknown MOTION_APP '$(MOTION_APP)'; use 0 or 1)
endif
ifeq ($(MOTION_APP),1)
ifneq ($(REMOTE_APP),1)
ifneq ($(NET)$(MQTT)$(MOTOR_PROBE)$(COMM_APP),none000)
$(error MOTION_APP=1 requires NET=none MQTT=0 MOTOR_PROBE=0 COMM_APP=0)
endif
endif
MOTION_APP_SUFFIX := _motion
endif

# CRASH_REPORT=1 also copies every console line to UART0 on GP0 (TX) /
# GP1 (RX), 115200 8N1.  UART output is sent immediately, so it still shows
# the last messages and the kernel's crash report after a crash, when the
# USB console can no longer send anything.  It needs a USB-serial adapter on
# GP0/GP1 and takes those two pins.
# CRASH_REPORT=0 (default): console on USB only; the boot code leaves GP0/GP1
# alone, so a subsystem may claim them in TEAM.md.
# CONSOLE=uart always uses UART0 as the console, so it forces CRASH_REPORT=1.
CRASH_REPORT ?= 0
ifneq ($(filter $(CRASH_REPORT),0 1),$(CRASH_REPORT))
$(error Unknown CRASH_REPORT '$(CRASH_REPORT)'; use 0 or 1)
endif
ifeq ($(CONSOLE),uart)
override CRASH_REPORT := 1
endif
ifeq ($(CONSOLE)$(CRASH_REPORT),usb_cdc1)
CRASH_SUFFIX := _crash
endif

#
# Images are named after the profile they were built with.  Every image is
# flashed by hand from a downloads folder, so identically-named artifacts are
# a live hazard: an SMP=1 build was once flashed in place of an SMP=0 one and
# only the banner in the log revealed it.
#
# Opt-in application capture backend; legacy GPIO remains the default.
ENCODER_PIO ?= 0
FOLLOW_APP ?= 0
FOLLOW_MQTT ?= 0
FOLLOW_DUTY ?= 35
FOLLOW_TURN ?= 8
FOLLOW_BLUE ?= 0
FOLLOW_RED ?= 0
FOLLOW_YELLOW ?= 0
FOLLOW_MULTI ?= 0
FOLLOW_CORNER ?= 0
FOLLOW_CLIPPED ?= 0
FOLLOW_BLUE_BEND_RIGHT ?= 0
FOLLOW_LOOP_RECOVERY_DIRECTION ?= 0
FOLLOW_EARLY_BEND ?= 0
FOLLOW_HOME_TRACK ?= 0
FOLLOW_BLUE_STEER_RAMP ?= 0
FOLLOW_BLUE_RAMP_START ?= 8
FOLLOW_TWO_TURN ?= 0
FOLLOW_YELLOW_EXIT_CUE ?= 0
FOLLOW_YELLOW_LEAD_CUE ?= 0
FOLLOW_BLUE_REVERSAL_DAMPING ?= 0
FOLLOW_BLUE_STEER_WIDE_RANGE ?= 0
FOLLOW_STOP_TRACE ?= 0
FOLLOW_BLUE_RED_HANDOFF ?= 0
FOLLOW_BLUE_SECOND_BOOST ?= 0
FOLLOW_BLUE_OUTER_EDGE ?= 0
FOLLOW_BLUE_OUTER_RECOVERY ?= 0
FOLLOW_BLUE_BEND_SLOW ?= 0
FOLLOW_BLUE_CONFIRM_GRACE ?= 0
FOLLOW_BEND_TRACE ?= 0
FOLLOW_BLUE_EXIT_LEAD ?= 0
FOLLOW_BLUE_EXIT_TURN ?= 12
FOLLOW_BLUE_RED_CONFIRM_GRACE ?= 0
FOLLOW_BLUE_EXIT_AFTER_RECOVERY ?= 0
FOLLOW_RED_EXIT_LEAD ?= 0
FOLLOW_RED_EXIT_EARLY ?= 0
FOLLOW_BLUE_RED_BEND_TRIAL ?= 0
FOLLOW_BLUE_RED_BEND_CUES ?= 0
FOLLOW_RED_SECOND_PIVOT ?= 0
FOLLOW_RED_SECOND_SLOW ?= 0
FOLLOW_RED_BEND_AFTER_HANDOFF ?= 0
FOLLOW_RED_FIRST_BEND_BOOST ?= 0
FOLLOW_RED_FIRST_BEND_TIGHTER ?= 0
FOLLOW_BLUE_RED_JOIN_REFINEMENT ?= 0
FOLLOW_BEND_CUE_REFINEMENT ?= 0
FOLLOW_RED_SECOND_EXIT_REFINEMENT ?= 0
FOLLOW_BLUE_DIAGONAL_EXIT ?= 0
FOLLOW_RED_HANDOFF_ALIGN ?= 0
FOLLOW_RED_EXIT_ALIGN ?= 0
FOLLOW_RED_BETWEEN_BENDS ?= 0
FOLLOW_RED_INSIDE_RELEASE ?= 0
FOLLOW_RED_EXIT_TURN ?= 0
FOLLOW_RED_EXIT_TURN_EARLY ?= 0
FOLLOW_RED_EXIT_TURN_TIGHTER ?= 0
FOLLOW_RED_RETURN_GRACE ?= 0
FOLLOW_RED_EXIT_TURN_STRONG ?= 0
FOLLOW_RED_EXIT_TURN_HOLD ?= 0
FOLLOW_BLUE_EXIT_PIVOT ?= 0
FOLLOW_BLUE_RED_LATE_CONFIRM ?= 0
FOLLOW_BLUE_RED_BLEND_GRACE ?= 0
FOLLOW_BLUE_RED_BLEND_MARGIN ?= 0
FOLLOW_BLUE_EXIT_LEFT_BOOST ?= 0
FOLLOW_RED_FIRST_LEFT_BOOST ?= 0
FOLLOW_RED_SECOND_ARC_BOOST ?= 0
FOLLOW_BLUE_SECOND_LEFT_BOOST ?= 0
FOLLOW_RED_FINAL_LEFT_BOOST ?= 0
FOLLOW_BLUE_EXIT_EDGE_CONFIRM ?= 0
FOLLOW_BLUE_RED_LATE_APPROACH ?= 0
FOLLOW_RED_DIAGONAL_REACQUIRE ?= 0
FOLLOW_RED_FINAL_ARC_EXTEND ?= 0
FOLLOW_RED_FINAL_ARC_STRONG ?= 0
FOLLOW_BLUE_RED_JOIN_EXTEND ?= 0
FOLLOW_RED_PIVOT_INSIDE_RELEASE ?= 0
FOLLOW_RED_OUTSIDE_RECOVERY ?= 0
FOLLOW_RED_RIGHT_EDGE_OUTSIDE ?= 0
FOLLOW_RED_CORNER_REFINE ?= 0
FOLLOW_RED_SPIN_HOLD ?= 0
RGB_HOME ?= 0
ifneq ($(RGB_HOME),0)
ifneq ($(RGB_HOME),1)
$(error RGB_HOME must be 0 or 1)
endif
RGB_CAL_SUFFIX := _rgbhome
endif
RGB_ROOM2 ?= 0
ifneq ($(RGB_ROOM2),0)
ifneq ($(RGB_ROOM2),1)
$(error RGB_ROOM2 must be 0 or 1)
endif
ifneq ($(FOLLOW_HOME_TRACK),1)
$(error RGB_ROOM2 requires FOLLOW_HOME_TRACK=1)
endif
RGB_CAL_SUFFIX := _rgbroom2
endif
ifneq ($(FOLLOW_CLIPPED),0)
ifneq ($(FOLLOW_CLIPPED),1)
$(error FOLLOW_CLIPPED must be 0 or 1)
endif
ifneq ($(FOLLOW_MULTI),1)
$(error FOLLOW_CLIPPED requires FOLLOW_MULTI=1)
endif
ifneq ($(FOLLOW_CORNER),0)
$(error FOLLOW_CLIPPED and FOLLOW_CORNER cannot share an image)
endif
ifneq ($(FOLLOW_TURN),8)
$(error FOLLOW_CLIPPED requires FOLLOW_TURN=8)
endif
endif
ifneq ($(FOLLOW_BLUE_BEND_RIGHT),0)
ifneq ($(FOLLOW_BLUE_BEND_RIGHT),1)
$(error FOLLOW_BLUE_BEND_RIGHT must be 0 or 1)
endif
ifneq ($(FOLLOW_CLIPPED),1)
$(error FOLLOW_BLUE_BEND_RIGHT requires FOLLOW_CLIPPED=1)
endif
BLUE_BEND_SUFFIX := _blue_bend_right
endif
ifneq ($(FOLLOW_LOOP_RECOVERY_DIRECTION),0)
ifneq ($(FOLLOW_LOOP_RECOVERY_DIRECTION),-1)
ifneq ($(FOLLOW_LOOP_RECOVERY_DIRECTION),1)
$(error FOLLOW_LOOP_RECOVERY_DIRECTION must be -1, 0 or 1)
endif
endif
ifneq ($(FOLLOW_CLIPPED)$(FOLLOW_BLUE_BEND_RIGHT),10)
$(error Directional recovery requires FOLLOW_CLIPPED=1 and FOLLOW_BLUE_BEND_RIGHT=0)
endif
LOOP_RECOVERY_SUFFIX := _recovery$(FOLLOW_LOOP_RECOVERY_DIRECTION)
endif
ifneq ($(FOLLOW_EARLY_BEND),0)
ifneq ($(FOLLOW_EARLY_BEND),1)
$(error FOLLOW_EARLY_BEND must be 0 or 1)
endif
ifneq ($(FOLLOW_LOOP_RECOVERY_DIRECTION),1)
$(error FOLLOW_EARLY_BEND requires clockwise loop recovery)
endif
EARLY_BEND_SUFFIX := _early_bend
endif
ifneq ($(FOLLOW_HOME_TRACK),0)
ifneq ($(FOLLOW_HOME_TRACK),1)
$(error FOLLOW_HOME_TRACK must be 0 or 1)
endif
ifneq ($(FOLLOW_EARLY_BEND)$(RGB_HOME),11)
$(error FOLLOW_HOME_TRACK requires FOLLOW_EARLY_BEND=1 and RGB_HOME=1)
endif
HOME_TRACK_SUFFIX := _home_track
endif
ifneq ($(FOLLOW_BLUE_STEER_RAMP),0)
ifneq ($(FOLLOW_BLUE_STEER_RAMP),1)
$(error FOLLOW_BLUE_STEER_RAMP must be 0 or 1)
endif
ifneq ($(FOLLOW_HOME_TRACK),1)
$(error FOLLOW_BLUE_STEER_RAMP requires FOLLOW_HOME_TRACK=1)
endif
BLUE_STEER_SUFFIX := _blue_steer_ramp
endif
ifneq ($(FOLLOW_BLUE_RAMP_START),8)
ifneq ($(FOLLOW_BLUE_RAMP_START),5)
$(error FOLLOW_BLUE_RAMP_START must be 5 or 8)
endif
ifneq ($(FOLLOW_BLUE_STEER_RAMP),1)
$(error FOLLOW_BLUE_RAMP_START=5 requires FOLLOW_BLUE_STEER_RAMP=1)
endif
BLUE_RAMP_START_SUFFIX := _start5
endif
ifneq ($(FOLLOW_TWO_TURN),0)
ifneq ($(FOLLOW_TWO_TURN),1)
$(error FOLLOW_TWO_TURN must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_STEER_RAMP)$(FOLLOW_BLUE_RAMP_START),15)
$(error FOLLOW_TWO_TURN requires the home track profile with the T5 blue ramp)
endif
TWO_TURN_SUFFIX := _two_turn
endif
ifneq ($(FOLLOW_CORNER),0)
ifneq ($(FOLLOW_CORNER),1)
ifneq ($(FOLLOW_CORNER),-1)
$(error FOLLOW_CORNER must be -1, 0 or 1)
endif
endif
ifneq ($(FOLLOW_MULTI),1)
$(error FOLLOW_CORNER requires FOLLOW_MULTI=1)
endif
endif
ifneq ($(FOLLOW_MULTI),0)
ifneq ($(FOLLOW_MULTI),1)
$(error FOLLOW_MULTI must be0 or1)
endif
ifneq ($(FOLLOW_APP),1)
$(error FOLLOW_MULTI requires FOLLOW_APP=1)
endif
ifneq ($(filter 1,$(FOLLOW_BLUE) $(FOLLOW_RED) $(FOLLOW_YELLOW)),)
$(error Select one corridor target per image)
endif
ifeq ($(FOLLOW_TWO_TURN),1)
VALID_MULTI_DUTY := 30
else ifeq ($(FOLLOW_BLUE_STEER_RAMP)$(FOLLOW_BLUE_RAMP_START),15)
VALID_MULTI_DUTY := 25 27.5 30
else
VALID_MULTI_DUTY := 25
endif
ifeq ($(filter $(FOLLOW_DUTY),$(VALID_MULTI_DUTY)),)
$(error FOLLOW_MULTI requires FOLLOW_DUTY in $(VALID_MULTI_DUTY) for this profile)
endif
endif
ifneq ($(filter $(FOLLOW_BLUE),0 1),$(FOLLOW_BLUE))
$(error FOLLOW_BLUE must be0 or1)
endif
ifeq ($(FOLLOW_BLUE),1)
ifneq ($(FOLLOW_APP),1)
$(error FOLLOW_BLUE requires FOLLOW_APP=1)
endif
ifneq ($(filter 1,$(FOLLOW_RED) $(FOLLOW_YELLOW)),)
$(error Select one corridor target per image)
endif
endif
ifneq ($(filter $(FOLLOW_RED),0 1),$(FOLLOW_RED))
$(error FOLLOW_RED must be0 or1)
endif
ifeq ($(FOLLOW_RED),1)
ifneq ($(FOLLOW_APP),1)
$(error FOLLOW_RED requires FOLLOW_APP=1)
endif
ifeq ($(FOLLOW_YELLOW),1)
$(error Select either RED or YELLOW corridor)
endif
endif
ifneq ($(filter $(FOLLOW_YELLOW),0 1),$(FOLLOW_YELLOW))
$(error FOLLOW_YELLOW must be0 or1)
endif
ifeq ($(FOLLOW_YELLOW),1)
ifneq ($(FOLLOW_APP),1)
$(error FOLLOW_YELLOW requires FOLLOW_APP=1)
endif
endif
ifeq ($(filter $(FOLLOW_TURN),8 20),)
$(error FOLLOW_TURN must be8 or20 nominal percent)
endif
ifeq ($(filter $(FOLLOW_DUTY),25 27.5 30 35 40 45 50),)
$(error Unsupported FOLLOW_DUTY)
endif
ifeq ($(FOLLOW_DUTY),27.5)
ifneq ($(FOLLOW_MULTI)$(FOLLOW_BLUE_STEER_RAMP)$(FOLLOW_BLUE_RAMP_START)$(FOLLOW_TWO_TURN),1150)
$(error FOLLOW_DUTY=27.5 requires the gentle blue follower)
endif
FOLLOW_DUTY_WHOLE := 27
FOLLOW_DUTY_HALF_STEP := 1
else
FOLLOW_DUTY_WHOLE := $(FOLLOW_DUTY)
FOLLOW_DUTY_HALF_STEP := 0
endif
ifneq ($(FOLLOW_YELLOW_EXIT_CUE),0)
ifneq ($(FOLLOW_YELLOW_EXIT_CUE),1)
$(error FOLLOW_YELLOW_EXIT_CUE must be 0 or 1)
endif
ifneq ($(FOLLOW_DUTY),27.5)
ifneq ($(FOLLOW_DUTY)$(FOLLOW_RED_SECOND_SLOW),251)
$(error FOLLOW_YELLOW_EXIT_CUE requires duty27.5 or the slower red second bend at duty25)
endif
endif
YELLOW_EXIT_CUE_SUFFIX := _yellow_exit_cue
endif
ifneq ($(FOLLOW_YELLOW_LEAD_CUE),0)
ifneq ($(FOLLOW_YELLOW_LEAD_CUE),1)
$(error FOLLOW_YELLOW_LEAD_CUE must be 0 or 1)
endif
ifneq ($(FOLLOW_YELLOW_EXIT_CUE),1)
$(error FOLLOW_YELLOW_LEAD_CUE requires FOLLOW_YELLOW_EXIT_CUE=1)
endif
YELLOW_LEAD_CUE_SUFFIX := _yellow_lead_cue
endif
ifneq ($(FOLLOW_BLUE_REVERSAL_DAMPING),0)
ifneq ($(FOLLOW_BLUE_REVERSAL_DAMPING),1)
$(error FOLLOW_BLUE_REVERSAL_DAMPING must be 0 or 1)
endif
ifneq ($(FOLLOW_YELLOW_LEAD_CUE),1)
$(error FOLLOW_BLUE_REVERSAL_DAMPING requires FOLLOW_YELLOW_LEAD_CUE=1)
endif
BLUE_REVERSAL_SUFFIX := _blue_reversal
endif
ifneq ($(FOLLOW_BLUE_STEER_WIDE_RANGE),0)
ifneq ($(FOLLOW_BLUE_STEER_WIDE_RANGE),1)
$(error FOLLOW_BLUE_STEER_WIDE_RANGE must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_REVERSAL_DAMPING),1)
$(error FOLLOW_BLUE_STEER_WIDE_RANGE requires FOLLOW_BLUE_REVERSAL_DAMPING=1)
endif
BLUE_WIDE_RANGE_SUFFIX := _blue_wide_range
endif
ifneq ($(FOLLOW_STOP_TRACE),0)
ifneq ($(FOLLOW_STOP_TRACE),1)
$(error FOLLOW_STOP_TRACE must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_STEER_WIDE_RANGE),1)
$(error FOLLOW_STOP_TRACE requires FOLLOW_BLUE_STEER_WIDE_RANGE=1)
endif
FOLLOW_STOP_TRACE_SUFFIX := _stop_trace
endif
ifneq ($(FOLLOW_BLUE_RED_HANDOFF),0)
ifneq ($(FOLLOW_BLUE_RED_HANDOFF),1)
$(error FOLLOW_BLUE_RED_HANDOFF must be 0 or 1)
endif
ifneq ($(FOLLOW_STOP_TRACE),1)
$(error FOLLOW_BLUE_RED_HANDOFF requires FOLLOW_STOP_TRACE=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_red_handoff
endif
ifneq ($(FOLLOW_BLUE_SECOND_BOOST),0)
ifneq ($(FOLLOW_BLUE_SECOND_BOOST),1)
$(error FOLLOW_BLUE_SECOND_BOOST must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_RED_HANDOFF),1)
$(error FOLLOW_BLUE_SECOND_BOOST requires FOLLOW_BLUE_RED_HANDOFF=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_red_handoff_second_boost
endif
ifneq ($(FOLLOW_BLUE_OUTER_EDGE),0)
ifneq ($(FOLLOW_BLUE_OUTER_EDGE),1)
$(error FOLLOW_BLUE_OUTER_EDGE must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_RED_HANDOFF)$(FOLLOW_BLUE_SECOND_BOOST),10)
$(error FOLLOW_BLUE_OUTER_EDGE requires FOLLOW_BLUE_RED_HANDOFF=1 and FOLLOW_BLUE_SECOND_BOOST=0)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_red_handoff_outer_edge
endif
ifneq ($(FOLLOW_BLUE_OUTER_RECOVERY),0)
ifneq ($(FOLLOW_BLUE_OUTER_RECOVERY),1)
$(error FOLLOW_BLUE_OUTER_RECOVERY must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_OUTER_EDGE),1)
$(error FOLLOW_BLUE_OUTER_RECOVERY requires FOLLOW_BLUE_OUTER_EDGE=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_red_handoff_outer_recovery
endif
ifneq ($(FOLLOW_BLUE_BEND_SLOW),0)
ifneq ($(FOLLOW_BLUE_BEND_SLOW),1)
$(error FOLLOW_BLUE_BEND_SLOW must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_OUTER_RECOVERY),1)
$(error FOLLOW_BLUE_BEND_SLOW requires FOLLOW_BLUE_OUTER_RECOVERY=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_red_handoff_bend_slow
endif
ifneq ($(FOLLOW_BLUE_CONFIRM_GRACE),0)
ifneq ($(FOLLOW_BLUE_CONFIRM_GRACE),1)
$(error FOLLOW_BLUE_CONFIRM_GRACE must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_BEND_SLOW),1)
$(error FOLLOW_BLUE_CONFIRM_GRACE requires FOLLOW_BLUE_BEND_SLOW=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_red_handoff_blue_confirm
endif
ifneq ($(FOLLOW_BEND_TRACE),0)
ifneq ($(FOLLOW_BEND_TRACE),1)
$(error FOLLOW_BEND_TRACE must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_CONFIRM_GRACE),1)
$(error FOLLOW_BEND_TRACE requires FOLLOW_BLUE_CONFIRM_GRACE=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_red_handoff_bend_trace
endif
ifneq ($(FOLLOW_BLUE_EXIT_LEAD),0)
ifneq ($(FOLLOW_BLUE_EXIT_LEAD),1)
$(error FOLLOW_BLUE_EXIT_LEAD must be 0 or 1)
endif
ifneq ($(FOLLOW_BEND_TRACE),1)
$(error FOLLOW_BLUE_EXIT_LEAD requires FOLLOW_BEND_TRACE=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_red_handoff_exit_lead
endif
ifneq ($(FOLLOW_BLUE_EXIT_TURN),12)
ifneq ($(FOLLOW_BLUE_EXIT_TURN),$(filter $(FOLLOW_BLUE_EXIT_TURN),18 22))
$(error FOLLOW_BLUE_EXIT_TURN must be 12, 18, or 22)
endif
ifneq ($(FOLLOW_BLUE_EXIT_LEAD),1)
$(error A higher FOLLOW_BLUE_EXIT_TURN requires FOLLOW_BLUE_EXIT_LEAD=1)
endif
ifeq ($(FOLLOW_BLUE_EXIT_TURN),22)
BLUE_RED_HANDOFF_SUFFIX := _blue_red_handoff_exit_early
else
BLUE_RED_HANDOFF_SUFFIX := _blue_red_handoff_exit_tight
endif
endif
ifneq ($(FOLLOW_BLUE_RED_CONFIRM_GRACE),0)
ifneq ($(FOLLOW_BLUE_RED_CONFIRM_GRACE),1)
$(error FOLLOW_BLUE_RED_CONFIRM_GRACE must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_EXIT_TURN),22)
$(error FOLLOW_BLUE_RED_CONFIRM_GRACE requires FOLLOW_BLUE_EXIT_TURN=22)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_red_handoff_red_confirm
endif
ifneq ($(FOLLOW_BLUE_EXIT_AFTER_RECOVERY),0)
ifneq ($(FOLLOW_BLUE_EXIT_AFTER_RECOVERY),1)
$(error FOLLOW_BLUE_EXIT_AFTER_RECOVERY must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_RED_CONFIRM_GRACE),1)
$(error FOLLOW_BLUE_EXIT_AFTER_RECOVERY requires FOLLOW_BLUE_RED_CONFIRM_GRACE=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_red_handoff_after_recovery
endif
ifneq ($(FOLLOW_RED_EXIT_LEAD),0)
ifneq ($(FOLLOW_RED_EXIT_LEAD),1)
$(error FOLLOW_RED_EXIT_LEAD must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_EXIT_AFTER_RECOVERY),1)
$(error FOLLOW_RED_EXIT_LEAD requires FOLLOW_BLUE_EXIT_AFTER_RECOVERY=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_exit_lead
endif
ifneq ($(FOLLOW_RED_EXIT_EARLY),0)
ifneq ($(FOLLOW_RED_EXIT_EARLY),1)
$(error FOLLOW_RED_EXIT_EARLY must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_EXIT_LEAD),1)
$(error FOLLOW_RED_EXIT_EARLY requires FOLLOW_RED_EXIT_LEAD=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_exit_early
endif
ifneq ($(FOLLOW_BLUE_RED_BEND_TRIAL),0)
ifneq ($(FOLLOW_BLUE_RED_BEND_TRIAL),1)
$(error FOLLOW_BLUE_RED_BEND_TRIAL must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_EXIT_EARLY),1)
$(error FOLLOW_BLUE_RED_BEND_TRIAL requires FOLLOW_RED_EXIT_EARLY=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_red_bends
endif
ifneq ($(FOLLOW_BLUE_RED_BEND_CUES),0)
ifneq ($(FOLLOW_BLUE_RED_BEND_CUES),1)
$(error FOLLOW_BLUE_RED_BEND_CUES must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_RED_BEND_TRIAL),1)
$(error FOLLOW_BLUE_RED_BEND_CUES requires FOLLOW_BLUE_RED_BEND_TRIAL=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_red_bend_cues
endif
ifneq ($(FOLLOW_RED_SECOND_PIVOT),0)
ifneq ($(FOLLOW_RED_SECOND_PIVOT),1)
$(error FOLLOW_RED_SECOND_PIVOT must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_RED_BEND_CUES),1)
$(error FOLLOW_RED_SECOND_PIVOT requires FOLLOW_BLUE_RED_BEND_CUES=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_second_pivot
endif
ifneq ($(FOLLOW_RED_SECOND_SLOW),0)
ifneq ($(FOLLOW_RED_SECOND_SLOW),1)
$(error FOLLOW_RED_SECOND_SLOW must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_SECOND_PIVOT)$(FOLLOW_DUTY),125)
$(error FOLLOW_RED_SECOND_SLOW requires FOLLOW_RED_SECOND_PIVOT=1 and FOLLOW_DUTY=25)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_second_slow
endif
ifneq ($(FOLLOW_RED_BEND_AFTER_HANDOFF),0)
ifneq ($(FOLLOW_RED_BEND_AFTER_HANDOFF),1)
$(error FOLLOW_RED_BEND_AFTER_HANDOFF must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_SECOND_SLOW),1)
$(error FOLLOW_RED_BEND_AFTER_HANDOFF requires FOLLOW_RED_SECOND_SLOW=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_bend_after_handoff
endif
ifneq ($(FOLLOW_RED_FIRST_BEND_BOOST),0)
ifneq ($(FOLLOW_RED_FIRST_BEND_BOOST),1)
$(error FOLLOW_RED_FIRST_BEND_BOOST must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_BEND_AFTER_HANDOFF),1)
$(error FOLLOW_RED_FIRST_BEND_BOOST requires FOLLOW_RED_BEND_AFTER_HANDOFF=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_first_bend_boost
endif
ifneq ($(FOLLOW_RED_FIRST_BEND_TIGHTER),0)
ifneq ($(FOLLOW_RED_FIRST_BEND_TIGHTER),1)
$(error FOLLOW_RED_FIRST_BEND_TIGHTER must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_FIRST_BEND_BOOST),1)
$(error FOLLOW_RED_FIRST_BEND_TIGHTER requires FOLLOW_RED_FIRST_BEND_BOOST=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_first_bend_tighter
endif
ifneq ($(FOLLOW_BLUE_RED_JOIN_REFINEMENT),0)
ifneq ($(FOLLOW_BLUE_RED_JOIN_REFINEMENT),1)
$(error FOLLOW_BLUE_RED_JOIN_REFINEMENT must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_FIRST_BEND_TIGHTER),1)
$(error FOLLOW_BLUE_RED_JOIN_REFINEMENT requires FOLLOW_RED_FIRST_BEND_TIGHTER=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_red_join_refinement
endif
ifneq ($(FOLLOW_BEND_CUE_REFINEMENT),0)
ifneq ($(FOLLOW_BEND_CUE_REFINEMENT),1)
$(error FOLLOW_BEND_CUE_REFINEMENT must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_RED_JOIN_REFINEMENT),1)
$(error FOLLOW_BEND_CUE_REFINEMENT requires FOLLOW_BLUE_RED_JOIN_REFINEMENT=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _bend_cue_refinement
endif
ifneq ($(FOLLOW_RED_SECOND_EXIT_REFINEMENT),0)
ifneq ($(FOLLOW_RED_SECOND_EXIT_REFINEMENT),1)
$(error FOLLOW_RED_SECOND_EXIT_REFINEMENT must be 0 or 1)
endif
ifneq ($(FOLLOW_BEND_CUE_REFINEMENT),1)
$(error FOLLOW_RED_SECOND_EXIT_REFINEMENT requires FOLLOW_BEND_CUE_REFINEMENT=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_second_exit_refinement
endif
ifneq ($(FOLLOW_BLUE_DIAGONAL_EXIT),0)
ifneq ($(FOLLOW_BLUE_DIAGONAL_EXIT),1)
$(error FOLLOW_BLUE_DIAGONAL_EXIT must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_SECOND_EXIT_REFINEMENT),1)
$(error FOLLOW_BLUE_DIAGONAL_EXIT requires FOLLOW_RED_SECOND_EXIT_REFINEMENT=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_diagonal_exit
endif
ifneq ($(FOLLOW_RED_HANDOFF_ALIGN),0)
ifneq ($(FOLLOW_RED_HANDOFF_ALIGN),1)
$(error FOLLOW_RED_HANDOFF_ALIGN must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_DIAGONAL_EXIT),1)
$(error FOLLOW_RED_HANDOFF_ALIGN requires FOLLOW_BLUE_DIAGONAL_EXIT=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_handoff_align
endif
ifneq ($(FOLLOW_RED_EXIT_ALIGN),0)
ifneq ($(FOLLOW_RED_EXIT_ALIGN),1)
$(error FOLLOW_RED_EXIT_ALIGN must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_HANDOFF_ALIGN),1)
$(error FOLLOW_RED_EXIT_ALIGN requires FOLLOW_RED_HANDOFF_ALIGN=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_exit_align
endif
ifneq ($(FOLLOW_RED_BETWEEN_BENDS),0)
ifneq ($(FOLLOW_RED_BETWEEN_BENDS),1)
$(error FOLLOW_RED_BETWEEN_BENDS must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_EXIT_ALIGN),1)
$(error FOLLOW_RED_BETWEEN_BENDS requires FOLLOW_RED_EXIT_ALIGN=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_between_bends
endif
ifneq ($(FOLLOW_RED_INSIDE_RELEASE),0)
ifneq ($(FOLLOW_RED_INSIDE_RELEASE),1)
$(error FOLLOW_RED_INSIDE_RELEASE must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_BETWEEN_BENDS),1)
$(error FOLLOW_RED_INSIDE_RELEASE requires FOLLOW_RED_BETWEEN_BENDS=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_inside_release
endif
ifneq ($(FOLLOW_RED_EXIT_TURN),0)
ifneq ($(FOLLOW_RED_EXIT_TURN),1)
$(error FOLLOW_RED_EXIT_TURN must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_INSIDE_RELEASE),1)
$(error FOLLOW_RED_EXIT_TURN requires FOLLOW_RED_INSIDE_RELEASE=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_exit_turn
endif
ifneq ($(FOLLOW_RED_EXIT_TURN_EARLY),0)
ifneq ($(FOLLOW_RED_EXIT_TURN_EARLY),1)
$(error FOLLOW_RED_EXIT_TURN_EARLY must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_EXIT_TURN),1)
$(error FOLLOW_RED_EXIT_TURN_EARLY requires FOLLOW_RED_EXIT_TURN=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_exit_turn_early
endif
ifneq ($(FOLLOW_RED_EXIT_TURN_TIGHTER),0)
ifneq ($(FOLLOW_RED_EXIT_TURN_TIGHTER),1)
$(error FOLLOW_RED_EXIT_TURN_TIGHTER must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_EXIT_TURN_EARLY),1)
$(error FOLLOW_RED_EXIT_TURN_TIGHTER requires FOLLOW_RED_EXIT_TURN_EARLY=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_exit_turn_tighter
endif
ifneq ($(FOLLOW_RED_RETURN_GRACE),0)
ifneq ($(FOLLOW_RED_RETURN_GRACE),1)
$(error FOLLOW_RED_RETURN_GRACE must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_EXIT_TURN_TIGHTER),1)
$(error FOLLOW_RED_RETURN_GRACE requires FOLLOW_RED_EXIT_TURN_TIGHTER=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_return_grace
endif
ifneq ($(FOLLOW_RED_EXIT_TURN_STRONG),0)
ifneq ($(FOLLOW_RED_EXIT_TURN_STRONG),1)
$(error FOLLOW_RED_EXIT_TURN_STRONG must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_RETURN_GRACE),1)
$(error FOLLOW_RED_EXIT_TURN_STRONG requires FOLLOW_RED_RETURN_GRACE=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_exit_turn_strong
endif
ifneq ($(FOLLOW_RED_EXIT_TURN_HOLD),0)
ifneq ($(FOLLOW_RED_EXIT_TURN_HOLD),1)
$(error FOLLOW_RED_EXIT_TURN_HOLD must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_EXIT_TURN_STRONG),1)
$(error FOLLOW_RED_EXIT_TURN_HOLD requires FOLLOW_RED_EXIT_TURN_STRONG=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_exit_turn_hold
endif
ifneq ($(FOLLOW_BLUE_EXIT_PIVOT),0)
ifneq ($(FOLLOW_BLUE_EXIT_PIVOT),1)
$(error FOLLOW_BLUE_EXIT_PIVOT must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_EXIT_TURN_HOLD),1)
$(error FOLLOW_BLUE_EXIT_PIVOT requires FOLLOW_RED_EXIT_TURN_HOLD=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_exit_pivot
endif
ifneq ($(FOLLOW_BLUE_RED_LATE_CONFIRM),0)
ifneq ($(FOLLOW_BLUE_RED_LATE_CONFIRM),1)
$(error FOLLOW_BLUE_RED_LATE_CONFIRM must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_EXIT_PIVOT),1)
$(error FOLLOW_BLUE_RED_LATE_CONFIRM requires FOLLOW_BLUE_EXIT_PIVOT=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_red_late_confirm
endif
ifneq ($(FOLLOW_BLUE_RED_BLEND_GRACE),0)
ifneq ($(FOLLOW_BLUE_RED_BLEND_GRACE),1)
$(error FOLLOW_BLUE_RED_BLEND_GRACE must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_RED_LATE_CONFIRM),1)
$(error FOLLOW_BLUE_RED_BLEND_GRACE requires FOLLOW_BLUE_RED_LATE_CONFIRM=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_red_blend_grace
endif
ifneq ($(FOLLOW_BLUE_RED_BLEND_MARGIN),0)
ifneq ($(FOLLOW_BLUE_RED_BLEND_MARGIN),1)
$(error FOLLOW_BLUE_RED_BLEND_MARGIN must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_RED_BLEND_GRACE),1)
$(error FOLLOW_BLUE_RED_BLEND_MARGIN requires FOLLOW_BLUE_RED_BLEND_GRACE=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_red_blend_margin
endif
ifneq ($(FOLLOW_BLUE_EXIT_LEFT_BOOST),0)
ifneq ($(FOLLOW_BLUE_EXIT_LEFT_BOOST),1)
$(error FOLLOW_BLUE_EXIT_LEFT_BOOST must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_RED_BLEND_MARGIN),1)
$(error FOLLOW_BLUE_EXIT_LEFT_BOOST requires FOLLOW_BLUE_RED_BLEND_MARGIN=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_exit_left_boost
endif
ifneq ($(FOLLOW_RED_FIRST_LEFT_BOOST),0)
ifneq ($(FOLLOW_RED_FIRST_LEFT_BOOST),1)
$(error FOLLOW_RED_FIRST_LEFT_BOOST must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_EXIT_LEFT_BOOST),1)
$(error FOLLOW_RED_FIRST_LEFT_BOOST requires FOLLOW_BLUE_EXIT_LEFT_BOOST=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_first_left_boost
endif
ifneq ($(FOLLOW_RED_SECOND_ARC_BOOST),0)
ifneq ($(FOLLOW_RED_SECOND_ARC_BOOST),1)
$(error FOLLOW_RED_SECOND_ARC_BOOST must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_FIRST_LEFT_BOOST),1)
$(error FOLLOW_RED_SECOND_ARC_BOOST requires FOLLOW_RED_FIRST_LEFT_BOOST=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_second_arc_boost
endif
ifneq ($(FOLLOW_BLUE_SECOND_LEFT_BOOST),0)
ifneq ($(FOLLOW_BLUE_SECOND_LEFT_BOOST),1)
$(error FOLLOW_BLUE_SECOND_LEFT_BOOST must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_SECOND_ARC_BOOST),1)
$(error FOLLOW_BLUE_SECOND_LEFT_BOOST requires FOLLOW_RED_SECOND_ARC_BOOST=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _bend_power
endif
ifneq ($(FOLLOW_RED_FINAL_LEFT_BOOST),0)
ifneq ($(FOLLOW_RED_FINAL_LEFT_BOOST),1)
$(error FOLLOW_RED_FINAL_LEFT_BOOST must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_SECOND_LEFT_BOOST),1)
$(error FOLLOW_RED_FINAL_LEFT_BOOST requires FOLLOW_BLUE_SECOND_LEFT_BOOST=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_final_left_boost
endif
ifneq ($(FOLLOW_BLUE_EXIT_EDGE_CONFIRM),0)
ifneq ($(FOLLOW_BLUE_EXIT_EDGE_CONFIRM),1)
$(error FOLLOW_BLUE_EXIT_EDGE_CONFIRM must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_FINAL_LEFT_BOOST),1)
$(error FOLLOW_BLUE_EXIT_EDGE_CONFIRM requires FOLLOW_RED_FINAL_LEFT_BOOST=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_exit_edge_confirm
endif
ifneq ($(FOLLOW_BLUE_RED_LATE_APPROACH),0)
ifneq ($(FOLLOW_BLUE_RED_LATE_APPROACH),1)
$(error FOLLOW_BLUE_RED_LATE_APPROACH must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_EXIT_EDGE_CONFIRM),1)
$(error FOLLOW_BLUE_RED_LATE_APPROACH requires FOLLOW_BLUE_EXIT_EDGE_CONFIRM=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_red_late_approach
endif
ifneq ($(FOLLOW_RED_DIAGONAL_REACQUIRE),0)
ifneq ($(FOLLOW_RED_DIAGONAL_REACQUIRE),1)
$(error FOLLOW_RED_DIAGONAL_REACQUIRE must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_RED_LATE_APPROACH),1)
$(error FOLLOW_RED_DIAGONAL_REACQUIRE requires FOLLOW_BLUE_RED_LATE_APPROACH=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_diagonal_reacquire
endif
ifneq ($(FOLLOW_RED_FINAL_ARC_EXTEND),0)
ifneq ($(FOLLOW_RED_FINAL_ARC_EXTEND),1)
$(error FOLLOW_RED_FINAL_ARC_EXTEND must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_DIAGONAL_REACQUIRE),1)
$(error FOLLOW_RED_FINAL_ARC_EXTEND requires FOLLOW_RED_DIAGONAL_REACQUIRE=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_final_arc_extend
endif
ifneq ($(FOLLOW_RED_FINAL_ARC_STRONG),0)
ifneq ($(FOLLOW_RED_FINAL_ARC_STRONG),1)
$(error FOLLOW_RED_FINAL_ARC_STRONG must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_FINAL_ARC_EXTEND),1)
$(error FOLLOW_RED_FINAL_ARC_STRONG requires FOLLOW_RED_FINAL_ARC_EXTEND=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_final_arc_strong
endif
ifneq ($(FOLLOW_BLUE_RED_JOIN_EXTEND),0)
ifneq ($(FOLLOW_BLUE_RED_JOIN_EXTEND),1)
$(error FOLLOW_BLUE_RED_JOIN_EXTEND must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_FINAL_ARC_STRONG),1)
$(error FOLLOW_BLUE_RED_JOIN_EXTEND requires FOLLOW_RED_FINAL_ARC_STRONG=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _blue_red_join_extend
endif
ifneq ($(FOLLOW_RED_PIVOT_INSIDE_RELEASE),0)
ifneq ($(FOLLOW_RED_PIVOT_INSIDE_RELEASE),1)
$(error FOLLOW_RED_PIVOT_INSIDE_RELEASE must be 0 or 1)
endif
ifneq ($(FOLLOW_BLUE_RED_JOIN_EXTEND),1)
$(error FOLLOW_RED_PIVOT_INSIDE_RELEASE requires FOLLOW_BLUE_RED_JOIN_EXTEND=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_pivot_inside_release
endif
ifneq ($(FOLLOW_RED_OUTSIDE_RECOVERY),0)
ifneq ($(FOLLOW_RED_OUTSIDE_RECOVERY),1)
$(error FOLLOW_RED_OUTSIDE_RECOVERY must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_PIVOT_INSIDE_RELEASE),1)
$(error FOLLOW_RED_OUTSIDE_RECOVERY requires FOLLOW_RED_PIVOT_INSIDE_RELEASE=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_outside_recovery
endif
ifneq ($(FOLLOW_RED_RIGHT_EDGE_OUTSIDE),0)
ifneq ($(FOLLOW_RED_RIGHT_EDGE_OUTSIDE),1)
$(error FOLLOW_RED_RIGHT_EDGE_OUTSIDE must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_OUTSIDE_RECOVERY),1)
$(error FOLLOW_RED_RIGHT_EDGE_OUTSIDE requires FOLLOW_RED_OUTSIDE_RECOVERY=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_right_edge_outside
endif
ifneq ($(FOLLOW_RED_CORNER_REFINE),0)
ifneq ($(FOLLOW_RED_CORNER_REFINE),1)
$(error FOLLOW_RED_CORNER_REFINE must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_RIGHT_EDGE_OUTSIDE),1)
$(error FOLLOW_RED_CORNER_REFINE requires FOLLOW_RED_RIGHT_EDGE_OUTSIDE=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_corner_refine
endif
ifneq ($(FOLLOW_RED_SPIN_HOLD),0)
ifneq ($(FOLLOW_RED_SPIN_HOLD),1)
$(error FOLLOW_RED_SPIN_HOLD must be 0 or 1)
endif
ifneq ($(FOLLOW_RED_CORNER_REFINE),1)
$(error FOLLOW_RED_SPIN_HOLD requires FOLLOW_RED_CORNER_REFINE=1)
endif
BLUE_RED_HANDOFF_SUFFIX := _red_spin_hold
endif
RGB_DIAG ?= 0
ifneq ($(filter $(FOLLOW_APP),0 1),$(FOLLOW_APP))
$(error Unknown FOLLOW_APP '$(FOLLOW_APP)'; use 0 or 1)
endif
ifneq ($(filter $(FOLLOW_MQTT),0 1),$(FOLLOW_MQTT))
$(error Unknown FOLLOW_MQTT '$(FOLLOW_MQTT)'; use 0 or 1)
endif
ifeq ($(FOLLOW_MQTT),1)
ifneq ($(FOLLOW_APP)$(FOLLOW_MULTI)$(REMOTE_APP)$(SMP)$(CONSOLE)$(WIFI)$(RGB_DIAG)$(ENCODER_PIO),1111usb_cdccyw4301)
$(error FOLLOW_MQTT=1 requires multicolour FOLLOW, REMOTE_APP=1, SMP, USB, cyw43 and ENCODER_PIO=1)
endif
FOLLOW_MQTT_SUFFIX := _followmqtt
endif
ifeq ($(FOLLOW_APP),1)
ifeq ($(FOLLOW_MQTT),0)
ifneq ($(SMP)$(CONSOLE)$(WIFI)$(NET)$(MQTT)$(MOTION_APP)$(REMOTE_APP)$(COMM_APP)$(RGB_DIAG)$(ENCODER_PIO),1usb_cdccyw43none000001)
$(error FOLLOW_APP=1 requires SMP=1 USB WIFI=cyw43 NET=none MQTT=0 MOTION_APP/REMOTE_APP/COMM_APP/RGB_DIAG=0 ENCODER_PIO=1)
endif
endif
FOLLOW_SUFFIX := _follow_r$(FOLLOW_RED)_y$(FOLLOW_YELLOW)_b$(FOLLOW_BLUE)_m$(FOLLOW_MULTI)_c$(FOLLOW_CORNER)_clip$(FOLLOW_CLIPPED)_d$(FOLLOW_DUTY)_t$(FOLLOW_TURN)$(BLUE_BEND_SUFFIX)$(LOOP_RECOVERY_SUFFIX)$(EARLY_BEND_SUFFIX)$(HOME_TRACK_SUFFIX)$(BLUE_STEER_SUFFIX)$(BLUE_RAMP_START_SUFFIX)$(TWO_TURN_SUFFIX)$(YELLOW_EXIT_CUE_SUFFIX)$(YELLOW_LEAD_CUE_SUFFIX)$(BLUE_REVERSAL_SUFFIX)$(BLUE_WIDE_RANGE_SUFFIX)$(FOLLOW_STOP_TRACE_SUFFIX)$(BLUE_RED_HANDOFF_SUFFIX)
endif
DISTANCE_RUN ?= 0
ifneq ($(filter $(DISTANCE_RUN),0 1),$(DISTANCE_RUN))
$(error Unknown DISTANCE_RUN '$(DISTANCE_RUN)'; use 0 or 1)
endif
ifeq ($(DISTANCE_RUN),1)
ifneq ($(MOTION_APP)$(REMOTE_APP)$(ENCODER_PIO)$(CONSOLE)$(WIFI),101usb_cdccyw43)
$(error DISTANCE_RUN=1 requires MOTION_APP=1 REMOTE_APP=0 ENCODER_PIO=1 CONSOLE=usb_cdc WIFI=cyw43)
endif
DISTANCE_RUN_SUFFIX := _distance
endif
ifneq ($(filter $(ENCODER_PIO),0 1),$(ENCODER_PIO))
$(error Unknown ENCODER_PIO '$(ENCODER_PIO)'; use 0 or 1)
endif
ifeq ($(ENCODER_PIO),1)
ifeq ($(filter 1,$(FOLLOW_APP) $(DISTANCE_RUN)),)
ifneq ($(WIFI)$(REMOTE_APP),cyw431)
$(error ENCODER_PIO=1 requires WIFI=cyw43 and REMOTE_APP=1, FOLLOW_APP=1 or DISTANCE_RUN=1)
endif
endif
ENCODER_PIO_SUFFIX := _encpio
endif

RGB_DIAG ?= 0
ifneq ($(filter $(RGB_DIAG),0 1),$(RGB_DIAG))
$(error Unknown RGB_DIAG '$(RGB_DIAG)'; use 0 or 1)
endif
ifeq ($(RGB_DIAG),1)
ifeq ($(FOLLOW_APP),1)
$(error RGB_DIAG and FOLLOW_APP cannot share an image)
endif
ifneq ($(CONSOLE)$(WIFI)$(NET)$(MQTT)$(MOTOR_PROBE)$(MOTION_APP)$(REMOTE_APP)$(COMM_APP),usb_cdccyw43none01000)
$(error RGB_DIAG=1 requires USB, WIFI=cyw43, NET=none, MQTT=0, MOTOR_PROBE=1 and MOTION_APP/REMOTE_APP/COMM_APP=0)
endif
RGB_DIAG_SUFFIX := _rgbdiag
endif

EXE_FILE := mtk3pico_smp$(SMP)_$(CONSOLE)$(WIFI_SUFFIX)$(NET_SUFFIX)$(MQTT_SUFFIX)$(CRASH_SUFFIX)$(MOTOR_PROBE_SUFFIX)$(COMM_APP_SUFFIX)$(MOTION_APP_SUFFIX)$(REMOTE_APP_SUFFIX)$(ENCODER_PIO_SUFFIX)$(RGB_DIAG_SUFFIX)$(RGB_CAL_SUFFIX)$(FOLLOW_SUFFIX)$(DISTANCE_RUN_SUFFIX)$(FOLLOW_MQTT_SUFFIX)$(WIFI_AUTH_SUFFIX)$(TEST_SUFFIX)

GCC := arm-none-eabi-gcc
AS := arm-none-eabi-gcc
LINK := arm-none-eabi-gcc
SIZE := arm-none-eabi-size
E2U := tools/elf2uf2

# Fail early, with a readable message, when the cross compiler is not on PATH.
ifneq ($(MAKECMDGOALS),clean)
ifeq ($(shell command -v $(GCC) 2>/dev/null),)
$(error $(GCC) not found. Add your Arm GNU Toolchain 'bin' folder to PATH, e.g. export PATH="/path/to/arm-gnu-toolchain/bin:$$PATH")
endif
endif

# The USB console and the radio take sources from the Pico C SDK.  Its location
# must come from the environment (export PICO_SDK_PATH=...) or the command line.
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(CONSOLE)$(WIFI),uartnone)
ifeq ($(strip $(PICO_SDK_PATH)),)
$(error PICO_SDK_PATH is not set. export PICO_SDK_PATH=/path/to/pico-sdk, or build with CONSOLE=uart WIFI=none)
endif
ifeq ($(wildcard $(PICO_SDK_PATH)/pico_sdk_init.cmake),)
$(error PICO_SDK_PATH='$(PICO_SDK_PATH)' does not look like a Pico C SDK folder)
endif
endif
endif

# The ELF-to-UF2 converter is a small host program built from tools/elf2uf2
# with the host C++ compiler the first time it is needed.
$(E2U): ../tools/elf2uf2/main.cpp
	@echo 'Building host tool: $@'
	c++ -O2 -std=c++14 -I../tools/elf2uf2 -o $@ $<

$(EXE_FILE).elf: | $(E2U)

CFLAGS := -mcpu=cortex-m0plus -mthumb -ffreestanding\
    -std=gnu11 \
    -O2 -g3 \
    -MMD -MP \
    -mfloat-abi=soft \

ASFLAGS := -mcpu=cortex-m0plus -mthumb -ffreestanding\
    -x assembler-with-cpp \
    -O2 -g3 \
    -MMD -MP \

LFLAGS := -mcpu=cortex-m0plus -mthumb -ffreestanding \
    -nostartfiles \
    -O2 -g3 \
    -mfloat-abi=soft \

LNKFILE := "../etc/linker/pico_rp2040/tkernel_map.ld"

# Applied here, after CFLAGS/ASFLAGS are assigned with ':=', which would
# otherwise discard anything appended above.
ifeq ($(SMP),1)
CFLAGS  += -DCNF_SMP=1
ASFLAGS += -DCNF_SMP=1
endif
CFLAGS  += -DAPP_TEST=$(TEST)
CFLAGS  += -DMOTOR_TEST_PROBE=$(MOTOR_PROBE)
CFLAGS  += -DAAI05_COMMS_APP=$(COMM_APP)
CFLAGS  += -DAAI05_MOTION_APP=$(MOTION_APP)
CFLAGS  += -DAAI05_DISTANCE_RUN=$(DISTANCE_RUN)
CFLAGS  += -DAAI05_FOLLOW_MQTT=$(FOLLOW_MQTT)
ifeq ($(WIFI_AUTH),wpa3)
CFLAGS  += -DWIFI_AUTH_MODE=CYW43_AUTH_WPA3_SAE_AES_PSK
endif
CFLAGS  += -DAAI05_REMOTE_APP=$(REMOTE_APP)
CFLAGS  += -DAAI05_ENCODER_PIO=$(ENCODER_PIO)
CFLAGS  += -DAAI05_RGB_DIAG=$(RGB_DIAG)
CFLAGS  += -DAAI05_RGB_HOME_PROFILE=$(RGB_HOME)
CFLAGS  += -DAAI05_RGB_ROOM2=$(RGB_ROOM2)
CFLAGS  += -DAAI05_FOLLOW_APP=$(FOLLOW_APP)
CFLAGS  += -DFOLLOW_DUTY_LIMIT_PCT=$(FOLLOW_DUTY_WHOLE)
CFLAGS  += -DFOLLOW_DUTY_HALF_STEP=$(FOLLOW_DUTY_HALF_STEP)
CFLAGS  += -DFOLLOW_TURN_LIMIT_PCT=$(FOLLOW_TURN)
CFLAGS  += -DFOLLOW_YELLOW_CORRIDOR=$(FOLLOW_YELLOW)
CFLAGS  += -DFOLLOW_RED_CORRIDOR=$(FOLLOW_RED)
CFLAGS  += -DFOLLOW_BLUE_CORRIDOR=$(FOLLOW_BLUE)
CFLAGS  += -DFOLLOW_MULTICOLOUR_CORRIDOR=$(FOLLOW_MULTI)
CFLAGS  += -DFOLLOW_CORNER_DIRECTION=$(FOLLOW_CORNER)
CFLAGS  += -DFOLLOW_CLIPPED_LOOP=$(FOLLOW_CLIPPED)
CFLAGS  += -DFOLLOW_BLUE_BEND_RIGHT=$(FOLLOW_BLUE_BEND_RIGHT)
CFLAGS  += -DFOLLOW_LOOP_RECOVERY_DIRECTION=$(FOLLOW_LOOP_RECOVERY_DIRECTION)
CFLAGS  += -DFOLLOW_EARLY_BEND=$(FOLLOW_EARLY_BEND)
CFLAGS  += -DFOLLOW_HOME_TRACK=$(FOLLOW_HOME_TRACK)
CFLAGS  += -DFOLLOW_BLUE_STEER_RAMP=$(FOLLOW_BLUE_STEER_RAMP)
CFLAGS  += -DFOLLOW_BLUE_RAMP_START=$(FOLLOW_BLUE_RAMP_START)
CFLAGS  += -DFOLLOW_TWO_TURN=$(FOLLOW_TWO_TURN)
CFLAGS  += -DFOLLOW_YELLOW_EXIT_CUE=$(FOLLOW_YELLOW_EXIT_CUE)
CFLAGS  += -DFOLLOW_YELLOW_LEAD_CUE=$(FOLLOW_YELLOW_LEAD_CUE)
CFLAGS  += -DFOLLOW_BLUE_REVERSAL_DAMPING=$(FOLLOW_BLUE_REVERSAL_DAMPING)
CFLAGS  += -DFOLLOW_BLUE_STEER_WIDE_RANGE=$(FOLLOW_BLUE_STEER_WIDE_RANGE)
CFLAGS  += -DFOLLOW_STOP_TRACE=$(FOLLOW_STOP_TRACE)
CFLAGS  += -DFOLLOW_BLUE_RED_HANDOFF=$(FOLLOW_BLUE_RED_HANDOFF)
CFLAGS  += -DFOLLOW_BLUE_SECOND_BOOST=$(FOLLOW_BLUE_SECOND_BOOST)
CFLAGS  += -DFOLLOW_BLUE_OUTER_EDGE=$(FOLLOW_BLUE_OUTER_EDGE)
CFLAGS  += -DFOLLOW_BLUE_OUTER_RECOVERY=$(FOLLOW_BLUE_OUTER_RECOVERY)
CFLAGS  += -DFOLLOW_BLUE_BEND_SLOW=$(FOLLOW_BLUE_BEND_SLOW)
CFLAGS  += -DFOLLOW_BLUE_CONFIRM_GRACE=$(FOLLOW_BLUE_CONFIRM_GRACE)
CFLAGS  += -DFOLLOW_BEND_TRACE=$(FOLLOW_BEND_TRACE)
CFLAGS  += -DFOLLOW_BLUE_EXIT_LEAD=$(FOLLOW_BLUE_EXIT_LEAD)
CFLAGS  += -DFOLLOW_BLUE_EXIT_TURN=$(FOLLOW_BLUE_EXIT_TURN)
CFLAGS  += -DFOLLOW_BLUE_RED_CONFIRM_GRACE=$(FOLLOW_BLUE_RED_CONFIRM_GRACE)
CFLAGS  += -DFOLLOW_BLUE_EXIT_AFTER_RECOVERY=$(FOLLOW_BLUE_EXIT_AFTER_RECOVERY)
CFLAGS  += -DFOLLOW_RED_EXIT_LEAD=$(FOLLOW_RED_EXIT_LEAD)
CFLAGS  += -DFOLLOW_RED_EXIT_EARLY=$(FOLLOW_RED_EXIT_EARLY)
CFLAGS  += -DFOLLOW_BLUE_RED_BEND_TRIAL=$(FOLLOW_BLUE_RED_BEND_TRIAL)
CFLAGS  += -DFOLLOW_BLUE_RED_BEND_CUES=$(FOLLOW_BLUE_RED_BEND_CUES)
CFLAGS  += -DFOLLOW_RED_SECOND_PIVOT=$(FOLLOW_RED_SECOND_PIVOT)
CFLAGS  += -DFOLLOW_RED_SECOND_SLOW=$(FOLLOW_RED_SECOND_SLOW)
CFLAGS  += -DFOLLOW_RED_BEND_AFTER_HANDOFF=$(FOLLOW_RED_BEND_AFTER_HANDOFF)
CFLAGS  += -DFOLLOW_RED_FIRST_BEND_BOOST=$(FOLLOW_RED_FIRST_BEND_BOOST)
CFLAGS  += -DFOLLOW_RED_FIRST_BEND_TIGHTER=$(FOLLOW_RED_FIRST_BEND_TIGHTER)
CFLAGS  += -DFOLLOW_BLUE_RED_JOIN_REFINEMENT=$(FOLLOW_BLUE_RED_JOIN_REFINEMENT)
CFLAGS  += -DFOLLOW_BEND_CUE_REFINEMENT=$(FOLLOW_BEND_CUE_REFINEMENT)
CFLAGS  += -DFOLLOW_RED_SECOND_EXIT_REFINEMENT=$(FOLLOW_RED_SECOND_EXIT_REFINEMENT)
CFLAGS  += -DFOLLOW_BLUE_DIAGONAL_EXIT=$(FOLLOW_BLUE_DIAGONAL_EXIT)
CFLAGS  += -DFOLLOW_RED_HANDOFF_ALIGN=$(FOLLOW_RED_HANDOFF_ALIGN)
CFLAGS  += -DFOLLOW_RED_EXIT_ALIGN=$(FOLLOW_RED_EXIT_ALIGN)
CFLAGS  += -DFOLLOW_RED_BETWEEN_BENDS=$(FOLLOW_RED_BETWEEN_BENDS)
CFLAGS  += -DFOLLOW_RED_INSIDE_RELEASE=$(FOLLOW_RED_INSIDE_RELEASE)
CFLAGS  += -DFOLLOW_RED_EXIT_TURN=$(FOLLOW_RED_EXIT_TURN)
CFLAGS  += -DFOLLOW_RED_EXIT_TURN_EARLY=$(FOLLOW_RED_EXIT_TURN_EARLY)
CFLAGS  += -DFOLLOW_RED_EXIT_TURN_TIGHTER=$(FOLLOW_RED_EXIT_TURN_TIGHTER)
CFLAGS  += -DFOLLOW_RED_RETURN_GRACE=$(FOLLOW_RED_RETURN_GRACE)
CFLAGS  += -DFOLLOW_RED_EXIT_TURN_STRONG=$(FOLLOW_RED_EXIT_TURN_STRONG)
CFLAGS  += -DFOLLOW_RED_EXIT_TURN_HOLD=$(FOLLOW_RED_EXIT_TURN_HOLD)
CFLAGS  += -DFOLLOW_BLUE_EXIT_PIVOT=$(FOLLOW_BLUE_EXIT_PIVOT)
CFLAGS  += -DFOLLOW_BLUE_RED_LATE_CONFIRM=$(FOLLOW_BLUE_RED_LATE_CONFIRM)
CFLAGS  += -DFOLLOW_BLUE_RED_BLEND_GRACE=$(FOLLOW_BLUE_RED_BLEND_GRACE)
CFLAGS  += -DFOLLOW_BLUE_RED_BLEND_MARGIN=$(FOLLOW_BLUE_RED_BLEND_MARGIN)
CFLAGS  += -DFOLLOW_BLUE_EXIT_LEFT_BOOST=$(FOLLOW_BLUE_EXIT_LEFT_BOOST)
CFLAGS  += -DFOLLOW_RED_FIRST_LEFT_BOOST=$(FOLLOW_RED_FIRST_LEFT_BOOST)
CFLAGS  += -DFOLLOW_RED_SECOND_ARC_BOOST=$(FOLLOW_RED_SECOND_ARC_BOOST)
CFLAGS  += -DFOLLOW_BLUE_SECOND_LEFT_BOOST=$(FOLLOW_BLUE_SECOND_LEFT_BOOST)
CFLAGS  += -DFOLLOW_RED_FINAL_LEFT_BOOST=$(FOLLOW_RED_FINAL_LEFT_BOOST)
CFLAGS  += -DFOLLOW_BLUE_EXIT_EDGE_CONFIRM=$(FOLLOW_BLUE_EXIT_EDGE_CONFIRM)
CFLAGS  += -DFOLLOW_BLUE_RED_LATE_APPROACH=$(FOLLOW_BLUE_RED_LATE_APPROACH)
CFLAGS  += -DFOLLOW_RED_DIAGONAL_REACQUIRE=$(FOLLOW_RED_DIAGONAL_REACQUIRE)
CFLAGS  += -DFOLLOW_RED_FINAL_ARC_EXTEND=$(FOLLOW_RED_FINAL_ARC_EXTEND)
CFLAGS  += -DFOLLOW_RED_FINAL_ARC_STRONG=$(FOLLOW_RED_FINAL_ARC_STRONG)
CFLAGS  += -DFOLLOW_BLUE_RED_JOIN_EXTEND=$(FOLLOW_BLUE_RED_JOIN_EXTEND)
CFLAGS  += -DFOLLOW_RED_PIVOT_INSIDE_RELEASE=$(FOLLOW_RED_PIVOT_INSIDE_RELEASE)
CFLAGS  += -DFOLLOW_RED_OUTSIDE_RECOVERY=$(FOLLOW_RED_OUTSIDE_RECOVERY)
CFLAGS  += -DFOLLOW_RED_RIGHT_EDGE_OUTSIDE=$(FOLLOW_RED_RIGHT_EDGE_OUTSIDE)
CFLAGS  += -DFOLLOW_RED_CORNER_REFINE=$(FOLLOW_RED_CORNER_REFINE)
CFLAGS  += -DFOLLOW_RED_SPIN_HOLD=$(FOLLOW_RED_SPIN_HOLD)
CFLAGS  += -DTM_CONSOLE_UART=$(CRASH_REPORT)
ifeq ($(CRASH_REPORT),0)
CFLAGS  += -DBOARD_UART0_FREE=1
endif

################################################################################
# Console selection
#
#   CONSOLE=usb_cdc             USB CDC-ACM console, with the UART0 mirror
#                               retained for early boot and panics.
#   CONSOLE=uart                UART0 only, no TinyUSB dependency.
#
# usb_cdc pulls seven TinyUSB sources and a set of pico-sdk headers from
# PICO_SDK_PATH.  Only headers and those sources are used; the port does not
# link the pico-sdk runtime, and lib/libtm/sysdepend/pico_rp2040/usb/
# usb_sdk_compat.c supplies the few SDK hooks TinyUSB calls, mapped onto
# micro T-Kernel's tk_def_int().
################################################################################

################################################################################
# Stale-object guard
#
# Every profile compiles into the same object tree, so switching profiles
# without cleaning silently relinks the previous profile's objects: SMP=0 and
# SMP=1 built back to back produced byte-identical images. Record the profile
# and discard the objects whenever it changes. Only *.o and *.d are removed:
# build_make/mtkernel_3/ also holds the tracked subdir.mk files, so removing
# the directory wholesale deletes source. Images are named after their
# profile, so both can coexist. This runs at parse time, so it completes
# before any recipe executes.
################################################################################

PROFILE_ID := smp$(SMP)-$(CONSOLE)-wifi$(WIFI)-net$(NET)-mqtt$(MQTT)-crash$(CRASH_REPORT)-probe$(MOTOR_PROBE)-comm$(COMM_APP)-motion$(MOTION_APP)-remote$(REMOTE_APP)-encpio$(ENCODER_PIO)-rgb$(RGB_DIAG)-rgbhome$(RGB_HOME)-rgbroom2$(RGB_ROOM2)-follow$(FOLLOW_APP)-duty$(FOLLOW_DUTY)-turn$(FOLLOW_TURN)-yellow$(FOLLOW_YELLOW)-red$(FOLLOW_RED)-blue$(FOLLOW_BLUE)-multi$(FOLLOW_MULTI)-corner$(FOLLOW_CORNER)-clip$(FOLLOW_CLIPPED)-test$(TEST)$(BLUE_BEND_SUFFIX)$(LOOP_RECOVERY_SUFFIX)$(EARLY_BEND_SUFFIX)$(HOME_TRACK_SUFFIX)$(BLUE_STEER_SUFFIX)$(BLUE_RAMP_START_SUFFIX)$(TWO_TURN_SUFFIX)$(YELLOW_EXIT_CUE_SUFFIX)$(YELLOW_LEAD_CUE_SUFFIX)$(BLUE_REVERSAL_SUFFIX)$(BLUE_WIDE_RANGE_SUFFIX)$(FOLLOW_STOP_TRACE_SUFFIX)$(BLUE_RED_HANDOFF_SUFFIX)$(DISTANCE_RUN_SUFFIX)$(WIFI_AUTH_SUFFIX)
PROFILE_FILE := .build_profile

ifneq ($(strip $(shell cat $(PROFILE_FILE) 2>/dev/null)),$(PROFILE_ID))
$(info Build profile is now $(PROFILE_ID); discarding objects built for another profile.)
$(shell find mtkernel_3 -name '*.o' -o -name '*.d' | xargs -r rm -f; \
	  rm -f $(EXE_FILE).elf $(EXE_FILE).map $(EXE_FILE).uf2; \
	  echo $(PROFILE_ID) > $(PROFILE_FILE))
endif


ifeq ($(CONSOLE),usb_cdc)

TINYUSB_PATH := $(PICO_SDK_PATH)/lib/tinyusb

ifeq ($(wildcard $(TINYUSB_PATH)/src/tusb.c),)
$(error TinyUSB not found at $(TINYUSB_PATH). Set PICO_SDK_PATH, or build with CONSOLE=uart)
endif

USBDIR := ../lib/libtm/sysdepend/pico_rp2040/usb

USB_SRCS := usb_console.c usb_descriptors.c usb_sdk_compat.c usb_tinyusb_glue.c
USB_OBJS := $(addprefix ./mtkernel_3/lib/libtm/sysdepend/pico_rp2040/usb/,$(USB_SRCS:.c=.o))

TINYUSB_SRCS := tusb.c \
                common/tusb_fifo.c \
                device/usbd.c \
                device/usbd_control.c \
                class/cdc/cdc_device.c \
                portable/raspberrypi/rp2040/rp2040_usb.c \
                portable/raspberrypi/rp2040/dcd_rp2040.c
TINYUSB_OBJS := $(addprefix ./mtkernel_3/tinyusb/,$(TINYUSB_SRCS:.c=.o))

USB_INCLUDES := -I"$(USBDIR)" \
                -I"$(TINYUSB_PATH)/src" \
                -I"$(PICO_SDK_PATH)/src/common/pico_base_headers/include" \
                -I"$(PICO_SDK_PATH)/src/common/pico_binary_info/include" \
                -I"$(PICO_SDK_PATH)/src/rp2040/pico_platform/include" \
                -I"$(PICO_SDK_PATH)/src/rp2_common/pico_platform_compiler/include" \
                -I"$(PICO_SDK_PATH)/src/rp2_common/pico_platform_sections/include" \
                -I"$(PICO_SDK_PATH)/src/rp2_common/pico_platform_panic/include" \
                -I"$(PICO_SDK_PATH)/src/rp2_common/pico_platform_common/include" \
                -I"$(PICO_SDK_PATH)/src/rp2_common/hardware_base/include" \
                -I"$(PICO_SDK_PATH)/src/rp2_common/hardware_irq/include" \
                -I"$(PICO_SDK_PATH)/src/rp2_common/hardware_resets/include" \
                -I"$(PICO_SDK_PATH)/src/rp2_common/hardware_sync/include" \
                -I"$(PICO_SDK_PATH)/src/rp2_common/hardware_sync_spin_lock/include" \
                -I"$(PICO_SDK_PATH)/src/rp2_common/hardware_timer/include" \
                -I"$(PICO_SDK_PATH)/src/rp2040/hardware_regs/include" \
                -I"$(PICO_SDK_PATH)/src/rp2040/hardware_structs/include"

CFLAGS += -DTM_CONSOLE_USB_CDC=1 -DNDEBUG $(USB_INCLUDES)

OBJS += $(USB_OBJS) $(TINYUSB_OBJS)
C_DEPS += $(USB_OBJS:.o=.d) $(TINYUSB_OBJS:.o=.d)

mtkernel_3/lib/libtm/sysdepend/pico_rp2040/usb/%.o: $(USBDIR)/%.c
	@echo 'Building file: $<'
	@mkdir -p "$(@D)"
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

mtkernel_3/tinyusb/%.o: $(TINYUSB_PATH)/src/%.c
	@echo 'Building TinyUSB file: $<'
	@mkdir -p "$(@D)"
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

else ifneq ($(CONSOLE),uart)
$(error Unknown CONSOLE '$(CONSOLE)'; use usb_cdc or uart)
endif

################################################################################
# CYW43439 radio (polling architecture). NET=lwip below adds lwIP on top,
# still within the same polling model -- see docs/RELEASE.md and
# lib/liblwip/sysdepend/pico_rp2040/.
################################################################################

ifeq ($(WIFI),cyw43)

CYW43_DRIVER_PATH := $(PICO_SDK_PATH)/lib/cyw43-driver
CYW43_PICO_PATH := $(PICO_SDK_PATH)/src/rp2_common/pico_cyw43_driver
WIFIDIR := ../lib/libwifi/sysdepend/pico_rp2040

ifeq ($(wildcard $(CYW43_DRIVER_PATH)/src/cyw43_ll.c),)
$(error CYW43 driver not found below $(PICO_SDK_PATH); initialize the pico-sdk submodules)
endif

WIFI_LOCAL_SRCS := cyw43_utk.c cyw43_utk_kernel.c cyw43_utk_compat.c
WIFI_LOCAL_OBJS := $(addprefix ./mtkernel_3/lib/libwifi/sysdepend/pico_rp2040/,$(WIFI_LOCAL_SRCS:.c=.o))

CYW43_OBJS := ./mtkernel_3/cyw43/cyw43_ll.o \
              ./mtkernel_3/cyw43/cyw43_ctrl.o \
              ./mtkernel_3/cyw43/cyw43_stats.o \
              ./mtkernel_3/cyw43/cyw43_bus_pio_spi.o \
              ./mtkernel_3/cyw43/pio.o \
              ./mtkernel_3/cyw43/dma.o \
              ./mtkernel_3/cyw43/gpio.o

WIFI_INCLUDES := -I"$(WIFIDIR)" \
                 -I"../lib/libtm/sysdepend/pico_rp2040/usb" \
                 -I"$(CYW43_DRIVER_PATH)/src" \
                 -I"$(CYW43_DRIVER_PATH)/firmware" \
                 -I"$(CYW43_PICO_PATH)/include" \
                 -I"$(PICO_SDK_PATH)/src/boards/include" \
                 -I"$(PICO_SDK_PATH)/src/common/pico_base_headers/include" \
                 -I"$(PICO_SDK_PATH)/src/common/hardware_claim/include" \
                 -I"$(PICO_SDK_PATH)/src/common/pico_binary_info/include" \
                 -I"$(PICO_SDK_PATH)/src/common/pico_util/include" \
                 -I"$(PICO_SDK_PATH)/src/rp2040/pico_platform/include" \
                 -I"$(PICO_SDK_PATH)/src/rp2040/hardware_regs/include" \
                 -I"$(PICO_SDK_PATH)/src/rp2040/hardware_structs/include" \
                 -I"$(PICO_SDK_PATH)/src/rp2_common/pico_platform_common/include" \
                 -I"$(PICO_SDK_PATH)/src/rp2_common/pico_platform_compiler/include" \
                 -I"$(PICO_SDK_PATH)/src/rp2_common/pico_platform_sections/include" \
                 -I"$(PICO_SDK_PATH)/src/rp2_common/pico_platform_panic/include" \
                 -I"$(PICO_SDK_PATH)/src/rp2_common/hardware_base/include" \
                 -I"$(PICO_SDK_PATH)/src/rp2_common/hardware_pio/include" \
                 -I"$(PICO_SDK_PATH)/src/rp2_common/hardware_gpio/include" \
                 -I"$(PICO_SDK_PATH)/src/rp2_common/hardware_dma/include" \
                 -I"$(PICO_SDK_PATH)/src/rp2_common/hardware_irq/include" \
                 -I"$(PICO_SDK_PATH)/src/rp2_common/hardware_sync/include" \
                 -I"$(PICO_SDK_PATH)/src/rp2_common/hardware_sync_spin_lock/include" \
                 -I"$(PICO_SDK_PATH)/src/rp2_common/hardware_clocks/include" \
                 -I"$(PICO_SDK_PATH)/src/rp2_common/hardware_resets/include" \
                 -I"$(PICO_SDK_PATH)/src/rp2_common/hardware_timer/include"

CFLAGS += -DTM_WIFI_CYW43=1 -DCYW43_LWIP=$(if $(filter lwip,$(NET)),1,0) -DCYW43_ENABLE_BLUETOOTH=0 \
          -DCYW43_USE_OTP_MAC=1 -DPICO_CYW43_LOGGING_ENABLED=0 \
          -DPICO_RP2040=1 -DPICO_32BIT=1 -DPICO_ON_DEVICE=1 -DPICO_BUILD=1 \
          -DPICO_NO_HARDWARE=0 -DRASPBERRYPI_PICO_W=1 \
          -DCYW43_DEFAULT_PIN_WL_REG_ON=23u \
          -DCYW43_DEFAULT_PIN_WL_DATA_OUT=24u \
          -DCYW43_DEFAULT_PIN_WL_DATA_IN=24u \
          -DCYW43_DEFAULT_PIN_WL_HOST_WAKE=24u \
          -DCYW43_DEFAULT_PIN_WL_CLOCK=29u \
          -DCYW43_DEFAULT_PIN_WL_CS=25u \
          -DNDEBUG -ffunction-sections -fdata-sections $(WIFI_INCLUDES)
LFLAGS += -Wl,--gc-sections

OBJS += $(WIFI_LOCAL_OBJS) $(CYW43_OBJS)
C_DEPS += $(WIFI_LOCAL_OBJS:.o=.d) $(CYW43_OBJS:.o=.d)

mtkernel_3/lib/libwifi/sysdepend/pico_rp2040/%.o: $(WIFIDIR)/%.c
	@echo 'Building Wi-Fi port file: $<'
	@mkdir -p "$(@D)"
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

mtkernel_3/cyw43/cyw43_ll.o: $(CYW43_DRIVER_PATH)/src/cyw43_ll.c
	@mkdir -p "$(@D)"
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

mtkernel_3/cyw43/cyw43_ctrl.o: $(CYW43_DRIVER_PATH)/src/cyw43_ctrl.c
	@mkdir -p "$(@D)"
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

mtkernel_3/cyw43/cyw43_stats.o: $(CYW43_DRIVER_PATH)/src/cyw43_stats.c
	@mkdir -p "$(@D)"
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

mtkernel_3/cyw43/cyw43_bus_pio_spi.o: $(CYW43_PICO_PATH)/cyw43_bus_pio_spi.c
	@mkdir -p "$(@D)"
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

mtkernel_3/cyw43/pio.o: $(PICO_SDK_PATH)/src/rp2_common/hardware_pio/pio.c
	@mkdir -p "$(@D)"
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

mtkernel_3/cyw43/dma.o: $(PICO_SDK_PATH)/src/rp2_common/hardware_dma/dma.c
	@mkdir -p "$(@D)"
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

mtkernel_3/cyw43/gpio.o: $(PICO_SDK_PATH)/src/rp2_common/hardware_gpio/gpio.c
	@mkdir -p "$(@D)"
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

ifeq ($(NET),lwip)

LWIPDIR := $(PICO_SDK_PATH)/lib/lwip/src
LIBLWIP_PORT := ../lib/liblwip/sysdepend/pico_rp2040

ifeq ($(wildcard $(LWIPDIR)/core/init.c),)
$(error lwIP not found below $(PICO_SDK_PATH); initialize the pico-sdk submodules)
endif

# COREFILES + CORE4FILES (IPv4 only) + the one NETIFFILES entry we need, per
# $(PICO_SDK_PATH)/lib/lwip/src/Filelists.mk. altcp*.c is included because
# apps/mqtt/mqtt.c (MQTT=1, below) is written against the altcp API -- used
# here in its default plain-TCP passthrough mode (LWIP_ALTCP=0), not TLS.
LWIP_CORE_SRCS := core/init.c core/def.c core/dns.c core/inet_chksum.c \
                   core/ip.c core/mem.c core/memp.c core/netif.c core/pbuf.c \
                   core/raw.c core/stats.c core/sys.c \
                   core/altcp.c core/altcp_alloc.c core/altcp_tcp.c \
                   core/tcp.c core/tcp_in.c core/tcp_out.c core/timeouts.c \
                   core/udp.c \
                   core/ipv4/acd.c core/ipv4/autoip.c core/ipv4/dhcp.c \
                   core/ipv4/etharp.c core/ipv4/icmp.c core/ipv4/igmp.c \
                   core/ipv4/ip4_frag.c core/ipv4/ip4.c core/ipv4/ip4_addr.c \
                   netif/ethernet.c
LWIP_CORE_OBJS := $(addprefix ./mtkernel_3/lwip/,$(LWIP_CORE_SRCS:.c=.o))

LWIP_PORT_OBJS := ./mtkernel_3/lib/liblwip/sysdepend/pico_rp2040/sys_arch.o

CYW43_LWIP_OBJS := ./mtkernel_3/cyw43/cyw43_lwip.o

LWIP_INCLUDES := -I"$(LIBLWIP_PORT)/include" \
                 -I"$(LIBLWIP_PORT)" \
                 -I"$(LWIPDIR)/include"

CFLAGS += -DTM_NET_LWIP=1 $(LWIP_INCLUDES)

OBJS += $(LWIP_CORE_OBJS) $(LWIP_PORT_OBJS) $(CYW43_LWIP_OBJS)
C_DEPS += $(LWIP_CORE_OBJS:.o=.d) $(LWIP_PORT_OBJS:.o=.d) $(CYW43_LWIP_OBJS:.o=.d)

mtkernel_3/lwip/%.o: $(LWIPDIR)/%.c
	@echo 'Building lwIP file: $<'
	@mkdir -p "$(@D)"
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

mtkernel_3/lib/liblwip/sysdepend/pico_rp2040/%.o: ../lib/liblwip/sysdepend/pico_rp2040/%.c
	@echo 'Building lwIP port file: $<'
	@mkdir -p "$(@D)"
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

mtkernel_3/cyw43/cyw43_lwip.o: $(CYW43_DRIVER_PATH)/src/cyw43_lwip.c
	@echo 'Building file: $<'
	@mkdir -p "$(@D)"
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

ifeq ($(MQTT),1)

MQTT_CORE_OBJS := ./mtkernel_3/lwip/apps/mqtt/mqtt.o
MQTT_PORT_OBJS := ./mtkernel_3/lib/liblwip/sysdepend/pico_rp2040/mqtt_utk.o

CFLAGS += -DTM_MQTT=1

OBJS += $(MQTT_CORE_OBJS) $(MQTT_PORT_OBJS)
C_DEPS += $(MQTT_CORE_OBJS:.o=.d) $(MQTT_PORT_OBJS:.o=.d)

mtkernel_3/lwip/apps/mqtt/mqtt.o: $(LWIPDIR)/apps/mqtt/mqtt.c
	@echo 'Building lwIP file: $<'
	@mkdir -p "$(@D)"
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

endif

endif

endif

ifneq ($(filter 1,$(RGB_DIAG) $(FOLLOW_APP)),)
CFLAGS += -I"$(PICO_SDK_PATH)/src/rp2_common/hardware_i2c/include" \
          -I"$(PICO_SDK_PATH)/src/common/pico_time/include" \
          -I"$(PICO_SDK_PATH)/src/common/pico_sync/include"
RGB_SDK_OBJS := ./mtkernel_3/rgb_sdk/i2c.o ./mtkernel_3/rgb_sdk/timeout_helper.o
OBJS += $(RGB_SDK_OBJS)
C_DEPS += $(RGB_SDK_OBJS:.o=.d)
mtkernel_3/rgb_sdk/i2c.o: $(PICO_SDK_PATH)/src/rp2_common/hardware_i2c/i2c.c
	@mkdir -p "$(@D)"
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
mtkernel_3/rgb_sdk/timeout_helper.o: $(PICO_SDK_PATH)/src/common/pico_time/timeout_helper.c
	@mkdir -p "$(@D)"
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
endif

include mtkernel_3/lib/libtm/sysdepend/pico_rp2040/subdir.mk
include mtkernel_3/lib/libtm/sysdepend/no_device/subdir.mk
include mtkernel_3/lib/libtk/sysdepend/cpu/rp2040/subdir.mk
include mtkernel_3/lib/libtk/sysdepend/cpu/core/armv6m/subdir.mk
include mtkernel_3/lib/libbsp/sysdepend/cpu/rp2040/subdir.mk
include mtkernel_3/kernel/sysdepend/pico_rp2040/subdir.mk
include mtkernel_3/kernel/sysdepend/cpu/rp2040/subdir.mk
include mtkernel_3/kernel/sysdepend/cpu/core/armv6m/subdir.mk
