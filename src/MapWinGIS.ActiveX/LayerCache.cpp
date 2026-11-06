// SPDX-License-Identifier: MPL-2.0

/**
 * @file LayerCache.cpp
 * @brief Implements the rendered-frame cache for pan-by-blit.
 */
#include "LayerCache.h"

namespace mwg::activex
{
    bool LayerCache::ComposeShifted(
        HDC composeDc,
        const int width,
        const int height) const noexcept
    {
        const RECT fill{0, 0, width, height};
        if (::FillRect(composeDc, &fill, ::GetSysColorBrush(COLOR_WINDOW)) == 0)
        {
            return false;
        }

        return ::BitBlt(
                   composeDc,
                   _offset.x,
                   _offset.y,
                   width,
                   height,
                   _buffer.Get(),
                   0,
                   0,
                   SRCCOPY) != 0;
    }
}
