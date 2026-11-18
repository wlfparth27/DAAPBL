#include "relief_schema.h"
#include <ctype.h>

/* Case-insensitive string comparison helper */
static int strcasecmp_custom(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int diff = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
        if (diff != 0) return diff;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

const char* category_to_string(CrateCategory cat) {
    switch (cat) {
        case CAT_MEDICAL: return "Medical";
        case CAT_WATER:   return "Water";
        case CAT_FOOD:    return "Food";
        case CAT_SHELTER: return "Shelter";
        default:          return "Unknown";
    }
}

CrateCategory string_to_category(const char *str) {
    if (!str) return CAT_UNKNOWN;
    if (strcasecmp_custom(str, "Medical") == 0) return CAT_MEDICAL;
    if (strcasecmp_custom(str, "Water") == 0)   return CAT_WATER;
    if (strcasecmp_custom(str, "Food") == 0)    return CAT_FOOD;
    if (strcasecmp_custom(str, "Shelter") == 0) return CAT_SHELTER;
    return CAT_UNKNOWN;
}

const char* status_to_string(CrateStatus status) {
    switch (status) {
        case STATUS_READY:       return "READY";
        case STATUS_QUARANTINED: return "QUARANTINED";
        case STATUS_DAMAGED:     return "DAMAGED";
        case STATUS_DUPLICATE:   return "DUPLICATE";
        default:                 return "UNKNOWN";
    }
}

CrateStatus string_to_status(const char *str) {
    if (!str) return STATUS_UNKNOWN;
    if (strcasecmp_custom(str, "READY") == 0)       return STATUS_READY;
    if (strcasecmp_custom(str, "QUARANTINED") == 0) return STATUS_QUARANTINED;
    if (strcasecmp_custom(str, "DAMAGED") == 0)     return STATUS_DAMAGED;
    if (strcasecmp_custom(str, "DUPLICATE") == 0)   return STATUS_DUPLICATE;
    return STATUS_UNKNOWN;
}

const char* validation_code_to_string(ValidationCode code) {
    switch (code) {
        case VALID_OK:             return "VALID_OK";
        case ERR_MISSING_FIELD:    return "ERR_MISSING_FIELD";
        case ERR_INVALID_WEIGHT:   return "ERR_INVALID_WEIGHT (w <= 0)";
        case ERR_INVALID_PRIORITY: return "ERR_INVALID_PRIORITY (v <= 0)";
        case ERR_NON_READY_STATUS: return "ERR_NON_READY_STATUS";
        case ERR_DUPLICATE_ID:     return "ERR_DUPLICATE_ID";
        case ERR_UNKNOWN_CATEGORY: return "ERR_UNKNOWN_CATEGORY";
        case ERR_MALFORMED_ROW:    return "ERR_MALFORMED_ROW";
        default:                   return "ERR_UNKNOWN";
    }
}

/* Compare two ISO-8601 timestamps (YYYY-MM-DDTHH:MM:SS) */
static long long civil_days(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    int era = (year >= 0 ? year : year - 399) / 400;
    unsigned year_of_era = (unsigned)(year - era * 400);
    unsigned adjusted_month = month > 2 ? month - 3 : month + 9;
    unsigned day_of_year = (153 * adjusted_month + 2) / 5 + day - 1;
    unsigned day_of_era = year_of_era * 365 + year_of_era / 4 -
                          year_of_era / 100 + day_of_year;
    return (long long)era * 146097 + (long long)day_of_era;
}

int compare_iso_timestamps(const char *ts1, const char *ts2) {
    if (!ts1 && !ts2) return 0;
    if (!ts1) return -1;
    if (!ts2) return 1;

    int y1, m1, d1, h1, min1, y2, m2, d2, h2, min2;
    double sec1, sec2;
    char zone1[16] = {0}, zone2[16] = {0};
    int n1 = sscanf(ts1, "%d-%d-%dT%d:%d:%lf%15s",
                    &y1, &m1, &d1, &h1, &min1, &sec1, zone1);
    int n2 = sscanf(ts2, "%d-%d-%dT%d:%d:%lf%15s",
                    &y2, &m2, &d2, &h2, &min2, &sec2, zone2);
    if (n1 >= 6 && n2 >= 6) {
        int offset1 = 0, offset2 = 0;
        int oh = 0, om = 0;
        if (zone1[0] == '+' || zone1[0] == '-') {
            if (sscanf(zone1 + 1, "%d:%d", &oh, &om) != 2) return strcmp(ts1, ts2);
            offset1 = (oh * 60 + om) * 60 * (zone1[0] == '+' ? 1 : -1);
        }
        oh = om = 0;
        if (zone2[0] == '+' || zone2[0] == '-') {
            if (sscanf(zone2 + 1, "%d:%d", &oh, &om) != 2) return strcmp(ts1, ts2);
            offset2 = (oh * 60 + om) * 60 * (zone2[0] == '+' ? 1 : -1);
        }
        double utc1 = (double)civil_days(y1, (unsigned)m1, (unsigned)d1) * 86400.0 +
                      h1 * 3600.0 + min1 * 60.0 + sec1 - offset1;
        double utc2 = (double)civil_days(y2, (unsigned)m2, (unsigned)d2) * 86400.0 +
                      h2 * 3600.0 + min2 * 60.0 + sec2 - offset2;
        if (utc1 < utc2) return -1;
        if (utc1 > utc2) return 1;
        return 0;
    }
    return strcmp(ts1, ts2);
}

void print_crate(const ReliefCrate *crate) {
    if (!crate) return;
    printf("[%s] %-24s | %-7s | %6.1f kg | Val: %5.1f | Dens: %5.2f | Fragile: %d | Status: %s\n",
           crate->crate_id,
           crate->description,
           category_to_string(crate->category),
           crate->weight_kg,
           crate->priority_score,
           crate->density,
           crate->fragility_constraint,
           status_to_string(crate->status));
}

void print_crate_table(const ReliefCrate *crates, int count, const char *table_title) {
    printf("\n========================================================================================================\n");
    printf("  %s (Total Crates: %d)\n", table_title ? table_title : "Relief Manifest", count);
    printf("========================================================================================================\n");
    printf("%-8s | %-26s | %-8s | %-9s | %-9s | %-8s | %-7s | %-10s\n",
           "ID", "Description", "Category", "Weight(kg)", "Priority", "Density", "Fragile", "Status");
    printf("--------------------------------------------------------------------------------------------------------\n");

    int display_count = count > 25 ? 25 : count;
    for (int i = 0; i < display_count; i++) {
        printf("%-8s | %-26.26s | %-8s | %9.2f | %9.2f | %8.2f | %-7s | %-10s\n",
               crates[i].crate_id,
               crates[i].description,
               category_to_string(crates[i].category),
               crates[i].weight_kg,
               crates[i].priority_score,
               crates[i].density,
               crates[i].fragility_constraint ? "YES" : "NO",
               status_to_string(crates[i].status));
    }
    if (count > display_count) {
        printf("... and %d more crates not displayed in preview table ...\n", count - display_count);
    }
    printf("========================================================================================================\n\n");
}

void print_manifest_summary(const ManifestStats *stats) {
    if (!stats) return;
    printf("\n+----------------------------------------------------------------+\n");
    printf("|                MANIFEST DATA HYGIENE AUDIT REPORT              |\n");
    printf("+----------------------------------------------------------------+\n");
    printf("| Total Ingested Raw Records:          %-25d |\n", stats->total_raw_records);
    printf("| Accepted READY Crates (Clean):       %-25d |\n", stats->accepted_ready_count);
    printf("| Quarantined / Filtered Records:      %-25d |\n", stats->quarantined_count);
    printf("|   - Duplicate IDs Resolved:          %-25d |\n", stats->duplicates_resolved);
    printf("|   - Missing Numeric Values:          %-25d |\n", stats->missing_values_count);
    printf("|   - Invalid Weights (w <= 0):        %-25d |\n", stats->invalid_weights_count);
    printf("|   - Invalid Priorities (v <= 0):     %-25d |\n", stats->invalid_priorities_count);
    printf("|   - Non-READY Status (Damaged/Quar): %-25d |\n", stats->non_ready_status_count);
    printf("|   - Schema / Formatting Violations:  %-25d |\n", stats->schema_violation_count);
    printf("+----------------------------------------------------------------+\n");
    printf("| Clean Manifest Total Weight:         %-22.2f kg |\n", stats->total_clean_weight);
    printf("| Clean Manifest Total Priority:       %-25.2f |\n", stats->total_clean_priority);
    printf("+----------------------------------------------------------------+\n\n");
}
