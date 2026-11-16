#include <iostream>
#include <string_view>
#include "graph_cut.h"
#include "cli.h"
#define NOMINMAX
#include <windows.h>
#include <stdexcept>

std::filesystem::path executable_directory()
{
    std::wstring buffer(512, L'\0');
    while (true)
    {
        const auto size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (size == 0)
            throw std::runtime_error("Cannot locate executable directory.");
        if (size < buffer.size())
        {
            buffer.resize(size);
            return std::filesystem::path(buffer).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
}

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        SetConsoleCP(CP_UTF8);
        SetConsoleOutputCP(CP_UTF8);
        try
        {
            return seam_carver::run_interactive(std::cin, std::cout, executable_directory());
        }
        catch (const std::exception &error)
        {
            std::cerr << "Error: " << error.what() << '\n';
            return 1;
        }
    }
    if (argc == 2 && std::string_view(argv[1]) == "--help")
    {
        std::cout << "Seam Carver\nUsage: seam-carver [--help | --graph-cut-demo]\n"
                     "  No arguments: interactive algorithm, image, and target-size prompts.\n"
                     "  Saves PNGs to output beside this executable. Shrinking only.\n"
                     "  --graph-cut-demo  Find vertical and horizontal seams on a sample energy map.\n";
        return 0;
    }

    if (argc == 2 && std::string_view(argv[1]) == "--graph-cut-demo")
    {
        const seam_carver::EnergyMap energy{
            {1, 9, 9, 9}, {9, 1, 9, 9}, {9, 9, 1, 9}, {9, 9, 9, 1}};
        const auto print = [](const char *label, const seam_carver::Seam &seam)
        {
            std::cout << label << " (zero-based):";
            for (auto index : seam.indices)
                std::cout << ' ' << index;
            std::cout << "\nEnergy: " << seam.energy
                      << "; min-cut/max-flow: " << seam.cut_capacity << '\n';
        };
        print("Vertical columns", seam_carver::find_vertical_graph_cut(energy));
        print("Horizontal rows", seam_carver::find_horizontal_graph_cut(energy));
        return 0;
    }

    std::cerr << "Invalid arguments. Run seam-carver --help for usage.\n";
    return 2;
}
