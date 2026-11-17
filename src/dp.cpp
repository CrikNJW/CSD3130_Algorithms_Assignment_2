#include "dp.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace seam_carver
{
    VerticalDpTable build_vertical_dp_table(const EnergyMap &energy)
    {
        if (energy.empty() || energy.front().empty())
            throw std::invalid_argument("Energy map must be nonempty.");

        const auto width = energy.front().size();
        for (const auto &row : energy)
        {
            if (row.size() != width)
                throw std::invalid_argument("Energy map must be rectangular.");
            for (const auto value : row)
            {
                if (!std::isfinite(value) || value < 0.0)
                    throw std::invalid_argument("Pixel energies must be finite and nonnegative.");
            }
        }

        // On the first row, each seam can start with just that pixel's energy.
        VerticalDpTable table{energy, std::vector<std::vector<std::size_t>>(energy.size())};
        for (std::size_t row = 1; row < energy.size(); ++row)
        {
            const auto &previous_costs = table.cumulative_costs[row - 1];
            auto &current_costs = table.cumulative_costs[row];
            auto &predecessors = table.predecessor_columns[row];
            predecessors.resize(width);

            for (std::size_t column = 0; column < width; ++column)
            {
                // A seam can move by at most one column between adjacent rows.
                const auto first_column = column == 0 ? 0 : column - 1;
                const auto last_column = std::min(column + 1, width - 1);
                auto best_column = first_column;

                // Scan left to right and keep the leftmost predecessor on ties.
                for (auto candidate = first_column + 1; candidate <= last_column; ++candidate)
                {
                    if (previous_costs[candidate] < previous_costs[best_column])
                        best_column = candidate;
                }

                current_costs[column] += previous_costs[best_column];
                predecessors[column] = best_column;
            }
        }

        return table;
    }
} // namespace seam_carver
