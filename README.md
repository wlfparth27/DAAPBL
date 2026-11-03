# 🚁 Non-Fractional Relief Packing
**Design and Analysis of Algorithms (DAA) PBL - Challenge #29**

[![Paradigm](https://img.shields.io/badge/Paradigm-Branch_and_Bound-blue.svg)]()
[![Problem](https://img.shields.io/badge/Model-0%2F1_Knapsack-orange.svg)]()
[![Status](https://img.shields.io/badge/Status-Prototype_Ready-success.svg)]()

This repository contains the prototype and algorithmic implementation for **Project Challenge 29: Non-Fractional Relief Packing**. The system optimizes the allocation of indivisible emergency relief crates to transport vehicles, maximizing life-saving utility without exceeding strict payload capacities.

---

## 👥 Team & Contributions

This project was built collaboratively by a 4-person team. Each member owned specific deliverables aligned with the DAA PBL rubric:

*   **Mehak (Core Solver Lead):** Implemented the Best-First Branch and Bound algorithm, the fractional upper-bound relaxation logic, state-space tracking, and search-tree trace logging.
*   **Parth (Baselines & Theory Lead):** Developed the 0/1 Dynamic Programming and Greedy baselines, formulated the mathematical proofs (upper-bound invariant), and conducted the manual verification of the small dataset.
*   **Bhavya (Dataset & Testing Lead):** Engineered the structured multi-scale datasets (Small, Medium, Large), implemented the data hygiene pipeline (handling missing/invalid records), and built the 5+ required edge case tests.
*   **Navvya (Dashboard & Profiling Lead):** Built the explainable decision interface/dashboard, implemented the empirical profiling tools (measuring memory, runtime, and node counts), and managed the reproducible environment setup.

---

## 📖 Problem Statement & Model

During disaster response, critical relief crates (e.g., medical kits, water purifiers) must be loaded onto aircraft or trucks. Because these crates are sealed and sterile, they cannot be split (**Non-Fractional**). 

We formalize this operational challenge as the **0/1 Knapsack Problem**:
*   **Objective:** Maximize the total humanitarian priority score $\sum (v_i \cdot x_i)$.
*   **Constraint:** Total weight $\sum (w_i \cdot x_i) \leq W$ (Maximum Vehicle Payload).
*   **Decision Variable:** $x_i \in \{0, 1\}$ (1 if crate $i$ is loaded, 0 otherwise).

### ⚙️ Algorithmic Methodology
Our primary solver uses **Best-First Branch and Bound (LC-B&B)**.
1.  **State Definition:** Each node represents a partial packing decision.
2.  **Bounding:** We use the continuous **Fractional Knapsack** relaxation as an upper bound. 
3.  **Pruning:** Any sub-tree whose optimal relaxed bound is less than or equal to the best integer solution found so far (the incumbent) is safely pruned.
4.  **Baselines:** The system runs alongside a **Dynamic Programming** exact solver and a **Greedy Density** heuristic for objective comparison.

---

## 🗄️ Dataset & Edge Cases

The system ingests structured CSV/JSON manifests containing: `crate_id`, `category`, `weight_kg`, `priority_score`, and `status`.

We evaluate the prototype across three scales:
1.  **Small Instance:** 5 crates, hand-verified for Checkpoint 2.
2.  **Medium Experiment:** 40 crates, simulating a standard cargo flight.
3.  **Large Stress Test:** 500+ crates, testing Branch and Bound's pruning efficiency against the $O(nW)$ DP table.

**Handled Edge Cases:**
*   Empty input manifests.
*   Infeasible capacity (all items heavier than payload).
*   Identical density ties ($v_i/w_i = v_j/w_j$).
*   Corrupted data (negative weights, missing values, duplicate IDs).
*   Extreme weight-to-value ratios.

---

## 🚀 Reproducible Environment Instructions

Follow these steps to run the prototype, execute the baselines, and view the profiling dashboard.

### Prerequisites
*   Python 3.10 or higher installed on your system.
*   Git for cloning the repository.

### Setup Steps
1. **Clone the repository:**
   ```bash
   git clone https://github.com/your-org/relief-packing-challenge29.git
   cd relief-packing-challenge29
   ```

2. **Create and activate a virtual environment (Recommended):**
   ```bash
   # Windows
   python -m venv venv
   .\venv\Scripts\activate
   
   # macOS/Linux
   python3 -m venv venv
   source venv/bin/activate
   ```

3. **Install dependencies:**
   ```bash
   pip install -r requirements.txt
   ```

### Running the System
**1. Run the Command-Line Report (Checkpoints 2 & 3):**
To execute the small hand-verified dataset with trace output:
```bash
python main.py --dataset data/small_manifest.csv --trace
```

**2. Run the Profiling Benchmark (Final Demo):**
To compare Branch and Bound vs. DP vs. Greedy on medium/large datasets:
```bash
python profile.py --scale medium
```

**3. Launch the Explainable Dashboard (Extensions):**
```bash
python dashboard.py
```
*(This will start a local web server. Open `http://localhost:5000` in your browser to view the live allocation visualizer).*

---
*Prepared for the Design and Analysis of Algorithms PBL.*