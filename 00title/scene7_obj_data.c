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

// Smoke animation frames (4-stage particle: appear → grow → shrink → gone)
const u16 scene7_smoke_055[] ALIGNED(4) = { 1, 0x40A0, 0x8080, 0x0502 };  // smoke grow
const u16 scene7_smoke_056[] ALIGNED(4) = { 1, 0x40A0, 0x8080, 0x0504 };  // smoke peak
const u16 scene7_smoke_057[] ALIGNED(4) = { 1, 0x40A0, 0x8080, 0x0506 };  // smoke shrink
