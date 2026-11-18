#include "dp.h"
#include "greedy.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace seam_carver;

namespace
{
    void require(bool condition, const char *message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    template <class Exception, class Action>
    void throws(Action action)
    {
        try { action(); }
        catch (const Exception &) { return; }
        throw std::runtime_error("Expected exception was not thrown.");
    }

    void verify_greedy_choices(const EnergyMap &energy, const GreedySeam &seam)
    {
        require(seam.indices.size() == energy.size(), "Wrong seam length");
        double sum = 0;
        for (std::size_t row = 0; row < energy.size(); ++row)
        {
            const auto column = seam.indices[row];
            require(column < energy[row].size(), "Column out of bounds");
            const auto previous = row == 0 ? 0 : seam.indices[row - 1];
            const auto distance = [](auto a, auto b) { return std::max(a, b) - std::min(a, b); };
            require(row == 0 || distance(column, previous) <= 1, "Disconnected seam");
            for (std::size_t candidate = 0; candidate < energy[row].size(); ++candidate)
            {
                if (row == 0 || distance(candidate, previous) <= 1)
                {
                    require(energy[row][column] < energy[row][candidate] ||
                            (energy[row][column] == energy[row][candidate] && column <= candidate),
                            "Local energy or tie-breaking rule violated");
                }
            }
            sum += energy[row][column];
        }
        require(seam.energy == sum, "Reported seam energy is incorrect");
    }
} // namespace

int main()
{
    try
    {
        const EnergyMap trap{{1, 8, 2}, {1, 8, 1}, {100, 100, 1}};
        const auto greedy = find_vertical_greedy(trap);
        const auto dp = find_vertical_dp(trap);
        require(greedy.indices == std::vector<std::size_t>{0, 0, 0} && greedy.energy == 102,
                "Unexpected greedy path in counterexample");
        require(dp.indices == std::vector<std::size_t>{2, 2, 2} && dp.energy == 4,
                "Unexpected DP path in counterexample");

        for (std::size_t bits = 0; bits < 512; ++bits)
        {
            EnergyMap energy(3, std::vector<double>(3)), transposed = energy;
            for (std::size_t r = 0; r < 3; ++r)
                for (std::size_t c = 0; c < 3; ++c)
                    transposed[c][r] = energy[r][c] = static_cast<double>((bits >> (3 * r + c)) & 1);
            const auto vertical = find_vertical_greedy(energy);
            const auto horizontal = find_horizontal_greedy(energy);
            verify_greedy_choices(energy, vertical);
            verify_greedy_choices(transposed, horizontal);
            require(find_vertical_dp(energy).energy <= vertical.energy &&
                    find_horizontal_dp(energy).energy <= horizontal.energy, "DP did not find a minimum");
        }
        require(find_vertical_greedy({{9, 1, 1}}).indices == std::vector<std::size_t>{1}, "One row or tie");
        require(find_horizontal_greedy({{3}, {3}, {1}}).indices == std::vector<std::size_t>{2}, "One column");
        require(find_vertical_greedy({{1}, {2}}).energy == 3, "One-column vertical seam");
        require(find_horizontal_greedy({{1, 2}}).energy == 3, "One-row horizontal seam");
        const auto infinity = std::numeric_limits<double>::infinity();
        const auto largest = std::numeric_limits<double>::max();
        throws<std::invalid_argument>([] { find_vertical_greedy({}); });
        throws<std::invalid_argument>([] { find_horizontal_greedy({{1}, {1, 2}}); });
        throws<std::invalid_argument>([&] { find_vertical_greedy({{1, infinity}}); });
        throws<std::invalid_argument>([] { find_horizontal_greedy({{1, -1}}); });
        throws<std::overflow_error>([&] { find_vertical_greedy({{largest}, {largest}}); });
        std::size_t checks = 0;
        throws<ResizeAborted>([&] { find_vertical_greedy(trap, [&] { return ++checks == 3; }); });

        Image image{3, 2, std::vector<Pixel>(6, Pixel{45, 60, 90, 177})};
        const auto original = image;
        throws<std::invalid_argument>([&] { resize_greedy(image, 4, 2); });
        throws<std::invalid_argument>([&] { resize_greedy(image, 1, 0); });
        throws<ResizeAborted>([&] { resize_greedy(image, 1, 1, {}, [] { return true; }); });
        resize_greedy(image, 3, 2);
        require(image.pixels == original.pixels, "No-op or failed resize changed the image");

        std::size_t progress_count = 0;
        std::size_t frames = 0;
        resize_greedy(image, 1, 1, [&](std::size_t completed, std::size_t total)
        {
            require(total == 3 && completed == progress_count++, "Incorrect resize progress");
        }, {}, [&](const Image &before, const GreedySeam &seam, bool vertical)
        {
            require(before.width == 3 - frames && before.height == 2, "Wrong pre-removal dimensions");
            require(vertical == (frames < 2), "Wrong removal order");
            require(std::all_of(seam.indices.begin(), seam.indices.end(), [](auto i) { return i == 0; }),
                    "Uniform image must choose index zero");
            const auto marked = highlight_greedy_seam(before, seam, vertical);
            const auto count = std::count(marked.pixels.begin(), marked.pixels.end(), Pixel{255, 0, 255, 255});
            require(static_cast<std::size_t>(count) == seam.indices.size(), "Overlay marked wrong number of pixels");
            ++frames;
        });
        require(progress_count == 4 && frames == 3 && image.width == 1 && image.height == 1 &&
                image.pixels.front() == original.pixels.front(), "Resize did not finish or preserve alpha");
        std::cout << "Greedy local choices checked on 512 maps in both directions, with resize and frame checks.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
