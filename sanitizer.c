#include "sanitizer.h"
#include <ctype.h>
#include <math.h>

/* Helper to strip leading and trailing whitespace/newlines/quotes */
static void trim_whitespace(char *str) {
    if (!str) return;
    char *start = str;
    while (*start && (isspace((unsigned char)*start) || *start == '"' || *start == '\'')) {
        start++;
    }
    char *end = start + strlen(start);
    while (end > start && (isspace((unsigned char)*(end - 1)) || *(end - 1) == '"' || *(end - 1) == '\'')) {
        end--;
    }
    *end = '\0';
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

/* Helper to split CSV line into tokens safely (handling empty fields) */
static int split_csv_line(char *line, char tokens[][MAX_LINE_LEN], int max_tokens) {
    int count = 0;
    char *cur = line;
    char *start = cur;

    while (*cur && count < max_tokens) {
        if (*cur == ',') {
            *cur = '\0';
            strncpy(tokens[count], start, MAX_LINE_LEN - 1);
            tokens[count][MAX_LINE_LEN - 1] = '\0';
            trim_whitespace(tokens[count]);
            count++;
            start = cur + 1;
        }
        cur++;
    }

    if (count < max_tokens) {
        strncpy(tokens[count], start, MAX_LINE_LEN - 1);
        tokens[count][MAX_LINE_LEN - 1] = '\0';
        trim_whitespace(tokens[count]);
        count++;
    }

    return count;
}

ValidationCode validate_crate_fields(const ReliefCrate *crate, char *reason_buf, size_t buf_len) {
    if (!crate) {
        if (reason_buf) snprintf(reason_buf, buf_len, "Null crate pointer");
        return ERR_MALFORMED_ROW;
    }

    if (strlen(crate->crate_id) == 0) {
        if (reason_buf) snprintf(reason_buf, buf_len, "Missing crate_id");
        return ERR_MISSING_FIELD;
    }

    if (crate->category == CAT_UNKNOWN) {
        if (reason_buf) snprintf(reason_buf, buf_len, "Unrecognized category");
        return ERR_UNKNOWN_CATEGORY;
    }

    if (!isfinite(crate->weight_kg) || crate->weight_kg <= 0.0) {
        if (reason_buf) snprintf(reason_buf, buf_len, "Invalid non-positive weight (%.2f kg <= 0)", crate->weight_kg);
        return ERR_INVALID_WEIGHT;
    }

    if (!isfinite(crate->priority_score) || crate->priority_score <= 0.0) {
        if (reason_buf) snprintf(reason_buf, buf_len, "Invalid non-positive priority score (%.2f <= 0)", crate->priority_score);
        return ERR_INVALID_PRIORITY;
    }

    if (crate->status != STATUS_READY) {
        if (reason_buf) snprintf(reason_buf, buf_len, "Inspection status is %s (only READY permitted for dispatch)",
                                 status_to_string(crate->status));
        return ERR_NON_READY_STATUS;
    }

    if (reason_buf) snprintf(reason_buf, buf_len, "All constraints satisfied");
    return VALID_OK;
}

int parse_csv_line(const char *raw_line, int line_num, ReliefCrate *crate, char *err_msg, size_t err_buf_len) {
    if (!raw_line || !crate) return -1;

    char line_copy[MAX_LINE_LEN];
    strncpy(line_copy, raw_line, sizeof(line_copy) - 1);
    line_copy[sizeof(line_copy) - 1] = '\0';

    char tokens[12][MAX_LINE_LEN];
    int token_count = split_csv_line(line_copy, tokens, 12);

    /*
     * We support two standard layouts:
     * 9-column exported layout:
     *   crate_id,description,category,weight_kg,priority_score,density,arrival_timestamp,fragility_constraint,status
     * 8-column layout (without density):
     *   crate_id,description,category,weight_kg,priority_score,arrival_timestamp,fragility_constraint,status
     * 7-column layout (without description):
     *   crate_id,category,weight_kg,priority_score,arrival_timestamp,fragility_constraint,status
     */

    memset(crate, 0, sizeof(ReliefCrate));
    crate->line_number = line_num;

    if (token_count < 7) {
        if (err_msg) snprintf(err_msg, err_buf_len, "Malformed row: expected at least 7 columns, got %d", token_count);
        return ERR_MALFORMED_ROW;
    }

    int idx_id, idx_desc, idx_cat, idx_w, idx_v, idx_ts, idx_frag, idx_st;

    if (token_count >= 9) {
        /* Exported clean-manifest layout includes density before timestamp. */
        idx_id = 0;
        idx_desc = 1;
        idx_cat = 2;
        idx_w = 3;
        idx_v = 4;
        idx_ts = 6;
        idx_frag = 7;
        idx_st = 8;
        strncpy(crate->description, tokens[idx_desc], MAX_DESC_LEN - 1);
    } else if (token_count == 8) {
        idx_id = 0;
        idx_desc = 1;
        idx_cat = 2;
        idx_w = 3;
        idx_v = 4;
        idx_ts = 5;
        idx_frag = 6;
        idx_st = 7;
        strncpy(crate->description, tokens[idx_desc], MAX_DESC_LEN - 1);
    } else {
        idx_id = 0;
        idx_desc = -1;
        idx_cat = 1;
        idx_w = 2;
        idx_v = 3;
        idx_ts = 4;
        idx_frag = 5;
        idx_st = 6;
        snprintf(crate->description, MAX_DESC_LEN, "Relief Crate %s", tokens[idx_id]);
    }

    /* 1. crate_id */
    if (strlen(tokens[idx_id]) == 0) {
        if (err_msg) snprintf(err_msg, err_buf_len, "Missing crate_id");
        return ERR_MISSING_FIELD;
    }
    strncpy(crate->crate_id, tokens[idx_id], MAX_ID_LEN - 1);

    /* 2. category */
    crate->category = string_to_category(tokens[idx_cat]);
    if (crate->category == CAT_UNKNOWN) {
        if (err_msg) snprintf(err_msg, err_buf_len, "Invalid category '%s'", tokens[idx_cat]);
        return ERR_UNKNOWN_CATEGORY;
    }

    /* 3. weight_kg */
    if (strlen(tokens[idx_w]) == 0) {
        if (err_msg) snprintf(err_msg, err_buf_len, "Missing numeric weight_kg");
        return ERR_MISSING_FIELD;
    }
    char *endptr;
    crate->weight_kg = strtod(tokens[idx_w], &endptr);
    if (endptr == tokens[idx_w] || (*endptr != '\0' && !isspace((unsigned char)*endptr))) {
        if (err_msg) snprintf(err_msg, err_buf_len, "Non-numeric weight value '%s'", tokens[idx_w]);
        return ERR_INVALID_WEIGHT;
    }
    if (!isfinite(crate->weight_kg) || crate->weight_kg <= 0.0) {
        if (err_msg) snprintf(err_msg, err_buf_len, "Weight must be finite and greater than zero");
        return ERR_INVALID_WEIGHT;
    }

    /* 4. priority_score */
    if (strlen(tokens[idx_v]) == 0) {
        if (err_msg) snprintf(err_msg, err_buf_len, "Missing numeric priority_score");
        return ERR_MISSING_FIELD;
    }
    crate->priority_score = strtod(tokens[idx_v], &endptr);
    if (endptr == tokens[idx_v] || (*endptr != '\0' && !isspace((unsigned char)*endptr))) {
        if (err_msg) snprintf(err_msg, err_buf_len, "Non-numeric priority value '%s'", tokens[idx_v]);
        return ERR_INVALID_PRIORITY;
    }
    if (!isfinite(crate->priority_score) || crate->priority_score <= 0.0) {
        if (err_msg) snprintf(err_msg, err_buf_len, "Priority must be finite and greater than zero");
        return ERR_INVALID_PRIORITY;
    }

    /* 5. density */
    crate->density = crate->priority_score / crate->weight_kg;

    /* 6. arrival_timestamp */
    if (strlen(tokens[idx_ts]) == 0) {
        strncpy(crate->arrival_timestamp, "1970-01-01T00:00:00Z", MAX_TIMESTAMP_LEN - 1);
    } else {
        strncpy(crate->arrival_timestamp, tokens[idx_ts], MAX_TIMESTAMP_LEN - 1);
    }

    /* 7. fragility_constraint */
    if (strlen(tokens[idx_frag]) == 0) {
        crate->fragility_constraint = 0;
    } else {
        crate->fragility_constraint = (atoi(tokens[idx_frag]) != 0) ? 1 : 0;
    }

    /* 8. status */
    crate->status = string_to_status(tokens[idx_st]);
    if (crate->status == STATUS_UNKNOWN) {
        if (err_msg) snprintf(err_msg, err_buf_len, "Unknown status '%s'", tokens[idx_st]);
        return ERR_MALFORMED_ROW;
    }

    return VALID_OK;
}

int sanitize_manifest(
    const char *input_csv_path,
    const char *clean_output_path,
    const char *quarantine_output_path,
    ReliefCrate *clean_crates,
    int max_crates,
    int *clean_count,
    ManifestStats *stats
) {
    if (!input_csv_path || !clean_crates || !clean_count || !stats) {
        return -1;
    }

    memset(stats, 0, sizeof(ManifestStats));
    *clean_count = 0;

    FILE *fin = fopen(input_csv_path, "r");
    if (!fin) {
        fprintf(stderr, "[ERROR] Cannot open input manifest file: %s\n", input_csv_path);
        return -1;
    }

    QuarantineRecord *quarantine_records = (QuarantineRecord*)malloc(sizeof(QuarantineRecord) * MAX_QUARANTINE_RECORDS);
    if (!quarantine_records) {
        fclose(fin);
        return -1;
    }
    int quarantine_count = 0;

    /* Temporary pool for candidates to undergo deduplication */
    ReliefCrate *raw_valid_pool = (ReliefCrate*)malloc(sizeof(ReliefCrate) * MAX_CRATES);
    if (!raw_valid_pool) {
        free(quarantine_records);
        fclose(fin);
        return -1;
    }
    int pool_count = 0;

    char line_buf[MAX_LINE_LEN];
    int line_number = 0;
    bool is_header = true;

    while (fgets(line_buf, sizeof(line_buf), fin)) {
        line_number++;

        /* Strip trailing CR/LF */
        size_t len = strlen(line_buf);
        while (len > 0 && (line_buf[len - 1] == '\r' || line_buf[len - 1] == '\n')) {
            line_buf[--len] = '\0';
        }

        /* Skip blank lines */
        if (len == 0) continue;

        /* Check header */
        if (is_header) {
            is_header = false;
            if (strstr(line_buf, "crate_id") || strstr(line_buf, "category") || strstr(line_buf, "weight")) {
                continue; /* Header successfully skipped */
            }
        }

        stats->total_raw_records++;

        ReliefCrate parsed_crate;
        char err_reason[MAX_REASON_LEN] = {0};
        int parse_res = parse_csv_line(line_buf, line_number, &parsed_crate, err_reason, sizeof(err_reason));

        if (parse_res != VALID_OK) {
            /* Log to quarantine */
            if (quarantine_count < MAX_QUARANTINE_RECORDS) {
                QuarantineRecord *qr = &quarantine_records[quarantine_count++];
                strncpy(qr->crate_id, parsed_crate.crate_id[0] ? parsed_crate.crate_id : "UNKNOWN", MAX_ID_LEN - 1);
                qr->line_number = line_number;
                strncpy(qr->raw_record, line_buf, MAX_LINE_LEN - 1);
                strncpy(qr->rejection_reason, err_reason, MAX_REASON_LEN - 1);
                strncpy(qr->action_taken, "Rejected at parser", MAX_REASON_LEN - 1);
                strncpy(qr->detected_timestamp, "CURRENT_AUDIT", MAX_TIMESTAMP_LEN - 1);
            }
            stats->quarantined_count++;

            switch (parse_res) {
                case ERR_MISSING_FIELD:    stats->missing_values_count++; break;
                case ERR_INVALID_WEIGHT:   stats->invalid_weights_count++; break;
                case ERR_INVALID_PRIORITY: stats->invalid_priorities_count++; break;
                default:                   stats->schema_violation_count++; break;
            }
            continue;
        }

        /* Check non-READY inspection status */
        if (parsed_crate.status != STATUS_READY) {
            if (quarantine_count < MAX_QUARANTINE_RECORDS) {
                QuarantineRecord *qr = &quarantine_records[quarantine_count++];
                strncpy(qr->crate_id, parsed_crate.crate_id, MAX_ID_LEN - 1);
                qr->line_number = line_number;
                strncpy(qr->raw_record, line_buf, MAX_LINE_LEN - 1);
                snprintf(qr->rejection_reason, MAX_REASON_LEN, "Non-READY status: %s", status_to_string(parsed_crate.status));
                strncpy(qr->action_taken, "Filtered out of dispatch manifest", MAX_REASON_LEN - 1);
                strncpy(qr->detected_timestamp, parsed_crate.arrival_timestamp, MAX_TIMESTAMP_LEN - 1);
            }
            stats->quarantined_count++;
            stats->non_ready_status_count++;
            continue;
        }

        /* Record passed initial syntax checks and is READY -> add to deduplication pool */
        if (pool_count < MAX_CRATES) {
            raw_valid_pool[pool_count++] = parsed_crate;
        }
    }

    fclose(fin);

    /*
     * Deduplication by crate_id: Keep the entry with the LATEST ISO-8601 timestamp.
     * Older duplicates are moved to quarantine log.
     */
    for (int i = 0; i < pool_count; i++) {
        ReliefCrate *cand = &raw_valid_pool[i];
        int existing_idx = -1;

        for (int j = 0; j < *clean_count; j++) {
            if (strcmp(clean_crates[j].crate_id, cand->crate_id) == 0) {
                existing_idx = j;
                break;
            }
        }

        if (existing_idx == -1) {
            /* Brand new unique crate_id */
            if (*clean_count < max_crates) {
                clean_crates[(*clean_count)++] = *cand;
            }
        } else {
            /* Duplicate crate_id encountered: compare arrival timestamps */
            stats->duplicates_resolved++;
            int cmp = compare_iso_timestamps(cand->arrival_timestamp, clean_crates[existing_idx].arrival_timestamp);

            if (cmp > 0) {
                /* cand is NEWER -> replace existing and quarantine the older one */
                if (quarantine_count < MAX_QUARANTINE_RECORDS) {
                    QuarantineRecord *qr = &quarantine_records[quarantine_count++];
                    strncpy(qr->crate_id, clean_crates[existing_idx].crate_id, MAX_ID_LEN - 1);
                    qr->line_number = clean_crates[existing_idx].line_number;
                    snprintf(qr->raw_record, MAX_LINE_LEN, "%s,%s,%.2f,%.2f,%s",
                             clean_crates[existing_idx].crate_id,
                             category_to_string(clean_crates[existing_idx].category),
                             clean_crates[existing_idx].weight_kg,
                             clean_crates[existing_idx].priority_score,
                             clean_crates[existing_idx].arrival_timestamp);
                    snprintf(qr->rejection_reason, MAX_REASON_LEN,
                             "Duplicate crate_id: superseded by newer record (%s > %s)",
                             cand->arrival_timestamp, clean_crates[existing_idx].arrival_timestamp);
                    strncpy(qr->action_taken, "Replaced by newer manifest record", MAX_REASON_LEN - 1);
                    strncpy(qr->detected_timestamp, cand->arrival_timestamp, MAX_TIMESTAMP_LEN - 1);
                }
                stats->quarantined_count++;
                clean_crates[existing_idx] = *cand; /* Replace with newer */
            } else {
                /* cand is OLDER or EQUAL -> quarantine cand and retain existing */
                if (quarantine_count < MAX_QUARANTINE_RECORDS) {
                    QuarantineRecord *qr = &quarantine_records[quarantine_count++];
                    strncpy(qr->crate_id, cand->crate_id, MAX_ID_LEN - 1);
                    qr->line_number = cand->line_number;
                    snprintf(qr->raw_record, MAX_LINE_LEN, "%s,%s,%.2f,%.2f,%s",
                             cand->crate_id,
                             category_to_string(cand->category),
                             cand->weight_kg,
                             cand->priority_score,
                             cand->arrival_timestamp);
                    snprintf(qr->rejection_reason, MAX_REASON_LEN,
                             "Duplicate crate_id: older record superseded by %s",
                             clean_crates[existing_idx].arrival_timestamp);
                    strncpy(qr->action_taken, "Ignored older duplicate record", MAX_REASON_LEN - 1);
                    strncpy(qr->detected_timestamp, cand->arrival_timestamp, MAX_TIMESTAMP_LEN - 1);
                }
                stats->quarantined_count++;
            }
        }
    }

    stats->accepted_ready_count = *clean_count;

    /* Compute clean manifest totals */
    stats->total_clean_weight = 0.0;
    stats->total_clean_priority = 0.0;
    for (int i = 0; i < *clean_count; i++) {
        stats->total_clean_weight += clean_crates[i].weight_kg;
        stats->total_clean_priority += clean_crates[i].priority_score;
    }

    /* Write outputs if paths specified */
    if (clean_output_path) {
        write_clean_manifest_csv(clean_output_path, clean_crates, *clean_count);
    }
    if (quarantine_output_path) {
        write_quarantine_log_csv(quarantine_output_path, quarantine_records, quarantine_count);
    }

    free(raw_valid_pool);
    free(quarantine_records);
    return 0;
}

int write_clean_manifest_csv(const char *filepath, const ReliefCrate *crates, int count) {
    if (!filepath || !crates) return -1;

    FILE *fout = fopen(filepath, "w");
    if (!fout) {
        fprintf(stderr, "[ERROR] Cannot create clean manifest file: %s\n", filepath);
        return -1;
    }

    /* Standard CSV header */
    fprintf(fout, "crate_id,description,category,weight_kg,priority_score,density,arrival_timestamp,fragility_constraint,status\n");

    for (int i = 0; i < count; i++) {
        fprintf(fout, "%s,\"%s\",%s,%.2f,%.2f,%.4f,%s,%d,%s\n",
                crates[i].crate_id,
                crates[i].description,
                category_to_string(crates[i].category),
                crates[i].weight_kg,
                crates[i].priority_score,
                crates[i].density,
                crates[i].arrival_timestamp,
                crates[i].fragility_constraint,
                status_to_string(crates[i].status));
    }

    fclose(fout);
    return 0;
}

int write_quarantine_log_csv(const char *filepath, const QuarantineRecord *records, int count) {
    if (!filepath || !records) return -1;

    FILE *fout = fopen(filepath, "w");
    if (!fout) {
        fprintf(stderr, "[ERROR] Cannot create quarantine log file: %s\n", filepath);
        return -1;
    }

    /* Structured Audit Trail Header */
    fprintf(fout, "log_id,line_number,crate_id,rejection_reason,action_taken,raw_record\n");

    for (int i = 0; i < count; i++) {
        fprintf(fout, "%d,%d,%s,\"%s\",\"%s\",\"%s\"\n",
                i + 1,
                records[i].line_number,
                records[i].crate_id,
                records[i].rejection_reason,
                records[i].action_taken,
                records[i].raw_record);
    }

    fclose(fout);
    return 0;
}
