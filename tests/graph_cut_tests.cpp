#include "graph_cut.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

using seam_carver::EnergyMap;

void require(bool condition, const char *message)
{
    // Unlike assert(), this remains active in Release builds used by CI.
    if (!condition)
        throw std::runtime_error(message);
}

bool near(double a, double b)
{
    return std::abs(a - b) <= 1e-9 * std::max({1.0, std::abs(a), std::abs(b)});
}

double dp_minimum(const EnergyMap& energy)
{
    // Test-only independent oracle; the production solver still uses max-flow.
    std::vector<double> previous = energy.front();
    for (std::size_t y = 1; y < energy.size(); ++y) {
        std::vector<double> current = energy[y];
        for (std::size_t x = 0; x < current.size(); ++x) {
            double best = previous[x];
            if (x > 0) best = std::min(best, previous[x - 1]);
            if (x + 1 < current.size()) best = std::min(best, previous[x + 1]);
            current[x] += best;
        }
        previous = std::move(current);
    }
    return *std::min_element(previous.begin(), previous.end());
}

double exhaustive_minimum(const EnergyMap &energy)
{
    double best = std::numeric_limits<double>::infinity();
    std::function<void(std::size_t, std::size_t, double)> visit;
    visit = [&](std::size_t y, std::size_t x, double cost)
    {
        cost += energy[y][x];
        if (y + 1 == energy.size())
        {
            best = std::min(best, cost);
            return;
        }
        const std::size_t begin = x == 0 ? 0 : x - 1;
        const std::size_t end = std::min(x + 1, energy.front().size() - 1);
        for (std::size_t next = begin; next <= end; ++next)
            visit(y + 1, next, cost);
    };
    for (std::size_t x = 0; x < energy.front().size(); ++x)
        visit(0, x, 0);
    return best;
}

void check(const EnergyMap &energy, bool horizontal)
{
    const seam_carver::Seam seam = horizontal ? seam_carver::find_horizontal_graph_cut(energy)
                                 : seam_carver::find_vertical_graph_cut(energy);
    EnergyMap oriented = energy;
    if (horizontal)
    {
        oriented.assign(energy.front().size(), std::vector<double>(energy.size()));
        for (std::size_t y = 0; y < energy.size(); ++y)
            for (std::size_t x = 0; x < energy.front().size(); ++x)
                oriented[x][y] = energy[y][x];
    }
    require(seam.indices.size() == oriented.size(), "Wrong seam length");
    double cost = 0;
    for (std::size_t y = 0; y < seam.indices.size(); ++y)
    {
        const std::size_t x = seam.indices[y];
        require(x < oriented.front().size(), "Seam outside image");
        if (y > 0)
        {
            const std::size_t previous = seam.indices[y - 1];
            require(std::max(x, previous) - std::min(x, previous) <= 1,
                    "Disconnected seam");
        }
        cost += oriented[y][x];
    }
    require(near(cost, seam.energy), "Incorrect reported energy");
    require(near(cost, seam.cut_capacity), "Max-flow differs from seam cost");
    const double expected = oriented.size() <= 5 && oriented.front().size() <= 5
        ? exhaustive_minimum(oriented) : dp_minimum(oriented);
    require(near(cost, expected), "Seam is not optimal");
}

int main()
{
    try
    {
        const std::vector<EnergyMap> cases{
            {{0}}, {{7}}, {{9, 8, 0}}, {{9}, {8}, {0}}, {{9, 9, 0}, {9, 9, 0}, {9, 9, 0}}, // Last column must be eligible.
            {{0, 9, 9}, {0, 9, 9}, {0, 9, 9}},
            {{0, 9, 9}, {9, 0, 9}, {9, 9, 0}},
            {{0, 100, 100}, {100, 100, 0}},        // Row minima are disconnected.
            {{1, 2, 9}, {1, 2, 9}, {100, 100, 0}}, // Greedy trap.
            {{0, 0, 0}, {0, 0, 0}},
            {{5, 5}, {5, 5}, {5, 5}},
            {{0.1, 0.2}, {0.3, 0.1}},
            {{1e-15, 0}, {1e-15, 0}},
            {{1e100, 2e100}, {3e100, 1e100}}};
        for (const EnergyMap &energy : cases)
        {
            check(energy, false);
            check(energy, true);
        }
        const seam_carver::Seam last = seam_carver::find_vertical_graph_cut(cases[4]);
        require(last.indices == std::vector<std::size_t>({2, 2, 2}),
                "Last-column regression");
        const seam_carver::Seam tiny = seam_carver::find_vertical_graph_cut(cases[12]);
        require(tiny.energy == 0, "Tiny positive energies must not be rounded to zero");

        // A much larger capacity must not hide a tiny positive seam cost.
        const seam_carver::Seam mixed_scale = seam_carver::find_vertical_graph_cut({{0, 1, 1e-130}, {1e70, 1e10, 0}});
        require(mixed_scale.indices == std::vector<std::size_t>({2, 2}) &&
                    mixed_scale.energy == 1e-130 && mixed_scale.cut_capacity == 1e-130,
                "Wrong cut with mixed-scale capacities");

        // Exhaust all binary 3x3 maps, not merely a few hand-picked paths.
        for (unsigned mask = 0; mask < 512; ++mask)
        {
            EnergyMap energy(3, std::vector<double>(3));
            for (unsigned bit = 0; bit < 9; ++bit)
                energy[bit / 3][bit % 3] = (mask >> bit) & 1;
            check(energy, false);
            check(energy, true);
        }
        std::mt19937 generator(3130);
        std::uniform_int_distribution<int> values(0, 100);
        for (std::size_t h = 1; h <= 5; ++h)
        {
            for (std::size_t w = 1; w <= 5; ++w)
            {
                for (int trial = 0; trial < 12; ++trial)
                {
                    EnergyMap energy(h, std::vector<double>(w));
                    for (std::vector<double> &row : energy)
                        for (double &value : row)
                            value = values(generator) / 7.0;
                    check(energy, false);
                    check(energy, true);
                }
            }
        }
        // Larger grids exercise repeated augmentations and reverse residual flow.
        for (int trial = 0; trial < 100; ++trial) {
            EnergyMap energy(10 + generator() % 45, std::vector<double>(10 + generator() % 65));
            for (std::vector<double> &row : energy)
                for (double &value : row) value = std::sqrt(double(values(generator)));
            check(energy, false);
            check(energy, true);
        }
        EnergyMap wide(1, std::vector<double>(12000, 5));
        wide[0][11998] = 0.125;
        check(wide, false);
        check(wide, true);
        const std::vector<EnergyMap> invalid{
            {}, {{}}, {{1}, {1, 2}}, {{-1}}, {{std::numeric_limits<double>::infinity()}}, {{std::numeric_limits<double>::quiet_NaN()}}};
        for (const EnergyMap &energy : invalid)
        {
            for (bool horizontal : {false, true})
            {
                bool rejected = false;
                try
                {
                    if (horizontal)
                        seam_carver::find_horizontal_graph_cut(energy);
                    else
                        seam_carver::find_vertical_graph_cut(energy);
                }
                catch (const std::invalid_argument &)
                {
                    rejected = true;
                }
                require(rejected, "Invalid map accepted");
            }
        }
        bool rejected = false;
        try
        {
            seam_carver::find_vertical_graph_cut({{std::numeric_limits<double>::max()}});
        }
        catch (const std::overflow_error &)
        {
            rejected = true;
        }
        require(rejected, "Unsafe capacity accepted");
        std::cout << "Graph Cut: 1855 optimality checks and input validation passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
