#include <stdio.h>
#include "blueprint_encoder.h"
#include "display_renderer.h"
#include "factorio_ui.h"
#include "arena.h"

// Results from the most recently processed media (contains null-terminators)
static string blueprint_string;
static string preview_filepath;

// Global memory reference
static arena g_arena;

static const char src_filename_cstr[MAX_FILE_LEN + 1];

int main() {
    // Allocate 64 MB for general use.
    // Note that SDL2 and FFmpeg may allocate additional memory,
    // but this will not affect the existing arena.
    g_arena = arena_new(1<<26);

    const char *error = Renderer_Init();
    if (error) {
        fprintf(stderr, "Error: %s\n", error);
        return 1;
    }

    // Use additional space necessary to prepare all icon data
    if (!Load_Icons(&g_arena)) {
        fprintf(stderr, "Error: failed to load game icons\n");
        return 1;
    }

    if (!Generate_Palette_Image()) {
        fprintf(stderr, "Error: failed to generate default color palette\n");
        return 1;
    }

    return 0;
}

/**
 * @brief Set the image format configuration.
 * 
 * @param width The target image width (in number of icons)
 * @param height The target image height (in number of icons)
 * @param tile_spacing The amount of spacing between display panels (in number of tiles)
 * @param format_flags The target image format flags
 * @param icon_resolution The target image icon resolution
 */
void Set_Image_Config(int width, int height, int tile_spacing, Image_Format_Flags format_flags, Factorio_Icon_Resolution icon_resolution) {
    media_config.image_config.width = width;
    media_config.image_config.height = height;
    media_config.image_config.tile_spacing = tile_spacing;
    media_config.image_config.format_flags = format_flags;
    media_config.image_config.icon_resolution = icon_resolution;
}

/**
 * @brief Set the video format configuration.
 * 
 * @param framerate The target video framerate
 */
void Set_Video_Config(int framerate) {
    media_config.video_config.framerate = framerate;
}

// /**
//  * @brief Set the audio format configuration.
//  */
// void Set_Audio_Config() {
//     media_config.audio_config;
// }

/**
 * @brief Gets a pointer to a string that can hold 255 characters.
 * Used to set the input filename from javascript.
 * 
 * @returns A pointer to the input filename string
 */
char *Get_Input_Filename() {
    return (char *)src_filename_cstr;
}

/**
 * @brief Determines the media format of the specified file and primes the media processor accordingly.
 * 
 * @returns The detected media format
 */
Media_Type_Flags Load_Media() {
    return load_media((char *)src_filename_cstr);
}

/**
 * @brief Generates an encoded blueprint string for the current media and renders an audiovisual preview.
 * 
 * @returns Whether the process completed successfully
 * 
 * @note Make sure `Load_Media` is called before attempting to process a file.
 */
bool Process_Media() {
    arena l_arena = g_arena;

    if (media_loaded_type == NONE_TYPE) {
        fprintf(stderr, "A valid media file has not been loaded.\n");
        goto error;
    }

    Blueprint_Encoder_Init(&l_arena);
    string preview_filepath_temp = process_media(Blueprint_Encoder_Add_Frame);
    string blueprint_string_temp = Blueprint_Encoder_Finish(true);

    if (preview_filepath_temp.len <= 0) {
        fprintf(stderr, "Failed to process media.\n");
        goto error;
    }
   
    if (blueprint_string_temp.len <= 0) {
        fprintf(stderr, "The encoded blueprint is too large for the string buffer.\n");
        goto error;
    }

    // Success; save output strings
    blueprint_string = blueprint_string_temp;
    preview_filepath = preview_filepath_temp;
    return true;
error:
    blueprint_string = string_cast(NULL);
    preview_filepath = string_cast(NULL);
    return false;
}

/**
 * @brief Gets the most recently generated blueprint string.
 * 
 * @returns the most recent blueprint string
 * 
 * @note May be null if an error occurred while generating.
 */
char *Get_Blueprint_String() {
    return blueprint_string.data;
}

/**
 * @brief Gets the most recently generated preview filepath.
 * 
 * @returns the most recent preview filepath
 * 
 * @note May be null if an error occurred while generating.
 */
char *Get_Preview_Filepath() {
    return preview_filepath.data;
}