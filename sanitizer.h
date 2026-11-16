#ifndef SANITIZER_H
#define SANITIZER_H

#include "relief_schema.h"

/* Max quarantine records tracked per batch */
#define MAX_QUARANTINE_RECORDS 2048

/* Main Entrypoint: Process raw CSV manifest into sanitized and quarantined manifests */
int sanitize_manifest(
    const char *input_csv_path,
    const char *clean_output_path,
    const char *quarantine_output_path,
    ReliefCrate *clean_crates,
    int max_crates,
    int *clean_count,
    ManifestStats *stats
);

/* Parse a single CSV row into a ReliefCrate structure with schema checks */
int parse_csv_line(
    const char *line,
    int line_num,
    ReliefCrate *crate,
    char *err_msg,
    size_t err_buf_len
);

/* Validate individual crate attributes against hard constraints and domain rules */
ValidationCode validate_crate_fields(
    const ReliefCrate *crate,
    char *reason_buf,
    size_t buf_len
);

/* Deduplicate records by crate_id: Keep the one with the latest ISO-8601 timestamp */
int deduplicate_manifest(
    ReliefCrate *raw_crates,
    int raw_count,
    ReliefCrate *clean_crates,
    int max_clean,
    QuarantineRecord *quarantine_records,
    int *quarantine_count,
    int max_quarantine,
    ManifestStats *stats
);

/* File Export Helpers */
int write_clean_manifest_csv(const char *filepath, const ReliefCrate *crates, int count);
int write_quarantine_log_csv(const char *filepath, const QuarantineRecord *records, int count);

#endif /* SANITIZER_H */
