#include "gba/gba.h"
#include "gameutils.h"

// from g_select.h
IWRAM_DATA s16 sSelSeq;
SAVE_DATA  u8  ucWorldNum;          // [SAVE] current world
SAVE_DATA  u8  ucStageNum;          // [SAVE] current stage
IWRAM_DATA s8 cSelectedNo;

IWRAM_DATA u16 usRandomCount;

SAVE_DATA  u16 usTotalGoldMedal;     // [SAVE] total gold medals collected

SAVE_DATA  u16 usMiniBestScore[3];   // [SAVE] mini game best scores

IWRAM_DATA u8 ucWorldNumBak;

IWRAM_DATA u8 ucEndingMessageKind;

// Jewel piece collection flags [SAVE]
SAVE_DATA u8 ucJewel1GetFlg;
SAVE_DATA u8 ucJewel2GetFlg;
SAVE_DATA u8 ucJewel3GetFlg;
SAVE_DATA u8 ucJewel4GetFlg;
SAVE_DATA u8 ucJewel5GetFlg;
SAVE_DATA u8 ucJewel6GetFlg;
SAVE_DATA u8 ucJewel7GetFlg;
SAVE_DATA u8 ucJewel8GetFlg;
SAVE_DATA u8 ucJewel9GetFlg;
SAVE_DATA u8 ucJewel10GetFlg;
SAVE_DATA u8 ucJewel11GetFlg;
SAVE_DATA u8 ucJewel12GetFlg;


// from g_map.h
SAVE_DATA  u8  ucDisConFlg;         // [SAVE] discontinue (interrupted save) flag
SAVE_DATA  u8  ucSaveFlg;           // [SAVE] save type: 0=none, 1=auto, 2=interrupt
SAVE_DATA  u8  ucSRAM;             // [SAVE] SRAM status: 1=ok, 0=error
SAVE_DATA  u8  ucPerfect;          // [SAVE] perfect ending flag

SAVE_DATA  u8  ucLanguage;          // [SAVE] language: 0=English, 1=Japanese
SAVE_DATA  u8  ucLevel;            // [SAVE] difficulty level
IWRAM_DATA u8  ucAllReset;

SAVE_DATA  u8  ucSaveNum;           // [SAVE] selected save slot number

IWRAM_DATA u8 ucBossPause;
IWRAM_DATA u8 ucBossRoomProgress;

SAVE_DATA  u8  ucBgmOff;            // [SAVE] BGM off flag
IWRAM_DATA u8 ucGndSaveFlg;

IWRAM_DATA u8 ucResetStop;

IWRAM_DATA u8 ucAutoDemoSW;
IWRAM_DATA u8 ucAutoDemoNum;

IWRAM_DATA u8 ucWorldMapSubSeq;
IWRAM_DATA u8 ucTitle2f;

SAVE_DATA  u8  ucCurrentLevelDoorDataTableID;  // [SAVE] current door data table
IWRAM_DATA u8  ucRoomID;

SAVE_DATA  u8  ucGateID;            // [SAVE] gate ID for discontinue save
SAVE_DATA  u8  ucGateType;          // [SAVE] gate type

SAVE_DATA  u8  ucEnemyGroup;         // [SAVE] enemy group for discontinue
IWRAM_DATA u8 ucEnemyBreakFlag;

IWRAM_DATA u16 usAlpfaBLD;
IWRAM_DATA u16 usBldCnt;

SAVE_DATA  u8  MapSw[5];           // [SAVE] map switches

IWRAM_DATA u8 ucObjPriFlg;
IWRAM_DATA u8 ucKeyPri;

IWRAM_DATA u8 ucBgCountDown;
IWRAM_DATA u8 ucBgCDcnt;

IWRAM_DATA u8 ucHblankFlag;

IWRAM_DATA u8  ucAlphaSw;

SAVE_DATA  u16 usEnemyBgPosX;       // [SAVE] enemy BG X position
SAVE_DATA  u16 usEnemyBgPosY;       // [SAVE] enemy BG Y position

SAVE_DATA  u16 usDisConR;          // [SAVE] discontinue counter
SAVE_DATA  u32 DisConSum;          // [SAVE] discontinue checksum

IWRAM_DATA u8 ucFlashLoop;

IWRAM_DATA u8 ucBgExpected;

IWRAM_DATA u8 ucBgKakiStop;

IWRAM_DATA u8 ucTimeUp;
IWRAM_DATA u8 ucSTEndType;

IWRAM_DATA struct BgDebugDef BgDebug;

IWRAM_DATA struct MapDef Map[4];

IWRAM_DATA struct RoomHeaderDef RoomHeader;

IWRAM_DATA struct PyakuwariDef Pyaku;

SAVE_DATA  struct BgScDef Scroll[4];  // [SAVE] BG scroll state for discontinue

IWRAM_DATA struct BgWinDef BgWin0;
IWRAM_DATA struct BgWinDef BgWin1;

IWRAM_DATA struct EarthquakeDef Earthquake;
IWRAM_DATA struct EarthquakeDef ShakeX;

IWRAM_DATA struct FadeDef Fade;

SAVE_DATA  struct AlphaBldDef AbEVB;  // [SAVE] alpha blend state for discontinue

IWRAM_DATA struct BlockScrofsDef BScrArea[2];

IWRAM_DATA struct CChangeDef CCobj;
IWRAM_DATA struct CChangeDef CCbg;

// from global.h
IWRAM_DATA s16 sMainSeq, sGameSeq;
IWRAM_DATA s8 cNextFlg, cGmStartFlg, cVblkFlg;
IWRAM_DATA u8 ucMainTimer;

IWRAM_DATA vu16 usIntrCheck;
IWRAM_DATA u32 IntrMainBuf[0x200];

IWRAM_DATA struct OamData OamBuf[128];
IWRAM_DATA u16 usCont, usCont2, usTrg;
IWRAM_DATA u8 ucCntObj;
IWRAM_DATA u16 usObjScaleX, usObjScaleY;
IWRAM_DATA u16 usObjRotate;

IWRAM_DATA s16 sBgPosX, sBgPosY;
IWRAM_DATA s32 iBgStartX, iBgStartY;
IWRAM_DATA u16 usBgMosaic;
IWRAM_DATA u16 usBgScaleX, usBgScaleY;
IWRAM_DATA u16 usBgRotate;
IWRAM_DATA s16 sBgPa, sBgPb, sBgPc, sBgPd;
IWRAM_DATA u16 usBgEvy, usBgEva, usBgEvb;
SAVE_DATA  u16 usBg0Hofs, usBg0Vofs;  // [SAVE] BG0 scroll for discontinue
SAVE_DATA  u16 usBg1Hofs, usBg1Vofs;  // [SAVE] BG1 scroll for discontinue
SAVE_DATA  u16 usBg2Hofs, usBg2Vofs;  // [SAVE] BG2 scroll for discontinue
SAVE_DATA  u16 usBg3Hofs, usBg3Vofs;  // [SAVE] BG3 scroll for discontinue
IWRAM_DATA u16 usBg0PosX, usBg0PosY;
IWRAM_DATA u8 ucAffObj;
IWRAM_DATA u8 cBgMode, cBgPage;

IWRAM_DATA u16 usFadeTimer;
IWRAM_DATA u16 usGatePosX, usGatePosY;
IWRAM_DATA u8 ucDemoSwitch;

// from ready/file select
// SaveReady[2] at 0x30031d0 -- 2 save slots x 8 bytes each
IWRAM_DATA struct SaveReadyDef SaveReady[2];
// OReady[4] at 0x3004a70 -- 4 OBJ animation slots x 18 bytes each (72 bytes total)
IWRAM_DATA u8 OReady[72];

// from g_pause.h
IWRAM_DATA s8 cPauseFlag;
IWRAM_DATA u8 ucSelectedRecordNum;

// from g_shop.h
IWRAM_DATA s8 cShopFlag;
IWRAM_DATA u8 ucShopItemType;

// VBlank function pointer (set by each screen module)
IWRAM_DATA IntrFunc sVblkFunc;

void SetVblkFunc(IntrFunc fnc)
{
    sVblkFunc = fnc;
}

// Palette fade DMA: blasts precomputed faded palette from EWRAM buffers
// to hardware palette RAM. Bit 0 of Fade.ucDMAf triggers BG palette copy,
// bit 1 triggers OBJ palette copy. Called each frame during fade transitions.
// Matches IDA FadeColor_DMA at 0x8072888.
void FadeColor_DMA(void)
{
    // EWRAM palette buffers (0x02020000 and 0x02020200)
    // These hold precomputed faded BG and OBJ palettes.
    // When ucDMAf bits are set, DMA the corresponding buffer to hardware.
    if (Fade.ucDMAf)
    {
        if (Fade.ucDMAf & 1)
        {
            // BG palette: EWRAM -> BG_PLTT (256 entries x 2 bytes)
            REG_DMA3SAD = (u32)0x02020000;
            REG_DMA3DAD = (u32)BG_PLTT;
            REG_DMA3CNT = ((DMA_ENABLE | DMA_16BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | 256;
            Fade.ucDMAf &= ~1;
        }
        if (Fade.ucDMAf & 2)
        {
            // OBJ palette: EWRAM+0x200 -> OBJ_PLTT (256 entries x 2 bytes)
            REG_DMA3SAD = (u32)0x02020200;
            REG_DMA3DAD = (u32)OBJ_PLTT;
            REG_DMA3CNT = ((DMA_ENABLE | DMA_16BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | 256;
            Fade.ucDMAf &= ~2;
        }
    }
}


// ----------------------------
// MapLH_kaitou -- Custom tilemap RLE decompression
// ----------------------------
// Matching IDA MapLH_kaitou at 0x806c374.
//
// Decompresses tilemap data in the game's custom format.
// Each tile entry is a u16 written at 2-byte stride into destination.
// Only the low byte of each tile entry is set (8-bit tile index).
// The upper byte (palette, flip, priority) comes from pre-initialized values.
//
// Compression format:
//   Byte 0 (mode 0): ASC-Type -> determines output size
//     0: 2048 bytes (one 32x32 screenblock)
//     1 or 2: 4096 bytes (two screenblocks)
//     other: 0x2000 bytes (four screenblocks)
//   Byte 0 (mode 1): first byte of 24-bit size prefix
//
//   Then two passes of RLE data:
//   Pass 1 -- opcode 0x01 sequences (byte-level):
//     Read count byte. Loop until count == 0.
//     Bit 7=1: repeat next byte (count & 0x7F) times
//     Bit 7=0: copy (count) literal bytes
//   Pass 2 -- other opcode sequences (16-bit level):
//     Read 2-byte big-endian count. Loop until count == 0.
//     Bit 15=1: repeat next byte (count & 0x7FFF) times
//     Bit 15=0: copy (count) literal bytes
//
int MapLH_kaitou(u8 mode, const u8 *src, u8 *dest)
{
    int size;
    const u8 *ptr;
    u8 *dst;
    int pass;

    // ---- Determine output size from mode ----
    if (mode == 0)
    {
        // ASC-Type: first byte determines tilemap type
        u8 ascType = *src++;
        if (ascType == 0)
            size = 2048;        // one 32x32 screenblock (0x800 bytes)
        else if (ascType == 1 || ascType == 2)
            size = 4096;        // two screenblocks (0x1000 bytes)
        else
            size = 0x2000;      // four screenblocks
    }
    else if (mode == 1)
    {
        // 24-bit size prefix: (byte0 << 16) | byte1
        size = (*src++ << 16) | *src++;
    }
    else
    {
        // Direct mode: first value written directly
        *dest = *src++;
        dest[1] = 0;
        dest += 2;
        dest[0] = *src++;
        dest[1] = 0;
        dest += 2;
        size = 0;  // no further decompression
        return size;
    }

    ptr = src;
    dst = dest;

    // ---- Two-pass decompression ----
    for (pass = 0; pass <= 1; pass++)
    {
        u8 opcode = *ptr++;

        if (opcode == 1)
        {
            // Pass 1: byte-level RLE
            int count = *ptr++;
            while (count != 0)
            {
                if (count & 0x80)
                {
                    // RLE: repeat next byte (count & 0x7F) times
                    int n = count & 0x7F;
                    u8 tile = *ptr++;
                    while (n-- > 0)
                    {
                        *dst = tile;
                        dst += 2;
                    }
                }
                else
                {
                    // Literal: copy count bytes
                    int n = count;
                    while (n-- > 0)
                    {
                        *dst = *ptr++;
                        dst += 2;
                    }
                }
                count = *ptr++;
            }
        }
        else
        {
            // Pass 2: 16-bit level RLE
            // Read 2-byte big-endian count
            ptr--;  // put back the opcode byte (it's part of the 16-bit value)
            {
                int count = (*ptr++ << 8) | *ptr++;
                while (count != 0)
                {
                    if (count & 0x8000)
                    {
                        // RLE: repeat next byte (count & 0x7FFF) times
                        int n = count & 0x7FFF;
                        u8 tile = *ptr++;
                        while (n-- > 0)
                        {
                            *dst = tile;
                            dst += 2;
                        }
                    }
                    else
                    {
                        // Literal: copy count bytes
                        int n = count;
                        while (n-- > 0)
                        {
                            *dst = *ptr++;
                            dst += 2;
                        }
                    }
                    count = (*ptr++ << 8) | *ptr++;
                }
            }
        }
    }

    return size;
}

// ----------------------------
// CPU fallback (small buffers)
// ----------------------------
static inline void *cpu_memcpy(void *dest, const void *src, u32 len)
{
    u8 *d = (u8*)dest;
    const u8 *s = (const u8*)src;
    while (len--) *d++ = *s++;
    return dest;
}

static inline void *cpu_memset(void *dest, int value, u32 len)
{
    u8 *d = (u8*)dest;
    while (len--) *d++ = (u8)value;
    return dest;
}

// ----------------------------
// DMA versions
// ----------------------------
static inline void *dma_memcpy(void *dest, const void *src, u32 len)
{
    // Only use DMA for word-aligned, large transfers
    if (len < 16) {
        return cpu_memcpy(dest, src, len);
    }

    REG_DMA3CNT = 0; // Stop DMA
    REG_DMA3SAD = (u32)src;
    REG_DMA3DAD = (u32)dest;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (len >> 2);

    // Wait until finished
    while (REG_DMA3CNT & (DMA_ENABLE << 16));

    // Tail bytes if not multiple of 4
    u32 rem = len & 3;
    if (rem) {
        u8 *d = (u8 *)dest + (len & ~3);
        const u8 *s = (const u8 *)src + (len & ~3);
        while (rem--) *d++ = *s++;
    }
    return dest;
}

static inline void *dma_memset(void *dest, int value, u32 len)
{
    // Only use DMA for word-aligned, large transfers
    if (len < 16) {
        return cpu_memset(dest, value, len);
    }

    u32 val= value;
    REG_DMA3SAD = (u32)&val;
    REG_DMA3DAD = (u32)dest;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (len >> 2);

    while (REG_DMA3CNT & (DMA_ENABLE << 16));

    // Tail bytes
    u32 rem = len & 3;
    if (rem) {
        u8 *d = (u8 *)dest + (len & ~3);
        while (rem--) *d++ = (u8)value;
    }
    return dest;
}

// ---- ClearGraphicRam: Clear all graphics RAM (VRAM + OAM + Palette) ----
// Matches SDK ClearGraphicRam = ClearVram + ClearOamRam + ClearPaletteRam.
// Also matches IDA ClearGraphicRam at 0x8000204 (palette clear), plus
// the original game's VRAM/OAM clear done during scene initialization.
void ClearGraphicRam(void)
{
    // 1) Clear all VRAM (96KB: 64KB BG + 32KB OBJ)
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)VRAM;
        REG_DMA3CNT = ((DMA_ENABLE | DMA_16BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16)
                    | (VRAM_SIZE >> 1);
    }

    // 2) Clear OAM -- set all 128 OBJs to disabled (attr0=0x0200, attr1=0)
    //    and set first/last affine param in each group to 0x0100 (double-size flag).
    {
        int i;
        vu16 *oam = (vu16 *)OAM;
        for (i = 0; i < 128; i += 4)
        {
            oam[0] = 0x0200; oam[1] = 0; oam[2] = 0; oam[3] = 0x0100;
            oam[4] = 0x0200; oam[5] = 0; oam[6] = 0; oam[7] = 0;
            oam[8] = 0x0200; oam[9] = 0; oam[10] = 0; oam[11] = 0;
            oam[12] = 0x0200; oam[13] = 0; oam[14] = 0; oam[15] = 0x0100;
            oam += 16;
        }
    }

    // 3) Clear palette RAM (1KB: 256 BG + 256 OBJ entries)
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)PLTT;
        REG_DMA3CNT = ((DMA_ENABLE | DMA_16BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16)
                    | (PLTT_SIZE >> 1);
    }
}

// ---- ClearOamBuf: Fill OamBuf with disabled offscreen OBJs ----
// Sets ucCntObj to 0.
void ClearOamBuf(void)
{
    int i;
    u16 *oam = (u16 *)OamBuf;

    for (i = 0; i < 128; i++)
    {
        oam[0] = 0x00D0;   // attr0: Y=208 (offscreen, not explicitly disabled)
        oam[1] = 0x0200;   // attr1: X=0, disabled
        oam[2] = 0;        // attr2: tile 0, priority 0, palette 0
        oam[3] = 0;        // affineParam: unused
        oam += 4;
    }
    ucCntObj = 0;
}

// ----------------------------
// Public API (overrides libc)
// ----------------------------
void *memcpy(void *dest, const void *src, u32 len)
{
    return dma_memcpy(dest, src, len);
}

void *memset(void *dest, int value, u32 len)
{
    return dma_memset(dest, value, len);
}
