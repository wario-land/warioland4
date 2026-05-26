#ifndef TITLE_H
#define TITLE_H

#include "../gba/gba.h"
#include "../wario/wario.h"

// Title screen return values (from GameTitle)
enum TitleResult {
    TITLE_RESULT_CONTINUE = 0,
    TITLE_RESULT_START    = 1,
    TITLE_RESULT_END      = 2,
    TITLE_RESULT_ESCAPE   = 3,
    TITLE_RESULT_ENDING   = 4,
};

// Title sequence states (sGameSeq values within GameTitle)
enum TitleSequence {
    TSEQ_INIT = 0,
    TSEQ_INIT_EXEC,
    TSEQ_NINTENDO_INIT,
    TSEQ_NINTENDO_EXEC,
    TSEQ_SCENE0_INIT,
    TSEQ_SCENE0_EXEC,
    TSEQ_SCENE1_INIT,
    TSEQ_SCENE1_EXEC,
    TSEQ_SCENE2_INIT,
    TSEQ_SCENE2_EXEC,
    TSEQ_SCENE3_INIT,
    TSEQ_SCENE3_EXEC,
    TSEQ_SCENE4_INIT,
    TSEQ_SCENE4_EXEC,
    TSEQ_NEWSPAPER_INIT,
    TSEQ_NEWSPAPER_EXEC,
    TSEQ_WARIO4_INIT,
    TSEQ_WARIO4_EXEC,
    TSEQ_PUSH_START,

    TSEQ_PYRAMID_INIT = 19,
    TSEQ_PYRAMID_EXEC,
    TSEQ_SCENE7_INIT,
    TSEQ_SCENE7_EXEC,
    TSEQ_SCENE8_INIT,
    TSEQ_SCENE8_EXEC,
    TSEQ_END,

    TSEQ_ESCAPE_INIT = 26,
    TSEQ_ESCAPE_EXEC,
    TSEQ_SCENE10_INIT,
    TSEQ_SCENE10_EXEC,
    TSEQ_KISS_INIT,
    TSEQ_KISS_EXEC,
    TSEQ_SCENE11_INIT,
    TSEQ_SCENE11_EXEC,
    TSEQ_ESCAPE_END,

    TSEQ_ENDING_INIT = 35,
    TSEQ_ENDING_EXEC,
    TSEQ_SCENE3B_INIT,
    TSEQ_SCENE3B_EXEC,
    TSEQ_SCENE4B_INIT,
    TSEQ_SCENE4B_EXEC,
    TSEQ_NEWS_END_INIT,
    TSEQ_NEWS_END_EXEC,
    TSEQ_SUPER_HARD_INIT,
    TSEQ_SUPER_HARD_EXEC,
    TSEQ_THE_END,

    TSEQ_QUICK_START = 46,
};

// Special sGameSeq entry points (from main loop -> GameTitle)
#define TITLE_ENTRY_NORMAL      0
#define TITLE_ENTRY_WARIOLAND  (-1)
#define TITLE_ENTRY_PYRAMID    (-2)
#define TITLE_ENTRY_ESCAPE     (-3)
#define TITLE_ENTRY_ENDING     (-4)

// Nintendo screen sub-sequence states
enum NintendoSubSeq {
    NIN_TILE_DECOMPRESS = 0,
    NIN_ENABLE_DISPLAY,
    NIN_ZOOM_ANIMATION,
    NIN_COLOR_CHANGE,
    NIN_FADE_OUT,
};

// Screen dimensions
#define LCD_WIDTH   240
#define LCD_HEIGHT  160
#define LCD_CENTER_X (LCD_WIDTH / 2)
#define LCD_CENTER_Y (LCD_HEIGHT / 2)

int GameTitle(void);
int GameTitle2(void);
void TitVCountIntr0(void);  // VCOUNT handler for Wario4 BG0 scroll

// ---- Shared helpers (defined in title.c) ----
void TitleInit(void);  // Screen reset -- clears palette, VRAM, display off
void UnPackScreen(const u16 *src, vu16 *screen);
void V_Wait(void);
u16 *SetObj(const u16 *src, u16 *dst, int x, int y);
void EndObj(u16 *dst);
void SetObjPABCD(int index, int angle, s16 scale_x, s16 scale_y);

// Blend / fade helpers
void SetBLDDownMin(u16 bg);
void SetBLDDownMax(u16 bg);
void SetBLDUpMin(u16 bg);
void SetBLDUpMax(u16 bg);
u8   FadeInc(u16 interval);
u8   FadeDec(u16 interval);

// ---- Scene Init/Exec functions ----
// Main title scenes
void Scene0_Init(void);
void Scene0_Exec(int time);
void Scene1_Init(void);
void Scene1_Exec(int time);
void Scene2_Init(void);
void Scene2_Exec(int time);

// Night road (scene3.c)
void Scene3_Init(void);
void Scene3_Exec(int time);

// Highway + cat (scene4.c)
void Scene4_Init(void);
void Scene4_Exec(int time);

// Newspaper headline (newspaper.c)
void Newspaper_Init(void);
void Newspaper_Exec(int time);

// Wario4 main title logo (title_wario4.c)
void Wario4_Init(void);
void Wario4_Exec(int time);

// Pyramid intro (pyramid.c)
void Pyramid_Init(void);
void Pyramid_Exec(int time);

// Into pyramid (scene7.c)
void Scene7_Init(void);
void Scene7_Exec(int time);

// Pyramid treasure room (scene8.c)
void Scene8_Init(void);
void Scene8_Exec(int time);

// Escape from pyramid (scene9.c)
void Scene9_Init(void);
void Scene9_Exec(int time);

// Pyramid collapse (scene10.c)
void Scene10_Init(void);
void Scene10_Exec(int time);

// Kiss scene (kiss.c)
void Kiss_Init(void);
void Kiss_Exec(int time);

// Angel sparkle sequence (scene11.c)
void Scene11_Init(void);
void Scene11_Exec(int time);

// Ending newspaper (news_end.c)
void NewsEnd_Init(void);
void NewsEnd_Exec(int time);

// Super hard message (super_hard.c)
void SuperHard_Init(void);
void SuperHard_Exec(int time);

// ---- OBJ animation functions (title_anim.c) ----
// Each maps a time value to an OBJ pattern table and returns TRUE on cycle completion

// Scene 3: Headlight beam
int scene3_Anime0(int time, const u16 **pattern);

// Scene 4: Cat + car animations
int scene4_Anime0(int time, const u16 **pattern);
int scene4_Anime2(int time, const u16 **pattern);
int scene4_Anime3(int time, const u16 **pattern);
int scene4_Anime4(int time, const u16 **pattern);
int scene4_Anime5(int time, const u16 **pattern);
int scene4_Anime6(int time, const u16 **pattern);
int scene4_Anime7(int time, const u16 **pattern);
int scene4_Anime8(int time, const u16 **pattern);
int scene4_Anime9(int time, const u16 **pattern);
int scene4_Anime10(int time, const u16 **pattern);
u16 *Scene4_SetCarObj(u16 *dst);

// Pyramid: Wario animations
int pyramid_Anime0(int time, const u16 **pattern);
int pyramid_Anime1(int time, const u16 **pattern);
int pyramid_Anime2(int time, const u16 **pattern);
int pyramid_Anime3(int time, const u16 **pattern);

// Scene 7: Cat and smoke animations
int scene7_cat_Anime0(int time, const u16 **pattern);
int scene7_cat_Anime1(int time, const u16 **pattern);
int scene7_cat_Anime2(int time, const u16 **pattern);
int scene7_smoke_Anime22(int time, const u16 **pattern);
int scene7_smoke_Anime23(int time, const u16 **pattern);

// Scene 8: Treasure room
void scene8_Anime0(const u16 **pattern);

// Wario4 car + UI animation functions
int  scene5_Anime0(int time, const u16 **pattern);
int  scene5_Anime1(int time, const u16 **pattern);
int  scene5_Anime2(int time, const u16 **pattern);
int  scene5_Anime6(int time, const u16 **pattern);
int  scene5_Anime7(int time, const u16 **pattern);
int  scene5_Anime9(int time, const u16 **pattern);
int  scene5_Anime10(int time, const u16 **pattern);
int  scene5_Anime11(int time, const u16 **pattern);
int  scene5_Anime14(const u16 **pattern);
int  scene5_Anime15(int time, const u16 **pattern);
void scene5_Anime16(const u16 **pattern);
void scene5_Rock1(const u16 **pattern);
void scene5_Rock2(const u16 **pattern);
void scene5_Rock3(const u16 **pattern);
void scene5_Rock4(const u16 **pattern);
u16 *Scene5_CarMove(int sLocalSeq, int time);  // multi-part car OBJ rendering
void scene5_Tire(const u16 **pattern);

#endif // TITLE_H
