// GameReady -- File Select / Save Select screen
//
// State machine matching IDA GameReady at 0x8092284:
//   0: INIT  -- Setup BG, load tiles/palettes, check save integrity
//   1: FIN   -- Fade in from black
//   2: MAIN  -- Handle cursor input via ReadyKey()
//   3: INTERRUPTED -- Interrupted-save warning (skip for now)
//   4: WAIT_GAME  -- Brief delay, then fade out to start game
//   5: WAIT_TITLE -- Brief delay, then fade out to return to title
//   6: FOUT  -- Fade out to black
//   7: READY -- Load save data, return 1 to main loop
//   8: FIN2  -- Fade in (corrupted save path, used when ngStat != 0)
//   9: CORRUPTED -- "Data corrupted" message, wait for A/START
//  10: FOUT2 -- Fade out from corrupted screen back to INIT

#include "ready.h"
#include "../gba/gba.h"
#include "../gameutils.h"
#include "../00title/title.h"
#include "../save/save.h"

// External globals
extern u32 uObjSize;          // DMA size for OamBuf->OAM (set by EndObj in title.c)
extern IntrFunc sVblkFunc;    // VBlank function pointer (defined in gameutils.c)

// Forward declarations
static void ReadyVblkIntr0(void);

// ---- Ready data files (palettes, LZ77 tiles, tilemap screens) ----
extern const u16 selectdata_Palette[256];
extern const u16 sdobj1_Palette[256];
extern const u8  selectdata_CharLZ[];   // BG tiles (LZ77)
extern const u8  sdobj1_CharLZ[];       // OBJ tiles (LZ77)

// Tilemap screen data -- compressed with MapLH_kaitou (custom tilemap codec)
// These contain the full 32x32 tile screen maps for the file select UI.
// TODO: implement MapLH_kaitou decompression; for now fill with blank tiles.
extern const u8 Scr_selectdata[];   // BG2 tilemap (left side with save info)
extern const u8 Scr_selectdata2[];  // BG3 tilemap (right side, normal)
extern const u8 Scr_selectdata3[];  // BG3 tilemap (right side, perfect/hard)

// Corrupted save OBJ tiles
extern const u8 corrupted1_CharLZ[];
extern const u8 corrupted2_CharLZ[];
extern const u8 corrupted3_CharLZ[];

// ---- Save data globals (IWRAM) ----
// SaveReady[2] at 0x30031d0 -- declared as struct SaveReadyDef in gameutils.h

// OReady is 72 bytes at 0x3004a70 = OBJ animation state for UI elements
extern u8 OReady[72];

// ---- Internal state ----
// Cursor position tracking (replaces the old Ready struct)
enum { POS_NAME = 0, POS_LEVEL = 1, POS_DELETE = 2, POS_OK = 3 };

// ---- ReadyInit: Full hardware setup for the file select screen ----
// Returns 0 if save data is OK, non-zero if corrupted.
// Matching IDA ReadyInit at 0x809151c.
static int ReadyInit(void)
{
    // Disable display and DMA during setup
    REG_DISPCNT = DISPCNT_MODE_0;
    REG_DMA1CNT = 0;

    // Start fully darkened (fade in from black)
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

    // Wait for VBlank before GPU-visible DMA
    while (REG_VCOUNT >= 160);
    while (REG_VCOUNT < 160);

    // Load OBJ palette (sdobj1_Palette: 256 entries for cursor + UI sprites)
    REG_DMA3SAD = (u32)sdobj1_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (256 * 2 >> 2);

    // Decompress BG tiles (selectdata_CharLZ -> VRAM charbase 0)
    LZ77UnCompVram((const u32 *)selectdata_CharLZ, (void *)BG_VRAM);

    // Decompress OBJ tiles (sdobj1_CharLZ -> OBJ_VRAM0)
    LZ77UnCompVram((const u32 *)sdobj1_CharLZ, (void *)OBJ_VRAM0);

    // Clear font tile area (tiles 32-63 in charblock 0: VRAM + 0x400)
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x400);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (1024 >> 2);
    }

    // ---- Setup BG tilemaps using MapLH_kaitou decompression ----
    // IMPORTANT: MapLH_kaitou uses 8-bit writes, which GBA VRAM DROPS.
    // The original game decompresses to an EWRAM buffer, then DMAs to VRAM.
    // We use a static EWRAM buffer (0x800 bytes = one 256x256 screenblock).
    //
    // Matching IDA ReadyInit:
    //   BG0: screenbase 30 = VRAM + 0xF000 -- Scr_selectdata (save slot info, left page)
    //   BG1: screenbase 26 = VRAM + 0xD000 -- blank tile fill (tile 0x1FE = 510)
    //   BG2: screenbase 28 = VRAM + 0xE000 -- Scr_selectdata2/3 (details, right page)
    {
        static u8 ewr_buf[0x800] __attribute__((section(".ewram")));
        int size;
        vu16 *vram;

        // Decompress Scr_selectdata to EWRAM buffer, then DMA to BG0 screenbase 30
        size = MapLH_kaitou(0, Scr_selectdata, ewr_buf);
        REG_DMA3SAD = (u32)ewr_buf;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0xF000);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (size >> 2);

        // Decompress Scr_selectdata2/3 to EWRAM buffer, then DMA to BG2 screenbase 28
        if (ucPerfect)
            size = MapLH_kaitou(0, Scr_selectdata3, ewr_buf);
        else
            size = MapLH_kaitou(0, Scr_selectdata2, ewr_buf);
        REG_DMA3SAD = (u32)ewr_buf;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0xE000);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (size >> 2);

        // Fill BG1 screenbase 26 with blank tile 0x1FE (510)
        // IDA fills 1024 u16 entries starting at VRAM+0xD000
        vram = (vu16 *)((u8 *)BG_VRAM + 0xD000);
        {
            int i;
            for (i = 0; i < 1024; i++)
                vram[i] = 510;
        }
    }

    // Load BG palette
    REG_DMA3SAD = (u32)selectdata_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (256 * 2 >> 2);

    // ---- BG control registers (matching IDA: 0x7801, 0x3A06, 0x3C07) ----
    // BG0: priority 0, screenbase 30 -- left page (save slot info)
    REG_BG0CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(30) | BGCNT_CHARBASE(0);
    // BG1: priority 2, screenbase 26 -- blank fill (page border background)
    REG_BG1CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(26) | BGCNT_CHARBASE(0);
    // BG2: priority 3, screenbase 28 -- right page details
    REG_BG2CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(3)
               | BGCNT_SCREENBASE(28) | BGCNT_CHARBASE(0);

    // ---- Initialize globals ----
    ucWorldMapSubSeq = 0;
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
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (128 * 8 >> 2);
    }

    // Reset OBJ affine
    REG_BG2PA = 0x0100; REG_BG2PB = 0;
    REG_BG2PC = 0;       REG_BG2PD = 0x0100;

    ucResetStop = 0;

    // Default to save slot 0 (Save A)
    ucSaveNum = 0;

    // Window disabled (all layers visible everywhere)
    REG_WIN0H = 0; REG_WIN0V = 0;
    REG_WIN1H = 0; REG_WIN1V = 0;
    REG_WININ  = 0x3F;
    REG_WINOUT = 0x3F;

    // Enable display: Mode 0, OBJ 1D mapping, BG0+BG1+BG2 (matching IDA: 0x5700)
    REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP
                | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_BG2_ON;

    // ---- Set VBlank handler for Ready screen ----
    // Must replace the title screen's VBlank handler which writes BG2 affine
    // registers (corrupting BG2 text-mode display).
    SetVblkFunc(ReadyVblkIntr0);

    // Blend: darken off, full visibility once fades complete
    REG_BLDCNT = BLDCNT_EFFECT_DARKEN | 0x3F;

    return 0;  // 0 = no corruption detected
}

// ---- Fade helpers ----
// Fade IN: decreases BLDY from 16 to 0 (dark -> visible)
// Returns 1 when fully visible.
static int ReadyFadeIn(void)
{
    if ((usFadeTimer & 3) == 3 && usBgEvy > 0)
    {
        usBgEvy--;
        REG_BLDY = usBgEvy;
    }
    return (usBgEvy == 0);
}

// Fade OUT: increases BLDY from 0 to 16 (visible -> dark)
// Returns 1 when fully dark.
static int ReadyFadeOut(void)
{
    if ((usFadeTimer & 3) == 3 && usBgEvy < 16)
    {
        usBgEvy++;
        REG_BLDY = usBgEvy;
    }
    return (usBgEvy == 16);
}

// ---- Ready sub-mode state ----
// Matches IDA Ready struct at 0x3004a68 (SelM, SelS, SelP, Delete, Start, Set).
// sCursorSub values:  POS_NAME=0, POS_LEVEL=1, POS_DELETE=2
// sCursorSlot values: 0=Save A, 1=Save B
// sDeleteConfirm: 0=not in delete mode, 1=delete Yes/No active
// sDeleteChoice:  0=No, 1=Yes (in delete confirmation)
static u8 sCursorSlot;       // Save slot: 0=A, 1=B
static u8 sCursorSub;        // Sub-option: 0=NAME, 1=LEVEL, 2=DELETE
static u8 sDeleteConfirm;    // 0=normal mode, 1=delete confirmation active
static u8 sDeleteChoice;     // 0=No (cancel), 1=Yes (confirm delete)

// ---- ReadyKey: Handle key input during MAIN state ----
// Matches IDA ReadyKey at 0x809253c.
// In normal mode (SelM <= 1): A/START returns 1 (start game), B returns 2 (back).
// In delete sub-mode (SelM == 2): handles Yes/No navigation and confirmation.
// Returns cNextFlg value or 0 if no action taken.
static int ReadyKey(void)
{
    // ---- Delete confirmation mode (sCursorSub == POS_DELETE, sDeleteConfirm == 1) ----
    if (sDeleteConfirm)
    {
        // Left/Right: switch between No and Yes
        if ((usTrg & DPAD_LEFT) && sDeleteChoice > 0)
            sDeleteChoice = 0;
        if ((usTrg & DPAD_RIGHT) && sDeleteChoice < 1)
            sDeleteChoice = 1;

        // A button on "Yes": confirm delete -> erase this save slot
        if (usTrg & A_BUTTON)
        {
            if (sDeleteChoice == 1)
            {
                // Erase the selected save slot from SRAM
                Save_ClearSlot(ucSaveNum);

                // Clear in-memory save data for this slot
                {
                    SaveReady[ucSaveNum].Flg = 0;
                    SaveReady[ucSaveNum].DisCon = 0;
                    SaveReady[ucSaveNum].World = 0;
                    SaveReady[ucSaveNum].Stage = 0;
                }

                // Reset delete state and return to normal mode
                sDeleteConfirm = 0;
                sCursorSub = POS_NAME;
                return 0;  // Stay on ready screen, slot is now empty
            }
            else
            {
                // "No" selected: cancel delete, return to normal mode
                sDeleteConfirm = 0;
                return 0;
            }
        }

        // B button: cancel delete, return to normal mode
        if (usTrg & B_BUTTON)
        {
            sDeleteConfirm = 0;
            return 0;
        }

        return 0;
    }

    // ---- Normal mode: A/START or B triggers transition ----
    if (usTrg & (A_BUTTON | START_BUTTON))
    {
        // If DELETE sub-option is selected, enter delete confirmation
        if (sCursorSub == POS_DELETE)
        {
            sDeleteConfirm = 1;
            sDeleteChoice = 0;  // Default to "No"
            return 0;           // Stay on ready screen
        }

        // NAME or LEVEL selected: start game with current save slot
        cNextFlg = 1;
        return 1;
    }

    if (usTrg & B_BUTTON)
    {
        // If in any sub-option besides NAME, go back to NAME
        if (sCursorSub > POS_NAME)
        {
            sCursorSub = POS_NAME;
            return 0;
        }

        cNextFlg = 2;  // Return to title
        return 2;
    }

    return 0;  // No action
}

// ---- ReadyCursorMove: Cursor navigation ----
// Handles Left/Right (slot selection) and Up/Down (sub-option selection).
// Sub-options: NAME (0), LEVEL (1), DELETE (2).
// In delete confirmation mode, navigation is handled by ReadyKey.
static void ReadyCursorMove(void)
{
    if (sDeleteConfirm)
        return;  // ReadyKey handles delete confirmation navigation

    // Left/right: switch between save slots (Save A / Save B)
    if ((usTrg & DPAD_LEFT) && sCursorSlot > 0)
        sCursorSlot--;
    if ((usTrg & DPAD_RIGHT) && sCursorSlot < 1)
        sCursorSlot++;

    // Up/down: move between sub-options (NAME -> LEVEL -> DELETE)
    if ((usTrg & DPAD_UP) && sCursorSub > 0)
        sCursorSub--;
    if ((usTrg & DPAD_DOWN) && sCursorSub < 2)  // Now 3 options
        sCursorSub++;

    ucSaveNum = sCursorSlot;
}

// ---- Ready VBlank handler ----
// Matching IDA SelectVblkIntr01 pattern.
// ALWAYS DMA-copies all 128 OBJs from OamBuf to hardware OAM.
// Then writes BG scroll registers.
static void ReadyVblkIntr0(void)
{
    // SoundVSync_rev01();  // TODO: add sound when audio system is ready

    // OAM DMA: always copy all 128 OBJs (128 x 8 = 1024 bytes = 256 words)
    REG_DMA3SAD = (u32)OamBuf;
    REG_DMA3DAD = (u32)OAM;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (128 * 8 >> 2);

    // BG scroll register update
    REG_BG2HOFS = usBg2Hofs;
    REG_BG2VOFS = usBg2Vofs;
    REG_BG1HOFS = usBg1Hofs;
    REG_BG1VOFS = usBg1Vofs;
    REG_BG0HOFS = usBg0Hofs;
    REG_BG0VOFS = usBg0Vofs;
}

// ---- OBJ rendering ----
// Sets up OAM entries in OamBuf for the cursor arrow and other UI sprites.
// The VBlank handler DMAs OamBuf to hardware OAM.
// Matching IDA ReadyObjCreate at 0x809301c.
//
// Cursor OBJ from IDA: tile 9 (frame 0) or tile 11 (frame 1),
// 16x16 pixels, 16-color palette bank 1, normal OBJ mode.
extern u16 *pObjEnd;  // End-of-OAM pointer, defined in title.c

static void ReadyObjCreate(void)
{
    u16 *dst = (u16 *)OamBuf;
    pObjEnd = (u16 *)OamBuf;  // Reset OAM end pointer for this screen

    // Cursor Y positions per sub-option: NAME, LEVEL, DELETE
    static const u16 curY[3] = { 56, 96, 120 };
    // Cursor X positions per save slot: Save A, Save B
    static const u16 curX[2] = { 72, 136 };

    int cx = curX[sCursorSlot];
    int cy = curY[sCursorSub];

    // Main cursor: 16x16 OBJ, tile 9, palette bank 1, 16-color mode
    dst[0] = (cy & 0xFF) | 0x0000;            // attr0: Y, normal, 16-color, square
    dst[1] = (cx & 0x1FF) | 0x4000;           // attr1: X, size=16x16 (bit14-15=01)
    dst[2] = 0x0009 | (1 << 12);              // attr2: tile 9, priority 0, palette bank 1
    dst[3] = 0;                                // affineParam: unused
    dst += 4;

    // Delete confirmation prompt (shown when sDeleteConfirm == 1)
    if (sDeleteConfirm)
    {
        // "Yes/No" confirmation cursor at fixed position near the prompt
        // Position depends on sDeleteChoice (0=No at left, 1=Yes at right)
        int confirmX = sDeleteChoice ? 136 : 88;
        int confirmY = 120;

        dst[0] = (confirmY & 0xFF) | 0x0000;  // attr0
        dst[1] = (confirmX & 0x1FF) | 0x4000; // attr1
        dst[2] = 0x000B | (1 << 12);          // attr2: tile 11, palette bank 1
        dst[3] = 0;
        dst += 4;
    }

    // Fill remaining OAM with offscreen entries
    {
        u16 *end = (u16 *)OamBuf + 128 * 4;   // 128 OBJs x 4 u16
        while (dst < end)
        {
            dst[0] = 0x00D0;                   // attr0: Y=208 (offscreen hide)
            dst[1] = 0;
            dst[2] = 0;
            dst[3] = 0;
            dst += 4;
        }
    }
}

// ================================================================
//  GameReady -- Main entry point (called each frame from AgbMain)
// ================================================================
int GameReady(void)
{
    int ngStat;

    usFadeTimer++;

    switch (sGameSeq)
    {
    case 0:
        // ---- INIT: Setup screen, check save integrity ----
        ngStat = ReadyInit();
        sGameSeq = ngStat ? 8 : 1;  // corrupted -> FIN2, normal -> FIN
        break;

    case 1:
    case 8:
        // ---- FIN / FIN2: Fade IN from black ----
        FadeColor_DMA();
        if (ReadyFadeIn())
        {
            // Blend setup after fade (normal display)
            REG_BLDCNT = BLDCNT_TGT1_BG0 | BLDCNT_TGT2_BG1 | BLDCNT_TGT2_BG2
                       | BLDCNT_EFFECT_BLEND;
            usBgEva = 8;
            usBgEvb = 8;
            REG_BLDALPHA = (usBgEvb << 8) | usBgEva;

            sGameSeq = (sGameSeq == 8) ? 9 : 2;  // FIN2->CORRUPTED, FIN->MAIN
        }
        break;

    case 2:
        // ---- MAIN: Handle input and cursor ----
        // Matches IDA: ReadyKey returns cNextFlg (1=start, 2=back, 0=none).
        // If key pressed: check interrupted save, then transition.
        // If no key: update cursor via ReadyMain_Save_Select (ReadyCursorMove).
        if (ReadyKey())
        {
            // Clear high bit of save slot flags (mark as "not new" save)
            SaveReady[0].Flg &= ~0x80;
            SaveReady[1].Flg &= ~0x80;
            usFadeTimer = 0;

            if (cNextFlg == 2)
            {
                // B button: return to title
                // m4aMPlayFadeOut(&m4a_mplay000, 2);  // TODO: sound
                sGameSeq = 5;  // WAIT_TITLE
            }
            // Check for interrupted save (DisCon & 0x80): show warning
            else if ((SaveReady[ucSaveNum].DisCon & 0x80) != 0)
            {
                // Interrupted save detected -- animate warning OBJ
                // ReadyObj_Interrupted_Set(cNextFlg);  // TODO: implement
                // m4aSongNumStart(287);            // TODO: sound
                sGameSeq = 3;  // INTERRUPTED
            }
            else
            {
                sGameSeq = 4;  // WAIT_GAME
            }
        }
        else
        {
            ReadyCursorMove();
        }
        break;

    case 3:
        // ---- INTERRUPTED: Interrupted save warning animation ----
        // Plays an OBJ animation showing "save was interrupted."
        // OReady[ucSaveNum + 2].uckind tracks the animation state.
        // Advances to state 4 when animation completes (uckind reaches 0).
        {
            int idx = ucSaveNum + 2;
            if (OReady[idx * 18] == 0)  // uckind == 0 means animation done
            {
                usFadeTimer = 0;
                sGameSeq++;
            }
        }
        break;

    case 4:
        // ---- WAIT_GAME: Brief delay, then fade out to start game ----
        if (usFadeTimer > 20)
        {
            // if (SaveReady[ucSaveNum].Flg) m4aSongNumStop(636);  // TODO: sound
            sGameSeq = 6;  // FOUT
        }
        break;

    case 5:
        // ---- WAIT_TITLE: Brief delay, then fade out to return to title ----
        if (usFadeTimer > 40)
            sGameSeq = 6;  // FOUT
        break;

    case 6:
        // ---- FOUT: Fade OUT to black ----
        FadeColor_DMA();
        REG_BLDCNT = BLDCNT_EFFECT_DARKEN | 0x3F;
        if (ReadyFadeOut())
            sGameSeq = 7;  // READY
        break;

    case 7:
        // ---- READY: Load save data from SRAM, return to main loop ----
        // Matching IDA: ReadyMain_SaveLoad() loads SRAM data into RAM.
        if (ucSRAM)
        {
            // SRAM is valid: load save data into the .save_data region
            Save_ReadAuto(ucSaveNum);
        }
        // NOTE: Do NOT set ucSaveFlg=1 here for new games.
        // The main loop checks !ucSaveFlg to route to MS_TITLE with sGameSeq=-2,
        // which plays the pyramid intro scenes before entering the DMAP.
        // ucSaveFlg is set later (in DMAP init or pyramid intro end).
        //
        // cNextFlg was set by ReadyKey in case 2 (1 = game, 2 = title)
        // The main loop checks cNextFlg to decide the transition.
        return 1;  // Signal main loop: GameReady is done

    case 9:
        // ---- CORRUPTED: "Data corrupted" message ----
        // Wait for A/START to acknowledge
        if (usTrg & (A_BUTTON | START_BUTTON))
        {
            usFadeTimer = 0;
            sGameSeq = 10;  // FOUT2
        }
        break;

    case 10:
        // ---- FOUT2: Fade out from corrupted screen ----
        FadeColor_DMA();
        REG_BLDCNT = BLDCNT_EFFECT_DARKEN | 0x3F;
        if (ReadyFadeOut())
            sGameSeq = 0;  // Back to INIT
        break;
    }

    // Per-frame: render cursor OBJ for OAM DMA in VBlank
    ucCntObj = 0;
    ReadyObjCreate();

    return 0;  // Still running
}
