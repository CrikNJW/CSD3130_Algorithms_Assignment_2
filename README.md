# Assignment 2 Seam Carver

The C++ project includes a Graph Cut seam-finding library using Edmonds-Karp
max-flow, with vertical and horizontal seams and CTest correctness checks.
See [Graph Cut design and integration](docs/graph_cut.md) for the API, graph
construction, complexity, and how to connect it to the team's image pipeline.

## Interactive CLI

Double-click `run.bat` in the repository root to compile
`build/Release/seam-carver.exe` and open the interactive menu in the same window.
The batch file can find the CMake bundled with Visual Studio. The build requires
the **Desktop development with C++** workload with the **C++ CMake tools for
Windows** component or a separate CMake installation.

Put your image in the `input` folder at the repository root. In the menu, choose
**1. Graph Cut**, **2. Dynamic Programming**, or **3. Greedy**. Enter the filename
such as `photo.png`, then enter the target **height** and **width**. You can also
open the compiled executable directly. Invalid choices,
files, and dimensions are prompted again. Enter `q` at any prompt or `0` at the
algorithm menu to exit.
After saving, the algorithm menu appears again so the window stays open.

Relative paths start in `input` regardless of where you launch the executable.
If you run an extracted release outside the repository, put `input` beside the
executable. Full paths also work, including files dragged into the terminal.
Paths may contain spaces and may be quoted. The image must decode successfully
before dimensions are requested.

- Input: PNG, JPEG, BMP, GIF, and TIFF via Windows Imaging Component. For
  multi-frame files, only the first frame is used. EXIF orientation and color
  profiles are not applied; output does not preserve source metadata.
- Targets: positive integers no larger than the decoded image dimensions.
  Enlargement is not implemented. Keeping either dimension unchanged is allowed.
- Output: PNG in an `output` directory **beside the executable**, regardless of
  the current working directory. Example: `output/photo_graph_cut_640x480.png`.
  DP and Greedy use the suffixes `_dp_640x480.png` and `_greedy_640x480.png`.
  Existing names receive `_1`, `_2`, etc. The source file is left intact.
- Graph Cut code is in `src/graph cut`. Shared image handling and CLI code are
  in `src`. RGB gradient energies are recalculated for each seam; width is
  reduced first, followed by height. Alpha bytes are preserved.
- A Braille dots spinner, seam count, and elapsed time update every 100 ms, including during
  the first seam. In an interactive terminal, press **A** to request an abort,
  then **Y** to stop and return to the menu or **N** to continue (no Enter needed).
  Computation continues while the question is shown; confirming abort discards
  the result even if it just finished. Aborted jobs do not save an output image.
  Keyboard controls are disabled for redirected input. Max-flow can
  be slow on large photos; start with a small image and a modest size reduction.

The image CLI requires Windows and the Windows SDK, with no Qt, OpenCV, or
downloaded image library dependencies.

## Build and test on Windows

Use Visual Studio with the C++ workload and CMake available in your terminal.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure --no-tests=error
./build/Release/seam-carver.exe
./build/Release/seam-carver.exe --graph-cut-demo
cpack --config build/CPackConfig.cmake -C Release -G ZIP -B build/packages
```

The package is `build/packages/seam-carver-windows-x64.zip`. Image I/O uses system
Windows codecs, so no additional image-processing DLLs need bundling. In the local
Release build, resized images are saved under `build/Release/output/`.

## GitHub Actions and releases

Pull requests into `main` and pushes to `main` build, test, and package the program. The workflow extracts and runs the executable before uploading the ZIP as a workflow artifact.

After the workflow reaches GitHub, push a `vMAJOR.MINOR.PATCH` tag on the commit you want to release. For example, from an up-to-date `main`:

```powershell
git tag -a v0.1.0 -m "Release v0.1.0"
git push origin refs/tags/v0.1.0
```

The tag runs the same checks and publishes the tested ZIP under **Releases**. Use a new version tag for each release.

## Dynamic Programming implementation

See [DP implementation notes](docs/dp.md) for the recurrence, complexity, API,
and test coverage. Implementation: `src/dp.cpp` and `src/dp.h`.

## Greedy implementation

Greedy selects the cheapest connected pixel at each step. It supports both
orientations and uses the same energy map and image removal routines as DP.
With option **3**, answer **y** to save one numbered PNG per removal in the
adjacent `*_steps` folder. Magenta marks the next seam on the image as it looks
at that step. The normal output PNG contains the completed resize.
See [Greedy design, Question 2 answers, and benchmark](docs/greedy.md).
