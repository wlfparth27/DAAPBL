#include "dataset_generator.h"
#include "sanitizer.h"
#include "dp_solver.h"
#include "greedy_solver.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char *manifest_path = "build/pipeline_small_manifest.csv";
    ReliefCrate crates[32];
    int clean_count = 0;
    ManifestStats stats;
    BaselineSolution dp = {0};
    BaselineSolution greedy = {0};
    int failures = 0;

    if (generate_small_dataset(manifest_path) != 0) {
        fprintf(stderr, "[FAIL] Could not generate integration fixture\n");
        return 1;
    }
    if (sanitize_manifest(manifest_path, NULL, NULL, crates, 32,
                          &clean_count, &stats) != 0) {
        fprintf(stderr, "[FAIL] Sanitizer rejected generated integration fixture\n");
        remove(manifest_path);
        return 1;
    }
    printf("[INFO] Sanitizer accepted %d READY crates from %d raw records\n",
           clean_count, stats.total_raw_records);
    if (clean_count != 5 || stats.accepted_ready_count != 5 ||
        stats.quarantined_count != 0) {
        fprintf(stderr, "[FAIL] Unexpected sanitizer output counts\n");
        ++failures;
    }

    BaselineStatus dp_status = dp_solve(crates, (size_t)clean_count, 15.0, &dp);
    BaselineStatus greedy_status = greedy_solve(crates, (size_t)clean_count, 15.0, &greedy);

    if (dp_status != BASELINE_OK) {
        fprintf(stderr, "[FAIL] DP returned %s\n", baseline_status_string(dp_status));
        ++failures;
    } else if (fabs(dp.total_priority - 102.0) > 1e-9 ||
               fabs(dp.total_weight - 15.0) > 1e-9) {
        fprintf(stderr, "[FAIL] DP expected 102 priority at 15 kg; got %.6f at %.6f kg\n",
                dp.total_priority, dp.total_weight);
        ++failures;
    } else {
        printf("[PASS] Sanitizer -> DP: %.2f priority at %.2f kg\n",
               dp.total_priority, dp.total_weight);
    }

    if (greedy_status != BASELINE_OK) {
        fprintf(stderr, "[FAIL] Greedy returned %s\n", baseline_status_string(greedy_status));
        ++failures;
    } else if (greedy.total_weight > 15.0 + 1e-9 ||
               greedy.total_priority > 102.0 + 1e-9) {
        fprintf(stderr, "[FAIL] Greedy returned an invalid or unexpectedly superior result\n");
        ++failures;
    } else {
        printf("[PASS] Sanitizer -> Greedy: %.2f priority at %.2f kg (feasible)\n",
               greedy.total_priority, greedy.total_weight);
    }

    if (dp_status == BASELINE_OK && greedy_status == BASELINE_OK &&
        greedy.total_priority <= dp.total_priority + 1e-9) {
        printf("[PASS] DP objective is at least the Greedy heuristic objective\n");
    } else {
        fprintf(stderr, "[FAIL] DP/Greedy objective relationship violated\n");
        ++failures;
    }

    baseline_solution_free(&dp);
    baseline_solution_free(&greedy);
    remove(manifest_path);

    if (failures) {
        fprintf(stderr, "PIPELINE INTEGRATION FAILED: %d issue(s)\n", failures);
        return 1;
    }
    printf("PIPELINE INTEGRATION PASSED\n");
    return 0;
}
