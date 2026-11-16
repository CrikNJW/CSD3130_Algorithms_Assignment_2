#include "image.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace seam_carver
{

    void validate_image(const Image &image)
    {
        if (image.width == 0 || image.height == 0 ||
            image.width > std::numeric_limits<std::size_t>::max() / image.height ||
            image.pixels.size() != image.width * image.height)
        {
            throw std::invalid_argument("Invalid image dimensions or pixel buffer.");
        }
    }

    EnergyMap calculate_energy(const Image &image)
    {
        validate_image(image);
        EnergyMap energy(image.height, std::vector<double>(image.width));
        // RGB dual-gradient magnitude with clamped borders. Alpha is preserved
        // in the image but not included in the energy calculation.
        for (std::size_t y = 0; y < image.height; ++y)
        {
            const auto up = y == 0 ? y : y - 1;
            const auto down = std::min(y + 1, image.height - 1);
            for (std::size_t x = 0; x < image.width; ++x)
            {
                const auto left = x == 0 ? x : x - 1;
                const auto right = std::min(x + 1, image.width - 1);
                double sum = 0;
                for (std::size_t c = 0; c < 3; ++c)
                {
                    const double dx = double(image.pixels[y * image.width + right][c]) -
                                      image.pixels[y * image.width + left][c];
                    const double dy = double(image.pixels[down * image.width + x][c]) -
                                      image.pixels[up * image.width + x][c];
                    sum += dx * dx + dy * dy;
                }
                energy[y][x] = std::sqrt(sum);
            }
        }
        return energy;
    }

    namespace
    {
        void validate_seam(const std::vector<std::size_t> &seam, std::size_t length,
                           std::size_t bound)
        {
            if (bound <= 1 || seam.size() != length)
            {
                throw std::invalid_argument("Cannot remove seam: invalid length or final dimension.");
            }
            for (std::size_t i = 0; i < seam.size(); ++i)
            {
                if (seam[i] >= bound || (i > 0 &&
                                         std::max(seam[i], seam[i - 1]) - std::min(seam[i], seam[i - 1]) > 1))
                {
                    throw std::invalid_argument("Cannot remove an out-of-bounds or disconnected seam.");
                }
            }
        }
    } // namespace

    void remove_vertical_seam(Image &image, const std::vector<std::size_t> &seam)
    {
        validate_image(image);
        validate_seam(seam, image.height, image.width);
        Image result{image.width - 1, image.height, {}};
        result.pixels.reserve(result.width * result.height);
        for (std::size_t y = 0; y < image.height; ++y)
            for (std::size_t x = 0; x < image.width; ++x)
                if (x != seam[y])
                    result.pixels.push_back(image.pixels[y * image.width + x]);
        image = std::move(result);
    }

    void remove_horizontal_seam(Image &image, const std::vector<std::size_t> &seam)
    {
        validate_image(image);
        validate_seam(seam, image.width, image.height);
        Image result{image.width, image.height - 1, {}};
        result.pixels.resize(result.width * result.height);
        for (std::size_t x = 0; x < image.width; ++x)
            for (std::size_t y = 0; y < result.height; ++y)
            {
                const auto source_y = y < seam[x] ? y : y + 1;
                result.pixels[y * result.width + x] = image.pixels[source_y * image.width + x];
            }
        image = std::move(result);
    }

} // namespace seam_carver
