#include "greedy.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace seam_carver
{
    namespace
    {
        GreedySeam find_seam(const EnergyMap &energy, bool vertical, const CancelCheck &cancelled)
        {
            check_cancelled(cancelled);
            if (energy.empty() || energy.front().empty())
                throw std::invalid_argument("Energy map must be nonempty.");

            const auto width = energy.front().size();
            for (const auto &row : energy)
                if (row.size() != width)
                    throw std::invalid_argument("Energy map must be rectangular.");

            const auto steps = vertical ? energy.size() : width;
            const auto breadth = vertical ? width : energy.size();
            const auto energy_at = [&](std::size_t step, std::size_t position)
            {
                const auto value = vertical ? energy[step][position] : energy[position][step];
                if (!std::isfinite(value) || value < 0.0)
                    throw std::invalid_argument("Pixel energies must be finite and nonnegative.");
                return value;
            };

            GreedySeam seam{std::vector<std::size_t>(steps), 0.0};
            std::size_t position = 0;
            for (std::size_t step = 0; step < steps; ++step)
            {
                check_cancelled(cancelled);
                // The first step may start anywhere. Later steps stay within one index.
                const auto first = step == 0 || position == 0 ? 0 : position - 1;
                const auto last = step == 0 ? breadth - 1 : std::min(position + 1, breadth - 1);
                auto best_energy = energy_at(step, first);
                position = first;
                for (auto candidate = first + 1; candidate <= last; ++candidate)
                {
                    const auto value = energy_at(step, candidate);
                    if (value < best_energy)
                    {
                        position = candidate;
                        best_energy = value;
                    }
                }
                seam.indices[step] = position;
                seam.energy += best_energy;
            }
            if (!std::isfinite(seam.energy))
                throw std::overflow_error("Greedy seam energy overflowed.");
            return seam;
        }
    } // namespace

    GreedySeam find_vertical_greedy(const EnergyMap &energy, const CancelCheck &cancelled)
    {
        return find_seam(energy, true, cancelled);
    }

    GreedySeam find_horizontal_greedy(const EnergyMap &energy, const CancelCheck &cancelled)
    {
        return find_seam(energy, false, cancelled);
    }

    void resize_greedy(Image &image, std::size_t target_width, std::size_t target_height,
                       const Progress &progress, const CancelCheck &cancelled,
                       const GreedyStep &before_removal)
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

        while (completed < total)
        {
            check_cancelled(cancelled);
            const bool vertical = image.width > target_width;
            const auto energy = calculate_energy(image);
            const auto seam = find_seam(energy, vertical, cancelled);
            if (before_removal)
                before_removal(image, seam, vertical);
            check_cancelled(cancelled);
            if (vertical)
                remove_vertical_seam(image, seam.indices);
            else
                remove_horizontal_seam(image, seam.indices);
            ++completed;
            if (progress)
                progress(completed, total);
        }
    }

    Image highlight_greedy_seam(const Image &image, const GreedySeam &seam, bool vertical)
    {
        validate_image(image);
        const auto steps = vertical ? image.height : image.width;
        const auto breadth = vertical ? image.width : image.height;
        if (seam.indices.size() != steps)
            throw std::invalid_argument("Seam length does not match the image.");
        auto marked = image;
        for (std::size_t step = 0; step < steps; ++step)
        {
            const auto position = seam.indices[step];
            if (position >= breadth)
                throw std::invalid_argument("Seam is out of bounds.");
            const auto offset = vertical ? step * image.width + position : position * image.width + step;
            marked.pixels[offset] = {255, 0, 255, 255};
        }
        return marked;
    }
} // namespace seam_carver
