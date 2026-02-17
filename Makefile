APP_NAME := idp-pong
ROOT := $(realpath .)
BUILD_DIR := $(ROOT)/build
BIN_DIR := $(ROOT)/bin

THIRD_PARTY_DIR := $(BUILD_DIR)/3rd-party
UDEV_VERSION := v0.0.1
UDEV_NAME := idp-udev-$(patsubst v%,%,$(UDEV_VERSION))
UDEV_ARCHIVE := $(THIRD_PARTY_DIR)/$(UDEV_NAME).tar.gz
UDEV_DIR := $(THIRD_PARTY_DIR)/$(UDEV_NAME)
UDEV_FETCH_STAMP := $(UDEV_DIR)/.fetched-$(UDEV_VERSION)
UDEV_BUILD_DIR := $(BUILD_DIR)/udev
UDEV_BIN_DIR := $(BUILD_DIR)/udev-bin
UDEV_LIB_STAMP := $(UDEV_BIN_DIR)/.built-$(UDEV_VERSION)

PARTNER_BUILD_DIR := $(BUILD_DIR)/partner
PARTNER_TARGET := $(BIN_DIR)/$(APP_NAME).com
PARTNER_IHX := $(PARTNER_BUILD_DIR)/$(APP_NAME).ihx
PARTNER_LINK := $(PARTNER_BUILD_DIR)/$(APP_NAME).link

COMMON_SRC := src/main.c src/game/game.c
PARTNER_SRC := $(COMMON_SRC) src/platform/platform_partner.c
PARTNER_OBJ := $(PARTNER_SRC:%.c=$(PARTNER_BUILD_DIR)/%.rel)

CRT0_SRC := $(UDEV_DIR)/src/ulibc/crt0.s
CRT0_OBJ := $(PARTNER_BUILD_DIR)/crt0.rel

DOCKER ?= docker
SDCC_IMAGE ?= wischner/sdcc-z80:latest
HOST_UID := $(shell id -u)
HOST_GID := $(shell id -g)

DOCKER_MAKE = $(DOCKER) run --rm \
	-u $(HOST_UID):$(HOST_GID) \
	-v $(ROOT):$(ROOT) \
	-w $(ROOT) \
	-e IN_DOCKER=1 \
	$(SDCC_IMAGE) \
	make -C $(ROOT)

SDCC := sdcc
AS := sdasz80
LD := sdldz80
OBJCOPY := sdobjcopy
PARTNER_INC_DIRS := src $(UDEV_DIR)/include
CFLAGS := --std-c11 -mz80 --debug --no-std-crt0 --nostdinc --nostdlib $(addprefix -I,$(PARTNER_INC_DIRS))
ASFLAGS := -xlos -g
LDFLAGS := -mz80 -Wl -y --code-loc 0x100 --no-std-crt0 --nostdinc \
	$(addprefix -L,$(UDEV_BIN_DIR)) -lusdcc -lulibc -lugpx -p
L2FIX := sed '/-b _DATA = 0x8000/d'

ifeq ($(IN_DOCKER),1)

all: __inner_partner

partner: __inner_partner

fetch-udev: $(UDEV_FETCH_STAMP)

$(UDEV_LIB_STAMP): $(UDEV_FETCH_STAMP)
	$(MAKE) -C $(UDEV_DIR) IN_DOCKER=1 BUILD_DIR=$(UDEV_BUILD_DIR) BIN_DIR=$(UDEV_BIN_DIR)
	touch $@

$(CRT0_OBJ): $(CRT0_SRC) | $(UDEV_LIB_STAMP)
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $@ $<

$(PARTNER_BUILD_DIR)/%.rel: %.c | $(UDEV_LIB_STAMP)
	@mkdir -p $(dir $@)
	$(SDCC) -c -o $@ $< $(CFLAGS)

$(PARTNER_IHX): $(CRT0_OBJ) $(PARTNER_OBJ) $(UDEV_LIB_STAMP)
	@mkdir -p $(PARTNER_BUILD_DIR)
	$(SDCC) $(LDFLAGS) -o $@ $(CRT0_OBJ) $(PARTNER_OBJ)
	$(L2FIX) $(PARTNER_BUILD_DIR)/$(APP_NAME).lk > $(PARTNER_LINK)
	$(LD) -nf $(PARTNER_LINK)

$(PARTNER_TARGET): $(PARTNER_IHX)
	@mkdir -p $(BIN_DIR)
	$(OBJCOPY) -I ihex -O binary $(PARTNER_IHX) $@

__inner_partner: $(PARTNER_TARGET)

__inner_clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

else

REQUIRED = docker curl tar
K := $(foreach exec,$(REQUIRED),\
	$(if $(shell which $(exec)),,$(error "$(exec) not found. Please install or add to path.")))

all: partner

partner: $(UDEV_FETCH_STAMP)
	$(DOCKER_MAKE) __inner_partner

fetch-udev: $(UDEV_FETCH_STAMP)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

docker-pull:
	$(DOCKER) pull $(SDCC_IMAGE)

endif

$(UDEV_FETCH_STAMP):
	@mkdir -p $(THIRD_PARTY_DIR)
	@if [ ! -d "$(UDEV_DIR)" ]; then \
		curl -L --max-time 120 -o $(UDEV_ARCHIVE) https://github.com/iskra-delta/idp-udev/archive/refs/tags/$(UDEV_VERSION).tar.gz; \
		tar -xzf $(UDEV_ARCHIVE) -C $(THIRD_PARTY_DIR); \
	fi
	touch $@

.PHONY: all partner fetch-udev clean docker-pull __inner_partner __inner_clean
