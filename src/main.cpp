#include <iostream>
#include <string_view>

int main(int argc, char* argv[])
{
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        std::cout << "Seam Carver\nUsage: seam-carver --help\n";
        return 0;
    }

    std::cerr << "Invalid arguments. Run seam-carver --help for usage.\n";
    return 2;
}
