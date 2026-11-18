#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifdef _WIN32
#include <direct.h>
#define MAKE_TEST_DIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MAKE_TEST_DIR(path) mkdir(path, 0777)
#endif
#include "relief_schema.h"
#include "sanitizer.h"
#include "dataset_generator.h"

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (cond) { \
        printf("  [PASS] %s\n", msg); \
        g_tests_passed++; \
    } else { \
        printf("  [FAIL] %s (Line %d)\n", msg, __LINE__); \
        g_tests_failed++; \
    } \
} while(0)

/* Test 1: Verify Small Dataset generation & schema alignment with Page 5 */
void test_small_dataset(void) {
    printf("\n=== Running Test Suite 1: Small Benchmark Dataset (Page 5 Synopsis) ===\n");
    const char *test_path = "data/test_small_tmp.csv";
    int res = generate_small_dataset(test_path);
    TEST_ASSERT(res == 0, "generate_small_dataset executed successfully");

    ReliefCrate crates[16];
    int clean_count = 0;
    ManifestStats stats;
    res = sanitize_manifest(test_path, NULL, NULL, crates, 16, &clean_count, &stats);

    TEST_ASSERT(res == 0, "sanitize_manifest processed small dataset without error");
    TEST_ASSERT(stats.total_raw_records == 5, "Small dataset contains exactly 5 raw records");
    TEST_ASSERT(stats.accepted_ready_count == 5, "All 5 crates accepted as valid READY crates");
    TEST_ASSERT(stats.quarantined_count == 0, "Zero records quarantined in clean small instance");

    /* Verify specific crates from Page 5 Table */
    TEST_ASSERT(strcmp(crates[0].crate_id, "CR-101") == 0, "Crate 1 is CR-101");
    TEST_ASSERT(crates[0].weight_kg == 2.0, "CR-101 weight is 2.0 kg");
    TEST_ASSERT(crates[0].priority_score == 40.0, "CR-101 priority score is 40.0");
    TEST_ASSERT(crates[0].density == 20.0, "CR-101 density is 20.0");

    TEST_ASSERT(strcmp(crates[2].crate_id, "CR-103") == 0, "Crate 3 is CR-103");
    TEST_ASSERT(crates[2].weight_kg == 10.0, "CR-103 weight is 10.0 kg");
    TEST_ASSERT(crates[2].priority_score == 50.0, "CR-103 priority score is 50.0");
    TEST_ASSERT(crates[2].density == 5.0, "CR-103 density is 5.0");

    remove(test_path);
}

/* Test 2: Verify Data Hygiene Sanitizer on Dirty Dataset */
void test_dirty_manifest_sanitization(void) {
    printf("\n=== Running Test Suite 2: Automated Data Sanitization & Quarantine ===\n");
    const char *dirty_path = "data/test_dirty_tmp.csv";
    const char *clean_path = "data/test_clean_out.csv";
    const char *quar_path = "data/test_quar_out.csv";

    generate_dirty_test_manifest(dirty_path);

    ReliefCrate crates[64];
    int clean_count = 0;
    ManifestStats stats;
    int res = sanitize_manifest(dirty_path, clean_path, quar_path, crates, 64, &clean_count, &stats);

    TEST_ASSERT(res == 0, "Sanitizer executed cleanly on dirty dataset");
    TEST_ASSERT(stats.total_raw_records >= 12, "Processed all raw rows in dirty manifest");
    TEST_ASSERT(stats.missing_values_count >= 2, "Detected and quarantined missing weight & priority values");
    TEST_ASSERT(stats.invalid_weights_count >= 2, "Detected negative and zero weight anomalies");
    TEST_ASSERT(stats.non_ready_status_count >= 2, "Filtered DAMAGED and QUARANTINED inspection statuses");
    TEST_ASSERT(stats.duplicates_resolved >= 2, "Successfully detected and resolved duplicate crate_ids");
    TEST_ASSERT(stats.schema_violation_count >= 1, "Caught schema violation for invalid category");
    TEST_ASSERT(stats.accepted_ready_count > 0, "Clean records successfully preserved");

    /* Check that clean output file was created and is non-empty */
    FILE *fclean = fopen(clean_path, "r");
    TEST_ASSERT(fclean != NULL, "Clean output CSV file successfully written");
    if (fclean) fclose(fclean);

    /* The sanitizer's own nine-column clean export must be valid input again. */
    if (fclean) {
        ReliefCrate roundtrip[64];
        int roundtrip_count = 0;
        ManifestStats roundtrip_stats;
        int roundtrip_res = sanitize_manifest(clean_path, NULL, NULL, roundtrip, 64,
                                              &roundtrip_count, &roundtrip_stats);
        TEST_ASSERT(roundtrip_res == 0, "Re-imported clean CSV without parser error");
        TEST_ASSERT(roundtrip_count == clean_count, "Clean CSV round-trip preserved accepted crate count");
    }

    /* Check that quarantine log was created and contains audit trail */
    FILE *fquar = fopen(quar_path, "r");
    TEST_ASSERT(fquar != NULL, "Quarantine audit log CSV successfully written");
    if (fquar) fclose(fquar);

    remove(dirty_path);
    remove(clean_path);
    remove(quar_path);
}

/* Test 3: Deduplication with ISO-8601 Timestamp Precedence */
void test_deduplication_timestamp_rule(void) {
    printf("\n=== Running Test Suite 3: Deduplication (Latest Timestamp Invariant) ===\n");
    const char *dedup_path = "data/test_dedup_tmp.csv";
    FILE *fp = fopen(dedup_path, "w");
    if (!fp) {
        TEST_ASSERT(0, "Could not create deduplication test fixture");
        return;
    }
    fprintf(fp, "crate_id,description,category,weight_kg,priority_score,arrival_timestamp,fragility_constraint,status\n");
    /* Earlier record: priority 50.0 */
    fprintf(fp, "CR-DEDUP-01,\"Draft Kit\",Medical,10.0,50.0,2026-11-16T08:00:00Z,1,READY\n");
    /* Later record: updated weight 9.0, priority 60.0 */
    fprintf(fp, "CR-DEDUP-01,\"Final Kit\",Medical,9.0,60.0,2026-11-16T11:00:00Z,1,READY\n");
    fclose(fp);

    ReliefCrate crates[8];
    int clean_count = 0;
    ManifestStats stats;
    sanitize_manifest(dedup_path, NULL, NULL, crates, 8, &clean_count, &stats);

    TEST_ASSERT(clean_count == 1, "Duplicate crate_id merged to single active entry");
    TEST_ASSERT(stats.duplicates_resolved == 1, "Exactly one duplicate conflict resolved");
    TEST_ASSERT(crates[0].priority_score == 60.0, "Newer timestamp record (priority 60.0) superseded older record");
    TEST_ASSERT(crates[0].weight_kg == 9.0, "Newer timestamp weight (9.0 kg) retained");

    remove(dedup_path);
}

/* Test 4: Edge Case 1 - Empty Manifest (n = 0) */
void test_edge_case_1_empty(void) {
    printf("\n=== Running Test Suite 4: Edge Case 1 (Empty Manifest) ===\n");
    const char *empty_path = "data/test_empty_tmp.csv";
    generate_edge_case_1_empty(empty_path);

    ReliefCrate crates[8];
    int clean_count = 0;
    ManifestStats stats;
    int res = sanitize_manifest(empty_path, NULL, NULL, crates, 8, &clean_count, &stats);

    TEST_ASSERT(res == 0, "Sanitizer handled empty manifest gracefully without crash");
    TEST_ASSERT(stats.total_raw_records == 0, "Reported 0 raw records");
    TEST_ASSERT(clean_count == 0, "Reported 0 clean crates");
    TEST_ASSERT(stats.quarantined_count == 0, "Reported 0 quarantined records");

    remove(empty_path);
}

/* Test 5: Edge Case 2 - Infeasible Manifest (Every item w_i > W) */
void test_edge_case_2_infeasible(void) {
    printf("\n=== Running Test Suite 5: Edge Case 2 (Infeasible Manifest) ===\n");
    double vehicle_capacity = 50.0; /* Vehicle limit 50 kg */
    const char *inf_path = "data/test_infeasible_tmp.csv";
    generate_edge_case_2_infeasible(inf_path, vehicle_capacity);

    ReliefCrate crates[16];
    int clean_count = 0;
    ManifestStats stats;
    sanitize_manifest(inf_path, NULL, NULL, crates, 16, &clean_count, &stats);

    TEST_ASSERT(clean_count == 4, "All 4 crates syntactically valid READY records");
    bool all_exceed = true;
    for (int i = 0; i < clean_count; i++) {
        if (crates[i].weight_kg <= vehicle_capacity) {
            all_exceed = false;
        }
    }
    TEST_ASSERT(all_exceed == true, "Every single crate strictly exceeds vehicle capacity W = 50.0 kg");

    remove(inf_path);
}

/* Test 6: Edge Case 3 - Identical Density Ties */
void test_edge_case_3_density_ties(void) {
    printf("\n=== Running Test Suite 6: Edge Case 3 (Identical Density Ties) ===\n");
    const char *tie_path = "data/test_ties_tmp.csv";
    generate_edge_case_3_density_ties(tie_path);

    ReliefCrate crates[16];
    int clean_count = 0;
    ManifestStats stats;
    sanitize_manifest(tie_path, NULL, NULL, crates, 16, &clean_count, &stats);

    TEST_ASSERT(clean_count == 5, "Parsed 5 crates with density ties");
    bool all_five_ratio = true;
    for (int i = 0; i < clean_count; i++) {
        if (crates[i].density != 5.0) {
            all_five_ratio = false;
        }
    }
    TEST_ASSERT(all_five_ratio == true, "Verified all 5 items have identical density ratio = 5.0");

    remove(tie_path);
}

/* Test 7: Edge Case 4 - Under-Capacity Trivial Fit */
void test_edge_case_4_trivial_fit(void) {
    printf("\n=== Running Test Suite 7: Edge Case 4 (Under-Capacity Trivial Fit) ===\n");
    double vehicle_capacity = 100.0;
    const char *fit_path = "data/test_fit_tmp.csv";
    generate_edge_case_4_trivial_fit(fit_path, vehicle_capacity);

    ReliefCrate crates[16];
    int clean_count = 0;
    ManifestStats stats;
    sanitize_manifest(fit_path, NULL, NULL, crates, 16, &clean_count, &stats);

    TEST_ASSERT(clean_count == 6, "All 6 crates accepted");
    TEST_ASSERT(stats.total_clean_weight <= vehicle_capacity, "Total manifest weight (36.0 kg) <= capacity (100.0 kg)");
    printf("  [INFO] Total Weight: %.1f kg / Capacity: %.1f kg -> Trivial 100%% packing possible\n",
           stats.total_clean_weight, vehicle_capacity);

    remove(fit_path);
}

/* Test 8: Edge Case 5 - Greedy Density Failure Trap */
void test_edge_case_5_greedy_trap(void) {
    printf("\n=== Running Test Suite 8: Edge Case 5 (Greedy Heuristic Failure Trap) ===\n");
    double vehicle_capacity = 100.0;
    const char *trap_path = "data/test_trap_tmp.csv";
    generate_edge_case_5_greedy_trap(trap_path, vehicle_capacity);

    ReliefCrate crates[8];
    int clean_count = 0;
    ManifestStats stats;
    sanitize_manifest(trap_path, NULL, NULL, crates, 8, &clean_count, &stats);

    TEST_ASSERT(clean_count == 2, "Both adversarial crates loaded");
    TEST_ASSERT(crates[0].density > crates[1].density, "Crate 1 has higher density (2.0 > 1.0)");
    TEST_ASSERT(crates[1].priority_score > crates[0].priority_score * 40.0, "Crate 2 has massive utility (100.0 vs 2.0)");

    /* Simulate Greedy vs Optimal */
    double greedy_value = crates[0].priority_score; /* picks higher density item (weight 1kg, val 2.0) */
    double optimal_value = crates[1].priority_score; /* picks item 2 (weight 100kg, val 100.0) */
    double optimality_gap_pct = ((optimal_value - greedy_value) / optimal_value) * 100.0;

    printf("  [INFO] Greedy Value: %.1f | Optimal B&B Value: %.1f | Optimality Gap: %.1f%%\n",
           greedy_value, optimal_value, optimality_gap_pct);
    TEST_ASSERT(optimality_gap_pct >= 95.0, "Verified massive optimality gap where greedy heuristic fails catastrophically");

    remove(trap_path);
}

int main(void) {
    if (MAKE_TEST_DIR("data") != 0) {
        /* An existing directory is fine; verify that it is writable. */
        FILE *probe = fopen("data/.write_probe", "w");
        if (!probe) {
            fprintf(stderr, "Unable to create or access the data/ test directory.\n");
            return 2;
        }
        fclose(probe);
        remove("data/.write_probe");
    }
    printf("========================================================================\n");
    printf(" PROJECT #29: NON-FRACTIONAL RELIEF PACKING\n");
    printf(" MEMBER 3 (BHAVYA) - DATASET HYGIENE & EDGE-CASE TEST SUITE\n");
    printf("========================================================================\n");

    test_small_dataset();
    test_dirty_manifest_sanitization();
    test_deduplication_timestamp_rule();
    test_edge_case_1_empty();
    test_edge_case_2_infeasible();
    test_edge_case_3_density_ties();
    test_edge_case_4_trivial_fit();
    test_edge_case_5_greedy_trap();

    printf("\n------------------------------------------------------------------------\n");
    printf("TEST RUN COMPLETE: %d Passed, %d Failed.\n", g_tests_passed, g_tests_failed);
    printf("------------------------------------------------------------------------\n");

    if (g_tests_failed == 0) {
        printf(">>> ALL TEST SUITES PASSED SANITIZATION & RUBRIC SPECIFICATIONS! <<<\n\n");
        return 0;
    } else {
        printf(">>> WARNING: SOME TESTS FAILED. PLEASE CHECK LOGS. <<<\n\n");
        return 1;
    }
}
