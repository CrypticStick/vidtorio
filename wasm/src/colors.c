#include <math.h>
#include "arena.h"
#include "colors.h"

RGB Get_RGB(uint8_t r, uint8_t g, uint8_t b) {
    RGB rgb = { r, g, b };
    return rgb;
}

RGBA Get_RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    RGBA rgba = { r, g, b, a };
    return rgba;
}

RGBA RGB_To_RGBA(RGB rgb) {
    RGBA rgba = { rgb.r, rgb.g, rgb.b, 255 };
    return rgba;
}

float _sRGB_Nonlinear_Transform(float x)
{
    if (x >= 0.0031308f)
        return 1.055f * powf(x, 1.0f/2.4f) - 0.055f;
    else
        return 12.92f * x;
}

float _sRGB_Nonlinear_Transform_Inv(float x)
{
    if (x >= 0.04045f)
        return powf((x + 0.055f)/(1 + 0.055f), 2.4f);
    else 
        return x / 12.92f;
}

Linear_RGB RGB_To_Linear_RGB(RGB rgb) {
    Linear_RGB lrgb = { 
        _sRGB_Nonlinear_Transform_Inv(rgb.r / 255.0f),
        _sRGB_Nonlinear_Transform_Inv(rgb.g / 255.0f),
        _sRGB_Nonlinear_Transform_Inv(rgb.b / 255.0f)
    };
    return lrgb;
}

RGB Linear_RGB_To_RGB(Linear_RGB lrgb) {
    RGB rgb = { 
        _sRGB_Nonlinear_Transform(lrgb.r) * 255,
        _sRGB_Nonlinear_Transform(lrgb.g) * 255,
        _sRGB_Nonlinear_Transform(lrgb.b) * 255
    };
    return rgb;
}

Oklab Linear_RGB_To_Oklab(Linear_RGB lrgb) {
    float l = 0.4122214708f * lrgb.r + 0.5363325363f * lrgb.g + 0.0514459929f * lrgb.b;
	float m = 0.2119034982f * lrgb.r + 0.6806995451f * lrgb.g + 0.1073969566f * lrgb.b;
	float s = 0.0883024619f * lrgb.r + 0.2817188376f * lrgb.g + 0.6299787005f * lrgb.b;

    float l_ = cbrtf(l);
    float m_ = cbrtf(m);
    float s_ = cbrtf(s);

    Oklab oklab = {
        0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_,
        1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_,
        0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_,
    };
    return oklab;
}

Linear_RGB Oklab_To_Linear_RGB(Oklab oklab) {
    float l_ = oklab.L + 0.3963377774f * oklab.a + 0.2158037573f * oklab.b;
    float m_ = oklab.L - 0.1055613458f * oklab.a - 0.0638541728f * oklab.b;
    float s_ = oklab.L - 0.0894841775f * oklab.a - 1.2914855480f * oklab.b;

    float l = l_ * l_ * l_;
    float m = m_ * m_ * m_;
    float s = s_ * s_ * s_;

    Linear_RGB lrgb = {
		+4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
		-1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
		-0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s,
    };
    return lrgb;
}

// A 1024*1024 OklabA image will require a 16MB buffer
OklabA *JS_Image_To_OklabA(RGBA *data, int width, int height, arena *a) {
    OklabA *oklab_image = a_malloc(a, OklabA, width * height, NO_ZERO);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            OklabA_As_Oklab(oklab_image[y * width + x]) = Linear_RGB_To_Oklab(RGB_To_Linear_RGB(RGBA_As_RGB(data[y * width + x])));
            oklab_image[y * width + x].A = data[y * width + x].a / 255.0;
        }
    }
    return oklab_image;
}

Oklab Oklab_Diff(Oklab c1, Oklab c2) {
    return (Oklab){ c1.L - c2.L, c1.a - c2.a, c1.b - c2.b };
}

Oklab Oklab_Sum(Oklab c1, Oklab c2) {
    return (Oklab){ c1.L + c2.L, c1.a + c2.a, c1.b + c2.b };
}

Oklab Oklab_Scale(Oklab c1, float scalar) {
    return (Oklab){ c1.L * scalar, c1.a * scalar, c1.b * scalar };
}

float Oklab_Dist(Oklab c1, Oklab c2) {
    return sqrtf(powf(c1.L - c2.L, 2) + powf(c1.a - c2.a, 2) + powf(c1.b - c2.b, 2));
}
