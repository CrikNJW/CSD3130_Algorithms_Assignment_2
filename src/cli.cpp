#include "cli.h"
#include "resize.h"
#include "dp.h"

#include <charconv>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <atomic>
#include <future>
#include <conio.h>
#include <io.h>
#include <cstdio>
#include <thread>
#include <sstream>

namespace seam_carver
{
    namespace
    {
        struct Cancelled
        {
        };

        std::string read_line(std::istream &input, std::ostream &output, const std::string &prompt)
        {
            output << prompt << std::flush;
            std::string line;
            if (!std::getline(input, line))
                throw Cancelled{};
            const auto first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
                return {};
            line = line.substr(first, line.find_last_not_of(" \t\r\n") - first + 1);
            if (line == "q" || line == "Q")
                throw Cancelled{};
            return line;
        }

        std::size_t dimension(std::istream &input, std::ostream &output,
                              const char *label, std::size_t maximum)
        {
            while (true)
            {
                const auto line = read_line(input, output, std::string("Target ") + label + " (1-" + std::to_string(maximum) + "): ");
                std::size_t value = 0;
                const auto result = std::from_chars(line.data(), line.data() + line.size(), value);
                if (result.ec == std::errc{} && result.ptr == line.data() + line.size() &&
                    value > 0 && value <= maximum)
                    return value;
                output << "Enter a whole number from 1 to " << maximum << ". Enlargement is not supported.\n";
            }
        }

        std::filesystem::path output_path(const std::filesystem::path &directory,
                                          const std::filesystem::path &input,
                                          std::size_t width, std::size_t height, const std::string &algorithm)
        {
            std::filesystem::create_directories(directory);
            const auto stem = input.stem().u8string() + "_" + algorithm + "_" +
                              std::to_string(width) + "x" + std::to_string(height);
            auto candidate = directory / std::filesystem::u8path(stem + ".png");
            for (std::size_t number = 1; std::filesystem::exists(candidate); ++number)
                candidate = directory / std::filesystem::u8path(stem + "_" + std::to_string(number) + ".png");
            return candidate;
        }
    } // namespace

    int run_interactive(std::istream &input, std::ostream &output,
                        const std::filesystem::path &executable_directory,
                        const std::function<int()> &poll_key)
    {
        const bool console_keys = &input == &std::cin && _isatty(_fileno(stdin));
        const auto key = [&]() -> int
        {
            if (poll_key)
                return poll_key();
            if (!console_keys || !_kbhit())
                return -1;
            const int value = _getch();
            if (value == 0 || value == 224)
            {
                _getch();
                return -1;
            }
            return value;
        };
        output << "Seam Carver\nShrink images using content-aware seam removal. Enter q at any prompt to quit.\n";
        try
        {
            while (true)
            {
                output << "\nChoose an algorithm:\n"
                          "  1. Graph Cut\n  2. Dynamic Programming\n"
                          "  3. Greedy (not implemented yet)\n  0. Exit\n";
                const auto choice = read_line(input, output, "Algorithm: ");
                if (choice == "0")
                    break;
                if (choice == "3")
                {
                    output << "That algorithm is not implemented yet. Please choose Graph Cut or Dynamic Programming.\n";
                    continue;
                }
                if (choice != "1" && choice != "2")
                {
                    output << "Please select 1, 2, 3, or 0.\n";
                    continue;
                }
                Image image;
                std::filesystem::path source;
                while (true)
                {
                    auto name = read_line(input, output, "Image file (PNG/JPEG/BMP/GIF/TIFF): ");
                    if (name.size() >= 2 && ((name.front() == '"' && name.back() == '"') ||
                                             (name.front() == '\'' && name.back() == '\'')))
                        name = name.substr(1, name.size() - 2);
                    try
                    {
                        source = std::filesystem::u8path(name);
                        if (!std::filesystem::is_regular_file(source))
                            throw std::runtime_error("The path is not a readable image file.");
                        image = load_image(source);
                        break;
                    }
                    catch (const std::exception &error)
                    {
                        output << "Invalid image: " << error.what() << "\nPlease try another file.\n";
                    }
                }
                output << "Loaded " << image.width << " x " << image.height << " (width x height).\n";
                const auto height = dimension(input, output, "height", image.height);
                const auto width = dimension(input, output, "width", image.width);
                try
                {
                    const auto destination = output_path(executable_directory / "output", source, width, height, choice == "2" ? "dp" : "graph_cut");
                    output << "Resizing with " << (choice == "2" ? "Dynamic Programming" : "Graph Cut") << ". Large images may take some time.\n";
                    const auto resize = choice == "2" ? resize_dp : resize_graph_cut;
                    const auto start = std::chrono::steady_clock::now();
                    const auto total = image.width - width + image.height - height;
                    std::atomic<std::size_t> completed{0};
                    std::atomic<bool> abort_requested{false};
                    const bool controls = console_keys || bool(poll_key);
                    if (controls)
                        output << "Press A to request abort, then Y or N (no Enter needed).\n";
                    auto work = std::async(std::launch::async, [&]
                                           { resize(image, width, height, [&](std::size_t done, std::size_t)
                                                              { completed.store(done, std::memory_order_relaxed); }, [&]
                                                              { return abort_requested.load(std::memory_order_relaxed); }); });
                    constexpr const char *spinner[] = {
                        u8"\u280B", u8"\u2819", u8"\u2839", u8"\u2838", u8"\u283C",
                        u8"\u2834", u8"\u2826", u8"\u2827", u8"\u2807", u8"\u280F"};
                    std::size_t frame = 0;
                    bool confirming = false;
                    std::size_t line_length = 0;
                    const auto clear_line = [&]
                    {
                        output << '\r' << std::string(line_length, ' ') << '\r';
                        line_length = 0;
                    };
                    do
                    {
                        const int pressed = key();
                        if (!confirming && (pressed == 'a' || pressed == 'A'))
                        {
                            confirming = true;
                            clear_line();
                            output << "Abort resizing? [Y/N]: " << std::flush;
                            line_length = 22;
                        }
                        else if (confirming && (pressed == 'y' || pressed == 'Y'))
                        {
                            abort_requested.store(true, std::memory_order_relaxed);
                            output << "Y\nAborting..." << std::flush;
                            work.get();            // Wait until the worker has safely unwound.
                            throw ResizeAborted{}; // Also discard a result that just finished.
                        }
                        else if (confirming && (pressed == 'n' || pressed == 'N'))
                        {
                            confirming = false;
                            output << "N\n";
                            line_length = 0;
                        }
                        if (confirming)
                        {
                            // Keep the question visible even if computation finishes.
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                            continue;
                        }
                        const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                        std::ostringstream status;
                        status << spinner[frame++ % 10] << " Seams: " << completed.load(std::memory_order_relaxed)
                               << '/' << total << " | " << std::fixed << std::setprecision(1)
                               << seconds << "s" << (controls ? " | [A] to abort" : "");
                        const auto line = status.str();
                        // A Braille frame is three UTF-8 bytes but one terminal column.
                        const auto columns = line.size() - 2;
                        output << '\r' << line
                               << std::string(line_length > columns ? line_length - columns : 0, ' ')
                               << std::flush;
                        line_length = columns;
                    } while (confirming || work.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready);
                    work.get(); // Propagate worker errors before saving.
                    clear_line();
                    output << "Seams removed: " << total << '/' << total << " | finished";
                    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                    save_png(image, destination);
                    output << "\nSaved: " << destination.u8string() << "\nResizing time: "
                           << std::fixed << std::setprecision(3) << elapsed << " seconds.\n";
                }
                catch (const ResizeAborted &)
                {
                    output << "\nResizing aborted. No output image saved. Returning to algorithm menu.\n";
                }
                catch (const std::exception &error)
                {
                    output << "\nResize/save failed: " << error.what() << '\n';
                }
            }
        }
        catch (const Cancelled &)
        {
            output << std::endl;
        }

        return 0;
    }
} // namespace seam_carver
