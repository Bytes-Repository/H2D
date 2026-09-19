#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL2_gfxPrimitives.h>

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* ── fixed-size handle tables ──────────────────────────────────────── */

#define H2D_MAX_TEXTURES 4096
#define H2D_MAX_SOUNDS   1024
#define H2D_MAX_MUSIC    64
#define H2D_MAX_KEYS     256

static SDL_Window   *g_window   = NULL;
static SDL_Renderer *g_renderer = NULL;

static SDL_Texture *g_textures[H2D_MAX_TEXTURES];
static int          g_texture_w[H2D_MAX_TEXTURES];
static int          g_texture_h[H2D_MAX_TEXTURES];
static int          g_texture_next = 1; /* 0 is reserved for "invalid handle" */

static Mix_Chunk *g_sounds[H2D_MAX_SOUNDS];
static int        g_sound_next = 1;

static Mix_Music *g_music[H2D_MAX_MUSIC];
static int        g_music_next = 1;

static int g_audio_ready = 0;

/* Keyboard state, snapshotted once per h2d_begin_frame() call so
 * is_key_pressed/is_key_released can compare "this frame" vs "last
 * frame" without H# itself having to track it. Indexed by our own
 * stable H2D_KEY_* codes (see h2d_key_to_scancode below), not raw SDL
 * scancodes, so the H# side never has to know an SDL enum value. */
static uint8_t g_keys_now[H2D_MAX_KEYS];
static uint8_t g_keys_prev[H2D_MAX_KEYS];

static int g_mouse_x = 0, g_mouse_y = 0;
static int g_mouse_buttons_now = 0;  /* bitmask: 1=left 2=middle 4=right */
static int g_mouse_buttons_prev = 0;
static double g_mouse_wheel = 0.0;

static int g_should_close = 0;
static int g_target_fps = 60;
static double g_frame_time = 0.0;
static uint64_t g_perf_freq = 0;
static uint64_t g_frame_start_counter = 0;
static uint64_t g_app_start_counter = 0;

/* ── stable key codes — MUST match src/h2d_keys.h#'s key_*() functions
 *    one-for-one; if you add a key here, add it there too. ──────────── */
enum {
    H2D_KEY_A = 0, H2D_KEY_B, H2D_KEY_C, H2D_KEY_D, H2D_KEY_E, H2D_KEY_F,
    H2D_KEY_G, H2D_KEY_H, H2D_KEY_I, H2D_KEY_J, H2D_KEY_K, H2D_KEY_L,
    H2D_KEY_M, H2D_KEY_N, H2D_KEY_O, H2D_KEY_P, H2D_KEY_Q, H2D_KEY_R,
    H2D_KEY_S, H2D_KEY_T, H2D_KEY_U, H2D_KEY_V, H2D_KEY_W, H2D_KEY_X,
    H2D_KEY_Y, H2D_KEY_Z,
    H2D_KEY_0, H2D_KEY_1, H2D_KEY_2, H2D_KEY_3, H2D_KEY_4,
    H2D_KEY_5, H2D_KEY_6, H2D_KEY_7, H2D_KEY_8, H2D_KEY_9,
    H2D_KEY_UP, H2D_KEY_DOWN, H2D_KEY_LEFT, H2D_KEY_RIGHT,
    H2D_KEY_SPACE, H2D_KEY_ENTER, H2D_KEY_ESCAPE, H2D_KEY_TAB,
    H2D_KEY_LSHIFT, H2D_KEY_RSHIFT, H2D_KEY_LCTRL, H2D_KEY_RCTRL,
    H2D_KEY_BACKSPACE,
    H2D_KEY_COUNT
};

static SDL_Scancode h2d_key_to_scancode(int key) {
    switch (key) {
        case H2D_KEY_A: return SDL_SCANCODE_A;
        case H2D_KEY_B: return SDL_SCANCODE_B;
        case H2D_KEY_C: return SDL_SCANCODE_C;
        case H2D_KEY_D: return SDL_SCANCODE_D;
        case H2D_KEY_E: return SDL_SCANCODE_E;
        case H2D_KEY_F: return SDL_SCANCODE_F;
        case H2D_KEY_G: return SDL_SCANCODE_G;
        case H2D_KEY_H: return SDL_SCANCODE_H;
        case H2D_KEY_I: return SDL_SCANCODE_I;
        case H2D_KEY_J: return SDL_SCANCODE_J;
        case H2D_KEY_K: return SDL_SCANCODE_K;
        case H2D_KEY_L: return SDL_SCANCODE_L;
        case H2D_KEY_M: return SDL_SCANCODE_M;
        case H2D_KEY_N: return SDL_SCANCODE_N;
        case H2D_KEY_O: return SDL_SCANCODE_O;
        case H2D_KEY_P: return SDL_SCANCODE_P;
        case H2D_KEY_Q: return SDL_SCANCODE_Q;
        case H2D_KEY_R: return SDL_SCANCODE_R;
        case H2D_KEY_S: return SDL_SCANCODE_S;
        case H2D_KEY_T: return SDL_SCANCODE_T;
        case H2D_KEY_U: return SDL_SCANCODE_U;
        case H2D_KEY_V: return SDL_SCANCODE_V;
        case H2D_KEY_W: return SDL_SCANCODE_W;
        case H2D_KEY_X: return SDL_SCANCODE_X;
        case H2D_KEY_Y: return SDL_SCANCODE_Y;
        case H2D_KEY_Z: return SDL_SCANCODE_Z;
        case H2D_KEY_0: return SDL_SCANCODE_0;
        case H2D_KEY_1: return SDL_SCANCODE_1;
        case H2D_KEY_2: return SDL_SCANCODE_2;
        case H2D_KEY_3: return SDL_SCANCODE_3;
        case H2D_KEY_4: return SDL_SCANCODE_4;
        case H2D_KEY_5: return SDL_SCANCODE_5;
        case H2D_KEY_6: return SDL_SCANCODE_6;
        case H2D_KEY_7: return SDL_SCANCODE_7;
        case H2D_KEY_8: return SDL_SCANCODE_8;
        case H2D_KEY_9: return SDL_SCANCODE_9;
        case H2D_KEY_UP: return SDL_SCANCODE_UP;
        case H2D_KEY_DOWN: return SDL_SCANCODE_DOWN;
        case H2D_KEY_LEFT: return SDL_SCANCODE_LEFT;
        case H2D_KEY_RIGHT: return SDL_SCANCODE_RIGHT;
        case H2D_KEY_SPACE: return SDL_SCANCODE_SPACE;
        case H2D_KEY_ENTER: return SDL_SCANCODE_RETURN;
        case H2D_KEY_ESCAPE: return SDL_SCANCODE_ESCAPE;
        case H2D_KEY_TAB: return SDL_SCANCODE_TAB;
        case H2D_KEY_LSHIFT: return SDL_SCANCODE_LSHIFT;
        case H2D_KEY_RSHIFT: return SDL_SCANCODE_RSHIFT;
        case H2D_KEY_LCTRL: return SDL_SCANCODE_LCTRL;
        case H2D_KEY_RCTRL: return SDL_SCANCODE_RCTRL;
        case H2D_KEY_BACKSPACE: return SDL_SCANCODE_BACKSPACE;
        default: return SDL_SCANCODE_UNKNOWN;
    }
}

/* ── window / frame lifecycle ──────────────────────────────────────── */

int64_t h2d_init_window(int64_t width, int64_t height, const char *title) {
    if (g_window != NULL) return 1; /* already open */

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        return 0;
    }
    /* Audio is opt-in via h2d_audio_init() — some games are silent /
     * some sandboxes have no audio device, and SDL_Init(AUDIO) failing
     * there shouldn't block opening a window. */

    g_window = SDL_CreateWindow(
        title ? title : "H2D",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        (int)width, (int)height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!g_window) return 0;

    g_renderer = SDL_CreateRenderer(
        g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!g_renderer) {
        /* Fall back to a software renderer rather than failing outright —
         * headless/CI/remote-desktop environments often have no GPU. */
        g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!g_renderer) {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
        return 0;
    }

    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);

    memset(g_textures, 0, sizeof(g_textures));
    memset(g_sounds, 0, sizeof(g_sounds));
    memset(g_music, 0, sizeof(g_music));
    memset(g_keys_now, 0, sizeof(g_keys_now));
    memset(g_keys_prev, 0, sizeof(g_keys_prev));

    g_perf_freq = SDL_GetPerformanceFrequency();
    g_app_start_counter = SDL_GetPerformanceCounter();
    g_frame_start_counter = g_app_start_counter;
    g_should_close = 0;

    return 1;
}

int64_t h2d_window_should_close(void) {
    return g_should_close;
}

void h2d_close_window(void) {
    for (int i = 0; i < H2D_MAX_TEXTURES; i++) {
        if (g_textures[i]) { SDL_DestroyTexture(g_textures[i]); g_textures[i] = NULL; }
    }
    for (int i = 0; i < H2D_MAX_SOUNDS; i++) {
        if (g_sounds[i]) { Mix_FreeChunk(g_sounds[i]); g_sounds[i] = NULL; }
    }
    for (int i = 0; i < H2D_MAX_MUSIC; i++) {
        if (g_music[i]) { Mix_FreeMusic(g_music[i]); g_music[i] = NULL; }
    }
    if (g_audio_ready) { Mix_CloseAudio(); g_audio_ready = 0; }
    if (g_renderer) { SDL_DestroyRenderer(g_renderer); g_renderer = NULL; }
    if (g_window) { SDL_DestroyWindow(g_window); g_window = NULL; }
    SDL_Quit();
}

void h2d_set_target_fps(int64_t fps) {
    g_target_fps = (int)fps;
}

/* Poll OS events, refresh keyboard/mouse snapshots. Call once at the
 * very start of every frame, before reading any input or drawing. */
void h2d_begin_frame(void) {
    memcpy(g_keys_prev, g_keys_now, sizeof(g_keys_now));
    g_mouse_buttons_prev = g_mouse_buttons_now;
    g_mouse_wheel = 0.0;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_QUIT:
                g_should_close = 1;
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                SDL_Scancode sc = ev.key.keysym.scancode;
                int down = (ev.type == SDL_KEYDOWN) ? 1 : 0;
                for (int k = 0; k < H2D_KEY_COUNT; k++) {
                    if (h2d_key_to_scancode(k) == sc) { g_keys_now[k] = (uint8_t)down; break; }
                }
                break;
            }
            case SDL_MOUSEMOTION:
                g_mouse_x = ev.motion.x;
                g_mouse_y = ev.motion.y;
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                int mask = 0;
                if (ev.button.button == SDL_BUTTON_LEFT) mask = 1;
                else if (ev.button.button == SDL_BUTTON_MIDDLE) mask = 2;
                else if (ev.button.button == SDL_BUTTON_RIGHT) mask = 4;
                if (ev.type == SDL_MOUSEBUTTONDOWN) g_mouse_buttons_now |= mask;
                else g_mouse_buttons_now &= ~mask;
                break;
            }
            case SDL_MOUSEWHEEL:
                g_mouse_wheel = (double)ev.wheel.y;
                break;
            default:
                break;
        }
    }
}

/* Present the frame and, if vsync isn't doing the job (software
 * renderer fallback, uncapped display), sleep off the rest of the
 * frame budget implied by h2d_set_target_fps. Also updates
 * h2d_get_frame_time()'s delta. Call once at the very end of every
 * frame, after all drawing. */
void h2d_end_frame(void) {
    SDL_RenderPresent(g_renderer);

    uint64_t now = SDL_GetPerformanceCounter();
    double elapsed = (double)(now - g_frame_start_counter) / (double)g_perf_freq;

    if (g_target_fps > 0) {
        double budget = 1.0 / (double)g_target_fps;
        if (elapsed < budget) {
            SDL_Delay((Uint32)((budget - elapsed) * 1000.0));
            now = SDL_GetPerformanceCounter();
            elapsed = (double)(now - g_frame_start_counter) / (double)g_perf_freq;
        }
    }

    g_frame_time = elapsed;
    g_frame_start_counter = now;
}

double h2d_get_frame_time(void) { return g_frame_time; }

double h2d_get_time(void) {
    uint64_t now = SDL_GetPerformanceCounter();
    return (double)(now - g_app_start_counter) / (double)g_perf_freq;
}

int64_t h2d_screen_width(void) {
    int w = 0, h = 0;
    if (g_window) SDL_GetWindowSize(g_window, &w, &h);
    return w;
}

int64_t h2d_screen_height(void) {
    int w = 0, h = 0;
    if (g_window) SDL_GetWindowSize(g_window, &w, &h);
    return h;
}

void h2d_set_window_title(const char *title) {
    if (g_window) SDL_SetWindowTitle(g_window, title ? title : "");
}

/* ── drawing primitives (0-255 rgba throughout) ────────────────────── */

void h2d_clear_background(int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer) return;
    SDL_SetRenderDrawColor(g_renderer, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
    SDL_RenderClear(g_renderer);
}

void h2d_draw_rectangle(double x, double y, double w, double h, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer) return;
    boxRGBA(g_renderer, (Sint16)x, (Sint16)y, (Sint16)(x + w), (Sint16)(y + h),
            (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
}

void h2d_draw_rectangle_lines(double x, double y, double w, double h, double thick, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer) return;
    int t = (int)(thick < 1.0 ? 1.0 : thick);
    for (int i = 0; i < t; i++) {
        rectangleRGBA(g_renderer, (Sint16)(x - i), (Sint16)(y - i),
                      (Sint16)(x + w + i), (Sint16)(y + h + i),
                      (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
    }
}

void h2d_draw_circle(double cx, double cy, double radius, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer) return;
    filledCircleRGBA(g_renderer, (Sint16)cx, (Sint16)cy, (Sint16)radius, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
}

void h2d_draw_circle_lines(double cx, double cy, double radius, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer) return;
    aacircleRGBA(g_renderer, (Sint16)cx, (Sint16)cy, (Sint16)radius, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
}

void h2d_draw_line(double x1, double y1, double x2, double y2, double thick, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer) return;
    Uint8 t = (Uint8)(thick < 1.0 ? 1.0 : thick);
    thickLineRGBA(g_renderer, (Sint16)x1, (Sint16)y1, (Sint16)x2, (Sint16)y2,
                  t, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
}

void h2d_draw_triangle(double x1, double y1, double x2, double y2, double x3, double y3, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer) return;
    filledTrigonRGBA(g_renderer, (Sint16)x1, (Sint16)y1, (Sint16)x2, (Sint16)y2, (Sint16)x3, (Sint16)y3,
                      (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
}

/* Bitmap 8x8 font baked into SDL2_gfx — no font file needed. `size` is
 * an integer scale factor (8 = native 8px glyphs, 16 = 2x, ...). */
void h2d_draw_text(const char *text, double x, double y, int64_t size, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer || !text) return;
    int scale = (int)(size / 8);
    if (scale < 1) scale = 1;
    if (scale == 1) {
        stringRGBA(g_renderer, (Sint16)x, (Sint16)y, text, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
        return;
    }
    /* SDL2_gfx has no built-in scaled text; approximate by rendering to
     * an offscreen 8px-per-char texture and stretching it, so bigger
     * on-screen text stays crisp-ish without needing SDL_ttf. */
    int len = (int)strlen(text);
    int tw = len * 8, th = 8;
    if (tw <= 0) return;
    SDL_Texture *tmp = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET, tw, th);
    if (!tmp) { stringRGBA(g_renderer, (Sint16)x, (Sint16)y, text, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a); return; }
    SDL_SetTextureBlendMode(tmp, SDL_BLENDMODE_BLEND);
    SDL_Texture *prev_target = SDL_GetRenderTarget(g_renderer);
    SDL_SetRenderTarget(g_renderer, tmp);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 0);
    SDL_RenderClear(g_renderer);
    stringRGBA(g_renderer, 0, 0, text, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
    SDL_SetRenderTarget(g_renderer, prev_target);
    SDL_Rect dst = { (int)x, (int)y, tw * scale, th * scale };
    SDL_RenderCopy(g_renderer, tmp, NULL, &dst);
    SDL_DestroyTexture(tmp);
}

/* Rough on-screen width in pixels for `text` at `size` — lets H# code
 * center/right-align text without needing real font metrics. */
int64_t h2d_measure_text(const char *text, int64_t size) {
    if (!text) return 0;
    int scale = (int)(size / 8);
    if (scale < 1) scale = 1;
    return (int64_t)strlen(text) * 8 * scale;
}

/* ── textures ───────────────────────────────────────────────────────── */

static int h2d_texture_alloc_slot(void) {
    for (int tries = 0; tries < H2D_MAX_TEXTURES; tries++) {
        int slot = g_texture_next;
        g_texture_next++;
        if (g_texture_next >= H2D_MAX_TEXTURES) g_texture_next = 1;
        if (g_textures[slot] == NULL) return slot;
    }
    return 0;
}

/* Loads any format SDL_image supports (PNG, JPG, BMP, GIF, ...) — a
 * real upgrade over H#'s std -> image, which only decodes uncompressed
 * 24-bit BMP. Returns 0 on failure. */
int64_t h2d_load_texture(const char *path) {
    if (!g_renderer || !path) return 0;
    SDL_Surface *surf = IMG_Load(path);
    if (!surf) return 0;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(g_renderer, surf);
    int w = surf->w, h = surf->h;
    SDL_FreeSurface(surf);
    if (!tex) return 0;
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    int slot = h2d_texture_alloc_slot();
    if (slot == 0) { SDL_DestroyTexture(tex); return 0; }
    g_textures[slot] = tex;
    g_texture_w[slot] = w;
    g_texture_h[slot] = h;
    return slot;
}

/* Builds a texture directly from a raw RGBA8888 buffer (top-down, 4
 * bytes/pixel) — the escape hatch for procedurally generated pixel
 * data, or for feeding in an H# `std -> image` `Image.pixels` buffer
 * after expanding it from RGB to RGBA yourself. `pixels` must contain
 * exactly width*height*4 bytes; `pixels` is H#'s `bytes` type, which
 * the compiler maps to `uint8_t*` (see the ABI note at the top of this
 * file), not `int64_t`, so it is NOT one of the width/height params. */
int64_t h2d_load_texture_from_rgba(uint8_t *pixels, int64_t width, int64_t height) {
    if (!g_renderer || !pixels || width <= 0 || height <= 0) return 0;
    int w = (int)width, h = (int)height;
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormatFrom(
        (void *)pixels, w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA8888
    );
    if (!surf) return 0;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(g_renderer, surf);
    SDL_FreeSurface(surf);
    if (!tex) return 0;
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    int slot = h2d_texture_alloc_slot();
    if (slot == 0) { SDL_DestroyTexture(tex); return 0; }
    g_textures[slot] = tex;
    g_texture_w[slot] = w;
    g_texture_h[slot] = h;
    return slot;
}

void h2d_unload_texture(int64_t handle) {
    int h = (int)handle;
    if (h <= 0 || h >= H2D_MAX_TEXTURES || !g_textures[h]) return;
    SDL_DestroyTexture(g_textures[h]);
    g_textures[h] = NULL;
}

int64_t h2d_texture_width(int64_t handle) {
    int h = (int)handle;
    if (h <= 0 || h >= H2D_MAX_TEXTURES) return 0;
    return g_texture_w[h];
}

int64_t h2d_texture_height(int64_t handle) {
    int h = (int)handle;
    if (h <= 0 || h >= H2D_MAX_TEXTURES) return 0;
    return g_texture_h[h];
}

void h2d_draw_texture(int64_t handle, double x, double y, int64_t r, int64_t g, int64_t b, int64_t a) {
    int h = (int)handle;
    if (!g_renderer || h <= 0 || h >= H2D_MAX_TEXTURES || !g_textures[h]) return;
    SDL_Texture *tex = g_textures[h];
    SDL_SetTextureColorMod(tex, (Uint8)r, (Uint8)g, (Uint8)b);
    SDL_SetTextureAlphaMod(tex, (Uint8)a);
    SDL_Rect dst = { (int)x, (int)y, g_texture_w[h], g_texture_h[h] };
    SDL_RenderCopy(g_renderer, tex, NULL, &dst);
}

/* Full transform draw: destination size (dst_w/dst_h — pass the
 * texture's own width/height for 1:1 scale), rotation in degrees
 * around (origin_x, origin_y) measured from the destination
 * top-left, optional horizontal/vertical flip (0/1), and an RGBA
 * tint. This is the GPU path (SDL_RenderCopyEx) — rotation/scale cost
 * nothing extra over a plain blit on any hardware-accelerated
 * renderer. */
void h2d_draw_texture_ex(int64_t handle, double x, double y, double dst_w, double dst_h,
                          double rotation_deg, double origin_x, double origin_y,
                          int64_t flip_h, int64_t flip_v, int64_t r, int64_t g, int64_t b, int64_t a) {
    int h = (int)handle;
    if (!g_renderer || h <= 0 || h >= H2D_MAX_TEXTURES || !g_textures[h]) return;
    SDL_Texture *tex = g_textures[h];
    SDL_SetTextureColorMod(tex, (Uint8)r, (Uint8)g, (Uint8)b);
    SDL_SetTextureAlphaMod(tex, (Uint8)a);
    SDL_Rect dst = { (int)x, (int)y, (int)dst_w, (int)dst_h };
    SDL_Point center = { (int)origin_x, (int)origin_y };
    SDL_RendererFlip flip = SDL_FLIP_NONE;
    if (flip_h) flip |= SDL_FLIP_HORIZONTAL;
    if (flip_v) flip |= SDL_FLIP_VERTICAL;
    SDL_RenderCopyEx(g_renderer, tex, NULL, &dst, rotation_deg, &center, flip);
}

/* Draws one sub-rectangle (src_x, src_y, src_w, src_h) of the texture,
 * stretched to (dst_w, dst_h) at (x, y) — the sprite-sheet / texture
 * atlas primitive that h2d_anim2d builds frame-by-frame animation on
 * top of. */
void h2d_draw_texture_rec(int64_t handle, double src_x, double src_y, double src_w, double src_h,
                           double x, double y, double dst_w, double dst_h,
                           int64_t r, int64_t g, int64_t b, int64_t a) {
    int h = (int)handle;
    if (!g_renderer || h <= 0 || h >= H2D_MAX_TEXTURES || !g_textures[h]) return;
    SDL_Texture *tex = g_textures[h];
    SDL_SetTextureColorMod(tex, (Uint8)r, (Uint8)g, (Uint8)b);
    SDL_SetTextureAlphaMod(tex, (Uint8)a);
    SDL_Rect src = { (int)src_x, (int)src_y, (int)src_w, (int)src_h };
    SDL_Rect dst = { (int)x, (int)y, (int)dst_w, (int)dst_h };
    SDL_RenderCopy(g_renderer, tex, &src, &dst);
}

/* ── input ──────────────────────────────────────────────────────────── */
/* All "is it true" queries return an H#-`int` 0/1 rather than a `bool`,
 * to keep the ABI to exactly two integer widths (see the note up top). */

int64_t h2d_is_key_down(int64_t key) {
    int k = (int)key;
    if (k < 0 || k >= H2D_KEY_COUNT) return 0;
    return g_keys_now[k];
}

int64_t h2d_is_key_pressed(int64_t key) {
    int k = (int)key;
    if (k < 0 || k >= H2D_KEY_COUNT) return 0;
    return (g_keys_now[k] && !g_keys_prev[k]) ? 1 : 0;
}

int64_t h2d_is_key_released(int64_t key) {
    int k = (int)key;
    if (k < 0 || k >= H2D_KEY_COUNT) return 0;
    return (!g_keys_now[k] && g_keys_prev[k]) ? 1 : 0;
}

double h2d_mouse_x(void) { return (double)g_mouse_x; }
double h2d_mouse_y(void) { return (double)g_mouse_y; }
double h2d_mouse_wheel(void) { return g_mouse_wheel; }

/* button: 0=left 1=middle 2=right */
int64_t h2d_is_mouse_button_down(int64_t button) {
    int mask = (button == 0) ? 1 : (button == 1) ? 2 : (button == 2) ? 4 : 0;
    return (g_mouse_buttons_now & mask) ? 1 : 0;
}

int64_t h2d_is_mouse_button_pressed(int64_t button) {
    int mask = (button == 0) ? 1 : (button == 1) ? 2 : (button == 2) ? 4 : 0;
    return ((g_mouse_buttons_now & mask) && !(g_mouse_buttons_prev & mask)) ? 1 : 0;
}

/* ── audio ──────────────────────────────────────────────────────────── */

int64_t h2d_audio_init(void) {
    if (g_audio_ready) return 1;
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return 0;
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) return 0;
    Mix_AllocateChannels(32);
    g_audio_ready = 1;
    return 1;
}

int64_t h2d_load_sound(const char *path) {
    if (!g_audio_ready || !path) return 0;
    Mix_Chunk *chunk = Mix_LoadWAV(path);
    if (!chunk) return 0;
    if (g_sound_next >= H2D_MAX_SOUNDS) g_sound_next = 1;
    int slot = g_sound_next++;
    if (g_sounds[slot]) Mix_FreeChunk(g_sounds[slot]);
    g_sounds[slot] = chunk;
    return slot;
}

void h2d_play_sound(int64_t handle) {
    int h = (int)handle;
    if (h <= 0 || h >= H2D_MAX_SOUNDS || !g_sounds[h]) return;
    Mix_PlayChannel(-1, g_sounds[h], 0);
}

void h2d_play_sound_loop(int64_t handle) {
    int h = (int)handle;
    if (h <= 0 || h >= H2D_MAX_SOUNDS || !g_sounds[h]) return;
    Mix_PlayChannel(-1, g_sounds[h], -1);
}

void h2d_set_sound_volume(int64_t handle, double volume) {
    int h = (int)handle;
    if (h <= 0 || h >= H2D_MAX_SOUNDS || !g_sounds[h]) return;
    int v = (int)(volume * MIX_MAX_VOLUME);
    if (v < 0) v = 0;
    if (v > MIX_MAX_VOLUME) v = MIX_MAX_VOLUME;
    Mix_VolumeChunk(g_sounds[h], v);
}

void h2d_stop_all_sounds(void) {
    if (!g_audio_ready) return;
    Mix_HaltChannel(-1);
}

int64_t h2d_load_music(const char *path) {
    if (!g_audio_ready || !path) return 0;
    Mix_Music *music = Mix_LoadMUS(path);
    if (!music) return 0;
    if (g_music_next >= H2D_MAX_MUSIC) g_music_next = 1;
    int slot = g_music_next++;
    if (g_music[slot]) Mix_FreeMusic(g_music[slot]);
    g_music[slot] = music;
    return slot;
}

void h2d_play_music(int64_t handle, int64_t loop) {
    int h = (int)handle;
    if (h <= 0 || h >= H2D_MAX_MUSIC || !g_music[h]) return;
    Mix_PlayMusic(g_music[h], loop ? -1 : 1);
}

void h2d_stop_music(void) {
    if (!g_audio_ready) return;
    Mix_HaltMusic();
}

void h2d_set_music_volume(double volume) {
    if (!g_audio_ready) return;
    int v = (int)(volume * MIX_MAX_VOLUME);
    if (v < 0) v = 0;
    if (v > MIX_MAX_VOLUME) v = MIX_MAX_VOLUME;
    Mix_VolumeMusic(v);
}
