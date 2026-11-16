# Graph Cut implementation (assignment item 4)

`src/graph cut/graph_cut.h` exposes a library independent of image storage. Supply the same
nonnegative, finite pixel energies used by the DP and Greedy implementations:

```cpp
#include "graph_cut.h"

seam_carver::EnergyMap energy = {{9, 1, 9}, {9, 9, 1}, {9, 1, 9}};
auto vertical = seam_carver::find_vertical_graph_cut(energy);
// vertical.indices[y] is the column to remove from row y: {1, 2, 1}.
auto horizontal = seam_carver::find_horizontal_graph_cut(energy);
// horizontal.indices[x] is the row to remove from column x.
```

The result also includes `energy` (sum of selected pixel costs) and `cut_capacity`
(computed max-flow). They should agree within floating-point rounding. Equal-cost
seams may differ from DP's tie-breaking while still being optimal. Inputs are not
modified. Invalid maps throw `std::invalid_argument`; unsafe capacity sums throw
`std::overflow_error`. Allocation failures propagate normally.

## Graph construction

For a height H, width W energy map, use W+1 boundary nodes per row. Pixel x is
represented by the directed edge from boundary x to boundary x+1, with capacity
`energy[y][x]`. Connect the source to boundary 0 and boundary W to the sink.
Add backward edges x+1 -> x and diagonal edges (y,x) -> (y-1,x-1) and
(y,x) -> (y+1,x-1), where in bounds. These constraint edges use a capacity
strictly greater than the sum of all pixel energies (1 for an all-zero map).

This boundary-node adjustment fixes an edge case in the brief's suggested
pixel-node construction: pinning the last pixel to the sink excludes that pixel
from the source side, so it can never be selected. The extra boundary permits
removing the first or last column and supports W=1.

Why it works:

1. Backward edges force source-side nodes in each row to be a prefix.
2. The terminals force exactly one finite pixel edge to cross the cut per row.
3. Diagonal edges force adjacent prefix endpoints to differ by at most one.
4. Thus every feasible minimum cut is a connected seam; conversely, each valid
   seam defines a cut crossing only its pixel edges, with the same cost.
5. Edmonds-Karp starts with zero flow and repeatedly uses BFS to find a
   shortest augmenting path by edge count. It sends the path's bottleneck
   capacity and updates both forward and reverse residual capacities.
6. When no augmenting path remains, residual BFS from the source identifies
   the minimum cut. Its rightmost boundary in each row gives the seam index.

Horizontal seams run the same algorithm on the transposed energy map. Each
original directed edge has a separate reverse residual edge; these are distinct
from the original backward constraint edges.
Floating capacities use `long double` internally (on MSVC this has double
precision). No fixed epsilon discards small positive energies. As with other
floating-point algorithms, extreme differences in energy scale can lose precision.

## Integration with the team's image pipeline

For each seam removal, calculate energies from the **current image**, call this
solver, remove the indicated pixels, and recalculate energies before the next
iteration. For a vertical seam, copy each row while skipping `indices[y]`;
for a horizontal seam, copy each column while skipping `indices[x]`.
Do not remove the final row or column of an image. The finder supports a
one-pixel dimension for completeness, but the resizing caller must enforce
positive target dimensions.

The finder implements seam finding only. `src/graph cut/resize.cpp` now connects
it to shared image operations in `src/image.cpp`: RGB dual-gradient magnitude
with clamped borders, pixel removal, and recalculation after every seam. The
resize loop removes vertical seams first, then horizontal seams. The Windows
image codec layer loads images and writes PNG files; `src/cli.cpp` provides the
interactive menu. DP and Greedy are still pending. The `--graph-cut-demo` option
continues to use a fixed energy map; running without arguments resizes images.
Costs are per-pixel additive energies; full predecessor-dependent forward energy
requires a different cost model and is not implemented here.

## Complexity and evaluation

The graph has V = H(W+1)+2 vertices and E = O(HW) edges, including residual edges.
Construction and storage take O(HW). Edmonds-Karp takes O(V E^2) time,
giving O((HW)^3) worst-case time and O(HW) space per seam for this graph.
DP's O(HW) per-seam time is preferable for this simple energy model; Graph Cut
provides the alternative min-cut/max-flow formulation required by the assignment.

Edmonds-Karp performs a fresh BFS for every augmentation, so it can be very
slow on large images. A 1199x506 image resized to 500x200 requires 1005 seam
removals, each with a fresh energy map and graph. Start with small images and
modest reductions. The CLI runs resizing in a worker and displays elapsed time
every 100 ms with a Braille dots spinner, including during the first seam. Press A
in an interactive terminal for an abort confirmation: Y cooperatively cancels
and returns to the algorithm menu; N continues. Cancellation is checked during
graph construction, residual BFS, and between seam removals, so it need not wait
for an entire seam. No output is saved after a confirmed abort. The image is
never silently downscaled or switched to DP. Ctrl+C still terminates the program.

CTest checks both orientations against exhaustive enumeration on all 512 binary
3x3 maps, 300 seeded random maps up to 5x5, and 14 specific edge cases. It also
checks connectivity, reported cost versus max-flow, invalid maps, and overflow.
An additional 202 checks use a test-only DP oracle on larger random maps and a
12000-pixel-wide map, bringing the total to 1854. These checks are independent
of the production max-flow algorithm and run in Release builds.

For the report, use the same images, energies, target sizes, seam-removal order,
and machine for all three methods. Record seam costs, seam overlays, resized
outputs, and elapsed times (seam finding separately from image I/O and energy
calculation). DP and Graph Cut should have equal minimum costs for the same
energy map; ties can select different pixels, affecting later removals. Actual
image-quality and runtime comparisons remain to be measured once the other
algorithms are available.

The brief asks for an AI usage declaration and main prompts. Record assistance
with this graph design, implementation, tests, and documentation in that declaration.
