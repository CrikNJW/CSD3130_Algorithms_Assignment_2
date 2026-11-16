#pragma once

#include "image.h"
#include <functional>

namespace seam_carver
{
    using Progress = std::function<void(std::size_t completed, std::size_t total)>;
    // Shrinks width first, then height, recalculating energies after each removal.
    void resize_graph_cut(Image &image, std::size_t target_width, std::size_t target_height,
                          const Progress &progress = {}, const CancelCheck &cancelled = {});
} // namespace seam_carver
