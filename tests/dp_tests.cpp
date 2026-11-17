#include "dp.h"
#include "cli.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
using namespace seam_carver;
void require(bool condition, const char *message = "DP test failed") {
    if (!condition) throw std::runtime_error(message);
}

void check_hand_worked_table() {
    const EnergyMap energy{{8, 1, 9}, {9, 6, 1}, {8, 1, 9}};
    const auto table = build_vertical_dp_table(energy);

    // Bottom column 0 can use only the first two costs (10 and 7) above it.
    // Bottom columns 1 and 2 can also reach the cheapest cost, 2, in column 2.
    const EnergyMap expected_costs{{8, 1, 9}, {10, 7, 2}, {15, 3, 11}};
    const std::vector<std::vector<std::size_t>> expected_predecessors{{}, {1, 1, 1}, {1, 2, 2}};
    require(table.cumulative_costs == expected_costs, "Vertical DP costs differ from the hand-worked grid.");
    require(table.predecessor_columns == expected_predecessors,
            "Vertical DP predecessor columns differ from the hand-worked grid.");

    // The cheapest bottom endpoint leads from column 1 through column 2 to column 1.
    const auto seam = find_vertical_dp(energy);
    require(seam.indices == std::vector<std::size_t>{1, 2, 1},
            "Vertical DP seam differs from the hand-worked path.");
    require(seam.energy == 3.0, "Vertical DP seam should have total energy 3.");

    // An asymmetric path checks top-to-bottom order, and one row needs no predecessor.
    const auto diagonal_seam = find_vertical_dp({{1, 9}, {9, 1}});
    require(diagonal_seam.indices == std::vector<std::size_t>{0, 1},
            "Vertical DP seam columns are out of row order.");
    const auto single_row_seam = find_vertical_dp({{9, 1, 1}});
    require(single_row_seam.indices == std::vector<std::size_t>{1} && single_row_seam.energy == 1.0,
            "Single-row seam should select the leftmost minimum.");

    // Equal costs keep the leftmost allowed predecessor at each column.
    const auto tied_table = build_vertical_dp_table({{1, 1, 1}, {1, 1, 1}});
    require(tied_table.predecessor_columns[1] == std::vector<std::size_t>{0, 0, 1},
            "Equal predecessor costs did not favor the leftmost column.");
}
double enumerate(const EnergyMap& e, std::size_t r, std::size_t c) {
    if (r + 1 == e.size()) return e[r][c];
    double best = std::numeric_limits<double>::infinity();
    for (auto j = c ? c - 1 : 0; j <= std::min(c + 1, e[0].size() - 1); ++j)
        best = std::min(best, enumerate(e, r + 1, j));
    return e[r][c] + best;
}
void check(const EnergyMap& e) {
    double best = std::numeric_limits<double>::infinity();
    for (std::size_t c = 0; c < e[0].size(); ++c) best = std::min(best, enumerate(e, 0, c));
    const auto s = find_vertical_dp(e);
    require(s.energy == best && s.indices.size() == e.size());
    double sum = 0;
    for (std::size_t r = 0; r < e.size(); ++r) {
        require(s.indices[r] < e[0].size());
        if (r) require(std::abs(int(s.indices[r]) - int(s.indices[r-1])) <= 1);
        sum += e[r][s.indices[r]];
    }
    require(sum == best);
    EnergyMap t(e[0].size(), std::vector<double>(e.size()));
    for (std::size_t r = 0; r < e.size(); ++r)
        for (std::size_t c = 0; c < e[0].size(); ++c) t[c][r] = e[r][c];
    const auto h = find_horizontal_dp(t);
    require(h.indices == s.indices && h.energy == s.energy);
}
template<class F> void rejects(F f) {
    bool rejected = false;
    try { f(); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected);
}
int main() {
 try {
    check_hand_worked_table();
    std::mt19937 rng(42);
    for (std::size_t h = 1; h <= 5; ++h) for (std::size_t w = 1; w <= 5; ++w)
        for (int trial = 0; trial < 20; ++trial) {
            EnergyMap e(h, std::vector<double>(w));
            for (auto& row : e) for (auto& v : row) v = rng() % 20;
            check(e);
        }
    require(find_vertical_dp({{0,0},{0,0}}).indices == std::vector<std::size_t>({0,0}));
    rejects([] { find_vertical_dp({}); });
    rejects([] { find_vertical_dp({{1},{1,2}}); });
    rejects([] { find_horizontal_dp({{-1}}); });
    rejects([] { find_vertical_dp({{std::numeric_limits<double>::quiet_NaN()}}); });
    Image original{4, 3, {}};
    for (int i = 0; i < 12; ++i) original.pixels.push_back({std::uint8_t(i*17),std::uint8_t(i*9),0,std::uint8_t(200+i)});
    auto expected = original;
    while (expected.width > 2) remove_vertical_seam(expected, find_vertical_dp(calculate_energy(expected)).indices);
    while (expected.height > 1) remove_horizontal_seam(expected, find_horizontal_dp(calculate_energy(expected)).indices);
    auto actual = original;
    std::size_t calls = 0;
    resize_dp(actual, 2, 1, [&](std::size_t n, std::size_t total) { require(n == calls++ && total == 4); });
    require(actual.width == 2 && actual.height == 1 && actual.pixels == expected.pixels && calls == 5);
    auto unchanged = original; resize_dp(unchanged, 4, 3); require(unchanged.pixels == original.pixels);
    rejects([&] { resize_dp(actual, 0, 1); });
    rejects([&] { resize_dp(actual, 9, 1); });
    bool aborted = false;
    try { resize_dp(unchanged, 1, 1, {}, [] { return true; }); } catch (const ResizeAborted&) { aborted = true; }
    require(aborted && unchanged.pixels == original.pixels);
    const auto temp = std::filesystem::temp_directory_path() / ("seam-dp-test-" + std::to_string(rng()));
    std::filesystem::create_directories(temp);
    save_png(original, temp / "input.png");
    std::istringstream input("2\n" + (temp / "input.png").string() + "\n1\n2\n0\n");
    std::ostringstream output;
    require(run_interactive(input, output, temp) == 0);
    const auto saved = load_image(temp / "output/input_dp_2x1.png");
    require(saved.width == 2 && saved.height == 1 && saved.pixels == expected.pixels);
    std::filesystem::remove_all(temp);
    std::cout << "DP: 500 exhaustive comparisons, orientations, validation, resize, cancellation and CLI passed.\n";
    return 0;
 } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
