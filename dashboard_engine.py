"""CSV ingestion, data hygiene, and optimization engines for the relief dashboard.

The dashboard-facing Branch & Bound routine follows the repository's best-first
LC Branch & Bound approach with a fractional-knapsack upper bound. The existing
C DP and Greedy modules remain in the repository; these Python adapters implement
the same contracts so the dashboard can run on Windows without a C shared-library
build step. See RUN_DASHBOARD.md for the distinction and limitations.
"""
from __future__ import annotations

import csv
import heapq
from fractions import Fraction
import io
import math
import time
import tracemalloc
from dataclasses import dataclass, field
from decimal import Decimal, InvalidOperation
from typing import Callable, Iterable, Optional

import numpy as np


class ManifestError(ValueError):
    """Raised when a CSV cannot be interpreted as a relief manifest."""


@dataclass(frozen=True)
class Crate:
    crate_id: str
    description: str
    category: str
    weight_kg: float
    priority_score: float
    arrival_timestamp: str
    fragility_constraint: int
    status: str
    line_number: int


@dataclass
class HygieneReport:
    total_raw_records: int = 0
    accepted_ready_count: int = 0
    quarantined_count: int = 0
    duplicates_resolved: int = 0
    missing_values_count: int = 0
    invalid_weights_count: int = 0
    invalid_priorities_count: int = 0
    non_ready_status_count: int = 0
    schema_violation_count: int = 0


@dataclass
class SolverResult:
    algorithm: str
    selected_indices: list[int]
    total_weight: float
    total_priority: float
    runtime_seconds: float
    peak_traced_mb: float
    work_count: int
    nodes_explored: int = 0
    nodes_pruned: int = 0
    optimal: bool = True
    status: str = "Completed"
    trace: list[str] = field(default_factory=list)


def _normalise_header(value: str) -> str:
    return (value or "").strip().lower().replace(" ", "_").replace("-", "_")


def _first_field(row: dict, aliases: Iterable[str], default: str = "") -> str:
    for key in aliases:
        if key in row and row[key] is not None:
            return str(row[key]).strip()
    return default


def _safe_float(value: str) -> Optional[float]:
    try:
        number = float(str(value).strip())
    except (TypeError, ValueError):
        return None
    return number if math.isfinite(number) else None


def parse_manifest(csv_text: str) -> tuple[list[Crate], list[dict], HygieneReport]:
    """Parse, validate and deduplicate a repository-style CSV manifest.

    READY records with valid IDs, known categories (when supplied), positive
    finite weights/priorities, and valid statuses are accepted. Non-READY and
    invalid records are retained in the quarantine report. Duplicate READY IDs
    retain the record with the latest ISO timestamp, matching sanitizer.c.
    """
    if not csv_text or not csv_text.strip():
        raise ManifestError("The CSV file is empty and has no header row.")
    try:
        reader = csv.DictReader(io.StringIO(csv_text.lstrip("\ufeff")))
        if not reader.fieldnames:
            raise ManifestError("The CSV file is missing its header row.")
        headers = {_normalise_header(h): h for h in reader.fieldnames if h}
    except csv.Error as exc:
        raise ManifestError(f"Could not read the CSV header: {exc}") from exc

    id_col = next((headers[x] for x in ("crate_id", "item_id", "package_id", "id") if x in headers), None)
    weight_col = next((headers[x] for x in ("weight_kg", "weight", "crate_weight") if x in headers), None)
    priority_col = next((headers[x] for x in ("priority_score", "priority", "value", "score") if x in headers), None)
    if not id_col or not weight_col or not priority_col:
        raise ManifestError(
            "Required columns not found. Include crate_id (or item_id/id), "
            "weight_kg (or weight), and priority_score (or priority)."
        )

    desc_col = next((headers[x] for x in ("description", "item_name", "name") if x in headers), None)
    category_col = next((headers[x] for x in ("category", "item_category") if x in headers), None)
    status_col = next((headers[x] for x in ("status", "inspection_status") if x in headers), None)
    timestamp_col = next((headers[x] for x in ("arrival_timestamp", "timestamp", "arrival_time") if x in headers), None)
    fragility_col = next((headers[x] for x in ("fragility_constraint", "fragile") if x in headers), None)

    report = HygieneReport()
    quarantine: list[dict] = []
    accepted_by_id: dict[str, Crate] = {}
    categories = {"medical", "water", "food", "shelter"}
    valid_statuses = {"ready", "quarantined", "damaged", "duplicate"}

    def reject(line_no: int, crate_id: str, reason: str, action: str = "Excluded from optimization") -> None:
        quarantine.append({
            "line_number": line_no,
            "crate_id": crate_id or "UNKNOWN",
            "rejection_reason": reason,
            "action_taken": action,
        })
        report.quarantined_count += 1

    try:
        for line_no, row_raw in enumerate(reader, start=2):
            if row_raw is None or not any(str(v or "").strip() for v in row_raw.values()):
                continue
            report.total_raw_records += 1
            row = {_normalise_header(k): (v.strip() if isinstance(v, str) else v) for k, v in row_raw.items() if k}
            crate_id = _first_field(row, ("crate_id", "item_id", "package_id", "id"))
            if not crate_id:
                report.missing_values_count += 1
                reject(line_no, crate_id, "Missing crate ID")
                continue

            category = _first_field(row, ("category", "item_category"), "Other")
            if category_col and category.lower() not in categories:
                report.schema_violation_count += 1
                reject(line_no, crate_id, f"Unrecognized category '{category}'")
                continue

            weight_raw = _first_field(row, ("weight_kg", "weight", "crate_weight"))
            weight = _safe_float(weight_raw)
            if not weight_raw:
                report.missing_values_count += 1
                reject(line_no, crate_id, "Missing weight_kg")
                continue
            if weight is None or weight <= 0:
                report.invalid_weights_count += 1
                reject(line_no, crate_id, f"Invalid weight '{weight_raw}' (must be finite and > 0)")
                continue

            priority_raw = _first_field(row, ("priority_score", "priority", "value", "score"))
            priority = _safe_float(priority_raw)
            if not priority_raw:
                report.missing_values_count += 1
                reject(line_no, crate_id, "Missing priority_score")
                continue
            if priority is None or priority <= 0:
                report.invalid_priorities_count += 1
                reject(line_no, crate_id, f"Invalid priority '{priority_raw}' (must be finite and > 0)")
                continue

            status = _first_field(row, ("status", "inspection_status"), "READY").upper()
            if status.lower() not in valid_statuses:
                report.schema_violation_count += 1
                reject(line_no, crate_id, f"Unknown inspection status '{status}'")
                continue
            if status != "READY":
                report.non_ready_status_count += 1
                reject(line_no, crate_id, f"Inspection status is {status}; only READY crates can be dispatched")
                continue

            timestamp = _first_field(row, ("arrival_timestamp", "timestamp", "arrival_time"), "1970-01-01T00:00:00Z")
            if not timestamp:
                timestamp = "1970-01-01T00:00:00Z"
            fragility_raw = _first_field(row, ("fragility_constraint", "fragile"), "0")
            fragility = 1 if fragility_raw.strip().lower() in {"1", "true", "yes", "y"} else 0
            crate = Crate(
                crate_id=crate_id,
                description=_first_field(row, ("description", "item_name", "name"), f"Relief Crate {crate_id}"),
                category=category.title(),
                weight_kg=weight,
                priority_score=priority,
                arrival_timestamp=timestamp,
                fragility_constraint=fragility,
                status="READY",
                line_number=line_no,
            )

            previous = accepted_by_id.get(crate_id)
            if previous is None:
                accepted_by_id[crate_id] = crate
            else:
                report.duplicates_resolved += 1
                # ISO-8601 sortable timestamps; on a tie retain the earlier row,
                # as the repository sanitizer does (replace only if newer).
                if crate.arrival_timestamp > previous.arrival_timestamp:
                    reject(
                        previous.line_number, previous.crate_id,
                        f"Duplicate crate_id superseded by newer record ({crate.arrival_timestamp} > {previous.arrival_timestamp})",
                        "Replaced by newer manifest record",
                    )
                    accepted_by_id[crate_id] = crate
                else:
                    reject(
                        crate.line_number, crate.crate_id,
                        f"Duplicate crate_id; older/equal record superseded by {previous.arrival_timestamp}",
                        "Ignored older duplicate record",
                    )
    except csv.Error as exc:
        raise ManifestError(f"CSV parsing failed: {exc}") from exc

    accepted = sorted(accepted_by_id.values(), key=lambda c: c.line_number)
    report.accepted_ready_count = len(accepted)
    return accepted, quarantine, report


def _decimal_places(value: float) -> int:
    try:
        d = Decimal(str(value)).normalize()
    except InvalidOperation as exc:
        raise ValueError("Value cannot be represented as a decimal") from exc
    return max(0, -d.as_tuple().exponent)


def _scaled_ints(values: list[float], label: str, max_places: int = 6) -> tuple[list[int], int]:
    places = max((_decimal_places(v) for v in values), default=0)
    if places > max_places:
        raise ValueError(f"{label} has more than {max_places} decimal places; exact integer scaling is unsupported.")
    scale = 10 ** places
    result = []
    for value in values:
        scaled = Decimal(str(value)) * scale
        if scaled != scaled.to_integral_value():
            raise ValueError(f"{label} cannot be represented exactly at six decimal places.")
        result.append(int(scaled))
    return result, scale


def _profile_call(algorithm: str, function: Callable[[], dict]) -> SolverResult:
    tracemalloc.start()
    start = time.perf_counter()
    try:
        output = function()
        elapsed = time.perf_counter() - start
        _, peak = tracemalloc.get_traced_memory()
    finally:
        tracemalloc.stop()
    return SolverResult(
        algorithm=algorithm,
        selected_indices=output["selected_indices"],
        total_weight=output["total_weight"],
        total_priority=output["total_priority"],
        runtime_seconds=elapsed,
        peak_traced_mb=peak / (1024 * 1024),
        work_count=output.get("work_count", 0),
        nodes_explored=output.get("nodes_explored", 0),
        nodes_pruned=output.get("nodes_pruned", 0),
        optimal=output.get("optimal", True),
        status=output.get("status", "Completed"),
        trace=output.get("trace", []),
    )


def solve_greedy(crates: list[Crate], capacity: float) -> SolverResult:
    def run() -> dict:
        if not math.isfinite(capacity) or capacity < 0:
            raise ValueError("Capacity must be finite and non-negative.")
        weights, _ = _scaled_ints([c.weight_kg for c in crates] + [capacity], "Weights/capacity", max_places=12)
        capacity_units = weights.pop()
        ordered = sorted(enumerate(crates), key=lambda pair: (
            -(pair[1].priority_score / pair[1].weight_kg),
            pair[1].weight_kg, pair[1].crate_id, pair[0],
        ))
        chosen: list[int] = []
        used_units = 0
        for original_index, crate in ordered:
            if used_units + weights[original_index] <= capacity_units:
                chosen.append(original_index)
                used_units += weights[original_index]
        chosen.sort()
        return {
            "selected_indices": chosen,
            "total_weight": sum(crates[i].weight_kg for i in chosen),
            "total_priority": sum(crates[i].priority_score for i in chosen),
            "work_count": len(crates),
            "trace": [f"Sorted {len(crates)} READY crates by priority/weight density.",
                      f"Selected {len(chosen)} complete crates using exact scaled-weight feasibility."],
        }
    try:
        return _profile_call("Greedy Density", run)
    except ValueError as exc:
        return SolverResult(algorithm="Greedy Density", selected_indices=[], total_weight=0.0,
            total_priority=0.0, runtime_seconds=0.0, peak_traced_mb=0.0, work_count=0,
            optimal=False, status=f"Not run: {exc}")


def solve_dynamic_programming(
    crates: list[Crate], capacity: float,
    progress: Optional[Callable[[int, int, int], None]] = None,
    memory_limit_mb: int = 256,
) -> SolverResult:
    def run() -> dict:
        if not math.isfinite(capacity) or capacity < 0:
            raise ValueError("Capacity must be finite and non-negative.")
        if not crates or capacity == 0:
            return {"selected_indices": [], "total_weight": 0.0, "total_priority": 0.0, "work_count": 0}
        all_weights, weight_scale = _scaled_ints([c.weight_kg for c in crates] + [capacity], "Weights/capacity")
        capacity_units = all_weights.pop()
        eligible_positions = [i for i, w in enumerate(all_weights) if w <= capacity_units]
        if not eligible_positions:
            return {"selected_indices": [], "total_weight": 0.0, "total_priority": 0.0, "work_count": 0}
        eligible_crates = [crates[i] for i in eligible_positions]
        weights = [all_weights[i] for i in eligible_positions]
        priorities, priority_scale = _scaled_ints([c.priority_score for c in eligible_crates], "Priority scores")
        if sum(priorities) > np.iinfo(np.int64).max:
            raise OverflowError("Scaled total priority exceeds the signed 64-bit DP limit.")
        effective_capacity = min(capacity_units, sum(weights))
        width = effective_capacity + 1
        choice_row_bytes = (width + 7) // 8
        required_bytes = len(eligible_crates) * choice_row_bytes + width * 16
        if required_bytes > memory_limit_mb * 1024 * 1024:
            raise MemoryError(f"DP decision table needs approximately {required_bytes / (1024*1024):.1f} MB; "
                              f"the dashboard safety limit is {memory_limit_mb} MB.")
        dp_value = np.zeros(width, dtype=np.int64)
        dp_weight = np.zeros(width, dtype=np.int64)
        choices = np.zeros((len(eligible_crates), choice_row_bytes), dtype=np.uint8)
        operations = 0
        for i, (w, p) in enumerate(zip(weights, priorities)):
            candidate_value = dp_value[:-w] + p
            candidate_weight = dp_weight[:-w] + w
            target_value, target_weight = dp_value[w:], dp_weight[w:]
            improve = (candidate_value > target_value) | (
                (candidate_value == target_value) & (candidate_weight < target_weight))
            target_value[improve] = candidate_value[improve]
            target_weight[improve] = candidate_weight[improve]
            choice_row = np.zeros(width, dtype=np.uint8)
            choice_row[w:] = improve.astype(np.uint8, copy=False)
            choices[i, :] = np.packbits(choice_row, bitorder="little")
            operations += width
            if progress and (i == len(eligible_crates) - 1 or (i + 1) % max(1, len(eligible_crates) // 20) == 0):
                progress(i + 1, len(eligible_crates), operations)
        best_capacity = int(np.argmax(dp_value))
        selected: list[int] = []
        remaining = best_capacity
        for i in range(len(eligible_crates) - 1, -1, -1):
            if choices[i, remaining >> 3] & (1 << (remaining & 7)):
                selected.append(eligible_positions[i])
                remaining -= weights[i]
        selected.sort()
        return {"selected_indices": selected,
            "total_weight": sum(crates[i].weight_kg for i in selected),
            "total_priority": sum(crates[i].priority_score for i in selected),
            "work_count": operations,
            "trace": [f"Exact integer scaling: weight scale {weight_scale}, priority scale {priority_scale}.",
                      f"Evaluated {operations:,} DP transitions across {len(eligible_crates)} eligible crates."]}
    try:
        return _profile_call("Dynamic Programming", run)
    except (ValueError, MemoryError, OverflowError) as exc:
        return SolverResult(algorithm="Dynamic Programming", selected_indices=[], total_weight=0.0,
            total_priority=0.0, runtime_seconds=0.0, peak_traced_mb=0.0, work_count=0,
            optimal=False, status=f"Not run: {exc}")


def solve_branch_and_bound(
    crates: list[Crate], capacity: float,
    progress: Optional[Callable[[int, int, float, int], None]] = None,
    max_nodes: int = 1_000_000,
    progress_interval: int = 500,
) -> SolverResult:
    def run() -> dict:
        if not math.isfinite(capacity) or capacity < 0:
            raise ValueError("Capacity must be finite and non-negative.")
        if not crates or capacity == 0:
            return {"selected_indices": [], "total_weight": 0.0, "total_priority": 0.0,
                    "work_count": 0, "nodes_explored": 0, "nodes_pruned": 0}
        # Exact integer units make feasibility decisions exact. Fraction preserves
        # the fractional-knapsack relaxation exactly for mathematically safe pruning.
        weight_units, weight_scale = _scaled_ints([c.weight_kg for c in crates] + [capacity], "Weights/capacity", max_places=12)
        capacity_units = weight_units.pop()
        value_units, value_scale = _scaled_ints([c.priority_score for c in crates], "Priority scores", max_places=12)
        ordered_indices = sorted(range(len(crates)),
            key=lambda i: (-Fraction(value_units[i], weight_units[i]), i))
        weights = [weight_units[i] for i in ordered_indices]
        values = [value_units[i] for i in ordered_indices]
        n = len(crates)
        greedy_mask = greedy_w = greedy_v = 0
        for i, (w, v) in enumerate(zip(weights, values)):
            if greedy_w + w <= capacity_units:
                greedy_mask |= 1 << i
                greedy_w += w
                greedy_v += v
        best_value, best_weight, best_mask = greedy_v, greedy_w, greedy_mask

        def bound(level: int, weight: int, value: int) -> Fraction:
            if weight > capacity_units:
                return Fraction(-1, 1)
            upper, total_w, j = Fraction(value, 1), weight, level
            while j < n and total_w + weights[j] <= capacity_units:
                total_w += weights[j]
                upper += values[j]
                j += 1
            if j < n and total_w < capacity_units:
                upper += Fraction((capacity_units - total_w) * values[j], weights[j])
            return upper

        serial = 0
        root_bound = bound(0, 0, 0)
        queue: list[tuple[Fraction, int, int, int, int, int]] = [(-root_bound, serial, 0, 0, 0, 0)]
        explored = pruned = 0
        trace = [f"Root created with exact fractional upper bound {float(root_bound) / value_scale:.3f}.",
                 "Items ordered by exact priority/weight density."]
        complete = True
        while queue:
            if explored >= max_nodes:
                complete = False
                break
            neg_bound, _, level, current_w, current_v, mask = heapq.heappop(queue)
            if -neg_bound <= best_value:
                pruned += 1
                explored += 1
                continue
            explored += 1
            if level >= n:
                continue
            item_w, item_v = weights[level], values[level]
            include_w = current_w + item_w
            if include_w <= capacity_units:
                include_v = current_v + item_v
                include_mask = mask | (1 << level)
                if include_v > best_value:
                    best_value, best_weight, best_mask = include_v, include_w, include_mask
                    if len(trace) < 300:
                        trace.append(f"New incumbent: priority {best_value / value_scale:.2f}, "
                                     f"weight {best_weight / weight_scale:.2f} kg.")
                include_bound = bound(level + 1, include_w, include_v)
                if include_bound > best_value:
                    serial += 1
                    heapq.heappush(queue, (-include_bound, serial, level + 1, include_w, include_v, include_mask))
                else:
                    pruned += 1
            else:
                pruned += 1
            exclude_bound = bound(level + 1, current_w, current_v)
            if exclude_bound > best_value:
                serial += 1
                heapq.heappush(queue, (-exclude_bound, serial, level + 1, current_w, current_v, mask))
            else:
                pruned += 1
            if progress and progress_interval > 0 and explored % progress_interval == 0:
                progress(explored, max_nodes, best_value / value_scale, pruned)
        selected = sorted(ordered_indices[i] for i in range(n) if (best_mask >> i) & 1)
        if not queue:
            complete = True
        if progress:
            progress(explored, max_nodes, best_value / value_scale, pruned)
        trace.append(f"Search ended after {explored:,} explored nodes and {pruned:,} pruned nodes.")
        trace.append("Priority queue exhausted: optimality proven for this model." if complete else
            f"Node safety limit ({max_nodes:,}) reached; incumbent is feasible but optimality is not proven.")
        return {"selected_indices": selected,
            "total_weight": sum(crates[i].weight_kg for i in selected),
            "total_priority": sum(crates[i].priority_score for i in selected),
            "work_count": explored, "nodes_explored": explored, "nodes_pruned": pruned,
            "optimal": complete, "status": "Optimality proven" if complete else "Stopped at node safety limit",
            "trace": trace}
    try:
        return _profile_call("Branch and Bound", run)
    except ValueError as exc:
        return SolverResult(algorithm="Branch and Bound", selected_indices=[], total_weight=0.0,
            total_priority=0.0, runtime_seconds=0.0, peak_traced_mb=0.0, work_count=0,
            optimal=False, status=f"Not run: {exc}")

def solve_brute_force(crates: list[Crate], capacity: float, max_items: int = 20) -> SolverResult:
    def run() -> dict:
        n = len(crates)
        if n > max_items:
            raise ValueError(f"Brute force is limited to {max_items} crates; this dataset has {n}.")
        if not math.isfinite(capacity) or capacity < 0:
            raise ValueError("Capacity must be finite and non-negative.")
        weights, _ = _scaled_ints([c.weight_kg for c in crates] + [capacity], "Weights/capacity", max_places=12)
        capacity_units = weights.pop()
        values, _ = _scaled_ints([c.priority_score for c in crates], "Priority scores", max_places=12)
        best_value = best_weight = best_mask = 0
        operations = 0
        for mask in range(1 << n):
            operations += 1
            weight = value = 0
            for i in range(n):
                if (mask >> i) & 1:
                    weight += weights[i]
                    value += values[i]
            if weight <= capacity_units and value > best_value:
                best_value, best_weight, best_mask = value, weight, mask
        selected = [i for i in range(n) if (best_mask >> i) & 1]
        return {
            "selected_indices": selected,
            "total_weight": sum(crates[i].weight_kg for i in selected),
            "total_priority": sum(crates[i].priority_score for i in selected),
            "work_count": operations,
        }
    try:
        return _profile_call("Brute Force Oracle", run)
    except ValueError as exc:
        return SolverResult(algorithm="Brute Force Oracle", selected_indices=[], total_weight=0.0,
            total_priority=0.0, runtime_seconds=0.0, peak_traced_mb=0.0, work_count=0,
            optimal=False, status=f"Not run: {exc}")
