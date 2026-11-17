# Correctness Proofs: Parth's Baselines

## 1. Dynamic Programming correctness

### Claim
For every prefix of eligible crates \`1..i\` and every scaled capacity \`c\`, the DP state stores a feasible subset with maximum total priority among all subsets of that prefix whose total weight is at most \`c\`. Among subsets with equal maximum priority, it stores one with minimum total weight.

### Proof by induction on \`i\`

**Base case (\`i = 0\`).** With no crates considered, the only available subset is the empty set. Its priority and weight are both zero. The initial DP row stores \`(0, 0)\` for every capacity, so the claim holds.

**Inductive hypothesis.** Assume the claim holds for the first \`i-1\` crates at every capacity.

**Inductive step.** Consider crate \`i\`, with scaled weight \`a_i\` and priority \`p_i\`, at capacity \`c\`.

Every feasible subset of the first \`i\` crates belongs to exactly one of two classes:

1. It excludes crate \`i\`. Its best possible result is represented by \`DP_(i-1)(c)\` by the inductive hypothesis.
2. It includes crate \`i\`. This is possible only when \`a_i <= c\`. Removing crate \`i\` leaves a feasible subset of the first \`i-1\` crates with capacity at most \`c-a_i\`. By the inductive hypothesis, the best such prefix subset is represented by \`DP_(i-1)(c-a_i)\`. Adding \`(a_i,p_i)\` therefore gives the best result in the include class.

The recurrence compares these two exhaustive classes, first maximizing priority and then minimizing total weight. It consequently stores the required optimum for prefix \`i\`. If both priority and weight tie, choosing the exclude state does not change either optimal objective and supplies a deterministic representative. By induction, the claim holds for all \`i\`, including the final row at capacity \`C\`.

The reconstruction bits record which of the two recurrence alternatives was chosen for each row/capacity. Following those decisions backward subtracts precisely the weight of every included crate, so the returned original indices describe the DP optimum.

## 2. Greedy correctness properties and limitation

### Feasibility claim
The Greedy solver returns a feasible 0/1 packing.

**Proof.** It starts with used weight zero. It accepts a crate only if its full weight is no greater than the remaining capacity. Every crate has positive weight, so after acceptance the used weight remains at most capacity. Rejected crates are never added, and each candidate appears once in the sorted list. Therefore the returned subset is indivisible and feasible.

### No optimality guarantee
Density order alone does not solve the 0/1 Knapsack problem optimally. Consider capacity \`W = 100\` and two crates:

| Crate | Weight | Priority | Density |
|---|---:|---:|---:|
| A | 1 | 2 | 2.0 |
| B | 100 | 100 | 1.0 |

Greedy selects A first because its density is higher. The remaining capacity is 99, so B no longer fits; Greedy obtains priority 2. The optimal 0/1 solution selects B and obtains priority 100. Thus the density heuristic is feasible but not guaranteed optimal.
