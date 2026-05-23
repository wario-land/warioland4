#ifndef GAMEUTILS_H
#define GAMEUTILS_H

#include "gba/types.h"
#include "main.h"

// from g_select.h
extern s16 sSelSeq; // 0x3000000, (A)SAVE
extern u8 ucWorldNum; // 0x3000002, (A)SAVE
extern u8 ucStageNum; // 0x3000003
extern s8 cSelectedNo; // which save slot is selected, 0x3000004

extern u16 usRandomCount;  // For random count, 0x3000006
//extern u16 usRandomCount2;  // For random count

extern u16 usTotalGoldMedal; // 0x3000008, (A)SAVE

extern u16 usMiniBestScore[3]; // 0x300000a, (A)SAVE

extern u8 ucWorldNumBak; // 0x3000010

extern u8 ucEndingMessageKind; // 0x3000011
enum{ NORMAL_MESSAGE, ALL_GET_MESSAGE, SHARD_MESSAGE };

// Jewel piece collection flags (save data, 0x3000c13-0x3000c3f)
extern u8 ucKakera1GetFlg;
extern u8 ucKakera2GetFlg;
extern u8 ucKakera3GetFlg;
extern u8 ucKakera4GetFlg;
extern u8 ucKakera5GetFlg;
extern u8 ucKakera6GetFlg;
extern u8 ucKakera7GetFlg;
extern u8 ucKakera8GetFlg;
extern u8 ucKakera9GetFlg;
extern u8 ucKakera10GetFlg;
extern u8 ucKakera11GetFlg;
extern u8 ucKakera12GetFlg;
// NORMAL_MESSAGE: The bosses has returned. let's get 12 tresures chest this time.
// ALL_GET_MESSAGE: The bosses has returned.
// SHARD_MESSAGE: You can now play the S-Hard mode.




// from g_map.h
extern u8 ucDisConFlg;  // interrupt SAVE enabled. 0x3000012
extern u8 ucSaveFlg;   // 0:none 1:auto, 2:interrupt, won't be detroyed after load. 0x3000013
extern u8 ucSRAM;    // SRAM no problem:1, error:0, 0x3000014
extern u8 ucPerfect;   // perfect ending flag, 0x3000015

extern u8 ucLanguage;   // 0x3000016 (save)
extern u8 ucLevel;   // game difficulty, 0x3000017 (save)
extern u8 ucAllReset;   // hard reset, 0x3000018

extern u8 ucSaveNum;   // save slot number, 0x3000019

extern u8 ucBossPause;  // 0x300001a
extern u8 ucBossRoomProgress;  // 0x300001b

extern u8 ucBgmOff;   // 0x300001c (save)
extern u8 ucGndSaveFlg;  //ground save 0:no 1:yes, 0x300001d

extern u8 ucResetStop;  //key reset 0:enabled 1:disabled, 0x300001e

extern u8 ucAutoDemoSW;  // 0x300001f
extern u8 ucAutoDemoNum;  // 0x3000020

extern u8 ucGmapSubSeq;  // 0x3000021
extern u8 ucTitle2f;   // set 1 when entering the game from Title 2, 0x3000022

extern u8 ucCurrentLevelDoorDataTableID; // 0x3000023 (save)
extern u8 ucRoomID;   // 0x3000024

extern u8 ucGateID;   // 0x3000025 (save)
extern u8 ucGateType;   // 0x3000026 (save)

extern u8 ucTekiGroup;  // 0x3000027 (save)
extern u8 ucTekiKowareFlg; // 0x3000028, Destroys blocks with "enemy-only" hits. 0: OFF

extern u16 usAlpfaBLD;   // 0x300002a (set and clear with VBLK)
extern u16 usBldCnt;   // 0x300002c (set and clear with VBLK)

extern u8 MapSw[5];   // 0x300002e (save)

extern u8 ucObjPriFlg;  // 0x3000033, Has the OBJ priority changed? (NO: 0, YES: 1)
extern u8 ucKeyPri;   // 0x3000034, Highest priority for the key. 0: No BG specified. 1: Priority=0

extern u8 ucBgCountDown;  // 0x3000035, BG event management after the SW is pressed
extern u8 ucBgCDcnt;   // 0x3000036

extern u8 ucHblkFlg;   // 0x3000037, HBLK processing type branch flag

extern u8  ucAlphaSw;   // 0x3000038, (for external operation)

extern u16 usTekiBgPosX;  // 0x300003a, SCR position setting from enemy (BG0-PosX) (save)
extern u16 usTekiBgPosY;  // 0x300003c, SCR position setting from enemy (save)

extern u16 usDisConR;   // 0x300003e, Number of broken blocks (for interrupted save) (save)
extern u32 DisConSum;   // 0x3000040, Sum check (probably unnecessary)

extern u8 ucFlashLoop;  // 0x3000044, Screen FLASH-WORK

extern u8 ucBgYoki;   // 0x3000045, Yoki Room BG-EVENT

extern u8 ucBgKakiStop;  // 0x3000046, Stop BG rewriting

extern u8 ucTimeUp;   // 0x3000047
extern u8 ucSTEndType;  // 0x3000048

struct BgDebugDef
{
    u8 DeScr;   // 0x300004d
    u8 GateNumH;  // 0x300004e
    u8 GateNumL;  // 0x300004f
    u16 PanelNum;  // 0x3000050
};
extern struct BgDebugDef BgDebug;

struct MapDef
{
    u16 *pData;  // decompressed map data address
    u16 uswidth;  // width
    u16 usheight;  // height
};
extern struct MapDef Map[4]; // layer 0, 1, 2, 3, start from 0x3000054

// 0x300006c - 0x3000073 reserved

struct RoomHeaderDef
{
    u8  TilesetID;
    u8  Layer0Type; // include mapping type and other things
    u8  Layer1Type;
    u8  Layer2Type;
    u8  Layer3Type;
    u8 *Layer0Data;
    u8 *Layer1Data;
    u8 *Layer2Data;
    u8 *Layer3Data;
    u8  CameraControlType; // FixedY = 1, NoLimit = 2, HasControlAttrs = 3, Vertical_Seperated = 4,
    u8  Layer3Scrolling;
    u8  RenderEffect;
    u8  DATA_1B;
    u8 *ucTekiData_Hard; //Hard
    u8 *ucTekiData_Normal; //Normal
    u8 *ucTekiData_SHard; //SHard
    u8  RasterKind;
    u8  Water;
    u16 usVolume; // BGM Volume
};
extern struct RoomHeaderDef RoomHeader; // start from 0x3000074

struct PyakuwariDef
{
 u16 usField; // 0x30000a0
 u16 usKoka;  // 0x30000a2, 01(in water) 00(else)
 u16 usTeki;  // 0x30000a4
};
extern struct PyakuwariDef Pyaku;

struct BgScDef
{
 u16 H; // Layer X-offset
 u16 V; // Layer Y-offset
};
extern struct BgScDef Scroll[4]; // Layer 0-3 offset, start from 0x30000a8

struct BgWinDef
{
 u8 ucXL; // window 1 horizontal dimensions, copy to 0x4000042
 u8 ucXR; // window 1 vertical dimensions, copy to 0x4000046
 u8 ucYU;
 u8 ucYD;
 u8 ucCnt;
};
extern struct BgWinDef BgWin0; // 0x30000c0
extern struct BgWinDef BgWin1;

struct JishinDef
{
 u8 Pow; // Setting frame fluctuation
 u8 Fcnt; // Initial value 0 or 2 or greater
 u8 Kind; // 0 or 1
 u8 Stat; // 0 or 1
};
extern struct JishinDef Jishin; // 0x30000cb
extern struct JishinDef YureX; // unused in the release version (?)

struct FadeDef
{
 u8 ucKind;  // GATE fade type, 0x30000d0
 u8 ucCnt;  // tbl-DATA reference number, 0x30000d1
 u8 ucDat;  // Calculation data, 0x30000d2
 u8 ucDip;  // local OK, 0x30000d3
 u8 ucDMAf;  // Color DMA transfer f (Set the specified bit to start DMA), 0x30000d4
 u8 ucSW;  // Calculation complete, 0x30000d5
};
extern struct FadeDef Fade;

struct AlphaBldDef
{
 u8 target; // Target value, 0x30000d8
 u8 work; // Changing value, 0x30000d9
 u8 room; // Room setting value, 0x30000da
 u8 cnt; // 0x30000db
};
extern struct AlphaBldDef AbEVB;    // (save)

struct BlockScrofsDef
{
 u8 Type; // 0x30000dc
 u16 R;  // camera X maximum
 u16 L;  // camera X minimum
 u16 U;  // camera Y minimum
 u16 D;  // camera Y maximum
};
extern struct BlockScrofsDef BScrArea[2];

struct CChangeDef
{
 u8 ucKind;  //
 u8 ucPltt;  // Starting palette number
 u8 ucKazu;  // Number of palettes
 u8 ucDat;  // Counter
 u8 ucDMAf;  // DMA transfer switch
 u8 ucSp;  // Change SP
};
extern struct CChangeDef CCobj; // 0x30000f4
extern struct CChangeDef CCbg; // 0x30000fc


// from g_pause.h
extern s8 cPauseFlag; // 0x3000c35, OFF 0, ON 1, RETURN 2, RETIRE 3, SAVE 4
extern u8 ucSelectedRecordNum; // 0x3000c36, 0: Default, 1-16 stage records

// from g_shop.h
extern s8 cShopFlag; // 0x3000c37, used with PAUSE, OFF 0, ON 1
extern u8 ucShopItemType; // 0x3000c38
enum ShopItemTypeDef
{
    SHOP_ITEM_TYPE_NONE,
    SHOP_ITEM_TYPE_BOMB,
    SHOP_ITEM_TYPE_CANON,
    SHOP_ITEM_TYPE_WHITEMAN,
    SHOP_ITEM_TYPE_MUSIC,
    SHOP_ITEM_TYPE_DOG,
    SHOP_ITEM_TYPE_KISS,
    SHOP_ITEM_TYPE_GENKOTU,
    SHOP_ITEM_TYPE_DRAGON
};

// from global.h
extern s16 sMainSeq, sGameSeq; // 0x3000c3a, // 0x3000c3c
enum MainSeqDef
{
    MS_TITLE = 0,
    MS_SELECT = 1,
    MS_MAIN = 2,
    MS_RESET = 3,
    MS_PAUSE = 4,
    MS_SAVEINTERRUPT = 5,
    MS_MINI = 6,
    MS_SHOP = 7,
    MS_DEMO = 8,
    MS_FILESELECT = 9,
    MS_DELETE = 10,
    MS_TITLE2 = 11,
    MS_CREDITS = 12
};

extern s8 cNextFlg, cGmStartFlg, cVblkFlg; // 0x3000c3e
extern u8 ucMainTimer; // 0x3000c41

extern vu16 usIntrCheck; // 0x3000c42
extern u32 IntrMainBuf[0x200];

extern struct OamData OamBuf[128]; // 0x3001444
extern u16 usCont; // 0x3001844, KeyPressContinuous
extern u16 usCont2; // KeyPressPrevious
extern u16 usTrg; // KeyPress1Frame
extern u8 ucCntObj; // 300184a
extern u16 usObjScaleX, usObjScaleY; //300184c
extern u16 usObjRotate; // 0x3001850

extern s16 sBgPosX, sBgPosY; // 0x3001852
extern s32 iBgStartX, iBgStartY; // 0x3001858
extern u16 usBgMosaic; // 0x3001860
extern u16 usBgScaleX, usBgScaleY; // 0x3001862
extern u16 usBgRotate; // 0x3001866
extern s16 sBgPa, sBgPb, sBgPc, sBgPd; // 0x3001868
extern u16 usBgEvy, usBgEva, usBgEvb; // 0x3001870
extern u16 usBg0Hofs, usBg0Vofs; // 0x3001876
extern u16 usBg1Hofs, usBg1Vofs; // 0x300187a
extern u16 usBg2Hofs, usBg2Vofs; // 0x300187e
extern u16 usBg3Hofs, usBg3Vofs; // 0x3001882
extern u16 usBg0PosX, usBg0PosY; // 0x3001886
extern u8 ucAffObj; // 0x300188a
extern u8 cBgMode, cBgPage; // 0x300188b

extern u16 usFadeTimer; // 0x300188e
extern u16 usGatePosX, usGatePosY; // 0x3001890
extern u8 ucDemoSwitch; // 0x3001894

// re-implement the classic c standard functions here,
// since the gcc will call things like memset(..) and other functions
// but we don't want to use dependencies
void *memcpy(void *dest, const void *src, u32 len);
void *memset(void *dest, int value, u32 len);

// Interrupt / VBlank management
extern IntrFunc sVblkFunc;
void SetVblkFunc(IntrFunc fnc);


#endif // GAMEUTILS_H