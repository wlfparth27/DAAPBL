# Hand Verification: Small Relief Manifest

This verification uses the five records already present in the repository's \`small_manifest.csv\`. The vehicle capacity for this worked instance is **15.0 kg**.

## Input

| Crate ID | Weight (kg) | Priority | Density |
|---|---:|---:|---:|
| CR-101 | 2.0 | 40.0 | 20.0 |
| CR-102 | 5.0 | 30.0 | 6.0 |
| CR-103 | 10.0 | 50.0 | 5.0 |
| CR-104 | 5.0 | 10.0 | 2.0 |
| CR-105 | 3.0 | 12.0 | 4.0 |

## Manual optimum

The selected subset \`{CR-101, CR-103, CR-105}\` has:

- Total weight = \`2 + 10 + 3 = 15 kg\`.
- Total priority = \`40 + 50 + 12 = 102\`.

It is feasible and reaches priority 102. Checking all \`2^5 = 32\` possible subsets (22 are feasible at 15 kg) confirms that no feasible subset has priority greater than 102. The highest competing feasible combinations include \`{CR-101, CR-102, CR-104, CR-105}\` at priority 92, \`{CR-101, CR-103}\` at priority 90, and \`{CR-101, CR-102, CR-105}\` at priority 82.

Therefore, the hand-verified optimum for capacity 15 kg is **102 priority points**, selecting **CR-101, CR-103, and CR-105**.

## Expected baseline outputs

| Algorithm | Selected crates | Total weight | Total priority | Interpretation |
|---|---|---:|---:|---|
| Dynamic Programming | CR-101, CR-103, CR-105 | 15 kg | 102 | Exact optimum |
| Density Greedy | CR-101, CR-102, CR-105, CR-104 | 15 kg | 92 | Feasible, but below the optimum |

The density-first ordering accepts CR-101 (density 20.0), then CR-102 (6.0). CR-103 (5.0) no longer fits in the remaining 8 kg; CR-105 (4.0) and then CR-104 (2.0) fill the remaining payload. Greedy obtains 92, below the DP optimum 102. This small manifest therefore demonstrates that density ordering is not an exact 0/1 method.

The separate two-crate counterexample is also documented in \`correctness_proofs.md\` and corresponds to the existing \`edge_case_5_greedy_trap.csv\` design.
