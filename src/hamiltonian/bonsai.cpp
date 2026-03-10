/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Implement the Bonsai mapping for fermionic Hamiltonian to qubit Hamiltonian ]
  Author       [ Mu-Te (Joshua) Lau (joshmtlau) ]
*/

#include "bonsai.hpp"

#include <cmath>
#include <queue>
#include <unordered_set>

#include "device/device_analysis.hpp"
#include "spdlog/spdlog.h"
#include "util/util.hpp"

namespace qsyn::hamiltonian {

namespace {
struct PQItem {
    float dist_to_center;
    float dist;
    QubitIdType qubit;
    QubitIdType parent_qubit;  // device qubit already in tree; new node will be its child
};

auto const comp = [](PQItem const& a, PQItem const& b) {
    // note that std::priority_queue is a max heap, so we need to invert the comparison

    // to maintain similar behavior as the Bonsai paper's description,
    // we first compare the distance to the center
    // this reduces to a layer-by-layer traversal when all couplings are equally good
    if (a.dist_to_center < b.dist_to_center) return false;
    if (a.dist_to_center > b.dist_to_center) return true;
    // for the same level, prioritize the best coupling (min distance)
    if (a.dist < b.dist) return false;
    if (a.dist > b.dist) return true;
    // for the same level and same distance, prioritize the qubit with smaller index
    return a.qubit > b.qubit;
};

using PQ = std::priority_queue<PQItem, std::vector<PQItem>, decltype(comp)>;
}  // namespace

/**
 * @brief Build a Bonsai ternary tree for a given device and APSP result. The tree will be rooted at the given qubit.
 * @param root_qubit_id ID of the qubit to root the tree at
 * @param device Device
 * @param apsp APSP result. The caller should compute the APSP result before calling this function.
 * @param n_qubits Number of qubits in the final Bonsai tree. If not specified,
 *        a tree with all qubits in the device will be built.
 * @return Bonsai ternary tree, or a failure reason.
 */
tl::expected<TernaryTree, BonsaiFailReason> build_bonsai_ternary_tree(std::size_t root_qubit_id, device::Device const& device, device::APSPResult const& apsp, size_t n_qubits) {
    if (root_qubit_id >= device.get_num_qubits()) {
        return tl::unexpected(BonsaiFailReason::invalid_root_qubit);
    }

    n_qubits = std::min(n_qubits, device.get_num_qubits());

    // keep track of already-visited device qubits
    std::unordered_set<QubitIdType> visited_qubits;
    TernaryTree tt;

    auto const assign_and_mark_visited = [&](size_t node_index, QubitIdType qubit) {
        tt.assign_qubit(node_index, qubit);
        visited_qubits.insert(qubit);
    };

    PQ qubit_queue(comp);

    // handle the center qubit, which is already in the tree
    assign_and_mark_visited(0, root_qubit_id);
    for (auto const& neighbor : device.get_adjacencies(root_qubit_id)) {
        auto const d = apsp.distance[root_qubit_id][neighbor];
        if (std::isinf(d)) {
            continue;
        }
        qubit_queue.push(PQItem{
            .dist_to_center = d,
            .dist           = d,
            .qubit          = neighbor,
            .parent_qubit   = root_qubit_id,
        });
    }

    while (tt.num_qubits() < n_qubits) {
        if (qubit_queue.empty()) {
            return tl::unexpected(BonsaiFailReason::not_enough_qubits);
        }
        auto const [dist_to_center, dist, qubit, parent_qubit] = qubit_queue.top();
        qubit_queue.pop();
        if (visited_qubits.contains(qubit)) {
            continue;
        }
        // add the qubit node as child of the node that corresponds to parent_qubit
        auto const node_index = tt.add_qubit_node_to_first_empty_branch(
            tt.get_node_by_qubit(parent_qubit));

        DVLAB_ASSERT(node_index.has_value(), "Some empty branch must exist for parent qubit");

        assign_and_mark_visited(*node_index, qubit);

        for (auto const& neighbor : device.get_adjacencies(qubit)) {
            auto const dist_to_center = apsp.distance[root_qubit_id][neighbor];
            if (std::isinf(dist_to_center)) {
                continue;
            }

            auto const dist = apsp.distance[qubit][neighbor];
            if (std::isinf(dist)) {
                continue;
            }

            qubit_queue.push(
                PQItem{
                    .dist_to_center = dist_to_center,
                    .dist           = dist,
                    .qubit          = neighbor,
                    .parent_qubit   = qubit,
                });
        }
    }

    tt.append_legs_to_tree();

    return tt;
}
/**
 * @brief Build a Bonsai ternary tree for a given device and APSP result.
 *        The tree will be rooted at one of the centers of the device coupling graph.
 * @param device Device
 * @param apsp APSP result. The caller should compute the APSP result before
          calling this function.
 * @param n_qubits Number of qubits in the final Bonsai tree. If not specified,
 *        a tree with all qubits in the device will be built.
 * @return Bonsai ternary tree, or a failure reason.
 */
tl::expected<TernaryTree, BonsaiFailReason>
build_bonsai_ternary_tree(
    device::Device const& device, device::APSPResult const& apsp, size_t n_qubits) {
    n_qubits                        = std::min(n_qubits, device.get_num_qubits());
    auto const connected_components = device::get_connected_components(apsp, device);
    assert(!connected_components.empty());
    for (auto const& component : connected_components) {
        // REVIEW: potential query speed concern
        auto const centers = device::get_centers(
            apsp, device, [&](QubitIdType const& qubit_id) {
                return dvlab::contains(component, qubit_id);
            });
        assert(!centers.empty());
        auto const component_size = component.size();
        // for now, we ignore the case where there are multiple centers in the same component
        auto result = build_bonsai_ternary_tree(centers[0], device, apsp, n_qubits);
        if (result.has_value()) {
            return result;
        }
    }
    return tl::unexpected(BonsaiFailReason::not_enough_qubits);
}
}  // namespace qsyn::hamiltonian
