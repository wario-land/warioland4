#include "../gba/gba.h"

// Scene7 OBJ pattern data

const u16 scene7_cat_000[] ALIGNED(4) = {
	2,
	0x00E8, 0x41F8, 0xC600,
	0x40F8, 0x01F8, 0xC640,
};

const u16 scene7_cat_001[] ALIGNED(4) = {
	2,
	0x00F0, 0x41F8, 0xC60E,
	0x40E8, 0x01F8, 0xC600,
};

const u16 scene7_cat_002[] ALIGNED(4) = {
	2,
	0x00F0, 0x41F8, 0xC610,
	0x40E8, 0x01F8, 0xC600,
};

const u16 scene7_glitter_000[] ALIGNED(4) = {
	1,
	0x00FC, 0x01FC, 0xE24D,
};

const u16 scene7_smoke_02D[] ALIGNED(4) = {	1,	0x00FF, 0x01FB, 0x43FF,};

const u16 scene7_smoke_054[] ALIGNED(4) = {	1,	0x00F8, 0x01FC, 0x44B4,};
