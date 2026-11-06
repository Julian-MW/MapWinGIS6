// SPDX-License-Identifier: MPL-2.0

/**
 * @file LayerCache.h
 * @brief Holds the last fully rendered client-area frame for pan-by-blit.
 *
 * The cache stores the frame the renderer produced for the current view
 * together with the pixel offset of the view relative to that frame. While
 * a left-drag pan shifts the offset, the control presents the cached frame
 * with BitBlt instead of re-rendering the layers; Invalidate() schedules a
 * full render once the drag ends or the view, layers, size, or backend
 * change. This is host-rendering infrastructure owned by the OCX layer; it
 * holds no scene, dataset, or view state.
 */
#pragma once

#include <Windows.h>

#include "FrameBuffer.h"

namespace mwg::activex
{
    class LayerCache final
    {
    public:
        LayerCache() noexcept = default;
        ~LayerCache() = default;

        LayerCache(const LayerCache&) = delete;
        LayerCache& operator=(const LayerCache&) = delete;

        /// True when the buffer holds the frame for the current view.
        [[nodiscard]] bool IsValid() const noexcept { return _valid; }

        /// True when the cached frame has exactly this size.
        [[nodiscard]] bool MatchesSize(const int width, const int height) const noexcept
        {
            return _valid && _buffer.Width() == width && _buffer.Height() == height;
        }

        /// Pixel position of the view relative to the cached frame;
        /// nonzero while a pan drag has moved the view away from it.
        [[nodiscard]] POINT Offset() const noexcept { return _offset; }

        [[nodiscard]] HDC Get() const noexcept { return _buffer.Get(); }

        /// Resizes the cached frame; see FrameBuffer::Resize.
        [[nodiscard]] bool Resize(const int width, const int height) noexcept
        {
            return _buffer.Resize(width, height);
        }

        /// Records that the buffer now holds the frame for the current view
        /// and resets the pan offset to it.
        void MarkRendered() noexcept
        {
            _valid = _buffer.IsValid();
            _offset = POINT{};
        }

        /// Drops the cached frame and resets the pan offset, so the next
        /// draw renders the layers again.
        void Invalidate() noexcept
        {
            _valid = false;
            _offset = POINT{};
        }

        /// Moves the view position relative to the cached frame by a pan
        /// step, in pixels.
        void Shift(const LONG deltaX, const LONG deltaY) noexcept
        {
            _offset.x += deltaX;
            _offset.y += deltaY;
        }

        /// Fills composeDc with the window background and copies the cached
        /// frame into it at the pan offset; GDI clips the overhang, leaving
        /// the exposed strips in the background color. The caller must
        /// ensure that MatchesSize(width, height) holds.
        [[nodiscard]] bool ComposeShifted(
            HDC composeDc,
            const int width,
            const int height) const noexcept;

    private:
        FrameBuffer _buffer;
        POINT _offset{};
        bool _valid = false;
    };
}
