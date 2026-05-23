#include "gba/gba.h"
#include "gameutils.h"

// from g_select.h
IWRAM_DATA s16 sSelSeq;
IWRAM_DATA u8 ucWorldNum;
IWRAM_DATA u8 ucStageNum;
IWRAM_DATA s8 cSelectedNo; // which save slot is selected

IWRAM_DATA u16 usRandomCount;  // For random count
//IWRAM_DATA u16 usRandomCount2;  // For random count

IWRAM_DATA u16 usTotalGoldMedal;

IWRAM_DATA u16 usMiniBestScore[3];

IWRAM_DATA u8 ucWorldNumBak;

IWRAM_DATA u8 ucEndingMessageKind;

// Jewel piece collection flags
IWRAM_DATA u8 ucKakera1GetFlg;
IWRAM_DATA u8 ucKakera2GetFlg;
IWRAM_DATA u8 ucKakera3GetFlg;
IWRAM_DATA u8 ucKakera4GetFlg;
IWRAM_DATA u8 ucKakera5GetFlg;
IWRAM_DATA u8 ucKakera6GetFlg;
IWRAM_DATA u8 ucKakera7GetFlg;
IWRAM_DATA u8 ucKakera8GetFlg;
IWRAM_DATA u8 ucKakera9GetFlg;
IWRAM_DATA u8 ucKakera10GetFlg;
IWRAM_DATA u8 ucKakera11GetFlg;
IWRAM_DATA u8 ucKakera12GetFlg;


// from g_map.h
IWRAM_DATA u8 ucDisConFlg;
IWRAM_DATA u8 ucSaveFlg;
IWRAM_DATA u8 ucSRAM;
IWRAM_DATA u8 ucPerfect;

IWRAM_DATA u8 ucLanguage;
IWRAM_DATA u8 ucLevel;
IWRAM_DATA u8 ucAllReset;

IWRAM_DATA u8 ucSaveNum;

IWRAM_DATA u8 ucBossPause;
IWRAM_DATA u8 ucBossRoomProgress;

IWRAM_DATA u8 ucBgmOff;
IWRAM_DATA u8 ucGndSaveFlg;

IWRAM_DATA u8 ucResetStop;

IWRAM_DATA u8 ucAutoDemoSW;
IWRAM_DATA u8 ucAutoDemoNum;

IWRAM_DATA u8 ucGmapSubSeq;
IWRAM_DATA u8 ucTitle2f;

IWRAM_DATA u8 ucCurrentLevelDoorDataTableID;
IWRAM_DATA u8 ucRoomID;

IWRAM_DATA u8 ucGateID;
IWRAM_DATA u8 ucGateType;

IWRAM_DATA u8 ucTekiGroup;
IWRAM_DATA u8 ucTekiKowareFlg;

IWRAM_DATA u16 usAlpfaBLD;
IWRAM_DATA u16 usBldCnt;

IWRAM_DATA u8 MapSw[5];

IWRAM_DATA u8 ucObjPriFlg;
IWRAM_DATA u8 ucKeyPri;

IWRAM_DATA u8 ucBgCountDown;
IWRAM_DATA u8 ucBgCDcnt;

IWRAM_DATA u8 ucHblkFlg;

IWRAM_DATA u8  ucAlphaSw;

IWRAM_DATA u16 usTekiBgPosX;
IWRAM_DATA u16 usTekiBgPosY;

IWRAM_DATA u16 usDisConR;
IWRAM_DATA u32 DisConSum;

IWRAM_DATA u8 ucFlashLoop;

IWRAM_DATA u8 ucBgYoki;

IWRAM_DATA u8 ucBgKakiStop;

IWRAM_DATA u8 ucTimeUp;
IWRAM_DATA u8 ucSTEndType;

IWRAM_DATA struct BgDebugDef BgDebug;

IWRAM_DATA struct MapDef Map[4];

IWRAM_DATA struct RoomHeaderDef RoomHeader;

IWRAM_DATA struct PyakuwariDef Pyaku;

IWRAM_DATA struct BgScDef Scroll[4];

IWRAM_DATA struct BgWinDef BgWin0;
IWRAM_DATA struct BgWinDef BgWin1;

IWRAM_DATA struct JishinDef Jishin;
IWRAM_DATA struct JishinDef YureX;

IWRAM_DATA struct FadeDef Fade;

IWRAM_DATA struct AlphaBldDef AbEVB;

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
IWRAM_DATA u16 usBg0Hofs, usBg0Vofs;
IWRAM_DATA u16 usBg1Hofs, usBg1Vofs;
IWRAM_DATA u16 usBg2Hofs, usBg2Vofs;
IWRAM_DATA u16 usBg3Hofs, usBg3Vofs;
IWRAM_DATA u16 usBg0PosX, usBg0PosY;
IWRAM_DATA u8 ucAffObj;
IWRAM_DATA u8 cBgMode, cBgPage;

IWRAM_DATA u16 usFadeTimer;
IWRAM_DATA u16 usGatePosX, usGatePosY;
IWRAM_DATA u8 ucDemoSwitch;

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
