// Wario multi-part OBJ system — shared across title scenes, select screen,
// ready screen, and in-game gameplay.
//
// Wario_Pattern sets Wario's animation state (reaction type, status,
// and facing direction). Wario_Move renders the multi-part Wario sprite
// using pattern tables set by the animation subsystem.
//
// Pattern tables are arrays of [count, attr0, attr1, attr2] tuples consumed
// by SetObj(). The system supports rendering Wario as a composite of multiple
// OBJ tiles (body, arms, legs, hat, etc.).

#ifndef WARIO_H
#define WARIO_H

#include "../gba/gba.h"

// Wario animation state structure (60 bytes, matching IDA WarioDef @ 0x30018a4)
// The original struct uses overlapping fields — ucAnmPat/ucAnmTimer share
// the low bytes of pObjData. Wario_Pattern clears the timer+pattern,
// then WarioOam_React sets pObjData to the correct table before Move uses it.
typedef struct {
    u8  ucReact;       // +0x00: reaction/action type (walk, run, jump, etc.)
    u8  ucStat;        // +0x01: animation state (frame group within reaction)
    u16 usMukiX;       // +0x02: direction flag (0x10 = facing right, 0 = left)
    u16 *pObjData;     // +0x04: OBJ pattern table pointer (set by render callback)
    u8  ucAnmTimer;    // +0x05: animation timer (incremented each Move call)
    // Warning: ucAnmTimer is the upper byte of pObjData's low halfword.
    // Wario_Pattern clears both low bytes (pattern + timer) to zero,
    // so pObjData values must have 0x0000 in their low 16 bits — OR the
    // render callback sets pObjData after Pattern is called.
    //
    // VBlank tile upload: DMA Wario OBJ char data to VRAM during VBlank.
    // Used by Title_Wario_VblkSync (called during Scene7 VBlank).
    // Set by scene animation callbacks before the Wario sprite renders.
    u16 usChrByte1;    // +0x06: byte count for first OBJ char block DMA
    u16 usChrByte2;    // +0x08: byte count for second OBJ char block DMA
    const u8 *pChrAddr1; // +0x0A: ROM source address for first DMA
    const u8 *pChrAddr2; // +0x0E: ROM source address for second DMA

    // Remaining fields reserved for future game-specific state.
    u8  pad[46];       // +0x12 to +0x3B
} WarioDef;

// Wario global (in IWRAM at 0x30018a4)
extern WarioDef Wario;

// ---- Wario system API ----

// Set Wario's animation state. kind=reaction type, stat=sub-state within
// reaction, muki=direction (0x10 for right, 0 for left).
// Resets animation pattern and timer to zero.
void Wario_Pattern(int kind, int stat, int muki);

// Render Wario as multi-part OBJ at (x, y) with given priority.
// Temporarily overrides usCont/usTrg with the passed values so that
// the key-response subsystem can process input in title/select context.
// Returns pointer to the next free OAM slot.
u16 *Wario_Move(int x, int y, u16 pri, u16 cont, u16 trg);

// Load Wario's OBJ palette (war_Palette) via DMA to OBJ_PLTT.
void Wario_Palette(void);

// ---- Title-screen Wario API (separate from in-game system) ----
// These match IDA Title_Wario_Pattern (0x800fd68) and Title_Wario_Palette (0x800fee4).
// Title_Wario_Move wraps Wario_Move for title coordinate conventions.
void Title_Wario_Pattern(int kind, int stat, int muki);
#define Title_Wario_Palette Wario_Palette
#define Title_Wario_Move(x, y, pri, cont, trg) Wario_Move(x, y, pri, cont, trg)

// ---- Function table types ----
typedef void (*WarioKeyFunc)(void);
typedef void (*WarioOamType)(void);

// ---- Internal function tables (defined in wario_data.c) ----
// These are called indirectly by Wario_Move:
//   WarioKey_React[Wario.ucReact] — handles key input for current reaction
//   WarioOam_React[ucReact]       — sets pObjData for current reaction/muki
extern WarioKeyFunc WarioKey_React[];
extern WarioOamType WarioOam_React[];

#endif // WARIO_H
