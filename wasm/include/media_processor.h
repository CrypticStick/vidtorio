#ifndef MEDIA_PROCESSOR_H_
#define MEDIA_PROCESSOR_H_

#include "factorio_ui.h"
#include "my_string.h"

typedef enum Media_Type_Flags_ {
    NONE_TYPE = 0x00,
    IMAGE_TYPE = 0x01,
    VIDEO_TYPE = 0x02,
    AUDIO_TYPE = 0x04
} Media_Type_Flags;

typedef enum Image_Format_Flags_ {
    IMAGE_FORMAT_DEFAULT = 0x00,
    IMAGE_FORMAT_BINARY = 0x01,
    IMAGE_USE_DITHERING = 0x02,
    IMAGE_USE_TRANSPARENCY = 0x04
} Image_Format_Flags;

typedef struct Image_Format_Config_ {
    int width;          // In number of icons
    int height;         // In number of icons
    int tile_spacing;   // In number of tiles
    Image_Format_Flags format_flags;
    Factorio_Icon_Resolution icon_resolution;
} Image_Format_Config;

typedef struct Video_Format_Config_ {
    int framerate;
} Video_Format_Config;

typedef struct Audio_Format_Config_ {
    // TODO
} Audio_Format_Config;

typedef struct Media_Format_Config_ {
    Image_Format_Config image_config;
    Video_Format_Config video_config;
    Audio_Format_Config audio_config;
} Media_Format_Config;

// The global media configuration.
extern Media_Format_Config media_config;

// The type of media currently loaded.
// TODO: make a proper getter; this should be read-only for other files
extern Media_Type_Flags media_loaded_type;

// A callback function that expects a paletted frame (using the AV_PIX_FMT_PAL8 format).
typedef void (*ProcessFrameCallback)(AVFrame *);

/**
 * @brief Determines the type of media contained at the specified location and initializes the
 * media processor.
 *
 * @param filename The name of the media file (255 character limit)
 * @returns The type of media identified
 * 
 * @note Videos with only one frame will be treated as images.
 */
Media_Type_Flags load_media(char *filename);

/**
 * @brief Processes the currently loaded media file (image, video, or sound) with the current
 * configuration and generates a cooresponding preview file.
 * 
 * @param process_frame A callback function for processing each icon frame, or NULL if not using a callback
 * @returns The filepath of the generated blueprint preview (is null-terminated)
 * 
 * @note `load_media` must be called with the desired filename before using this function.
 */
string process_media(ProcessFrameCallback process_frame);

#endif // MEDIA_PROCESSOR_H_
