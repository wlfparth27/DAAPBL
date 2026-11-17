#ifndef BASELINE_COMMON_H
#define BASELINE_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include "relief_schema.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BASELINE_OK = 0,
    BASELINE_INVALID_ARGUMENT,
    BASELINE_INVALID_CAPACITY,
    BASELINE_INVALID_CRATE,
    BASELINE_DUPLICATE_ID,
    BASELINE_UNSUPPORTED_PRECISION,
    BASELINE_NUMERIC_RANGE,
    BASELINE_RESOURCE_LIMIT,
    BASELINE_OUT_OF_MEMORY
} BaselineStatus;

typedef struct {
    /* Indices refer to the original ReliefCrate array and are sorted ascending. */
    size_t *selected_indices;
    size_t selected_count;
    double total_weight;
    double total_priority;
    uint64_t operations;
} BaselineSolution;

/*
 * Call when a result is no longer needed and before passing it to another solve.
 * Solve functions expect a zero-initialized or previously freed output object.
 */
void baseline_solution_free(BaselineSolution *solution);
const char *baseline_status_string(BaselineStatus status);

/*
 * Shared validation contract:
 * - count may be zero; in that case crates may be NULL.
 * - capacity must be finite and >= 0.
 * - only STATUS_READY crates are considered; other statuses are skipped.
 * - READY crates must have a nonempty, terminated, unique crate_id and
 *   finite, strictly-positive weight and priority values.
 * - the manifest must already have passed the repository sanitizer. The
 *   solver does not parse CSV or alter the caller's array.
 */
BaselineStatus baseline_validate_input(
    const ReliefCrate *crates,
    size_t count,
    double capacity,
    size_t **ready_indices,
    size_t *ready_count
);

#ifdef __cplusplus
}
#endif
#endif
