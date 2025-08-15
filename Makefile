NDEBUG	=	1

# ARCDIR0 = constants
# ARCDIR1 = gba

# ARCDIRS		=	$(ARCDIR0) $(ARCDIR1)

# .AFILES		=	$(addprefix lib,$(addsuffix .a,$(ARCDIRS)))
.SFILES		=	crt0.s rom_header.s
.CFILES		=	main.c
.OFILES		=	$(.SFILES:.s=.o) $(.CFILES:.c=.o)

ASFLAGS		=	-mthumb-interwork
CFLAGS		=	-g -c -O2 -mthumb-interwork -nostdlib -fno-common -DNDEBUG
LDFLAGS		+=  -Map $(MAPFILE) -nostartfiles \
				-T ld_script.ld # \
# 				-L. $(addprefix -l,$(ARCDIRS))

MAPFILE		=	rom.map
TARGET_ELF	=	rom.elf
TARGET_BIN	=	rom.gba

default:
# 	@$(foreach ARCDIR_TMP, $(ARCDIRS), make -C $(ARCDIR_TMP);)
	@make $(TARGET_BIN)

$(TARGET_BIN): $(TARGET_ELF)
	objcopy -v -O binary $< $@

$(TARGET_ELF): $(.OFILES)
	@echo > $(MAPFILE)
	$(CC) -g -o $@ $(.OFILES) -Wl,$(LDFLAGS)


.PHONY: all clean


all:	clean default

clean:
	-rm -rf $(.OFILES) $(MAPFILE) $(TARGET_ELF) $(TARGET_BIN)
# 	@$(foreach ARCDIR_TMP, $(ARCDIRS), make -C $(ARCDIR_TMP) clean;)
