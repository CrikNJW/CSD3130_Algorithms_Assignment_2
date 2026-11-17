#include "dp.h"

#include <iostream>
#include <stdexcept>

using seam_carver::build_vertical_dp_table;
using seam_carver::EnergyMap;

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

        // Equal costs should keep the leftmost allowed predecessor at each column.
        const auto tied_table = build_vertical_dp_table({{1, 1, 1}, {1, 1, 1}});
        require(tied_table.predecessor_columns[1] == std::vector<std::size_t>{0, 0, 1},
                "Equal predecessor costs did not favor the leftmost column.");
        std::cout << "Vertical DP costs and predecessor columns passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
