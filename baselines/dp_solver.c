#include "dp_solver.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DP_MAX_DECIMAL_PLACES 6u
#define DP_MAX_SCALED_VALUE 1.0e12L
#define DP_MEMORY_LIMIT_BYTES ((size_t)256u * 1024u * 1024u)

typedef struct {
    uint64_t priority_units;
    uint64_t used_weight_units;
} DpCell;

typedef struct {
    size_t original_index;
    double weight;
    double priority;
} DpItem;

static int checked_mul_size(size_t a, size_t b, size_t *result) {
    if (a != 0 && b > SIZE_MAX / a) return 0;
    *result = a * b;
    return 1;
}

static int convert_one(double value, uint64_t scale, uint64_t *out) {
    long double scaled = (long double)value * (long double)scale;
    if (!isfinite(scaled) || scaled < 0.0L || scaled > DP_MAX_SCALED_VALUE) return 0;

    long double nearest = roundl(scaled);
    long double tolerance = 4.0L * (long double)DBL_EPSILON * fmaxl(1.0L, fabsl(scaled));
    if (fabsl(scaled - nearest) > tolerance) return 0;
    if ((value > 0.0 && nearest == 0.0L) || nearest < 0.0L ||
        nearest > (long double)UINT64_MAX) return 0;

    *out = (uint64_t)nearest;
    return 1;
}

static int choose_scale(
    const double *values,
    size_t count,
    uint64_t *units,
    uint64_t *chosen_scale
) {
    uint64_t scale = 1;
    for (unsigned int places = 0; places <= DP_MAX_DECIMAL_PLACES; ++places) {
        int valid = 1;
        for (size_t i = 0; i < count; ++i) {
            if (!convert_one(values[i], scale, &units[i])) {
                valid = 0;
                break;
            }
        }
        if (valid) {
            *chosen_scale = scale;
            return 1;
        }
        if (places < DP_MAX_DECIMAL_PLACES) scale *= 10u;
    }
    return 0;
}

static void set_choice_bit(unsigned char *bits, size_t bit_index) {
    bits[bit_index >> 3u] |= (unsigned char)(1u << (bit_index & 7u));
}

static int get_choice_bit(const unsigned char *bits, size_t bit_index) {
    return (bits[bit_index >> 3u] >> (bit_index & 7u)) & 1u;
}

static int compare_size_t(const void *left, const void *right) {
    size_t a = *(const size_t *)left;
    size_t b = *(const size_t *)right;
    return (a > b) - (a < b);
}

BaselineStatus dp_solve(
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

    DpItem *items = (DpItem *)malloc(ready_count * sizeof(*items));
    if (!items) {
        free(ready_indices);
        return BASELINE_OUT_OF_MEMORY;
    }

    size_t item_count = 0;
    for (size_t i = 0; i < ready_count; ++i) {
        size_t original = ready_indices[i];
        if (crates[original].weight_kg <= capacity_kg) {
            items[item_count].original_index = original;
            items[item_count].weight = crates[original].weight_kg;
            items[item_count].priority = crates[original].priority_score;
            ++item_count;
        }
    }
    free(ready_indices);

    if (item_count == 0) {
        free(items);
        return BASELINE_OK;
    }

    if (item_count == SIZE_MAX) {
        free(items);
        return BASELINE_RESOURCE_LIMIT;
    }

    double *weight_values = (double *)malloc((item_count + 1u) * sizeof(*weight_values));
    uint64_t *weight_units = (uint64_t *)malloc(item_count * sizeof(*weight_units));
    double *priority_values = (double *)malloc(item_count * sizeof(*priority_values));
    uint64_t *priority_units = (uint64_t *)malloc(item_count * sizeof(*priority_units));
    if (!weight_values || !weight_units || !priority_values || !priority_units) {
        free(items); free(weight_values); free(weight_units);
        free(priority_values); free(priority_units);
        return BASELINE_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < item_count; ++i) {
        weight_values[i] = items[i].weight;
        priority_values[i] = items[i].priority;
    }
    weight_values[item_count] = capacity_kg;

    uint64_t weight_scale = 0;
    uint64_t capacity_units = 0;
    uint64_t *all_weight_units = (uint64_t *)malloc((item_count + 1u) * sizeof(*all_weight_units));
    if (!all_weight_units) {
        free(items); free(weight_values); free(weight_units);
        free(priority_values); free(priority_units);
        return BASELINE_OUT_OF_MEMORY;
    }

    if (!choose_scale(weight_values, item_count + 1u, all_weight_units, &weight_scale)) {
        free(items); free(weight_values); free(weight_units); free(priority_values);
        free(priority_units); free(all_weight_units);
        return BASELINE_UNSUPPORTED_PRECISION;
    }
    for (size_t i = 0; i < item_count; ++i) weight_units[i] = all_weight_units[i];
    capacity_units = all_weight_units[item_count];
    free(all_weight_units);
    free(weight_values);

    uint64_t priority_scale = 0;
    if (!choose_scale(priority_values, item_count, priority_units, &priority_scale)) {
        free(items); free(weight_units); free(priority_values); free(priority_units);
        return BASELINE_UNSUPPORTED_PRECISION;
    }
    free(priority_values);

    uint64_t total_priority_units = 0;
    uint64_t effective_capacity_units = 0;
    for (size_t i = 0; i < item_count; ++i) {
        if (priority_units[i] > UINT64_MAX - total_priority_units) {
            free(items); free(weight_units); free(priority_units);
            return BASELINE_NUMERIC_RANGE;
        }
        total_priority_units += priority_units[i];

        /* No packing can weigh more than the sum of all eligible item weights. */
        if (weight_units[i] >= capacity_units - effective_capacity_units) {
            effective_capacity_units = capacity_units;
        } else {
            effective_capacity_units += weight_units[i];
        }
    }
    (void)total_priority_units;
    capacity_units = effective_capacity_units;

    if (capacity_units > (uint64_t)(SIZE_MAX - 1u)) {
        free(items); free(weight_units); free(priority_units);
        return BASELINE_RESOURCE_LIMIT;
    }
    size_t width = (size_t)capacity_units + 1u;
    size_t cells = 0;
    if (!checked_mul_size(item_count, width, &cells)) {
        free(items); free(weight_units); free(priority_units);
        return BASELINE_RESOURCE_LIMIT;
    }
    size_t choice_bytes = cells / 8u + ((cells % 8u) != 0u);
    size_t row_elements = 0, row_bytes = 0, total_bytes = 0;
    if (!checked_mul_size(width, 2u, &row_elements) ||
        !checked_mul_size(row_elements, sizeof(DpCell), &row_bytes) ||
        choice_bytes > SIZE_MAX - row_bytes) {
        free(items); free(weight_units); free(priority_units);
        return BASELINE_RESOURCE_LIMIT;
    }
    total_bytes = choice_bytes + row_bytes;
    if (total_bytes > DP_MEMORY_LIMIT_BYTES) {
        free(items); free(weight_units); free(priority_units);
        return BASELINE_RESOURCE_LIMIT;
    }

    unsigned char *choices = (unsigned char *)calloc(choice_bytes, sizeof(*choices));
    DpCell *row_a = (DpCell *)calloc(width, sizeof(*row_a));
    DpCell *row_b = (DpCell *)calloc(width, sizeof(*row_b));
    if (!choices || !row_a || !row_b) {
        free(items); free(weight_units); free(priority_units);
        free(choices); free(row_a); free(row_b);
        return BASELINE_OUT_OF_MEMORY;
    }

    DpCell *previous = row_a;
    DpCell *current = row_b;
    uint64_t operations = 0;

    for (size_t i = 0; i < item_count; ++i) {
        uint64_t item_weight = weight_units[i];
        uint64_t item_priority = priority_units[i];
        for (size_t c = 0; c < width; ++c) {
            DpCell best = previous[c];
            int take = 0;
            if (item_weight <= (uint64_t)c) {
                const DpCell base = previous[c - (size_t)item_weight];
                DpCell candidate;
                candidate.priority_units = base.priority_units + item_priority;
                candidate.used_weight_units = base.used_weight_units + item_weight;
                if (candidate.priority_units > best.priority_units ||
                    (candidate.priority_units == best.priority_units &&
                     candidate.used_weight_units < best.used_weight_units)) {
                    best = candidate;
                    take = 1;
                }
            }
            current[c] = best;
            if (take) set_choice_bit(choices, i * width + c);
            ++operations;
        }
        DpCell *swap = previous;
        previous = current;
        current = swap;
    }

    size_t *selected = (size_t *)malloc(item_count * sizeof(*selected));
    if (!selected) {
        free(items); free(weight_units); free(priority_units);
        free(choices); free(row_a); free(row_b);
        return BASELINE_OUT_OF_MEMORY;
    }

    size_t selected_count = 0;
    size_t remaining_capacity = (size_t)capacity_units;
    uint64_t selected_weight_units = 0;
    uint64_t selected_priority_units = 0;
    for (size_t i = item_count; i > 0; --i) {
        size_t row = i - 1u;
        size_t bit_index = row * width + remaining_capacity;
        if (get_choice_bit(choices, bit_index)) {
            selected[selected_count++] = items[row].original_index;
            if (weight_units[row] > (uint64_t)remaining_capacity ||
                weight_units[row] > UINT64_MAX - selected_weight_units ||
                priority_units[row] > UINT64_MAX - selected_priority_units) {
                free(selected); free(items); free(weight_units); free(priority_units);
                free(choices); free(row_a); free(row_b);
                return BASELINE_NUMERIC_RANGE;
            }
            remaining_capacity -= (size_t)weight_units[row];
            selected_weight_units += weight_units[row];
            selected_priority_units += priority_units[row];
        }
    }
    qsort(selected, selected_count, sizeof(*selected), compare_size_t);

    /* Report the exact scaled model totals, not a second floating-point re-sum. */
    long double total_weight = (long double)selected_weight_units / (long double)weight_scale;
    long double total_priority = (long double)selected_priority_units / (long double)priority_scale;
    if (!isfinite(total_weight) || !isfinite(total_priority) ||
        total_weight > (long double)DBL_MAX || total_priority > (long double)DBL_MAX) {
        free(selected); free(items); free(weight_units); free(priority_units);
        free(choices); free(row_a); free(row_b);
        return BASELINE_NUMERIC_RANGE;
    }

    solution->selected_indices = selected_count ? selected : NULL;
    if (selected_count == 0) free(selected);
    solution->selected_count = selected_count;
    solution->total_weight = (double)total_weight;
    solution->total_priority = (double)total_priority;
    solution->operations = operations;

    free(items); free(weight_units); free(priority_units);
    free(choices); free(row_a); free(row_b);
    (void)weight_scale;
    (void)priority_scale;
    return BASELINE_OK;
}
