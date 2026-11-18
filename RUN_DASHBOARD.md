# Relief Packing Optimizer — dashboard setup

## Prerequisites

- Python 3.10 or newer
- Git

## Setup

From the repository root:

```bash
python -m venv .venv
```

Activate the environment:

```powershell
# Windows PowerShell
.venv\Scripts\Activate.ps1
```

```bash
# macOS/Linux
source .venv/bin/activate
```

Install dependencies and launch Streamlit:

```bash
python -m pip install -r requirements-dashboard.txt
python -m streamlit run dashboard.py
```

Streamlit prints the local URL, normally `http://localhost:8501`.

## Datasets and uploads

The sidebar discovers the curated CSV manifests in the repository root, including the small, medium, large, dirty, and edge-case datasets. You can also upload a CSV. Required headers are `crate_id`, `weight_kg`, and `priority_score`. Optional fields include `category`, `description`, `arrival_timestamp`, `fragility_constraint`, and `status`. When category is supplied, supported values are Medical, Water, Food, and Shelter. Only READY records are eligible.

## Dashboard features

- Dataset selection and custom CSV upload.
- Manifest preview, validation summary, and quarantine report.
- Branch and Bound, Dynamic Programming, and Greedy Density comparison.
- Optional exhaustive brute-force oracle for small instances.
- Selected crates, total priority, packed weight, unused capacity, and utilization.
- Runtime, peak Python-traced memory, work counters, and B&B nodes explored/pruned.
- Progress updates and CSV exports for accepted records, quarantine, and benchmarks.

## Important architecture distinction

The dashboard executes Python adapters in `dashboard_engine.py`. It does **not** invoke the C solvers. The standalone `DAA_PBL.c` is an interactive console implementation. The C DP and Greedy baseline APIs are in `baselines/` and can be tested independently:

```bash
make -C baselines test
```

This separation avoids requiring a compiled shared library to launch the Streamlit application on Windows.

## Numerical and resource limits

- Dashboard DP uses exact integer scaling for values with up to six decimal places. It refuses to run if the decision table exceeds 256 MiB or scaled total priority exceeds signed 64-bit range.
- Dashboard Branch and Bound uses exact scaled integer weights/priorities and an exact rational fractional upper bound. If its node safety limit is reached, it reports the best feasible incumbent but marks optimality as unproven.
- Reported peak memory is Python-traced allocation, not total operating-system RSS.


## Verification

Run the Python dashboard-engine regression suite after installing the dependencies:

```bash
python -m unittest test_dashboard_engine.py
```

Run the C sanitizer and baseline suites with a C compiler and GNU Make:

```bash
make test
```
