/**
 * @file lv_image_decoder.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_bin_decoder.h"
#include "../../draw/lv_draw_image.h"
#include "../../draw/lv_draw_buf.h"
#include "../../stdlib/lv_string.h"
#include "../../stdlib/lv_sprintf.h"
#include "../../libs/rle/lv_rle.h"
#include "../../libs/lz4/lz4.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**
 * Data format for compressed image data.
 */

typedef struct _lv_image_compressed_t {
    uint32_t method: 4; /*Compression method, see `lv_image_compress_t`*/
    uint32_t reserved : 28;  /*Reserved to be used later*/
    uint32_t compressed_size;  /*Compressed data size in byte*/
    uint32_t decompressed_size;  /*Decompressed data size in byte*/
    const uint8_t * data; /*Compressed data*/
} lv_image_compressed_t;

typedef struct {
    lv_fs_file_t * f;
    lv_color32_t * palette;
    uint8_t * img_data;
    lv_opa_t * opa;
    uint8_t * decompressed;
    lv_image_compressed_t compressed;
} decoder_data_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static decoder_data_t * get_decoder_data(lv_image_decoder_dsc_t * dsc);
static void free_decoder_data(lv_image_decoder_dsc_t * dsc);
static lv_result_t decode_indexed(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc);
#if LV_BIN_DECODER_RAM_LOAD
    static lv_result_t decode_rgb(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc);
#endif
static lv_result_t decode_alpha_only(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc);
static lv_result_t decode_indexed_line(lv_color_format_t color_format, const lv_color32_t * palette, int32_t x,
                                       int32_t w_px, const uint8_t * in, lv_color32_t * out);
static lv_result_t decode_compressed(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc);

static lv_fs_res_t fs_read_file_at(lv_fs_file_t * f, uint32_t pos, void * buff, uint32_t btr, uint32_t * br);

static lv_result_t decompress_image(lv_image_decoder_dsc_t * dsc, const lv_image_compressed_t * compressed);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Initialize the lvgl binary image decoder module
 */
void lv_bin_decoder_init(void)
{
    lv_image_decoder_t * decoder;

    decoder = lv_image_decoder_create();
    LV_ASSERT_MALLOC(decoder);
    if(decoder == NULL) {
        LV_LOG_WARN("Out of memory");
        return;
    }

    lv_image_decoder_set_info_cb(decoder, lv_bin_decoder_info);
    lv_image_decoder_set_open_cb(decoder, lv_bin_decoder_open);
    lv_image_decoder_set_get_area_cb(decoder, lv_bin_decoder_get_area);
    lv_image_decoder_set_close_cb(decoder, lv_bin_decoder_close);
}

/**
 * Get info about a lvgl binary image
 * @param decoder the decoder where this function belongs
 * @param src the image source: pointer to an `lv_image_dsc_t` variable, a file path or a symbol
 * @param header store the image data here
 * @return LV_RESULT_OK: the info is successfully stored in `header`; LV_RESULT_INVALID: unknown format or other error.
 */
lv_result_t lv_bin_decoder_info(lv_image_decoder_t * decoder, const void * src, lv_image_header_t * header)
{
    LV_UNUSED(decoder); /*Unused*/

    lv_image_src_t src_type = lv_image_src_get_type(src);
    if(src_type == LV_IMAGE_SRC_VARIABLE) {
        lv_image_dsc_t * image = (lv_image_dsc_t *)src;
        lv_memcpy(header, &image->header, sizeof(lv_image_header_t));
    }
    else if(src_type == LV_IMAGE_SRC_FILE) {
        /*Support only "*.bin" files*/
        if(lv_strcmp(lv_fs_get_ext(src), "bin")) return LV_RESULT_INVALID;

        lv_fs_file_t f;
        lv_fs_res_t res = lv_fs_open(&f, src, LV_FS_MODE_RD);
        if(res == LV_FS_RES_OK) {
            uint32_t rn;
            res = lv_fs_read(&f, header, sizeof(lv_image_header_t), &rn);
            lv_fs_close(&f);
            if(res != LV_FS_RES_OK || rn != sizeof(lv_image_header_t)) {
                LV_LOG_WARN("Read file header failed: %d", res);
                return LV_RESULT_INVALID;
            }

            /*File is always read to buf, thus data can be modified.*/
            header->flags |= LV_IMAGE_FLAGS_MODIFIABLE;
        }
    }
    else if(src_type == LV_IMAGE_SRC_SYMBOL) {
        /*The size depend on the font but it is unknown here. It should be handled outside of the
         *function*/
        header->w = 1;
        header->h = 1;
        /*Symbols always have transparent parts. Important because of cover check in the draw
         *function. The actual value doesn't matter because lv_draw_label will draw it*/
        header->cf = LV_COLOR_FORMAT_A8;
    }
    else {
        LV_LOG_WARN("Image get info found unknown src type");
        return LV_RESULT_INVALID;
    }

    /*For backward compatibility, all images are not premultiplied for now.*/
    header->flags &= ~LV_IMAGE_FLAGS_PREMULTIPLIED;

    return LV_RESULT_OK;
}

/**
 * Open a lvgl binary image
 * @param decoder the decoder where this function belongs
 * @param dsc pointer to decoder descriptor. `src`, `color` are already initialized in it.
 * @param args arguments of how to decode the image.
 * @return LV_RESULT_OK: the info is successfully stored in `header`; LV_RESULT_INVALID: unknown format or other error.
 */
lv_result_t lv_bin_decoder_open(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc,
                                const lv_image_decoder_args_t * args)
{
    LV_UNUSED(decoder);
    LV_UNUSED(args);
    lv_fs_res_t res = LV_RESULT_INVALID;

    /*Open the file if it's a file*/
    if(dsc->src_type == LV_IMAGE_SRC_FILE) {
        /*Support only "*.bin" files*/
        if(lv_strcmp(lv_fs_get_ext(dsc->src), "bin")) return LV_RESULT_INVALID;

        /*If the file was open successfully save the file descriptor*/
        decoder_data_t * decoder_data = get_decoder_data(dsc);
        if(decoder_data == NULL) {
            return LV_RESULT_INVALID;
        }

        dsc->user_data = decoder_data;
        lv_fs_file_t * f = lv_malloc(sizeof(*f));
        if(f == NULL) {
            free_decoder_data(dsc);
            return LV_RESULT_INVALID;
        }

        res = lv_fs_open(f, dsc->src, LV_FS_MODE_RD);
        if(res != LV_FS_RES_OK) {
            LV_LOG_WARN("Open file failed: %d", res);
            lv_free(f);
            free_decoder_data(dsc);
            return LV_RESULT_INVALID;
        }

        decoder_data->f = f;    /*Now free_decoder_data will take care of the file*/

        lv_color_format_t cf = dsc->header.cf;

        if(dsc->header.flags & LV_IMAGE_FLAGS_COMPRESSED) {
            res = decode_compressed(decoder, dsc);
        }
        else if(LV_COLOR_FORMAT_IS_INDEXED(cf)) {
            /*Palette for indexed image and whole image of A8 image are always loaded to RAM for simplicity*/
            res = decode_indexed(decoder, dsc);
        }
        else if(LV_COLOR_FORMAT_IS_ALPHA_ONLY(cf)) {
            res = decode_alpha_only(decoder, dsc);
        }
#if LV_BIN_DECODER_RAM_LOAD
        else if(cf == LV_COLOR_FORMAT_ARGB8888      \
                || cf == LV_COLOR_FORMAT_XRGB8888   \
                || cf == LV_COLOR_FORMAT_RGB888     \
                || cf == LV_COLOR_FORMAT_RGB565     \
                || cf == LV_COLOR_FORMAT_RGB565A8) {
            res = decode_rgb(decoder, dsc);
        }
#else
        else {
            /* decode them in get_area_cb */
            res = LV_RESULT_OK;
        }
#endif
    }

    else if(dsc->src_type == LV_IMAGE_SRC_VARIABLE) {
        /*The variables should have valid data*/
        lv_image_dsc_t * image = (lv_image_dsc_t *)dsc->src;
        if(image->data == NULL) {
            return LV_RESULT_INVALID;
        }

        lv_color_format_t cf = image->header.cf;
        if(dsc->header.flags & LV_IMAGE_FLAGS_COMPRESSED) {
            /*@todo*/
            res = LV_RESULT_INVALID;
        }
        if(LV_COLOR_FORMAT_IS_INDEXED(cf)) {
            /*Need decoder data to store converted image*/
            decoder_data_t * decoder_data = get_decoder_data(dsc);
            if(decoder_data == NULL) {
                return LV_RESULT_INVALID;
            }

            res = decode_indexed(decoder, dsc);
        }
        else if(LV_COLOR_FORMAT_IS_ALPHA_ONLY(cf)) {
            /*Alpha only image will need decoder data to store pointer to decoded image, to free it when decoder closes*/
            decoder_data_t * decoder_data = get_decoder_data(dsc);
            if(decoder_data == NULL) {
                return LV_RESULT_INVALID;
            }

            res = decode_alpha_only(decoder, dsc);
        }
        else {
            /*In case of uncompressed formats the image stored in the ROM/RAM.
             *So simply give its pointer*/
            dsc->img_data = ((lv_image_dsc_t *)dsc->src)->data;
            res = LV_RESULT_OK;
        }
    }

    if(res != LV_RESULT_OK) {
        free_decoder_data(dsc);
    }

    return res;
}

/**
 * Close the pending decoding. Free resources etc.
 * @param decoder pointer to the decoder the function associated with
 * @param dsc pointer to decoder descriptor
 */
void lv_bin_decoder_close(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc)
{
    LV_UNUSED(decoder); /*Unused*/
    free_decoder_data(dsc);
}

lv_result_t lv_bin_decoder_get_area(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc,
                                    const lv_area_t * full_area, lv_area_t * decoded_area)
{
    LV_UNUSED(decoder); /*Unused*/

    lv_color_format_t cf = dsc->header.cf;
    /*Check if cf is supported*/

    bool supported = LV_COLOR_FORMAT_IS_INDEXED(cf)
                     || cf == LV_COLOR_FORMAT_ARGB8888  \
                     || cf == LV_COLOR_FORMAT_XRGB8888  \
                     || cf == LV_COLOR_FORMAT_RGB888    \
                     || cf == LV_COLOR_FORMAT_RGB565    \
                     || cf == LV_COLOR_FORMAT_RGB565A8;
    if(!supported) {
        LV_LOG_WARN("CF: %d is not supported", cf);
        return LV_RESULT_INVALID;
    }

    lv_result_t res = LV_RESULT_INVALID;
    decoder_data_t * decoder_data = dsc->user_data;
    lv_fs_file_t * f = decoder_data->f;
    uint32_t bpp = lv_color_format_get_bpp(cf);
    int32_t w_px = lv_area_get_width(full_area);
    uint8_t * img_data = NULL;
    uint32_t offset = sizeof(lv_image_header_t); /*All image starts with image header*/

    /*We only support read line by line for now*/
    if(decoded_area->y1 == LV_COORD_MIN) {
        /*Indexed image is converted to ARGB888*/
        uint32_t len = LV_COLOR_FORMAT_IS_INDEXED(cf) ? sizeof(lv_color32_t) * 8 : bpp;
        len = (len * w_px) / 8;
        img_data = lv_draw_buf_malloc(len, cf);
        LV_ASSERT_NULL(img_data);
        if(img_data == NULL)
            return LV_RESULT_INVALID;

        *decoded_area = *full_area;
        decoded_area->y2 = decoded_area->y1;
        decoder_data->img_data = img_data; /*Free on decoder close*/
    }
    else {
        decoded_area->y1++;
        decoded_area->y2++;
        img_data = decoder_data->img_data;
    }

    if(decoded_area->y1 > full_area->y2) {
        return LV_RESULT_INVALID;
    }

    if(LV_COLOR_FORMAT_IS_INDEXED(cf)) {
        int32_t x_fraction = decoded_area->x1 % (8 / bpp);
        uint32_t len = (w_px * bpp + 7) / 8 + 1; /*10px for 1bpp may across 3bytes*/
        uint8_t * buf = NULL;
        offset += dsc->palette_size * 4; /*Skip palette*/
        offset += decoded_area->y1 * dsc->header.stride;
        offset += decoded_area->x1 * bpp / 8; /*Move to x1*/
        if(dsc->src_type == LV_IMAGE_SRC_FILE) {
            buf = lv_malloc(len);
            LV_ASSERT_NULL(buf);
            if(buf == NULL)
                return LV_RESULT_INVALID;

            res = fs_read_file_at(f, offset, buf, len, NULL);
            if(res != LV_FS_RES_OK) {
                lv_free(buf);
                return LV_RESULT_INVALID;
            }
        }
        else {
            const lv_image_dsc_t * image = dsc->src;
            buf = (void *)(image->data + offset);
        }

        decode_indexed_line(cf, dsc->palette, x_fraction, w_px, buf, (lv_color32_t *)img_data);

        if(dsc->src_type == LV_IMAGE_SRC_FILE) lv_free((void *)buf);

        dsc->img_data = img_data; /*Return decoded image*/
        return LV_RESULT_OK;
    }

    if(cf == LV_COLOR_FORMAT_ARGB8888 || cf == LV_COLOR_FORMAT_XRGB8888 || cf == LV_COLOR_FORMAT_RGB888
       || cf == LV_COLOR_FORMAT_RGB565) {
        uint32_t len = (w_px * bpp) / 8;
        offset += decoded_area->y1 * dsc->header.stride;
        offset += decoded_area->x1 * bpp / 8; /*Move to x1*/
        res = fs_read_file_at(f, offset, img_data, len, NULL);
        if(res != LV_FS_RES_OK) {
            return LV_RESULT_INVALID;
        }

        dsc->img_data = img_data; /*Return decoded image*/
        return LV_RESULT_OK;
    }

    if(cf == LV_COLOR_FORMAT_RGB565A8) {
        bpp = 16; /* RGB565 + A8 mask*/
        uint32_t len = (w_px * bpp) / 8; /*map comes firstly*/
        offset += decoded_area->y1 * dsc->header.w * bpp / 8; /*Move to y1*/
        offset += decoded_area->x1 * bpp / 8; /*Move to x1*/
        res = fs_read_file_at(f, offset, img_data, len, NULL);
        if(res != LV_FS_RES_OK) {
            return LV_RESULT_INVALID;
        }

        /*Now the A8 mask*/
        offset = sizeof(lv_image_header_t);
        offset += dsc->header.h * dsc->header.w * bpp / 8; /*Move to A8 map*/
        offset += decoded_area->y1 * dsc->header.w * 1; /*Move to y1*/
        offset += decoded_area->x1 * 1; /*Move to x1*/
        res = fs_read_file_at(f, offset, img_data + len, w_px * 1, NULL);
        if(res != LV_FS_RES_OK) {
            return LV_RESULT_INVALID;
        }

        dsc->img_data = img_data; /*Return decoded image*/
        return LV_RESULT_OK;
    }

    return LV_RESULT_INVALID;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static decoder_data_t * get_decoder_data(lv_image_decoder_dsc_t * dsc)
{
    decoder_data_t * data = dsc->user_data;
    if(data == NULL) {
        data = lv_malloc_zeroed(sizeof(decoder_data_t));
        LV_ASSERT_MALLOC(data);
        if(data == NULL) {
            LV_LOG_ERROR("Out of memory");
            return NULL;
        }

        dsc->user_data = data;
    }

    return data;
}

static void free_decoder_data(lv_image_decoder_dsc_t * dsc)
{
    decoder_data_t * decoder_data = dsc->user_data;
    if(decoder_data) {
        if(decoder_data->f) {
            lv_fs_close(decoder_data->f);
            lv_free(decoder_data->f);
        }

        lv_draw_buf_free(decoder_data->img_data);
        lv_draw_buf_free(decoder_data->decompressed);
        lv_free(decoder_data->palette);
        lv_free(decoder_data);
        dsc->user_data = NULL;
    }
}

static lv_result_t decode_indexed(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc)
{
    LV_UNUSED(decoder); /*Unused*/
    lv_result_t res;
    uint32_t rn;
    decoder_data_t * decoder_data = dsc->user_data;
    lv_fs_file_t * f = decoder_data->f;
    lv_color_format_t cf = dsc->header.cf;
    uint32_t palette_len = sizeof(lv_color32_t) * LV_COLOR_INDEXED_PALETTE_SIZE(cf);
    const lv_color32_t * palette;
    const uint8_t * indexed_data = NULL;
    uint32_t stride = dsc->header.stride;

    bool is_compressed = dsc->header.flags & LV_IMAGE_FLAGS_COMPRESSED;
    if(is_compressed) {
        uint8_t * data = decoder_data->decompressed;
        palette = (lv_color32_t *)data;
        indexed_data = data + palette_len;
    }
    else if(dsc->src_type == LV_IMAGE_SRC_FILE) {
        /*read palette for indexed image*/
        palette = lv_malloc(palette_len);
        LV_ASSERT_MALLOC(palette);
        if(palette == NULL) {
            LV_LOG_ERROR("Out of memory");
            return LV_RESULT_INVALID;
        }

        res = fs_read_file_at(f, sizeof(lv_image_header_t), (uint8_t *)palette, palette_len, &rn);
        if(res != LV_FS_RES_OK || rn != palette_len) {
            LV_LOG_WARN("Read palette failed: %d", res);
            lv_free((void *)palette);
            return LV_RESULT_INVALID;
        }

#if LV_BIN_DECODER_RAM_LOAD
        indexed_data = lv_draw_buf_malloc(stride * dsc->header.h, cf);
        LV_ASSERT_MALLOC(indexed_data);
        if(indexed_data == NULL) {
            LV_LOG_ERROR("Draw buffer alloc failed");
            goto exit_with_buf;
        }

        uint32_t data_len = 0;
        if(lv_fs_seek(f, 0, LV_FS_SEEK_END) != LV_FS_RES_OK ||
           lv_fs_tell(f, &data_len) != LV_FS_RES_OK) {
            LV_LOG_WARN("Failed to get file to size");
            goto exit_with_buf;
        }

        uint32_t data_offset = sizeof(lv_image_header_t) + palette_len;
        data_len -= data_offset;
        res = fs_read_file_at(f, data_offset, (uint8_t *)indexed_data, data_len, &rn);
        if(res != LV_FS_RES_OK || rn != data_len) {
            LV_LOG_WARN("Read indexed image failed: %d", res);
            goto exit_with_buf;
        }
#endif
    }
    else if(dsc->src_type == LV_IMAGE_SRC_VARIABLE) {
        lv_image_dsc_t * image = (lv_image_dsc_t *)dsc->src;
        palette = (lv_color32_t *)image->data;
        indexed_data = image->data + palette_len;
    }
    else {
        return LV_RESULT_INVALID;
    }

    dsc->palette = palette;
    dsc->palette_size = LV_COLOR_INDEXED_PALETTE_SIZE(cf);

#if LV_BIN_DECODER_RAM_LOAD
    /*Convert to ARGB8888, since sw renderer cannot render it directly even it's in RAM*/
    stride = lv_draw_buf_width_to_stride(dsc->header.w, LV_COLOR_FORMAT_ARGB8888);
    uint8_t * img_data = lv_draw_buf_malloc(stride * dsc->header.h, cf);
    if(img_data == NULL) {
        LV_LOG_ERROR("No memory for indexed image");
        goto exit_with_buf;
    }

    const uint8_t * in = indexed_data;
    uint8_t * out = img_data;
    for(uint32_t y = 0; y < dsc->header.h; y++) {
        decode_indexed_line(cf, dsc->palette, 0, dsc->header.w, in, (lv_color32_t *)out);
        in += dsc->header.stride;
        out += stride;
    }

    dsc->header.stride = stride;
    dsc->header.cf = LV_COLOR_FORMAT_ARGB8888;
    dsc->img_data = img_data;
    decoder_data->img_data = img_data; /*Free when decoder closes*/
    if(dsc->src_type == LV_IMAGE_SRC_FILE && !is_compressed) {
        decoder_data->palette = (void *)palette; /*Free decoder data on close*/
        lv_draw_buf_free((void *)indexed_data);
    }

    return LV_RESULT_OK;
exit_with_buf:
    if(dsc->src_type == LV_IMAGE_SRC_FILE && !is_compressed) {
        lv_free((void *)palette);
        lv_draw_buf_free((void *)indexed_data);
    }
    return LV_RESULT_INVALID;
#else
    LV_UNUSED(stride);
    LV_UNUSED(indexed_data);
    /*It needs to be read by get_area_cb later*/
    return LV_RESULT_OK;
#endif
}

#if LV_BIN_DECODER_RAM_LOAD
static lv_result_t decode_rgb(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc)
{
    LV_UNUSED(decoder);
    lv_result_t res;
    decoder_data_t * decoder_data = dsc->user_data;
    lv_fs_file_t * f = decoder_data->f;
    lv_color_format_t cf = dsc->header.cf;

    uint32_t len = dsc->header.stride * dsc->header.h;
    if(cf == LV_COLOR_FORMAT_RGB565A8) {
        len += (dsc->header.stride / 2) * dsc->header.h; /*A8 mask*/
    }

    uint8_t * img_data = lv_draw_buf_malloc(len, cf);
    LV_ASSERT_MALLOC(img_data);
    if(img_data == NULL) {
        LV_LOG_ERROR("No memory for rgb file read");
        return LV_RESULT_INVALID;
    }

    uint32_t rn;
    res = fs_read_file_at(f, sizeof(lv_image_header_t), img_data, len, &rn);
    if(res != LV_FS_RES_OK || rn != len) {
        LV_LOG_WARN("Read rgb file failed: %d", res);
        lv_draw_buf_free(img_data);
        return LV_RESULT_INVALID;
    }

    dsc->img_data = img_data;
    decoder_data->img_data = img_data; /*Free when decoder closes*/
    return LV_RESULT_OK;
}
#endif

static lv_result_t decode_alpha_only(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc)
{
    LV_UNUSED(decoder);
    lv_result_t res;
    uint32_t rn;
    decoder_data_t * decoder_data = dsc->user_data;
    uint8_t bpp = lv_color_format_get_bpp(dsc->header.cf);
    uint32_t w = (dsc->header.stride * 8) / bpp;
    uint32_t buf_stride = (w * 8 + 7) >> 3; /*stride for img_data*/
    uint32_t buf_len = w * dsc->header.h; /*always decode to A8 format*/
    uint8_t * img_data = lv_draw_buf_malloc(buf_len, dsc->header.cf);
    uint32_t file_len = (uint32_t)dsc->header.stride * dsc->header.h;

    LV_ASSERT_MALLOC(img_data);
    if(img_data == NULL) {
        LV_LOG_ERROR("Out of memory");
        return LV_RESULT_INVALID;
    }

    if(dsc->header.flags & LV_IMAGE_FLAGS_COMPRESSED) {
        /*Copy from image data*/
        lv_memcpy(img_data, decoder_data->decompressed, file_len);
    }
    else if(dsc->src_type == LV_IMAGE_SRC_FILE) {
        res = fs_read_file_at(decoder_data->f, sizeof(lv_image_header_t), img_data, file_len, &rn);
        if(res != LV_FS_RES_OK || rn != file_len) {
            LV_LOG_WARN("Read header failed: %d", res);
            lv_draw_buf_free(img_data);
            return LV_RESULT_INVALID;
        }
    }
    else if(dsc->src_type == LV_IMAGE_SRC_VARIABLE) {
        /*Copy from image data*/
        lv_memcpy(img_data, ((lv_image_dsc_t *)dsc->src)->data, file_len);
    }

    if(dsc->header.cf != LV_COLOR_FORMAT_A8) {
        /*Convert A1/2/4 to A8 from last pixel to first pixel*/
        uint8_t * in = img_data + file_len - 1;
        uint8_t * out = img_data + buf_len - 1;
        uint8_t mask = (1 << bpp) - 1;
        uint8_t shift = 0;
        for(uint32_t i = 0; i < buf_len; i++) {
            /**
             * Rounding error:
             * Take bpp = 4 as example, alpha value of 0x0 to 0x0F should be
             * mapped to 0x00 to 0xFF, thus the equation should be below Equation 3.
             *
             * But it involves division and multiplication, which is slow. So, if
             * we ignore the rounding errors, Equation1, 2 could be faster. But it
             * will either has error when alpha is 0xff or 0x00.
             *
             * We use Equation 3 here for maximum accuracy.
             *
             * Equation 1: *out = ((*in >> shift) & mask) << (8 - bpp);
             * Equation 2: *out = ((((*in >> shift) & mask) + 1) << (8 - bpp)) - 1;
             * Equation 3: *out = ((*in >> shift) & mask) * 255 / ((1L << bpp) - 1) ;
             */
            *out = ((*in >> shift) & mask) * 255L / ((1L << bpp) - 1) ;
            shift += bpp;
            if(shift >= 8) {
                shift = 0;
                in--;
            }
            out--;
        }
    }

    decoder_data->img_data = img_data;
    dsc->img_data = img_data;
    dsc->header.stride = buf_stride;
    dsc->header.cf = LV_COLOR_FORMAT_A8;
    return LV_RESULT_OK;
}

static lv_result_t decode_compressed(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc)
{
    uint32_t rn;
    uint32_t len;
    uint32_t compressed_len;
    uint8_t * file_buf;
    decoder_data_t * decoder_data = get_decoder_data(dsc);
    lv_fs_file_t * f = decoder_data->f;
    lv_result_t res;
    lv_image_compressed_t * compressed = &decoder_data->compressed;
    lv_memzero(compressed, sizeof(lv_image_compressed_t));

    if(lv_fs_seek(f, 0, LV_FS_SEEK_END) != LV_FS_RES_OK ||
       lv_fs_tell(f, &compressed_len) != LV_FS_RES_OK) {
        LV_LOG_WARN("Failed to get compressed file len");
        return LV_RESULT_INVALID;
    }

    compressed_len -= sizeof(lv_image_header_t);
    compressed_len -= 12;

    /*Read compress header*/
    len = 12;
    res = fs_read_file_at(f, sizeof(lv_image_header_t), compressed, len, &rn);
    if(res != LV_FS_RES_OK || rn != len) {
        LV_LOG_WARN("Read compressed header failed: %d", res);
        return LV_RESULT_INVALID;
    }

    if(compressed->compressed_size != compressed_len) {
        LV_LOG_WARN("Compressed size mismatch: %" LV_PRIu32" != %" LV_PRIu32, compressed->compressed_size, compressed_len);
        return LV_RESULT_INVALID;
    }

    file_buf = lv_malloc(compressed_len);
    if(file_buf == NULL) {
        LV_LOG_WARN("No memory for compressed file");
        return LV_RESULT_INVALID;

    }

    /*Continue to read the compressed data following compression header*/
    res = lv_fs_read(f, file_buf, compressed_len, &rn);
    if(res != LV_FS_RES_OK || rn != compressed_len) {
        LV_LOG_WARN("Read compressed file failed: %d", res);
        lv_free(file_buf);
        return LV_RESULT_INVALID;
    }

    /*Decompress the image*/
    compressed->data = file_buf;
    res = decompress_image(dsc, compressed);
    compressed->data = NULL; /*No need to store the data any more*/
    lv_free(file_buf);
    if(res != LV_RESULT_OK) {
        LV_LOG_WARN("Decompress failed");
        return LV_RESULT_INVALID;
    }

    /*Depends on the cf, need to further decode image like an C-array image*/
    lv_image_dsc_t * image = (lv_image_dsc_t *)dsc->src;
    if(image->data == NULL) {
        return LV_RESULT_INVALID;
    }

    lv_color_format_t cf = dsc->header.cf;
    if(LV_COLOR_FORMAT_IS_INDEXED(cf)) {
        res = decode_indexed(decoder, dsc);
    }
    else if(LV_COLOR_FORMAT_IS_ALPHA_ONLY(cf)) {
        res = decode_alpha_only(decoder, dsc);
    }
    else {
        /*The decompressed data is the original image data.*/
        dsc->img_data = decoder_data->decompressed;
        res = LV_RESULT_OK;
    }

    return res;
}

static lv_result_t decode_indexed_line(lv_color_format_t color_format, const lv_color32_t * palette, int32_t x,
                                       int32_t w_px, const uint8_t * in, lv_color32_t * out)
{
    uint8_t px_size;
    uint16_t mask;

    int8_t shift   = 0;
    switch(color_format) {
        case LV_COLOR_FORMAT_I1:
            px_size = 1;
            in += x / 8;                /*8pixel per byte*/
            shift = 7 - (x & 0x7);
            break;
        case LV_COLOR_FORMAT_I2:
            px_size = 2;
            in += x / 4;                /*4pixel per byte*/
            shift = 6 - 2 * (x & 0x3);
            break;
        case LV_COLOR_FORMAT_I4:
            px_size = 4;
            in += x / 2;                /*2pixel per byte*/
            shift = 4 - 4 * (x & 0x1);
            break;
        case LV_COLOR_FORMAT_I8:
            px_size = 8;
            in += x;
            shift = 0;
            break;
        default:
            return LV_RESULT_INVALID;
    }

    mask   = (1 << px_size) - 1; /*E.g. px_size = 2; mask = 0x03*/

    int32_t i;
    for(i = 0; i < w_px; i++) {
        uint8_t val_act = (*in >> shift) & mask;
        out[i] = palette[val_act];

        shift -= px_size;
        if(shift < 0) {
            shift = 8 - px_size;
            in++;
        }
    }
    return LV_RESULT_OK;
}

static lv_fs_res_t fs_read_file_at(lv_fs_file_t * f, uint32_t pos, void * buff, uint32_t btr, uint32_t * br)
{
    lv_fs_res_t res;
    if(br) *br = 0;

    res = lv_fs_seek(f, pos, LV_FS_SEEK_SET);
    if(res != LV_FS_RES_OK) {
        return res;
    }

    res |= lv_fs_read(f, buff, btr, br);
    if(res != LV_FS_RES_OK) {
        return res;
    }

    return LV_FS_RES_OK;
}

static lv_result_t decompress_image(lv_image_decoder_dsc_t * dsc, const lv_image_compressed_t * compressed)
{
    /*Need to store decompressed data to decoder to free on close*/
    decoder_data_t * decoder_data = get_decoder_data(dsc);
    if(decoder_data == NULL) {
        return LV_RESULT_INVALID;
    }

    uint8_t * decompressed;
    uint32_t input_len = compressed->compressed_size;
    uint32_t out_len = compressed->decompressed_size;

    /*Note, stride must match.*/
    decompressed = lv_draw_buf_malloc(out_len, dsc->header.cf);
    if(decompressed == NULL) {
        LV_LOG_WARN("No memory for decompressed image, input: %" LV_PRIu32 ", output: %" LV_PRIu32, input_len, out_len);
        return LV_RESULT_INVALID;
    }

    if(compressed->method == LV_IMAGE_COMPRESS_RLE) {
#if LV_USE_RLE
        /*Compress always happen on byte*/
        uint32_t pixel_byte;
        if(dsc->header.cf == LV_COLOR_FORMAT_RGB565A8)
            pixel_byte = 2;
        else
            pixel_byte = (lv_color_format_get_bpp(dsc->header.cf) + 7) >> 3;
        const uint8_t * input = compressed->data;
        uint8_t * output = decompressed;
        uint32_t len;
        len = lv_rle_decompress(input, input_len, output, out_len, pixel_byte);
        if(len != compressed->decompressed_size) {
            LV_LOG_WARN("Decompress failed: %" LV_PRIu32 ", got: %" LV_PRIu32, out_len, len);
            lv_draw_buf_free(decompressed);
            return LV_RESULT_INVALID;
        }
#else
        LV_LOG_WARN("RLE decompress is not enabled");
        lv_draw_buf_free(decompressed);
        return LV_RESULT_INVALID;
#endif
    }
    else if(compressed->method == LV_IMAGE_COMPRESS_LZ4) {
#if LV_USE_LZ4
        const char * input = (const char *)compressed->data;
        char * output = (char *)decompressed;
        int len;
        len = LZ4_decompress_safe(input, output, input_len, out_len);
        if(len < 0 || (uint32_t)len != compressed->decompressed_size) {
            LV_LOG_WARN("Decompress failed: %" LV_PRId32 ", got: %" LV_PRId32, out_len, len);
            lv_draw_buf_free(decompressed);
            return LV_RESULT_INVALID;
        }
#else
        LV_LOG_WARN("LZ4 decompress is not enabled");
        lv_draw_buf_free(decompressed);
        return LV_RESULT_INVALID;
#endif
    }

    decoder_data->decompressed = decompressed; /*Free on decoder close*/
    return LV_RESULT_OK;
}
