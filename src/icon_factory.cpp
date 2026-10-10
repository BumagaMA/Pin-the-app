#include "icon_factory.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr int kIconSize = 32;

double DistanceToSegment(double x, double y, double x1, double y1, double x2, double y2)
{
    const double dx = x2 - x1;
    const double dy = y2 - y1;
    const double lengthSquared = dx * dx + dy * dy;
    const double projection = lengthSquared == 0.0
        ? 0.0
        : std::clamp(((x - x1) * dx + (y - y1) * dy) / lengthSquared, 0.0, 1.0);
    const double closestX = x1 + projection * dx;
    const double closestY = y1 + projection * dy;
    return std::hypot(x - closestX, y - closestY);
}

bool IsPinPixel(double x, double y, double expansion)
{
    const bool head = std::hypot(x - 16.0, y - 8.0) <= 5.0 + expansion;
    const bool shoulders = DistanceToSegment(x, y, 9.0, 14.0, 23.0, 14.0) <= 1.6 + expansion;
    const bool leftSide = DistanceToSegment(x, y, 11.0, 11.0, 13.5, 17.0) <= 1.4 + expansion;
    const bool rightSide = DistanceToSegment(x, y, 21.0, 11.0, 18.5, 17.0) <= 1.4 + expansion;
    const bool stem = DistanceToSegment(x, y, 16.0, 16.0, 16.0, 27.0) <= 1.0 + expansion;
    return head || shoulders || leftSide || rightSide || stem;
}

HICON CreatePinImage(COLORREF color, bool cursor)
{
    BITMAPV5HEADER header{};
    header.bV5Size = sizeof(header);
    header.bV5Width = kIconSize;
    header.bV5Height = -kIconSize;
    header.bV5Planes = 1;
    header.bV5BitCount = 32;
    header.bV5Compression = BI_BITFIELDS;
    header.bV5RedMask = 0x00FF0000;
    header.bV5GreenMask = 0x0000FF00;
    header.bV5BlueMask = 0x000000FF;
    header.bV5AlphaMask = 0xFF000000;

    void* rawPixels = nullptr;
    HDC screen = GetDC(nullptr);
    HBITMAP colorBitmap = CreateDIBSection(
        screen, reinterpret_cast<BITMAPINFO*>(&header), DIB_RGB_COLORS, &rawPixels, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (colorBitmap == nullptr || rawPixels == nullptr) {
        if (colorBitmap != nullptr) {
            DeleteObject(colorBitmap);
        }
        return nullptr;
    }

    auto* pixels = static_cast<std::uint32_t*>(rawPixels);
    constexpr int maskStride = ((kIconSize + 15) / 16) * 2;
    std::vector<std::uint8_t> mask(maskStride * kIconSize, 0xFF);

    const auto makePixel = [](COLORREF value) {
        return 0xFF000000U | (static_cast<std::uint32_t>(GetRValue(value)) << 16U)
            | (static_cast<std::uint32_t>(GetGValue(value)) << 8U)
            | static_cast<std::uint32_t>(GetBValue(value));
    };
    const std::uint32_t fillPixel = makePixel(color);
    const std::uint32_t outlinePixel = makePixel(RGB(45, 45, 48));

    for (int y = 0; y < kIconSize; ++y) {
        for (int x = 0; x < kIconSize; ++x) {
            const bool outline = IsPinPixel(x + 0.5, y + 0.5, 1.1);
            const bool fill = IsPinPixel(x + 0.5, y + 0.5, 0.0);
            pixels[y * kIconSize + x] = fill ? fillPixel : (outline ? outlinePixel : 0U);
            if (outline) {
                mask[y * maskStride + x / 8] &= static_cast<std::uint8_t>(~(0x80U >> (x % 8)));
            }
        }
    }

    HBITMAP maskBitmap = CreateBitmap(kIconSize, kIconSize, 1, 1, mask.data());
    if (maskBitmap == nullptr) {
        DeleteObject(colorBitmap);
        return nullptr;
    }

    ICONINFO info{};
    info.fIcon = cursor ? FALSE : TRUE;
    info.xHotspot = 16;
    info.yHotspot = 27;
    info.hbmColor = colorBitmap;
    info.hbmMask = maskBitmap;
    HICON result = CreateIconIndirect(&info);

    DeleteObject(maskBitmap);
    DeleteObject(colorBitmap);
    return result;
}

} // namespace

IconSet::~IconSet()
{
    Destroy();
}

bool IconSet::Create()
{
    Destroy();
    idle_ = CreatePinImage(RGB(245, 245, 245), false);
    selecting_ = CreatePinImage(RGB(55, 145, 255), false);
    pinned_ = CreatePinImage(RGB(225, 55, 65), false);
    selectionCursor_ = reinterpret_cast<HCURSOR>(CreatePinImage(RGB(55, 145, 255), true));
    return idle_ != nullptr && selecting_ != nullptr && pinned_ != nullptr && selectionCursor_ != nullptr;
}

void IconSet::Destroy()
{
    if (idle_ != nullptr) {
        DestroyIcon(idle_);
        idle_ = nullptr;
    }
    if (selecting_ != nullptr) {
        DestroyIcon(selecting_);
        selecting_ = nullptr;
    }
    if (pinned_ != nullptr) {
        DestroyIcon(pinned_);
        pinned_ = nullptr;
    }
    if (selectionCursor_ != nullptr) {
        DestroyCursor(selectionCursor_);
        selectionCursor_ = nullptr;
    }
}
