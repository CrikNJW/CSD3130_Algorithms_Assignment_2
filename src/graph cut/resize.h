#pragma once

#include "image.h"
#include <functional>

namespace seam_carver
{
    using Progress = std::function<void(std::size_t completed, std::size_t total)>;
    // Receives the selected seam's positions and energy before it is removed.
    using SeamObserver = std::function<void(const std::vector<std::size_t> &indices, double energy)>;
    // Shrinks width first, then height, recalculating energies after each removal.
    void resize_graph_cut(Image &image, std::size_t target_width, std::size_t target_height,
                          const Progress &progress = {}, const CancelCheck &cancelled = {},
                          const SeamObserver &before_removal = {});
} // namespace seam_carver
