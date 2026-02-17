TARGET ?= idp-pong

DOCKER_IMAGE ?= wischner/sdcc-z80:latest
WORKDIR := $(PWD)
UID := $(shell id -u)
GID := $(shell id -g)

THIRD_PARTY_DIR := build/3rd-party
UDEV_VERSION := v0.0.1
UDEV_NAME := libidpudev-$(patsubst v%,%,$(UDEV_VERSION))
UDEV_ARCHIVE := $(THIRD_PARTY_DIR)/$(UDEV_NAME).tar.gz
UDEV_DIR := $(THIRD_PARTY_DIR)/$(UDEV_NAME)
UDEV_FETCH_STAMP := $(UDEV_DIR)/.fetched-$(UDEV_VERSION)

DOCKER_RUN = docker run --rm \
	--user $(UID):$(GID) \
	-v "$(WORKDIR):/work" -w /work \
	$(DOCKER_IMAGE) env PATH=/opt/sdcc/bin:$$PATH

all: $(TARGET)

$(TARGET): $(UDEV_FETCH_STAMP)
	@echo "[host] building (inside docker) -> bin/$(TARGET).com"
	@$(DOCKER_RUN) sh -c 'make -C src TARGET=$(TARGET) all'

build: $(TARGET)

rebuild:
	@$(DOCKER_RUN) sh -c 'make -C src TARGET=$(TARGET) clean'
	@$(MAKE) all

clean:
	@echo "[host] removing ./build and ./bin"
	@rm -rf build
	@rm -rf bin

docker-pull:
	@echo "[host] pulling docker image $(DOCKER_IMAGE) ..."
	@docker pull $(DOCKER_IMAGE)

fetch-udev:
	@$(MAKE) $(UDEV_FETCH_STAMP)

$(UDEV_FETCH_STAMP):
	@mkdir -p "$(THIRD_PARTY_DIR)"
	@if [ ! -d "$(UDEV_DIR)" ]; then \
		curl -L --max-time 120 -o "$(UDEV_ARCHIVE)" "https://github.com/iskra-delta/idp-udev/releases/download/$(UDEV_VERSION)/$(UDEV_NAME).tar.gz"; \
		tar -xzf "$(UDEV_ARCHIVE)" -C "$(THIRD_PARTY_DIR)"; \
	fi
	@touch "$@"

.PHONY: all $(TARGET) build rebuild clean docker-pull fetch-udev
