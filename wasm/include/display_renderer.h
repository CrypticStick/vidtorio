#ifndef DISPLAY_RENDERER_H_
#define DISPLAY_RENDERER_H_

#include <libavutil/frame.h>
#include "factorio_ui.h"

/**
 * @brief Gets the output dimensions of an icon frame with the specified properties.
 * 
 * @param icon_resolution The resolution of each icon to be drawn.
 * @param icons_width The width of the frame (in number of icons).
 * @param icons_height The height of the frame (in number of icons).
 * @param icon_spacing The desired spacing between icons (e.g. number of in-game tiles).
 * @return The dimensions of the resulting output frame (in pixels).
 */
Icon_Rect Get_Icon_Frame_Rect(Factorio_Icon_Resolution icon_resolution, int icons_width, int icons_height, int icon_spacing);

/**
 * @brief Processes a paletted frame (AV_PIX_FMT_PAL8), draws the cooresponding Factorio icons, converts it to RGBA, and renders the frame to the canvas.
 *
 * @param paletted_frame The input frame (must be AV_PIX_FMT_PAL8, with data[1] containing a palette based on the current icon list).
 * @param icon_resolution The resolution of each icon to be drawn.
 * @param icon_spacing The desired spacing between icons (e.g. number of in-game tiles).
 * @return A newly allocated AVFrame containing the upscaled RGBA data, or NULL on error.
 * 
 * The caller is responsible for freeing the returned frame using av_frame_free().
 */
AVFrame *Render_Icon_Frame(AVFrame *paletted_frame, Factorio_Icon_Resolution icon_resolution, int icon_spacing);

/**
 * @brief Closes and cleans up the display simulation.
 */
// void Renderer_Close();

#endif // DISPLAY_RENDERER_H_
