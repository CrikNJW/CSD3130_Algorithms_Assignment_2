#pragma once

#include "resize.h"

namespace seam_carver
{
    struct GreedySeam
    {
        std::vector<std::size_t> indices;
        double energy;
    };

    // The map must be rectangular, nonempty and contain finite, nonnegative energies.
    // Only the pixels considered by the greedy walk are checked for valid energies.
    // Ties favor the lowest column for a vertical seam and lowest row for a horizontal seam.
    GreedySeam find_vertical_greedy(const EnergyMap &energy, const CancelCheck &cancelled = {});
    GreedySeam find_horizontal_greedy(const EnergyMap &energy, const CancelCheck &cancelled = {});

    // The observer receives the current image and its seam immediately before removal.
    using GreedyStep = std::function<void(const Image &, const GreedySeam &, bool vertical)>;

    void resize_greedy(Image &image, std::size_t target_width, std::size_t target_height,
                       const Progress &progress = {}, const CancelCheck &cancelled = {},
                       const GreedyStep &before_removal = {});

    Image highlight_greedy_seam(const Image &image, const GreedySeam &seam, bool vertical);
} // namespace seam_carver
