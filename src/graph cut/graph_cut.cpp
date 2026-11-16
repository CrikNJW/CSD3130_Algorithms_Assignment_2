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
        constexpr auto missing = std::numeric_limits<std::size_t>::max();

        Capacity validate(const EnergyMap &energy, const CancelCheck &cancelled)
        {
            if (energy.empty() || energy.front().empty())
            {
                throw std::invalid_argument("Energy map must be nonempty.");
            }
            Capacity total = 0;
            for (const auto &row : energy)
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
                const auto forward_index = edges_[from].size();
                const auto reverse_index = edges_[to].size();
                edges_[from].push_back({to, reverse_index, capacity});
                edges_[to].push_back({from, forward_index, 0});
            }

            Capacity max_flow(std::size_t source, std::size_t sink, const CancelCheck &cancelled)
            {
                // Edmonds-Karp: BFS finds a shortest augmenting path (by edge count)
                // in the residual graph. Start from zero flow and repeat until no
                // source-to-sink path remains.
                Capacity flow = 0;
                std::vector<std::size_t> parent(edges_.size());
                std::vector<std::size_t> parent_edge(edges_.size());
                while (true)
                {
                    check_cancelled(cancelled);
                    std::fill(parent.begin(), parent.end(), missing);
                    parent[source] = source;
                    std::queue<std::size_t> pending;
                    pending.push(source);
                    std::size_t visited = 0;
                    while (!pending.empty() && parent[sink] == missing)
                    {
                        if ((visited++ % 256) == 0)
                            check_cancelled(cancelled);
                        const auto from = pending.front();
                        pending.pop();
                        for (std::size_t i = 0; i < edges_[from].size(); ++i)
                        {
                            const auto &edge = edges_[from][i];
                            if (edge.residual > 0 && parent[edge.to] == missing)
                            {
                                parent[edge.to] = from;
                                parent_edge[edge.to] = i;
                                pending.push(edge.to);
                            }
                        }
                    }
                    if (parent[sink] == missing)
                        return flow;

                    Capacity amount = std::numeric_limits<Capacity>::max();
                    for (auto v = sink; v != source; v = parent[v])
                    {
                        amount = std::min(amount, edges_[parent[v]][parent_edge[v]].residual);
                    }
                    for (auto v = sink; v != source; v = parent[v])
                    {
                        auto &edge = edges_[parent[v]][parent_edge[v]];
                        edge.residual -= amount;
                        edges_[edge.to][edge.reverse].residual += amount;
                    }
                    flow += amount;
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
                    const auto from = pending.front();
                    pending.pop();
                    for (const auto &edge : edges_[from])
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
        const auto height = energy.size();
        const auto width = energy.front().size();
        if (width == missing || height > (missing - 2) / (width + 1))
        {
            throw std::overflow_error("Energy map dimensions are too large.");
        }

        // W+1 boundary nodes per row let a cut select ANY of the W pixels.
        // Pixel x is represented by the finite edge boundary x -> boundary x+1.
        const auto stride = width + 1;
        const auto source = height * stride;
        const auto sink = source + 1;
        FlowNetwork graph(sink + 1);
        
        // Greater than every feasible seam cost, no IEEE infinity arithmetic.
        const Capacity hard = total + std::max(Capacity{1}, total);
        const auto node = [stride](std::size_t y, std::size_t x)
        { return y * stride + x; };

        for (std::size_t y = 0; y < height; ++y)
        {
            check_cancelled(cancelled);
            graph.add_edge(source, node(y, 0), hard);
            graph.add_edge(node(y, width), sink, hard);
            for (std::size_t x = 0; x < width; ++x)
            {
                graph.add_edge(node(y, x), node(y, x + 1), energy[y][x]);
                // Source-side nodes must form a prefix in each row.
                graph.add_edge(node(y, x + 1), node(y, x), hard);
            }
            for (std::size_t x = 1; x <= width; ++x)
            {
                // Adjacent row prefixes can differ in length by at most one.
                if (y > 0)
                    graph.add_edge(node(y, x), node(y - 1, x - 1), hard);
                if (y + 1 < height)
                    graph.add_edge(node(y, x), node(y + 1, x - 1), hard);
            }
        }

        const auto flow = graph.max_flow(source, sink, cancelled);
        const auto reachable = graph.source_side(source, cancelled);
        Seam seam{{}, 0, static_cast<double>(flow)};
        seam.indices.reserve(height);
        Capacity cost = 0;
        for (std::size_t y = 0; y < height; ++y)
        {
            check_cancelled(cancelled);
            std::size_t x = 0;
            while (x + 1 < width && reachable[node(y, x + 1)])
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
