######################################
# target
######################################
TARGET = Fv1_bootloader

######################################
# building variables
######################################
DEBUG = 0
OPT = -Os

#######################################
# paths
#######################################
BUILD_DIR = build

# Shared driver paths (no duplication)
DRIVERS = ../Fv1_programmer/Drivers
MIDDLEWARES = ../Fv1_programmer/Middlewares

######################################
# source
######################################
C_SOURCES =  \
Core/Src/main.c \
Core/Src/system_stm32l0xx.c \
Core/Src/stm32l0xx_it.c \
Core/Src/stm32l0xx_hal_msp.c \
Core/Src/syscalls.c \
Core/Src/sysmem.c \
$(DRIVERS)/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal.c \
$(DRIVERS)/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_rcc.c \
$(DRIVERS)/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_rcc_ex.c \
$(DRIVERS)/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_gpio.c \
$(DRIVERS)/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_flash.c \
$(DRIVERS)/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_flash_ex.c \
$(DRIVERS)/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_cortex.c \
$(DRIVERS)/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_pwr.c \
$(DRIVERS)/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_pwr_ex.c \
$(DRIVERS)/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_crc.c \
$(DRIVERS)/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_crc_ex.c \
Core/Src/flash_driver.c \
Core/Src/crc32.c \
USB_DEVICE/App/usb_device.c \
USB_DEVICE/App/usbd_desc.c \
USB_DEVICE/App/usbd_cdc_if.c \
USB_DEVICE/Target/usbd_conf.c \
$(DRIVERS)/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_pcd.c \
$(DRIVERS)/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_pcd_ex.c \
$(DRIVERS)/STM32L0xx_HAL_Driver/Src/stm32l0xx_ll_usb.c \
$(MIDDLEWARES)/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c \
$(MIDDLEWARES)/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c \
$(MIDDLEWARES)/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c \
$(MIDDLEWARES)/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.c

ASM_SOURCES = \
startup_stm32l053xx.s

ASMM_SOURCES =

#######################################
# binaries
#######################################
PREFIX = arm-none-eabi-
ifdef GCC_PATH
CC = $(GCC_PATH)/$(PREFIX)gcc
AS = $(GCC_PATH)/$(PREFIX)gcc -x assembler-with-cpp
CP = $(GCC_PATH)/$(PREFIX)objcopy
SZ = $(GCC_PATH)/$(PREFIX)size
else
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
endif
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

#######################################
# CFLAGS
#######################################
CPU = -mcpu=cortex-m0plus
MCU = $(CPU) -mthumb

AS_DEFS =

C_DEFS = \
-DUSE_HAL_DRIVER \
-DSTM32L053xx

AS_INCLUDES =

C_INCLUDES = \
-ICore/Inc \
-IUSB_DEVICE/App \
-IUSB_DEVICE/Target \
-I$(MIDDLEWARES)/ST/STM32_USB_Device_Library/Core/Inc \
-I$(MIDDLEWARES)/ST/STM32_USB_Device_Library/Class/CDC/Inc \
-I$(DRIVERS)/STM32L0xx_HAL_Driver/Inc \
-I$(DRIVERS)/STM32L0xx_HAL_Driver/Inc/Legacy \
-I$(DRIVERS)/CMSIS/Device/ST/STM32L0xx/Include \
-I$(DRIVERS)/CMSIS/Include

ASFLAGS = $(MCU) $(AS_DEFS) $(AS_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

CFLAGS += $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif

CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

#######################################
# LDFLAGS
#######################################
LDSCRIPT = STM32L053XX_FLASH_BOOT.ld

LIBS = -lc -lm -lnosys
LIBDIR =
LDFLAGS = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

#######################################
# build the application
#######################################
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASMM_SOURCES:.S=.o)))
vpath %.S $(sort $(dir $(ASMM_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@
$(BUILD_DIR)/%.o: %.S Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@

$(BUILD_DIR):
	mkdir $@

#######################################
# clean up
#######################################
clean:
	-rm -fR $(BUILD_DIR)

#######################################
# OpenOCD flash and debug
#######################################
OPENOCD = openocd
OPENOCD_INTERFACE = stlink
OPENOCD_TARGET = stm32l0

flash: $(BUILD_DIR)/$(TARGET).elf
	$(OPENOCD) -f interface/$(OPENOCD_INTERFACE).cfg -f target/$(OPENOCD_TARGET).cfg -c "program $(BUILD_DIR)/$(TARGET).elf verify reset exit"

erase:
	$(OPENOCD) -f interface/$(OPENOCD_INTERFACE).cfg -f target/$(OPENOCD_TARGET).cfg -c "init; reset halt; flash erase_address 0x08000000 0x10000; exit"

flash_erase: $(BUILD_DIR)/$(TARGET).elf
	$(OPENOCD) -f interface/$(OPENOCD_INTERFACE).cfg -f target/$(OPENOCD_TARGET).cfg -c "init; reset halt; flash erase_address 0x08000000 0x10000; program $(BUILD_DIR)/$(TARGET).elf verify reset exit"

debug: $(BUILD_DIR)/$(TARGET).elf
	$(OPENOCD) -f interface/$(OPENOCD_INTERFACE).cfg -f target/$(OPENOCD_TARGET).cfg

#######################################
# dependencies
#######################################
-include $(wildcard $(BUILD_DIR)/*.d)
