#ifndef BLUEPRINT_ENCODER_H_
#define BLUEPRINT_ENCODER_H_

#include <libavutil/frame.h>
#include "arena.h"
#include "blueprint.h"
#include "factorio_ui.h"
#include "media_processor.h"
#include "my_string.h"

// Compression chunk size (16 KB)
#define Z_CHUNK 2<<14

/**
 * @brief Initializes the blueprint encoder to begin building a blueprint string.
 * 
 * @param a An arena for allocating all necessary data structures
 * @returns Whether the blueprint encoder successfully initialized
 */
bool Blueprint_Encoder_Init(arena *a);

/**
 * @brief Adds the entities necessary for displaying a given frame in the current blueprint.
 * 
 * @param paletted_frame A frame of display icons (using the AV_PIX_FMT_PAL8 format)
 */
void Blueprint_Encoder_Add_Frame(AVFrame *paletted_frame);

/**
 * @brief Adds an entity to the current blueprint.
 * 
 * @param entity An entity to add to the current blueprint
 */
void Blueprint_Encoder_Add_Entity(entity *entity);

/**
 * @brief Adds a string to the current blueprint.
 * 
 * @param str A string to add to the current blueprint
 * @returns Whether the string was successfully encoded
 */
bool Blueprint_Encoder_Add_String(string str);

/**
 * @brief Adds an integer to the current blueprint.
 * 
 * @param str An integer to add to the current blueprint
 */
void Blueprint_Encoder_Add_Integer(int val);

/**
 * @brief Flushes the encoder and returns the final encoded blueprint string.
 * 
 * @param add_terminator whether the string should include a null-terminator
 * @returns The finished blueprint string (may be empty on error)
 */
string Blueprint_Encoder_Finish(bool add_terminator);

/**
* @brief Encodes the given string in base64 format, appending it to the provided buffer.

* @param src Data to be encoded
* @param dest String buffer to store the encoded data
* @returns The length of the encoded string (0 on error)
*/
ptrdiff_t Base64_Encode(string src, string_buffer* dest);

#endif // BLUEPRINT_ENCODER_H_