SRC_DIR := .
BUILD_ROOT := ../build
BIN_DIR := ../bin

THIRD_PARTY_DIR := ../build/3rd-party
UDEV_VERSION := v0.0.1
UDEV_NAME := libidpudev-$(patsubst v%,%,$(UDEV_VERSION))
UDEV_DIR := $(THIRD_PARTY_DIR)/$(UDEV_NAME)
UDEV_READY := $(UDEV_DIR)/lib/crt0.rel

BUILD_DIR := $(BUILD_ROOT)/$(TARGET)
OUT_BASE := $(BUILD_DIR)/$(TARGET)

CC := sdcc
AS := sdasz80
LD := sdldz80
OBJCOPY := sdobjcopy

CFLAGS := --std-c11 -mz80 --debug \
	--no-std-crt0 --nostdinc --nostdlib \
	-I. -I$(UDEV_DIR)/include
ASFLAGS := -xlos -g

CRT0_OBJ := $(abspath $(UDEV_DIR))/lib/crt0.rel

C_SRCS := $(wildcard $(SRC_DIR)/*.c) \
	$(wildcard $(SRC_DIR)/game/*.c) \
	$(wildcard $(SRC_DIR)/platform/*.c)
C_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.rel,$(C_SRCS))
S_SRCS := $(wildcard $(SRC_DIR)/*.s) \
	$(wildcard $(SRC_DIR)/game/*.s) \
	$(wildcard $(SRC_DIR)/platform/*.s)
S_OBJS := $(patsubst $(SRC_DIR)/%.s,$(BUILD_DIR)/%.rel,$(S_SRCS))

IHX := $(OUT_BASE).ihx
COM := $(BIN_DIR)/$(TARGET).com
LINKFK := $(OUT_BASE).lk
MAP := $(OUT_BASE).map

LIBS := $(abspath $(UDEV_DIR))/lib/usdcc.lib \
	$(abspath $(UDEV_DIR))/lib/ulibc.lib \
	$(abspath $(UDEV_DIR))/lib/ugpx.lib

all: $(COM)
	@echo "ok: $(notdir $(COM))"

$(UDEV_READY):
	@echo "missing idp-udev release package under $(UDEV_DIR)"
	@echo "run 'make fetch-udev' from project root"
	@exit 1

$(BUILD_DIR)/%.rel: $(SRC_DIR)/%.c | $(UDEV_READY)
	@mkdir -p $(dir $@)
	@echo "[cc] $< -> $@"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.rel: $(SRC_DIR)/%.s | $(UDEV_READY)
	@mkdir -p $(dir $@)
	@echo "[as] $< -> $@"
	@$(AS) $(ASFLAGS) -o $@ $<

$(IHX): $(CRT0_OBJ) $(C_OBJS) $(S_OBJS) $(UDEV_READY)
	@mkdir -p $(BUILD_DIR)
	@echo "[link] generating $(LINKFK)"
	@{ \
	  echo "-b_CODE=0x100"; \
	  echo "-i"; \
	  echo "-m"; \
	  echo "-j"; \
	  echo "-o $(notdir $(TARGET)).ihx"; \
	  echo "$(abspath $(CRT0_OBJ))"; \
	  for obj in $(abspath $(C_OBJS)); do echo "$$obj"; done; \
	  for obj in $(abspath $(S_OBJS)); do echo "$$obj"; done; \
	  for lib in $(LIBS); do echo "$$lib"; done; \
	} > $(LINKFK)
	@echo "[link] $(LD) -f $(notdir $(LINKFK))"
	@cd $(BUILD_DIR) && $(LD) -f $(notdir $(LINKFK))
	@[ -f "$(BUILD_DIR)/$(TARGET).map" ] && mv -f "$(BUILD_DIR)/$(TARGET).map" "$(MAP)" || true

$(COM): $(IHX)
	@mkdir -p $(BIN_DIR)
	@echo "[objcopy] $< -> $@"
	@$(OBJCOPY) -I ihex -O binary $< $@

clean:
	@rm -rf "$(BUILD_DIR)" "$(BIN_DIR)"

.PHONY: all clean
