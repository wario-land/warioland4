// Scene 0 OBJ animation stubs
// These functions drive the cat sprite and text OBJ animations
// during the ending sequence of Scene 0.
// The full implementations would read from pattern ROM tables
// and return sprite attribute lists.

#include "../gba/gba.h"

// English text cat animation
int scene0_E_Anime0(int time, u16 **pattern)
{
    *pattern = 0;
    return 1;  // done immediately
}

// Japanese text cat animation
int scene0_J_Anime0(int time, u16 **pattern)
{
    *pattern = 0;
    return 1;
}
