#pragma once

#include "graph_cut.h"

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
} // namespace seam_carver
