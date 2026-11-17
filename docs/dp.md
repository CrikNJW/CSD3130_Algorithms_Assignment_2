# Dynamic Programming seam carving

Select option **2** in the interactive CLI, provide an image, then target height
and width. Width is reduced first, then height. The result is saved beside the
executable in `output/<name>_dp_<width>x<height>.png`.

## How the algorithm works

1. Use the shared `calculate_energy` function to calculate RGB gradient energy.
2. For a vertical seam, initialise the first row with its pixel energies.
3. For each later pixel, add its energy to the smallest cumulative energy among
   the valid upper-left, upper, and upper-right predecessors:

   `M[r][c] = E[r][c] + min(M[r-1][k])`, where `|k-c| <= 1`.

4. Store the chosen predecessor. Choose the cheapest endpoint in the last row
   and backtrack to recover one column index per row.
5. Remove that seam, recalculate energy, and repeat until the target width.
6. For height reduction, use the same recurrence with rows and columns swapped.

Out-of-bounds predecessors are excluded. Equal costs prefer the smallest
predecessor index and smallest endpoint, so results are deterministic. This
minimises the energy of each individual seam on the current image; it does not
guarantee a globally optimal sequence of all removals.

## API and complexity

`find_vertical_dp` and `find_horizontal_dp` accept an EnergyMap and optional
cancellation callback. They return DPSeam with zero-based indices and total
energy. Empty, ragged, negative, or non-finite maps are rejected.

A seam search takes O(HW) time and O(HW) auxiliary space for backtracking;
cumulative costs use two rolling rows (O(W), or O(H) horizontally).
`resize_dp` validates dimensions and recalculates energy after each removal,
sharing the existing pixel-removal functions, alpha preservation, progress
callback and abort controls. Removing k seams takes O(kHW) time as an upper
bound based on the original dimensions, with O(HW) peak auxiliary space.
Enlargement is not supported.

## Verification

Run the usual CMake build and CTest commands from the README. The `dp-correctness`
test compares 500 small maps against exhaustive enumeration, verifies horizontal
transposition, path connectivity, tie-breaking, invalid inputs, unchanged sizes,
repeated removal, progress, cancellation, and an end-to-end CLI PNG resize.
Existing Graph Cut and image/CLI regression tests run alongside it.

For a manual demo, select option 2, load a photo, keep its height unchanged and
reduce width slightly. Then repeat with height reduction or both dimensions.
Compare the output dimensions and preservation of important image content.

## AI assistance record

Implementation and tests were prepared with Codex assistance at the student's
request to help implement the Assignment 2 seam-carving component. The student
provided the project's CMake configure, build and CTest commands and requested
that changes remain uncommitted. Include this assistance and the relevant chat
prompts in the report's AI declaration; review and understand the code before
presenting it.
