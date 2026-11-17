# Git Commit Guide: Parth's Contribution

This contribution is isolated to new paths. It does not edit \`DAA_PBL.c\`, the existing schema/sanitizer files, Bhavya's datasets, the team's README, or any existing test file.

From the repository root after fetching the contribution branch:

\`\`\`bash
git switch main
git pull --ff-only
git switch -c parth/baselines-dp-greedy-theory

# Commit 1: reusable C baseline modules and independent build target.
git add baselines/.gitignore baselines/Makefile baselines/README.md \\
  baselines/baseline_common.c baselines/baseline_common.h \\
  baselines/dp_solver.c baselines/dp_solver.h \\
  baselines/greedy_solver.c baselines/greedy_solver.h
git commit -m "feat(baselines): implement exact 0/1 DP and density greedy in C"

# Commit 2: deterministic, edge-case, and exhaustive-oracle tests.
git add tests/test_baselines.c
git commit -m "test(baselines): verify exact DP and feasible greedy with exhaustive cases"

# Commit 3: DAA analysis and individually checkable proofs.
git add theory/algorithm_analysis.md theory/correctness_proofs.md \\
  theory/hand_verification.md theory/git_commit_guide.md
git commit -m "docs(theory): add baseline complexity proofs and hand verification"

# Re-run before publishing.
make -C baselines clean test
make -C baselines sanitize

git status --short
git push -u origin parth/baselines-dp-greedy-theory
\`\`\`

Open a pull request from \`parth/baselines-dp-greedy-theory\` into \`main\`. Do not merge until the team has reviewed the API and confirmed that its clean-manifest integration point is correct.
