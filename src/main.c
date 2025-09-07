#include <snes.h>

/* BRR SFX blobs + handles */
extern char sfx_coin, sfx_coin_end;
extern char sfx_gameover, sfx_gameover_end;
brrsamples SFX_COIN_HANDLE;
brrsamples SFX_GAMEOVER_HANDLE;


/* BG font from your existing assets */
extern char tilfont, palfont;

/* NEW: sprite sheet (see data.asm) */
extern char sprgfx, sprgfx_end, sprpal;

/* -------------------- helpers -------------------- */
static u16 rng = 0xACE1u;
static u16 rnd16(void) { rng = (u16)(rng * 25173u + 13849u); return rng; }
static s16 sgn(s16 v) { return (v > 0) - (v < 0); }

/* arena bounds (tile coords) */
#define MIN_X 1
#define MAX_X 30
#define MIN_Y 5
#define MAX_Y 24

/* sprite system
   - PVSnesLib wants OAM ids 0,4,8,... (each sprite uses 4 bytes)  */
#define SPR_PLAYER_ID 0
#define SPR_COIN_ID   4
#define SPR_ENEMY_ID  8

/* gfx offsets inside sprites.pic (8x8 tiles) */
#define GFX_PLAYER 0
#define GFX_COIN   1
#define GFX_ENEMY  2

static void hud_init(void) {
    consoleDrawText(2, 1, "COIN CHASE (SNES)");
    consoleDrawText(2, 2, "Score: 000   Time: 060   Sel+Start=Quit");
}

static void hud_score(u16 score) { consoleDrawText(9, 2, "%03u", score); }
static void hud_time(u16 frames_left) {
    u16 secs = frames_left / 60; if (secs > 999) secs = 999;
    consoleDrawText(21, 2, "%03u", secs);
}

static void draw_border(void) {
    char line[33]; int i, y;
    for (i = 0; i < 32; i++) line[i] = '#'; line[32] = 0;
    consoleDrawText(0, MIN_Y - 1, line);
    consoleDrawText(0, MAX_Y + 1, line);
    for (y = MIN_Y; y <= MAX_Y; y++) { consoleDrawText(0, y, "#"); consoleDrawText(31, y, "#"); }
}

static void clear_inside(void) {
    char blanks[31]; int i, y; for (i = 0; i < 30; i++) blanks[i] = ' '; blanks[30] = 0;
    for (y = MIN_Y; y <= MAX_Y; y++) consoleDrawText(1, y, blanks);
}

/* grid <-> pixel (use 8x8 grid; sprites are 8x8) */
static inline u16 tx2px(s16 tx) { return (u16)(tx * 8); }
static inline u16 ty2py(s16 ty) { return (u16)(ty * 8); }

static void rand_tile_avoid(s16 *rx, s16 *ry, s16 ax, s16 ay) {
    do {
        *rx = MIN_X + (rnd16() % (MAX_X - MIN_X + 1));
        *ry = MIN_Y + (rnd16() % (MAX_Y - MIN_Y + 1));
    } while (*rx == ax && *ry == ay);
}

/* wait on Game Over screen: START => restart, SELECT+START => quit */
#define RES_QUIT    0
#define RES_RESTART 1
static int wait_restart_or_quit(void) {
    u16 pad = 0, prev = 0;
    consoleDrawText(5,  ((MIN_Y + MAX_Y) / 2) + 2, "Press START to Restart");
    consoleDrawText(4,  ((MIN_Y + MAX_Y) / 2) + 3, "SELECT+START to Quit");
    while (1) {
        pad = padsCurrent(0);
        if ((pad & KEY_START) && (pad & KEY_SELECT)) return RES_QUIT;
        if ((pad & KEY_START) && !(prev & KEY_START)) return RES_RESTART;
        prev = pad;
        /* Keep SPC700 commands flowing during wait screen */
        spcProcess();
        WaitForVBlank();
    }
}

/* one round using sprites */
static int play_round(void) {
    /* HUD + arena on BG */
    hud_init(); draw_border(); clear_inside();

    /* --- SPRITES setup -------------------------------------------------- */
    /* load sprite GFX+PAL to VRAM; set global OBJ size to 8x8/16x16 */
    oamInitGfxSet((u8*)&sprgfx, (u16)(&sprgfx_end - &sprgfx), (u8*)&sprpal, 16, 0, 0x0000, OBJ_SIZE8_L16);
    /* define the 3 sprites (priority 3 = above BG) */
    oamSet(SPR_PLAYER_ID, 0, 0, 3, 0, 0, GFX_PLAYER, 0);
    oamSetEx(SPR_PLAYER_ID, OBJ_SMALL, OBJ_SHOW);
    oamSet(SPR_COIN_ID,   0, 0, 3, 0, 0, GFX_COIN, 0);
    oamSetEx(SPR_COIN_ID,   OBJ_SMALL, OBJ_SHOW);
    oamSet(SPR_ENEMY_ID,  0, 0, 3, 0, 0, GFX_ENEMY, 0);
    oamSetEx(SPR_ENEMY_ID,  OBJ_SMALL, OBJ_SHOW);

    /* --- game state ----------------------------------------------------- */
    u16 score = 0, pad = 0;
    u8  moveCooldown = 0;
    u16 timeLeft = 60 * 60; /* 60s @ ~60fps */

    s16 px = (MIN_X + MAX_X) / 2,  py = (MIN_Y + MAX_Y) / 2;
    s16 cx, cy, ex, ey;               /* coin & enemy (grid coords) */
    u8  enemyTick = 0;  const u8 ENEMY_RATE = 6;

    s16 nx, ny, dx, dy, stepx, stepy;

    rand_tile_avoid(&cx, &cy, px, py);
    rand_tile_avoid(&ex, &ey, px, py);
    while (ex == cx && ey == cy) rand_tile_avoid(&ex, &ey, px, py);

    /* position sprites */
    oamSetXY(SPR_PLAYER_ID, tx2px(px), ty2py(py));
    oamSetXY(SPR_COIN_ID,   tx2px(cx), ty2py(cy));
    oamSetXY(SPR_ENEMY_ID,  tx2px(ex), ty2py(ey));

    hud_score(score); hud_time(timeLeft);

    spcProcess();

    while (1) {
        pad = padsCurrent(0);
        if ((pad & KEY_START) && (pad & KEY_SELECT)) return RES_QUIT;

        /* timer */
        if (timeLeft) {
            timeLeft--; hud_time(timeLeft);
            if (!timeLeft) {
                consoleDrawText(8, (MIN_Y + MAX_Y) / 2, "TIME UP!  GAME OVER");
                spcPlaySound(1);
                return wait_restart_or_quit();
            }
        }

        /* player move (grid) */
        if (moveCooldown) moveCooldown--;
        else {
            nx = px; ny = py;
            if (pad & KEY_LEFT)  nx--;
            if (pad & KEY_RIGHT) nx++;
            if (pad & KEY_UP)    ny--;
            if (pad & KEY_DOWN)  ny++;
            if (nx < MIN_X) nx = MIN_X; if (nx > MAX_X) nx = MAX_X;
            if (ny < MIN_Y) ny = MIN_Y; if (ny > MAX_Y) ny = MAX_Y;
            if (nx != px || ny != py) {
                px = nx; py = ny;
                oamSetXY(SPR_PLAYER_ID, tx2px(px), ty2py(py));
                moveCooldown = 3;
            }
        }

        /* collect coin */
        if (px == cx && py == cy) {
            score++; hud_score(score);
            rand_tile_avoid(&cx, &cy, px, py);
            while (cx == ex && cy == ey) rand_tile_avoid(&cx, &cy, px, py);
            oamSetXY(SPR_COIN_ID, tx2px(cx), ty2py(cy));
            if (timeLeft < 60 * 60 - 60) timeLeft += 60;
            hud_time(timeLeft);
            /* play BRR #0 (coin) */
            spcPlaySound(0);
        }

        /* enemy move (greedy, throttled) */
        if (++enemyTick >= ENEMY_RATE) {
            enemyTick = 0;
            dx = px - ex; dy = py - ey;
            stepx = sgn(dx); stepy = sgn(dy);
            nx = ex; ny = ey;

            if (dx*dx >= dy*dy) {
                if (ex + stepx >= MIN_X && ex + stepx <= MAX_X && !(ex + stepx == cx && ey == cy)) { nx = ex + stepx; }
                else if (ey + stepy >= MIN_Y && ey + stepy <= MAX_Y && !(ex == cx && ey + stepy == cy)) { ny = ey + stepy; }
            } else {
                if (ey + stepy >= MIN_Y && ey + stepy <= MAX_Y && !(ex == cx && ey + stepy == cy)) { ny = ey + stepy; }
                else if (ex + stepx >= MIN_X && ex + stepx <= MAX_X && !(ex + stepx == cx && ey == cy)) { nx = ex + stepx; }
            }

            if (nx != ex || ny != ey) { ex = nx; ey = ny; oamSetXY(SPR_ENEMY_ID, tx2px(ex), ty2py(ey)); }

            if (ex == px && ey == py) {
                consoleDrawText(7, (MIN_Y + MAX_Y) / 2, "CAUGHT BY X!  GAME OVER");
                spcPlaySound(1);
                return wait_restart_or_quit();
            }
        }

        /* PVSnesLib pushes OAM during VBlank; just wait */
        /* Process SPC700 command queue each frame so SFX play correctly */
        spcProcess();
        WaitForVBlank();
    }
}

/* -------------------- entry -------------------- */
int main(void) {
    /* BG text console init (also sets up OAM internally) */
    consoleSetTextMapPtr(0x6800);
    consoleSetTextGfxPtr(0x3000);
    consoleSetTextOffset(0x0100);
    consoleInitText(0, 16 * 2, &tilfont, &palfont);

    bgSetGfxPtr(0, 0x2000);
    bgSetMapPtr(0, 0x6800, SC_32x32);

    setMode(BG_MODE1, 0);
    bgSetDisable(1); bgSetDisable(2);
    setScreenOn();

    /* ---- SPC / BRR init ---- */
    spcBoot();                       /* upload sound driver to SPC700 */
    spcStop();

    /* Reserve APU RAM blocks for BRR playback (39 ~= 10 KB is plenty for short SFX) */
    spcAllocateSoundRegion(39);

    /* Register two BRR samples into the driver */
    spcSetSoundEntry(
        15, 15, 4,
        (u16)(&sfx_gameover_end - &sfx_gameover),
        (u8*)&sfx_gameover,
        &SFX_GAMEOVER_HANDLE
    );
    spcSetSoundEntry(
        15, 15, 4,
        (u16)(&sfx_coin_end - &sfx_coin),  /* size in bytes */
        (u8*)&sfx_coin,
        &SFX_COIN_HANDLE
    );

    while (1) {
        int r = play_round();
        if (r == RES_QUIT) break;
    }

    consoleDrawText(8, (MIN_Y + MAX_Y) / 2, "Thanks for playing!");
    while (1) { WaitForVBlank(); }
    return 0;
}