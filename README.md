# Non-Fractional Relief Packing
**Design and Analysis of Algorithms (DAA) PBL — Challenge #29**

This repository implements a 0/1 knapsack model for allocating indivisible relief crates to a vehicle with a strict payload limit. The objective is to maximize total priority without exceeding capacity.

## Algorithms and implementation

- **Branch and Bound:** best-first search with a fractional-knapsack upper bound in the standalone C program (\`DAA_PBL.c\`) and in the dashboard's Python adapter (\`dashboard_engine.py\`).
- **Dynamic Programming:** exact 0/1 knapsack baseline in \`baselines/dp_solver.c\`; the dashboard also has a Python adapter.
- **Greedy Density:** heuristic baseline in \`baselines/greedy_solver.c\`; the dashboard also has a Python adapter.
- **Data hygiene:** CSV validation, quarantine, and duplicate resolution in \`sanitizer.c\`.
- **Dashboard:** Streamlit interface for CSV validation, algorithm comparison, profiling, and result inspection.

**Architecture note:** The Streamlit dashboard runs the Python implementations in \`dashboard_engine.py\`. It does not call the compiled C solvers. The C implementations are separate deliverables and are tested independently.

## Team contributions

- **Mehak:** core Branch and Bound solver and search trace.
- **Parth:** C Dynamic Programming and Greedy baselines, theory, and manual verification.
- **Bhavya:** datasets, data hygiene, and C sanitizer tests.
- **Navvya:** Streamlit dashboard and profiling interface.

## Setup

Requires Python 3.10+ and Git.

\`\`\`bash
git clone https://github.com/wlfparth27/DAAPBL.git
cd DAAPBL
python -m venv .venv
\`\`\`

Activate the environment:

\`\`\`powershell
# Windows PowerShell
.venv\Scripts\Activate.ps1
\`\`\`

\`\`\`bash
# macOS/Linux
source .venv/bin/activate
\`\`\`

Install dashboard dependencies:

\`\`\`bash
python -m pip install -r requirements-dashboard.txt
\`\`\`

## Run the dashboard

\`\`\`bash
python -m streamlit run dashboard.py
\`\`\`

Streamlit prints the local URL (normally \`http://localhost:8501\`).

## Run the C baselines

With a C compiler and GNU Make:

\`\`\`bash
make -C baselines test
\`\`\`

## Data and solver limitations

- The dashboard accepts CSV manifests. Required fields are \`crate_id\`, \`weight_kg\`, and \`priority_score\`; optional fields include description, category, timestamp, fragility, and status.
- Only READY records with finite, positive weights and priorities are eligible.
- Dashboard DP uses exact integer scaling up to six decimal places and has a 256 MiB decision-table safety limit. If the required table exceeds the limit or scaled priorities exceed the safe integer range, it reports that the run was not performed.
- Dashboard Branch and Bound uses exact scaled integer weights/priorities and an exact rational fractional upper bound. If its node limit is reached, it returns the best feasible incumbent and explicitly marks optimality as unproven.
- Runtime and peak traced memory depend on the machine. Python-traced memory is not total process RSS.

## Theory and verification

See \`theory/algorithm_analysis.md\`, \`theory/correctness_proofs.md\`, and \`theory/hand_verification.md\` for complexity analysis, correctness arguments, and the hand-worked instance.
