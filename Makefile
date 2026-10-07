BUILD_DIR=build
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/main.o

all: tiltball.z64

$(BUILD_DIR)/tiltball.elf: $(OBJS)

tiltball.z64: N64_ROM_TITLE = "Tilt Ball"

clean:
	rm -rf $(BUILD_DIR) *.z64

-include $(wildcard $(BUILD_DIR)/*.d)
.PHONY: all clean
