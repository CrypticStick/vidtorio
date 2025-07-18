#include <assert.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include "display_renderer.h"
#include <libavutil/frame.h>

// SDL_Window *window = NULL;
// SDL_Renderer *renderer = NULL;

// const char *Renderer_Init() {
//     if (SDL_Init(SDL_INIT_VIDEO)) {
//         return SDL_GetError();
//     }

//     // Only the canvas should capture keyboard events
//     SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");

//     // Enable transparent canvas
//     if(SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8)) {
//         return SDL_GetError();
//     }

//     window = SDL_CreateWindow(NULL, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 512, 512, SDL_WINDOW_RESIZABLE);
//     if (!window) {
//         return SDL_GetError();
//     }

//     renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
//     if (!renderer) {
//         return SDL_GetError();
//     }

//     // The window dimensions should not affect the canvas size
//     emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, false, NULL);
//     emscripten_set_canvas_element_size("#canvas", 512, 512);

//     return NULL;
// }

// SDL_Renderer *Get_Renderer() {
//     return renderer;
// }

Icon_Rect Get_Icon_Frame_Rect(Factorio_Icon_Resolution icon_resolution, int icons_width, int icons_height, int icon_spacing) {
    Icon_Rect icon_rect = Get_Icon_Rect(icon_resolution);

    // Icons tile every 16 spaces in-game
    float tile_w = icon_rect.w * icon_spacing / 16;
    float tile_h = icon_rect.h * icon_spacing / 16;

    // Set new frame dimensions
    int new_w = (icons_width - 1) * tile_w + icon_rect.w;
    int new_h = (icons_height - 1) * tile_h + icon_rect.h;

    return (Icon_Rect){0, 0, new_w, new_h};
}

AVFrame *Render_Icon_Frame(AVFrame *paletted_frame, Factorio_Icon_Resolution icon_resolution, int icon_spacing) {
    // assert(window && renderer);
    if (!paletted_frame || paletted_frame->format != AV_PIX_FMT_PAL8 || !paletted_frame->data[1]) {
        av_log(NULL, AV_LOG_ERROR, "Invalid input frame for processing. Expected PAL8 with palette.\n");
        return NULL;
    }

    Icon_Rect icon_rect = Get_Icon_Rect(icon_resolution);

    // Icons tile every 16 spaces in-game
    float tile_w = icon_rect.w * icon_spacing / 16;
    float tile_h = icon_rect.h * icon_spacing / 16;

    // Get original frame dimensions
    int orig_w = paletted_frame->width;
    int orig_h = paletted_frame->height;

    // Set new frame dimensions
    int new_w = (orig_w - 1) * tile_w + icon_rect.w;
    int new_h = (orig_h - 1) * tile_h + icon_rect.h;

    AVFrame *rgba_frame = av_frame_alloc();
    if (!rgba_frame) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate new frame for upscaling\n");
        return NULL;
    }

    // Configure the new frame properties
    rgba_frame->width = new_w;
    rgba_frame->height = new_h;
    rgba_frame->format = AV_PIX_FMT_RGBA; // Output format
    rgba_frame->sample_aspect_ratio = paletted_frame->sample_aspect_ratio;
    // Copy timing information
    rgba_frame->pts = paletted_frame->pts;
    rgba_frame->pkt_dts = paletted_frame->pkt_dts;
    rgba_frame->time_base = paletted_frame->time_base; // Keep original timebase for now, rescale later

    // Allocate buffer for the new frame
    int ret = av_frame_get_buffer(rgba_frame, 0); // Use default alignment
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate buffer for rendered frame: %s\n", av_err2str(ret));
        av_frame_free(&rgba_frame);
        return NULL;
    }

    // Update and clear the window
    SDL_SetWindowSize(window, new_w, new_h);
    SDL_RenderClear(renderer);

    uint8_t *src_row = paletted_frame->data[0];
    Icon_Rect out_rect = { 0, 0, icon_rect.w, icon_rect.h };
    Factorio_Icon *cur_icon;

    // Render the icon frame
    for (int y = 0; y < orig_h; ++y) {
        uint8_t *src_pix = src_row;
        for (int x = 0; x < orig_w; ++x) {
            uint8_t palette_index = *src_pix++; // Get palette index from source (should coorespond to icon list index)
            cur_icon = Get_Icon(palette_index);
            if (!cur_icon) {
                continue;
            }
            out_rect.x = x * tile_w;
            out_rect.y = y * tile_h;
            SDL_RenderCopy(
                renderer,
                cur_icon->texture,
                &icon_rect,
                &out_rect
            ); 
        }
        src_row += paletted_frame->linesize[0]; // Move to next source row
    }

    uint8_t *dst_base = rgba_frame->data[0];
    int dst_linesize = rgba_frame->linesize[0]; // Stride for the destination RGBA frame

    SDL_Surface *surface = SDL_GetWindowSurface(window);
    assert(surface->format->BytesPerPixel == 4);
    uint8_t *src_base = surface->pixels;
    int src_linesize = surface->pitch;

    // Copy the rendered frame data to rgba_frame
    for (int y = 0; y < new_h; ++y) {
        // Pointer to the start of the destination row
        uint8_t *dst_row = dst_base + y * dst_linesize;
        uint8_t *src_row = src_base + y * src_linesize;
        memcpy(dst_row, src_row, new_w);
    }

    // Make the frame writable (av_frame_get_buffer makes it ref-counted)
    ret = av_frame_make_writable(rgba_frame);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not make upscaled frame writable: %s\n", av_err2str(ret));
        av_frame_free(&rgba_frame);
        return NULL;
    }

    // Show the finished frame
    SDL_RenderPresent(renderer);

    return rgba_frame;
}

// void Renderer_Close() {
//     SDL_DestroyRenderer(renderer);
//     SDL_DestroyWindow(window);
//     SDL_Quit();
// }