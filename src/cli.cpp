#include "cli.h"
#include "resize.h"
#include "dp.h"
#include "greedy.h"

#include <charconv>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <optional>
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

        std::filesystem::path image_input_directory(const std::filesystem::path &executable_directory)
        {
            // The local build places the executable in build/Release (or directly in build).
            const auto build_directory = executable_directory.filename() == "build"
                                             ? executable_directory
                                             : executable_directory.parent_path();
            const auto project_directory = build_directory.parent_path();
            if (build_directory.filename() == "build" &&
                std::filesystem::is_regular_file(project_directory / "CMakeLists.txt"))
                return project_directory / "input";
            return executable_directory / "input";
        }

        std::filesystem::path output_path(const std::filesystem::path &directory,
                                          const std::filesystem::path &input,
                                          std::size_t width, std::size_t height, const std::string &algorithm)
        {
            std::filesystem::create_directories(directory);
            const auto stem = input.stem().u8string() + "_" + algorithm + "_" +
                              std::to_string(width) + "x" + std::to_string(height);
            auto candidate = directory / std::filesystem::u8path(stem + ".png");
            for (std::size_t number = 1;
                 std::filesystem::exists(candidate) ||
                 (algorithm == "greedy" && std::filesystem::exists(candidate.parent_path() /
                     std::filesystem::u8path(candidate.stem().u8string() + "_steps"))); ++number)
                candidate = directory / std::filesystem::u8path(stem + "_" + std::to_string(number) + ".png");
            return candidate;
        }

        // A failed or cancelled run removes only the frames it created.
        struct SeamFrames
        {
            std::filesystem::path directory;
            std::vector<std::filesystem::path> files;
            std::size_t digits;
            bool saved = false;

            SeamFrames(const std::filesystem::path &destination, std::size_t total)
                : directory(destination.parent_path() /
                            std::filesystem::u8path(destination.stem().u8string() + "_steps")),
                  digits(std::to_string(total).size())
            {
                if (!std::filesystem::create_directory(directory))
                    throw std::runtime_error("Seam frame folder already exists.");
            }

            ~SeamFrames()
            {
                if (saved)
                    return;
                std::error_code ignored;
                for (const auto &file : files)
                    std::filesystem::remove(file, ignored);
                std::filesystem::remove(directory, ignored);
            }

            void write(const Image &image, const GreedySeam &seam, bool vertical)
            {
                const auto number = std::to_string(files.size() + 1);
                files.push_back(directory / ("step_" + std::string(digits - number.size(), '0') +
                                             number + (vertical ? "_vertical.png" : "_horizontal.png")));
                save_png(highlight_greedy_seam(image, seam, vertical), files.back());
            }
        };
    } // namespace

    int run_interactive(std::istream &input, std::ostream &output,
                        const std::filesystem::path &executable_directory,
                        const std::function<int()> &poll_key)
    {
        const auto input_directory = image_input_directory(executable_directory);
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
                          "  3. Greedy\n  0. Exit\n";
                const auto choice = read_line(input, output, "Algorithm: ");
                if (choice == "0")
                    break;
                if (choice != "1" && choice != "2" && choice != "3")
                {
                    output << "Please select 1, 2, 3, or 0.\n";
                    continue;
                }
                Image image;
                std::filesystem::path source;
                output << "Input folder: " << input_directory.u8string() << '\n';
                while (true)
                {
                    auto name = read_line(input, output, "Image file (PNG/JPEG/BMP/GIF/TIFF): ");
                    if (name.size() >= 2 && ((name.front() == '"' && name.back() == '"') ||
                                             (name.front() == '\'' && name.back() == '\'')))
                        name = name.substr(1, name.size() - 2);
                    try
                    {
                        const auto path = std::filesystem::u8path(name);
                        source = path.is_absolute() ? path : input_directory / path;
                        if (!std::filesystem::is_regular_file(source))
                            throw std::runtime_error("No readable image file at " + source.u8string());
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
                const auto total = image.width - width + image.height - height;
                const bool greedy = choice == "3";
                bool record_frames = false;
                if (greedy && total > 0)
                {
                    while (true)
                    {
                        const auto answer = read_line(input, output, "Save a PNG frame for every seam? [y/N]: ");
                        record_frames = answer == "y" || answer == "Y";
                        if (record_frames || answer.empty() || answer == "n" || answer == "N")
                            break;
                        output << "Enter y or n.\n";
                    }
                }
                try
                {
                    const auto destination = output_path(executable_directory / "output", source, width, height,
                                                         greedy ? "greedy" : choice == "2" ? "dp" : "graph_cut");
                    std::optional<SeamFrames> frames;
                    if (record_frames)
                        frames.emplace(destination, total);
                    output << "Resizing with " << (greedy ? "Greedy" : choice == "2" ? "Dynamic Programming" : "Graph Cut")
                           << ". Large images may take some time.\n";
                    const auto resize = choice == "2" ? resize_dp : resize_graph_cut;
                    const auto start = std::chrono::steady_clock::now();
                    std::atomic<std::size_t> completed{0};
                    std::atomic<bool> abort_requested{false};
                    const bool controls = console_keys || bool(poll_key);
                    if (controls)
                        output << "Press A to request abort, then Y or N (no Enter needed).\n";
                    const Progress progress = [&](std::size_t done, std::size_t)
                    { completed.store(done, std::memory_order_relaxed); };
                    const CancelCheck cancelled = [&]
                    { return abort_requested.load(std::memory_order_relaxed); };
                    GreedyStep observe;
                    if (frames)
                    {
                        observe = [&](const Image &current, const GreedySeam &seam, bool vertical)
                        { frames->write(current, seam, vertical); };
                    }
                    auto work = std::async(std::launch::async, [&]
                    {
                        if (greedy)
                            resize_greedy(image, width, height, progress, cancelled, observe);
                        else
                            resize(image, width, height, progress, cancelled);
                    });
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
                    if (frames)
                    {
                        frames->saved = true;
                        output << "Seam frames: " << frames->directory.u8string() << '\n';
                    }
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
