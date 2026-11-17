#include "cli.h"
#include "resize.h"

#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace seam_carver;
namespace fs = std::filesystem;

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

template<class Action> void rejects(Action action)
{
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Invalid input was accepted");
}

int main()
{
    try {
        Image original{3, 3, {}};
        for (std::uint8_t i = 0; i < 9; ++i) original.pixels.push_back({i, 0, 0, std::uint8_t(200 + i)});
        const auto energy = calculate_energy(original);
        require(std::abs(energy[1][1] - std::sqrt(40.0)) < 1e-10, "Incorrect interior energy");
        require(std::abs(energy[0][0] - std::sqrt(10.0)) < 1e-10, "Incorrect border energy");
        require(calculate_energy(Image{1, 1, {{1, 2, 3, 255}}})[0][0] == 0,
                "Incorrect singleton energy");
        auto vertical = original;
        remove_vertical_seam(vertical, {0, 1, 2});
        const std::vector<Pixel> vertical_expected{
            original.pixels[1], original.pixels[2], original.pixels[3],
            original.pixels[5], original.pixels[6], original.pixels[7]};
        require(vertical.width == 2 && vertical.height == 3 && vertical.pixels == vertical_expected,
                "Vertical removal corrupted pixels");
        auto horizontal = original;
        remove_horizontal_seam(horizontal, {0, 1, 2});
        const std::vector<Pixel> horizontal_expected{
            original.pixels[3], original.pixels[1], original.pixels[2],
            original.pixels[6], original.pixels[7], original.pixels[5]};
        require(horizontal.width == 3 && horizontal.height == 2 && horizontal.pixels == horizontal_expected,
                "Horizontal removal corrupted pixels");
        rejects([&] { remove_vertical_seam(original, {0, 2, 0}); });
        rejects([&] { remove_horizontal_seam(original, {0, 1, 3}); });
        rejects([&] { remove_vertical_seam(original, {0}); });
        rejects([&] { resize_graph_cut(original, 4, 2); });
        rejects([&] { resize_graph_cut(original, 0, 2); });
        rejects([] { calculate_energy({2, 2, {}}); });
        std::size_t callbacks = 0;
        auto resized = original;
        resize_graph_cut(resized, 1, 1, [&](std::size_t done, std::size_t total) {
            require(total == 4 && done == callbacks, "Incorrect resize progress");
            ++callbacks;
        });
        require(resized.width == 1 && resized.height == 1 && callbacks == 5, "Resize did not finish");
        rejects([&] { remove_horizontal_seam(resized, {0}); });
        rejects([&] { remove_vertical_seam(resized, {0}); });
        auto unchanged = original;
        resize_graph_cut(unchanged, 3, 3);
        require(unchanged.pixels == original.pixels, "Same-size resize changed pixels");
        bool aborted = false;
        try { resize_graph_cut(unchanged, 1, 1, {}, [] { return true; }); }
        catch (const ResizeAborted&) { aborted = true; }
        require(aborted && unchanged.pixels == original.pixels, "Early cancellation changed the image");
        // 100 row-validation and 100 graph-construction checkpoints occur
        // first; checkpoint 300 is inside the max-flow search.
        std::size_t checkpoints = 0;
        aborted = false;
        try {
            find_vertical_graph_cut(EnergyMap(100, std::vector<double>(100, 1)),
                                    [&] { return ++checkpoints == 300; });
        } catch (const ResizeAborted&) { aborted = true; }
        require(aborted && checkpoints == 300, "Cannot cancel during max-flow");

        const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto directory = fs::current_path() / ("image-cli-test-" + unique);
        const auto exe_directory = directory / "fake executable directory";
        fs::create_directories(exe_directory);
        // Unicode plus spaces verifies native Windows paths and CLI UTF-8 conversion.
        const auto source = directory / fs::u8path("sample \xE5\x9B\xBE image.png");
        save_png(original, source);
        const auto decoded = load_image(source);
        require(decoded.width == 3 && decoded.height == 3 && decoded.pixels == original.pixels,
                "PNG round-trip did not preserve BGRA pixels");
        const auto corrupt = directory / "corrupt.png";
        std::ofstream(corrupt) << "This is not an image.";
        std::ostringstream commands;
        commands << "bad\n3\n1\nmissing-file.png\n\"" << corrupt.u8string()
                 << "\"\n\"" << source.u8string() << "\"\n0\n4\n1.5\n2\n-1\n2junk\n2\n"
                 << "1\n" << source.u8string() << "\n2\n2\n0\n";
        std::istringstream input(commands.str());
        std::ostringstream output;
        require(run_interactive(input, output, exe_directory) == 0, "Interactive session failed");
        require(output.str().find("Invalid image:") != std::string::npos, "Missing image validation");
        require(output.str().find("not implemented yet") != std::string::npos, "Missing unavailable choice message");
        require(output.str().find("Saved:") != std::string::npos, "CLI did not save");
        const auto base = source.stem().u8string() + "_graph_cut_2x2";
        const auto first = exe_directory / "output" / fs::u8path(base + ".png");
        const auto second = exe_directory / "output" / fs::u8path(base + "_1.png");
        const auto first_image = load_image(first);
        require(first_image.width == 2 && first_image.height == 2, "Wrong CLI output dimensions");
        require(load_image(second).pixels == first_image.pixels, "Repeated save is inconsistent");
        require(!fs::exists(directory / "output"), "Output used working directory instead of executable directory");
        require(load_image(source).pixels == original.pixels, "Input image was changed");
        const auto third = exe_directory / "output" / fs::u8path(base + "_2.png");
        for (bool confirm : {true, false}) {
            std::istringstream session("1\n" + source.u8string() + "\n2\n2\n0\nexit-marker\n");
            std::ostringstream messages;
            const std::string keys = confirm ? "axy" : "an";
            std::size_t next_key = 0;
            const auto result = run_interactive(session, messages, exe_directory, [&]() -> int {
                return next_key < keys.size() ? keys[next_key++] : -1;
            });
            require(messages.str().find("Abort resizing? [Y/N]") != std::string::npos,
                    "Abort confirmation was not shown");
            if (confirm) {
                require(messages.str().find("Resizing aborted.") != std::string::npos,
                        "Confirmed abort did not return to menu");
                require(messages.str().find("Saved:") == std::string::npos && !fs::exists(third),
                        "Aborted resize saved output");
            } else {
                require(messages.str().find("Saved:") != std::string::npos && fs::exists(third),
                        "Declining abort did not finish resizing");
            }
            std::string remaining;
            std::getline(session, remaining);
            require(result == 0 && remaining == "exit-marker",
                    "Could not exit via menu after resize/abort");
        }
        for (const auto& text : {std::string(), std::string("1\nq\n"), std::string("1\n") + source.u8string() + "\nq\n"}) {
            std::istringstream cancelled(text);
            std::ostringstream messages;
            require(run_interactive(cancelled, messages, exe_directory) == 0, "Cancellation failed");
        }
        // Remove only this test's known files and then its empty directories.
        for (const auto& file : {source, corrupt, first, second, third}) fs::remove(file);
        fs::remove(exe_directory / "output");
        fs::remove(exe_directory);
        fs::remove(directory);
        std::cout << "Image pixels, energy, resizing, PNG I/O, and interactive CLI checks passed.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
