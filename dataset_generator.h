#ifndef DATASET_GENERATOR_H
#define DATASET_GENERATOR_H

#include "relief_schema.h"

/* Three Evaluation Scales according to Project Synopsis */
int generate_small_dataset(const char *filepath);
int generate_medium_dataset(const char *filepath, int num_crates, unsigned int seed);
int generate_large_correlated_dataset(const char *filepath, int num_crates, unsigned int seed);

/* Edge Case Manifests */
int generate_edge_case_1_empty(const char *filepath);
int generate_edge_case_2_infeasible(const char *filepath, double vehicle_capacity);
int generate_edge_case_3_density_ties(const char *filepath);
int generate_edge_case_4_trivial_fit(const char *filepath, double vehicle_capacity);
int generate_edge_case_5_greedy_trap(const char *filepath, double vehicle_capacity);

/* Comprehensive Dirty Data Manifest for Sanitizer Testing */
int generate_dirty_test_manifest(const char *filepath);

/* Batch Generation of All Benchmark and Edge-Case Files */
int generate_all_datasets(const char *output_dir);

#endif /* DATASET_GENERATOR_H */
