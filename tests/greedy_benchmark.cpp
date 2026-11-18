#include "dp.h"
#include "greedy.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

using namespace seam_carver;
namespace fs = std::filesystem;

namespace
{
    using Clock = std::chrono::steady_clock;
    constexpr std::size_t trials = 9;
    volatile double result_sink = 0.0;

    // Run both orders to limit timing bias. Each function returns milliseconds.
    template <class Greedy, class DP>
    std::array<double, 2> medians(Greedy greedy, DP dp)
    {
        greedy();
        dp();
        std::array<double, trials> g{}, d{};
        for (std::size_t i = 0; i < trials; ++i)
        {
            if (i % 2 == 0)
            {
                g[i] = greedy();
                d[i] = dp();
            }
            else
            {
                d[i] = dp();
                g[i] = greedy();
            }
        }
        std::sort(g.begin(), g.end());
        std::sort(d.begin(), d.end());
        return {g[trials / 2], d[trials / 2]};
    }

    double elapsed_ms(Clock::time_point start)
    {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }

    Image fixture(bool grid)
    {
        Image image{grid ? 256u : 160u, grid ? 160u : 96u, {}};
        image.pixels.reserve(image.width * image.height);
        for (std::size_t y = 0; y < image.height; ++y)
        {
            for (std::size_t x = 0; x < image.width; ++x)
            {
                if (grid)
                {
                    const auto offset = (x * 37 + y * 73 + x * y * 13) % 13;
                    const bool lines = (x + y / 3) % 26 < 2 || (y + x / 12) % 29 < 2;
                    const auto base = lines ? 52 : 160 + (x + y) % 37;
                    image.pixels.push_back({static_cast<std::uint8_t>(base + offset),
                                            static_cast<std::uint8_t>(base + offset + 12),
                                            static_cast<std::uint8_t>(base + offset + 25), 255});
                }
                else
                {
                    const bool subject = x >= 57 && x < 104 && y >= 24 && y < 74;
                    image.pixels.push_back(subject ? Pixel{52, 132, 244, 255} : Pixel{155, 100, 33, 255});
                }
            }
        }
        return image;
    }

    void compare(const Image &original, const std::string &name, const fs::path &output)
    {
        const auto energy = calculate_energy(original);
        save_png(original, output / (name + "_original.png"));
        constexpr std::size_t calls_per_trial = 16;
        for (const bool vertical : {true, false})
        {
            const auto greedy = vertical ? find_vertical_greedy : find_horizontal_greedy;
            const auto dp = vertical ? find_vertical_dp : find_horizontal_dp;
            const auto g = greedy(energy, {});
            const auto d = dp(energy, {});
            const auto timed = [&](auto find)
            {
                const auto start = Clock::now();
                for (std::size_t i = 0; i < calls_per_trial; ++i)
                    result_sink = find(energy, {}).energy;
                return elapsed_ms(start) / calls_per_trial;
            };
            const auto time = medians([&] { return timed(greedy); }, [&] { return timed(dp); });
            std::size_t matches = 0;
            for (std::size_t i = 0; i < g.indices.size(); ++i)
                matches += (g.indices[i] == d.indices[i]);
            std::cout << name << ',' << (vertical ? "vertical" : "horizontal") << ','
                      << g.energy << ',' << d.energy << ',' << matches << ',' << g.indices.size() << ','
                      << time[0] << ',' << time[1] << '\n';
            const auto axis = vertical ? "_vertical_" : "_horizontal_";
            save_png(highlight_greedy_seam(original, g, vertical), output / (name + axis + "greedy.png"));
            save_png(highlight_greedy_seam(original, {d.indices, d.energy}, vertical),
                     output / (name + axis + "dp.png"));
        }

        const auto width = original.width - std::max<std::size_t>(1, original.width / 8);
        const auto height = original.height - std::max<std::size_t>(1, original.height / 16);
        auto g = original;
        auto d = original;
        const auto timed_greedy = [&]
        {
            g = original;
            const auto start = Clock::now();
            resize_greedy(g, width, height);
            return elapsed_ms(start);
        };
        const auto timed_dp = [&]
        {
            d = original;
            const auto start = Clock::now();
            resize_dp(d, width, height);
            return elapsed_ms(start);
        };
        const auto time = medians(timed_greedy, timed_dp);
        std::cout << name << ",resize," << original.width << 'x' << original.height << ','
                  << width << 'x' << height << ',' << time[0] << ',' << time[1] << ','
                  << (g.pixels == d.pixels ? "identical" : "different") << '\n';
        save_png(g, output / (name + "_greedy.png"));
        save_png(d, output / (name + "_dp.png"));
    }
} // namespace

int main(int argc, char *argv[])
{
    try
    {
        if (argc < 2 || argc > 3)
        {
            std::cerr << "Usage: greedy-benchmark OUTPUT_DIRECTORY [PHOTO]\n";
            return 2;
        }
        const auto output = fs::u8path(argv[1]);
        fs::create_directories(output);
        std::cout << std::fixed << std::setprecision(5)
                  << "Seam rows: scene,axis,greedy energy,DP energy,coincident positions,length,greedy ms,DP ms\n"
                  << "Resize rows: scene,resize,input,target,greedy ms,DP ms,pixel equality\n";
        compare(fixture(false), "flat", output);
        compare(fixture(true), "grid", output);
        if (argc == 3)
            compare(load_image(fs::u8path(argv[2])), "photo", output);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
