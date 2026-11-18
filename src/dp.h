#pragma once

#include "resize.h"

namespace seam_carver
{
    struct VerticalDpTable
    {
        // Minimum cumulative energy of a vertical seam ending at [row][column].
        EnergyMap cumulative_costs;

        // For each cell below the first row, the zero-based column in the row above.
        // The first row is empty because a seam can start in any column there.
        std::vector<std::vector<std::size_t>> predecessor_columns;
    };

    // Builds the tables from a nonempty, rectangular map of finite, nonnegative energies.
    // Equal predecessor costs favor the smallest column. Invalid maps throw std::invalid_argument.
    VerticalDpTable build_vertical_dp_table(const EnergyMap &energy);

    struct DPSeam
    {
        std::vector<std::size_t> indices;
        double energy;
    };
    using DpSeam = DPSeam;

    // Returns one zero-based column per row, ordered top to bottom, and their summed energy.
    // Ties favor smaller indices. Invalid maps, overflowing minimum costs, and cancellation
    // throw std::invalid_argument, std::overflow_error, and ResizeAborted, respectively.
    DPSeam find_vertical_dp(const EnergyMap &energy, const CancelCheck &cancelled = {});

    // Returns one zero-based row per column, ordered left to right, and their summed energy.
    // Uses the same validation, tie-breaking, and cancellation rules as find_vertical_dp.
    DPSeam find_horizontal_dp(const EnergyMap &energy, const CancelCheck &cancelled = {});

    // Shrinks image in place, width first, and reports progress after each removal.
    // Targets must be positive and no larger than the original dimensions.
    // Cancellation throws ResizeAborted and can leave earlier removals in place.
    void resize_dp(Image &image, std::size_t target_width, std::size_t target_height,
                   const Progress &progress = {}, const CancelCheck &cancelled = {},
                   const SeamObserver &before_removal = {});
} // namespace seam_carver
