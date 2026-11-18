#include "graph_cut.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <stdexcept>

namespace seam_carver
{
    namespace
    {

        using Capacity = long double;
        constexpr std::size_t missing = std::numeric_limits<std::size_t>::max();

        Capacity validate(const EnergyMap &energy, const CancelCheck &cancelled)
        {
            if (energy.empty() || energy.front().empty())
            {
                throw std::invalid_argument("Energy map must be nonempty.");
            }
            Capacity total = 0;
            for (const std::vector<double> &row : energy)
            {
                check_cancelled(cancelled);
                if (row.size() != energy.front().size())
                {
                    throw std::invalid_argument("Energy map must be rectangular.");
                }
                for (double value : row)
                {
                    if (!std::isfinite(value) || value < 0)
                    {
                        throw std::invalid_argument("Energies must be finite and nonnegative.");
                    }
                    total += value;
                    if (total > std::numeric_limits<double>::max() / 4)
                    {
                        throw std::overflow_error("Energy sum is too large.");
                    }
                }
            }
            return total;
        }

        std::size_t boundary_node(std::size_t row, std::size_t column,
                                  std::size_t width, std::size_t source)
        {
            if (column == 0)
                return source;
            if (column == width)
                return source + 1;
            return row * (width - 1) + column - 1;
        }

        struct Edge
        {
            std::size_t to;
            std::size_t reverse;
            Capacity residual;
        };

        class FlowNetwork
        {
        public:
            explicit FlowNetwork(std::size_t vertices) : edges_(vertices) {}

            void add_edge(std::size_t from, std::size_t to, Capacity capacity)
            {
                // Each original directed edge has its own reverse residual edge,
                // separate from any original backward constraint edge.
                const std::size_t forward_index = edges_[from].size();
                const std::size_t reverse_index = edges_[to].size();
                edges_[from].push_back({to, reverse_index, capacity});
                edges_[to].push_back({from, forward_index, 0});
            }

            Capacity max_flow(std::size_t source, std::size_t sink, const CancelCheck &cancelled)
            {
                const std::size_t vertices = edges_.size();
                std::vector<std::size_t> level(vertices);
                std::vector<std::size_t> current(vertices);
                std::vector<std::size_t> pending(vertices);
                struct PathStep
                {
                    std::size_t vertex;
                    Capacity bottleneck;
                };
                std::vector<PathStep> path;
                path.reserve(vertices);
                Capacity flow = 0;

                while (true)
                {
                    check_cancelled(cancelled);
                    // Build the level graph with BFS over positive residual edges.
                    std::fill(level.begin(), level.end(), missing);
                    level[source] = 0;
                    std::size_t read = 0;
                    std::size_t write = 1;
                    pending[0] = source;
                    while (read < write)
                    {
                        if (read % 256 == 0)
                            check_cancelled(cancelled);
                        const std::size_t from = pending[read++];
                        if (level[from] >= level[sink])
                            break;
                        for (const Edge &edge : edges_[from])
                        {
                            if (edge.residual > 0 && level[edge.to] == missing)
                            {
                                level[edge.to] = level[from] + 1;
                                pending[write++] = edge.to;
                            }
                        }
                    }
                    if (level[sink] == missing)
                        return flow;

                    // Find the blocking flow with iterative DFS. Keep each vertex's
                    // current edge until it saturates or leads to a dead end.
                    std::fill(current.begin(), current.end(), 0);
                    path.push_back({source, std::numeric_limits<Capacity>::max()});
                    std::size_t inspected = 0;
                    while (!path.empty())
                    {
                        if (inspected++ % 256 == 0)
                            check_cancelled(cancelled);
                        const std::size_t from = path.back().vertex;
                        if (from == sink)
                        {
                            const Capacity amount = path.back().bottleneck;
                            for (std::size_t i = 0; i + 1 < path.size(); ++i)
                            {
                                const std::size_t vertex = path[i].vertex;
                                Edge &edge = edges_[vertex][current[vertex]];
                                edge.residual -= amount;
                                edges_[edge.to][edge.reverse].residual += amount;
                            }
                            flow += amount;
                            path.resize(1);
                            continue;
                        }

                        std::size_t &index = current[from];
                        if (index == edges_[from].size())
                        {
                            path.pop_back();
                            if (!path.empty())
                                ++current[path.back().vertex];
                            continue;
                        }

                        const Edge &edge = edges_[from][index];
                        if (edge.residual > 0 && level[edge.to] == level[from] + 1 &&
                            (edge.to == sink || level[edge.to] < level[sink]))
                        {
                            path.push_back({edge.to, std::min(path.back().bottleneck, edge.residual)});
                        }
                        else
                        {
                            ++index;
                        }
                    }
                }
            }

            std::vector<bool> source_side(std::size_t source, const CancelCheck &cancelled) const
            {
                std::vector<bool> reached(edges_.size(), false);
                std::queue<std::size_t> pending;
                reached[source] = true;
                pending.push(source);
                std::size_t visited = 0;
                while (!pending.empty())
                {
                    if ((visited++ % 256) == 0)
                        check_cancelled(cancelled);
                    const std::size_t from = pending.front();
                    pending.pop();
                    for (const Edge &edge : edges_[from])
                    {
                        if (edge.residual > 0 && !reached[edge.to])
                        {
                            reached[edge.to] = true;
                            pending.push(edge.to);
                        }
                    }
                }
                return reached;
            }

        private:
            std::vector<std::vector<Edge>> edges_;
        };

    } // namespace

    Seam find_vertical_graph_cut(const EnergyMap &energy, const CancelCheck &cancelled)
    {
        const Capacity total = validate(energy, cancelled);
        const std::size_t height = energy.size();
        const std::size_t width = energy.front().size();
        if (width > 1 && height > (missing - 2) / (width - 1))
        {
            throw std::overflow_error("Energy map dimensions are too large.");
        }

        // All left boundaries are fixed to the source side of every minimum
        // cut, and all right boundaries are fixed to the sink side. Merge them
        // with the corresponding terminals.
        const std::size_t source = height * (width - 1);
        const std::size_t sink = source + 1;
        // Greater than every feasible seam cost, no IEEE infinity arithmetic.
        const Capacity hard = total + std::max(Capacity{1}, total);
        FlowNetwork graph(sink + 1);
        for (std::size_t y = 0; y < height; ++y)
        {
            check_cancelled(cancelled);
            for (std::size_t x = 0; x < width; ++x)
            {
                const std::size_t left = boundary_node(y, x, width, source);
                const std::size_t right = boundary_node(y, x + 1, width, source);
                graph.add_edge(left, right, energy[y][x]);

                // Backward edges force a prefix. Arcs out of the sink or into
                // the source never cross a cut and can be omitted.
                if (x > 0 && x + 1 < width)
                    graph.add_edge(right, left, hard);
            }
            for (std::size_t x = 2; x < width; ++x)
            {
                // Adjacent row prefixes can differ in length by at most one.
                const std::size_t from = boundary_node(y, x, width, source);
                if (y > 0)
                    graph.add_edge(from, boundary_node(y - 1, x - 1, width, source), hard);
                if (y + 1 < height)
                    graph.add_edge(from, boundary_node(y + 1, x - 1, width, source), hard);
            }
        }

        const Capacity flow = graph.max_flow(source, sink, cancelled);
        const std::vector<bool> reachable = graph.source_side(source, cancelled);
        Seam seam{{}, 0, static_cast<double>(flow)};

        seam.indices.reserve(height);
        Capacity cost = 0;
        for (std::size_t y = 0; y < height; ++y)
        {
            check_cancelled(cancelled);
            std::size_t x = 0;
            while (x + 1 < width && reachable[boundary_node(y, x + 1, width, source)])
                ++x;
            seam.indices.push_back(x);
            cost += energy[y][x];
        }
        seam.energy = static_cast<double>(cost);
        return seam;
    }

    Seam find_horizontal_graph_cut(const EnergyMap &energy, const CancelCheck &cancelled)
    {
        validate(energy, cancelled);
        EnergyMap transposed(energy.front().size(), std::vector<double>(energy.size()));
        for (std::size_t y = 0; y < energy.size(); ++y)
        {
            check_cancelled(cancelled);
            for (std::size_t x = 0; x < energy.front().size(); ++x)
            {
                transposed[x][y] = energy[y][x];
            }
        }
        return find_vertical_graph_cut(transposed, cancelled);
    }

} // namespace seam_carver
