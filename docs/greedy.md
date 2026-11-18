# Greedy seam carving and comparison with dynamic programming

## Q2(a)(i) Number of connected seams

Let a vertical seam in an m × n image be a sequence of column indices
`c₁, …, cₘ`, with `1 ≤ cᵢ ≤ n` and `|cᵢ − cᵢ₋₁| ≤ 1`. When n > 1, restrict every
row to columns 1 and 2. Both choices in the next row are legal regardless of
the current column. The m independent binary choices give 2^m distinct seams.
There are at most n · 3^(m−1) seams because the first pixel has n choices and
each later pixel has at most three. With n = 2 there are exactly 2^m seams.

## Q2(a)(ii) Optimal subproblems

Define D(i,j) as the minimum disruption of a vertical seam prefix ending at
(i,j). Each final step must come from one of the valid neighboring columns in
row i−1, so the base case and recurrence are:

```text
D(1,j) = e(1,j)
D(i,j) = e(i,j) + min { D(i−1,k) : 1 ≤ k ≤ n and |k−j| ≤ 1 }
minimum seam energy = min_j D(m,j)
```

If an optimal seam used a more costly prefix at a pixel, replacing that prefix
with a cheaper one ending at the same pixel would preserve the seam's suffix
and lower the total, a contradiction. Store the minimizing predecessor for
each pixel and backtrack from the best endpoint. For horizontal seams,
exchange rows and columns. Two cumulative-cost rows suffice for the value.
The team's DP implementation also stores Θ(mn) predecessor entries in the
worst case to reconstruct a seam directly.

## Q2(b) Divide and conquer for Sobel

A fixed 3 × 3 Sobel stencil performs constant work per sample, so a direct pass
uses Θ(mn) time. A tiled divide-and-conquer implementation can read a
one-pixel halo from the input around each tile, calculate only pixels owned by
that tile, then assemble disjoint output tiles. At the actual image border use
the same padding rule as the untiled pass. This preserves results and still
requires Θ(mn) total work because every energy value must be written. Tiling
can improve cache locality or parallel wall-clock time on sufficiently large
images. Halo reads, scheduling, and memory bandwidth limit the gains. A
separable Sobel kernel uses a 1-D smoothing pass and a 1-D derivative pass,
reducing arithmetic by a constant factor. The project uses a fixed RGB
dual-gradient stencil, also Θ(mn); the experiment below uses this same
energy function for both algorithms.

## Q3 Greedy choice, feasibility, and limits

Start at the minimum-energy pixel in the first row. At each subsequent row
scan the in-bounds columns in [previous−1, previous+1] and select the lowest
pixel energy. Scan in ascending order and update only for strict improvement
so an energy tie favors the smallest index. A horizontal seam starts in the
first column and moves across columns with the same rule on row indices.
Both directions share one traversal and index the corresponding axes in
place. The selector accumulates the energies only to report the final cost.

The candidate set contains the previous column at every step. It therefore
never runs out of candidates. By induction, the chosen positions give one
pixel per row, all in bounds, with at most a one-column move each time.
This proves feasibility. Local choice can still increase the total:

| Energy map | Column 1 | Column 2 | Column 3 |
| --- | ---: | ---: | ---: |
| Row 1 | **1** | 8 | **2** |
| Row 2 | **1** | 8 | **1** |
| Row 3 | **100** | 100 | **1** |

Greedy follows column 1 for a total of 102. From row 2 it cannot reach
column 3 on the final step. DP takes column 3 for a total of 4. Replacing
the two 100s with an arbitrary M > 2 gives ratio (M+2)/4, so there is no
constant approximation factor for arbitrary nonnegative maps. Trying
several starting pixels would address some starting-point failures, at
additional cost, but local decisions could still fail farther down each path.

For a precomputed m × n map, both orientations spend Θ(m+n) time including
shape checks, scan of the initial row or column, and at most three candidates
per later step. Greedy holds O(1) working state beyond the map and returned
path, which stores Θ(m) vertical indices or Θ(n) horizontal indices. The
team DP selector takes Θ(mn) time and Θ(mn) auxiliary space in the worst
case, including predecessor data. Image energy computation and removal
both scan the current image and dominate Greedy's end-to-end asymptotics.
For dimensions mₜ × nₜ just before removal t, either resize takes
Θ(Σₜ mₜnₜ) time with full energy recomputation and Θ(mn) peak memory
for the original dimensions. For a no-op the two algorithms skip carving.
Width shrinks before height in both implementations.

## Measurements

The benchmark used a Windows x64 MSVC 19.41 Release build on an Intel
Core i9-14900HX. Each measurement is the median of nine trials after a
warm-up, alternating method order. Selector timings average 16 calls per
trial on the same precomputed map. Resize timings include every energy
recalculation, selection, and removal. They exclude input copies, image
decoding and encoding, CLI display, and frame export.

| Input | First seam | Greedy energy | DP energy | Shared positions |
| --- | --- | ---: | ---: | ---: |
| Flat background | Vertical | 0.00 | 0.00 | 96 / 96 |
| Flat background | Horizontal | 0.00 | 0.00 | 160 / 160 |
| Slanted grid | Vertical | 6972.51 | 4715.24 | 0 / 160 |
| Slanted grid | Horizontal | 10981.15 | 7979.77 | 0 / 256 |
| NASA portrait | Vertical | 3636.87 | 2343.43 | 0 / 512 |
| NASA portrait | Horizontal | 5449.23 | 3489.51 | 16 / 512 |

| Input → target (width × height) | Greedy resize | DP resize | Speedup | Output |
| --- | ---: | ---: | ---: | --- |
| Flat, 160 × 96 → 140 × 90 | 2.49 ms | 4.55 ms | 1.83× | Pixel-identical |
| Grid, 256 × 160 → 224 × 150 | 9.52 ms | 16.30 ms | 1.71× | Different |
| Portrait, 512 × 512 → 448 × 480 | 124.51 ms | 202.96 ms | 1.63× | Different |

For the portrait, the vertical selector alone took 4.98 µs with Greedy and
787.66 µs with DP. Energy recomputation and removal explain why the whole
resize gained much less. The flat image retained exactly the same subject
and output pixels. Both grid outputs bent some lines, at different positions.
Both methods kept the face recognizable in the portrait. The Greedy output
narrowed the face, and the DP output introduced a visible kink in the white
rim of the helmet. Thus Greedy can give comparable results when seams can
remain in low-texture regions. Its quality is less predictable around
structures. The energy totals compare the first seams on the same input.
DP optimizes that per-seam objective. Visual inspection assesses a different
criterion, and successive removals create different energy maps.

Reproduce the two built-in inputs or add the public-domain NASA portrait from
[the scikit-image v0.20.0 sample data](https://github.com/scikit-image/scikit-image/blob/v0.20.0/skimage/data/astronaut.png):

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --target greedy-benchmark
./build/Release/greedy-benchmark.exe build/greedy-results
./build/Release/greedy-benchmark.exe build/greedy-results path/to/astronaut.png
```

The program emits numerical results, the first-seam overlays in both
directions, and the final PNGs. Timings vary by machine and build.

## Verification and bonus visualization

`greedy-correctness` checks every 3 × 3 binary energy grid (512) in both
orientations, testing the local choice, ties, feasibility, summed energy, and
the bound obtained from DP. It also checks degenerate dimensions, invalid
inputs, cancellation, and observer behavior. `image-cli-correctness` verifies
that the interactive path writes valid output and both kinds of frame, and
that an aborted run discards its frame directory. CTest runs these cases in
the repository's existing Windows build, test, and package workflow.

For a short demo, choose Greedy from the interactive menu, reduce width
and height by a few pixels, and answer **y** when asked about exporting
frames. Open the numbered PNGs in order. Each shows the actual magenta
seam on the image immediately before that removal. Switch to the final PNG
to show the resulting dimensions. Keep export disabled when timing the
algorithm, since copying and encoding each frame adds work and requires
disk space proportional to the sum of the encoded frame sizes.

## References

- Avidan, S. and Shamir, A. (2007). [Seam Carving for Content-Aware Image Resizing](https://www.cs.princeton.edu/courses/archive/fall07/cos597B/papers/avidan-seam-carving.pdf). ACM Transactions on Graphics, 26(3), article 10.
- NASA, *Eileen Collins*, public domain. [scikit-image sample data and attribution](https://scikit-image.org/docs/0.20.x/api/skimage.data.html#skimage.data.astronaut).
