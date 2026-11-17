#include "greedy_solver.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t original_index;
    long double density_mantissa;
    int density_exponent;
    double weight;
    double priority;
    const char *crate_id;
} GreedyCandidate;

/*
 * Represent v/w as mantissa * 2^exponent.  This avoids overflowing a
 * floating-point division for extreme but finite double inputs.
 */
static void make_density_key(double priority, double weight,
                             long double *mantissa, int *exponent) {
    int priority_exponent = 0;
    int weight_exponent = 0;
    long double priority_mantissa = frexpl((long double)priority, &priority_exponent);
    long double weight_mantissa = frexpl((long double)weight, &weight_exponent);
    int adjustment = 0;
    *mantissa = frexpl(priority_mantissa / weight_mantissa, &adjustment);
    *exponent = priority_exponent - weight_exponent + adjustment;
}

static int compare_candidates(const void *left, const void *right) {
    const GreedyCandidate *a = (const GreedyCandidate *)left;
    const GreedyCandidate *b = (const GreedyCandidate *)right;

    if (a->density_exponent > b->density_exponent) return -1;
    if (a->density_exponent < b->density_exponent) return 1;
    if (a->density_mantissa > b->density_mantissa) return -1;
    if (a->density_mantissa < b->density_mantissa) return 1;
    /* Deterministic tie policy: lighter crate, then ID, then source position. */
    if (a->weight < b->weight) return -1;
    if (a->weight > b->weight) return 1;
    int id_order = strcmp(a->crate_id, b->crate_id);
    if (id_order != 0) return id_order;
    return (a->original_index > b->original_index) - (a->original_index < b->original_index);
}

static int compare_size_t(const void *left, const void *right) {
    size_t a = *(const size_t *)left;
    size_t b = *(const size_t *)right;
    return (a > b) - (a < b);
}

BaselineStatus greedy_solve(
    const ReliefCrate *crates,
    size_t count,
    double capacity_kg,
    BaselineSolution *solution
) {
    if (!solution) return BASELINE_INVALID_ARGUMENT;
    memset(solution, 0, sizeof(*solution));

    size_t *ready_indices = NULL;
    size_t ready_count = 0;
    BaselineStatus status = baseline_validate_input(
        crates, count, capacity_kg, &ready_indices, &ready_count
    );
    if (status != BASELINE_OK) return status;
    if (ready_count == 0 || capacity_kg == 0.0) {
        free(ready_indices);
        return BASELINE_OK;
    }

    GreedyCandidate *candidates = (GreedyCandidate *)malloc(ready_count * sizeof(*candidates));
    size_t *selected = (size_t *)malloc(ready_count * sizeof(*selected));
    if (!candidates || !selected) {
        free(ready_indices); free(candidates); free(selected);
        return BASELINE_OUT_OF_MEMORY;
    }

    size_t candidate_count = 0;
    for (size_t i = 0; i < ready_count; ++i) {
        size_t index = ready_indices[i];
        const ReliefCrate *crate = &crates[index];
        if (crate->weight_kg > capacity_kg) continue;
        candidates[candidate_count].original_index = index;
        candidates[candidate_count].weight = crate->weight_kg;
        candidates[candidate_count].priority = crate->priority_score;
        make_density_key(crate->priority_score, crate->weight_kg,
                         &candidates[candidate_count].density_mantissa,
                         &candidates[candidate_count].density_exponent);
        candidates[candidate_count].crate_id = crate->crate_id;
        ++candidate_count;
    }
    free(ready_indices);

    if (candidate_count == 0) {
        free(candidates); free(selected);
        return BASELINE_OK;
    }

    qsort(candidates, candidate_count, sizeof(*candidates), compare_candidates);

    long double used_weight = 0.0L;
    long double total_priority = 0.0L;
    size_t selected_count = 0;
    for (size_t i = 0; i < candidate_count; ++i) {
        const GreedyCandidate *candidate = &candidates[i];
        if ((long double)candidate->weight <= (long double)capacity_kg - used_weight) {
            used_weight += (long double)candidate->weight;
            total_priority += (long double)candidate->priority;
            selected[selected_count++] = candidate->original_index;
        }
    }

    if (!isfinite(used_weight) || !isfinite(total_priority) ||
        used_weight > (long double)DBL_MAX || total_priority > (long double)DBL_MAX) {
        free(candidates); free(selected);
        return BASELINE_NUMERIC_RANGE;
    }

    qsort(selected, selected_count, sizeof(*selected), compare_size_t);
    solution->selected_indices = selected_count ? selected : NULL;
    if (selected_count == 0) free(selected);
    solution->selected_count = selected_count;
    solution->total_weight = (double)used_weight;
    solution->total_priority = (double)total_priority;
    solution->operations = (uint64_t)candidate_count;

    free(candidates);
    return BASELINE_OK;
}
