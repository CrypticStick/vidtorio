#ifndef COLORS_H_
#define COLORS_H_

#include <stdint.h>

#define OklabA_As_Oklab(c)  (*(Oklab *)&c)
#define RGBA_As_RGB(c)      (*(RGB *)&c)

typedef struct RGB_ {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RGB;

typedef struct RGBA_ {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} RGBA;

typedef struct Linear_RGB_ {
    float r;
    float g;
    float b;
} Linear_RGB;

typedef struct Oklab_ {
    float L;
    float a;
    float b;
} Oklab;

typedef struct OklabA_ {
    float L;
    float a;
    float b;
    float A;
} OklabA;

RGB Get_RGB(uint8_t r, uint8_t g, uint8_t b);

RGBA Get_RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

RGBA RGB_To_RGBA(RGB rgb);

Linear_RGB RGB_To_Linear_RGB(RGB rgb);

RGB Linear_RGB_To_RGB(Linear_RGB lrgb);

Oklab Linear_RGB_To_Oklab(Linear_RGB lrgb);

Linear_RGB Oklab_To_Linear_RGB(Oklab oklab);

OklabA *JS_Image_To_OklabA(RGBA *data, int width, int height, arena *a);

// Calculates c1 - c2
Oklab Oklab_Diff(Oklab c1, Oklab c2);

// Calculates c1 + c2
Oklab Oklab_Sum(Oklab c1, Oklab c2);

// Calculates c1 * scalar
Oklab Oklab_Scale(Oklab c1, float scalar);

// Calculates euclidean distance between c1 and c2
float Oklab_Dist(Oklab c1, Oklab c2);

#endif // COLORS_H_
