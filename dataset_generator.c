#include "dataset_generator.h"
#include <time.h>

int generate_small_dataset(const char *filepath) {
    if (!filepath) return -1;
    FILE *fp = fopen(filepath, "w");
    if (!fp) return -1;

    /* Exact hand-verified dataset from Section 4 & Section 5 of the synopsis */
    fprintf(fp, "crate_id,description,category,weight_kg,priority_score,arrival_timestamp,fragility_constraint,status\n");
    fprintf(fp, "CR-101,\"Trauma Surgical Kit\",Medical,2.0,40.0,2026-11-16T08:00:00Z,1,READY\n");
    fprintf(fp, "CR-102,\"Water Purification Unit\",Water,5.0,30.0,2026-11-16T08:05:00Z,0,READY\n");
    fprintf(fp, "CR-103,\"Field Generator Module\",Shelter,10.0,50.0,2026-11-16T08:10:00Z,0,READY\n");
    fprintf(fp, "CR-104,\"High-Energy Ration Pallet\",Food,5.0,10.0,2026-11-16T08:15:00Z,0,READY\n");
    fprintf(fp, "CR-105,\"Pediatric Antibiotic Pack\",Medical,3.0,12.0,2026-11-16T08:20:00Z,1,READY\n");

    fclose(fp);
    return 0;
}

int generate_medium_dataset(const char *filepath, int num_crates, unsigned int seed) {
    if (!filepath || num_crates <= 0) return -1;
    FILE *fp = fopen(filepath, "w");
    if (!fp) return -1;

    srand(seed);

    const char *categories[] = {"Medical", "Water", "Food", "Shelter"};
    const char *med_names[] = {"Trauma Kit", "Antibiotic Crate", "Insulin Cooler", "Blood Plasma Box", "Surgical Field Kit"};
    const char *water_names[] = {"Purification Unit", "Chlorine Tablets Crate", "Desalination Pump", "Emergency Bladder 50L", "Filter Cartridges"};
    const char *food_names[] = {"High-Energy Biscuits", "MRE Ration Box", "Infant Nutrition Pack", "Canned Protein Pallet", "Dehydrated Meal Crate"};
    const char *shelter_names[] = {"Winterized Family Tent", "Thermal Blanket Bale", "Field Generator 2kW", "Structural Tarpaulins", "Solar Power Station"};

    fprintf(fp, "crate_id,description,category,weight_kg,priority_score,arrival_timestamp,fragility_constraint,status\n");

    for (int i = 1; i <= num_crates; i++) {
        int cat_idx = rand() % 4;
        const char *cat_str = categories[cat_idx];
        const char *desc;
        int sub_idx = rand() % 5;
        if (cat_idx == 0) desc = med_names[sub_idx];
        else if (cat_idx == 1) desc = water_names[sub_idx];
        else if (cat_idx == 2) desc = food_names[sub_idx];
        else desc = shelter_names[sub_idx];

        double weight = 5.0 + (rand() % 450) / 10.0; /* 5.0 to 50.0 kg */
        double priority = 10.0 + (rand() % 1500) / 10.0; /* 10.0 to 160.0 */
        int hour = 8 + (i / 10);
        int minute = (i * 7) % 60;
        int fragile = (cat_idx == 0 && (rand() % 2 == 0)) ? 1 : 0;

        fprintf(fp, "CR-M%03d,\"%s\",%s,%.1f,%.1f,2026-11-16T%02d:%02d:00Z,%d,READY\n",
                i, desc, cat_str, weight, priority, hour, minute, fragile);
    }

    fclose(fp);
    return 0;
}

int generate_large_correlated_dataset(const char *filepath, int num_crates, unsigned int seed) {
    if (!filepath || num_crates <= 0) return -1;
    FILE *fp = fopen(filepath, "w");
    if (!fp) return -1;

    srand(seed);
    const char *categories[] = {"Medical", "Water", "Food", "Shelter"};

    fprintf(fp, "crate_id,description,category,weight_kg,priority_score,arrival_timestamp,fragility_constraint,status\n");

    for (int i = 1; i <= num_crates; i++) {
        int cat_idx = rand() % 4;
        double weight = 10.0 + (rand() % 900) / 10.0; /* 10.0 to 100.0 kg */
        double priority;

        /* Knapsack benchmark hardness partitioning */
        int correlation_type = i % 4;
        if (correlation_type == 0) {
            /* Strongly Correlated: v = w + 20 (forces dense search tree) */
            priority = weight + 20.0;
        } else if (correlation_type == 1) {
            /* Weakly Correlated: v = w + [-10, +25] */
            double delta = (rand() % 350 - 100) / 10.0;
            priority = weight + delta;
            if (priority < 5.0) priority = 5.0;
        } else if (correlation_type == 2) {
            /* Subset Sum type: v = w */
            priority = weight;
        } else {
            /* Uncorrelated varied utility */
            priority = 20.0 + (rand() % 1800) / 10.0;
        }

        int day = 16 + (i / 200);
        int hour = (i / 25) % 24;
        int min = (i * 3) % 60;
        int sec = (i * 17) % 60;
        int fragile = (rand() % 5 == 0) ? 1 : 0;

        fprintf(fp, "CR-L%04d,\"Heavy Relief Crate #%04d\",%s,%.1f,%.1f,2026-11-%02dT%02d:%02d:%02dZ,%d,READY\n",
                i, i, categories[cat_idx], weight, priority, day, hour, min, sec, fragile);
    }

    fclose(fp);
    return 0;
}

/* Edge Case 1: Empty Manifest (n = 0) */
int generate_edge_case_1_empty(const char *filepath) {
    if (!filepath) return -1;
    FILE *fp = fopen(filepath, "w");
    if (!fp) return -1;
    /* Only header, zero data rows */
    fprintf(fp, "crate_id,description,category,weight_kg,priority_score,arrival_timestamp,fragility_constraint,status\n");
    fclose(fp);
    return 0;
}

/* Edge Case 2: Infeasible Manifest (Every item w_i > Vehicle Payload W) */
int generate_edge_case_2_infeasible(const char *filepath, double vehicle_capacity) {
    if (!filepath) return -1;
    FILE *fp = fopen(filepath, "w");
    if (!fp) return -1;

    fprintf(fp, "crate_id,description,category,weight_kg,priority_score,arrival_timestamp,fragility_constraint,status\n");
    /* If capacity W = 50 kg, all crates weigh >= 60 kg */
    double base_weight = vehicle_capacity + 15.0;
    fprintf(fp, "CR-INF-01,\"Heavy Generator 20kVA\",Shelter,%.1f,120.0,2026-11-16T08:00:00Z,0,READY\n", base_weight);
    fprintf(fp, "CR-INF-02,\"Industrial Water Cistern\",Water,%.1f,140.0,2026-11-16T08:10:00Z,0,READY\n", base_weight + 30.0);
    fprintf(fp, "CR-INF-03,\"Mobile X-Ray Station\",Medical,%.1f,200.0,2026-11-16T08:20:00Z,1,READY\n", base_weight + 50.0);
    fprintf(fp, "CR-INF-04,\"Bulky Grain Container\",Food,%.1f,90.0,2026-11-16T08:30:00Z,0,READY\n", base_weight + 20.0);

    fclose(fp);
    return 0;
}

/* Edge Case 3: Identical Density Ties (v_i / w_i = v_j / w_j) */
int generate_edge_case_3_density_ties(const char *filepath) {
    if (!filepath) return -1;
    FILE *fp = fopen(filepath, "w");
    if (!fp) return -1;

    fprintf(fp, "crate_id,description,category,weight_kg,priority_score,arrival_timestamp,fragility_constraint,status\n");
    /* All items have exact density = 5.0, testing deterministic tie-breaking */
    fprintf(fp, "CR-TIE-01,\"Trauma Kit A\",Medical,10.0,50.0,2026-11-16T08:00:00Z,1,READY\n");     /* 5.0 */
    fprintf(fp, "CR-TIE-02,\"Water Filter Pack\",Water,4.0,20.0,2026-11-16T08:05:00Z,0,READY\n");     /* 5.0 */
    fprintf(fp, "CR-TIE-03,\"Shelter Tarps\",Shelter,6.0,30.0,2026-11-16T08:10:00Z,0,READY\n");       /* 5.0 */
    fprintf(fp, "CR-TIE-04,\"High Energy Food\",Food,2.0,10.0,2026-11-16T08:15:00Z,0,READY\n");       /* 5.0 */
    fprintf(fp, "CR-TIE-05,\"Sanitation Kit\",Water,8.0,40.0,2026-11-16T08:20:00Z,0,READY\n");        /* 5.0 */

    fclose(fp);
    return 0;
}

/* Edge Case 4: Under-Capacity Trivial Fit (Sum w_i <= W) */
int generate_edge_case_4_trivial_fit(const char *filepath, double vehicle_capacity) {
    (void)vehicle_capacity;
    if (!filepath) return -1;
    FILE *fp = fopen(filepath, "w");
    if (!fp) return -1;

    /* Total weight is ~36 kg, easily fits inside e.g. W = 100 kg or 500 kg */
    fprintf(fp, "crate_id,description,category,weight_kg,priority_score,arrival_timestamp,fragility_constraint,status\n");
    fprintf(fp, "CR-FIT-01,\"First Aid Module\",Medical,5.0,35.0,2026-11-16T09:00:00Z,1,READY\n");
    fprintf(fp, "CR-FIT-02,\"Purification Tablets\",Water,4.0,28.0,2026-11-16T09:05:00Z,0,READY\n");
    fprintf(fp, "CR-FIT-03,\"Emergency Rations\",Food,8.0,40.0,2026-11-16T09:10:00Z,0,READY\n");
    fprintf(fp, "CR-FIT-04,\"Light Sleeping Bags\",Shelter,7.0,25.0,2026-11-16T09:15:00Z,0,READY\n");
    fprintf(fp, "CR-FIT-05,\"Infant Formula\",Food,6.0,30.0,2026-11-16T09:20:00Z,0,READY\n");
    fprintf(fp, "CR-FIT-06,\"Antibiotic Vials\",Medical,6.0,45.0,2026-11-16T09:25:00Z,1,READY\n");

    fclose(fp);
    return 0;
}

/* Edge Case 5: Extreme Ratio where Greedy Density Heuristic Fails */
int generate_edge_case_5_greedy_trap(const char *filepath, double vehicle_capacity) {
    if (!filepath) return -1;
    FILE *fp = fopen(filepath, "w");
    if (!fp) return -1;

    /*
     * Classical Adversarial Knapsack Case:
     * Payload W = 100 kg.
     * Crate 1: Weight = 1.0 kg, Priority = 2.0 (Density = 2.0)
     * Crate 2: Weight = 100.0 kg, Priority = 100.0 (Density = 1.0)
     * Greedy sorts by density -> selects Crate 1 (rem cap = 99 kg).
     * Crate 2 cannot fit anymore. Greedy Total = 2.0.
     * Branch & Bound explores optimal -> selects Crate 2. Optimal Total = 100.0!
     */
    fprintf(fp, "crate_id,description,category,weight_kg,priority_score,arrival_timestamp,fragility_constraint,status\n");
    fprintf(fp, "CR-TRAP-01,\"Lightweight Radio (Greedy Bait)\",Shelter,1.0,2.0,2026-11-16T10:00:00Z,0,READY\n");
    fprintf(fp, "CR-TRAP-02,\"Full Vehicle Medical ICU Container\",Medical,%.1f,%.1f,2026-11-16T10:05:00Z,1,READY\n",
            vehicle_capacity, vehicle_capacity);

    fclose(fp);
    return 0;
}

/* Dirty Data Test Manifest for Ingestion Hygiene Verification */
int generate_dirty_test_manifest(const char *filepath) {
    if (!filepath) return -1;
    FILE *fp = fopen(filepath, "w");
    if (!fp) return -1;

    fprintf(fp, "crate_id,description,category,weight_kg,priority_score,arrival_timestamp,fragility_constraint,status\n");
    /* 1. Normal valid record */
    fprintf(fp, "CR-D01,\"Valid Surgical Kit\",Medical,10.0,50.0,2026-11-16T08:00:00Z,1,READY\n");

    /* 2. Missing numeric weight */
    fprintf(fp, "CR-D02,\"Missing Weight Crate\",Water,,45.0,2026-11-16T08:05:00Z,0,READY\n");

    /* 3. Missing numeric priority */
    fprintf(fp, "CR-D03,\"Missing Priority Crate\",Food,12.0,,2026-11-16T08:10:00Z,0,READY\n");

    /* 4. Invalid negative weight */
    fprintf(fp, "CR-D04,\"Negative Weight Box\",Medical,-5.0,30.0,2026-11-16T08:15:00Z,1,READY\n");

    /* 5. Invalid zero weight */
    fprintf(fp, "CR-D05,\"Zero Weight Cargo\",Shelter,0.0,25.0,2026-11-16T08:20:00Z,0,READY\n");

    /* 6. Invalid zero priority */
    fprintf(fp, "CR-D06,\"Zero Utility Waste\",Food,15.0,0.0,2026-11-16T08:25:00Z,0,READY\n");

    /* 7. Non-READY status: DAMAGED */
    fprintf(fp, "CR-D07,\"Crushed Water Bottles\",Water,20.0,40.0,2026-11-16T08:30:00Z,0,DAMAGED\n");

    /* 8. Non-READY status: QUARANTINED */
    fprintf(fp, "CR-D08,\"Contaminated Antibiotics\",Medical,8.0,60.0,2026-11-16T08:35:00Z,1,QUARANTINED\n");

    /* 9. Unknown Category */
    fprintf(fp, "CR-D09,\"Hazardous Flammable Fuel\",Petroleum,30.0,70.0,2026-11-16T08:40:00Z,0,READY\n");

    /* 10. Duplicate Pair: CR-D10 with older timestamp */
    fprintf(fp, "CR-D10,\"Field Tent (Old Initial Draft)\",Shelter,18.0,40.0,2026-11-16T07:30:00Z,0,READY\n");

    /* 11. Duplicate Pair: CR-D10 with NEWER timestamp (should supersede and keep this one) */
    fprintf(fp, "CR-D10,\"Field Tent (Updated Verified Weight)\",Shelter,16.0,48.0,2026-11-16T09:45:00Z,0,READY\n");

    /* 12. Duplicate with older timestamp arriving later */
    fprintf(fp, "CR-D01,\"Valid Surgical Kit Duplicate\",Medical,10.0,50.0,2026-11-16T07:15:00Z,1,READY\n");

    /* 13. Another valid crate */
    fprintf(fp, "CR-D11,\"Emergency Rations Pack\",Food,14.0,55.0,2026-11-16T08:50:00Z,0,READY\n");

    fclose(fp);
    return 0;
}

int generate_all_datasets(const char *output_dir) {
    char path[512];

    snprintf(path, sizeof(path), "%s/small_manifest.csv", output_dir);
    generate_small_dataset(path);

    snprintf(path, sizeof(path), "%s/medium_manifest.csv", output_dir);
    generate_medium_dataset(path, 40, 101);

    snprintf(path, sizeof(path), "%s/large_manifest.csv", output_dir);
    generate_large_correlated_dataset(path, 500, 202);

    snprintf(path, sizeof(path), "%s/dirty_test_manifest.csv", output_dir);
    generate_dirty_test_manifest(path);

    snprintf(path, sizeof(path), "%s/edge_case_1_empty.csv", output_dir);
    generate_edge_case_1_empty(path);

    snprintf(path, sizeof(path), "%s/edge_case_2_infeasible.csv", output_dir);
    generate_edge_case_2_infeasible(path, 50.0);

    snprintf(path, sizeof(path), "%s/edge_case_3_density_ties.csv", output_dir);
    generate_edge_case_3_density_ties(path);

    snprintf(path, sizeof(path), "%s/edge_case_4_trivial_fit.csv", output_dir);
    generate_edge_case_4_trivial_fit(path, 100.0);

    snprintf(path, sizeof(path), "%s/edge_case_5_greedy_trap.csv", output_dir);
    generate_edge_case_5_greedy_trap(path, 100.0);

    return 0;
}
