// Title screen OBJ animation functions
//
// These functions map a time value (animation frame counter) to an OBJ sprite
// pattern table. Each pattern table is an array of [count, attr0, attr1, attr2]
// tuples consumed by SetObj(). Returns TRUE (non-zero) when animation cycle
// completes (time == last frame of cycle).
//
// OBJ pattern tables extracted from original ROM via IDA Pro MCP.
// Animation logic verified against IDA decompilation.

#include "../gba/gba.h"
#include "title.h"

// ====================================================================
//  Scene 3: Headlight beam
// ====================================================================
extern const u16 scene3_000[];

int scene3_Anime0(int time, const u16 **pattern)
{
    if (time & 1)
        *pattern = 0;
    else
        *pattern = scene3_000;
    return time & 1;
}

// ====================================================================
//  Scene 4: Highway — Cat, car, paper plane
// ====================================================================
extern const u16 scene4_000[], scene4_001[], scene4_002[], scene4_003[];
extern const u16 scene4_004[], scene4_005[], scene4_006[], scene4_007[];
extern const u16 scene4_008[], scene4_009[];
extern const u16 scene4_00A[], scene4_00B[], scene4_00C[], scene4_00D[], scene4_00E[];
extern const u16 scene4_00F[], scene4_010[], scene4_011[];
extern const u16 scene4_012[], scene4_013[], scene4_014[], scene4_015[];
extern const u16 scene4_016[], scene4_017[], scene4_018[], scene4_019[], scene4_01A[];
extern const u16 scene4_01B[], scene4_01C[], scene4_01D[], scene4_01E[];
extern const u16 scene4_obj_carL[], scene4_obj_carR[];
extern s16 ob_pos_x, ob_pos_y;
extern u16 ob_scale_x, ob_palette, ob_priority;

// Anime0: Cat running cycle (72 frames, 12f per pose)
int scene4_Anime0(int time, const u16 **pattern)
{
    int t = time % 72;
    if (t <= 5)       *pattern = scene4_000;
    else if (t <= 11)  *pattern = scene4_001;
    else if (t <= 17)  *pattern = scene4_002;
    else if (t <= 23)  *pattern = scene4_003;
    else if (t <= 29)  *pattern = scene4_000;
    else if (t <= 35)  *pattern = scene4_005;
    else if (t <= 41)  *pattern = scene4_006;
    else if (t <= 47)  *pattern = scene4_003;
    else if (t <= 53)  *pattern = scene4_000;
    else if (t <= 59)  *pattern = scene4_001;
    else if (t <= 65)  *pattern = scene4_002;
    else               *pattern = scene4_003;
    return (t == 71);
}

// Anime2: Cat crouching/idle (40 frames, alternates every 20f)
int scene4_Anime2(int time, const u16 **pattern)
{
    int t = time % 40;
    if (t > 19) *pattern = scene4_009;
    else        *pattern = scene4_008;
    return (t == 39);
}

// Anime3: Cat looking around (24 frames)
int scene4_Anime3(int time, const u16 **pattern)
{
    int t = time % 24;
    if (t <= 3 || (t > 11 && t <= 15) || (t > 19))
        *pattern = scene4_00A;
    else
        *pattern = scene4_00B;
    return (t == 23);
}

// Anime4: Cat catching paper (52 frames)
int scene4_Anime4(int time, const u16 **pattern)
{
    int t = time % 52;
    if (t <= 3 || t > 31)
        *pattern = scene4_00C;
    else if (t <= 7 || (t > 11 && t <= 15) || (t > 23 && t <= 27) || (t > 27 && t <= 31))
        *pattern = scene4_00D;
    else
        *pattern = scene4_00E;
    return (t == 51);
}

// Anime5: Cat flying with paper (59 frames)
int scene4_Anime5(int time, const u16 **pattern)
{
    int t = time % 59;
    if (t <= 5 || t > 30)
        *pattern = scene4_00F;
    else if (t <= 11 || (t > 20 && t <= 23) || (t > 26 && t <= 30))
        *pattern = scene4_010;
    else
        *pattern = scene4_011;
    return (t == 58);
}

// Anime6: Cat diving (90 frames, 4 poses)
int scene4_Anime6(int time, const u16 **pattern)
{
    int t = time % 90;
    if (t <= 19)       *pattern = scene4_00C;
    else if (t <= 29)   *pattern = scene4_012;
    else if (t <= 39)   *pattern = scene4_01D;
    else                *pattern = scene4_01E;
    return (t == 89);
}

// Anime7: Cat flying up with paper — rapid wing flapping (63 frames)
int scene4_Anime7(int time, const u16 **pattern)
{
    int t = time % 63;
    if (t <= 0)        *pattern = scene4_013;
    else if (t <= 1)   *pattern = scene4_014;
    else if (t <= 3)   *pattern = scene4_013;
    else if (t <= 4)   *pattern = scene4_014;
    else               *pattern = scene4_015;
    return (t == 62);
}

// Anime8: Paper floating motion (60 frames, 5 poses)
int scene4_Anime8(int time, const u16 **pattern)
{
    int t = time % 60;
    if (t <= 29)       *pattern = scene4_016;
    else if (t <= 37)   *pattern = scene4_017;
    else if (t <= 45)   *pattern = scene4_018;
    else if (t <= 53)   *pattern = scene4_019;
    else                *pattern = scene4_01A;
    return (t == 59);
}

// Anime9: Paper gliding/rocking (55 frames, 5 poses)
int scene4_Anime9(int time, const u16 **pattern)
{
    int t = time % 55;
    if (t <= 9)        *pattern = scene4_01A;
    else if (t <= 18)   *pattern = scene4_019;
    else if (t <= 26)   *pattern = scene4_018;
    else if (t <= 34)   *pattern = scene4_017;
    else                *pattern = scene4_016;
    return (t == 54);
}

// Anime10: Cat shaking/vibrating (8 frames, alternates every 4f)
int scene4_Anime10(int time, const u16 **pattern)
{
    int t = time % 8;
    if (t > 3) *pattern = scene4_01C;
    else       *pattern = scene4_01B;
    return (t == 7);
}

// Scene4_SetCarObj: Renders car as two affine OBJ halves.
// Uses ob_scale_x for width, ob_palette/ob_priority for attr2.
u16 *Scene4_SetCarObj(u16 *dst)
{
    int scaledWidth, carX, carY;
    u16 palPrio;

    palPrio = (ob_palette << 12) | (ob_priority << 10);
    scaledWidth = (s16)((s32)64 * ob_scale_x / 256);
    carX = ob_pos_x + (64 - scaledWidth) / 2;
    carY = ob_pos_y;

    dst = SetObj(scene4_obj_carL, dst, carX, carY);
    dst[-2] = (dst[-2] & 0x3FF) | palPrio;

    dst = SetObj(scene4_obj_carR, dst, carX + scaledWidth, carY);
    dst[-2] = (dst[-2] & 0x3FF) | palPrio;

    return dst;
}

// ====================================================================
//  Pyramid: Wario OBJ animation states
// ====================================================================
extern const u16 pyramid_000[], pyramid_001[], pyramid_002[], pyramid_003[];
extern const u16 pyramid_004[], pyramid_005[], pyramid_006[], pyramid_007[];
extern const u16 pyramid_008[], pyramid_009[], pyramid_00A[], pyramid_00B[];
extern const u16 pyramid_00C[], pyramid_00D[], pyramid_00E[], pyramid_00F[], pyramid_010[];
extern const u16 pyramid_011[], pyramid_012[], pyramid_013[], pyramid_014[], pyramid_015[];
extern const u16 pyramid_016[], pyramid_017[], pyramid_018[], pyramid_019[];

// Anime0: Wario guts pose (72 frames, 13 poses)
int pyramid_Anime0(int time, const u16 **pattern)
{
    int t = time % 72;
    if (t <= 19)       *pattern = pyramid_004;
    else if (t <= 23)   *pattern = pyramid_005;
    else if (t <= 27)   *pattern = pyramid_006;
    else if (t <= 31)   *pattern = pyramid_007;
    else if (t <= 37)   *pattern = pyramid_008;
    else if (t <= 43)   *pattern = pyramid_009;
    else if (t <= 47)   *pattern = pyramid_00F;
    else if (t <= 51)   *pattern = pyramid_010;
    else if (t <= 55)   *pattern = pyramid_011;
    else if (t <= 59)   *pattern = pyramid_012;
    else if (t <= 63)   *pattern = pyramid_013;
    else if (t <= 67)   *pattern = pyramid_014;
    else                *pattern = pyramid_015;
    return (t == 71);
}

// Anime1: Wario waiting (28 frames)
int pyramid_Anime1(int time, const u16 **pattern)
{
    int t = time % 28;
    if (t <= 7)                    *pattern = pyramid_001;
    else if (t <= 13 || t > 21)    *pattern = pyramid_002;
    else                           *pattern = pyramid_003;
    return (t == 27);
}

// Anime2: Wario walking (36-frame cycle, 6 poses)
int pyramid_Anime2(int time, const u16 **pattern)
{
    int t = time % 36;
    if (t <= 5)       *pattern = pyramid_000;
    else if (t <= 11)  *pattern = pyramid_00A;
    else if (t <= 17)  *pattern = pyramid_00B;
    else if (t <= 23)  *pattern = pyramid_00C;
    else if (t <= 29)  *pattern = pyramid_00D;
    else               *pattern = pyramid_00E;
    return (t == 35);
}

// Anime3: Wario happy dance (24-frame cycle, 4 poses)
int pyramid_Anime3(int time, const u16 **pattern)
{
    int t = time % 24;
    if (t <= 5)       *pattern = pyramid_016;
    else if (t <= 11)  *pattern = pyramid_017;
    else if (t <= 17)  *pattern = pyramid_018;
    else               *pattern = pyramid_019;
    return (t == 0);
}

// ====================================================================
//  Scene 7: Cat and smoke in tunnel
// ====================================================================
extern const u16 scene7_cat_000[], scene7_cat_001[], scene7_cat_002[];
extern const u16 scene7_cat_003[], scene7_cat_004[], scene7_cat_005[];
extern const u16 scene7_cat_006[], scene7_cat_007[], scene7_cat_008[];
extern const u16 scene7_cat_009[], scene7_cat_00A[], scene7_cat_00B[];
extern const u16 scene7_smoke_054[], scene7_smoke_055[];
extern const u16 scene7_smoke_056[], scene7_smoke_057[];

int scene7_cat_Anime0(int time, const u16 **pattern)
{
    int t = time % 329;
    if (t <= 49)       *pattern = scene7_cat_000;
    else if (t <= 53)   *pattern = scene7_cat_001;
    else if (t <= 57)   *pattern = scene7_cat_002;
    else if (t <= 61)   *pattern = scene7_cat_001;
    else if (t <= 111)  *pattern = scene7_cat_000;
    else if (t <= 115)  *pattern = scene7_cat_001;
    else if (t <= 123)  *pattern = scene7_cat_002;
    else if (t <= 127)  *pattern = scene7_cat_001;
    else if (t <= 177)  *pattern = scene7_cat_000;
    else if (t <= 192)  *pattern = scene7_cat_003;
    else if (t <= 200)  *pattern = scene7_cat_004;
    else if (t <= 208)  *pattern = scene7_cat_005;
    else if (t <= 212)  *pattern = scene7_cat_006;
    else if (t <= 216)  *pattern = scene7_cat_007;
    else if (t <= 220)  *pattern = scene7_cat_008;
    else if (t <= 224)  *pattern = scene7_cat_009;
    else if (t <= 228)  *pattern = scene7_cat_00A;
    else                *pattern = scene7_cat_00B;
    return (t == 328);
}

int scene7_cat_Anime1(int time, const u16 **pattern)
{
    *pattern = scene7_cat_000;
    return (time % 40 == 39);
}

int scene7_cat_Anime2(int time, const u16 **pattern)
{
    *pattern = scene7_cat_002;
    return (time % 20 == 19);
}

int scene7_smoke_Anime22(int time, const u16 **pattern)
{
    int t = time % 22;
    if (t <= 3)        *pattern = scene7_smoke_054;
    else if (t <= 7)   *pattern = scene7_smoke_055;
    else if (t <= 11)  *pattern = scene7_smoke_056;
    else if (t <= 15)  *pattern = scene7_smoke_057;
    else               *pattern = 0;
    return (t == 0);
}

int scene7_smoke_Anime23(int time, const u16 **pattern)
{
    return scene7_smoke_Anime22(time, pattern);
}

// ====================================================================
//  Scene 8: Wario in treasure room
// ====================================================================
extern const u16 scene8_000[];

void scene8_Anime0(const u16 **pattern)
{
    *pattern = scene8_000;
}

// ====================================================================
//  Wario4 / Scene5: Car, smoke, UI
// ====================================================================
extern const u16 scene5_000[], scene5_001[];
extern const u16 scene5_002[], scene5_003[];
extern const u16 scene5_00B[], scene5_00C[], scene5_00D[];
extern const u16 scene5_012[], scene5_013[], scene5_014[];
extern const u16 scene5_01C[];
extern const u16 scene5_020[], scene5_021[];
extern const u16 scene5_022[], scene5_023[];
extern const u16 scene5_024[], scene5_025[];
extern const u16 scene5_026[], scene5_027[], scene5_028[], scene5_029[];
extern const u16 scene5_02A[], scene5_02B[], scene5_02C[], scene5_02D[], scene5_02E[];
extern const u16 scene5_02F[];

int scene5_Anime0(int time, const u16 **pattern)
{
    int t = time % 9;
    if (t > 5) *pattern = scene5_001;
    else       *pattern = scene5_000;
    return (t == 8);
}

int scene5_Anime1(int time, const u16 **pattern)
{
    int t = time % 18;
    if (t > 8) *pattern = scene5_003;
    else       *pattern = scene5_002;
    return (t == 17);
}

int scene5_Anime2(int time, const u16 **pattern)
{
    int t = time % 36;
    if (t > 17) *pattern = scene5_021;
    else        *pattern = scene5_020;
    return (t == 35);
}

int scene5_Anime6(int time, const u16 **pattern)
{
    int t = time % 24;
    if (t > 15)      *pattern = scene5_00D;
    else if (t > 7)  *pattern = scene5_00C;
    else             *pattern = scene5_00B;
    return (t == 23);
}

int scene5_Anime7(int time, const u16 **pattern)
{
    int t = time % 116;
    if (t > 15)      *pattern = scene5_014;
    else if (t > 7)  *pattern = scene5_013;
    else             *pattern = scene5_012;
    return (t == 115);
}

int scene5_Anime9(int time, const u16 **pattern)
{
    int t = time % 24;
    if (t > 11) *pattern = scene5_023;
    else        *pattern = scene5_022;
    return (t == 23);
}

int scene5_Anime10(int time, const u16 **pattern)
{
    int t = time % 16;
    if (t > 7) *pattern = scene5_025;
    else       *pattern = scene5_024;
    return (t == 15);
}

int scene5_Anime11(int time, const u16 **pattern)
{
    *pattern = scene5_026;
    return 0;
}

int scene5_Anime14(const u16 **pattern)
{
    *pattern = scene5_02A;
    return 0;
}

int scene5_Anime15(int time, const u16 **pattern)
{
    int t = time % 42;
    if (t <= 2)       *pattern = scene5_026;
    else if (t <= 5)   *pattern = scene5_027;
    else if (t <= 9)   *pattern = scene5_028;
    else if (t <= 13)  *pattern = scene5_029;
    else if (t <= 18)  *pattern = scene5_02A;
    else if (t <= 23)  *pattern = scene5_02B;
    else if (t <= 29)  *pattern = scene5_02C;
    else if (t <= 35)  *pattern = scene5_02D;
    else               *pattern = scene5_02E;
    return (t == 41);
}

void scene5_Anime16(const u16 **pattern)
{
    *pattern = scene5_02F;
}

void scene5_Tire(const u16 **pattern)
{
    *pattern = scene5_01C;
}
