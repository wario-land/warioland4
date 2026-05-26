// Wario OBJ system data: palette, key/OBJ function tables, pattern tables
//
// The original game has ~50+ Wario reaction types for in-game physics.
// Title/select scenes use a minimal subset — idle, walk, run, look, etc.
//
// Each reaction has:
//   - A key handler (WarioKey_React[reaction]) — processes simulated input
//   - An OBJ renderer (WarioOam_React[reaction]) — sets pObjData pattern table
//
// Pattern tables: arrays of [count, attr0, attr1, attr2] consumed by SetObj.
// The count field determines how many OBJ tiles make up Wario's body.

#include "../gba/gba.h"
#include "wario.h"

// ---- Wario OBJ palette (16 entries, extracted from ROM) ----
// Located at 0x82E16A4 in original ROM
const u16 war_Palette[16] = {
    0x0000, 0x1CE7, 0x2529, 0x2D6B, 0x35AD, 0x3DEF, 0x4210, 0x4A52,
    0x4E73, 0x5294, 0x56B5, 0x5AD6, 0x5EF7, 0x6739, 0x6B5A, 0x6F7B,
};

// ---- Wario key reaction stubs ----
// Title/select scenes don't need real Wario input handling — these are minimal
// callbacks used indirectly by Wario_Move.

// Stub 0: Do nothing (Wario doesn't respond to input in non-game scenes)
static void WarioKey_None(void)
{
}

// Stub 1: Idle input (doesn't change state)
static void WarioKey_Idle(void)
{
}

// Stub 2: Walk right (simulated walking)
static void WarioKey_Walk(void)
{
}

// Key handler table: indexed by Wario.ucReact (reaction type)
// Entries 0-15 cover the reaction types used in title/select scenes.
WarioKeyFunc WarioKey_React[16] = {
    WarioKey_None,   //  0: default/none
    WarioKey_Idle,   //  1: idle
    WarioKey_Walk,   //  2: walk
    WarioKey_None,   //  3
    WarioKey_None,   //  4
    WarioKey_None,   //  5: look up / surprised
    WarioKey_None,   //  6: falling
    WarioKey_None,   //  7
    WarioKey_None,   //  8
    WarioKey_None,   //  9: running
    WarioKey_None,   // 10
    WarioKey_None,   // 11
    WarioKey_None,   // 12
    WarioKey_None,   // 13
    WarioKey_None,   // 14
    WarioKey_None,   // 15
};

// ---- Wario OBJ render callbacks ----
// Each sets Wario.pObjData to a pattern table for the current state.
// Called by Wario_Move after key processing.

// ======================================================================
//  Wario OBJ pattern tables — title screen Wario animation
// ======================================================================
// Pattern format: [count:u16] [attr0, attr1, attr2] × count
// attr0 Y and attr1 X are RELATIVE offsets — Wario_Move adds (x,y).
// Tile numbers reference the scene-loaded Wario tiles in OBJ VRAM.
// Palette row 0 (Wario's own palette loaded by Wario_Palette).
//
// Wario idle (ucReact=0): standing pose, slight breathing animation
// Wario walk (ucReact=2): walking right with leg swing

// ---- Wario idle — frame 0 (standing) ----
// Multi-part OBJ: body (32x32), head (16x16), feet (16x8)
// Tile indices start at 0; actual tiles are at scene OBJ load offset
static const u16 WarioIdle_Frame0[] = {
    3,                      // 3 OBJ parts
    0x4000, 0x8000, 0x0000, // body: 32x32 at (0,0), tile 0
    0x4000, 0x8008, 0x0020, // head: 16x16 at (8,0), tile 32
    0x4018, 0x8000, 0x0040, // feet: 16x8 at (0,24), tile 64
};

// ---- Wario idle — frame 1 (breathing) ----
static const u16 WarioIdle_Frame1[] = {
    3,
    0x3FFF, 0x8000, 0x0000, // body: 32x32 at (0,-1), tile 0
    0x3FFF, 0x8008, 0x0020, // head: 16x16 at (8,-1), tile 32
    0x4018, 0x8000, 0x0040, // feet: 16x8 at (0,24), tile 64
};

// ---- Wario walk right — frame 0 (step) ----
static const u16 WarioWalk_Frame0[] = {
    3,
    0x4000, 0x8000, 0x0080, // body: 32x32, tile 128
    0x4000, 0x8008, 0x00A0, // head: 16x16, tile 160
    0x4018, 0x8002, 0x00C0, // feet: 16x8 at (2,24), tile 192
};

// ---- Wario walk right — frame 1 (step) ----
static const u16 WarioWalk_Frame1[] = {
    3,
    0x4001, 0x8000, 0x0080, // body: Y+1, tile 128
    0x4000, 0x8008, 0x00A0, // head, tile 160
    0x4018, 0x8000, 0x00C0, // feet: X-2, tile 192
};

// ---- Idle animation sequence ----
static const u16 *WarioIdleFrames[2] = {
    WarioIdle_Frame0, WarioIdle_Frame1,
};

// ---- Walk animation sequence ----
static const u16 *WarioWalkFrames[2] = {
    WarioWalk_Frame0, WarioWalk_Frame1,
};

// ---- Title Wario OBJ render callbacks ----
// Indexed by Wario.ucReact. Each sets pObjData to the correct pattern table
// for the current animation frame (ucAnmPat selects within the sequence).

static void WarioOam_Idle(void)
{
    // Cycle through 2 idle frames based on animation timer
    int frame = (Wario.ucAnmTimer >> 3) & 1;  // change every 8 frames
    Wario.pObjData = (u16 *)WarioIdleFrames[frame];
}

static void WarioOam_Walk(void)
{
    // Cycle through 2 walk frames
    int frame = (Wario.ucAnmTimer >> 3) & 1;  // change every 8 frames
    Wario.pObjData = (u16 *)WarioWalkFrames[frame];
}

// OBJ render table: indexed by Wario.ucReact (reaction type)
WarioOamType WarioOam_React[16] = {
    WarioOam_Idle,     //  0: idle (title default)
    WarioOam_Idle,     //  1
    WarioOam_Walk,     //  2: walk
    WarioOam_Idle,     //  3
    WarioOam_Idle,     //  4
    WarioOam_Idle,     //  5: surprised/look up
    WarioOam_Idle,     //  6: falling
    WarioOam_Idle,     //  7
    WarioOam_Idle,     //  8
    WarioOam_Idle,     //  9: running
    WarioOam_Idle,     // 10
    WarioOam_Idle,     // 11
    WarioOam_Idle,     // 12
    WarioOam_Idle,     // 13
    WarioOam_Idle,     // 14
    WarioOam_Idle,     // 15
};
