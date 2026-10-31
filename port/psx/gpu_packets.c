#include <libgpu.h>

#include "psx/error.h"

#include <string.h>

u16 getClut(s32 x, s32 y) {
    return (u16)((((u32)y & 0x1FF) << 6) | (((u32)x >> 4) & 0x3F));
}

u16 GetClut(s32 x, s32 y) { return getClut(x, y); }

u16 getTPage(s32 depth, s32 blend, s32 x, s32 y) {
    return (u16)(((depth & 3) << 7) | ((blend & 3) << 5)
                 | (((u32)y >> 4) & 16) | (((u32)x >> 6) & 15));
}

u16 GetTPage(s32 depth, s32 blend, s32 x, s32 y) { return getTPage(depth, blend, x, y); }

static void hm_gpu_packet_type(void *packet, u8 words, u8 command) {
    setlen(packet, words);
    ((u8 *)packet)[7] = command;
}

void setSprt(SPRT *packet) { hm_gpu_packet_type(packet, 4, 0x64); }
void setPolyFT4(void *packet) { hm_gpu_packet_type(packet, 9, 0x2C); }
void SetPolyF4(POLY_F4 *packet) { hm_gpu_packet_type(packet, 5, 0x28); }

void setSemiTrans(void *packet, s32 enabled) {
    u8 *command = (u8 *)packet + 7;
    *command = (*command & (u8)~2) | (enabled ? 2 : 0);
}

void SetSemiTrans(void *packet, s32 enabled) { setSemiTrans(packet, enabled); }

void setShadeTex(void *packet, s32 raw_texture) {
    u8 *command = (u8 *)packet + 7;
    *command = (*command & (u8)~1) | (raw_texture ? 1 : 0);
}

static u32 hm_gpu_draw_mode(s32 display_enable, s32 dither, s32 page) {
    return 0xE1000000 | ((u32)page & 0x1FF) | (dither ? 0x200 : 0)
           | (display_enable ? 0x400 : 0);
}

void setDrawTPage(void *packet, s32 display_enable, s32 dither, s32 page) {
    u32 command = hm_gpu_draw_mode(display_enable, dither, page);
    setlen(packet, 1);
    memcpy((u8 *)packet + 4, &command, 4);
}

void SetDrawMode(DR_MODE *packet, s32 display_enable, s32 dither, s32 page, const RECT *window) {
    setlen(packet, 2);
    packet->code[0] = hm_gpu_draw_mode(display_enable, dither, page);
    u32 texture_window = 0;
    if (window != NULL) {
        u32 mask_x = (256 - (u32)window->w) & 0xF8;
        u32 mask_y = (256 - (u32)window->h) & 0xF8;
        texture_window = (mask_x >> 3) | (mask_y << 2)
                         | (((u32)window->x & mask_x) << 7)
                         | (((u32)window->y & mask_y) << 12);
    }
    packet->code[1] = 0xE2000000 | texture_window;
}

static u32 hm_gpu_area_xy(s32 x, s32 y) {
    return ((u32)x & 0x3FF) | (((u32)y & 0x1FF) << 10);
}

void SetDrawArea(DR_AREA *packet, const RECT *area) {
    setlen(packet, 2);
    packet->code[0] = 0xE3000000 | hm_gpu_area_xy(area->x, area->y);
    packet->code[1] = 0xE4000000 | hm_gpu_area_xy(area->x + area->w - 1, area->y + area->h - 1);
}

void SetDrawLoad(DR_LOAD *packet, const RECT *area) {
    s32 pixels = (s32)area->w * area->h;
    if (area->w <= 0 || area->h <= 0 || pixels > 26) {
        hm_runtime_error("SetDrawLoad", "inline transfer exceeds packet capacity");
    }
    setlen(packet, (u8)(3 + (pixels + 1) / 2));
    packet->code[0] = 0xA0000000;
    packet->code[1] = (u16)area->x | ((u32)(u16)area->y << 16);
    packet->code[2] = (u16)area->w | ((u32)(u16)area->h << 16);
}
