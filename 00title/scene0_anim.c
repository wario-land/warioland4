// Scene 0 OBJ animation functions — ending text animations
// Drives cat/sprite OBJ patterns during Scene 0's ending sequence.
// Each function maps a time counter to an OBJ pattern table.
// Returns TRUE when the animation cycle completes (time > 150).

#include "../gba/gba.h"

// ---- Pattern data references (extracted from ROM via IDA) ----
extern const u16 scene0_E_000[], scene0_E_001[], scene0_E_002[], scene0_E_003[];
extern const u16 scene0_E_004[], scene0_E_005[], scene0_E_006[], scene0_E_007[];
extern const u16 scene0_E_008[], scene0_E_009[], scene0_E_00A[], scene0_E_00B[];
extern const u16 scene0_E_00C[], scene0_E_00D[], scene0_E_00E[], scene0_E_00F[];
extern const u16 scene0_J_000[], scene0_J_001[], scene0_J_002[], scene0_J_003[];
extern const u16 scene0_J_004[], scene0_J_005[], scene0_J_006[], scene0_J_007[];

// English text ending animation (from IDA scene0_E_Anime0 at 0x800ba4c)
// 16 pattern frames over ~150 time units, each lasting 8-16 frames.
// Frame boundaries: 0-15, 16-23, 24-31, 32-47, 48-55, 56-63, 64-71,
//   72-87, 88-95, 96-103, 104-111, 112-119, 120-127, 128-135, 136-143, 144+
int scene0_E_Anime0(int time, u16 **pattern)
{
    u16 *v3;

    if (time > 15)
    {
        if (time > 23)
        {
            if (time > 31)
            {
                if (time > 47)
                {
                    if (time > 55)
                    {
                        if (time > 63)
                        {
                            if (time > 71)
                            {
                                if (time > 87)
                                {
                                    if (time > 95)
                                    {
                                        if (time > 103)
                                        {
                                            if (time > 111)
                                            {
                                                if (time > 119)
                                                {
                                                    if (time > 127)
                                                    {
                                                        if (time > 135)
                                                        {
                                                            if (time > 143)
                                                                v3 = (u16 *)scene0_E_00F;
                                                            else
                                                                v3 = (u16 *)scene0_E_00E;
                                                        }
                                                        else { v3 = (u16 *)scene0_E_00D; }
                                                    }
                                                    else { v3 = (u16 *)scene0_E_00C; }
                                                }
                                                else { v3 = (u16 *)scene0_E_00B; }
                                            }
                                            else { v3 = (u16 *)scene0_E_00A; }
                                        }
                                        else { v3 = (u16 *)scene0_E_009; }
                                    }
                                    else { v3 = (u16 *)scene0_E_008; }
                                }
                                else { v3 = (u16 *)scene0_E_007; }
                            }
                            else { v3 = (u16 *)scene0_E_006; }
                        }
                        else { v3 = (u16 *)scene0_E_005; }
                    }
                    else { v3 = (u16 *)scene0_E_004; }
                }
                else { v3 = (u16 *)scene0_E_003; }
            }
            else { v3 = (u16 *)scene0_E_002; }
        }
        else { v3 = (u16 *)scene0_E_001; }
    }
    else { v3 = (u16 *)scene0_E_000; }

    *pattern = v3;

    // Sound cue at each pattern transition (skip: m4a sound)
    // if (!time || time==16 || time==24 || time==32 || time==48 ||
    //     time==56 || time==64 || time==72 || time==88 || time==96 ||
    //     time==104 || time==112 || time==120)
    //     m4aSongNumStartOrChange(490);

    return time > 150;
}

// Japanese text ending animation (from IDA scene0_J_Anime0 at 0x800bb5c)
// 8 pattern frames over ~150 time units.
// Frame boundaries: 0-18, 19-37, 38-56, 57-75, 76-94, 95-113, 114-132, 133+
int scene0_J_Anime0(int time, u16 **pattern)
{
    u16 *v3;

    if (time > 18)
    {
        if (time > 37)
        {
            if (time > 56)
            {
                if (time > 75)
                {
                    if (time > 94)
                    {
                        if (time > 113)
                        {
                            if (time > 132)
                                v3 = (u16 *)scene0_J_007;
                            else
                                v3 = (u16 *)scene0_J_006;
                        }
                        else { v3 = (u16 *)scene0_J_005; }
                    }
                    else { v3 = (u16 *)scene0_J_004; }
                }
                else { v3 = (u16 *)scene0_J_003; }
            }
            else { v3 = (u16 *)scene0_J_002; }
        }
        else { v3 = (u16 *)scene0_J_001; }
    }
    else { v3 = (u16 *)scene0_J_000; }

    *pattern = v3;

    // Sound cue (skip: m4a sound)
    // if (!time || time==19 || time==38 || time==57 || time==76)
    //     m4aSongNumStartOrChange(490);

    return time > 150;
}
