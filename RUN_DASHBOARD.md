# Relief Packing Optimizer — dashboard setup

## Files to copy

Copy these files into the **root folder** of the `DAAPBL` repository (the same folder that contains `DAA_PBL.c`, `small_manifest.csv`, `medium_manifest.csv`, and `large_manifest.csv`):

- `dashboard.py`
- `dashboard_engine.py`
- `requirements-dashboard.txt`
- `run_dashboard.bat` (optional one-click Windows launcher)
- `test_dashboard_engine.py` (optional verification tests)

Do not move the CSV manifests into a different folder. The dashboard discovers the project datasets beside `dashboard.py`.

## Windows setup (recommended)

Open PowerShell or the VS Code terminal in the `DAAPBL` root folder:

```powershell
py -m venv .venv
.\.venv\Scripts\Activate.ps1
py -m pip install -r requirements-dashboard.txt
py -m streamlit run dashboard.py
```

If PowerShell blocks virtual-environment activation, use Command Prompt instead:

```bat
py -m venv .venv
.venv\Scripts\activate.bat
py -m pip install -r requirements-dashboard.txt
py -m streamlit run dashboard.py
```

Streamlit will print a local URL, usually `http://localhost:8501`. Open it in the browser.

## Dataset selection

The sidebar's **Choose a project dataset** menu discovers the repository's curated CSVs:

- `small_manifest.csv`
- `medium_manifest.csv`
- `large_manifest.csv`
- `dirty_test_manifest.csv`
- `edge_case_1_empty.csv` through `edge_case_5_greedy_trap.csv`

The professor can instead upload a new CSV using **Upload a CSV manifest**. The minimum required headers are `crate_id`, `weight_kg`, and `priority_score`. `category`, `description`, `arrival_timestamp`, `fragility_constraint`, and `status` are also understood. When a category column is supplied, supported values are Medical, Water, Food, and Shelter. Only READY crates are eligible for packing.

The medium and large manifests start with a suggested capacity equal to 40% of validated READY weight. The small dataset and test datasets use scenario-oriented defaults (for example, 100 kg for the Greedy Trap dataset and 50 kg for the Infeasible dataset) so the intended cases are visible immediately. These are editable suggestions, not fixed project constraints.

## Dashboard features

- Dataset selection and custom CSV upload.
- Manifest preview, validation summary, and rejected-record quarantine report.
- Branch and Bound, Dynamic Programming, and Greedy Density comparison, with an optional exhaustive brute-force oracle for datasets of 20 crates or fewer.
- Selected crates, total priority, packed weight, unused capacity, and utilization.
- Runtime, peak Python-traced memory, algorithm-specific work counters, and B&B nodes explored/pruned.
- Live progress messages while Branch and Bound and DP execute.
- Benchmark runner for the small, medium, and large manifests.
- CSV downloads for the accepted manifest, quarantine report, and benchmark results.

## Important implementation note for the presentation

The repository's existing `DAA_PBL.c` is a standalone interactive console program: it reads package values through `scanf`, uses a fixed `MAX 100`, and does not load the repository CSV schema. The dashboard uses a CSV-compatible Python adapter implementing the same **best-first Branch and Bound + fractional upper bound + pruning** approach, so it can work with the 500-row large manifest without changing the teammate-owned C source.

The repository's `baselines/dp_solver.c` and `baselines/greedy_solver.c` provide the C baseline APIs. To keep this Streamlit dashboard straightforward to launch on Windows without compiling a shared C library, the dashboard's Python adapters mirror the corresponding algorithm contracts and policies. The dashboard labels these as Dynamic Programming and Greedy Density; they are not calls into the compiled C library. The C baseline source remains available in `baselines/` and its own tests can be run with:

```powershell
make -C baselines test
```

The Python DP adapter uses exact integer-scaled weights and priorities up to six decimal places, reflecting the existing DP baseline's precision policy. It has a 256 MB decision-table safety limit. Large DP cases beyond that limit are explicitly reported as not run rather than silently approximated.

Branch and Bound has a configurable node safety limit. If the limit is reached, the interface shows the best feasible packing found but explicitly marks optimality as **not proven**. Do not present a node-limited result as guaranteed optimal.

Peak memory is reported as Python-traced allocation, not total operating-system process RSS. Runtime and memory values depend on the machine and current run.

## Quick verification (optional)

Before launching the UI, the dashboard engine can be checked with the included unit tests:

```powershell
py -m unittest test_dashboard_engine.py
```

Or double-click `run_dashboard.bat` on Windows; it creates a virtual environment if missing, installs the dashboard requirements, and starts Streamlit.
