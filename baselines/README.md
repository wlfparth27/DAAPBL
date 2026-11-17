# C Baseline Solvers

These modules add Parth's independent 0/1 Dynamic Programming and density-first Greedy baselines. They consume the existing ReliefCrate type from the repository's root relief_schema.h; they do not modify, replace, or include the standalone DAA_PBL.c Branch and Bound program.

## Build and test

From the repository root:

    make -C baselines
    make -C baselines test
    make -C baselines sanitize

The library is produced at baselines/build/librelief_baselines.a.

## API

Include baselines/dp_solver.h and/or baselines/greedy_solver.h. Each function accepts the sanitized ReliefCrate array, its count, and vehicle capacity in kilograms. It fills a BaselineSolution whose selected indices refer to the original array.

Example:

    BaselineSolution dp_result = {0};
    BaselineStatus status = dp_solve(crates, (size_t)crate_count, 15.0, &dp_result);
    if (status == BASELINE_OK) {
        printf("DP priority: %.2f\n", dp_result.total_priority);
    }
    baseline_solution_free(&dp_result);

The input must be the clean manifest produced by sanitize_manifest. The solvers defensively skip non-READY entries and reject malformed READY records and duplicate READY IDs. DP uses exact fixed-point units inferred from up to six decimal places; it refuses silent rounding and has a 256 MiB allocation guard. Greedy uses the original finite double values and does not discretize weights.

BASELINE_OK with zero selected crates is a valid result (for example, empty input, zero capacity, or all crates individually infeasible). Always check the returned status and call baseline_solution_free when finished and before reusing the same result object; initialize result objects as {0} before their first call.
