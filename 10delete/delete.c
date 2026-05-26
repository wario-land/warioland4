// Delete Save Data screen -- "Delete All Save Data?"
//
// Matches IDA GameDelete at 0x80960e4 and wl4leaks Delete_main.c.
// Accessible via L+R held at boot (checked in AllReset / AgbMain).
//
// State machine:
//   0: INIT -- Setup screen, load tiles/palettes, init OBJ state
//   1: FIN  -- Fade in from black (decrease brightness)
//   2: MAIN -- Handle Yes/No cursor input via DeleteMain()
//   3: WAIT -- Short delay after confirmation
//   4: FOUT -- Fade out to black, return 1 to main loop
//
// Cursor navigation (ucDelete / ucCurSet):
//   PSEL_NO  = 0 -- "NO"  selected (initial)
//   PSEL_YES = 1 -- "YES" selected
//   DSEL_NO  = 2 -- "Really? NO"  selected (after pressing YES)
//   DSEL_YES = 3 -- "Really? YES" selected (final confirmation)
//
// OBJ layout (ODel[3] -- reuses OReady[0..2]):
//   OCUR = 0 -- cursor arrow sprite
//   OMES = 1 -- message text sprite ("Delete?" / "Really?")
//   OSEL = 2 -- selection indicator sprite ("NO" / "YES")

#include "delete.h"
#include "../gba/gba.h"
#include "../gameutils.h"
#include "../save/save.h"

// ====================================================================
//  Delete state machine states (sGameSeq within GameDelete)
// ====================================================================
enum { DEL_INIT, DEL_FIN, DEL_MAIN, DEL_WAIT, DEL_FOUT };

// ====================================================================
//  Cursor selection states -- which option the cursor is pointing at
// ====================================================================
enum DeleteSelect {
    PSEL_NO  = 0,  // Parent select: "NO"  -- cancel delete
    PSEL_YES = 1,  // Parent select: "YES" -- proceed to confirm
    DSEL_NO  = 2,  // Detail select: "NO"  -- go back to PSEL_NO
    DSEL_YES = 3,  // Detail select: "YES" -- final confirmation
};

// ====================================================================
//  OBJ animation kind enum -- indexes into DelerteAnm[] table
// ====================================================================
enum ObjAnmKind {
    ANON = 0,          // No animation (hidden OBJ)
    ACUR_SEL,          // Cursor -- selection mode (blinking)
    ACUR_SET,          // Cursor -- set/confirm animation (one-shot)
    ACUR_STP1,         // Cursor -- stopped state 1 (after cancel)
    ACUR_STP2,         // Cursor -- stopped state 2 (after transition)
    ASEL_NOj,          // "NO" text -- Japanese
    ASEL_YESj,         // "YES" text -- Japanese
    ASEL_NOe,          // "NO" text -- English
    ASEL_YESe,         // "YES" text -- English
    ADEL_NOj,          // "Really? NO" text -- Japanese
    ADEL_YESj,         // "Really? YES" text -- Japanese
    ADEL_NOe,          // "Really? NO" text -- English
    ADEL_YESe,         // "Really? YES" text -- English
    ANMNUM_MAX,
};

// ====================================================================
//  Language-indexed animation tables
//  Index: ucLanguage (0=English, 1=Japanese)
// ====================================================================
static const u8 Anm_SEL_NO[2]  = { ASEL_NOe,  ASEL_NOj  };
static const u8 Anm_SEL_YES[2] = { ASEL_YESe, ASEL_YESj };
static const u8 Anm_DEL_NO[2]  = { ADEL_NOe,  ADEL_NOj  };
static const u8 Anm_DEL_YES[2] = { ADEL_YESe, ADEL_YESj };

// ====================================================================
//  Global state variables (IWRAM)
// ====================================================================
IWRAM_DATA u8 ucDelete;   // Current selection state (PSEL_NO..DSEL_YES)
IWRAM_DATA u8 ucCurSet;   // Cursor set flag (bit 7 = needs update)

// ====================================================================
//  OBJ state (ODel = OReady[0..2] in original)
//  OReady is defined in gameutils.c at 0x3004a70, 72 bytes (4 slots x 18)
// ====================================================================
enum ObjNum { OCUR = 0, OMES = 1, OSEL = 2, OBJNUM_MAX };

// ODel[n] maps to OReady[n] (same memory)
// Each slot: sYpos(s16), sXpos(s16), iAnmAddr(u32), ucAnmTimer(u8), ucAnmPat(u8), uckind(u8) = 12 bytes
#define ODel(n) ((DeleteObjSlot *)((u8 *)OReady + (n) * 18))

typedef struct {
    s16 sYpos;
    s16 sXpos;
    u32 iAnmAddr;
    u8  ucAnmTimer;
    u8  ucAnmPat;
    u8  uckind;
    u8  pad[7];  // Padding to 18 bytes (matches OReady slot size)
} DeleteObjSlot;

// ====================================================================
//  External data references (from delete_data.c -- TODO: extract data)
// ====================================================================
// BG/OBJ palettes
extern const u16 alldelete_Palette[];
extern const u16 alldeleteobj_Palette[];

// LZ77 compressed tile data
extern const u8 alldelete_CharLZ[];
extern const u8 alldeleteobj_CharLZ[];

// Tilemap screens (MapLH_kaitou compressed)
extern const u8 Scr_alldeletee[];   // English
extern const u8 Scr_alldeletej[];   // Japanese

// OBJ animation definitions (AnmDef arrays)
extern const u32 alldeleteobj_Anm_00[];  // ANON (hidden)
extern const u32 alldeleteobj_Anm_01[];  // ACUR_SEL
extern const u32 alldeleteobj_Anm_02[];  // ACUR_SET
extern const u32 alldeleteobj_Anm_03[];  // ACUR_STP1
extern const u32 alldeleteobj_Anm_04[];  // ACUR_STP2
extern const u32 alldeleteobj_Anm_05[];  // ASEL_NOj
extern const u32 alldeleteobj_Anm_06[];  // ASEL_YESj
extern const u32 alldeleteobj_Anm_07[];  // ASEL_NOe
extern const u32 alldeleteobj_Anm_08[];  // ASEL_YESe
extern const u32 alldeleteobj_Anm_09[];  // ADEL_NOj
extern const u32 alldeleteobj_Anm_0a[];  // ADEL_YESj
extern const u32 alldeleteobj_Anm_0b[];  // ADEL_NOe
extern const u32 alldeleteobj_Anm_0c[];  // ADEL_YESe

// Animation table indexed by uckind -- maps animation kind to AnmDef array
// Each AnmDef: { uiObjAddr(u32), ucTimer(u8), pad(3 bytes) } = 8 bytes total
static const u32 *const DelerteAnm[ANMNUM_MAX] = {
    [ANON]       = alldeleteobj_Anm_00,
    [ACUR_SEL]   = alldeleteobj_Anm_01,
    [ACUR_SET]   = alldeleteobj_Anm_02,
    [ACUR_STP1]  = alldeleteobj_Anm_03,
    [ACUR_STP2]  = alldeleteobj_Anm_04,
    [ASEL_NOj]   = alldeleteobj_Anm_05,
    [ASEL_YESj]  = alldeleteobj_Anm_06,
    [ASEL_NOe]   = alldeleteobj_Anm_07,
    [ASEL_YESe]  = alldeleteobj_Anm_08,
    [ADEL_NOj]   = alldeleteobj_Anm_09,
    [ADEL_YESj]  = alldeleteobj_Anm_0a,
    [ADEL_NOe]   = alldeleteobj_Anm_0b,
    [ADEL_YESe]  = alldeleteobj_Anm_0c,
};

// ====================================================================
//  Cursor position table: (Y offset, X offset) relative to
//  the selection/message OBJ position for each ucDelete state.
//  Matches IDA ObjCur_PosiTbl[4][2].
// ====================================================================
static const s8 ObjCur_PosiTbl[4][2] = {
    {  2, -56 },   // PSEL_NO:  cursor near "NO" indicator
    {  2,  12 },   // PSEL_YES: cursor near "YES" indicator
    { 42, -56 },   // DSEL_NO:  cursor near "Really? NO"
    { 42,   4 },   // DSEL_YES: cursor near "Really? YES"
};

// ====================================================================
//  Forward declarations
// ====================================================================
static void DeleteInit(void);
static int  DeleteMain(void);
static void DeleteMain_SetSelect(void);
static int  DeleteMain_CurSelect(void);
static void Delete_ObjSet(void);
static void DeleteInit_ObjSet(void);
static void DeleteObjCreate(void);
static void DeleteVblkIntr0(void);
static void DeleteVblkInit(void);

// ====================================================================
//  GameDelete -- Main entry point (called each frame from AgbMain)
// ====================================================================
int GameDelete(void)
{
    int done = 0;

    usFadeTimer++;

    switch (sGameSeq)
    {
    case DEL_INIT:
        // Setup screen: load tiles, palettes, tilemap, init OBJ state
        DeleteInit();
        sGameSeq++;
        break;

    case DEL_FIN:
        // Fade in from black (decrease BLDY from 16 -> 0)
        if (usBgEvy)
        {
            usBgEvy--;
            REG_BLDY = usBgEvy;
        }
        else
        {
            // m4aSongNumStart(286);  // SE "data trouble" message -- TODO: sound
            sGameSeq++;
            usFadeTimer = 0;
        }
        break;

    case DEL_MAIN:
        // Handle Yes/No cursor input
        cNextFlg = DeleteMain();
        if (cNextFlg)
        {
            sGameSeq++;
            usFadeTimer = 0;
            if (cNextFlg != 1)
            {
                // Cancel: set BRIGHTNESS_DOWN for fade-out
                REG_BLDCNT = BLDCNT_EFFECT_DARKEN | 0x3F;
            }
        }
        break;

    case DEL_WAIT:
        // Short delay (40 frames) after confirmation before fade-out
        if (usFadeTimer > 40)
            sGameSeq++;
        break;

    case DEL_FOUT:
        // Fade out (increase BLDY from 0 -> 16)
        if (usBgEvy < 16)
        {
            usBgEvy++;
            REG_BLDY = usBgEvy;
        }
        else
        {
            done = 1;  // Screen complete -- return to main loop
        }
        break;

    default:
        break;
    }

    // Per-frame OBJ rendering
    ucCntObj = 0;
    DeleteObjCreate();
    ClearOamBuf();

    return done;
}

// ====================================================================
//  DeleteInit -- Full hardware setup for the delete confirmation screen
// ====================================================================
static void DeleteInit(void)
{
    int size;
    u8 *scrData;

    // Disable HBlank interrupt during setup
    REG_DISPSTAT &= ~DISPSTAT_HBLANK_INTR;
    REG_IE &= ~INTR_FLAG_HBLANK;

    SetVblkFunc(DeleteVblkInit);

    // Start fully darkened (fade in from black)
    REG_BLDCNT = BLDCNT_EFFECT_LIGHTEN | 0x3F;
    usBgEvy = 16;
    REG_BLDY = usBgEvy;

    ClearGraphicRam();
    ucCntObj = 0;
    ClearOamBuf();

    // Wait for VBlank before GPU-visible DMA
    while (REG_VCOUNT >= 160);
    while (REG_VCOUNT < 160);

    // Load BG palette -> BG_PLTT
    REG_DMA3SAD = (u32)alldelete_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_16BIT | DMA_SRC_INC | DMA_DEST_INC) << 16)
                | 256;  // 256 halfwords = full BG palette

    // Load OBJ palette -> OBJ_PLTT
    REG_DMA3SAD = (u32)alldeleteobj_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_16BIT | DMA_SRC_INC | DMA_DEST_INC) << 16)
                | 256;

    // Decompress BG tiles (alldelete_CharLZ -> BG_VRAM charbase 0)
    LZ77UnCompVram((const u32 *)alldelete_CharLZ, (void *)BG_VRAM);

    // Decompress OBJ tiles (alldeleteobj_CharLZ -> OBJ_VRAM0)
    LZ77UnCompVram((const u32 *)alldeleteobj_CharLZ, (void *)OBJ_VRAM0);

    // Decompress tilemap screen to EWRAM buffer, then DMA to BG1 screenbase 26
    // (VRAM + 0xD000 = screenbase 26, 256x256 text mode)
    if (ucLanguage)
        scrData = (u8 *)Scr_alldeletej;
    else
        scrData = (u8 *)Scr_alldeletee;

    {
        static u8 ewr_buf[0x800] __attribute__((section(".ewram")));
        size = MapLH_kaitou(0, scrData, ewr_buf);
        REG_DMA3SAD = (u32)ewr_buf;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0xD000);  // screenbase 26
        REG_DMA3CNT = ((DMA_ENABLE | DMA_16BIT | DMA_SRC_INC | DMA_DEST_INC) << 16)
                    | (size >> 1);
    }

    // BG control: only BG1 enabled (256x256, 16-color, priority 1, screenbase 26)
    REG_BG0CNT = 0;
    REG_BG1CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(26) | BGCNT_CHARBASE(0);
    REG_BG2CNT = 0;
    REG_BG3CNT = 0;

    // Blend: normal alpha
    usBgEvb = 0;
    REG_BLDALPHA = 0;

    cGmStartFlg = 0;

    // Initial BG1 scroll: horizontal offset of 4 pixels
    usBg1Hofs = 4;
    usBg1Vofs = 0;

    // Initialize OBJ state
    DeleteInit_ObjSet();
    ucCntObj = 0;
    DeleteObjCreate();

    // Enable display: Mode 0, BG1 + OBJ
    // OBJ 2D mapping bit = (1 << 6) = 0x0040
    REG_DISPCNT = DISPCNT_MODE_0 | 0x0040 | DISPCNT_OBJ_ON | DISPCNT_BG1_ON;

    SetVblkFunc(DeleteVblkIntr0);
}

// ====================================================================
//  DeleteMain -- Handle cursor input during MAIN state
//  Returns: 1=delete confirmed, 2=cancelled
// ====================================================================
static int DeleteMain(void)
{
    int result = 0;

    if (ucCurSet & 0x80)
    {
        // Cursor needs visual update (bit 7 set)
        DeleteMain_SetSelect();
    }
    else
    {
        // Process player input
        result = DeleteMain_CurSelect();
    }
    Delete_ObjSet();

    return result;
}

// ====================================================================
//  DeleteMain_SetSelect -- Update OBJ animation state when cursor moves
//  Called when ucCurSet has bit 7 set (cursor position changed).
//  Clears bit 7 after updating OBJ animation kind.
// ====================================================================
static void DeleteMain_SetSelect(void)
{
    DeleteObjSlot *pSel = ODel(OSEL);
    DeleteObjSlot *pMes = ODel(OMES);
    DeleteObjSlot *pCur = ODel(OCUR);

    // If cursor is already in SET animation, don't interrupt it
    if (pCur->uckind == ACUR_SET)
        return;

    ucCurSet &= 0x7F;          // Clear update flag
    ucDelete = ucCurSet;       // Sync current selection

    switch (ucCurSet)
    {
    case PSEL_NO:
        pSel->uckind = Anm_SEL_NO[ucLanguage];
        pMes->uckind = ANON;
        break;
    case PSEL_YES:
        pSel->uckind = Anm_SEL_YES[ucLanguage];
        pMes->uckind = ANON;
        break;
    case DSEL_NO:
        pMes->uckind = Anm_DEL_NO[ucLanguage];
        break;
    case DSEL_YES:
        pMes->uckind = Anm_DEL_YES[ucLanguage];
        break;
    default:
        break;
    }

    pMes->ucAnmTimer = 0;
    pMes->ucAnmPat = 0;
    pSel->ucAnmTimer = 0;
    pSel->ucAnmPat = 0;

    pCur->uckind = ACUR_SEL;
    pCur->ucAnmTimer = 0;
    pCur->ucAnmPat = 0;
}

// ====================================================================
//  DeleteMain_CurSelect -- Process player input for cursor navigation
//  Returns: 1=SRAM delete confirmed, 2=cancelled (back to title)
//
//  Navigation flow:
//    PSEL_NO  <--> PSEL_YES (L/R keys)
//    PSEL_YES   -> DSEL_NO  (A button: proceed to detail confirm)
//    DSEL_NO  <--> DSEL_YES (L/R keys)
//    DSEL_YES   -> DELETE   (A button: final confirmation)
//    Any state  -> CANCEL   (B button, or A on PSEL_NO)
// ====================================================================
static int DeleteMain_CurSelect(void)
{
    int result = 0;  // 1=delete, 2=cancel
    int action = 0;  // 1=confirm, 2=move, 3=cancel, 4=final confirm, 5=cancel-sound, 6=A-sound

    switch (ucDelete)
    {
    case PSEL_NO:
        if (usTrg & R_BUTTON)
        {
            ucCurSet = PSEL_YES | 0x80;   // Move cursor to YES
            action = 2;
        }
        else if (usTrg & A_BUTTON)
        {
            result = 2;                     // Cancel -- go back to title
            action = 3;
        }
        break;

    case PSEL_YES:
        if (usTrg & L_BUTTON)
        {
            ucCurSet = PSEL_NO | 0x80;    // Move cursor to NO
            action = 2;
        }
        else if (usTrg & A_BUTTON)
        {
            ucCurSet = DSEL_NO | 0x80;    // Proceed to detail confirmation
            action = 6;
        }
        break;

    case DSEL_NO:
        if (usTrg & R_BUTTON)
        {
            ucCurSet = DSEL_YES | 0x80;   // Move cursor to YES
            action = 2;
        }
        else if (usTrg & (A_BUTTON | B_BUTTON))
        {
            ucCurSet = PSEL_NO | 0x80;    // Go back to parent selection
            action = 5;
        }
        break;

    case DSEL_YES:
        if (usTrg & L_BUTTON)
        {
            ucCurSet = DSEL_NO | 0x80;    // Move cursor to NO
            action = 2;
        }
        else if (usTrg & A_BUTTON)
        {
            result = 1;                     // FINAL CONFIRM -- delete all SRAM
            action = 4;
            // Perform the actual SRAM erase
            CartridgeSram_AllClear();
        }
        else if (usTrg & B_BUTTON)
        {
            ucCurSet = PSEL_NO | 0x80;    // Go back to parent selection
            action = 5;
        }
        break;

    default:
        break;
    }

    // Apply cursor animation and sound based on action type
    switch (action)
    {
    case 1:  // Cursor confirm + A-sound-A
        ODel(OCUR)->uckind = ACUR_SET;
        ODel(OCUR)->ucAnmTimer = 0;
        ODel(OCUR)->ucAnmPat = 0;
        // m4aSongNumStart(S_BUTTON_A);  // TODO: sound
        break;
    case 2:  // Cursor move sound
        // m4aSongNumStart(S_CURSOL_1);   // TODO: sound
        break;
    case 3:  // Cancel + B-sound
        ODel(OCUR)->uckind = ACUR_STP1;
        ODel(OCUR)->ucAnmTimer = 0;
        ODel(OCUR)->ucAnmPat = 0;
        // m4aSongNumStart(S_BUTTON_B);   // TODO: sound
        break;
    case 4:  // Final confirm + A1-sound
        ODel(OCUR)->uckind = ACUR_SET;
        ODel(OCUR)->ucAnmTimer = 0;
        ODel(OCUR)->ucAnmPat = 0;
        // m4aSongNumStart(S_BUTTON_A1);  // TODO: sound
        break;
    case 5:  // Cancel-back sound
        // m4aSongNumStart(S_BUTTON_B);   // TODO: sound
        break;
    case 6:  // A-confirm sound
        // m4aSongNumStart(S_BUTTON_A);   // TODO: sound
        break;
    }

    return result;
}

// ====================================================================
//  Delete_ObjSet -- Position cursor OBJ relative to selection/message
//  Cursor Y/X = reference OBJ Y/X + ObjCur_PosiTbl[ucDelete]
// ====================================================================
static void Delete_ObjSet(void)
{
    DeleteObjSlot *pCur = ODel(OCUR);
    DeleteObjSlot *pRef;

    // Cursor follows either the SEL or MES OBJ based on state
    if (ucDelete < 2)
        pRef = ODel(OSEL);     // Parent select: cursor near SEL
    else
        pRef = ODel(OMES);     // Detail select: cursor near MES

    pCur->sYpos = pRef->sYpos + ObjCur_PosiTbl[ucDelete][0];
    pCur->sXpos = pRef->sXpos + ObjCur_PosiTbl[ucDelete][1];
}

// ====================================================================
//  DeleteInit_ObjSet -- Initialize OBJ animation state
// ====================================================================
static void DeleteInit_ObjSet(void)
{
    ucDelete = PSEL_NO;
    ucCurSet = PSEL_NO | 0x80;

    ODel(OSEL)->uckind = Anm_SEL_NO[ucLanguage];
    ODel(OSEL)->ucAnmTimer = 0;
    ODel(OSEL)->ucAnmPat = 0;
    ODel(OSEL)->sYpos = 102;     // 0x66
    ODel(OSEL)->sXpos = 120;     // 0x78

    ODel(OCUR)->uckind = ACUR_SEL;
    ODel(OCUR)->ucAnmTimer = 0;
    ODel(OCUR)->ucAnmPat = 0;

    ODel(OMES)->uckind = ANON;
    ODel(OMES)->ucAnmTimer = 0;
    ODel(OMES)->ucAnmPat = 0;
    ODel(OMES)->sYpos = 92;      // 0x5C
    ODel(OMES)->sXpos = 144;     // 0x90

    Delete_ObjSet();
}

// ====================================================================
//  VBlank handlers
// ====================================================================
static void DeleteVblkIntr0(void)
{
    // m4aSoundVSync();  // TODO: sound

    // OAM DMA: copy all 128 OBJs to hardware OAM
    REG_DMA3SAD = (u32)OamBuf;
    REG_DMA3DAD = (u32)OAM;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16)
                | (128 * 8 >> 2);

    // Update hardware registers
    REG_BLDY = usBgEvy;
    REG_BG1HOFS = usBg1Hofs;
    REG_BG1VOFS = usBg1Vofs;
}

static void DeleteVblkInit(void)
{
    // m4aSoundVSync();   // TODO: sound
    // m4aSoundMain();    // TODO: sound
}

// ====================================================================
//  DeleteObjCreate -- Render OBJ sprites into OamBuf
//
//  Iterates through ODel[OCUR..OSEL], advances animation timers,
//  and writes OAM entries to OamBuf starting at ucCntObj index.
//  Each OBJ entry takes 4 u16 (attr0, attr1, attr2, affineParam).
//
//  Animation format (AnmDef):
//    uiObjAddr -> pointer to OBJ pattern table [count, attr0, attr1, ...]
//    ucTimer   -> frame duration before advancing to next pattern
//
//  If ucTimer == 0: end of animation cycle -> loop or transition
// ====================================================================
typedef struct {
    u32 uiObjAddr;   // Offset 0: pointer to OBJ pattern table
    u8  ucTimer;     // Offset 4: frame duration (0=end marker)
    u8  pad2;        // Padding
    u16 pad3;        // Padding (struct is 8 bytes in original)
} AnmDef;

static void DeleteObjCreate(void)
{
    int  i, num;
    s16  x, y;
    AnmDef *anmd;
    u16  *oam;
    u16  *anm;
    u16  data;
    int  n;

    oam = (u16 *)OamBuf + ucCntObj * 4;  // Start at ucCntObj offset (4 u16 per OBJ)
    n = ucCntObj;

    for (num = 0; num < OBJNUM_MAX; num++)
    {
        DeleteObjSlot *slot = ODel(num);

        if (slot->uckind == ANON)
            continue;

        // Get animation definition table for this animation kind
        anmd = (AnmDef *)DelerteAnm[slot->uckind];
        if (!anmd)
            continue;

        switch (slot->uckind)
        {
        default:
            // ---- Looping animations ----
            if (slot->ucAnmTimer >= anmd[slot->ucAnmPat].ucTimer)
            {
                slot->ucAnmTimer = 0;
                slot->ucAnmPat++;
                if (!anmd[slot->ucAnmPat].ucTimer)
                    slot->ucAnmPat = 0;  // Loop to start
            }
            break;

        case ACUR_SET:
            // ---- One-shot animation (transitions to next kind) ----
            if (slot->ucAnmTimer >= anmd[slot->ucAnmPat].ucTimer)
            {
                slot->ucAnmTimer = 0;
                slot->ucAnmPat++;
                if (!anmd[slot->ucAnmPat].ucTimer)
                {
                    slot->uckind += 2;  // ACUR_SET->ACUR_STP2 transition
                    anmd++;
                    slot->ucAnmTimer = 0;
                    slot->ucAnmPat = 0;
                }
            }
            break;
        }

        slot->ucAnmTimer++;

        // Compute screen position (subtract BG scroll for parallax)
        y = slot->sYpos - (s16)usBg1Vofs;
        x = slot->sXpos - (s16)usBg1Hofs;

        // Get current OBJ pattern table from animation frame
        anmd = (AnmDef *)DelerteAnm[slot->uckind] + slot->ucAnmPat;
        anm  = (u16 *)anmd->uiObjAddr;
        if (!anm)
            continue;

        i = n;
        n += *anm++;  // OBJ count from pattern table header

        // Write OBJ entries: attr0, attr1, attr2, affineParam per OBJ
        for (; i < n; i++)
        {
            u16 a0, a1, a2;

            a0 = *anm++;
            a1 = *anm++;
            a2 = *anm++;

            oam[0] = (a0 & 0xFF00) | ((a0 + y) & 0xFF);    // attr0: Y adjusted
            oam[1] = (a1 & 0xFE00) | ((a1 + x) & 0x1FF);    // attr1: X adjusted
            oam[2] = a2;                                      // attr2: tile+palette
            oam[3] = 0;                                       // affineParam: unused
            oam += 4;  // 4 u16 per OBJ (hardware OAM stride)
        }
    }

    ucCntObj = n;
}
