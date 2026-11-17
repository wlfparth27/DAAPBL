# Git Commit Guide: Parth's Contribution

This contribution is isolated to new paths. It does not edit \`DAA_PBL.c\`, the existing schema/sanitizer files, Bhavya's datasets, the team's README, or any existing test file.

The work is kept on \`parth/baselines-dp-greedy-theory\` for review before merging into \`main\`.

The logical commit sequence is:

1. \`feat(baselines): implement exact 0/1 DP and density greedy in C\` — reusable C baseline modules and isolated build target.
2. \`test(baselines): verify exact DP and feasible greedy with exhaustive cases\` — hand-verified sample, deterministic edge cases, and 200 exhaustive-oracle comparisons.
3. \`docs(theory): add baseline complexity proofs and hand verification\` — DP induction proof, Greedy feasibility and counterexample, complexity analysis, and manual calculation.
4. \`fix(baselines): preserve decimal capacity boundaries in both solvers\` — fixed-point feasibility for ordinary decimal weights and a binary-floating-point boundary regression test.

From the repository root, run:

\`\`\`bash
make -C baselines clean test
make -C baselines sanitize
git status --short
\`\`\`

Review the API and clean-manifest integration point before merging.
