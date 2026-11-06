// SPDX-License-Identifier: MPL-2.0

/**
 * @file LayerCacheTests.cpp
 * @brief Tests the rendered-frame cache used for pan-by-blit.
 *
 * Covers the validity and pan-offset state machine and the shifted
 * composition, verifying pixels through real device contexts.
 */
#include <Windows.h>

#include <gtest/gtest.h>

#include "FrameBuffer.h"
#include "LayerCache.h"

TEST(LayerCache, IsInvalidBeforeRender)
{
    mwg::activex::LayerCache cache;
    EXPECT_FALSE(cache.IsValid());
    EXPECT_FALSE(cache.MatchesSize(40, 30));
    const POINT offset = cache.Offset();
    EXPECT_EQ(offset.x, 0);
    EXPECT_EQ(offset.y, 0);
}

TEST(LayerCache, ResizingAloneDoesNotValidate)
{
    mwg::activex::LayerCache cache;
    ASSERT_TRUE(cache.Resize(40, 30));
    EXPECT_FALSE(cache.IsValid());
    EXPECT_FALSE(cache.MatchesSize(40, 30));
}

TEST(LayerCache, MarkRenderedValidatesTheCurrentBufferSize)
{
    mwg::activex::LayerCache cache;
    ASSERT_TRUE(cache.Resize(40, 30));
    cache.MarkRendered();
    EXPECT_TRUE(cache.IsValid());
    EXPECT_TRUE(cache.MatchesSize(40, 30));
    EXPECT_FALSE(cache.MatchesSize(41, 30));
    EXPECT_FALSE(cache.MatchesSize(40, 31));
}

TEST(LayerCache, ShiftAccumulatesThePanOffset)
{
    mwg::activex::LayerCache cache;
    ASSERT_TRUE(cache.Resize(40, 30));
    cache.MarkRendered();

    cache.Shift(3, 4);
    cache.Shift(2, -1);
    const POINT offset = cache.Offset();
    EXPECT_EQ(offset.x, 5);
    EXPECT_EQ(offset.y, 3);
}

TEST(LayerCache, InvalidateDropsTheFrameAndResetsTheOffset)
{
    mwg::activex::LayerCache cache;
    ASSERT_TRUE(cache.Resize(40, 30));
    cache.MarkRendered();
    cache.Shift(7, 8);

    cache.Invalidate();
    EXPECT_FALSE(cache.IsValid());
    EXPECT_FALSE(cache.MatchesSize(40, 30));
    const POINT offset = cache.Offset();
    EXPECT_EQ(offset.x, 0);
    EXPECT_EQ(offset.y, 0);
}

TEST(LayerCache, MarkRenderedResetsThePanOffset)
{
    mwg::activex::LayerCache cache;
    ASSERT_TRUE(cache.Resize(40, 30));
    cache.MarkRendered();
    cache.Shift(7, 8);

    // A fresh render makes the buffer current again.
    cache.MarkRendered();
    const POINT offset = cache.Offset();
    EXPECT_EQ(offset.x, 0);
    EXPECT_EQ(offset.y, 0);
    EXPECT_TRUE(cache.IsValid());
}

namespace
{
    /// Fills the whole cache with black and makes it current.
    void FillCacheBlack(mwg::activex::LayerCache& cache)
    {
        const RECT whole{0, 0, 40, 40};
        ASSERT_NE(
            ::FillRect(
                cache.Get(),
                &whole,
                static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH))),
            0);
        cache.MarkRendered();
    }
}

TEST(LayerCache, ComposeShiftedPaintsTheFrameAtThePanOffset)
{
    mwg::activex::LayerCache cache;
    ASSERT_TRUE(cache.Resize(40, 40));
    FillCacheBlack(cache);
    cache.Shift(8, 6);

    mwg::activex::FrameBuffer compose;
    ASSERT_TRUE(compose.Resize(40, 40));
    ASSERT_TRUE(cache.ComposeShifted(compose.Get(), 40, 40));

    const COLORREF background = ::GetSysColor(COLOR_WINDOW);
    // Cache pixel (5, 5) lands at (5 + 8, 5 + 6); the strips the shift
    // exposes keep the background color.
    EXPECT_EQ(::GetPixel(compose.Get(), 13, 11), RGB(0, 0, 0));
    EXPECT_EQ(::GetPixel(compose.Get(), 39, 39), RGB(0, 0, 0));
    EXPECT_EQ(::GetPixel(compose.Get(), 2, 20), background);
    EXPECT_EQ(::GetPixel(compose.Get(), 20, 2), background);
}

TEST(LayerCache, ComposeShiftedWithZeroOffsetCopiesTheWholeFrame)
{
    mwg::activex::LayerCache cache;
    ASSERT_TRUE(cache.Resize(40, 40));
    FillCacheBlack(cache);

    mwg::activex::FrameBuffer compose;
    ASSERT_TRUE(compose.Resize(40, 40));
    ASSERT_TRUE(cache.ComposeShifted(compose.Get(), 40, 40));

    EXPECT_EQ(::GetPixel(compose.Get(), 0, 0), RGB(0, 0, 0));
    EXPECT_EQ(::GetPixel(compose.Get(), 39, 39), RGB(0, 0, 0));
}

TEST(LayerCache, ComposeShiftedLeavesBackgroundWhenTheOffsetExceedsTheFrame)
{
    mwg::activex::LayerCache cache;
    ASSERT_TRUE(cache.Resize(40, 40));
    FillCacheBlack(cache);
    cache.Shift(100, 0);

    mwg::activex::FrameBuffer compose;
    ASSERT_TRUE(compose.Resize(40, 40));
    ASSERT_TRUE(cache.ComposeShifted(compose.Get(), 40, 40));

    const COLORREF background = ::GetSysColor(COLOR_WINDOW);
    EXPECT_EQ(::GetPixel(compose.Get(), 0, 0), background);
    EXPECT_EQ(::GetPixel(compose.Get(), 39, 39), background);
}

TEST(LayerCache, ComposeShiftedAcceptsNegativeOffsets)
{
    mwg::activex::LayerCache cache;
    ASSERT_TRUE(cache.Resize(40, 40));
    FillCacheBlack(cache);
    cache.Shift(-8, -6);

    mwg::activex::FrameBuffer compose;
    ASSERT_TRUE(compose.Resize(40, 40));
    ASSERT_TRUE(cache.ComposeShifted(compose.Get(), 40, 40));

    const COLORREF background = ::GetSysColor(COLOR_WINDOW);
    // Cache pixel (8, 6) lands at the origin; the right and bottom strips
    // keep the background color.
    EXPECT_EQ(::GetPixel(compose.Get(), 0, 0), RGB(0, 0, 0));
    EXPECT_EQ(::GetPixel(compose.Get(), 31, 33), RGB(0, 0, 0));
    EXPECT_EQ(::GetPixel(compose.Get(), 39, 20), background);
    EXPECT_EQ(::GetPixel(compose.Get(), 20, 39), background);
}

TEST(LayerCache, ComposeShiftedFailsForAnInvalidTarget)
{
    mwg::activex::LayerCache cache;
    ASSERT_TRUE(cache.Resize(40, 40));
    FillCacheBlack(cache);
    EXPECT_FALSE(cache.ComposeShifted(nullptr, 40, 40));
}
