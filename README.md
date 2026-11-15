# Assignment 2 Seam Carver

The C++ starter provides a `--help` command for testing the build and release pipeline. Add seam-carving code to the `seam-carver` target and register algorithm tests with CTest as you implement it.

## Build and test on Windows

Use Visual Studio with the C++ workload and CMake available in your terminal.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure --no-tests=error
cpack --config build/CPackConfig.cmake -C Release -G ZIP -B build/packages
```

The package is `build/packages/seam-carver-windows-x64.zip`. If you add runtime dependencies such as image-processing DLLs, include them in the CMake install rules so CPack adds them to the ZIP.

## GitHub Actions and releases

Pull requests into `main` and pushes to `main` build, test, and package the program. The workflow extracts and runs the executable before uploading the ZIP as a workflow artifact.

After the workflow reaches GitHub, push a `vMAJOR.MINOR.PATCH` tag on the commit you want to release. For example, from an up-to-date `main`:

```powershell
git tag -a v0.1.0 -m "Release v0.1.0"
git push origin refs/tags/v0.1.0
```

The tag runs the same checks and publishes the tested ZIP under **Releases**. Use a new version tag for each release.
