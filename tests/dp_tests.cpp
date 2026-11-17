#include "dp.h"

#include <iostream>
#include <stdexcept>

using seam_carver::build_vertical_dp_table;
using seam_carver::EnergyMap;
using seam_carver::find_vertical_dp;

void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

int main()
{
    try
    {
        const EnergyMap energy{{8, 1, 9}, {9, 6, 1}, {8, 1, 9}};
        const auto table = build_vertical_dp_table(energy);

        // In the bottom row, column 0 can use only the first two costs (10 and 7).
        // Columns 1 and 2 can also reach the cheapest cost, 2, in column 2.
        const EnergyMap expected_costs{{8, 1, 9}, {10, 7, 2}, {15, 3, 11}};
        const std::vector<std::vector<std::size_t>> expected_predecessors{{}, {1, 1, 1}, {1, 2, 2}};
        require(table.cumulative_costs == expected_costs, "Vertical DP costs differ from the hand-worked grid.");
        require(table.predecessor_columns == expected_predecessors,
                "Vertical DP predecessor columns differ from the hand-worked grid.");

        // Bottom column 1 costs 3. Its predecessors lead through middle column 2 to top column 1.
        const auto seam = find_vertical_dp(energy);
        require(seam.indices == std::vector<std::size_t>{1, 2, 1},
                "Vertical DP seam differs from the hand-worked path.");
        require(seam.energy == 3.0, "Vertical DP seam should have total energy 3.");

        // This path differs when reversed, so it checks that output is top to bottom.
        const auto diagonal_seam = find_vertical_dp({{1, 9}, {9, 1}});
        require(diagonal_seam.indices == std::vector<std::size_t>{0, 1},
                "Vertical DP seam columns are out of row order.");

        // A single row uses its leftmost minimum directly, without any predecessor.
        const auto single_row_seam = find_vertical_dp({{9, 1, 1}});
        require(single_row_seam.indices == std::vector<std::size_t>{1} && single_row_seam.energy == 1.0,
                "Single-row seam should select the leftmost minimum.");

        // Equal costs should keep the leftmost allowed predecessor at each column.
        const auto tied_table = build_vertical_dp_table({{1, 1, 1}, {1, 1, 1}});
        require(tied_table.predecessor_columns[1] == std::vector<std::size_t>{0, 0, 1},
                "Equal predecessor costs did not favor the leftmost column.");
        std::cout << "Vertical DP costs, predecessor columns, and backtracking passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
