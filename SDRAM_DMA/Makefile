CROSS ?= arm-none-eabi

CC      := $(CROSS)-gcc
OBJCOPY := $(CROSS)-objcopy
SIZE    := $(CROSS)-size

TARGET := mimxrt1020_baremetal

BUILD_DIR := build
SRC_DIRS := src startup
INCLUDES := -Iinclude

CPUFLAGS := -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16
CFLAGS := $(CPUFLAGS) -std=c11 -ffreestanding -fdata-sections -ffunction-sections -Wall -Wextra -Wpedantic $(INCLUDES)
LDFLAGS := $(CPUFLAGS) -Tlinker/MIMXRT1020_flexspi.ld -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(TARGET).map

SRCS := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean size

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin

$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

size: $(BUILD_DIR)/$(TARGET).elf
	$(SIZE) $<

