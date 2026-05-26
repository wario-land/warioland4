// Wario multi-part OBJ system — shared across title, select, ready, and in-game
//
// Wario's sprite is built from multiple OBJ tiles (head, body, arms, legs,
// hat, etc.) laid out by pattern tables. Wario_Pattern sets the
// reaction type and state, Wario_Move renders the composite sprite.
//
// Pattern tables are arrays of u16: [count, attr0, attr1, attr2, ...]
// consumed by OamBuf directly. Each OBJ entry takes 3 u16 for attr0-attr2.
// count=0 means no OBJ data (hidden).

#include "wario.h"
#include "../gba/gba.h"
#include "../gameutils.h"

// ---- Wario global (IWRAM at 0x30018a4, 60 bytes) ----
IWRAM_DATA WarioDef Wario;

// ---- External data: Wario OBJ palette (from ROM data in wario_data.c) ----
extern const u16 war_Palette[16];  // Wario OBJ palette (16 entries, row 0)

// ---- Wario OBJ render table (defined in wario_data.c) ----
extern WarioOamType WarioOam_React[];

// ---- Wario_Palette: Load Wario OBJ palette ----
// DMA-copies 16 palette entries (32 bytes) from war_Palette to OBJ_PLTT.
void Wario_Palette(void)
{
    REG_DMA3SAD = (u32)war_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (16 * 2 >> 2);
}

// ---- Wario_Pattern: Set up Wario animation state ----
// Sets the reaction type (kind), state (stat), and facing direction (muki).
// Resets animation pattern and timer so the OBJ render callback can start
// from the beginning of the animation cycle.
//
//   kind: reaction/action type (0=idle, 1=walk, 2=run, ...)
//   stat: sub-state within the reaction (e.g., walk phase 0, 1, 2)
//   muki: direction flag (0x10 = facing right, 0 = facing left)
// ---- Title_Wario_Pattern: Set Wario animation state for title scenes ----
// Matches IDA Title_Wario_Pattern at 0x800fd68.
// Clears the animation pattern (ucAnmPat at +0x04 low) and timer (ucAnmTimer
// at +0x05), which overlap the low 16 bits of pObjData. The render callback
// (WarioOam_React) sets pObjData afterward, overwriting these zeros.
void Title_Wario_Pattern(int kind, int stat, int muki)
{
    Wario.ucReact = kind;      // reaction type
    Wario.ucStat = stat;       // animation sub-state
    Wario.usMukiX = muki;      // facing direction (0x10 = right)
    // Clear low 16 bits of pObjData (ucAnmPat + ucAnmTimer overlap)
    *(u16 *)&Wario.pObjData = 0;
}

// ---- Wario_Pattern: Shared wrapper (same as Title_Wario_Pattern) ----
void Wario_Pattern(int kind, int stat, int muki)
{
    Title_Wario_Pattern(kind, stat, muki);
}

// ---- Wario_Move: Render Wario as multi-part OBJ ----
// 1. Saves and overrides usCont/usTrg with passed values (select/title
//    scenes don't use the real key state for Wario — they simulate it).
// 2. Calls WarioKey_React[Wario.ucReact] to process simulated input.
// 3. Restores original usCont/usTrg.
// 4. Calls WarioOam_React[Wario.ucReact] to set pObjData.
// 5. Renders each OBJ entry from pObjData at (x, y) with priority pri.
// 6. Increments Wario.ucAnmTimer.
//
// Returns pointer past the last OAM entry used (for EndObj chaining).
//
// Pattern table format:
//   [0]: u16 count (number of OBJ entries)
//   [1..N]: OBJ data = [attr0, attr1, attr2] repeated count times
//
// attr0 is adjusted by +y (screen Y offset)
// attr1 is adjusted by +x (screen X offset, masked to 9 bits)
// attr2 bits [10:11] get overridden with pri & 3 (OBJ priority)
u16 *Wario_Move(int x, int y, u16 pri, u16 cont, u16 trg)
{
    u16 savedCont, savedTrg;
    u16 *dst;
    u16 count, attr0, attr1, attr2;
    int muki;
    WarioOamType oamFunc;

    // Save real key state and override with passed values
    savedTrg = usTrg;
    savedCont = usCont;
    usTrg = trg;
    usCont = cont;

    // Process key input for current reaction (may update Wario state)
    // WarioKey_React is a function table indexed by ucReact
    (*WarioKey_React[Wario.ucReact])();

    // Restore real key state
    usTrg = savedTrg;
    usCont = savedCont;

    // Clear OBJ counter (ucCntObj used by OamBuf DMA system)
    ucCntObj = 0;

    // Select OBJ render function based on facing direction
    // If usMukiX has bit 4 set (facing right), use entry 0 of pair
    // Otherwise (facing left), use entry 1 of pair
    muki = ((Wario.usMukiX & 0x10) != 0) ? 0 : 1;
    oamFunc = WarioOam_React[Wario.ucReact];

    // Call render function (may call through to muki-specific function)
    // The function sets Wario.pObjData to the correct pattern table
    // based on ucReact, ucStat, usMukiX, ucAnmTimer
    (*oamFunc)();

    // Render OBJ entries from pattern table
    dst = (u16 *)OamBuf;
    if (Wario.pObjData != 0)
    {
        u16 *src = Wario.pObjData;
        count = *src++;  // number of OBJ entries

        while (count > 0)
        {
            attr0 = *src++;
            attr1 = *src++;
            attr2 = *src++;

            // Adjust Y position (attr0 low byte)
            dst[0] = (attr0 & 0xFF00) | ((attr0 + y) & 0xFF);

            // Adjust X position (attr1 low 9 bits)
            dst[1] = (attr1 & 0xFE00) | ((attr1 + x) & 0x1FF);

            // Tile + shape + size
            dst[2] = attr2;

            // Set OBJ priority in attr2 bits [10:11]
            dst[2] = (dst[2] & ~(3 << 10)) | ((pri & 3) << 10);

            dst[3] = 0;  // affineParam (unused for non-affine OBJs)
            dst += 4;    // 4 u16 per OBJ = hardware OAM stride
            count--;
        }

        // Use first entry's count for ucCntObj
        ucCntObj = (u8)(Wario.pObjData[0]);
    }

    // Increment animation timer
    Wario.ucAnmTimer++;

    return dst;
}
