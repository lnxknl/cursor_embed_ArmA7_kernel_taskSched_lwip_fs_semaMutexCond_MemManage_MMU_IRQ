# 编译器设置
CROSS_COMPILE ?= arm-none-eabi-
CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump

# 目录设置
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

# 编译标志
ASFLAGS = -mcpu=cortex-a7
CFLAGS = -mcpu=cortex-a7 -Wall -O2 -ffreestanding -nostdlib -nostartfiles \
         -I$(INC_DIR) -DQEMU_PLATFORM
LDFLAGS = -T $(SRC_DIR)/linker.ld -nostdlib

# 源文件
ASRC = $(wildcard $(SRC_DIR)/*.s)
CSRC = $(wildcard $(SRC_DIR)/*.c)

# 目标文件
AOBJ = $(ASRC:$(SRC_DIR)/%.s=$(BUILD_DIR)/%.o)
COBJ = $(CSRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
OBJ = $(AOBJ) $(COBJ)

# 最终目标
TARGET = $(BUILD_DIR)/kernel.elf
TARGET_BIN = $(BUILD_DIR)/kernel.bin

.PHONY: all clean qemu debug

all: $(TARGET_BIN)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s
	@mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $^
	$(OBJDUMP) -D $@ > $(BUILD_DIR)/kernel.list

$(TARGET_BIN): $(TARGET)
	$(OBJCOPY) -O binary $< $@

qemu: $(TARGET_BIN)
	qemu-system-arm -M vexpress-a9 -m 128M -nographic -kernel $(TARGET_BIN) -S -s

debug: $(TARGET_BIN)
	qemu-system-arm -M vexpress-a9 -m 128M -nographic -kernel $(TARGET_BIN) -S -s &
	arm-none-eabi-gdb $(TARGET) -x gdb.script

clean:
	rm -rf $(BUILD_DIR) 