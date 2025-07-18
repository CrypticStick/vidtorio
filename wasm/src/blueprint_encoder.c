#include <assert.h>
#include <zlib.h>
#include "blueprint_encoder.h"
#include "factorio_ui.h"

#define MAX_NUM_LEN 11

static const unsigned char base64_table[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const string DISPLAY_PANEL = str("display-panel");

static string_buffer bp_raw_chunk = {0};
static string_buffer bp_compressed_chunk = {0};
static string_buffer bp_compressed_output = {0};

static const char number_cstr_buf[MAX_NUM_LEN];
static string_buffer number_string_buf = {{(char *)number_cstr_buf, 0}, MAX_NUM_LEN};
static bool entities_exist = false;

static arena *local_arena = {0};
static z_stream strm = {0};

static int entity_idx = 0;
static bool bp_error = false;

bool Blueprint_Encoder_Init(arena *a) {
    local_arena = a;
    entity_idx = 0;
    entities_exist = false;
    bp_error = false;

    // Used to store the raw blueprint string in chunks
    bp_raw_chunk = string_buffer_new(Z_CHUNK, local_arena);
    // Used to store the raw blueprint string in chunks
    bp_raw_chunk = string_buffer_new(Z_CHUNK, local_arena);
    // Used to store the compressed blueprint string in chunks
    bp_compressed_chunk = string_buffer_new(Z_CHUNK, local_arena);
    // Use 2 MB for the blueprint string output
    bp_compressed_output = string_buffer_new(1<<21, local_arena);

    // Blueprint string is prepended with a 0
    string_buffer_add_str(&bp_compressed_output, string("0"));

    // Initialize the zlib stream for compression
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    if (deflateInit(&strm, Z_BEST_COMPRESSION) == Z_OK) {
        Blueprint_Encoder_Add_String(BLUEPRINT_PRE_ENTITIES);
        return true;
    }

    return false;
}

bool Blueprint_Encoder_Add_String(string str) {
    ptrdiff_t have;
    int ret;

    if (bp_error) {
        // If something went wrong with the blueprint, fail early
        return false;
    }

    // Compress until the full input string has been written
    do {
        // Add next chunk of text to the input
        ptrdiff_t in_added = string_buffer_add_str(&bp_raw_chunk, str);

        // Wait for input buffer to fill before compressing data
        if (string_buffer_remaining(&bp_raw_chunk) > 0) {
            return true;
        }

        // Update input string to only include unwritten data
        if (in_added < str.len) {
            str = string_after(str, in_added);
        } else {
            str.len = 0;
        }

        strm.avail_in = bp_raw_chunk.str.len;
        strm.next_in = (Bytef *)bp_raw_chunk.str.data;

        // Compress until the input chunk is empty
        do {
            strm.avail_out = string_buffer_remaining(&bp_compressed_chunk);
            strm.next_out = (Bytef *)bp_compressed_chunk.str.data + bp_compressed_chunk.str.len;
            have = strm.avail_out;
            ret = deflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);
            // Manually update output buffer length
            bp_compressed_chunk.str.len += have - strm.avail_out;
            if (strm.avail_out == 0) {
                // Save compressed chunk in base64 and clear the buffer
                bp_error = Base64_Encode(bp_compressed_chunk.str, &bp_compressed_output) == 0;
                if (bp_error) {
                    return false;
                }
                string_buffer_clear(&bp_compressed_chunk);
            }
        } while (strm.avail_in > 0);
    } while (str.len > 0);

    return true;
}

void Blueprint_Encoder_Add_Integer(int val) {
    // TODO: optimize this to reduce redundant string building
    string_buffer_add_int(&number_string_buf, val);
    Blueprint_Encoder_Add_String(number_string_buf.str);
}

string Blueprint_Encoder_Finish(bool add_terminator) {
    ptrdiff_t have;
    int ret;
    
    if (!bp_error) { 
        Blueprint_Encoder_Add_String(BLUEPRINT_END);

        // Compress until the input is empty
        do {
            strm.avail_out = string_buffer_remaining(&bp_compressed_chunk);
            strm.next_out = (Bytef *)bp_compressed_chunk.str.data + bp_compressed_chunk.str.len;
            have = strm.avail_out;
            ret = deflate(&strm, Z_FINISH);
            assert(ret != Z_STREAM_ERROR);
            // Manually update output buffer length
            bp_compressed_chunk.str.len += have - strm.avail_out;
            if (strm.avail_out == 0) {
                // Save compressed chunk in base64 and clear the buffer
                bp_error = Base64_Encode(bp_compressed_chunk.str, &bp_compressed_output) == 0;
                if (bp_error) {
                    break;
                }
                string_buffer_clear(&bp_compressed_chunk);
            }
        } while (strm.avail_in > 0 || ret != Z_STREAM_END);

        // Save remaining compressed data in base64
        if (!bp_error && bp_compressed_chunk.str.len > 0) {
            bp_error = Base64_Encode(bp_compressed_chunk.str, &bp_compressed_output) == 0;
        }

        // Append a null-terminator to the string if desired
        if (!bp_error && add_terminator) {
            bp_error = !string_buffer_terminate(&bp_compressed_output);
        }
    }

    // Clean up
    deflateEnd(&strm);

    // Return output string
    return bp_error ? string_cast(NULL) : bp_compressed_output.str;
}

void Blueprint_Encoder_Add_Entity(entity *entity) {
    // entity_number
    if (entities_exist) {
        // Prepend comma to maintain comma-seperated list
        Blueprint_Encoder_Add_String(string(",{\"entity_number\":"));
    } else {
        Blueprint_Encoder_Add_String(string("{\"entity_number\":"));
        entities_exist = true;
    }
    Blueprint_Encoder_Add_Integer(entity->entity_number);
    // name
    Blueprint_Encoder_Add_String(string(",\"name\":\""));
    Blueprint_Encoder_Add_String(*entity->name);
    // position: x
    Blueprint_Encoder_Add_String(string("\",\"position\":{\"x\":"));
    Blueprint_Encoder_Add_Integer(entity->position_x);
    // position: y
    Blueprint_Encoder_Add_String(string(",\"y\":"));
    Blueprint_Encoder_Add_Integer(entity->position_y);
    Blueprint_Encoder_Add_String(string("}"));
    if (entity->icon) {
        // entity: type
        // entity: name
        Blueprint_Encoder_Add_String(string(",\"icon\": {\"type\": \"virtual\",\"name\": \""));
        Blueprint_Encoder_Add_String(*entity->icon->name);
        // show_in_chart
        Blueprint_Encoder_Add_String(string("\"},\"show_in_chart\": true"));
    }
}

void Blueprint_Encoder_Add_Frame(AVFrame *paletted_frame) {
    if (media_loaded_type & IMAGE_TYPE) {
        uint8_t *pixels_base = paletted_frame->data[0];
        int pixels_linesize = paletted_frame->linesize[0];
        for (int y = 0; y < paletted_frame->height; ++y) {
            for (int x = 0; x < paletted_frame->width; ++x) {
                arena scratch = *local_arena;
                Factorio_Icon *icon = Get_Icon(pixels_base[y * pixels_linesize + x]);
                if (icon) {
                    entity_icon *new_entity_icon = entity_icon_new(&icon->name, &scratch);
                    entity *new_entity = entity_new(entity_idx++, (string *)&DISPLAY_PANEL, x, y, new_entity_icon, &scratch);
                    Blueprint_Encoder_Add_Entity(new_entity);
                }
            }
        }
    } else if (media_loaded_type & VIDEO_TYPE) {
        // TODO!
        // If first frame in blueprint:
        //     - Generate display panels
        //     - Generate clock decider combinator
        //     - Generate compression decider combinators (if used)
        //     - Generate power poles for main display (if needed)
        // For each pixel:
        //     - Generate decider combinators
        //       (each signal name & rarity from pixel index; each signal value from pixel value)
    }
}

ptrdiff_t Base64_Encode(string src, string_buffer* dest) {
    unsigned char *out, *pos;
    const unsigned char *end, *in;

    ptrdiff_t olen;

    olen = 4*((src.len + 2) / 3); /* 3-byte blocks to 4-byte */

    if (olen < src.len || olen + dest->str.len > dest->cap) {
        return 0; // integer overflow or output is too large
    }

    out = (unsigned char *)dest->str.data + dest->str.len;
    end = (unsigned char *)src.data + src.len;
    in = (unsigned char *)src.data;
    pos = out;

    while (end - in >= 3) {
        *pos++ = base64_table[in[0] >> 2];
        *pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
        *pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
        *pos++ = base64_table[in[2] & 0x3f];
        in += 3;
    }

    if (end - in) {
        *pos++ = base64_table[in[0] >> 2];
        if (end - in == 1) {
            *pos++ = base64_table[(in[0] & 0x03) << 4];
            *pos++ = '=';
        } else {
            *pos++ = base64_table[((in[0] & 0x03) << 4) |
                (in[1] >> 4)];
            *pos++ = base64_table[(in[1] & 0x0f) << 2];
        }
        *pos++ = '=';
    }

    dest->str.len += olen;
    return olen;
}