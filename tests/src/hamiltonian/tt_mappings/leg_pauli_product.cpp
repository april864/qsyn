/*
  TTMapper::leg_pauli_product tests.
*/

#include "hamiltonian/tt_mappings/fixtures.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace qsyn::hamiltonian;
using namespace qsyn::hamiltonian::tt_mappings_test;

TEST_CASE("leg_pauli_product: JW chain is i Z_j per mode", "[tt_mappings][leg_pauli]") {
    constexpr size_t num_qubits = 5;

    TernaryTree tree = make_jw_chain_tree(num_qubits);
    TTMapper mapper(std::move(tree));

    for (size_t mode = 0; mode < num_qubits; ++mode) {
        auto [left_leg, mid_leg] = mapper.leg_pairs().at(mode);
        auto product             = mapper.leg_pauli_product(left_leg, mid_leg);

        REQUIRE(product.coeff() == std::complex<double>(0, 1));
        REQUIRE(product.get_pauli_type(mode) == qsyn::tableau::Pauli::z);
        for (size_t q = 0; q < num_qubits; ++q) {
            if (q != mode) {
                REQUIRE(product.is_i(q));
            }
        }
    }
}

TEST_CASE("leg_pauli_product: parity chain", "[tt_mappings][leg_pauli]") {
    constexpr size_t num_qubits = 5;
    TernaryTree tree            = make_parity_chain_tree(num_qubits);
    TTMapper mapper(std::move(tree));

    {
        auto [left_leg, mid_leg] = mapper.leg_pairs().at(0);
        auto product             = mapper.leg_pauli_product(left_leg, mid_leg);
        REQUIRE(product.coeff() == std::complex<double>(0, 1));
        REQUIRE(product.get_pauli_type(0) == qsyn::tableau::Pauli::z);
    }
    for (size_t j = 1; j < num_qubits; ++j) {
        auto [left_leg, mid_leg] = mapper.leg_pairs().at(j);
        auto product             = mapper.leg_pauli_product(left_leg, mid_leg);
        REQUIRE(product.coeff() == std::complex<double>(0, 1));
        REQUIRE(product.get_pauli_type(j - 1) == qsyn::tableau::Pauli::z);
        REQUIRE(product.get_pauli_type(j) == qsyn::tableau::Pauli::z);
        for (size_t q = 0; q < num_qubits; ++q) {
            if (q != j - 1 && q != j) {
                REQUIRE(product.is_i(q));
            }
        }
    }
}

TEST_CASE("leg_pauli_product: branching example leaf modes", "[tt_mappings][leg_pauli]") {
    TernaryTree tree = make_branching_example_tree();
    TTMapper mapper(std::move(tree));

    for (size_t const mode : {0, 2, 3, 6, 8}) {
        auto [left_leg, mid_leg] = mapper.leg_pairs().at(mode);
        auto product             = mapper.leg_pauli_product(left_leg, mid_leg);
        REQUIRE(product.coeff() == std::complex<double>(0, 1));
        REQUIRE(product.get_pauli_type(mode) == qsyn::tableau::Pauli::z);
        for (size_t q = 0; q < 9; ++q) {
            if (q != mode) {
                REQUIRE(product.is_i(q));
            }
        }
    }
}
