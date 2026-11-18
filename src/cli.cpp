#include "cli.h"
#include "resize.h"
#include "dp.h"
#include "greedy.h"

#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
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

        struct ConsoleMode
        {
            HANDLE handle;
            DWORD original_mode = 0;
            const bool enabled;

            ConsoleMode(DWORD stream, bool active, DWORD set, DWORD clear = 0)
                : handle(active ? GetStdHandle(stream) : INVALID_HANDLE_VALUE),
                  enabled(active && GetConsoleMode(handle, &original_mode) &&
                          SetConsoleMode(handle, (original_mode | set) & ~clear))
            {
            }

            ~ConsoleMode()
            {
                if (enabled)
                    SetConsoleMode(handle, original_mode);
            }
        };

        void heading(std::ostream &output, std::string_view title, bool styled)
        {
            output << '\n' << (styled ? "\x1b[96m" : "") << title
                   << (styled ? "\x1b[39m" : "") << '\n';
        }

        void print_image_row(std::ostream &output, const std::filesystem::path &image,
                             std::size_t index, bool selected = false)
        {
            output << (selected ? "> " : "  ") << index + 1 << ". ";
            if (selected)
                output << "\x1b[92m";
            output << image.filename().u8string();
            if (selected)
                output << "\x1b[39m";
            output << '\n';
        }

        std::filesystem::path choose_image(std::istream &input, std::ostream &output,
                                           const std::filesystem::path &directory,
                                           const std::vector<std::filesystem::path> &images,
                                           bool use_arrow_keys, const ConsoleMode &display)
        {
            if (use_arrow_keys && !images.empty())
            {
                CONSOLE_SCREEN_BUFFER_INFO screen{};
                GetConsoleScreenBufferInfo(display.handle, &screen);
                const auto available_rows = std::max(1, screen.srWindow.Bottom - screen.srWindow.Top - 2);
                const auto visible_rows = std::min(images.size(), static_cast<std::size_t>(available_rows));
                if (!display.enabled)
                    for (std::size_t index = 0; index < images.size(); ++index)
                        print_image_row(output, images[index], index);
                output << "  Up/Down: Move   Enter: Select   P: Enter path   Q: Quit\n";
                std::size_t selected = 0;
                std::size_t first_visible = 0;
                bool redraw = false;
                while (true)
                {
                    if (display.enabled)
                    {
                        if (selected < first_visible)
                            first_visible = selected;
                        if (selected >= first_visible + visible_rows)
                            first_visible = selected - visible_rows + 1;

                        if (redraw)
                            output << "\x1b[" << visible_rows << 'F';
                        // Keep each filename on one terminal row, even when it is wider than the window.
                        output << "\x1b[?7l";
                        for (std::size_t index = first_visible; index < first_visible + visible_rows; ++index)
                        {
                            output << "\x1b[2K";
                            print_image_row(output, images[index], index, index == selected);
                        }
                        output << "\x1b[2K";
                    }
                    output << "\r  Image " << selected + 1 << " of " << images.size() << "  ";
                    if (display.enabled)
                        output << "\x1b[?7h";
                    output << std::flush;
                    redraw = true;
                    const int key = _getch();
                    if (key == '\r')
                    {
                        output << '\n';
                        return images[selected];
                    }
                    if (key == 'q' || key == 'Q')
                        throw Cancelled{};
                    if (key == 'p' || key == 'P')
                    {
                        output << '\n';
                        break;
                    }
                    if (key == 0 || key == 224)
                    {
                        const int arrow = _getch();
                        if (arrow == 72 && selected > 0) // Up
                            --selected;
                        if (arrow == 80 && selected + 1 < images.size()) // Down
                            ++selected;
                    }
                }
            }

            auto name = read_line(input, output, "Image file (PNG/JPEG/BMP/GIF/TIFF): ");
            if (name.size() >= 2 && ((name.front() == '"' && name.back() == '"') ||
                                     (name.front() == '\'' && name.back() == '\'')))
                name = name.substr(1, name.size() - 2);
            const auto path = std::filesystem::u8path(name);
            return path.is_absolute() ? path : directory / path;
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

        struct Algorithm
        {
            const char *name;
            const char *suffix;
            decltype(&resize_dp) resize;
        };

        constexpr Algorithm algorithms[]{
            {"Graph Cut", "graph_cut", resize_graph_cut},
            {"Dynamic Programming", "dp", resize_dp},
            {"Greedy", "greedy", nullptr}};

        struct SeamStats
        {
            std::vector<std::size_t> first_indices;
            double first_energy = 0;
            double total_energy = 0;
            std::size_t removed_pixels = 0;
        };

        struct SavedResize
        {
            std::filesystem::path path;
            double seconds;
            SeamStats seams;
        };

        SavedResize resize_and_save(Image &image, std::size_t width, std::size_t height,
                                    const Algorithm &algorithm, bool record_frames, bool compare,
                                    const std::filesystem::path &directory, const std::filesystem::path &source,
                                    std::ostream &output, const std::function<int()> &key, const ConsoleMode &display)
        {
            const auto total = image.width - width + image.height - height;
            const auto destination = output_path(directory, source, width, height, algorithm.suffix);
            std::optional<SeamFrames> frames;
            if (record_frames)
                frames.emplace(destination, total);
            const auto start = std::chrono::steady_clock::now();
            std::atomic<std::size_t> completed{0};
            std::atomic<bool> abort_requested{false};
            CONSOLE_SCREEN_BUFFER_INFO screen{};
            const bool live = GetConsoleScreenBufferInfo(display.handle, &screen);
            const auto max_columns = std::max(1, screen.srWindow.Right - screen.srWindow.Left);
            const Progress progress = [&](std::size_t done, std::size_t)
            { completed.store(done, std::memory_order_relaxed); };
            const CancelCheck cancelled = [&]
            { return abort_requested.load(std::memory_order_relaxed); };
            SeamStats seams;
            SeamObserver observe;
            if (compare)
            {
                observe = [&](const std::vector<std::size_t> &indices, double energy)
                {
                    if (seams.first_indices.empty())
                    {
                        seams.first_indices = indices;
                        seams.first_energy = energy;
                    }
                    seams.total_energy += energy;
                    seams.removed_pixels += indices.size();
                };
            }
            GreedyStep observe_greedy;
            if (!algorithm.resize && (frames || observe))
            {
                observe_greedy = [&](const Image &current, const GreedySeam &seam, bool vertical)
                {
                    if (frames)
                        frames->write(current, seam, vertical);
                    if (observe)
                        observe(seam.indices, seam.energy);
                };
            }
            auto work = std::async(std::launch::async, [&]
            {
                const auto resize_start = std::chrono::steady_clock::now();
                if (algorithm.resize)
                    algorithm.resize(image, width, height, progress, cancelled, observe);
                else
                    resize_greedy(image, width, height, progress, cancelled, observe_greedy);
                return std::chrono::duration<double>(std::chrono::steady_clock::now() - resize_start).count();
            });
            constexpr std::string_view spinner = "|/-\\";
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
                    output << "Y\n  Stopping...\n" << std::flush;
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
                if (live)
                {
                    const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                    const auto done = completed.load(std::memory_order_relaxed);
                    const auto fraction = total == 0 ? 1.0 : static_cast<double>(done) / total;
                    constexpr std::size_t bar_width = 16;
                    const auto filled = static_cast<std::size_t>(fraction * bar_width);
                    std::ostringstream status;
                    status << "  " << spinner[frame++ % spinner.size()] << ' ';
                    if (max_columns >= 60)
                        status << '[' << std::string(filled, '#') << std::string(bar_width - filled, '-') << "] ";
                    status << std::setw(3) << static_cast<int>(fraction * 100) << "%  "
                           << done << '/' << total << " seams  " << std::fixed << std::setprecision(1) << seconds << 's';
                    const auto line = status.str().substr(0, max_columns);
                    output << '\r' << line
                           << std::string(line_length > line.size() ? line_length - line.size() : 0, ' ')
                           << std::flush;
                    line_length = line.size();
                }
            } while (confirming || work.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready);
            if (line_length != 0)
                clear_line();
            const auto elapsed = work.get(); // Propagate worker errors before saving.
            save_png(image, destination);
            output << "  " << (display.enabled ? "\x1b[92m" : "") << "Completed"
                   << (display.enabled ? "\x1b[39m" : "") << "  " << total << " seams  |  "
                   << std::fixed << std::setprecision(3) << elapsed << " s\n";
            if (frames)
            {
                frames->saved = true;
                output << "  Seam frames: " << frames->directory.u8string() << '\n';
            }
            return {destination, elapsed, std::move(seams)};
        }

        struct ComparisonResult
        {
            const Algorithm *algorithm;
            SavedResize saved;
            Image image;
        };

        Image side_by_side(const Image &left, const Image &right)
        {
            constexpr std::size_t gutter = 8;
            Image combined{left.width + gutter + right.width, left.height, {}};
            combined.pixels.resize(combined.width * combined.height, Pixel{48, 48, 48, 255});
            for (std::size_t row = 0; row < combined.height; ++row)
            {
                auto start = combined.pixels.begin() + row * combined.width;
                std::copy_n(left.pixels.begin() + row * left.width, left.width, start);
                std::copy_n(right.pixels.begin() + row * right.width, right.width, start + left.width + gutter);
            }
            return combined;
        }

        void print_pair(std::ostream &output, const ComparisonResult &dp, const ComparisonResult &other,
                        const std::filesystem::path &source, bool first_vertical, bool styled)
        {
            const auto name = other.algorithm->name;
            heading(output, std::string("Dynamic Programming vs ") + name, styled);
            const auto &dp_seams = dp.saved.seams;
            const auto &other_seams = other.saved.seams;
            if (dp_seams.first_indices.empty())
                output << "  Seams: No removals at the requested dimensions.\n";
            else
            {
                const auto length = dp_seams.first_indices.size();
                std::size_t shared = 0;
                for (std::size_t position = 0; position < length; ++position)
                    shared += dp_seams.first_indices[position] == other_seams.first_indices[position];
                output << "  First " << (first_vertical ? "vertical" : "horizontal") << " seam on the original\n"
                       << "    Shared positions: " << shared << '/' << length << " ("
                       << std::setprecision(2) << 100.0 * shared / length << "%)\n"
                       << "    Seam energy: DP " << dp_seams.first_energy << " | " << name << ' ' << other_seams.first_energy
                       << "\n  Mean energy per removed pixel (all seams): DP " << dp_seams.total_energy / dp_seams.removed_pixels
                       << " | " << name << ' ' << other_seams.total_energy / other_seams.removed_pixels << '\n';
            }

            const auto &slower = dp.saved.seconds >= other.saved.seconds ? dp : other;
            const auto &faster = dp.saved.seconds >= other.saved.seconds ? other : dp;
            if (faster.saved.seconds > 0 && slower.saved.seconds > faster.saved.seconds)
                output << "  Running time: " << slower.algorithm->name << " took " << std::setprecision(2)
                       << slower.saved.seconds / faster.saved.seconds << "x as long as " << faster.algorithm->name << ".\n";
            else
                output << "  Running time: DP " << std::setprecision(3) << dp.saved.seconds * 1000 << " ms | "
                       << name << ' ' << other.saved.seconds * 1000 << " ms\n";

            try
            {
                const auto path = output_path(dp.saved.path.parent_path(), source, dp.image.width, dp.image.height,
                                              std::string("dp_vs_") + other.algorithm->suffix);
                save_png(side_by_side(dp.image, other.image), path);
                output << "  Visual quality (DP left, " << name << " right)\n    " << path.filename().u8string() << '\n';
            }
            catch (const std::exception &error)
            {
                output << "  Side-by-side PNG could not be saved: " << error.what() << '\n'
                       << "  Visual quality: Open the individual PNGs above.\n";
            }
        }

        void print_comparison(std::ostream &output, const std::vector<ComparisonResult> &results,
                              const std::filesystem::path &source, std::size_t width, std::size_t height,
                              bool first_vertical, bool styled)
        {
            std::ostringstream title;
            title << "Comparison at " << width << " x " << height << " (" << results.size() << " of 3 completed)";
            heading(output, title.str(), styled);
            if (results.empty())
            {
                output << "  No PNGs were saved.\n";
                return;
            }

            output << "  " << std::left << std::setw(22) << "Algorithm" << std::right << std::setw(12) << "Resize (ms)"
                   << "  Output PNG\n"
                   << "  " << std::string(46, '-') << '\n';
            output << std::fixed << std::setprecision(3);
            for (const auto &result : results)
                output << "  " << std::left << std::setw(22) << result.algorithm->name << std::right << std::setw(12)
                       << result.saved.seconds * 1000 << "  " << result.saved.path.filename().u8string() << '\n';
            output << "\n  Saved to: " << results.front().saved.path.parent_path().u8string() << '\n';

            const auto dp = std::find_if(results.begin(), results.end(), [](const ComparisonResult &result)
            { return result.algorithm->resize == resize_dp; });
            if (dp == results.end())
                return;
            for (auto other = results.rbegin(); other != results.rend(); ++other)
                if (other->algorithm != dp->algorithm)
                    print_pair(output, *dp, *other, source, first_vertical, styled);
        }
    } // namespace

    int run_interactive(std::istream &input, std::ostream &output,
                        const std::filesystem::path &executable_directory,
                        const std::function<int()> &poll_key)
    {
        const auto input_directory = image_input_directory(executable_directory);
        const bool console_keys = &input == &std::cin && _isatty(_fileno(stdin));
        const bool console_output = &output == &std::cout && _isatty(_fileno(stdout));
        const bool use_arrow_keys = console_keys && console_output;
        // Classic Command Prompt can suspend console I/O while a mouse selection is active.
        const ConsoleMode keyboard(STD_INPUT_HANDLE, console_keys, ENABLE_EXTENDED_FLAGS, ENABLE_QUICK_EDIT_MODE);
        const ConsoleMode display(STD_OUTPUT_HANDLE, console_output, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        const std::function<int()> key = [&]() -> int
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
        heading(output, "SEAM CARVER", display.enabled);
        output << "Shrink images using content-aware seam removal. Enter Q at any prompt to quit.\n";
        try
        {
            while (true)
            {
                heading(output, "Choose an algorithm", display.enabled);
                output << "  1. Graph Cut\n  2. Dynamic Programming\n  3. Greedy\n"
                          "  4. Compare all three\n\n  0. Exit\n\n";
                std::string choice;
                while (true)
                {
                    choice = read_line(input, output, "Algorithm [0-4]: ");
                    if (choice.size() == 1 && choice.front() >= '0' && choice.front() <= '4')
                        break;
                    if (!choice.empty())
                        output << "Please select 1, 2, 3, 4, or 0.\n";
                }
                if (choice == "0")
                    break;
                const auto selection = static_cast<std::size_t>(choice.front() - '1');
                const bool compare = choice == "4";
                Image image;
                std::filesystem::path source;
                heading(output, "Choose an image", display.enabled);
                output << "  Input folder: " << input_directory.u8string() << '\n';
                std::vector<std::filesystem::path> images;
                try
                {
                    images = list_supported_images(input_directory);
                    if (images.empty())
                        output << "No supported images found in the input folder.\n";
                    if (!use_arrow_keys)
                        for (std::size_t index = 0; index < images.size(); ++index)
                            print_image_row(output, images[index], index);
                }
                catch (const std::exception &error)
                {
                    output << "Could not list images: " << error.what() << '\n';
                }
                while (true)
                {
                    try
                    {
                        source = choose_image(input, output, input_directory, images, use_arrow_keys, display);
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
                heading(output, "Set target dimensions", display.enabled);
                output << "  " << source.filename().u8string() << "  |  Original: "
                       << image.width << " x " << image.height << " px\n";
                const auto height = dimension(input, output, "height", image.height);
                const auto width = dimension(input, output, "width", image.width);
                const auto total = image.width - width + image.height - height;
                bool record_frames = false;
                if (choice == "3" && total > 0)
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
                std::vector<ComparisonResult> results;
                if (compare)
                    results.reserve(std::size(algorithms));
                const auto first_algorithm = compare ? 0 : selection;
                const auto last_algorithm = compare ? std::size(algorithms) : selection + 1;
                heading(output, "Resize to " + std::to_string(width) + " x " + std::to_string(height) + " px", display.enabled);
                if (use_arrow_keys)
                    output << "  A: Abort (then Y or N)\n\n";
                for (auto index = first_algorithm; index < last_algorithm; ++index)
                {
                    const auto &algorithm = algorithms[index];
                    output << "  ";
                    if (compare)
                        output << '[' << index + 1 << '/' << last_algorithm << "] ";
                    output << algorithm.name << '\n' << std::flush;
                    try
                    {
                        Image comparison_image;
                        if (compare)
                            comparison_image = image;
                        auto saved = resize_and_save(compare ? comparison_image : image, width, height,
                                                     algorithm, record_frames, compare, executable_directory / "output", source,
                                                     output, key, display);
                        if (compare)
                            results.push_back({&algorithm, std::move(saved), std::move(comparison_image)});
                        else
                            output << "  Saved: " << saved.path.u8string() << '\n';
                    }
                    catch (const ResizeAborted &)
                    {
                        if (compare)
                            output << "  Comparison stopped during " << algorithm.name << ".\n";
                        else
                            output << "  Resizing aborted. No PNG was saved.\n";
                        break;
                    }
                    catch (const std::exception &error)
                    {
                        output << "  Resize/save failed: " << error.what() << '\n';
                    }
                }
                if (compare)
                    print_comparison(output, results, source, width, height, image.width > width, display.enabled);
            }
        }
        catch (const Cancelled &)
        {
            output << std::endl;
        }

        return 0;
    }
} // namespace seam_carver
