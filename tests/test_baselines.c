#include "baseline_common.h"
#include "dp_solver.h"
#include "greedy_solver.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int tests_run = 0;
static unsigned int tests_failed = 0;

#define CHECK(condition, message) do { \
    ++tests_run; \
    if (condition) printf("[PASS] %s\n", message); \
    else { printf("[FAIL] %s (line %d)\n", message, __LINE__); ++tests_failed; } \
} while (0)

static ReliefCrate make_crate(const char *id, double weight, double priority, CrateStatus status) {
    ReliefCrate crate;
    memset(&crate, 0, sizeof(crate));
    (void)snprintf(crate.crate_id, sizeof(crate.crate_id), "%s", id);
    crate.weight_kg = weight;
    crate.priority_score = priority;
    crate.status = status;
    crate.category = CAT_MEDICAL;
    crate.density = weight > 0.0 ? priority / weight : 0.0;
    return crate;
}

static int contains_index(const BaselineSolution *solution, size_t index) {
    for (size_t i = 0; i < solution->selected_count; ++i)
        if (solution->selected_indices[i] == index) return 1;
    return 0;
}

static double brute_force_optimum(const ReliefCrate *crates, size_t count, double capacity) {
    unsigned long long best = 0;
    unsigned long long cap_units = (unsigned long long)llround(capacity * 10.0);
    unsigned long long limit = 1ULL << count;
    for (unsigned long long mask = 0; mask < limit; ++mask) {
        unsigned long long weight = 0, priority = 0;
        int feasible = 1;
        for (size_t i = 0; i < count; ++i) {
            if ((mask & (1ULL << i)) == 0) continue;
            if (crates[i].status != STATUS_READY) { feasible = 0; break; }
            weight += (unsigned long long)llround(crates[i].weight_kg * 10.0);
            priority += (unsigned long long)llround(crates[i].priority_score * 10.0);
        }
        if (feasible && weight <= cap_units && priority > best) best = priority;
    }
    return (double)best / 10.0;
}

static void test_small_manifest(void) {
    ReliefCrate crates[5] = {
        make_crate("CR-101", 2.0, 40.0, STATUS_READY),
        make_crate("CR-102", 5.0, 30.0, STATUS_READY),
        make_crate("CR-103", 10.0, 50.0, STATUS_READY),
        make_crate("CR-104", 5.0, 10.0, STATUS_READY),
        make_crate("CR-105", 3.0, 12.0, STATUS_READY)
    };
    BaselineSolution dp = {0}, greedy = {0};
    CHECK(dp_solve(crates, 5, 15.0, &dp) == BASELINE_OK, "small manifest DP completes");
    CHECK(fabs(dp.total_priority - 102.0) < 1e-9, "DP obtains hand-verified optimum 102");
    CHECK(fabs(dp.total_weight - 15.0) < 1e-9 && dp.selected_count == 3 &&
          contains_index(&dp, 0) && contains_index(&dp, 2) && contains_index(&dp, 4),
          "DP reconstructs CR-101, CR-103, and CR-105 at 15 kg");
    CHECK(greedy_solve(crates, 5, 15.0, &greedy) == BASELINE_OK, "small manifest Greedy completes");
    CHECK(fabs(greedy.total_priority - 92.0) < 1e-9 && greedy.selected_count == 4 &&
          contains_index(&greedy, 0) && contains_index(&greedy, 1) &&
          contains_index(&greedy, 3) && contains_index(&greedy, 4),
          "Greedy returns the density-first 92-point packing");
    baseline_solution_free(&dp);
    baseline_solution_free(&greedy);
}

static void test_greedy_counterexample(void) {
    ReliefCrate crates[2] = {
        make_crate("TRAP-LIGHT", 1.0, 2.0, STATUS_READY),
        make_crate("TRAP-HEAVY", 100.0, 100.0, STATUS_READY)
    };
    BaselineSolution dp = {0}, greedy = {0};
    CHECK(dp_solve(crates, 2, 100.0, &dp) == BASELINE_OK, "counterexample DP completes");
    CHECK(greedy_solve(crates, 2, 100.0, &greedy) == BASELINE_OK, "counterexample Greedy completes");
    CHECK(fabs(dp.total_priority - 100.0) < 1e-9 && contains_index(&dp, 1),
          "DP chooses the 100-priority heavy crate");
    CHECK(fabs(greedy.total_priority - 2.0) < 1e-9 && contains_index(&greedy, 0),
          "Greedy demonstrates its non-optimality");
    baseline_solution_free(&dp);
    baseline_solution_free(&greedy);
}

static void test_empty_and_infeasible(void) {
    BaselineSolution result = {0};
    CHECK(dp_solve(NULL, 0, 10.0, &result) == BASELINE_OK && result.selected_count == 0,
          "empty input is a valid empty DP solution");
    CHECK(greedy_solve(NULL, 0, 10.0, &result) == BASELINE_OK && result.selected_count == 0,
          "Greedy handles empty input");
    CHECK(dp_solve(NULL, 0, 0.0, &result) == BASELINE_OK,
          "zero capacity is handled without allocating a DP table");

    ReliefCrate infeasible[2] = {
        make_crate("INF-1", 11.0, 100.0, STATUS_READY),
        make_crate("INF-2", 12.0, 120.0, STATUS_READY)
    };
    CHECK(dp_solve(infeasible, 2, 10.0, &result) == BASELINE_OK && result.selected_count == 0,
          "all-infeasible input produces an empty feasible solution");

    ReliefCrate mixed[2] = {
        make_crate("READY-1", 2.0, 10.0, STATUS_READY),
        make_crate("DAMAGED-1", 1.0, 1000.0, STATUS_DAMAGED)
    };
    CHECK(dp_solve(mixed, 2, 2.0, &result) == BASELINE_OK &&
          result.selected_count == 1 && contains_index(&result, 0),
          "non-READY crates are excluded");
    baseline_solution_free(&result);
}

static void test_density_ties(void) {
    ReliefCrate crates[5] = {
        make_crate("CR-TIE-01", 10.0, 50.0, STATUS_READY),
        make_crate("CR-TIE-02", 4.0, 20.0, STATUS_READY),
        make_crate("CR-TIE-03", 6.0, 30.0, STATUS_READY),
        make_crate("CR-TIE-04", 2.0, 10.0, STATUS_READY),
        make_crate("CR-TIE-05", 8.0, 40.0, STATUS_READY)
    };
    BaselineSolution first = {0}, second = {0};
    CHECK(greedy_solve(crates, 5, 12.0, &first) == BASELINE_OK, "equal-density input is accepted");
    CHECK(fabs(first.total_priority - 60.0) < 1e-9 && fabs(first.total_weight - 12.0) < 1e-9,
          "tie policy produces a feasible deterministic packing");
    CHECK(greedy_solve(crates, 5, 12.0, &second) == BASELINE_OK &&
          first.selected_count == second.selected_count &&
          memcmp(first.selected_indices, second.selected_indices,
                 first.selected_count * sizeof(size_t)) == 0,
          "repeated Greedy calls return identical original indices");
    baseline_solution_free(&first);
    baseline_solution_free(&second);
}

static void test_invalid_and_extreme_inputs(void) {
    BaselineSolution result = {0};
    ReliefCrate valid = make_crate("VALID", 1.0, 1.0, STATUS_READY);
    CHECK(dp_solve(&valid, 1, -1.0, &result) == BASELINE_INVALID_CAPACITY,
          "negative capacity is rejected");
    CHECK(greedy_solve(&valid, 1, INFINITY, &result) == BASELINE_INVALID_CAPACITY,
          "infinite capacity is rejected");

    ReliefCrate invalid = make_crate("BAD", NAN, 1.0, STATUS_READY);
    CHECK(dp_solve(&invalid, 1, 5.0, &result) == BASELINE_INVALID_CRATE,
          "NaN weight is rejected");
    invalid = make_crate("BAD", 1.0, INFINITY, STATUS_READY);
    CHECK(greedy_solve(&invalid, 1, 5.0, &result) == BASELINE_INVALID_CRATE,
          "infinite priority is rejected");

    ReliefCrate duplicates[2] = {
        make_crate("DUP", 1.0, 1.0, STATUS_READY),
        make_crate("DUP", 2.0, 2.0, STATUS_READY)
    };
    CHECK(dp_solve(duplicates, 2, 5.0, &result) == BASELINE_DUPLICATE_ID,
          "duplicate READY IDs are rejected for sanitizer-level deduplication");

    ReliefCrate fine = make_crate("FINE", 0.0000001, 1.0, STATUS_READY);
    CHECK(dp_solve(&fine, 1, 1.0, &result) == BASELINE_UNSUPPORTED_PRECISION,
          "DP refuses to silently round beyond six decimal places");
    CHECK(greedy_solve(&fine, 1, 1.0, &result) == BASELINE_OK && result.selected_count == 1,
          "Greedy accepts finite positive double weights without fixed-point conversion");
    baseline_solution_free(&result);

    valid = make_crate("HUGE", 1000000000.0, 1.0, STATUS_READY);
    CHECK(dp_solve(&valid, 1, 1000000000.0, &result) == BASELINE_RESOURCE_LIMIT,
          "DP rejects an impractical capacity table before allocation");
    baseline_solution_free(&result);

    ReliefCrate extreme[2] = {
        make_crate("EXTREME-HIGH", 1.0e-250, 1.0e300, STATUS_READY),
        make_crate("EXTREME-LOW", 1.0e-300, 1.0e100, STATUS_READY)
    };
    BaselineSolution greedy = {0};
    CHECK(greedy_solve(extreme, 2, 1.0e-250, &greedy) == BASELINE_OK,
          "Greedy handles extreme finite density ratios");
    CHECK(greedy.selected_count == 1 && contains_index(&greedy, 0),
          "extreme-density ordering chooses the higher-density crate");
    baseline_solution_free(&greedy);
}

static void test_randomized_exhaustive_comparison(void) {
    unsigned int seed = 0xC0FFEEu;
    for (unsigned int trial = 0; trial < 200; ++trial) {
        size_t n = 1u + (trial % 12u);
        ReliefCrate crates[12];
        for (size_t i = 0; i < n; ++i) {
            seed = seed * 1664525u + 1013904223u;
            double weight = (double)(1u + (seed % 50u)) / 10.0;
            seed = seed * 1664525u + 1013904223u;
            double priority = (double)(1u + (seed % 200u)) / 10.0;
            char id[MAX_ID_LEN];
            (void)snprintf(id, sizeof(id), "R%u_%u", trial, (unsigned)i);
            crates[i] = make_crate(id, weight, priority, STATUS_READY);
        }
        seed = seed * 1664525u + 1013904223u;
        double capacity = (double)(10u + (seed % 100u)) / 10.0;
        double optimum = brute_force_optimum(crates, n, capacity);
        BaselineSolution dp = {0}, greedy = {0};
        BaselineStatus dp_status = dp_solve(crates, n, capacity, &dp);
        BaselineStatus greedy_status = greedy_solve(crates, n, capacity, &greedy);
        CHECK(dp_status == BASELINE_OK && fabs(dp.total_priority - optimum) <= 1e-8,
              "randomized DP matches exhaustive 0/1 enumeration");
        CHECK(greedy_status == BASELINE_OK &&
              greedy.total_weight <= capacity + 1e-9 &&
              greedy.total_priority <= optimum + 1e-8,
              "randomized Greedy output is feasible and cannot exceed optimum");
        baseline_solution_free(&dp);
        baseline_solution_free(&greedy);
    }
}

int main(void) {
    printf("============================================================\n");
    printf(" PARTH: 0/1 DYNAMIC PROGRAMMING & GREEDY BASELINE TESTS\n");
    printf("============================================================\n");
    test_small_manifest();
    test_greedy_counterexample();
    test_empty_and_infeasible();
    test_density_ties();
    test_invalid_and_extreme_inputs();
    test_randomized_exhaustive_comparison();
    printf("\nTests run: %u | Passed: %u | Failed: %u\n",
           tests_run, tests_run - tests_failed, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
