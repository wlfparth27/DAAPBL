#ifndef RELIEF_SCHEMA_H
#define RELIEF_SCHEMA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_ID_LEN 32
#define MAX_DESC_LEN 64
#define MAX_CAT_LEN 32
#define MAX_TIMESTAMP_LEN 32
#define MAX_STATUS_LEN 24
#define MAX_LINE_LEN 512
#define MAX_REASON_LEN 128
#define MAX_CRATES 2048

/* Crate Category Enum: Medical, Water, Food, Shelter */
typedef enum {
    CAT_MEDICAL = 0,
    CAT_WATER,
    CAT_FOOD,
    CAT_SHELTER,
    CAT_UNKNOWN
} CrateCategory;

/* Crate Inspection Status: Only READY items may be loaded */
typedef enum {
    STATUS_READY = 0,
    STATUS_QUARANTINED,
    STATUS_DAMAGED,
    STATUS_DUPLICATE,
    STATUS_UNKNOWN
} CrateStatus;

/* Data Hygiene Validation Return Codes */
typedef enum {
    VALID_OK = 0,
    ERR_MISSING_FIELD,
    ERR_INVALID_WEIGHT,
    ERR_INVALID_PRIORITY,
    ERR_NON_READY_STATUS,
    ERR_DUPLICATE_ID,
    ERR_UNKNOWN_CATEGORY,
    ERR_MALFORMED_ROW
} ValidationCode;

/* Core Relief Crate Representation matching Project Synopsis Schema */
typedef struct {
    char crate_id[MAX_ID_LEN];                 /* Unique Identifier (e.g., CR-101) */
    char description[MAX_DESC_LEN];           /* Cargo Description */
    CrateCategory category;                   /* Medical / Water / Food / Shelter */
    double weight_kg;                         /* Numeric > 0 */
    double priority_score;                    /* Numeric > 0 */
    double density;                           /* Computed: priority_score / weight_kg */
    char arrival_timestamp[MAX_TIMESTAMP_LEN];/* ISO-8601 string (e.g. 2026-11-16T08:30:00Z) */
    int fragility_constraint;                 /* 1 = Fragile, 0 = Standard */
    CrateStatus status;                       /* READY, QUARANTINED, DAMAGED, DUPLICATE */
    int line_number;                          /* Source line in CSV */
} ReliefCrate;

/* Structured Audit Trail Record for Rejected / Quarantined Entries */
typedef struct {
    char crate_id[MAX_ID_LEN];
    int line_number;
    char raw_record[MAX_LINE_LEN];
    char rejection_reason[MAX_REASON_LEN];
    char action_taken[MAX_REASON_LEN];
    char detected_timestamp[MAX_TIMESTAMP_LEN];
} QuarantineRecord;

/* Manifest Sanitization & Ingestion Statistics */
typedef struct {
    int total_raw_records;
    int accepted_ready_count;
    int quarantined_count;
    int duplicates_resolved;
    int missing_values_count;
    int invalid_weights_count;
    int invalid_priorities_count;
    int non_ready_status_count;
    int schema_violation_count;
    double total_clean_weight;
    double total_clean_priority;
} ManifestStats;

/* Helper & Conversion Prototypes */
const char* category_to_string(CrateCategory cat);
CrateCategory string_to_category(const char *str);

const char* status_to_string(CrateStatus status);
CrateStatus string_to_status(const char *str);

const char* validation_code_to_string(ValidationCode code);

/* ISO-8601 Timestamp comparator: returns >0 if ts1 > ts2, <0 if ts1 < ts2, 0 if equal */
int compare_iso_timestamps(const char *ts1, const char *ts2);

/* Pretty Printing Helpers */
void print_crate(const ReliefCrate *crate);
void print_manifest_summary(const ManifestStats *stats);
void print_crate_table(const ReliefCrate *crates, int count, const char *table_title);

#endif /* RELIEF_SCHEMA_H */
