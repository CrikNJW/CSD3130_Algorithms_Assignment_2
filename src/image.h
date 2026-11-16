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
    Image load_image(const std::filesystem::path &path);
    void save_png(const Image &image, const std::filesystem::path &path);
    EnergyMap calculate_energy(const Image &image);
    void remove_vertical_seam(Image &image, const std::vector<std::size_t> &seam);
    void remove_horizontal_seam(Image &image, const std::vector<std::size_t> &seam);

} // namespace seam_carver
