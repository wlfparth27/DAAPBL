#ifndef DP_SOLVER_H
#define DP_SOLVER_H

#include "baseline_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exact 0/1 dynamic-programming baseline. */
BaselineStatus dp_solve(
    const ReliefCrate *crates,
    size_t count,
    double capacity_kg,
    BaselineSolution *solution
);

#ifdef __cplusplus
}
#endif
#endif
