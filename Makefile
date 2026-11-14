export PS3DEV ?= /mnt/d/ps3dev-sdk
export PSL1GHT ?= $(PS3DEV)
PORTLIBS := $(PS3DEV)/portlibs/ppu
CC := $(PS3DEV)/ppu/bin/ppu-gcc

SRC_DIR := src/src
OBJ_DIR := build/obj
TARGET := build/sdlpop.elf

# Same machine flags as the working PS3 ports (PCE-PS3, CPC-PS3): see ppu_rules.
MACHDEP := -mhard-float -fmodulo-sched -ffunction-sections -fdata-sections

CFLAGS := -O2 -mcpu=cell -std=gnu99 -fcommon -fno-strict-aliasing -fsigned-char $(MACHDEP) \
          -fno-tree-vectorize -fno-tree-slp-vectorize -DSTB_VORBIS_BIG_ENDIAN -Wall \
          -Wno-unused-but-set-variable -Wno-unused-variable -Wno-pointer-sign \
          -DPS3 -D_GNU_SOURCE=1 -DSDL_MAIN_HANDLED \
          -I$(SRC_DIR) -Iplatform/ps3 \
          -I$(PS3DEV)/ppu/include -I$(PS3DEV)/ppu/powerpc64-ps3-elf/include -I$(PORTLIBS)/include -I$(PORTLIBS)/include/SDL2

LDFLAGS := $(MACHDEP) -L$(PS3DEV)/ppu/lib -L$(PORTLIBS)/lib

LIBS := platform/ps3/SDL_PSL1GHTrender.o -lSDL2_image -lpng -ljpeg -lSDL2 \
        -lgcm_sys -lio -laudio -lsysutil -lsysmodule -lrsx -lrt -llv2 -lm -lz -lc

SRC_NAMES := main data seg000 seg001 seg002 seg003 seg004 seg005 seg006 seg007 seg008 seg009 \
             seqtbl replay options lighting screenshot menu midi opl3 stb_vorbis
OBJS := $(addprefix $(OBJ_DIR)/,$(addsuffix .o,$(SRC_NAMES))) $(OBJ_DIR)/ps3_platform.o $(OBJ_DIR)/ps3_audio.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/ps3_platform.o: platform/ps3/ps3_platform.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/ps3_audio.o: platform/ps3/ps3_audio.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build

.PHONY: all clean
