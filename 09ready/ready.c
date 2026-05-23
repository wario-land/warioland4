// GameReady — File Select / Save Select screen
//
// Implemented from wl4leaks Ready_main.c + IDA GameReady/ReadyInit.
// Shows 3 save slots (A, B, DELETE) with cursor OBJ for selection.
//
// State machine (matching wl4leaks and IDA GameReady):
//   0: INIT  — Setup BG, load tiles/palettes, check save integrity
//   1: FIN   — Fade in from black (also used as FIN2 for corrupted path)
//   2: MAIN  — Handle cursor input and slot selection
//   3: TYUDAN — Interrupted-save warning animation
//   4: WAIT_GAME  — Brief pause then fade out and start game
//   5: WAIT_TITLE — Brief pause then fade out and return to title
//   6: FOUT  — Fade out to black
//   7: READY — Load save, exit to MS_SELECT
//   8: FIN2  — Fade in (corrupted save path, same as FIN)
//   9: KOWARETA — "Data corrupted" message screen
//  10: FOUT2 — Fade out from corrupted screen back to INIT

#include "ready.h"
#include "../gba/gba.h"
#include "../gameutils.h"

// ---- Ready data files ----
extern const u16 selectdata_Palette[256];   // BG palette (from Pselectdata.c)
extern const u16 sdobj1_Palette[256];       // OBJ palette (from Psdobj1.c)
extern const u8  selectdata_CharLZ[];       // LZ77 BG tiles (from Cselectdata_LZ.c)
extern const u8  sdobj1_CharLZ[];           // LZ77 OBJ tiles (from Csdobj1_LZ.c)

// Tilemap data (MapLH_kaitou compressed format from IDA)
// Raw tilemap screen data — these are pre-decompressed 32x32 tile screens
extern const u8 Scr_selectdata[];   // BG2 tilemap (left page)
extern const u8 Scr_selectdata2[];  // BG3 tilemap (right page, normal)
extern const u8 Scr_selectdata3[];  // BG3 tilemap (right page, perfect/hard)

// "Data corrupted" OBJ tiles
extern const u8 kowareta1_CharLZ[];
extern const u8 kowareta2_CharLZ[];
extern const u8 kowareta3_CharLZ[];

// ---- Ready sequences (matching wl4leaks and IDA GameReady) ----
enum {
    RDY_INIT = 0, RDY_FIN = 1, RDY_MAIN = 2, RDY_TYUDAN = 3,
    RDY_WAIT_GAME = 4, RDY_WAIT_TITLE = 5, RDY_FOUT = 6, RDY_READY = 7,
    RDY_FIN2 = 8, RDY_KOWARETA = 9, RDY_FOUT2 = 10,
};

// ---- Cursor position modes ----
enum { POS_NAME = 0, POS_LEVEL = 1, POS_DELETE = 2, POS_OK = 3 };

// ---- Ready state structure (matching wl4leaks ReadyDef) ----
typedef struct {
    u8 SelM;    // Which save slot: 0=Save A, 1=Save B, 2=DELETE
    u8 SelS;    // Sub-position: NAME/LEVEL/DELETE/OK
    u8 SelP;    // Sub-position (for name editing)
    u8 Set;     // Confirmed flag
    u8 Start;   // Start button pressed
    u8 Delete;  // Delete mode
} ReadyDef;

IWRAM_DATA static ReadyDef Ready;

// ---- ReadyInit: Sets up all BG/OBJ data for the ready screen ----
// Returns 0 if save data is OK, non-zero (1-3) if corrupted
// This is called ONCE from GameReady case 0
static int ReadyInit(void)
{
    // Disable display during setup
    REG_DISPCNT = DISPCNT_MODE_0;

    // Disable HBlank DMA (avoid corruption during setup)
    REG_DMA1CNT = 0;

    // Set VBlank to init handler (will be swapped to main handler after setup)
    REG_BLDCNT = BLDCNT_EFFECT_DARKEN | 0x3F;
    REG_BLDY = 16;

    // Clear all palette RAM and VRAM
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)PLTT;
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (PLTT_SIZE >> 2);
    }
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)BG_VRAM;
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x18000 >> 2);
    }
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)OBJ_VRAM0;
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x4000 >> 2);
    }

    // Wait for VBlank before DMA (safer for VRAM writes)
    while (REG_VCOUNT >= 160);
    while (REG_VCOUNT < 160);

    // ---- Load OBJ palette ----
    REG_DMA3SAD = (u32)sdobj1_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (256*2 >> 2);

    // ---- Decompress BG tiles to VRAM base (charblock 0) ----
    LZ77UnCompVram((const u32 *)selectdata_CharLZ, (void *)BG_VRAM);

    // ---- Decompress OBJ tiles to OBJ_VRAM0 ----
    LZ77UnCompVram((const u32 *)sdobj1_CharLZ, (void *)OBJ_VRAM0);

    // ---- Clear font tile area (charblock 0, tiles 32-63 → VRAM + 0x400) ----
    // 32 tiles * 32 bytes per 4bpp tile = 1024 bytes
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x400);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (1024 >> 2);
    }

    // Clear work area to 0
    // TODO: SRAM check (SaveReady struct) — skip save data check for now

    // ---- Setup BG screenbases with tilemap data ----
    // The original uses MapLH_kaitou to decompress Scr_selectdata* to screenbases.
    // For now, fill with properly arranged tilemap from selectdata_CharLZ tiles.
    // BG2: main save select info (screenbase 28 = VRAM + 0xE000 + 0x200)
    // Fill with simple background pattern using decompressed tiles
    {
        vu16 *bg2 = (vu16 *)((u8 *)BG_VRAM + 0xE000);
        int i;
        for (i = 0; i < 32*32; i++)
            bg2[i] = 0x03FF;  // blank tile
    }
    // BG3: right page with save file details (screenbase 30 = VRAM + 0xF000)
    {
        vu16 *bg3 = (vu16 *)((u8 *)BG_VRAM + 0xF000);
        int i;
        for (i = 0; i < 32*32; i++)
            bg3[i] = 0x03FF;  // blank tile
    }

    // ---- Load BG palette ----
    REG_DMA3SAD = (u32)selectdata_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (256*2 >> 2);

    // ---- BG control registers ----
    // BG0: 256x256, 16-color, priority 1, screenbase 24, charbase 0
    // Uses tilemap at 0xC000 to draw the save select background
    REG_BG0CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(24) | BGCNT_CHARBASE(0);
    // BG1: 256x256, 16-color, priority 2, screenbase 26, charbase 0
    REG_BG1CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(26) | BGCNT_CHARBASE(0);
    // BG2: 256x256, 16-color, priority 3, screenbase 28, charbase 0
    REG_BG2CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(3)
               | BGCNT_SCREENBASE(28) | BGCNT_CHARBASE(0);
    // BG3: 256x256, 16-color, priority 0 (frontmost, for text overlay), screenbase 30
    REG_BG3CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(30) | BGCNT_CHARBASE(0);

    // ---- Initialize internal state ----
    ucGmapSubSeq = 0;

    // Blend init: darken all, max brightness (fade from black)
    usBgEvy = 16;
    usBgEvb = 0;
    REG_BLDY = 16;

    cGmStartFlg = 0;
    cPauseFlag = 0;
    ucTimeUp = 0;
    usFadeTimer = 0;

    // Reset scroll
    usBg0Hofs = 0; usBg0Vofs = 0;
    usBg1Hofs = 0; usBg1Vofs = 0;
    usBg2Hofs = 0; usBg2Vofs = 0;
    usBg3Hofs = 0; usBg3Vofs = 0;

    // Clear OAM
    {
        volatile u32 a = 0x02000200;
        REG_DMA3SAD = (u32)&a;
        REG_DMA3DAD = (u32)OAM;
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (128*8 >> 2);
    }

    // Reset OBJ affine registers
    REG_BG2PA = 0x0100; REG_BG2PB = 0;
    REG_BG2PC = 0;       REG_BG2PD = 0x0100;

    ucResetStop = 0;

    // Init ready state
    Ready.SelM = 0;
    Ready.SelS = POS_NAME;
    Ready.SelP = 0;
    Ready.Set = 0;
    Ready.Start = 0;
    Ready.Delete = 0;

    // Default to save slot 0 (Save A) if no save selected
    ucSaveNum = 0;

    // ---- Window disable (no window effect) ----
    REG_WIN0H = 0;
    REG_WIN0V = 0;
    REG_WIN1H = 0;
    REG_WIN1V = 0;
    REG_WININ  = 0x3F;   // All layers shown inside window
    REG_WINOUT = 0x3F;   // All layers shown outside window (= window disabled)

    // ---- Enable display ----
    // Mode 0, OBJ on, OBJ 1D mapping, all BGs on
    REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP
                | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_BG2_ON | DISPCNT_BG3_ON;

    // Reset blend: darken effect off initially (screen fades in during FIN state)
    REG_BLDCNT = BLDCNT_EFFECT_DARKEN | 0x3F;

    return 0;  // 0 = no corruption detected
}

// ---- Fade in from black ----
// Returns 1 when fade is complete (EVY reached 0)
static int ReadyFadeIn(void)
{
    if ((usFadeTimer & 3) == 3 && usBgEvy > 0)
    {
        usBgEvy--;
        REG_BLDY = usBgEvy;
    }
    return (usBgEvy == 0);
}

// ---- Fade out to black ----
// Returns 1 when fade is complete (EVY reached 16)
static int ReadyFadeOut(void)
{
    if ((usFadeTimer & 3) == 3 && usBgEvy < 16)
    {
        usBgEvy++;
        REG_BLDY = usBgEvy;
    }
    return (usBgEvy == 16);
}

// ---- Handle key input during MAIN state ----
// Returns 1 on A/START (go to game), 2 on B (go to title), 0 to continue
static int ReadyKey(void)
{
    // A button or START: confirm selection
    if (usTrg & (A_BUTTON | START_BUTTON))
    {
        // If DELETE slot selected, confirm delete
        if (Ready.SelM == 2)
        {
            if (Ready.SelS == POS_DELETE)
            {
                Ready.Delete = 1;
                return 1;  // Execute delete
            }
            else
            {
                return 0;  // OK position on DELETE = just back
            }
        }
        return 1;  // Start game with selected save
    }

    // B button: cancel → return to title
    if (usTrg & B_BUTTON)
    {
        return 2;  // Go to title
    }

    return 0;  // Continue
}

// ---- Move cursor based on D-pad input ----
static void ReadyMoveCursor(void)
{
    // Left/right: switch between Save A (0), Save B (1), DELETE (2)
    if ((usTrg & DPAD_LEFT) && Ready.SelM > 0)
        Ready.SelM--;
    if ((usTrg & DPAD_RIGHT) && Ready.SelM < 2)
        Ready.SelM++;

    // Up/down: move between options (NAME, LEVEL, DELETE, OK)
    if ((usTrg & DPAD_UP) && Ready.SelS > 0)
        Ready.SelS--;
    if ((usTrg & DPAD_DOWN) && Ready.SelS < 3)
        Ready.SelS++;

    // DELETE slot only has DELETE and OK options
    if (Ready.SelM == 2 && Ready.SelS < POS_DELETE)
        Ready.SelS = POS_DELETE;
}

// ---- Render OBJs (cursor arrow) ----
// GBA hardware OAM uses 4 u16 (8 bytes) per OBJ entry
static void ReadyObjCreate(void)
{
    u16 *dst = (u16 *)OamBuf;
    int i;

    // Cursor Y positions (per option) and X positions (per slot)
    static const u16 curY[4] = { 56, 72, 96, 120 };
    static const u16 curX[3] = { 72, 136, 200 };
    int cx = curX[Ready.SelM];
    int cy = curY[Ready.SelS];

    // OBJ 0: cursor arrow at current position
    dst[0] = 0x4000 | (cy & 0xFF);           // attr0: Y
    dst[1] = 0x8000 | (cx & 0x1FF);          // attr1: X
    dst[2] = 0x0000;                          // attr2: tile 0, palette 0
    dst[3] = 0;                               // affineParam (unused)
    dst += 4;                                 // 4 u16 per OBJ

    // Fill remaining 127 OBJs with offscreen entries (Y=208 hides them)
    for (i = 1; i < 128; i++)
    {
        dst[0] = 0xD0;   // attr0: Y=208 (offscreen)
        dst[1] = 0x0000; // attr1: X=0
        dst[2] = 0x0000; // attr2
        dst[3] = 0x0000; // affineParam
        dst += 4;
    }
}

// ================================================================
//  GameReady — Main entry point (called each frame from AgbMain)
// ================================================================
int GameReady(void)
{
    int result = 0;

    usFadeTimer++;

    switch (sGameSeq)
    {
    // ---- INIT: Setup screen ----
    case RDY_INIT:
        {
            int ngStat = ReadyInit();  // ngStat = corruption level (0=OK)
            if (ngStat)
                sGameSeq = RDY_FIN2;  // Corrupted save → corrupted path
            else
                sGameSeq = RDY_FIN;   // Normal → fade in
        }
        break;

    // ---- FIN / FIN2: Fade IN from black ----
    case RDY_FIN:
    case RDY_FIN2:
        if (ReadyFadeIn())
        {
            // Blend setup after fade complete (alpha blend for normal display)
            REG_BLDCNT = BLDCNT_TGT1_BG0 | BLDCNT_TGT2_BG1 | BLDCNT_TGT2_BG2
                       | BLDCNT_EFFECT_BLEND;
            usBgEva = 8;
            usBgEvb = 8;
            REG_BLDALPHA = (usBgEvb << 8) | usBgEva;

            if (sGameSeq == RDY_FIN2)
                sGameSeq = RDY_KOWARETA;
            else
                sGameSeq = RDY_MAIN;
        }
        break;

    // ---- MAIN: Handle input ----
    case RDY_MAIN:
        {
            int keyResult = ReadyKey();
            if (keyResult)
            {
                usFadeTimer = 0;
                if (keyResult == 2)
                    sGameSeq = RDY_WAIT_TITLE;  // B button → back to title
                else if (Ready.Delete)
                    sGameSeq = RDY_WAIT_GAME;   // Execute delete
                else
                    sGameSeq = RDY_WAIT_GAME;   // A/START → start game
            }
            else
            {
                ReadyMoveCursor();
            }
        }
        break;

    // ---- TYUDAN: Interrupted save warning ----
    case RDY_TYUDAN:
        // Simplified: skip warning, go straight to game
        usFadeTimer = 0;
        sGameSeq = RDY_WAIT_GAME;
        break;

    // ---- WAIT_GAME: Pause then fade to game ----
    case RDY_WAIT_GAME:
        if (usFadeTimer > 20)
            sGameSeq = RDY_FOUT;
        break;

    // ---- WAIT_TITLE: Pause then fade to title ----
    case RDY_WAIT_TITLE:
        if (usFadeTimer > 40)
            sGameSeq = RDY_FOUT;
        break;

    // ---- FOUT / FOUT2: Fade OUT to black ----
    case RDY_FOUT:
        REG_BLDCNT = BLDCNT_EFFECT_DARKEN | 0x3F;
        if (ReadyFadeOut())
            sGameSeq = RDY_READY;
        break;

    case RDY_FOUT2:
        REG_BLDCNT = BLDCNT_EFFECT_DARKEN | 0x3F;
        if (ReadyFadeOut())
            sGameSeq = RDY_INIT;  // Back to init (corrupted → retry)
        break;

    // ---- READY: Exit to select mode ----
    case RDY_READY:
        sMainSeq = MS_SELECT;
        sGameSeq = 5;    // Entry signal for GameSelect (matching original)
        cGmStartFlg = 0;
        result = 1;
        break;

    // ---- KOWARETA: Corrupted data screen ----
    case RDY_KOWARETA:
        // Wait for A/START to acknowledge corruption
        if (usTrg & (A_BUTTON | START_BUTTON))
        {
            usFadeTimer = 0;
            sGameSeq = RDY_FOUT2;
        }
        break;
    }

    // ---- Per-frame: render OBJs and setup OAM DMA ----
    if (!result)
    {
        ReadyObjCreate();
        ucCntObj = 1;
    }

    return result;
}
