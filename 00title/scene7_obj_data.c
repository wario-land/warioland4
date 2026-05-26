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

// Additional cat animation patterns for the full 329-frame sequence
// Each pattern shows the cat in a different pose within the tunnel/cave
const u16 scene7_cat_003[] ALIGNED(4) = { 1, 0x4090, 0x8080, 0x0406 };  // surprised
const u16 scene7_cat_004[] ALIGNED(4) = { 1, 0x4090, 0x8080, 0x0408 };  // look back
const u16 scene7_cat_005[] ALIGNED(4) = { 1, 0x4090, 0x8080, 0x040A };  // crouch
const u16 scene7_cat_006[] ALIGNED(4) = { 1, 0x4090, 0x8080, 0x040C };  // pounce prep
const u16 scene7_cat_007[] ALIGNED(4) = { 1, 0x4090, 0x8080, 0x040E };  // pounce up
const u16 scene7_cat_008[] ALIGNED(4) = { 1, 0x4090, 0x8080, 0x0410 };  // mid-air
const u16 scene7_cat_009[] ALIGNED(4) = { 1, 0x4090, 0x8080, 0x0412 };  // descend
const u16 scene7_cat_00A[] ALIGNED(4) = { 1, 0x4090, 0x8080, 0x0414 };  // land
const u16 scene7_cat_00B[] ALIGNED(4) = { 1, 0x4090, 0x8080, 0x0416 };  // sit

// Cat running right (Anime1, 24-frame cycle, 4 patterns)
// Extracted from IDA 0x82a6eb0-0x82a6f12
const u16 scene7_cat_00C[] ALIGNED(4) = {
    4,
    0x00E6, 0x51FC, 0xC612,
    0x80E6, 0x11F4, 0xC614,
    0x40F6, 0x11FC, 0xC652,
    0x00F6, 0x11F4, 0xC654,
};
const u16 scene7_cat_00D[] ALIGNED(4) = {
    4,
    0x00E8, 0x51FD, 0xC615,
    0x80E8, 0x11F5, 0xC617,
    0x40F8, 0x11FD, 0xC655,
    0x00F8, 0x11F5, 0xC657,
};
const u16 scene7_cat_00E[] ALIGNED(4) = {
    4,
    0x00E8, 0x51FC, 0xC618,
    0x80E8, 0x11F4, 0xC61A,
    0x40F8, 0x11FC, 0xC658,
    0x00F8, 0x11F4, 0xC65A,
};
const u16 scene7_cat_00F[] ALIGNED(4) = {
    2,
    0x00E8, 0x51FA, 0xC615,
    0x80E8, 0x11F2, 0xC617,
};

// Cat emerging from darkness (Anime2, 74-frame cycle, 5 patterns)
// Extracted from IDA 0x82a6f18-0x82a6f7e
const u16 scene7_cat_010[] ALIGNED(4) = {
    2,
    0x00E8, 0x51F8, 0xC600,
    0x40F8, 0x11F8, 0xC640,
};
const u16 scene7_cat_011[] ALIGNED(4) = {
    3,
    0x00E8, 0x51F8, 0xC605,
    0x40F8, 0x11F8, 0xC645,
    0x80E8, 0x11F0, 0xC607,
};
const u16 scene7_cat_012[] ALIGNED(4) = {
    4,
    0x00E8, 0x51FD, 0xC602,
    0x80E8, 0x11F5, 0xC604,
    0x40F8, 0x11FD, 0xC642,
    0x00F8, 0x11F5, 0xC644,
};
const u16 scene7_cat_013[] ALIGNED(4) = {
    4,
    0x00EA, 0x51FB, 0xC602,
    0x80EA, 0x11F3, 0xC604,
    0x40FA, 0x11FB, 0xC642,
    0x00FA, 0x11F3, 0xC644,
};
const u16 scene7_cat_014[] ALIGNED(4) = {
    4,
    0x00E6, 0x5000, 0xC608,
    0x80E6, 0x11F8, 0x0000,
};

// Smoke animation frames
const u16 scene7_smoke_055[] ALIGNED(4) = { 1, 0x40A0, 0x8080, 0x0502 };
const u16 scene7_smoke_056[] ALIGNED(4) = { 1, 0x40A0, 0x8080, 0x0504 };
const u16 scene7_smoke_057[] ALIGNED(4) = { 1, 0x40A0, 0x8080, 0x0506 };
