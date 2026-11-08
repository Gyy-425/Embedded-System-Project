################################################################################
# micro T-Kernel 3.0 BSP  makefile
################################################################################

# app_program/*.c plus one level of subfolders (one per subsystem)
TEMP_SRCS = $(wildcard ../app_program/*.c) $(wildcard ../app_program/*/*.c)
TEMP_OBJS = $(TEMP_SRCS:.c=.o)
TEMP_DEPS = $(TEMP_SRCS:.c=.d)

OBJS += $(subst ../, ./mtkernel_3/, $(TEMP_OBJS))
C_DEPS += $(subst ../, ./mtkernel_3/, $(TEMP_DEPS))

# New communication modules use strict diagnostics without reformatting the
# inherited RTOS/SDK implementation.
mtkernel_3/app_program/subsystem1/colour_classifier.o: CFLAGS += -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes
mtkernel_3/app_program/subsystem2/encoder_pio_count.o: CFLAGS += -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes
mtkernel_3/app_program/subsystem2/encoder_capture_pio.o: CFLAGS += -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes
mtkernel_3/app_program/subsystem2/encoder_pio_io_rp2040.o: CFLAGS += -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes
mtkernel_3/app_program/subsystem3/comm_protocol.o: CFLAGS += -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes
mtkernel_3/app_program/subsystem3/mqtt_app.o: CFLAGS += -Wall -Wextra -Werror -Wshadow -Wstrict-prototypes
mtkernel_3/app_program/subsystem2/motor_pulse.o: CFLAGS += -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes
mtkernel_3/app_program/subsystem2/motion_control.o: CFLAGS += -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes
mtkernel_3/app_program/subsystem2/motion_button.o: CFLAGS += -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes
mtkernel_3/app_program/subsystem2/motion_runtime.o: CFLAGS += -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes
mtkernel_3/app_program/subsystem3/drive_protocol.o: CFLAGS += -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes
mtkernel_3/app_program/subsystem3/drive_control.o: CFLAGS += -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes
mtkernel_3/app_program/subsystem3/mqtt_drive_app.o: CFLAGS += -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes
mtkernel_3/app_program/subsystem3/drive_platform_rp2040.o: CFLAGS += -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes
mtkernel_3/app_program/subsystem3/drive_entropy_rp2040.o: CFLAGS += -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes

mtkernel_3/app_program/%.o: ../app_program/%.c
	@echo 'Building file: $<'
	@mkdir -p $(dir $@)
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '
