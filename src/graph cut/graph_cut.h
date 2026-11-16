#pragma once

#include <cstddef>
#include <vector>
#include <functional>
#include <stdexcept>

namespace seam_carver
{
    using CancelCheck = std::function<bool()>;

    class ResizeAborted : public std::runtime_error
    {
    public:
        ResizeAborted() : std::runtime_error("Resizing aborted") {}
    };
    inline void check_cancelled(const CancelCheck &cancelled)
    {
        if (cancelled && cancelled())
            throw ResizeAborted{};
    }

    // Row-major, rectangular, nonempty. Energies must be finite and nonnegative.
    using EnergyMap = std::vector<std::vector<double>>;

    struct Seam
    {
        // Vertical: column per row. Horizontal: row per column. Zero-based.
        std::vector<std::size_t> indices;
        double energy;
        double cut_capacity;
    };

    // Edmonds-Karp max-flow followed by residual source-side reachability.
    // Throws std::invalid_argument for invalid input and std::overflow_error
    // if the sum of energies is too large for safe graph capacities.
    Seam find_vertical_graph_cut(const EnergyMap &energy, const CancelCheck &cancelled = {});
    Seam find_horizontal_graph_cut(const EnergyMap &energy, const CancelCheck &cancelled = {});

} // namespace seam_carver
