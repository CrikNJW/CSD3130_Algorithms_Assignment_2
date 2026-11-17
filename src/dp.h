#pragma once
#include "image.h"
#include "resize.h"
namespace seam_carver {
struct DPSeam { std::vector<std::size_t> indices; double energy; };
DPSeam find_vertical_dp(const EnergyMap&, const CancelCheck& cancelled = {});
DPSeam find_horizontal_dp(const EnergyMap&, const CancelCheck& cancelled = {});
void resize_dp(Image&, std::size_t width, std::size_t height,
               const Progress& progress = {}, const CancelCheck& cancelled = {});
}
