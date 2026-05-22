NDEBUG	=	1

CC		=	"E:/Program Files (x86)/GNU Arm Embedded Toolchain/10 2020-q4-major/bin/arm-none-eabi-gcc.exe"
AS		=	"E:/Program Files (x86)/GNU Arm Embedded Toolchain/10 2020-q4-major/bin/arm-none-eabi-as.exe"
OBJCOPY	=	"E:/Program Files (x86)/GNU Arm Embedded Toolchain/10 2020-q4-major/bin/arm-none-eabi-objcopy.exe"

# Assembly source files (root only)
.SFILES		=	crt0.s rom_header.s bios_calls.s

# C source files — core
.CFILES		=	main.c gameutils.c

# Subdirectories containing game modules
SUBDIRS		=	00title 01select 02game 04pause 06mini 07shop 08demo 09ready 10delete 12ending

# Auto-discover all .c files in subdirectories
.CFILES		+=	$(foreach dir,$(SUBDIRS),$(wildcard $(dir)/*.c))

# Data version: set DATA_VERSION=alt for alternate title screen data
# Both wario4_scene_data.c and wario4_scene_data_alt.c define the same symbols;
# remove the alt version from the auto-discovered list, then add back if needed.
.CFILES		:=	$(filter-out %_alt.c,$(.CFILES))

DATA_VERSION ?= ida
ifeq ($(DATA_VERSION),alt)
.CFILES		+=	00title/wario4_scene_data_alt.c
CFLAGS		+=	-DDATA_VERSION_ALT
else
CFLAGS		+=	-DDATA_VERSION_IDA
endif

# Object files derived from source files
.OFILES		=	$(.SFILES:.s=.o) $(.CFILES:.c=.o)

ASFLAGS		=	-mthumb-interwork
CFLAGS		=	-g -c -O2 -mthumb-interwork -nostdlib -fno-common -DNDEBUG
LDFLAGS		+=	-Map $(MAPFILE) -nostartfiles \
				-T ld_script.ld

MAPFILE		=	rom.map
TARGET_ELF	=	rom.elf
TARGET_BIN	=	rom.gba

default:
	@make $(TARGET_BIN)

$(TARGET_BIN): $(TARGET_ELF)
	$(OBJCOPY) -v -O binary $< $@

$(TARGET_ELF): $(.OFILES)
	@echo > $(MAPFILE)
	$(CC) -g -o $@ $(.OFILES) -Wl,$(LDFLAGS)


.PHONY: all clean


all:	clean default

clean:
	-rm -rf $(.OFILES) $(MAPFILE) $(TARGET_ELF) $(TARGET_BIN)
