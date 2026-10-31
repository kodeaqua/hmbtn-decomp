#ifndef HMBTN_COMPAT_LIBGPU_H
#define HMBTN_COMPAT_LIBGPU_H

#include "compat/types.h"

/* Packet fields follow public GP0 command formats and src/ field use.
 * The first word is a PS1 DMA tag, never a truncated host pointer.
 * No OT traversal or rasterizer is provided by these declarations. */
typedef struct { s16 x, y, w, h; } RECT;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0, x1, y1, x2, y2;
} POLY_F3;
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0, x1, y1, x2, y2, x3, y3;
} POLY_F4;
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad2;
} POLY_FT3;
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad2;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad3;
} POLY_FT4;
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
} POLY_G3;
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
    u8 r3, g3, b3, pad3;
    s16 x3, y3;
} POLY_G4;
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad3;
} POLY_GT3;
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad3;
    u8 r3, g3, b3, pad4;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad5;
} POLY_GT4;
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 w, h;
} SPRT;
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0, w, h;
} TILE;
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0, x1, y1;
} LINE_F2;

typedef struct { u32 tag, code[1]; } DR_TPAGE;
typedef struct { u32 tag, code[2]; } DR_MODE;
typedef struct { u32 tag, code[2]; } DR_AREA;
/* clut.c reserves 0x44 bytes for this inline transfer packet. */
typedef struct { u32 tag, code[3], p[13]; } DR_LOAD;
typedef struct { u32 tag, code[15]; } DR_ENV;

/* Host-side environments have no pointers and retain the game's field layout. */
typedef struct {
    RECT clip;
    s16 ofs[2];
    RECT tw;
    u16 tpage;
    u8 dtd, dfe, isbg, r0, g0, b0;
    DR_ENV dr_env;
} DRAWENV;
typedef struct {
    RECT disp, screen;
    u8 isinter, isrgb24, pad0, pad1;
} DISPENV;

#define HM_GPU_SIZE(type, bytes) _Static_assert(sizeof(type) == (bytes), #type " packet size")
HM_GPU_SIZE(RECT, 8);
HM_GPU_SIZE(POLY_F3, 20);
HM_GPU_SIZE(POLY_F4, 24);
HM_GPU_SIZE(POLY_FT3, 32);
HM_GPU_SIZE(POLY_FT4, 40);
HM_GPU_SIZE(POLY_G3, 28);
HM_GPU_SIZE(POLY_G4, 36);
HM_GPU_SIZE(POLY_GT3, 40);
HM_GPU_SIZE(POLY_GT4, 52);
HM_GPU_SIZE(SPRT, 20);
HM_GPU_SIZE(TILE, 16);
HM_GPU_SIZE(LINE_F2, 16);
HM_GPU_SIZE(DR_TPAGE, 8);
HM_GPU_SIZE(DR_MODE, 12);
HM_GPU_SIZE(DR_AREA, 12);
HM_GPU_SIZE(DR_LOAD, 68);
HM_GPU_SIZE(DRAWENV, 92);
HM_GPU_SIZE(DISPENV, 20);
#undef HM_GPU_SIZE

#define HM_GPU_OFFSET(type, field, bytes)                                                            \
    _Static_assert(__builtin_offsetof(type, field) == (bytes), #type "." #field " offset")
HM_GPU_OFFSET(POLY_F3, code, 7);
HM_GPU_OFFSET(POLY_F3, x2, 16);
HM_GPU_OFFSET(POLY_F4, x3, 20);
HM_GPU_OFFSET(POLY_FT3, clut, 14);
HM_GPU_OFFSET(POLY_FT3, tpage, 22);
HM_GPU_OFFSET(POLY_FT4, x3, 32);
HM_GPU_OFFSET(POLY_G3, r1, 12);
HM_GPU_OFFSET(POLY_G4, x3, 32);
HM_GPU_OFFSET(POLY_GT3, tpage, 26);
HM_GPU_OFFSET(POLY_GT4, x3, 44);
HM_GPU_OFFSET(SPRT, w, 16);
HM_GPU_OFFSET(TILE, w, 12);
HM_GPU_OFFSET(LINE_F2, x1, 12);
HM_GPU_OFFSET(DR_TPAGE, code, 4);
HM_GPU_OFFSET(DR_MODE, code, 4);
HM_GPU_OFFSET(DR_AREA, code, 4);
HM_GPU_OFFSET(DR_LOAD, p, 16);
HM_GPU_OFFSET(DRAWENV, ofs, 8);
HM_GPU_OFFSET(DRAWENV, tw, 12);
HM_GPU_OFFSET(DRAWENV, tpage, 20);
HM_GPU_OFFSET(DRAWENV, dr_env, 28);
HM_GPU_OFFSET(DISPENV, screen, 8);
HM_GPU_OFFSET(DISPENV, isinter, 16);
#undef HM_GPU_OFFSET

s32 DrawSync(s32 mode);
s32 ResetGraph(s32 mode);
s32 SetGraphDebug(s32 mode);
DRAWENV *SetDefDrawEnv(DRAWENV *environment, s32 x, s32 y, s32 width, s32 height);
DISPENV *SetDefDispEnv(DISPENV *environment, s32 x, s32 y, s32 width, s32 height);
DRAWENV *PutDrawEnv(DRAWENV *environment);
DISPENV *PutDispEnv(DISPENV *environment);
u32 *ClearOTagR(u32 *table, s32 entries);
void setlen(void *packet, u8 words);
void addPrim(u32 *head, void *packet);
void AddPrim(u32 *head, void *packet);
void DrawOTag(u32 *table);

enum { HWD0 = 320, VWD0 = 240 };
u16 getClut(s32 x, s32 y);
u16 GetClut(s32 x, s32 y);
u16 getTPage(s32 depth, s32 blend, s32 x, s32 y);
u16 GetTPage(s32 depth, s32 blend, s32 x, s32 y);
void setSprt(SPRT *packet);
void setPolyFT4(void *packet);
void SetPolyF4(POLY_F4 *packet);
void setSemiTrans(void *packet, s32 enabled);
void SetSemiTrans(void *packet, s32 enabled);
void setShadeTex(void *packet, s32 raw_texture);
void setDrawTPage(void *packet, s32 display_enable, s32 dither, s32 page);
void SetDrawMode(DR_MODE *packet, s32 display_enable, s32 dither, s32 page, const RECT *window);
void SetDrawArea(DR_AREA *packet, const RECT *area);
void SetDrawLoad(DR_LOAD *packet, const RECT *area);
s32 LoadImage(const RECT *area, const u32 *pixels);
s32 StoreImage(const RECT *area, u32 *pixels);
s32 MoveImage(const RECT *area, s32 x, s32 y);
s32 MoveImage2(const RECT *area, s32 x, s32 y);
s32 ClearImage(const RECT *area, u8 r, u8 g, u8 b);
s32 SetDispMask(s32 enabled);
s32 FntPrint(const char *format, ...);
void FntLoad(s32 x, s32 y);
s32 FntOpen(s32 x, s32 y, s32 width, s32 height, s32 background, s32 characters);
void FntFlush(s32 stream);
void SetDumpFnt(s32 stream);
void KanjiFntClose(void);
void KanjiFntOpen(s32 x, s32 y, s32 width, s32 height, s32 texture_x, s32 texture_y,
                  s32 clut_x, s32 red, s32 green, s32 blue);
s32 KanjiFntPrint(const char *format, ...);
void KanjiFntFlush(s32 stream);

/* Source call sites pass several primitive types; field macros retain their
 * concrete types and evaluate only the requested primitive attributes. */
#define setXY0(p, x, y) ((p)->x0 = (x), (p)->y0 = (y))
#define setWH(p, width, height) ((p)->w = (width), (p)->h = (height))
#define setUV0(p, u, v) ((p)->u0 = (u), (p)->v0 = (v))
#define setRGB0(p, r, g, b) ((p)->r0 = (r), (p)->g0 = (g), (p)->b0 = (b))
#define setXYWH(p, x, y, width, height) \
    ((p)->x0 = (x), (p)->y0 = (y), (p)->x1 = (x) + (width), (p)->y1 = (y), \
     (p)->x2 = (x), (p)->y2 = (y) + (height), \
     (p)->x3 = (x) + (width), (p)->y3 = (y) + (height))
#define setUVWH(p, u, v, width, height) \
    ((p)->u0 = (u), (p)->v0 = (v), (p)->u1 = (u) + (width), (p)->v1 = (v), \
     (p)->u2 = (u), (p)->v2 = (v) + (height), \
     (p)->u3 = (u) + (width), (p)->v3 = (v) + (height))

#endif
