#include "baseline_common.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ID_HASH_SLOTS ((size_t)MAX_CRATES * 2u)

static uint64_t hash_id(const char *text) {
    /* 64-bit FNV-1a; IDs are short fixed-size schema fields. */
    uint64_t hash = UINT64_C(1469598103934665603);
    while (*text) {
        hash ^= (unsigned char)*text++;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

void baseline_solution_free(BaselineSolution *solution) {
    if (!solution) return;
    free(solution->selected_indices);
    memset(solution, 0, sizeof(*solution));
}

const char *baseline_status_string(BaselineStatus status) {
    switch (status) {
        case BASELINE_OK: return "success";
        case BASELINE_INVALID_ARGUMENT: return "invalid argument";
        case BASELINE_INVALID_CAPACITY: return "capacity must be finite and non-negative";
        case BASELINE_INVALID_CRATE: return "READY crate has an invalid identifier, weight, or priority";
        case BASELINE_DUPLICATE_ID: return "duplicate READY crate identifier";
        case BASELINE_UNSUPPORTED_PRECISION: return "DP input is not representable at up to six decimal places within supported numeric bounds";
        case BASELINE_NUMERIC_RANGE: return "numeric value exceeds the exact DP representation range";
        case BASELINE_RESOURCE_LIMIT: return "DP memory requirement exceeds the configured resource limit";
        case BASELINE_OUT_OF_MEMORY: return "memory allocation failed";
        default: return "unknown baseline status";
    }
}

BaselineStatus baseline_validate_input(
    const ReliefCrate *crates,
    size_t count,
    double capacity,
    size_t **ready_indices,
    size_t *ready_count
) {
    if (!ready_indices || !ready_count) return BASELINE_INVALID_ARGUMENT;
    *ready_indices = NULL;
    *ready_count = 0;

    if (!isfinite(capacity) || capacity < 0.0) return BASELINE_INVALID_CAPACITY;
    if (count > (size_t)MAX_CRATES) return BASELINE_INVALID_ARGUMENT;
    if (count > 0 && !crates) return BASELINE_INVALID_ARGUMENT;
    if (count == 0) return BASELINE_OK;

    size_t *indices = (size_t *)malloc(count * sizeof(*indices));
    const char **id_slots = (const char **)calloc(ID_HASH_SLOTS, sizeof(*id_slots));
    if (!indices || !id_slots) {
        free(indices);
        free(id_slots);
        return BASELINE_OUT_OF_MEMORY;
    }

    size_t used = 0;
    for (size_t i = 0; i < count; ++i) {
        const ReliefCrate *crate = &crates[i];
        if (crate->status != STATUS_READY) continue;

        const char *terminator = (const char *)memchr(crate->crate_id, '\0', sizeof(crate->crate_id));
        if (!terminator || terminator == crate->crate_id ||
            !isfinite(crate->weight_kg) || crate->weight_kg <= 0.0 ||
            !isfinite(crate->priority_score) || crate->priority_score <= 0.0) {
            free(indices);
            free(id_slots);
            return BASELINE_INVALID_CRATE;
        }

        size_t slot = (size_t)(hash_id(crate->crate_id) % ID_HASH_SLOTS);
        while (id_slots[slot] != NULL) {
            if (strcmp(id_slots[slot], crate->crate_id) == 0) {
                free(indices);
                free(id_slots);
                return BASELINE_DUPLICATE_ID;
            }
            slot = (slot + 1u) % ID_HASH_SLOTS;
        }
        id_slots[slot] = crate->crate_id;
        indices[used++] = i;
    }

    free(id_slots);
    if (used == 0) {
        free(indices);
        indices = NULL;
    }
    *ready_indices = indices;
    *ready_count = used;
    return BASELINE_OK;
}
