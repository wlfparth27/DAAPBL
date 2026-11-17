from __future__ import annotations

import csv
import io
import json
from pathlib import Path
from typing import Optional

import pandas as pd
import streamlit as st

from dashboard_engine import (
    ManifestError,
    parse_manifest,
    run_solver_suite,
    solve_brute_force,
)

ROOT = Path(__file__).resolve().parent
MAIN_DATASETS = ["small_manifest.csv", "medium_manifest.csv", "large_manifest.csv"]
EDGE_CASE_DEFAULTS = {
    "edge_case_1_empty.csv": 0.0,
    "edge_case_2_infeasible.csv": 50.0,
    "edge_case_3_density_ties.csv": 20.0,
    "edge_case_4_trivial_fit.csv": 36.0,
    "edge_case_5_greedy_trap.csv": 100.0,
    "dirty_test_manifest.csv": 30.0,
    "small_manifest.csv": 15.0,
}

st.set_page_config(
    page_title="Relief Packing Optimizer",
    layout="wide",
    initial_sidebar_state="expanded",
)

st.markdown(
    """
    <style>
      @import url('https://fonts.googleapis.com/css2?family=DM+Sans:wght@400;500;600;700;800&family=Playfair+Display:wght@600;700;800&display=swap');
      :root {
        --slate: #1B2A3D;
        --slate-deep: #142131;
        --slate-soft: #263950;
        --slate-panel: #22344A;
        --cream: #EDE3C8;
        --cream-dim: #cfc3a7;
        --burgundy: #5C1F2E;
        --burgundy-bright: #783044;
        --burgundy-soft: rgba(92,31,46,.38);
        --line: rgba(237,227,200,.18);
        --muted: #b8b6ad;
        --shadow: 0 18px 50px rgba(4,10,18,.22);
      }
      html, body, [class*="css"] { font-family: 'DM Sans', sans-serif; }
      .stApp {
        color: var(--cream);
        background:
          radial-gradient(ellipse at 84% 4%, rgba(92,31,46,.32), transparent 36%),
          radial-gradient(ellipse at 8% 82%, rgba(237,227,200,.055), transparent 28%),
          linear-gradient(145deg, #1B2A3D 0%, #182638 58%, #1B2A3D 100%);
      }
      .stApp:before {
        content: ""; position: fixed; inset: 0; z-index: 0; pointer-events: none;
        opacity: .12;
        background-image: radial-gradient(rgba(237,227,200,.55) .6px, transparent .6px);
        background-size: 23px 23px;
        mask-image: linear-gradient(120deg, transparent, #000 25%, #000 80%, transparent);
      }
      .main .block-container {
        position: relative; z-index: 1; max-width: 1460px;
        padding: 2.2rem 2.4rem 4rem;
      }
      header[data-testid="stHeader"] { background: rgba(27,42,61,.72); }
      [data-testid="stToolbar"] { right: 1.2rem; }
      [data-testid="stSidebar"] {
        background: linear-gradient(180deg, #172538 0%, #1B2A3D 55%, #172538 100%);
        border-right: 1px solid rgba(237,227,200,.16);
        box-shadow: 12px 0 40px rgba(4,10,18,.12);
      }
      [data-testid="stSidebar"] > div:first-child { padding: 1.65rem 1.15rem 2rem; }
      [data-testid="stSidebar"] h2, [data-testid="stSidebar"] h3 {
        color: var(--cream); font-family: 'Playfair Display', Georgia, serif;
        font-weight: 700; letter-spacing: .01em;
      }
      [data-testid="stSidebar"] label, [data-testid="stSidebar"] p { color: var(--cream-dim); }
      [data-testid="stSidebar"] hr { border-color: var(--line); margin: 1.25rem 0; }

      .hero {
        position: relative; overflow: hidden; text-align: left;
        padding: 3rem 3.35rem 3.05rem; margin: .1rem 0 1.85rem;
        color: var(--cream);
        background:
          radial-gradient(ellipse at 93% 12%, rgba(92,31,46,.48), transparent 43%),
          linear-gradient(112deg, rgba(35,52,74,.98), rgba(27,42,61,.96) 62%, rgba(40,40,55,.94));
        border: 1px solid rgba(237,227,200,.2);
        border-left: 4px solid var(--burgundy);
        border-radius: 5px 20px 5px 20px;
        box-shadow: var(--shadow);
        animation: luxFade .75s cubic-bezier(.2,.75,.25,1) both;
      }
      .hero:before {
        content: ""; position: absolute; right: -75px; top: -125px;
        width: 340px; height: 340px; border-radius: 50%;
        border: 1px solid rgba(237,227,200,.15);
        box-shadow: 0 0 0 22px rgba(237,227,200,.025), 0 0 0 46px rgba(237,227,200,.018);
        pointer-events: none;
      }
      .hero:after {
        content: ""; position: absolute; left: 3.35rem; bottom: 1.65rem;
        width: 76px; height: 2px; background: var(--burgundy-bright);
        box-shadow: 88px 0 0 rgba(237,227,200,.35); pointer-events: none;
      }
      .hero-kicker {
        color: var(--cream-dim); font-size: .7rem; letter-spacing: .25em;
        font-weight: 800; text-transform: uppercase; margin-bottom: .85rem;
      }
      .hero h1 {
        position: relative; z-index: 1; font-family: 'Playfair Display', Georgia, serif;
        font-size: clamp(2.8rem, 5.1vw, 4.55rem); line-height: 1.08;
        font-weight: 800; letter-spacing: -.035em; margin: .1rem 0 .9rem;
        color: var(--cream); text-shadow: 0 4px 24px rgba(0,0,0,.18);
      }
      .hero p {
        position: relative; z-index: 1; color: #d2cbb9; margin: 0;
        max-width: 820px; font-size: .98rem; line-height: 1.7; letter-spacing: .005em;
      }
      .section-label {
        font-size: .68rem; font-weight: 800; letter-spacing: .2em;
        color: #d5c8aa; text-transform: uppercase; margin: .4rem 0 .75rem;
      }
      .metric-card {
        border: 1px solid var(--line); border-radius: 10px; padding: 1rem 1.1rem;
        background: linear-gradient(145deg, rgba(38,57,80,.9), rgba(27,42,61,.84));
        min-height: 104px; box-shadow: 0 8px 24px rgba(4,10,18,.1);
        transition: transform .24s ease, border-color .24s ease, box-shadow .24s ease;
      }
      .metric-card:hover { transform: translateY(-3px); border-color: rgba(237,227,200,.42); box-shadow: 0 14px 30px rgba(4,10,18,.18); }
      .metric-label { color: var(--muted); font-size: .76rem; font-weight: 600; }
      .metric-value { font-family: 'Playfair Display', Georgia, serif; font-size: 1.9rem; font-weight: 800; margin-top: .3rem; color: var(--cream); }
      .muted, [data-testid="stCaptionContainer"] { color: var(--muted); }
      div[data-testid="stMetric"] {
        border: 1px solid rgba(237,227,200,.17); padding: 1.05rem 1.15rem;
        border-radius: 10px; background: linear-gradient(145deg, rgba(38,57,80,.94), rgba(27,42,61,.86));
        box-shadow: 0 8px 24px rgba(4,10,18,.11);
        transition: transform .22s ease, border-color .22s ease, box-shadow .22s ease;
      }
      div[data-testid="stMetric"]:hover { transform: translateY(-3px); border-color: rgba(237,227,200,.4); box-shadow: 0 14px 28px rgba(4,10,18,.2); }
      div[data-testid="stMetricLabel"] p { color: #c8c0ae; font-size: .76rem; font-weight: 600; }
      div[data-testid="stMetricValue"] { color: var(--cream); font-family: 'Playfair Display', Georgia, serif; font-weight: 800; }
      div[data-testid="stMetricDelta"] { color: #d8cbaa; }

      .stButton > button, .stDownloadButton > button {
        border-radius: 8px; border: 1px solid rgba(237,227,200,.34); font-weight: 700;
        background: rgba(92,31,46,.32); color: var(--cream);
        transition: transform .2s ease, background .2s ease, border-color .2s ease, box-shadow .2s ease;
      }
      .stButton > button:hover, .stDownloadButton > button:hover {
        transform: translateY(-2px); border-color: var(--cream); color: var(--cream);
        background: rgba(92,31,46,.72); box-shadow: 0 8px 20px rgba(4,10,18,.2);
      }
      .stButton > button:active, .stDownloadButton > button:active { transform: translateY(0) scale(.99); }
      .stButton > button[kind="primary"] {
        background: linear-gradient(135deg, #5C1F2E, #713044);
        border: 1px solid rgba(237,227,200,.38); color: var(--cream);
        border-radius: 8px; font-weight: 800; padding: .68rem 1.25rem;
        box-shadow: 0 8px 22px rgba(92,31,46,.24);
      }
      .stButton > button[kind="primary"]:hover {
        background: linear-gradient(135deg, #713044, #84384f);
        border-color: var(--cream); color: #fff9e9; box-shadow: 0 12px 28px rgba(92,31,46,.32);
      }
      div[data-testid="stTabs"] { border-bottom: 1px solid var(--line); margin-bottom: 1.2rem; }
      div[data-testid="stTabs"] button {
        color: #c4bdaF; font-weight: 700; letter-spacing: .025em;
        padding: .72rem 1rem; transition: color .2s ease, background .2s ease;
      }
      div[data-testid="stTabs"] button[aria-selected="true"] {
        color: var(--cream) !important; border-bottom-color: var(--burgundy) !important;
      }
      div[data-testid="stTabs"] button:hover { color: #fff6df; background: rgba(92,31,46,.13); }
      div[data-baseweb="select"] > div, div[data-baseweb="input"] > div, div[data-baseweb="textarea"] > div {
        background: #24364c !important; border-color: rgba(237,227,200,.2) !important;
        border-radius: 8px !important; color: var(--cream) !important;
        transition: border-color .2s ease, box-shadow .2s ease;
      }
      div[data-baseweb="select"] > div:focus-within, div[data-baseweb="input"] > div:focus-within,
      div[data-baseweb="textarea"] > div:focus-within { border-color: rgba(237,227,200,.65) !important; box-shadow: 0 0 0 2px rgba(92,31,46,.3) !important; }
      div[data-baseweb="select"] span, div[data-baseweb="input"] input, div[data-baseweb="textarea"] textarea { color: var(--cream) !important; }
      div[data-baseweb="popover"] div[role="option"] { background: #24364c; color: var(--cream); }
      div[data-baseweb="popover"] div[role="option"]:hover { background: #5C1F2E; }
      [data-testid="stMultiSelect"] span[data-baseweb="tag"] { background: #5C1F2E !important; color: var(--cream) !important; border: 1px solid rgba(237,227,200,.25); border-radius: 5px; }
      [data-testid="stFileUploader"] section { border: 1px dashed rgba(237,227,200,.38); background: rgba(38,57,80,.55); border-radius: 10px; }
      [data-testid="stFileUploader"] button { border: 1px solid rgba(237,227,200,.34); background: rgba(92,31,46,.36); color: var(--cream); border-radius: 7px; }
      [data-testid="stDataFrame"], [data-testid="stTable"] { border: 1px solid rgba(237,227,200,.2); border-radius: 10px; overflow: hidden; }
      [data-testid="stDataFrame"] iframe { color-scheme: dark; }
      [data-testid="stProgress"] > div > div { background: var(--burgundy) !important; }
      [data-testid="stStatusWidget"] { background: rgba(38,57,80,.72); border: 1px solid var(--line); border-radius: 10px; }
      [data-testid="stExpander"] { border: 1px solid var(--line); border-radius: 9px; background: rgba(38,57,80,.36); }
      .status-pill {
        display:inline-block; padding:.34rem .72rem; margin:.18rem .15rem .18rem 0;
        border-radius:999px; font-size:.7rem; letter-spacing:.025em; font-weight:800;
        background:rgba(92,31,46,.35); color:var(--cream); border:1px solid rgba(237,227,200,.22);
      }
      [data-testid="stAlert"] { border-radius: 9px; border-left-color: var(--burgundy); background: rgba(38,57,80,.82); }
      hr { border-color: var(--line); }
      code { color: var(--cream); background: rgba(92,31,46,.24); }
      .main [data-testid="stVerticalBlock"] { gap: .9rem; }
      @keyframes luxFade { from { opacity: 0; transform: translateY(14px); } to { opacity: 1; transform: translateY(0); } }
      @media (prefers-reduced-motion: reduce) { *, *::before, *::after { animation-duration: .01ms !important; transition-duration: .01ms !important; scroll-behavior: auto !important; } }
      @media (max-width: 800px) {
        .main .block-container { padding: 1.2rem 1rem 2.5rem; }
        .hero { padding: 2.1rem 1.5rem 2.4rem; border-radius: 4px 14px 4px 14px; }
        .hero:after { left: 1.5rem; bottom: 1.2rem; }
        .hero h1 { font-size: clamp(2.35rem, 9vw, 3.2rem); }
      }
    </style>
    """,
    unsafe_allow_html=True,
)


def find_datasets() -> list[str]:
    found = []
    for path in ROOT.glob("*.csv"):
        name = path.name.lower()
        if name in MAIN_DATASETS or name == "dirty_test_manifest.csv" or name.startswith("edge_case_"):
            found.append(path.name)
    order = {name: index for index, name in enumerate(MAIN_DATASETS)}
    return sorted(found, key=lambda x: (0, order[x]) if x in order else (1, x))


def load_text(filename: str) -> str:
    path = ROOT / filename
    return path.read_text(encoding="utf-8-sig")


def crates_frame(crates) -> pd.DataFrame:
    return pd.DataFrame([
        {
            "Crate ID": c.crate_id,
            "Description": c.description,
            "Category": c.category,
            "Weight (kg)": c.weight_kg,
            "Priority": c.priority_score,
            "Priority / kg": round(c.priority_score / c.weight_kg, 3),
            "Status": c.status,
        }
        for c in crates
    ])


def quarantine_frame(quarantine: list[dict]) -> pd.DataFrame:
    return pd.DataFrame(quarantine, columns=["line_number", "crate_id", "rejection_reason", "action_taken"]).rename(
        columns={
            "line_number": "CSV line",
            "crate_id": "Crate ID",
            "rejection_reason": "Reason",
            "action_taken": "Action taken",
        }
    )


def result_frame(results) -> pd.DataFrame:
    rows = []
    for result in results:
        rows.append({
            "Algorithm": result.algorithm,
            "Priority score": round(result.total_priority, 3) if result.status.startswith("Not run:") is False else "—",
            "Weight (kg)": round(result.total_weight, 3) if result.status.startswith("Not run:") is False else "—",
            "Runtime (ms)": round(result.runtime_seconds * 1000, 3),
            "Work counter": f"{result.work_count:,}",
            "Nodes explored": f"{result.nodes_explored:,}" if result.algorithm == "Branch and Bound" else "—",
            "Nodes pruned": f"{result.nodes_pruned:,}" if result.algorithm == "Branch and Bound" else "—",
            "Peak traced memory (MB)": round(result.peak_traced_mb, 3),
            "Optimality": ("Proven" if result.optimal else "Not proven") if result.algorithm != "Greedy Density" else "Heuristic",
            "Status": result.status,
        })
    return pd.DataFrame(rows)


def csv_bytes(rows: list[dict], fieldnames: list[str]) -> bytes:
    buf = io.StringIO()
    writer = csv.DictWriter(buf, fieldnames=fieldnames)
    writer.writeheader()
    writer.writerows(rows)
    return buf.getvalue().encode("utf-8")


def run_progress_factory(placeholder, progress_bar, max_nodes: int):
    def callback(algorithm: str, current: int, maximum: int, incumbent: float, pruned: int):
        if algorithm == "Branch and Bound":
            fraction = min(1.0, current / max(1, max_nodes))
            progress_bar.progress(fraction, text=f"Branch & Bound · {current:,} nodes explored (node budget {max_nodes:,})")
            placeholder.caption(f"Best feasible priority: **{incumbent:,.2f}**  ·  Nodes pruned: **{pruned:,}**")
        elif algorithm == "Dynamic Programming":
            fraction = min(1.0, current / max(1, maximum))
            progress_bar.progress(fraction, text=f"Dynamic Programming · processing crate {current:,}/{maximum:,}")
            placeholder.caption(f"Evaluated DP transitions so far: **{int(incumbent):,}**")
    return callback


st.markdown(
    """
    <div class="hero">
      <div class="hero-kicker">Disaster response &nbsp;·&nbsp; Decision support</div>
      <h1>Relief Packing Optimizer</h1>
      <p>Intelligent allocation of essential relief crates, balancing maximum humanitarian priority with limited vehicle capacity.</p>
    </div>
    """,
    unsafe_allow_html=True,
)

with st.sidebar:
    st.markdown("### Run configuration")
    dataset_files = find_datasets()
    if not dataset_files:
        st.error("No project CSV datasets found. Place dashboard.py in the repository root beside the *_manifest.csv files.")
        st.stop()
    selected_file = st.selectbox("Choose a project dataset", dataset_files, index=0)
    uploaded = st.file_uploader("Or upload a CSV manifest", type=["csv"], help="Your CSV can use crate_id, weight_kg and priority_score. Category and status are also supported.")
    source_name = uploaded.name if uploaded is not None else selected_file
    try:
        csv_text = uploaded.getvalue().decode("utf-8-sig") if uploaded is not None else load_text(selected_file)
        crates, quarantine, hygiene = parse_manifest(csv_text)
    except (UnicodeDecodeError, OSError, ManifestError) as exc:
        st.error(f"Dataset could not be loaded: {exc}")
        st.stop()

    total_ready_weight = sum(c.weight_kg for c in crates)
    suggested_capacity = EDGE_CASE_DEFAULTS.get(source_name, max(0.0, round(total_ready_weight * 0.4, 1)))
    capacity = st.number_input(
        "Vehicle capacity (kg)", min_value=0.0, value=float(suggested_capacity),
        step=1.0, format="%.2f", key=f"capacity_{source_name}",
        help="Editable capacity. Main scale manifests start at 40% of READY load; edge-case defaults are chosen to trigger the named test scenario.",
    )
    st.caption(f"{len(crates)} valid READY crates · {hygiene.quarantined_count} quarantined")
    st.markdown("---")
    algorithms = st.multiselect(
        "Algorithms to compare",
        ["Branch and Bound", "Dynamic Programming", "Greedy Density"],
        default=["Branch and Bound", "Dynamic Programming", "Greedy Density"],
    )
    max_nodes = st.select_slider("B&B node safety limit", options=[100_000, 250_000, 500_000, 1_000_000, 2_000_000], value=1_000_000, format_func=lambda x: f"{x:,}")
    brute_check = st.checkbox("Verify with brute force (up to 20 crates)", value=(len(crates) <= 20), disabled=(len(crates) > 20))
    run_clicked = st.button("Run optimization", type="primary", use_container_width=True, disabled=not algorithms)

opt_tab, benchmark_tab, quality_tab = st.tabs(["Optimization", "Performance lab", "Data quality"])

with opt_tab:
    left, right = st.columns([1.35, 1], gap="large")
    with left:
        st.markdown('<div class="section-label">Dataset preview</div>', unsafe_allow_html=True)
        st.markdown(f"**{source_name}**  ·  {len(crates)} accepted READY crates")
        if crates:
            st.dataframe(crates_frame(crates), use_container_width=True, hide_index=True, height=265)
        else:
            st.info("This manifest has no valid READY crates. The correct result is an empty packing plan.")
    with right:
        st.markdown('<div class="section-label">Capacity configuration</div>', unsafe_allow_html=True)
        st.metric("Vehicle capacity", f"{capacity:,.2f} kg")
        st.metric("Total validated crate weight", f"{total_ready_weight:,.2f} kg")
        utilisation = (capacity / total_ready_weight * 100) if total_ready_weight > 0 else 0
        st.caption(f"Capacity is {utilisation:,.1f}% of the total validated load. You can change it in the sidebar.")
        st.markdown("**Included solvers**")
        for algo in algorithms:
            st.markdown(f"<span class='status-pill'>{algo}</span>", unsafe_allow_html=True)

    if run_clicked:
        with st.status("Running optimization pipeline…", expanded=True) as status_box:
            progress = st.progress(0.0, text="Preparing validated manifest…")
            live_text = st.empty()
            callback = run_progress_factory(live_text, progress, max_nodes)
            results = run_solver_suite(crates, float(capacity), tuple(algorithms), callback, max_nodes=max_nodes)
            if brute_check and len(crates) <= 20:
                live_text.caption("Running exhaustive correctness oracle on this small dataset…")
                results.append(solve_brute_force(crates, float(capacity)))
            progress.progress(1.0, text="Selected algorithms finished.")
            live_text.caption("Optimization run completed. Review the results below.")
            status_box.update(label="Optimization pipeline completed", state="complete", expanded=False)
        st.session_state["last_run"] = {
            "source": source_name,
            "capacity": float(capacity),
            "crates": crates,
            "results": results,
            "quarantine": quarantine,
            "hygiene": hygiene,
        }

    run_data = st.session_state.get("last_run")
    if run_data and run_data["source"] == source_name and abs(run_data["capacity"] - float(capacity)) < 1e-9:
        results = run_data["results"]
        st.markdown("---")
        st.markdown('<div class="section-label">Optimization outcome</div>', unsafe_allow_html=True)
        exact_results = [r for r in results if r.algorithm in {"Branch and Bound", "Dynamic Programming", "Brute Force Oracle"} and r.optimal and not r.status.startswith("Not run:")]
        completed_feasible = [r for r in results if not r.status.startswith("Not run:")]
        if exact_results:
            best = max(exact_results, key=lambda r: r.total_priority)
            title = "Best exact solution" if len(exact_results) > 1 else f"{best.algorithm} solution"
        elif completed_feasible:
            best = max(completed_feasible, key=lambda r: r.total_priority)
            title = "Best feasible solution found"
        else:
            best = None
            title = "No completed result"

        if best:
            m1, m2, m3, m4 = st.columns(4)
            m1.metric("Total relief priority", f"{best.total_priority:,.2f}")
            m2.metric("Packed weight", f"{best.total_weight:,.2f} kg")
            m3.metric("Remaining capacity", f"{max(0.0, capacity-best.total_weight):,.2f} kg")
            m4.metric("Capacity used", f"{(best.total_weight/capacity*100) if capacity else 0:,.1f}%")
            st.caption(f"{title} · {best.algorithm} · {'Optimality proven' if best.optimal and best.algorithm != 'Greedy Density' else 'Feasible result; see algorithm status'}")
            selected_crates = [best_crates for i, best_crates in enumerate(run_data["crates"]) if i in set(best.selected_indices)]
            if selected_crates:
                st.markdown("**Selected relief crates**")
                st.dataframe(crates_frame(selected_crates), use_container_width=True, hide_index=True)
            else:
                st.info("No crates fit within the selected vehicle capacity. The empty packing is feasible.")
        else:
            st.warning("No algorithm completed successfully. See the comparison status below.")

        st.markdown('<div class="section-label">Algorithm comparison matrix</div>', unsafe_allow_html=True)
        st.dataframe(result_frame(results), use_container_width=True, hide_index=True)
        if results:
            chart_data = pd.DataFrame([
                {"Algorithm": r.algorithm, "Runtime (ms)": r.runtime_seconds * 1000}
                for r in results if not r.status.startswith("Not run:")
            ]).set_index("Algorithm")
            if not chart_data.empty:
                st.markdown("**Execution time comparison**")
                st.bar_chart(chart_data, use_container_width=True)
            bb = next((r for r in results if r.algorithm == "Branch and Bound"), None)
            if bb:
                n1, n2, n3 = st.columns(3)
                n1.metric("B&B nodes explored", f"{bb.nodes_explored:,}")
                n2.metric("B&B nodes pruned", f"{bb.nodes_pruned:,}")
                n3.metric("Total B&B work count", f"{bb.work_count:,}")
                st.caption("A branch is pruned when it is overweight or its fractional upper bound cannot beat the current best feasible priority.")
                if not bb.optimal:
                    st.warning("Branch and Bound reached its node safety limit. Its displayed packing is feasible, but the dashboard does not claim optimality for this run.")
            dp = next((r for r in results if r.algorithm == "Dynamic Programming"), None)
            if dp and dp.status.startswith("Not run:"):
                st.warning(dp.status)
            if len(exact_results) >= 2:
                scores = [r.total_priority for r in exact_results]
                if max(scores) - min(scores) > 1e-6:
                    st.error("The exact solvers disagree. Treat this as a correctness warning and investigate before presenting the result.")
                else:
                    st.success("All selected exact solvers agree on the optimal priority score for this run.")

        with st.expander("Explainable algorithm trace"):
            for result in results:
                st.markdown(f"**{result.algorithm}**")
                if result.trace:
                    st.code("\n".join(result.trace[-25:]), language="text")
                else:
                    st.caption(result.status)

with benchmark_tab:
    st.markdown("### Benchmark across dataset scales")
    st.write("Run the same algorithms over the repository's small, medium and large manifests. Each dataset gets a capacity calculated as the selected fraction of its total validated READY weight.")
    available_scales = [f for f in MAIN_DATASETS if (ROOT / f).exists()]
    if not available_scales:
        st.info("Add the repository's small_manifest.csv, medium_manifest.csv and large_manifest.csv files beside dashboard.py to enable scale benchmarking.")
    else:
        scales = st.multiselect("Datasets to benchmark", available_scales, default=available_scales)
        ratio = st.slider("Capacity as a percentage of total READY weight", min_value=10, max_value=90, value=40, step=5)
        bench_limit = st.select_slider("Benchmark B&B node limit", options=[100_000, 250_000, 500_000, 1_000_000], value=500_000, format_func=lambda x: f"{x:,}")
        if st.button("Run benchmark suite", type="primary", disabled=not scales):
            rows = []
            bench_status = st.status("Benchmarking selected datasets…", expanded=True)
            bench_progress = st.progress(0.0)
            bench_note = st.empty()
            total_jobs = len(scales)
            for j, filename in enumerate(scales, start=1):
                try:
                    bench_crates, _, hygiene_report = parse_manifest(load_text(filename))
                except Exception as exc:
                    rows.append({"Dataset": filename, "Algorithm": "All", "Items": 0, "Capacity (kg)": 0, "Priority": None, "Runtime (ms)": None, "Work": None, "Status": f"Load error: {exc}"})
                    continue
                bench_capacity = round(sum(c.weight_kg for c in bench_crates) * (ratio / 100.0), 1)
                bench_note.caption(f"Dataset {j}/{total_jobs}: {filename} · {len(bench_crates)} accepted crates · capacity {bench_capacity:.2f} kg")
                live = st.empty()
                pbar = st.progress(0.0)
                callback = run_progress_factory(live, pbar, bench_limit)
                bench_results = run_solver_suite(bench_crates, bench_capacity, ("Branch and Bound", "Dynamic Programming", "Greedy Density"), callback, max_nodes=bench_limit)
                for result in bench_results:
                    rows.append({
                        "Dataset": filename,
                        "Algorithm": result.algorithm,
                        "Items": len(bench_crates),
                        "Capacity (kg)": round(bench_capacity, 2),
                        "Priority": round(result.total_priority, 3) if not result.status.startswith("Not run:") else None,
                        "Runtime (ms)": round(result.runtime_seconds * 1000, 3),
                        "Work": result.work_count,
                        "Nodes explored": result.nodes_explored if result.algorithm == "Branch and Bound" else None,
                        "Nodes pruned": result.nodes_pruned if result.algorithm == "Branch and Bound" else None,
                        "Optimality": ("Proven" if result.optimal else "Not proven") if result.algorithm != "Greedy Density" else "Heuristic",
                        "Status": result.status,
                        "Accepted": hygiene_report.accepted_ready_count,
                        "Quarantined": hygiene_report.quarantined_count,
                    })
                bench_progress.progress(j / total_jobs)
            bench_status.update(label="Benchmark suite finished", state="complete", expanded=False)
            st.session_state["benchmark_rows"] = rows

        rows = st.session_state.get("benchmark_rows")
        if rows:
            df = pd.DataFrame(rows)
            st.markdown("**Benchmark performance matrix**")
            st.dataframe(df, use_container_width=True, hide_index=True)
            valid = df[df["Runtime (ms)"].notna()]
            if not valid.empty:
                st.markdown("**Runtime by dataset and algorithm**")
                pivot = valid.pivot_table(index="Dataset", columns="Algorithm", values="Runtime (ms)", aggfunc="first").fillna(0)
                st.line_chart(pivot, use_container_width=True)
                bb_rows = valid[valid["Algorithm"] == "Branch and Bound"].set_index("Dataset")
                if not bb_rows.empty:
                    st.markdown("**Branch and Bound search-tree work**")
                    st.bar_chart(bb_rows[["Nodes explored", "Nodes pruned"]].fillna(0), use_container_width=True)
            st.download_button("Download benchmark results (CSV)", df.to_csv(index=False).encode("utf-8"), file_name="relief_benchmark_results.csv", mime="text/csv")

with quality_tab:
    st.markdown("### Manifest hygiene and error handling")
    q1, q2, q3, q4 = st.columns(4)
    q1.metric("Raw records", hygiene.total_raw_records)
    q2.metric("Accepted READY", hygiene.accepted_ready_count)
    q3.metric("Quarantined", hygiene.quarantined_count)
    q4.metric("Duplicates resolved", hygiene.duplicates_resolved)
    st.markdown("**Validation breakdown**")
    validation_breakdown = pd.DataFrame([
        {"Validation outcome": "Missing values", "Records": hygiene.missing_values_count},
        {"Validation outcome": "Invalid weights", "Records": hygiene.invalid_weights_count},
        {"Validation outcome": "Invalid priorities", "Records": hygiene.invalid_priorities_count},
        {"Validation outcome": "Non-READY status", "Records": hygiene.non_ready_status_count},
        {"Validation outcome": "Schema violations", "Records": hygiene.schema_violation_count},
    ])
    st.dataframe(validation_breakdown, use_container_width=True, hide_index=True)
    st.markdown("**Quarantined / rejected records**")
    qdf = quarantine_frame(quarantine)
    if qdf.empty:
        st.success("No records were rejected or quarantined in this manifest.")
    else:
        st.dataframe(qdf, use_container_width=True, hide_index=True, height=260)
    if crates:
        clean_csv = crates_frame(crates).rename(columns={
            "Crate ID": "crate_id", "Description": "description", "Category": "category",
            "Weight (kg)": "weight_kg", "Priority": "priority_score", "Priority / kg": "density", "Status": "status"
        }).to_csv(index=False).encode("utf-8")
        st.download_button("Download accepted manifest", clean_csv, file_name="accepted_relief_manifest.csv", mime="text/csv")
    if not qdf.empty:
        st.download_button("Download quarantine report", qdf.to_csv(index=False).encode("utf-8"), file_name="relief_quarantine_report.csv", mime="text/csv")

st.markdown("---")
st.caption("Relief Packing Optimizer · DAA PBL Challenge #29 · Profiling values are measured locally for each run. DP is pseudo-polynomial; Greedy is a heuristic and is not guaranteed optimal.")
