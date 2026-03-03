/*
 * png_to_csource.c — Convert a 240x240 PNG to a GIMP-compatible RGB565
 *                    C-source file, replacing the manual GIMP export workflow.
 *
 * Build:  gcc -o png_to_csource rp/png_to_csource.c -lm
 *
 * Usage:  ./png_to_csource <input.png> [-o output.c] [--verify reference.c]
 *         ./png_to_csource --decode <input.c> [-o output.ppm]
 */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMG_W 240
#define IMG_H 240
#define BPP   2
#define NUM_BYTES (IMG_W * IMG_H * BPP)

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

/* Alpha-composite a single channel against black background. */
static uint8_t alpha_composite(uint8_t channel, uint8_t alpha)
{
    return (uint8_t)(0.5 + (alpha / 255.0) * channel);
}

/* Pack 8-bit RGB into RGB565 little-endian (2 bytes). */
static void rgb565_le(uint8_t r, uint8_t g, uint8_t b,
                      uint8_t *lo, uint8_t *hi)
{
    uint16_t v = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    *lo = (uint8_t)(v & 0xFF);
    *hi = (uint8_t)(v >> 8);
}

/* Return 1 if the byte should be emitted as a literal ASCII character.
   Printable range 0x21–0x7E, excluding  \  "  and digits 0-9.          */
static int is_literal(uint8_t d)
{
    if (d < 0x21 || d > 0x7E) return 0;
    if (d == '\\' || d == '"') return 0;
    if (d >= '0' && d <= '9') return 0;
    return 1;
}

/* Column width GIMP uses for the line-break decision.
   GIMP counts the *minimal* octal width, even though it always emits
   3-digit octal in the output.                                        */
static int col_width(uint8_t d)
{
    if (is_literal(d))          return 1;
    if (d == '\\' || d == '"')  return 2;
    return 1 + 1 + (d > 7) + (d > 63);   /* \N, \NN, or \NNN */
}

/* Write one byte into the C string literal. */
static void emit_byte(FILE *out, uint8_t d)
{
    if (is_literal(d))         { fputc(d, out);               return; }
    if (d == '\\')             { fputs("\\\\", out);          return; }
    if (d == '"')              { fputs("\\\"", out);          return; }
    fprintf(out, "\\%03o", d);
}

/* ------------------------------------------------------------------ */
/* Derive the variable name from the output filename.                 */
/* Strip any leading path, then strip the .c extension.               */
/* ------------------------------------------------------------------ */
static void varname_from_path(const char *path, char *buf, size_t bufsz)
{
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;

    size_t len = strlen(base);
    if (len > 2 && strcmp(base + len - 2, ".c") == 0)
        len -= 2;
    if (len >= bufsz) len = bufsz - 1;
    memcpy(buf, base, len);
    buf[len] = '\0';
}

/* ------------------------------------------------------------------ */
/* Write the C-source file.                                           */
/* ------------------------------------------------------------------ */
static int write_csource(const char *outpath, const char *outname,
                         const uint8_t *data, size_t nbytes)
{
    FILE *out = fopen(outpath, "w");
    if (!out) { perror(outpath); return 1; }

    char varname[256];
    varname_from_path(outname, varname, sizeof(varname));

    /* ---- header ---- */
    fprintf(out, "/* GIMP RGBA C-Source image dump (%s) */\n", outname);
    fprintf(out, "\n");
    fprintf(out, "static const struct {\n");
    fprintf(out, "  unsigned int \t width;\n");
    fprintf(out, "  unsigned int \t height;\n");
    fprintf(out, "  unsigned int \t bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */ \n");
    fprintf(out, "  unsigned char\t pixel_data[%d * %d * %d + 1];\n",
            IMG_W, IMG_H, BPP);
    fprintf(out, "} %s = {\n", varname);
    fprintf(out, "  %d, %d, %d,\n", IMG_W, IMG_H, BPP);

    /* ---- pixel data as string literals ---- */
    int col = 0;
    for (size_t i = 0; i < nbytes; i++) {
        uint8_t d = data[i];
        int w = col_width(d);

        if (col == 0) {
            /* start a new line */
            fputs("  \"", out);
            col = 3;
        } else if (col > 74) {
            /* GIMP checks the current column BEFORE adding the next
               byte's width.  If it already exceeds 74, start fresh. */
            fputs("\"\n  \"", out);
            col = 3;
        }

        emit_byte(out, d);
        col += w;
    }

    /* close the last data line with trailing comma */
    fputs("\",\n", out);

    /* ---- footer ---- */
    fputs("};\n\n", out);

    fclose(out);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Verification: parse a GIMP C-source file and extract raw pixel     */
/* bytes, then compare against our generated data.                    */
/* ------------------------------------------------------------------ */
static long parse_csource_pixels(const char *path, uint8_t *buf, size_t bufsz)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return -1; }

    /* Read entire file */
    fseek(f, 0, SEEK_END);
    long filesz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *src = malloc(filesz + 1);
    if (!src) { fclose(f); return -1; }
    fread(src, 1, filesz, f);
    src[filesz] = '\0';
    fclose(f);

    /* Find the opening quote of pixel_data (after the "2,\n" line) */
    const char *p = strstr(src, "2,\n");
    if (!p) { free(src); fprintf(stderr, "Cannot find pixel data start\n"); return -1; }
    p += 3; /* skip "2,\n" */

    long count = 0;
    while (*p && count < (long)bufsz) {
        if (*p == '"') {
            p++;
            /* inside a string literal */
            while (*p && *p != '"') {
                if (*p == '\\') {
                    p++;
                    if (*p == '\\')      { buf[count++] = '\\'; p++; }
                    else if (*p == '"')  { buf[count++] = '"';  p++; }
                    else if (*p >= '0' && *p <= '7') {
                        /* octal escape: 1-3 digits */
                        unsigned val = 0;
                        int digits = 0;
                        while (*p >= '0' && *p <= '7' && digits < 3) {
                            val = val * 8 + (*p - '0');
                            p++;
                            digits++;
                        }
                        buf[count++] = (uint8_t)val;
                    } else {
                        /* unknown escape — skip */
                        p++;
                    }
                } else {
                    buf[count++] = (uint8_t)*p;
                    p++;
                }
            }
            if (*p == '"') p++;
        } else if (*p == ';') {
            break; /* end of struct */
        } else {
            p++;
        }
    }

    free(src);
    return count;
}

static int verify(const char *generated_path, const char *reference_path,
                  const uint8_t *our_data, size_t nbytes)
{
    int ret = 0;

    /* --- byte-level pixel comparison --- */
    uint8_t *ref_data = malloc(NUM_BYTES + 1);
    if (!ref_data) return 1;

    long ref_count = parse_csource_pixels(reference_path, ref_data, NUM_BYTES + 1);
    if (ref_count < 0) { free(ref_data); return 1; }

    if ((size_t)ref_count != nbytes) {
        fprintf(stderr, "PIXEL MISMATCH: reference has %ld bytes, we have %zu\n",
                ref_count, nbytes);
        free(ref_data);
        return 1;
    }

    for (size_t i = 0; i < nbytes; i++) {
        if (our_data[i] != ref_data[i]) {
            size_t pixel = i / 2;
            size_t px = pixel % IMG_W;
            size_t py = pixel / IMG_W;
            fprintf(stderr, "PIXEL MISMATCH at byte %zu (pixel %zu,%zu): "
                    "ours=0x%02X ref=0x%02X\n",
                    i, px, py, our_data[i], ref_data[i]);
            ret = 1;
            break;
        }
    }
    free(ref_data);

    if (ret == 0)
        printf("Pixel data: MATCH\n");

    /* --- full-text comparison (line-by-line) --- */
    FILE *fg = fopen(generated_path, "r");
    FILE *fr = fopen(reference_path, "r");
    if (!fg || !fr) {
        if (fg) fclose(fg);
        if (fr) fclose(fr);
        fprintf(stderr, "Cannot open files for text comparison\n");
        return 1;
    }

    char lgen[512], lref[512];
    int line = 0;
    int text_match = 1;
    int mismatches = 0;
    while (1) {
        char *gg = fgets(lgen, sizeof(lgen), fg);
        char *rr = fgets(lref, sizeof(lref), fr);
        if (!gg && !rr) break;
        line++;
        if (!gg || !rr || strcmp(lgen, lref) != 0) {
            /* Lines 1 and 8 contain the filename/varname — skip if
               the only difference is due to name mismatch.           */
            if (line == 1 || line == 8) continue;
            fprintf(stderr, "TEXT MISMATCH at line %d\n", line);
            if (gg) fprintf(stderr, "  ours: %s", lgen);
            if (rr) fprintf(stderr, "  ref:  %s", lref);
            text_match = 0;
            ret = 1;
            if (++mismatches >= 3) break;
        }
    }
    fclose(fg);
    fclose(fr);

    if (text_match)
        printf("Full text:  MATCH\n");

    return ret;
}

/* ------------------------------------------------------------------ */
/* Decode: read a C-source image and write a PPM for viewing.         */
/* ------------------------------------------------------------------ */
static int decode_csource(const char *cpath, const char *outpath)
{
    uint8_t *pixels = malloc(NUM_BYTES);
    if (!pixels) return 1;

    long count = parse_csource_pixels(cpath, pixels, NUM_BYTES);
    if (count < 0) { free(pixels); return 1; }
    if (count != NUM_BYTES) {
        fprintf(stderr, "Expected %d bytes, got %ld\n", NUM_BYTES, count);
        free(pixels);
        return 1;
    }

    FILE *out = fopen(outpath, "wb");
    if (!out) { perror(outpath); free(pixels); return 1; }

    /* PPM header */
    fprintf(out, "P6\n%d %d\n255\n", IMG_W, IMG_H);

    /* Convert RGB565 LE back to 8-bit RGB */
    for (int i = 0; i < IMG_W * IMG_H; i++) {
        uint16_t v = (uint16_t)(pixels[i * 2] | (pixels[i * 2 + 1] << 8));
        uint8_t r = (v >> 11) & 0x1F;
        uint8_t g = (v >> 5)  & 0x3F;
        uint8_t b =  v        & 0x1F;
        /* Expand to 8-bit (replicate top bits into lower bits) */
        uint8_t rgb[3] = {
            (uint8_t)((r << 3) | (r >> 2)),
            (uint8_t)((g << 2) | (g >> 4)),
            (uint8_t)((b << 3) | (b >> 2)),
        };
        fwrite(rgb, 1, 3, out);
    }

    fclose(out);
    free(pixels);
    printf("Wrote %s\n", outpath);
    return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    const char *input_path    = NULL;
    const char *output_path   = NULL;
    const char *verify_path   = NULL;
    const char *decode_path   = NULL;

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_path = argv[++i];
        } else if (strcmp(argv[i], "--verify") == 0 && i + 1 < argc) {
            verify_path = argv[++i];
        } else if (strcmp(argv[i], "--decode") == 0 && i + 1 < argc) {
            decode_path = argv[++i];
        } else if (argv[i][0] != '-') {
            input_path = argv[i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        }
    }

    /* --decode mode: C source -> PPM */
    if (decode_path) {
        char default_ppm[512];
        if (!output_path) {
            const char *base = strrchr(decode_path, '/');
            base = base ? base + 1 : decode_path;
            size_t len = strlen(base);
            if (len > 2 && strcmp(base + len - 2, ".c") == 0)
                snprintf(default_ppm, sizeof(default_ppm), "%.*s.ppm",
                         (int)(len - 2), base);
            else
                snprintf(default_ppm, sizeof(default_ppm), "%s.ppm", base);
            output_path = default_ppm;
        }
        return decode_csource(decode_path, output_path);
    }

    if (!input_path) {
        fprintf(stderr,
            "Usage: %s <input.png> [-o output.c] [--verify reference.c]\n"
            "       %s --decode <input.c> [-o output.ppm]\n",
            argv[0], argv[0]);
        return 1;
    }

    /* Default output path: replace .png with .c */
    char default_out[512];
    if (!output_path) {
        const char *base = strrchr(input_path, '/');
        base = base ? base + 1 : input_path;
        size_t len = strlen(base);
        if (len > 4 && strcmp(base + len - 4, ".png") == 0) {
            snprintf(default_out, sizeof(default_out), "%.*s.c",
                     (int)(len - 4), base);
        } else {
            snprintf(default_out, sizeof(default_out), "%s.c", base);
        }
        output_path = default_out;
    }

    /* Load PNG */
    int w, h, channels;
    uint8_t *img = stbi_load(input_path, &w, &h, &channels, 4); /* force RGBA */
    if (!img) {
        fprintf(stderr, "Failed to load %s: %s\n", input_path, stbi_failure_reason());
        return 1;
    }

    if (w != IMG_W || h != IMG_H) {
        fprintf(stderr, "Image is %dx%d, expected %dx%d\n", w, h, IMG_W, IMG_H);
        stbi_image_free(img);
        return 1;
    }

    /* Convert to RGB565 LE */
    uint8_t *rgb565 = malloc(NUM_BYTES);
    if (!rgb565) { stbi_image_free(img); return 1; }

    for (int i = 0; i < IMG_W * IMG_H; i++) {
        uint8_t r = img[i * 4 + 0];
        uint8_t g = img[i * 4 + 1];
        uint8_t b = img[i * 4 + 2];
        uint8_t a = img[i * 4 + 3];

        r = alpha_composite(r, a);
        g = alpha_composite(g, a);
        b = alpha_composite(b, a);

        rgb565_le(r, g, b, &rgb565[i * 2], &rgb565[i * 2 + 1]);
    }

    stbi_image_free(img);

    /* Derive output filename (basename only) for the comment/varname */
    const char *outname = strrchr(output_path, '/');
    outname = outname ? outname + 1 : output_path;

    /* Write C source */
    if (write_csource(output_path, outname, rgb565, NUM_BYTES) != 0) {
        free(rgb565);
        return 1;
    }
    printf("Wrote %s\n", output_path);

    /* Verify if requested */
    int ret = 0;
    if (verify_path) {
        ret = verify(output_path, verify_path, rgb565, NUM_BYTES);
    }

    free(rgb565);
    return ret;
}
