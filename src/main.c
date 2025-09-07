#include <snes.h>

extern char tilfont, palfont;          /* from data.asm incbins */

/* -------------------- helpers -------------------- */
static u16 rng = 0xACE1u;
static u16 rnd16(void) { rng = (u16)(rng * 25173u + 13849u); return rng; }
static s16 sgn(s16 v) { return (v > 0) - (v < 0); }

/* arena bounds (tile coords) */
#define MIN_X 1
#define MAX_X 30
#define MIN_Y 5
#define MAX_Y 24

#define ENEMY_CHAR 'X'

static void put_char(u16 x, u16 y, char c) { consoleDrawText(x, y, "%c", c); }

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
    char line[33];
    int i, y;
    for (i = 0; i < 32; i++) line[i] = '#';
    line[32] = 0;
    consoleDrawText(0, MIN_Y - 1, line);
    consoleDrawText(0, MAX_Y + 1, line);
    for (y = MIN_Y; y <= MAX_Y; y++) {
        consoleDrawText(0,  y, "#");
        consoleDrawText(31, y, "#");
    }
}

/* clear inside the arena (keep border) */
static void clear_inside(void) {
    char blanks[31];
    int i, y;
    for (i = 0; i < 30; i++) blanks[i] = ' ';
    blanks[30] = 0;
    for (y = MIN_Y; y <= MAX_Y; y++) consoleDrawText(1, y, blanks);
}

/* random tile not equal to (ax,ay) */
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
        if ((pad & KEY_START) && !(prev & KEY_START)) return RES_RESTART; /* rising edge */
        prev = pad;
        WaitForVBlank();
    }
}

/* one full round; returns RES_RESTART or RES_QUIT */
static int play_round(void) {
    /* fresh HUD + arena */
    hud_init();
    draw_border();
    clear_inside();

    /* state */
    u16 score = 0;
    u16 pad = 0, prev = 0;
    u8  moveCooldown = 0;
    u16 timeLeft = 60 * 60; /* 60s at ~60fps */

    s16 px = (MIN_X + MAX_X) / 2;
    s16 py = (MIN_Y + MAX_Y) / 2;

    s16 cx, cy;           /* coin    */
    s16 ex, ey;           /* enemy   */
    u8  enemyTick = 0;    /* rate    */
    const u8 ENEMY_RATE = 6;

    s16 nx, ny, dx, dy, stepx, stepy;

    rand_tile_avoid(&cx, &cy, px, py);
    rand_tile_avoid(&ex, &ey, px, py);
    while (ex == cx && ey == cy) rand_tile_avoid(&ex, &ey, px, py);

    put_char(px, py, '@');
    put_char(cx, cy, '*');
    put_char(ex, ey, ENEMY_CHAR);

    hud_score(score);
    hud_time(timeLeft);

    while (1) {
        pad = padsCurrent(0);

        /* quit mid-round */
        if ((pad & KEY_START) && (pad & KEY_SELECT)) return RES_QUIT;

        /* timer */
        if (timeLeft > 0) {
            timeLeft--;
            hud_time(timeLeft);
            if (timeLeft == 0) {
                consoleDrawText(8, (MIN_Y + MAX_Y) / 2, "TIME UP!  GAME OVER");
                return wait_restart_or_quit();
            }
        }

        /* player move (throttled) */
        if (moveCooldown) {
            moveCooldown--;
        } else {
            nx = px; ny = py;
            if (pad & KEY_LEFT)  nx--;
            if (pad & KEY_RIGHT) nx++;
            if (pad & KEY_UP)    ny--;
            if (pad & KEY_DOWN)  ny++;

            if (nx < MIN_X) nx = MIN_X; if (nx > MAX_X) nx = MAX_X;
            if (ny < MIN_Y) ny = MIN_Y; if (ny > MAX_Y) ny = MAX_Y;

            if (nx != px || ny != py) {
                put_char(px, py, ' ');
                px = nx; py = ny;
                put_char(px, py, '@');
                moveCooldown = 3;
            }
        }

        /* collect coin */
        if (px == cx && py == cy) {
            score++; hud_score(score);
            put_char(cx, cy, ' ');
            rand_tile_avoid(&cx, &cy, px, py);
            while (cx == ex && cy == ey) rand_tile_avoid(&cx, &cy, px, py);
            put_char(cx, cy, '*');
            if (timeLeft < 60 * 60 - 60) timeLeft += 60;
            hud_time(timeLeft);
        }

        /* enemy move (greedy, throttled) */
        enemyTick++;
        if (enemyTick >= ENEMY_RATE) {
            enemyTick = 0;
            dx = px - ex; dy = py - ey;
            stepx = sgn(dx); stepy = sgn(dy);
            nx = ex; ny = ey;

            if (dx*dx >= dy*dy) {
                if (ex + stepx >= MIN_X && ex + stepx <= MAX_X &&
                    !(ex + stepx == cx && ey == cy)) { nx = ex + stepx; ny = ey; }
                else if (ey + stepy >= MIN_Y && ey + stepy <= MAX_Y &&
                         !(ex == cx && ey + stepy == cy)) { nx = ex; ny = ey + stepy; }
            } else {
                if (ey + stepy >= MIN_Y && ey + stepy <= MAX_Y &&
                    !(ex == cx && ey + stepy == cy)) { nx = ex; ny = ey + stepy; }
                else if (ex + stepx >= MIN_X && ex + stepx <= MAX_X &&
                         !(ex + stepx == cx && ey == cy)) { nx = ex + stepx; ny = ey; }
            }

            if (nx != ex || ny != ey) {
                if (!(ex == cx && ey == cy)) put_char(ex, ey, ' ');
                ex = nx; ey = ny;
                put_char(ex, ey, ENEMY_CHAR);
            }

            if (ex == px && ey == py) {
                consoleDrawText(7, (MIN_Y + MAX_Y) / 2, "CAUGHT BY X!  GAME OVER");
                return wait_restart_or_quit();
            }
        }

        prev = pad;
        WaitForVBlank();
    }
}

/* -------------------- entry -------------------- */
int main(void) {
    /* one-time console/BG init */
    consoleSetTextMapPtr(0x6800);
    consoleSetTextGfxPtr(0x3000);
    consoleSetTextOffset(0x0100);
    consoleInitText(0, 16 * 2, &tilfont, &palfont);

    bgSetGfxPtr(0, 0x2000);
    bgSetMapPtr(0, 0x6800, SC_32x32);

    setMode(BG_MODE1, 0);
    bgSetDisable(1);
    bgSetDisable(2);
    setScreenOn();

    /* play forever until quit */
    while (1) {
        int r = play_round();
        if (r == RES_QUIT) break;
        /* otherwise loop to restart with a clean arena */
    }

    /* optional goodbye screen */
    consoleDrawText(8, (MIN_Y + MAX_Y) / 2, "Thanks for playing!");
    while (1) { WaitForVBlank(); }
    return 0;
}