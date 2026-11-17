#ifndef GREEDY_SOLVER_H
#define GREEDY_SOLVER_H

#include "baseline_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Deterministic, non-optimality-guaranteed density-first 0/1 heuristic. */
BaselineStatus greedy_solve(
    const ReliefCrate *crates,
    size_t count,
    double capacity_kg,
    BaselineSolution *solution
);

#ifdef __cplusplus
}
#endif
#endif
