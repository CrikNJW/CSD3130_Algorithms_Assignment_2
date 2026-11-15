cmake_minimum_required(VERSION 3.20)
project(SeamCarver LANGUAGES CXX)

add_executable(seam-carver src/main.cpp)
target_compile_features(seam-carver PRIVATE cxx_std_17)

# Use the static MSVC runtime so the starter executable runs on its own.
set_property(TARGET seam-carver PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(CTest)
if(BUILD_TESTING)
    add_test(NAME cli-help COMMAND seam-carver --help)
endif()

install(TARGETS seam-carver RUNTIME DESTINATION .)

set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_FILE_NAME "seam-carver-windows-x64")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
include(CPack)
