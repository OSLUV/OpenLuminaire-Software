#include "image_basic.c"      /* image_basic.{width,height,pixel_data}     */
#include "image_dimmable.c"   /* image_dimmable.{…}                        */
#include "image_default.c"    /* image_default.{…}                         */
#include "lvgl.h"

/* NOTE: every *.pixel_data[] array must already be RGB565 little-endian */

/* ---------- BASIC (non-dimmable) ---------- */
const lv_image_dsc_t splash_basic_img = {
    .header = {
        .reserved_2 = 0,
        .cf         = LV_COLOR_FORMAT_RGB565,
        .w          = image_basic.width,
        .h          = image_basic.height,
        .stride     = 0               /* 0 → use w*bytes_per_pixel        */
    },
    .data_size = image_basic.width * image_basic.height * 2,
    .data      = (const uint8_t *)image_basic.pixel_data
};

/* ---------- DIMMABLE ---------------------- */
const lv_image_dsc_t splash_dimmable_img = {
    .header = {
        .reserved_2 = 0,
        .cf         = LV_COLOR_FORMAT_RGB565,
        .w          = image_dimmable.width,
        .h          = image_dimmable.height,
        .stride     = 0
    },
    .data_size = image_dimmable.width * image_dimmable.height * 2,
    .data      = (const uint8_t *)image_dimmable.pixel_data
};

/* ---------- DEFAULT / UNKNOWN ------------- */
const lv_image_dsc_t splash_default_img = {
    .header = {
        .reserved_2 = 0,
        .cf         = LV_COLOR_FORMAT_RGB565,
        .w          = image_default.width,
        .h          = image_default.height,
        .stride     = 0
    },
    .data_size = image_default.width * image_default.height * 2,
    .data      = (const uint8_t *)image_default.pixel_data
};
