#include <stdio.h>
#include <string.h>

#include "graphics/ps1_framebuffer.h"

static u32 g_crc_table[256];
static u32 g_crc_cur;

static void crc_table_init(void) {
    u32 i, k;
    for (i = 0; i < 256; i++) {
        u32 c = i;
        for (k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        g_crc_table[i] = c;
    }
}

static void crc_start(void) { g_crc_cur = 0xFFFFFFFFu; }

static void crc_add(const u8* p, u32 n) {
    u32 i;
    for (i = 0; i < n; i++)
        g_crc_cur = g_crc_table[(g_crc_cur ^ p[i]) & 0xFFu] ^ (g_crc_cur >> 8);
}

static u32 crc_end(void) { return g_crc_cur ^ 0xFFFFFFFFu; }

static void be32(FILE* f, u32 v) {
    u8 b[4];
    b[0] = (u8)(v >> 24); b[1] = (u8)(v >> 16); b[2] = (u8)(v >> 8); b[3] = (u8)v;
    fwrite(b, 1, 4, f);
}

static void emit(FILE* f, const u8* p, u32 n) {
    fwrite(p, 1, n, f);
    crc_add(p, n);
}

void ps1_fb_clear(Ps1Framebuffer* fb, u16 color) {
    u32 i;
    for (i = 0; i < PS1_SCREEN_W * PS1_SCREEN_H; i++) fb->px[i] = color;
}

void ps1_fb_put(Ps1Framebuffer* fb, int x, int y, u16 color) {
    if (x < 0 || y < 0 || x >= PS1_SCREEN_W || y >= PS1_SCREEN_H) return;
    fb->px[y * PS1_SCREEN_W + x] = color;
}

u16 ps1_fb_get(const Ps1Framebuffer* fb, int x, int y) {
    if (x < 0 || y < 0 || x >= PS1_SCREEN_W || y >= PS1_SCREEN_H) return 0;
    return fb->px[y * PS1_SCREEN_W + x];
}

static void png_chunk(FILE* f, const char* type, const u8* data, u32 len) {
    u8 t[4];
    t[0] = (u8)type[0]; t[1] = (u8)type[1]; t[2] = (u8)type[2]; t[3] = (u8)type[3];
    be32(f, len);
    crc_start();
    emit(f, t, 4);
    if (len) emit(f, data, len);
    be32(f, crc_end());
}

int ps1_fb_export_png(const Ps1Framebuffer* fb, const char* path) {
    static u8 raw[PS1_SCREEN_H * (1 + PS1_SCREEN_W * 3)];
    static u8 ihdr[13];
    u32 raw_len = PS1_SCREEN_H * (1u + PS1_SCREEN_W * 3u);
    u32 nblocks = (raw_len + 65534u) / 65535u;
    u32 a = 1, b = 0;
    u32 p = 0;
    int y, x;
    FILE* f;
    static u8 sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};

    crc_table_init();

    for (y = 0; y < PS1_SCREEN_H; y++) {
        raw[p++] = 0;
        b = (b + a) % 65521u;
        for (x = 0; x < PS1_SCREEN_W; x++) {
            u16 c = fb->px[y * PS1_SCREEN_W + x];
            u8 r = (u8)(((c >> 10) & 31) * 255u / 31u);
            u8 g = (u8)(((c >> 5) & 31) * 255u / 31u);
            u8 bl = (u8)((c & 31) * 255u / 31u);
            raw[p++] = r; raw[p++] = g; raw[p++] = bl;
            a = (a + r) % 65521u;
            b = (b + a) % 65521u;
            a = (a + g) % 65521u;
            b = (b + a) % 65521u;
            a = (a + bl) % 65521u;
            b = (b + a) % 65521u;
        }
    }

    f = fopen(path, "wb");
    if (!f) return 0;

    fwrite(sig, 1, 8, f);

    ihdr[0] = (u8)(PS1_SCREEN_W >> 24); ihdr[1] = (u8)(PS1_SCREEN_W >> 16);
    ihdr[2] = (u8)(PS1_SCREEN_W >> 8); ihdr[3] = (u8)PS1_SCREEN_W;
    ihdr[4] = (u8)(PS1_SCREEN_H >> 24); ihdr[5] = (u8)(PS1_SCREEN_H >> 16);
    ihdr[6] = (u8)(PS1_SCREEN_H >> 8); ihdr[7] = (u8)PS1_SCREEN_H;
    ihdr[8] = 8;
    ihdr[9] = 2;
    ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    png_chunk(f, "IHDR", ihdr, 13);

    {
        u32 idat_len = raw_len + nblocks * 5u + 6u;
        u8 hdr4[4];
        u8 zhdr[2] = {0x78, 0x9C};
        hdr4[0] = (u8)('I'); hdr4[1] = (u8)('D'); hdr4[2] = (u8)('A'); hdr4[3] = (u8)('T');
        be32(f, idat_len);
        crc_start();
        emit(f, hdr4, 4);
        emit(f, zhdr, 2);
        {
            u32 off = 0;
            u32 block = 0;
            while (off < raw_len) {
                u32 n = raw_len - off;
                u8 bh;
                if (n > 65535u) n = 65535u;
                bh = (u8)(((off + n) >= raw_len) ? 1 : 0);
                {
                    u8 hb[5];
                    hb[0] = bh;
                    hb[1] = (u8)(n & 0xFF); hb[2] = (u8)((n >> 8) & 0xFF);
                    hb[3] = (u8)(~(n & 0xFF)); hb[4] = (u8)(~((n >> 8) & 0xFF));
                    emit(f, hb, 5);
                }
                emit(f, raw + off, n);
                off += n;
                block++;
            }
            (void)block;
        }
        {
            u32 adval = (b << 16) | a;
            u8 ad[4];
            ad[0] = (u8)(adval >> 24); ad[1] = (u8)((adval >> 16) & 0xFF);
            ad[2] = (u8)((adval >> 8) & 0xFF); ad[3] = (u8)(adval & 0xFF);
            emit(f, ad, 4);
        }
        be32(f, crc_end());
    }

    png_chunk(f, "IEND", NULL, 0);

    fclose(f);
    return 1;
}

int ps1_fb_export_bmp(const Ps1Framebuffer* fb, const char* path) {
    FILE* f = fopen(path, "wb");
    u8 hdr[54];
    u32 row_bytes = PS1_SCREEN_W * 3;
    u32 pad = (4 - (row_bytes & 3)) & 3;
    u32 image_size = (row_bytes + pad) * PS1_SCREEN_H;
    int y, x;
    if (!f) return 0;

    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'B'; hdr[1] = 'M';
    hdr[2] = (u8)((54u + image_size) & 0xFF);
    hdr[3] = (u8)(((54u + image_size) >> 8) & 0xFF);
    hdr[4] = (u8)(((54u + image_size) >> 16) & 0xFF);
    hdr[5] = (u8)(((54u + image_size) >> 24) & 0xFF);
    hdr[10] = 54;
    hdr[14] = 40;
    hdr[18] = (u8)(PS1_SCREEN_W & 0xFF);
    hdr[19] = (u8)((PS1_SCREEN_W >> 8) & 0xFF);
    hdr[22] = (u8)(PS1_SCREEN_H & 0xFF);
    hdr[23] = (u8)((PS1_SCREEN_H >> 8) & 0xFF);
    hdr[26] = 1;
    hdr[28] = 24;
    hdr[34] = (u8)(image_size & 0xFF);
    hdr[35] = (u8)((image_size >> 8) & 0xFF);
    hdr[36] = (u8)((image_size >> 16) & 0xFF);
    hdr[37] = (u8)((image_size >> 24) & 0xFF);
    fwrite(hdr, 1, 54, f);

    for (y = PS1_SCREEN_H - 1; y >= 0; y--) {
        for (x = 0; x < PS1_SCREEN_W; x++) {
            u16 c = fb->px[y * PS1_SCREEN_W + x];
            u8 r = (u8)(((c >> 10) & 31) * 255u / 31u);
            u8 g = (u8)(((c >> 5) & 31) * 255u / 31u);
            u8 bl = (u8)((c & 31) * 255u / 31u);
            putc(bl, f); putc(g, f); putc(r, f);
        }
        for (x = 0; x < (int)pad; x++) putc(0, f);
    }
    fclose(f);
    return 1;
}

int ps1_fb_export_raw555(const Ps1Framebuffer* fb, const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    fwrite(fb->px, 2, PS1_SCREEN_W * PS1_SCREEN_H, f);
    fclose(f);
    return 1;
}