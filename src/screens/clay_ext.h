#pragma once

#include "clay.h"

namespace YomitanMPV {
    struct TextConfigExt {
        int outline_thickness = 0;

        Clay_Color outline_color{
            .r = 0,
            .g = 0,
            .b = 0,
            .a = 255
        };
    };
}