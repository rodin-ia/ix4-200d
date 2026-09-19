TARGET := ix4lcd

OPENWRT_DIR := /home/master/build_dir/openwrt
STAGING_DIR := $(OPENWRT_DIR)/staging_dir

CC := $(STAGING_DIR)/toolchain-arm_xscale_gcc-14.4.0_musl_eabi/bin/arm-openwrt-linux-muslgnueabi-gcc

CFLAGS := -march=armv5te -O2 -Wall -Wextra

SRC := \
	ix4lcd.c \
	menu.c \
	simulate_button.c \
	page_device.c \
	page_space.c \
	page_ip.c \
	page_datetime.c \
	page_storage.c

OBJ := $(SRC:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	STAGING_DIR=$(STAGING_DIR) $(CC) $(CFLAGS) -o $@ $^

%.o: %.c ix4lcd.h
	STAGING_DIR=$(STAGING_DIR) $(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
