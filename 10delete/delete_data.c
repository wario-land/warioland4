// Placeholder data for the Delete Save screen.
// TODO: Extract actual tile, palette, and tilemap data from the original ROM.
//
// These stubs provide minimum data to compile and show a basic screen.
// The BG tilemap is a simple "DELETE ALL SAVE DATA?" message using the
// font tiles that are already present in VRAM from the ready screen.

#include "../gba/gba.h"

// BG palette: 256 entries (black background, white text colors)
const u16 alldelete_Palette[256] = {
    0x0000, 0x7FFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    // ... rest filled with zeros
};

// OBJ palette: 256 entries (cursor colors)
const u16 alldeleteobj_Palette[256] = {
    0x0000, 0x7FFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

// Minimal LZ77 compressed empty BG tile data (VT (type=0x10, size=0 → zero bytes))
const u8 alldelete_CharLZ[] = { 0x10, 0x00, 0x00, 0x00, 0x00 };

// Minimal LZ77 compressed empty OBJ tile data
const u8 alldeleteobj_CharLZ[] = { 0x10, 0x00, 0x00, 0x00, 0x00 };

// Placeholder tilemap: MapLH_kaitou compressed empty screen
// Format: ascType(0), then two passes of RLE with 0 termination
// ascType 0 = single screenblock (0x800 bytes)
// Pass 1 (opcode 1): 0x80 | 0x01, tile 0 → repeat tile 0 one time (effectively nothing)
// Pass 2 (opcode 2+): 16-bit count = 0 → end
const u8 Scr_alldeletee[] = {
    0x00,       // ascType: single screenblock
    0x01, 0x81, 0x00,  // Pass 1: repeat tile 0 once, then count=0 → end pass 1
    0x00, 0x00, // Pass 2: count = 0 → end
};

const u8 Scr_alldeletej[] = {
    0x00,       // ascType: single screenblock
    0x01, 0x81, 0x00,
    0x00, 0x00,
};

// OBJ animation table stubs (each entry: uiObjAddr, ucTimer)
// uiObjAddr = pointer to OBJ count/data table
// These are minimal patterns for compile-time linking.
// Actual animation data needs to be extracted from the ROM.

static const u16 anm_pattern_empty[] = { 0 };  // Count 0 = no OBJ entries

const u32 alldeleteobj_Anm_00[] = { (u32)anm_pattern_empty, 0 };  // ANON
const u32 alldeleteobj_Anm_01[] = { (u32)anm_pattern_empty, 0 };  // ACUR_SEL
const u32 alldeleteobj_Anm_02[] = { (u32)anm_pattern_empty, 0 };  // ACUR_SET
const u32 alldeleteobj_Anm_03[] = { (u32)anm_pattern_empty, 0 };  // ACUR_STP1
const u32 alldeleteobj_Anm_04[] = { (u32)anm_pattern_empty, 0 };  // ACUR_STP2
const u32 alldeleteobj_Anm_05[] = { (u32)anm_pattern_empty, 0 };  // ASEL_NOj
const u32 alldeleteobj_Anm_06[] = { (u32)anm_pattern_empty, 0 };  // ASEL_YESj
const u32 alldeleteobj_Anm_07[] = { (u32)anm_pattern_empty, 0 };  // ASEL_NOe
const u32 alldeleteobj_Anm_08[] = { (u32)anm_pattern_empty, 0 };  // ASEL_YESe
const u32 alldeleteobj_Anm_09[] = { (u32)anm_pattern_empty, 0 };  // ADEL_NOj
const u32 alldeleteobj_Anm_0a[] = { (u32)anm_pattern_empty, 0 };  // ADEL_YESj
const u32 alldeleteobj_Anm_0b[] = { (u32)anm_pattern_empty, 0 };  // ADEL_NOe
const u32 alldeleteobj_Anm_0c[] = { (u32)anm_pattern_empty, 0 };  // ADEL_YESe
