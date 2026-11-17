# Algorithm Analysis: DP and Greedy Baselines

## Mathematical model

For each READY relief crate \`i\`, let \`w_i > 0\` be its weight in kilograms and \`v_i > 0\` its humanitarian priority score. Let \`W >= 0\` be the vehicle payload capacity. A crate is indivisible, so \`x_i\` is either zero or one:

\[
\max \sum_{i=1}^{n} v_i x_i
\quad\text{subject to}\quad
\sum_{i=1}^{n} w_i x_i \le W,\qquad x_i\in\{0,1\}.
\]

Only records whose inspection status is \`STATUS_READY\` are eligible. The solvers consume the cleaned \`ReliefCrate\` array produced by the project's sanitizer; they do not parse CSV or mutate input records.

## 1. 0/1 Dynamic Programming baseline

### State and recurrence

The implementation converts weights and capacity to a common exact integer unit inferred from the input decimal values (up to six decimal places). The table capacity is reduced to \`min(W, sum_i w_i)\` because no feasible selection can weigh more than the sum of all eligible crates. Priorities are independently converted to exact integer score units. This avoids rounding kilograms to whole numbers and preserves the repository's one-decimal generated datasets.

Let \`C\` be the scaled capacity and define \`DP_i(c)\` as the best attainable pair after considering the first \`i\` eligible crates with capacity at most \`c\`. The pair is ordered by:

1. greater total priority; then
2. lower actual total weight.

For item \`i\` with scaled weight \`a_i\` and scaled priority \`p_i\`:

\[
DP_i(c)=\operatorname{best}\left(DP_{i-1}(c),\ DP_{i-1}(c-a_i)+(p_i,a_i)\right)
\]

The include alternative is considered only when \`a_i <= c\`. If priority and weight are both equal, the implementation keeps the exclude alternative. This makes exact ties deterministic and favors an already-selected earlier input prefix.

### Complexity

Let \`n\` be the number of eligible READY crates and \`C\` the capacity measured in the inferred integer units.

- **Time:** \`O(nC)\` DP transitions.
- **DP value storage:** \`O(C)\` using two rows.
- **Reconstruction storage:** \`O(nC)\` bits, because one decision bit is retained per item/capacity pair.
- **Overall space:** \`O(C + nC/8)\` bytes, ignoring fixed-size input/result arrays.

This is pseudo-polynomial: its running time depends on the numeric capacity after scaling, not only on the number of crates. The implementation checks multiplication overflow and enforces a 256 MiB table-allocation ceiling. It returns an explicit status rather than approximating inputs with more than six decimal places or allocating an impractical table.

The selected result is reconstructed by walking the packed decisions backward from the final capacity. The API returns original input-array indices, not reordered or copied \`ReliefCrate\` records.

## 2. Density-first Greedy baseline

Each eligible crate is ranked by decreasing density \`v_i / w_i\`. The solver scans that order and accepts a crate exactly when its full weight fits in the remaining payload. No crate is fractionally selected.

For deterministic density ties, the implementation prefers lower weight, then lexicographically smaller crate ID, then original input position.

- **Time:** \`O(n log n)\` for sorting, followed by \`O(n)\` selection.
- **Space:** \`O(n)\` for candidates and selected indices.
- **Guarantee:** feasible output, but no general optimality guarantee for 0/1 Knapsack.

The ratio comparator represents each positive density as a normalized mantissa and binary exponent, avoiding overflow from directly dividing extreme finite doubles. For ordinary manifest weights with at most six decimal places, feasibility uses exact integer-unit comparisons. For weights outside that fixed-point range, the solver falls back to conservative \`long double\` comparisons. The heuristic does not mutate the manifest.

## 3. Input, numeric, and edge-case policy

- Empty input and zero capacity return a successful empty solution.
- A crate heavier than capacity is skipped; if every crate is too heavy, the result is empty.
- Non-READY inspection statuses are skipped defensively.
- READY crates must have finite, positive weights and priorities, and a nonempty null-terminated unique ID.
- Duplicate READY IDs are rejected; the sanitizer is responsible for resolving them before optimization.
- Negative, NaN, and infinite capacities are rejected.
- DP supports at most six decimal places and uses exact integer-unit comparisons after conversion; it will not silently round unsupported precision.
- DP checks integer multiplication and allocation limits before creating the decision table.
- Totals are accumulated in \`long double\` and checked before conversion to the public \`double\` result fields.

## 4. Comparison with Branch and Bound

Branch and Bound remains the project's primary exact solver. DP is an independent exact baseline with pseudo-polynomial complexity in scaled capacity. Greedy is a fast heuristic baseline used to quantify the value of exact optimization. Agreement between DP and Branch and Bound on small/medium cases provides a useful cross-check; disagreement should trigger investigation rather than an assumption that either implementation is correct.
