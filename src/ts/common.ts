// From wasm/include/media_processor.h
export const enum Media_Type_Flags {
    NONE_TYPE = 0x00,
    IMAGE_TYPE = 0x01,
    VIDEO_TYPE = 0x02,
    AUDIO_TYPE = 0x04
}

// From wasm/include/media_processor.h
export const enum Image_Format_Flags {
    IMAGE_FORMAT_DEFAULT = 0x00,
    IMAGE_FORMAT_BINARY = 0x01,
    IMAGE_USE_DITHERING = 0x02,
    IMAGE_USE_TRANSPARENCY = 0x04
}

// From wasm/include/factorio_ui.h
export const enum Factorio_Icon_Resolution {
    RES_VERY_LARGE = 0,
    RES_LARGE = 1,
    RES_MEDIUM = 2,
    RES_SMALL = 3
}
