#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

void Append16(std::vector<std::uint8_t>& output, std::uint16_t value)
{
    output.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    output.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void Append32(std::vector<std::uint8_t>& output, std::uint32_t value)
{
    Append16(output, static_cast<std::uint16_t>(value & 0xFFFFU));
    Append16(output, static_cast<std::uint16_t>((value >> 16U) & 0xFFFFU));
}

double DistanceToSegment(double x, double y, double x1, double y1, double x2, double y2)
{
    const double dx = x2 - x1;
    const double dy = y2 - y1;
    const double lengthSquared = dx * dx + dy * dy;
    const double projection = lengthSquared == 0.0
        ? 0.0
        : std::clamp(((x - x1) * dx + (y - y1) * dy) / lengthSquared, 0.0, 1.0);
    return std::hypot(x - (x1 + projection * dx), y - (y1 + projection * dy));
}

bool IsPinPixel(double x, double y, double expansion)
{
    return std::hypot(x - 16.0, y - 8.0) <= 5.0 + expansion
        || DistanceToSegment(x, y, 9.0, 14.0, 23.0, 14.0) <= 1.6 + expansion
        || DistanceToSegment(x, y, 11.0, 11.0, 13.5, 17.0) <= 1.4 + expansion
        || DistanceToSegment(x, y, 21.0, 11.0, 18.5, 17.0) <= 1.4 + expansion
        || DistanceToSegment(x, y, 16.0, 16.0, 16.0, 27.0) <= 1.0 + expansion;
}

std::vector<std::uint8_t> CreateImage(int size)
{
    const int maskStride = ((size + 31) / 32) * 4;
    std::vector<std::uint8_t> image;
    image.reserve(40 + size * size * 4 + maskStride * size);

    Append32(image, 40);
    Append32(image, static_cast<std::uint32_t>(size));
    Append32(image, static_cast<std::uint32_t>(size * 2));
    Append16(image, 1);
    Append16(image, 32);
    Append32(image, 0);
    Append32(image, static_cast<std::uint32_t>(size * size * 4));
    Append32(image, 0);
    Append32(image, 0);
    Append32(image, 0);
    Append32(image, 0);

    const double scale = 32.0 / static_cast<double>(size);
    for (int outputY = 0; outputY < size; ++outputY) {
        const int sourceY = size - 1 - outputY;
        for (int x = 0; x < size; ++x) {
            const double normalizedX = (x + 0.5) * scale;
            const double normalizedY = (sourceY + 0.5) * scale;
            const bool fill = IsPinPixel(normalizedX, normalizedY, 0.0);
            const bool outline = IsPinPixel(normalizedX, normalizedY, 1.1);
            if (fill) {
                image.insert(image.end(), {65, 55, 225, 255});
            } else if (outline) {
                image.insert(image.end(), {48, 45, 45, 255});
            } else {
                image.insert(image.end(), {0, 0, 0, 0});
            }
        }
    }

    for (int outputY = 0; outputY < size; ++outputY) {
        const int sourceY = size - 1 - outputY;
        std::vector<std::uint8_t> row(static_cast<std::size_t>(maskStride), 0xFF);
        for (int x = 0; x < size; ++x) {
            const double normalizedX = (x + 0.5) * scale;
            const double normalizedY = (sourceY + 0.5) * scale;
            if (IsPinPixel(normalizedX, normalizedY, 1.1)) {
                row[static_cast<std::size_t>(x / 8)] &= static_cast<std::uint8_t>(~(0x80U >> (x % 8)));
            }
        }
        image.insert(image.end(), row.begin(), row.end());
    }
    return image;
}

} // namespace

int main(int argumentCount, char** arguments)
{
    if (argumentCount != 2) {
        std::cerr << "Usage: pin_app_icon_generator <output.ico>\n";
        return 1;
    }

    const std::vector<int> sizes{16, 20, 24, 32, 48, 256};
    std::vector<std::vector<std::uint8_t>> images;
    for (const int size : sizes) {
        images.push_back(CreateImage(size));
    }

    std::vector<std::uint8_t> icon;
    Append16(icon, 0);
    Append16(icon, 1);
    Append16(icon, static_cast<std::uint16_t>(sizes.size()));

    std::uint32_t offset = static_cast<std::uint32_t>(6 + sizes.size() * 16);
    for (std::size_t index = 0; index < sizes.size(); ++index) {
        const int size = sizes[index];
        icon.push_back(size == 256 ? 0 : static_cast<std::uint8_t>(size));
        icon.push_back(size == 256 ? 0 : static_cast<std::uint8_t>(size));
        icon.push_back(0);
        icon.push_back(0);
        Append16(icon, 1);
        Append16(icon, 32);
        Append32(icon, static_cast<std::uint32_t>(images[index].size()));
        Append32(icon, offset);
        offset += static_cast<std::uint32_t>(images[index].size());
    }
    for (const auto& image : images) {
        icon.insert(icon.end(), image.begin(), image.end());
    }

    std::ofstream output(arguments[1], std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "Could not open output file.\n";
        return 1;
    }
    output.write(reinterpret_cast<const char*>(icon.data()), static_cast<std::streamsize>(icon.size()));
    return output ? 0 : 1;
}
