#include "dp.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace seam_carver
{
    namespace
    {
        struct AccumulatedSeamCosts
        {
            std::vector<double> final_costs;
            std::vector<std::vector<std::size_t>> predecessor_columns;
        };

        AccumulatedSeamCosts accumulate_seam_costs(const EnergyMap &energy, bool horizontal,
                                                   const CancelCheck &cancelled,
                                                   EnergyMap *cumulative_costs = nullptr)
        {
            check_cancelled(cancelled);
            if (energy.empty() || energy.front().empty())
                throw std::invalid_argument("Energy map must be nonempty.");

            const auto width = energy.front().size();
            for (const auto &row : energy)
            {
                check_cancelled(cancelled);
                if (row.size() != width)
                    throw std::invalid_argument("Energy map must be rectangular.");
                for (const auto value : row)
                {
                    if (!std::isfinite(value) || value < 0.0)
                        throw std::invalid_argument("Pixel energies must be finite and nonnegative.");
                }
            }

            // Swap the axes for horizontal seams without copying the energy map.
            const auto step_count = horizontal ? width : energy.size();
            const auto breadth = horizontal ? energy.size() : width;
            const auto energy_at = [&](std::size_t step, std::size_t position)
            {
                return horizontal ? energy[position][step] : energy[step][position];
            };

            AccumulatedSeamCosts result{std::vector<double>(breadth),
                                       std::vector<std::vector<std::size_t>>(step_count)};
            auto &previous_costs = result.final_costs;
            std::vector<double> current_costs(breadth);
            // At the first step, each seam can start with just that pixel's energy.
            for (std::size_t position = 0; position < breadth; ++position)
                previous_costs[position] = energy_at(0, position);

            // Only the public table builder retains every cost row. Seam finding keeps two.
            if (cumulative_costs)
                cumulative_costs->push_back(previous_costs);

            for (std::size_t step = 1; step < step_count; ++step)
            {
                check_cancelled(cancelled);
                auto &predecessors = result.predecessor_columns[step];
                predecessors.resize(breadth);
                for (std::size_t position = 0; position < breadth; ++position)
                {
                    // Adjacent seam pixels can move by at most one position.
                    const auto first = position == 0 ? 0 : position - 1;
                    const auto last = std::min(position + 1, breadth - 1);
                    auto best = first;

                    // Scanning in index order keeps the smallest predecessor on ties.
                    for (auto candidate = first + 1; candidate <= last; ++candidate)
                    {
                        if (previous_costs[candidate] < previous_costs[best])
                            best = candidate;
                    }

                    current_costs[position] = energy_at(step, position) + previous_costs[best];
                    predecessors[position] = best;
                }

                previous_costs.swap(current_costs);
                if (cumulative_costs)
                    cumulative_costs->push_back(previous_costs);
            }
            return result;
        }

        DPSeam find_seam(const EnergyMap &energy, bool horizontal, const CancelCheck &cancelled)
        {
            const auto costs = accumulate_seam_costs(energy, horizontal, cancelled);
            const auto &final_costs = costs.final_costs;
            // The first minimum gives the smallest endpoint index on ties.
            const auto endpoint = std::min_element(final_costs.begin(), final_costs.end());
            if (!std::isfinite(*endpoint))
                throw std::overflow_error("Minimum seam energy overflowed.");

            auto position = static_cast<std::size_t>(endpoint - final_costs.begin());
            const auto &predecessors = costs.predecessor_columns;
            DPSeam seam{std::vector<std::size_t>(predecessors.size()), *endpoint};
            // Walk backward and store each index in its original traversal position.
            // The first step has no predecessor, so record it after the loop.
            for (std::size_t step = seam.indices.size() - 1; step > 0; --step)
            {
                seam.indices[step] = position;
                position = predecessors[step][position];
            }
            seam.indices.front() = position;
            return seam;
        }
    } // namespace

    VerticalDpTable build_vertical_dp_table(const EnergyMap &energy)
    {
        VerticalDpTable table;
        auto costs = accumulate_seam_costs(energy, false, {}, &table.cumulative_costs);
        table.predecessor_columns = std::move(costs.predecessor_columns);
        return table;
    }

    DPSeam find_vertical_dp(const EnergyMap &energy, const CancelCheck &cancelled)
    {
        return find_seam(energy, false, cancelled);
    }

    DPSeam find_horizontal_dp(const EnergyMap &energy, const CancelCheck &cancelled)
    {
        return find_seam(energy, true, cancelled);
    }

    void resize_dp(Image &image, std::size_t target_width, std::size_t target_height,
                   const Progress &progress, const CancelCheck &cancelled,
                   const SeamObserver &before_removal)
    {
        check_cancelled(cancelled);
        validate_image(image);
        if (target_width == 0 || target_height == 0 ||
            target_width > image.width || target_height > image.height)
            throw std::invalid_argument("Target dimensions must be positive and cannot exceed the original.");

        const auto total = image.width - target_width + image.height - target_height;
        std::size_t completed = 0;
        if (progress)
            progress(completed, total);

        while (image.width > target_width || image.height > target_height)
        {
            check_cancelled(cancelled);
            const bool vertical = image.width > target_width;
            // Removing a seam changes pixel neighbors and the next energy map.
            const auto energy = calculate_energy(image);
            const auto seam = vertical ? find_vertical_dp(energy, cancelled) : find_horizontal_dp(energy, cancelled);
            check_cancelled(cancelled);
            if (before_removal)
                before_removal(seam.indices, seam.energy);
            if (vertical)
                remove_vertical_seam(image, seam.indices);
            else
                remove_horizontal_seam(image, seam.indices);
            if (progress)
                progress(++completed, total);
        }
    }
} // namespace seam_carver
