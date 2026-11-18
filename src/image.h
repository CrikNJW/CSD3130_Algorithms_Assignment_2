#pragma once

#include "graph_cut.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace seam_carver
{

    // Unpremultiplied BGRA bytes, preserving the alpha channel during removal.
    using Pixel = std::array<std::uint8_t, 4>;
    struct Image
    {
        std::size_t width = 0;
        std::size_t height = 0;
        std::vector<Pixel> pixels;
    };

    void validate_image(const Image &image);
    std::vector<std::filesystem::path> list_supported_images(const std::filesystem::path &directory);
    Image load_image(const std::filesystem::path &path);
    void save_png(const Image &image, const std::filesystem::path &path);

    // Returns one RGB gradient magnitude per pixel as energy[row][column].
    // Border lookups use the nearest valid pixel. The calculation ignores alpha.
    EnergyMap calculate_energy(const Image &image);
    void remove_vertical_seam(Image &image, const std::vector<std::size_t> &seam);
    void remove_horizontal_seam(Image &image, const std::vector<std::size_t> &seam);

} // namespace seam_carver
