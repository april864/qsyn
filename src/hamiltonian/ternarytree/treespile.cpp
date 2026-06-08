/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Treespilation mapping from fermionic Hamiltonian to mapped quantum circuits ]
  Author       [ Mu-Te (Joshua) Lau (joshmtlau) ]
*/

#include "./treespile.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <optional>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <fstream>
#include <random>

#include "device/device_analysis.hpp"
#include "./bonsai.hpp"
#include "hamiltonian/f2q_mappings.hpp"
#include "./ternary_tree.hpp"
#include "./tree_rotations.hpp"
#include "qcir/basic_gate_type.hpp"
#include "util/graph/minimum_spanning_arborescence.hpp"
#include "util/simulated_annealing.hpp"
#include "tree_optimizations.hpp"

namespace qsyn::hamiltonian {

namespace {

TernaryQubitNode* as_qubit_node_or_throw(TernaryNode* node) {
    auto const qubit_node = dynamic_cast<TernaryQubitNode*>(node);
    if (!qubit_node) {
        throw std::runtime_error("Should not happen: tree node is not a qubit node");
    }
    return qubit_node;
}

size_t find_unique_connecting_ancilla(
    std::vector<std::vector<size_t>> const& connected_components,
    std::vector<size_t> const& physical_qubits,
    TernaryTree const& tree) {
    std::unordered_set<size_t> physical_qubits_set(
        physical_qubits.begin(),
        physical_qubits.end());
    std::vector<size_t> potential_ancilla_qubits;

    for (auto const& component : connected_components) {
        for (auto const& qubit : component) {
            auto const tree_node = as_qubit_node_or_throw(tree.get_node_by_qubit(qubit));
            auto const parent    = tree_node->parent;
            if (!parent) {
                // root node; skip
                continue;
            }
            auto const parent_node  = as_qubit_node_or_throw(parent);
            auto const parent_qubit = parent_node->qubit_label.value();
            if (!physical_qubits_set.contains(parent_qubit)) {
                potential_ancilla_qubits.push_back(parent_qubit);
            }
        }
    }

    if (potential_ancilla_qubits.empty()) {
        throw std::runtime_error("Should not happen: no potential ancilla qubits found");
    }

    auto majority_ancilla_qubit = potential_ancilla_qubits[0];
    for (auto const& qubit : potential_ancilla_qubits) {
        if (std::ranges::count(potential_ancilla_qubits, qubit) > potential_ancilla_qubits.size() / 2) {
            majority_ancilla_qubit = qubit;
        }
    }

    return majority_ancilla_qubit;
}

struct TermSynthesisInfo {
    std::vector<size_t> qubits;
    std::vector<bool> is_ancilla;

    void add_ancilla(size_t qubit) {
        qubits.push_back(qubit);
        is_ancilla.push_back(true);
    }
};

std::pair<size_t, size_t> find_shortest_terminals_between_two_connected_components(
    std::vector<std::vector<size_t>> const& connected_components,
    device::APSPResult const& apsp) {
    auto const& component0 = connected_components[0];
    auto const& component1 = connected_components[1];

    float min_dist                     = std::numeric_limits<float>::infinity();
    std::pair<size_t, size_t> min_pair = {0, 0};
    for (auto const& qubit : component0) {
        for (auto const& other_qubit : component1) {
            auto const dist = apsp.distance[qubit][other_qubit];
            if (dist < min_dist) {
                min_dist = dist;
                min_pair = {qubit, other_qubit};
            }
        }
    }

    return min_pair;
}

TermSynthesisInfo form_connected_components(
    HermitianPauliTerm const& term,
    TernaryTree const& tree,
    device::APSPResult const& apsp,
    device::Device const& device) {
    //
    auto const& pauli_product = term.pauli_product();
    std::vector<size_t> physical_qubits;

    for (size_t i = 0; i < pauli_product.n_qubits(); i++) {
        if (pauli_product.is_i(i)) {
            continue;
        }
        auto const tree_node = as_qubit_node_or_throw(tree.get_node_by_index(i));
        physical_qubits.push_back(tree_node->qubit_label.value());
    }

    auto const connected_components =
        get_connected_components(apsp, device, [&](size_t qubit) {
            return dvlab::contains(physical_qubits, qubit);
        });

    spdlog::debug("Connected components for term {}:", term.to_string());
    for (auto const& component : connected_components) {
        spdlog::debug("- Component: [{}]", fmt::join(component, ", "));
    }

    TermSynthesisInfo result{
        .qubits     = physical_qubits,
        .is_ancilla = std::vector<bool>(physical_qubits.size(), false),
    };

    if (physical_qubits.empty()) {
        return result;
    }

    // case 1: all qubits are in the same connected component
    if (connected_components.size() == 1) {
        return result;
    }

    // case 2: qubits are in two connected components
    // find the shortest path between the two connected components using the APSP result
    // add them to the synthesis info as ancilla qubits

    if (connected_components.size() == 2) {
        auto const [terminal0, terminal1] = find_shortest_terminals_between_two_connected_components(connected_components, apsp);
        auto const path                   = device::get_shortest_path(apsp, terminal0, terminal1);
        if (!path.has_value()) {
            throw std::runtime_error("Should not happen: no path found between two connected components");
        }

        // the two terminals are a part of the physical qubits. The rest must be
        // ancilla qubits because we are using the shortest path.
        auto const n_ancilla = path.value().size() - 2;
        result.qubits.reserve(result.qubits.size() + n_ancilla);
        result.is_ancilla.reserve(result.is_ancilla.size() + n_ancilla);
        for (auto const& qubit : path.value() | std::views::drop(1) | std::views::take(n_ancilla)) {
            result.add_ancilla(qubit);
        }

        return result;
    }

    // case 3: qubits are in multiple connected components
    // in this case, there must be a unique qubit that is not in physical_qubits
    // such that adding it to physical_qubits will form a single connected component
    // use the tree to check the parent of each tree nodes.

    auto const ancilla_qubit =
        find_unique_connecting_ancilla(connected_components, physical_qubits, tree);

    result.add_ancilla(ancilla_qubit);
    return result;
}

dvlab::Digraph<size_t, float> form_device_subgraph(
    TermSynthesisInfo const& synthesis_info,
    device::Device const& device,
    device::APSPResult const& apsp) {
    auto device_subgraph = dvlab::Digraph<size_t, float>{};
    // add all physical qubits to the subgraph (use qubit IDs as vertex IDs so
    // apsp.distance[src][dst] and ancilla lookups work correctly)
    for (auto const& qubit : synthesis_info.qubits) {
        device_subgraph.add_vertex_with_id(qubit);
    }
    for (auto const& qubit : synthesis_info.qubits) {
        for (auto const& other_qubit : synthesis_info.qubits) {
            if (qubit == other_qubit) continue;
            if (device.is_adjacent(qubit, other_qubit)) {
                device_subgraph.add_edge(qubit, other_qubit, apsp.distance[qubit][other_qubit]);
            }
        }
    }

    return device_subgraph;
}

void append_mst_node(
    dvlab::Digraph<size_t, float> const& mst,
    size_t v,
    std::unordered_set<size_t> const* ancilla_qubits,
    std::string const& prefix,
    bool is_last,
    std::string& out) {
    auto const branch_char = is_last ? "└── " : "├── ";
    out += prefix + branch_char;

    out += fmt::format("q{}", v);
    if (ancilla_qubits && ancilla_qubits->contains(v)) {
        out += " (ancilla)";
    }
    out += "\n";

    std::vector<size_t> children_vec(mst.out_neighbors(v).begin(),
                                     mst.out_neighbors(v).end());
    std::ranges::sort(children_vec);

    std::string child_prefix = prefix + (is_last ? "    " : "│   ");
    for (size_t i = 0; i < children_vec.size(); ++i) {
        append_mst_node(mst, children_vec[i], ancilla_qubits, child_prefix,
                        i == children_vec.size() - 1, out);
    }
}

std::string mst_to_string(
    dvlab::Digraph<size_t, float> const& mst,
    size_t root,
    std::unordered_set<size_t> const* ancilla_qubits = nullptr) {
    std::string out;
    out += fmt::format("q{}", root);
    if (ancilla_qubits && ancilla_qubits->contains(root)) {
        out += " (ancilla)";
    }
    out += "\n";

    std::vector<size_t> children_vec(mst.out_neighbors(root).begin(),
                                     mst.out_neighbors(root).end());
    std::ranges::sort(children_vec);

    for (size_t i = 0; i < children_vec.size(); ++i) {
        append_mst_node(mst, children_vec[i], ancilla_qubits, "",
                        i == children_vec.size() - 1, out);
    }
    return out;
}

/** Maps fermion tree indices (and ancilla physical ids) to dense logical QCir lines. */
struct LogicalQubitLayout {
    std::vector<size_t> physical_qubits;
    std::unordered_map<size_t, size_t> physical_to_logical;

    explicit LogicalQubitLayout(TernaryTree const& tree) {
        auto const n = tree.num_qubits();
        physical_qubits.resize(n);
        for (size_t i = 0; i < n; ++i) {
            auto const physical = as_qubit_node_or_throw(tree.get_node_by_index(i))->qubit_label.value();
            physical_qubits[i] = physical;
            physical_to_logical.emplace(physical, i);
        }
    }

    size_t num_qubits() const { return physical_qubits.size(); }

    size_t to_logical(size_t physical) const { return physical_to_logical.at(physical); }

    void register_ancilla(size_t physical) {
        if (physical_to_logical.contains(physical)) {
            return;
        }
        auto const logical = physical_qubits.size();
        physical_qubits.push_back(physical);
        physical_to_logical.emplace(physical, logical);
    }
};

void collect_ancillas_for_hamiltonian(
    QubitHamiltonian const& qubit_hamiltonian,
    TernaryTree const& tree,
    device::APSPResult const& apsp,
    device::Device const& device,
    LogicalQubitLayout& layout) {
    for (auto const& term : qubit_hamiltonian) {
        if (term.pauli_product().is_identity()) {
            continue;
        }
        auto const synthesis_info = form_connected_components(term, tree, apsp, device);
        for (size_t i = 0; i < synthesis_info.qubits.size(); ++i) {
            if (synthesis_info.is_ancilla[i]) {
                layout.register_ancilla(synthesis_info.qubits[i]);
            }
        }
    }
}

void synthesize_term(
    HermitianPauliTerm const& term,
    TernaryTree const& tree,
    device::APSPResult const& apsp,
    device::Device const& device,
    double dt,
    qcir::QCir& qcir,
    LogicalQubitLayout const* layout) {
    auto const& pauli_product = term.pauli_product();
    std::vector<size_t> physical_qubits;
    for (size_t i = 0; i < pauli_product.n_qubits(); i++) {
        if (pauli_product.is_i(i)) {
            continue;
        }
        auto const tree_node = as_qubit_node_or_throw(tree.get_node_by_index(i));
        physical_qubits.push_back(tree_node->qubit_label.value());
    }

    if (pauli_product.is_identity()) {
        return;
    }

    auto const synthesis_info = form_connected_components(term, tree, apsp, device);

    auto const device_subgraph = form_device_subgraph(synthesis_info, device, apsp);

    // Build set of ancilla qubit IDs (is_ancilla is parallel to qubits, not indexed by qubit ID)
    auto ancilla_qubits = std::unordered_set<size_t>{};
    for (size_t i = 0; i < synthesis_info.qubits.size(); ++i) {
        if (synthesis_info.is_ancilla[i]) {
            ancilla_qubits.insert(synthesis_info.qubits[i]);
        }
    }

    auto const mst_cost_fn = [&](auto const& e) {
        auto const [src, dst] = e;

        // a cnot (dst, src) is needed to connect the two qubits
        // if src is an ancilla qubit, we also need to synthesize a cnot (src, dst)
        if (ancilla_qubits.contains(src)) {
            return apsp.distance[dst][src] + apsp.distance[src][dst];
        }
        return apsp.distance[dst][src];
    };
    auto const [mst, root] =
        dvlab::minimum_spanning_arborescence_with_cost(device_subgraph, mst_cost_fn);

    spdlog::debug("MST for term {}:\n{}", term.to_string(), mst_to_string(mst, root, &ancilla_qubits));

    std::vector<size_t> post_order_traversal;
    std::stack<size_t> stack;

    stack.push(root);
    while (!stack.empty()) {
        auto const v = stack.top();
        stack.pop();
        post_order_traversal.push_back(v);
        for (auto const& n : mst.out_neighbors(v)) {
            stack.push(n);
        }
    }

    std::ranges::reverse(post_order_traversal);

    auto const num_qubits = layout ? layout->num_qubits() : device.get_num_qubits();
    auto const to_line    = [&](size_t physical) -> size_t {
        return layout ? layout->to_logical(physical) : physical;
    };

    qcir::QCir conjugation_qcir(num_qubits);

    // conjugate by V and H gates to put all Pauli letters to Z
    for (size_t i = 0; i < term.pauli_product().n_qubits(); ++i) {
        if (term.pauli_product().is_i(i)) {
            continue;
        }

        auto const qubit_line = layout ? i : as_qubit_node_or_throw(tree.get_node_by_index(i))->qubit_label.value();
        if (term.pauli_product().is_x(i)) {
            conjugation_qcir.append(qcir::HGate(), {qubit_line});
        }
        if (term.pauli_product().is_y(i)) {
            conjugation_qcir.append(qcir::SXGate(), {qubit_line});
        }
    }

    // build CX sequence according to the post-order traversal
    for (auto const& dst : post_order_traversal) {
        if (mst.in_degree(dst) == 0) {
            continue;
        }
        auto const src = *mst.in_neighbors(dst).begin();
        conjugation_qcir.append(qcir::CXGate(), {to_line(dst), to_line(src)});
        if (ancilla_qubits.contains(dst)) {
            conjugation_qcir.append(qcir::CXGate(), {to_line(src), to_line(dst)});
        }
    }
    qcir.compose(conjugation_qcir);

    // synthesize a phase gate at the root.
    // for now, assumes there's only one trotterization step

    qcir.append(qcir::PZGate(-term.coeff() * dt), {to_line(root)});
    conjugation_qcir.adjoint_inplace();
    qcir.compose(conjugation_qcir);
}

}  // namespace

tl::expected<TreespileResult, TreespileFailReason>
treespile(
    FermionHamiltonian const& hamiltonian,
    device::Device const& device,
    double time,
    size_t n_trotterization_steps,
    device::APSPCostFnType const& cost_fn,
    bool optimize1,
    bool optimize2,
    bool exhaustive,
    bool use_logical_indices) {
    //
    using FailReason = TreespileFailReason;

    if (n_trotterization_steps == 0) {
        return tl::unexpected(FailReason::invalid_n_trotterization_steps);
    }

    auto const n_modes = hamiltonian.n_modes();
    if (n_modes == 0) {
        return tl::unexpected(FailReason::empty_hamiltonian);
    }

    if (n_modes > device.get_num_qubits()) {
        return tl::unexpected(FailReason::device_too_small);
    }

    // generate a ternary tree

    auto const apsp = floyd_warshall(device, cost_fn);

    auto tree = exhaustive
                    ? build_bonsai_ternary_tree_exhaustive(device, apsp, n_modes)
                    : build_bonsai_ternary_tree(device, apsp, n_modes);

    if (!tree.has_value()) {
        // NOTE: it's still possible for bonsai to fail because the device might
        // be disconnected.
        return tl::unexpected(FailReason::tt_build_failed_not_enough_qubits);
    }

    if (optimize1) {
        tree = pauli_weight_optimize_mapping(*tree, hamiltonian, &device);
    } else if (optimize2) {
        tree = infidelity_proxy_optimize_mapping(*tree, hamiltonian, &device);
    }

    auto const mapping = TernaryTreeMapping(tree.value());

    auto const qubit_hamiltonian = qubitize(hamiltonian, mapping);

    // if all terms are commutative, fix trotter steps to 1
    if (is_all_commutative(qubit_hamiltonian)) {
        n_trotterization_steps = 1;
    }

    std::optional<LogicalQubitLayout> layout;
    if (use_logical_indices) {
        layout.emplace(tree.value());
        collect_ancillas_for_hamiltonian(qubit_hamiltonian, tree.value(), apsp, device, *layout);
    }

    qcir::QCir qcir(use_logical_indices ? layout->num_qubits() : device.get_num_qubits());

    auto const dt = time / static_cast<double>(n_trotterization_steps);
    LogicalQubitLayout const* layout_ptr = layout ? &*layout : nullptr;

    for (auto const& term : qubit_hamiltonian) {
        synthesize_term(term, tree.value(), apsp, device, dt, qcir, layout_ptr);
    }

    if (n_trotterization_steps > 1) {
        auto copy_qcir = qcir;
        for (size_t i = 1; i < n_trotterization_steps; ++i) {
            qcir.compose(copy_qcir);
        }
    }

    TreespileResult result;
    result.circuit         = std::move(qcir);
    result.physical_qubits = use_logical_indices ? layout->physical_qubits : std::vector<size_t>{};
    result.encoding        = std::make_unique<TernaryTreeMapping>(std::move(tree.value()));
    return result;
}


// NEW
void evaluate_proxy_cost(
    FermionHamiltonian const& hamiltonian,
    device::Device const& device,
    std::string const& output_csv,
    size_t samples) {

    auto const apsp = floyd_warshall(device, device::default_floyd_warshall_cost);

    std::ofstream csv(output_csv, std::ios::app);
    
    // Seed randomizer
    std::random_device rd;
    std::mt19937 rng(rd());

    for (size_t i = 0; i < samples; ++i) {
        fmt::println("Starting tree {}", i);
        auto base_tree = build_bonsai_ternary_tree(device, apsp, hamiltonian.n_modes());
        if (!base_tree) {
            spdlog::error("Failed to build base tree on sample {}.", i);
            continue;
        }
        TernaryTree tree = std::move(base_tree.value());

        // Apply random tree rotation
        TreeRotator rotator;
        for (int m = 0; m < 20; ++m) {
            int mutation_type = rng() % 4; 
            
            if (mutation_type == 0) {
                rotator.root_change(&tree);
            } else if (mutation_type == 1) {
                rotator.pauli_shuffle(&tree);
            } else if (mutation_type == 2) {
                rotator.mode_association_swap(&tree);
            } else if (mutation_type == 3) {
                rotator.majorana_braiding_change(&tree);
            }
        }

        // BEGIN stuff for fidelity_apsp
        auto noise_aware_cost = [&tree, &device](qsyn::device::Device::QubitPair const& edge, qsyn::device::Device const& dev) -> float {
            
            // If edge not in tree, return infinite cost
            if (!tree.has_edge(edge.src, edge.dst) && !tree.has_edge(edge.dst, edge.src)) {
                return std::numeric_limits<float>::infinity();
            }

            auto const& edge_gates = dev.get_gate_info(edge);
            auto const& gate_set = dev.get_gate_set();
            float error_rate = 1.0f;
            
            // Find 2-qubit gate
            for (auto const& info : edge_gates) {
                std::string name = dvlab::str::tolower_string(gate_set[info.gate_idx]);
                if (name == "cx" || name == "cnot" || name == "ecr" || name == "cz") {
                    error_rate = info.error; 
                    break;
                }
            }
            
            // Return log infidelity
            if (error_rate >= 1.0f || error_rate < 0.0f) {
                return std::numeric_limits<float>::infinity();
            }
            return -std::log(1.0f - error_rate);
        };

        auto const fidelity_apsp = floyd_warshall(device, noise_aware_cost);
        // END stuff for fidelity apsp

        // double proxy_cost = fast_tree_cost(tree, hamiltonian, apsp);
        double proxy_cost = infidelity_cost(tree, hamiltonian, fidelity_apsp);
        qcir::QCir qcir(device.get_num_qubits());

        // Check if circuit can be synthesized on hardware
        auto mapping = TernaryTreeMapping(tree);
        auto q_ham = qubitize(hamiltonian, mapping);
        bool routable = true;
        try {
            for (const auto& term : q_ham) {
                synthesize_term(term, tree, apsp, device, 1.0, qcir, nullptr);
            }
        } catch (...) {
            routable = false;
        }
        if (!routable) {
            i--; 
            continue;
        }

        std::string qasm_file = fmt::format("sample_{}.qasm", i);
        qcir.write_qasm(qasm_file);

        csv << i << "," << proxy_cost << "\n";
        csv.flush();
    }
}

// For use in evaluate_proxy_termwise_depth
// Keeps array of num_qubits x num_circuit_layers. For each gate in each Hamiltonian
// term, updates array[qubit][layer] to hold the index of the term.
struct DepthArray {
    size_t num_qubits;
    int IDLE = -1;
    std::vector<std::vector<int>> array;

    DepthArray(size_t q) : num_qubits(q), array(q) {}

    void next_gate(const std::vector<size_t>& qubits, int term_id) {
        if (qubits.size() == 1) {
            array[qubits[0]].push_back(term_id);
        } 
        else if (qubits.size() == 2) {
            size_t q0 = qubits[0];
            size_t q1 = qubits[1];
            
            // Take max time slice between the two qubits
            size_t depth = std::max(array[q0].size(), array[q1].size());
            
            // Pad other entries
            while (array[q0].size() < depth) array[q0].push_back(IDLE);
            while (array[q1].size() < depth) array[q1].push_back(IDLE);
            
            array[q0].push_back(term_id);
            array[q1].push_back(term_id);
        }
    }

    // Counts up each term's contribution to the overall circuit depth
    std::vector<size_t> get_term_contributions(size_t num_terms) {
        std::vector<size_t> contributions(num_terms, 0);
        
        // Find last qubit in critical path
        size_t max_depth = 0;
        size_t curr_q = 0;
        for (size_t q = 0; q < num_qubits; ++q) {
            if (array[q].size() > max_depth) {
                max_depth = array[q].size();
                curr_q = q;
            }
        }

        // Pad leftover empty spots
        for (size_t q = 0; q < num_qubits; ++q) {
            while (array[q].size() < max_depth) array[q].push_back(IDLE);
        }

        // Fill term contributions vector
        int curr_t = max_depth - 1;
        while (curr_t >= 0) {
            int current_term = array[curr_q][curr_t];
            
            if (current_term != IDLE) {
                contributions[current_term]++; // current_term has contributed to overall circuit depth
                
                // Check if need to switch qubits because of 2-qubit gate
                if (curr_t > 0) {
                    // Find if another qubit shares this gate
                    int other_q = -1;
                    for (size_t q = 0; q < num_qubits; ++q) {
                        if (q != curr_q && array[q][curr_t] == current_term) {
                            other_q = q;
                            break;
                        }
                    }

                    // Switch qubits if this qubit is part of a 2q gate and was idle before this gate
                    if (other_q != -1 && array[curr_q][curr_t - 1] == IDLE) {
                        curr_q = other_q; 
                    }
                }
            }
            curr_t--;
        }
        
        return contributions;
    }
};

// TODO: wasn't using fidelity apsp??? fix: decide which is better: to use or not to use
void evaluate_proxy_termwise_depth(
    FermionHamiltonian const& hamiltonian,
    device::Device const& device,
    std::string const& output_csv,
    size_t samples) {

    auto const apsp = floyd_warshall(device, device::default_floyd_warshall_cost);

    std::ofstream csv(output_csv, std::ios::app);
    
    // Seed randomizer
    std::random_device rd;
    std::mt19937 rng(rd());

    for (size_t i = 0; i < samples; ++i) {
        fmt::println("Starting tree {}", i);
        auto base_tree = build_bonsai_ternary_tree(device, apsp, hamiltonian.n_modes());
        if (!base_tree) {
            spdlog::error("Failed to build base tree on sample {}.", i);
            continue;
        }
        TernaryTree tree = std::move(base_tree.value());

        // Apply random tree rotations
        TreeRotator rotator;
        for (int m = 0; m < 20; ++m) {
            int mutation_type = rng() % 4; 
            
            if (mutation_type == 0) {
                rotator.root_change(&tree);
            } else if (mutation_type == 1) {
                rotator.pauli_shuffle(&tree);
            } else if (mutation_type == 2) {
                rotator.mode_association_swap(&tree);
            } else if (mutation_type == 3) {
                rotator.majorana_braiding_change(&tree);
            }
        }

        // BEGIN stuff for fidelity_apsp
        auto noise_aware_cost = [&tree, &device](qsyn::device::Device::QubitPair const& edge, qsyn::device::Device const& dev) -> float {
            
            // If edge not in tree, return infinite cost
            if (!tree.has_edge(edge.src, edge.dst) && !tree.has_edge(edge.dst, edge.src)) {
                return std::numeric_limits<float>::infinity();
            }

            auto const& edge_gates = dev.get_gate_info(edge);
            auto const& gate_set = dev.get_gate_set();
            float error_rate = 1.0f;
            
            // Find 2-qubit gate
            for (auto const& info : edge_gates) {
                std::string name = dvlab::str::tolower_string(gate_set[info.gate_idx]);
                if (name == "cx" || name == "cnot" || name == "ecr" || name == "cz") {
                    error_rate = info.error; 
                    break;
                }
            }
            
            // Return log infidelity
            if (error_rate >= 1.0f || error_rate < 0.0f) {
                return std::numeric_limits<float>::infinity();
            }
            return -std::log(1.0f - error_rate);
        };

        auto const fidelity_apsp = floyd_warshall(device, noise_aware_cost);

        // Tree map on hardware
        auto mapping = TernaryTreeMapping(tree);
        auto q_ham = qubitize(hamiltonian, mapping);
        TreeOracle oracle(tree, fidelity_apsp);
        
        // Initialize circuit
        qcir::QCir full_qcir(device.get_num_qubits());
        DepthArray deptharray(device.get_num_qubits()); 
        bool routable = true;
        
        // To store values from each term (single_term_cnots and cnot_diff count 2-qubit gates)
        struct TermData {
            size_t term_index;
            double proxy;
        };
        std::vector<TermData> sample_terms(q_ham.n_terms());

        try {
            size_t term_index = 0;
            for (const auto& term : q_ham) {
                // Calculate proxy cost
                std::vector<size_t> active_nodes;
                for (size_t i = 0; i < term.n_qubits(); ++i) {
                    if (!term.is_i(i)) active_nodes.push_back(i);
                }
                double term_proxy = oracle.get_subtree_weight(active_nodes);
                sample_terms[term_index] = {term_index, term_proxy}; 

                // Calculate contribution to full circuit
                size_t gates_before = full_qcir.get_gates().size();
                synthesize_term(term, tree, apsp, device, 1.0, full_qcir, nullptr);
                size_t gates_after = full_qcir.get_gates().size();

                auto const& all_gates = full_qcir.get_gates();
                for (size_t g = gates_before; g < gates_after; ++g) {
                    deptharray.next_gate(all_gates[g]->get_qubits(), term_index);
                }

                term_index++;
            }
        } catch (...) {
            routable = false;
        }

        if (!routable) {
            i--; 
            continue;
        }

        std::vector<size_t> term_contributions = deptharray.get_term_contributions(q_ham.n_terms());

        // Write to CSV
        for (size_t t = 0; t < sample_terms.size(); ++t) {
            csv << sample_terms[t].term_index << "," 
                << sample_terms[t].proxy << "," 
                << term_contributions[t] << "\n";
        }
        
        csv.flush(); 
    }
}

void evaluate_proxy_termwise_fidelity(
    FermionHamiltonian const& hamiltonian,
    device::Device const& device,
    std::string const& output_csv,
    size_t samples) {

    auto const apsp = floyd_warshall(device, device::default_floyd_warshall_cost);

    auto noise_aware_cost = [](qsyn::device::Device::QubitPair const& edge, qsyn::device::Device const& dev) -> float {
        // Fetch available gates for this edge
        auto const& edge_gates = dev.get_gate_info(edge);
        auto const& gate_set = dev.get_gate_set();
        
        float error_rate = 1.0f;

        // Loop through gates on this edge to find the CNOT
        for (auto const& info : edge_gates) {
            std::string gate_name = dvlab::str::tolower_string(gate_set[info.gate_idx]);
            if (gate_name == "cx" || gate_name == "cnot" || gate_name == "ecr" || gate_name == "cz") {
                error_rate = info.error;
                break; 
            }   
        }
        
        if (error_rate >= 1.0f || error_rate < 0.0f) {
            return std::numeric_limits<float>::infinity();
        }
        
        float fidelity = 1.0f - error_rate;
        return -std::log(fidelity); 
    };

    auto const fidelity_apsp = floyd_warshall(device, noise_aware_cost);

    std::ofstream csv(output_csv, std::ios::app);
    
    // Create random tree
    // Seed randomizer
    std::random_device rd;
    std::mt19937 rng(rd());

    for (size_t i = 0; i < samples; ++i) {
        fmt::println("Starting tree {}", i);
        auto base_tree = build_bonsai_ternary_tree(device, fidelity_apsp, hamiltonian.n_modes());
        if (!base_tree) {
            spdlog::error("Failed to build base tree on sample {}.", i);
            continue;
        }
        TernaryTree tree = std::move(base_tree.value());

        // Apply random tree rotations
        TreeRotator rotator;
        for (int m = 0; m < 20; ++m) {
            int mutation_type = rng() % 4; 
            
            if (mutation_type == 0) {
                rotator.root_change(&tree);
            } else if (mutation_type == 1) {
                rotator.pauli_shuffle(&tree);
            } else if (mutation_type == 2) {
                rotator.mode_association_swap(&tree);
            } else if (mutation_type == 3) {
                rotator.majorana_braiding_change(&tree);
            }
        }

        // Build tree-constrained apsp
        auto tree_constrained_cost = [&tree, &device](qsyn::device::Device::QubitPair const& edge, qsyn::device::Device const& dev) -> float {
            
            if (!tree.has_edge(edge.src, edge.dst) && !tree.has_edge(edge.dst, edge.src)) {
                return std::numeric_limits<float>::infinity();
            }

            auto const& edge_gates = dev.get_gate_info(edge);
            auto const& gate_set = dev.get_gate_set();
            float error_rate = 1.0f;
            
            for (auto const& info : edge_gates) {
                std::string name = dvlab::str::tolower_string(gate_set[info.gate_idx]);
                if (name == "cx" || name == "cnot" || name == "ecr" || name == "cz") {
                    error_rate = info.error; 
                    break;
                }
            }
            
            if (error_rate >= 1.0f || error_rate < 0.0f) {
                return std::numeric_limits<float>::infinity();
            }
            return -std::log(1.0f - error_rate);
        };

        auto const tree_apsp = floyd_warshall(device, tree_constrained_cost);
        TreeOracle oracle(tree, tree_apsp);

        // Tree map on hardware
        auto mapping = TernaryTreeMapping(tree);
        auto q_ham = qubitize(hamiltonian, mapping);
        
        // Initialize circuit
        qcir::QCir full_qcir(device.get_num_qubits());
        bool routable = true;
        
        // To store values from each term (single_term_cnots and cnot_diff count 2-qubit gates)
        struct TermData {
            size_t term_index;
            double proxy_log_infidelity;
            double actual_log_infidelity;
        };
        std::vector<TermData> sample_terms(q_ham.n_terms());

        try {
            size_t term_index = 0;
            for (const auto& term : q_ham) {
                // Calculate proxy cost
                std::vector<size_t> active_nodes;
                for (size_t i = 0; i < term.n_qubits(); ++i) {
                    if (!term.is_i(i)) active_nodes.push_back(i);
                }
                
                // Calculate proxy fidelity
                double term_infidelity = 0.0;
                if (!active_nodes.empty()) {
                    term_infidelity = 2.0 * oracle.get_subtree_weight(active_nodes);
                }

                // Calculate actual fidelity
                size_t gates_before = full_qcir.get_gates().size();
                synthesize_term(term, tree, apsp, device, 1.0, full_qcir, nullptr);
                size_t gates_after = full_qcir.get_gates().size();

                double actual_cost = 0.0;
                auto const& all_gates = full_qcir.get_gates();
                
                for (size_t g = gates_before; g < gates_after; ++g) {
                    auto qubits = all_gates[g]->get_qubits();
                    
                    if (qubits.size() == 2) {
                        size_t q0 = qubits[0];
                        size_t q1 = qubits[1];
                        
                        qsyn::device::Device::QubitPair edge{q0, q1};
                        double edge_error = 1.0; 

                        // Check the forward direction
                        if (device.is_adjacent(edge)) {
                            auto const& edge_gates = device.get_gate_info(edge);
                            auto const& gate_set = device.get_gate_set();
                            for (auto const& info : edge_gates) {
                                std::string gate_name = dvlab::str::tolower_string(gate_set[info.gate_idx]);
                                if (gate_name == "cx" || gate_name == "cnot" || gate_name == "ecr" || gate_name == "cz") {
                                    edge_error = info.error;
                                    break;
                                }
                            }
                        } else {
                            // Check the reverse direction in case the digraph is strictly directional
                            qsyn::device::Device::QubitPair rev_edge{q1, q0};
                            if (device.is_adjacent(rev_edge)) {
                                auto const& edge_gates = device.get_gate_info(rev_edge);
                                auto const& gate_set = device.get_gate_set();
                                for (auto const& info : edge_gates) {
                                    std::string gate_name = dvlab::str::tolower_string(gate_set[info.gate_idx]);
                                    if (gate_name == "cx" || gate_name == "cnot" || gate_name == "ecr" || gate_name == "cz") {
                                        edge_error = info.error;
                                        break; 
                                    }   
                                }
                            }
                        }

                        double edge_fidelity = 1.0 - edge_error;
                        actual_cost += -std::log(edge_fidelity);
                    }
                }

                sample_terms[term_index] = {term_index, term_infidelity, actual_cost};
                term_index++;
            }
        } catch (...) {
            routable = false;
        }

        if (!routable) {
            i--; 
            continue;
        }

        // Write to CSV
        for (const auto& td : sample_terms) {
            csv << td.term_index << "," 
                << td.proxy_log_infidelity << "," 
                << td.actual_log_infidelity << "\n";
        }
        
        csv.flush(); 
    }
}

}  // namespace qsyn::hamiltonian
