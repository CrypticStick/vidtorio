#include <assert.h>
#include <SDL2/SDL_image.h>
#include <dirent.h>
#include <float.h>
#include "my_math.h"
#include "display_renderer.h"
#include "factorio_ui.h"

Factorio_Icon *icon_list = NULL;
int16_t icon_list_len = 0;

int16_t icon_black = -1;
int16_t icon_white = -1;

// Icon texture bounding boxes (by descending resolution)
const SDL_Rect ICON_RECT_VERY_LARGE = {   0, 0, 64, 64 };
const SDL_Rect ICON_RECT_LARGE      = {  64, 0, 32, 32 };
const SDL_Rect ICON_RECT_MEDIUM     = {  96, 0, 16, 16 };
const SDL_Rect ICON_RECT_SMALL      = { 112, 0,  8,  8 };

SDL_Rect Get_Icon_Rect(Factorio_Icon_Resolution resolution) {
    switch (resolution) {
        case RES_VERY_LARGE:
            return ICON_RECT_VERY_LARGE;
        case RES_LARGE:
            return ICON_RECT_LARGE;
        case RES_MEDIUM:
            return ICON_RECT_MEDIUM;
        case RES_SMALL:
            return ICON_RECT_SMALL;
    }
}

// Calculates the average color of an icon (translucent pixels are skipped).
Oklab Icon_Average_Color(SDL_Surface *icon_surface) {
    bool skip_translucent = icon_surface->format->BytesPerPixel == 4;
    Oklab avg_oklab = { 0.0f, 0.0f, 0.0f };
    int total_pixels = ICON_RECT_VERY_LARGE.w * ICON_RECT_VERY_LARGE.h;

    if (icon_surface->format->BytesPerPixel < 3) {
        // Invalid pixel format!
        return avg_oklab;
    }
    
    for (int y = 0; y < ICON_RECT_VERY_LARGE.h; ++y) {
        for (int x = 0; x < ICON_RECT_VERY_LARGE.w; ++x) {
            uint8_t *p_pixel = (uint8_t *)icon_surface->pixels
                + y * icon_surface->pitch
                + x * icon_surface->format->BytesPerPixel;
            if (skip_translucent && p_pixel[3] < 255) {
                // Don't count pixels with transparency
                total_pixels -= 1;
                continue;
            } else {
                Oklab pixel = Linear_RGB_To_Oklab(RGB_To_Linear_RGB(*(RGB *)p_pixel));
                avg_oklab.L += pixel.L;
                avg_oklab.a += pixel.a;
                avg_oklab.b += pixel.b;
            }
        }
    }
    
    avg_oklab.L /= total_pixels;
    avg_oklab.a /= total_pixels;
    avg_oklab.b /= total_pixels;
    return avg_oklab;
}

bool Load_Icons(arena *a) {
    SDL_Renderer *renderer = Get_Renderer();
    if (!renderer) {
        return false;
    }

    const char* icon_pathname = "assets/factorio-icons/";
    DIR *icon_dir = opendir(icon_pathname);
    if (icon_dir == NULL) {
        return false;
    }
    
    // Get number of icons to save
    struct dirent *icon_ent;
    int16_t icon_idx = 0;
    while ((icon_ent = readdir(icon_dir)) != NULL) {
        // Load all .png files as textures
        char *dot = strrchr(icon_ent->d_name, '.');
        if (dot && !strcmp(dot, ".png")) {
            icon_idx++;
        }
    }
    rewinddir(icon_dir);

    // Ready to import icons; clear existing data if any exists
    Free_Icons();

    icon_list = a_malloc(a, Factorio_Icon, icon_idx);
    icon_idx = 0;

    char path_buf[128];
    while ((icon_ent = readdir(icon_dir)) != NULL) {
        // Load all .png files as textures
        char *dot = strrchr(icon_ent->d_name, '.');
        if (dot && !strcmp(dot, ".png")) {
            // Save texture name
            icon_list[icon_idx].name = string_new_sized(icon_ent->d_name, (ptrdiff_t)(dot - icon_ent->d_name), a);
            // Save reference to black and white icons
            if (icon_black < 0 && strncmp((char *)icon_list[icon_idx].name.data, "signal-black", lengthof("signal-black")) == 0) {
                icon_black = icon_idx;
            } else if (icon_white < 0 && strncmp((char *)icon_list[icon_idx].name.data, "signal-white", lengthof("signal-white")) == 0) {
                icon_white = icon_idx;
            }
            // Get the image as a surface
            snprintf(path_buf, sizeof(path_buf), "%s%s", icon_pathname, icon_ent->d_name);
            SDL_Surface *icon_surface = IMG_Load(path_buf);
            if (icon_surface == NULL) {
                return false;
            }
            // Store the average color from the surface
            icon_list[icon_idx].avg_oklab = Icon_Average_Color(icon_surface);
            // Store the surface as a texture
            icon_list[icon_idx].texture = SDL_CreateTextureFromSurface(renderer, icon_surface);
            if (icon_list[icon_idx].texture == NULL) {
                return false;
            }
            // Free the surface and move on to the next icon
            SDL_FreeSurface(icon_surface);
            icon_idx += 1;
        }
    }

    icon_list_len = icon_idx;
    return true;
}

void Free_Icons() {
    if (icon_list != NULL) {
        for (int16_t idx = 0; idx < icon_list_len; ++idx) {
            if (icon_list[idx].texture != NULL) {
                SDL_DestroyTexture(icon_list[idx].texture);
            }
        }
        icon_list = NULL;
    }
    icon_list_len = 0;
}

int16_t Get_Icon_By_Color(Oklab oklab) {
    int16_t closest_idx = 0;
    float closest_distance = FLT_MAX;
    for (int16_t idx = 0; idx < icon_list_len; ++idx) {
        float cur_distance = Oklab_Dist(icon_list[idx].avg_oklab, oklab);
        if (cur_distance < closest_distance) {
            closest_idx = idx;
            closest_distance = cur_distance;
        }
    }
    return closest_idx;
}

int16_t Get_Icon_By_Value(Oklab oklab) {
    return Oklab_Dist(icon_list[icon_white].avg_oklab, oklab) < Oklab_Dist(icon_list[icon_black].avg_oklab, oklab) ?
        icon_white : icon_black;
}

Factorio_Icon *Get_Icon(int16_t icon_id) {
    if (icon_id >= 0) {
        assert(icon_id < icon_list_len);
        return &icon_list[icon_id];
    }
    assert(icon_id == -1);
    return NULL;
}

Factorio_Icon_Frame Generate_Icon_Frame(RGBA* data, int width, int height, int spacing, Display_Render_Flags fmt, arena *a) {
    int16_t *icon_ids = a_malloc(a, int16_t, width * height, NO_ZERO);
    arena scratch = *a;
    // Image data temporarily needs to be in Oklab format
    OklabA *oklab_image = JS_Image_To_OklabA(data, width, height, &scratch);
    Factorio_Icon_Frame icon_frame = { icon_ids, width, height, spacing };
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (fmt & DISPLAY_RENDER_TRANSPARENT) {
                // Make sure pixel is mostly opaque before processing
                if (oklab_image[y * width + x].A < 0.5) {
                    icon_ids[y * width + x] = -1;
                    continue;
                }
            }
            // Remove alpha channel for remaining computations
            Oklab oklab_pixel = OklabA_As_Oklab(oklab_image[y * width + x]);
            // Add icon id to the list
            int16_t icon_id = (fmt & DISPLAY_RENDER_BINARY) ? Get_Icon_By_Value(oklab_pixel)
                                                            : Get_Icon_By_Color(oklab_pixel);
            icon_ids[y * width + x] = icon_id;
            if (fmt & DISPLAY_RENDER_DITHERING) {
                // Apply quantization error to the neighboring pixels
                Factorio_Icon *icon = Get_Icon(icon_id);
                assert(icon != NULL);
                Oklab oklab_icon = icon->avg_oklab;
                Oklab quant_error = Oklab_Diff(oklab_pixel, oklab_icon);
                if (x + 1 < width) {
                    OklabA_As_Oklab(oklab_image[y * width + x + 1]) = Oklab_Sum(
                        OklabA_As_Oklab(oklab_image[y * width + x + 1]),
                        Oklab_Scale(quant_error, 7.0 / 16.0)
                    );
                }
                if (y + 1 < height) {
                    if (x - 1 >= 0) {
                        OklabA_As_Oklab(oklab_image[(y + 1) * width + (x - 1)]) = Oklab_Sum(
                            OklabA_As_Oklab(oklab_image[(y + 1) * width + (x - 1)]),
                            Oklab_Scale(quant_error, 3.0 / 16.0)
                        );
                    }
                    OklabA_As_Oklab(oklab_image[(y + 1) * width + x]) = Oklab_Sum(
                        OklabA_As_Oklab(oklab_image[(y + 1) * width + x]),
                        Oklab_Scale(quant_error, 5.0 / 16.0)
                    );
                    if (x + 1 < width) {
                        OklabA_As_Oklab(oklab_image[(y + 1) * width + (x + 1)]) = Oklab_Sum(
                            OklabA_As_Oklab(oklab_image[(y + 1) * width + (x + 1)]),
                            Oklab_Scale(quant_error, 1.0 / 16.0)
                        );
                    }
                }
            }
        }
    }
    return icon_frame;
}

Factorio_Icon_Video Generate_Icon_Video(RGBA* data, int width, int height, int spacing, int num_frames, int ticks_per_frame, Display_Render_Flags fmt, arena *a) {
    Factorio_Icon_Frame *frames = a_malloc(a, Factorio_Icon_Frame, num_frames);
    Factorio_Icon_Video icon_video = { frames, num_frames, ticks_per_frame };
    for (int frame = 0; frame < num_frames; ++frame) {
        frames[frame] = Generate_Icon_Frame(data + frame * width * height, width, height, spacing, fmt, a);
    }
    return icon_video;
}
