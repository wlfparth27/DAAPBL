import unittest

from dashboard_engine import (
    Crate,
    solve_branch_and_bound,
    solve_brute_force,
    solve_dynamic_programming,
    solve_greedy,
    run_solver_suite,
    parse_manifest,
)


def crate(crate_id: str, weight: float, priority: float) -> Crate:
    return Crate(
        crate_id=crate_id,
        description=crate_id,
        category="Medical",
        weight_kg=weight,
        priority_score=priority,
        arrival_timestamp="2026-11-18T00:00:00Z",
        fragility_constraint=0,
        status="READY",
        line_number=1,
    )


class ExactSolverRegressionTests(unittest.TestCase):
    def test_branch_and_bound_does_not_prune_a_real_improvement_below_old_epsilon(self):
        items = [
            crate("A", 0.5, 1.0),
            crate("B", 1.0, 1.00000000005),
        ]
        result = solve_branch_and_bound(items, 1.0)
        self.assertTrue(result.optimal, result.status)
        self.assertEqual(result.selected_indices, [1])
        self.assertAlmostEqual(result.total_priority, 1.00000000005, places=11)

    def test_all_solvers_reject_weight_that_only_fits_with_old_tolerance(self):
        items = [crate("OVER", 1.0000000000005, 10.0)]
        for solver in (solve_greedy, solve_branch_and_bound, solve_dynamic_programming, solve_brute_force):
            with self.subTest(solver=solver.__name__):
                result = solver(items, 1.0)
                self.assertEqual(result.selected_indices, [])
                if solver is solve_branch_and_bound:
                    self.assertTrue(result.optimal)

    def test_branch_and_bound_matches_exact_brute_force_on_small_instance(self):
        items = [
            crate("A", 2.25, 8.0),
            crate("B", 3.50, 11.0),
            crate("C", 1.75, 5.5),
            crate("D", 4.00, 13.0),
            crate("E", 2.00, 6.0),
        ]
        bnb = solve_branch_and_bound(items, 7.0)
        oracle = solve_brute_force(items, 7.0)
        self.assertTrue(bnb.optimal, bnb.status)
        self.assertTrue(oracle.optimal, oracle.status)
        self.assertAlmostEqual(bnb.total_priority, oracle.total_priority, places=9)
        self.assertLessEqual(bnb.total_weight, 7.0)

    def test_duplicate_timestamp_comparison_normalizes_utc_offsets(self):
        manifest = (
            "crate_id,description,category,weight_kg,priority_score,arrival_timestamp,fragility_constraint,status\n"
            "DUP,Older UTC instant,Medical,1.0,10.0,2026-11-18T08:00:00+02:00,0,READY\n"
            "DUP,Newer UTC instant,Medical,1.0,20.0,2026-11-18T06:30:00Z,0,READY\n"
        )
        accepted, _, report = parse_manifest(manifest)
        self.assertEqual(len(accepted), 1)
        self.assertEqual(accepted[0].priority_score, 20.0)
        self.assertEqual(accepted[0].line_number, 3)
        self.assertEqual(report.duplicates_resolved, 1)

    def test_dp_refuses_values_that_overflow_int64(self):
        items = [crate("A", 1.0, 5e18), crate("B", 1.0, 5e18)]
        result = solve_dynamic_programming(items, 2.0)
        self.assertFalse(result.optimal)
        self.assertIn("signed 64-bit", result.status)


    def test_dashboard_solver_suite_runs_all_selected_algorithms(self):
        items = [crate("A", 2.0, 10.0), crate("B", 3.0, 15.0), crate("C", 5.0, 25.0)]
        progress_events = []
        results = run_solver_suite(
            items, 5.0,
            ("Branch and Bound", "Dynamic Programming", "Greedy Density"),
            lambda *event: progress_events.append(event),
            max_nodes=1000,
        )
        self.assertEqual([r.algorithm for r in results], [
            "Branch and Bound", "Dynamic Programming", "Greedy Density"
        ])
        self.assertTrue(results[0].optimal, results[0].status)
        self.assertTrue(results[1].optimal, results[1].status)
        self.assertAlmostEqual(results[0].total_priority, 25.0)
        self.assertAlmostEqual(results[1].total_priority, 25.0)
        self.assertLessEqual(results[2].total_weight, 5.0)
        self.assertTrue(progress_events)


if __name__ == "__main__":
    unittest.main()
