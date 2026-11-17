#include "dp.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace seam_carver {
namespace {
DPSeam find_seam(const EnergyMap& energy, bool horizontal, const CancelCheck& cancelled) {
    check_cancelled(cancelled);
    if (energy.empty() || energy.front().empty())
        throw std::invalid_argument("Energy map must be nonempty.");
    const auto width = energy.front().size();
    for (const auto& row : energy) {
        check_cancelled(cancelled);
        if (row.size() != width) throw std::invalid_argument("Energy map must be rectangular.");
        for (double v : row)
            if (!std::isfinite(v) || v < 0) throw std::invalid_argument("Invalid energy.");
    }
    // Horizontal traversal uses a transposed view, without copying the image.
    const auto steps = horizontal ? width : energy.size();
    const auto breadth = horizontal ? energy.size() : width;
    const auto value = [&](std::size_t step, std::size_t i) {
        return horizontal ? energy[i][step] : energy[step][i];
    };
    std::vector<double> previous(breadth), current(breadth);
    std::vector<std::vector<std::size_t>> parent(steps, std::vector<std::size_t>(breadth));
    for (std::size_t i = 0; i < breadth; ++i) previous[i] = value(0, i);
    for (std::size_t step = 1; step < steps; ++step) {
        check_cancelled(cancelled);
        for (std::size_t i = 0; i < breadth; ++i) {
            const auto first = i == 0 ? 0 : i - 1;
            const auto last = std::min(i + 1, breadth - 1);
            auto best = first;
            // Strict comparison resolves ties to the smallest predecessor index.
            for (auto j = first + 1; j <= last; ++j)
                if (previous[j] < previous[best]) best = j;
            parent[step][i] = best;
            current[i] = value(step, i) + previous[best];
        }
        previous.swap(current);
    }
    auto endpoint = static_cast<std::size_t>(std::min_element(previous.begin(), previous.end()) - previous.begin());
    if (!std::isfinite(previous[endpoint])) throw std::overflow_error("Minimum seam energy overflowed.");
    DPSeam result{std::vector<std::size_t>(steps), previous[endpoint]};
    for (auto step = steps; step-- > 0;) {
        result.indices[step] = endpoint;
        endpoint = parent[step][endpoint];
    }
    return result;
}
}
DPSeam find_vertical_dp(const EnergyMap& e, const CancelCheck& c) { return find_seam(e, false, c); }
DPSeam find_horizontal_dp(const EnergyMap& e, const CancelCheck& c) { return find_seam(e, true, c); }
void resize_dp(Image& image, std::size_t width, std::size_t height,
               const Progress& progress, const CancelCheck& cancelled) {
    check_cancelled(cancelled);
    validate_image(image);
    if (!width || !height || width > image.width || height > image.height)
        throw std::invalid_argument("Target dimensions must be positive and cannot exceed the original.");
    const auto total = image.width - width + image.height - height;
    std::size_t done = 0;
    if (progress) progress(done, total);
    while (image.width > width || image.height > height) {
        check_cancelled(cancelled);
        const bool vertical = image.width > width;
        // Neighbours change after removal: recalculate energy for every seam.
        const auto energy = calculate_energy(image);
        const auto seam = vertical ? find_vertical_dp(energy, cancelled) : find_horizontal_dp(energy, cancelled);
        check_cancelled(cancelled);
        if (vertical) remove_vertical_seam(image, seam.indices);
        else remove_horizontal_seam(image, seam.indices);
        ++done;
        if (progress) progress(done, total);
    }
}
}
