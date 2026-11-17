#include "dp_solver.h"
#include "greedy_solver.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static ReliefCrate make_crate(const char *id, double weight, double priority) {
    ReliefCrate crate;
    memset(&crate, 0, sizeof(crate));
    (void)snprintf(crate.crate_id, sizeof(crate.crate_id), "%s", id);
    crate.weight_kg = weight;
    crate.priority_score = priority;
    crate.status = STATUS_READY;
    crate.category = CAT_MEDICAL;
    return crate;
}

int main(void) {
    ReliefCrate crates[2] = {
        make_crate("DECIMAL-1", 0.1, 1.0),
        make_crate("DECIMAL-2", 0.2, 2.0)
    };
    BaselineSolution dp = {0};
    BaselineSolution greedy = {0};

    BaselineStatus dp_status = dp_solve(crates, 2, 0.3, &dp);
    BaselineStatus greedy_status = greedy_solve(crates, 2, 0.3, &greedy);

    int dp_ok = dp_status == BASELINE_OK && dp.selected_count == 2 &&
                fabs(dp.total_weight - 0.3) < 1e-12 &&
                fabs(dp.total_priority - 3.0) < 1e-12;
    int greedy_ok = greedy_status == BASELINE_OK && greedy.selected_count == 2 &&
                    fabs(greedy.total_weight - 0.3) < 1e-12 &&
                    fabs(greedy.total_priority - 3.0) < 1e-12;

    printf("%s DP decimal boundary (.1 + .2 <= .3)\n", dp_ok ? "[PASS]" : "[FAIL]");
    printf("%s Greedy decimal boundary (.1 + .2 <= .3)\n", greedy_ok ? "[PASS]" : "[FAIL]");

    baseline_solution_free(&dp);
    baseline_solution_free(&greedy);
    return (dp_ok && greedy_ok) ? 0 : 1;
}
