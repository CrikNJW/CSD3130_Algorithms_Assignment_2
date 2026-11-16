#pragma once
#include <filesystem>
#include <iosfwd>
#include <functional>

namespace seam_carver {
int run_interactive(std::istream& input, std::ostream& output,
                    const std::filesystem::path& executable_directory,
                    const std::function<int()>& poll_key = {});
}
