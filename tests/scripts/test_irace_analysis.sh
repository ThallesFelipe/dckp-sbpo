#!/usr/bin/env bash

set -euo pipefail

repo_root="$1"
tmp_dir="$(mktemp -d)"
trap 'rm -rf -- "${tmp_dir}"' EXIT
input="${tmp_dir}/results.csv"

# The single quotes intentionally protect R's $ expressions from Bash.
# shellcheck disable=SC2016
Rscript -e '
args <- commandArgs(trailingOnly = TRUE)
d <- expand.grid(
    configuration = c("baseline", "tuned"),
    instance = sprintf("%dI5", 1:20),
    run = 1:5,
    KEEP.OUT.ATTRS = FALSE,
    stringsAsFactors = FALSE
)
d$seed <- 42 + (as.integer(sub("I5", "", d$instance)) - 1) * 5 + d$run - 1
d$profit <- 1000 + as.integer(sub("I5", "", d$instance)) * 10 + ifelse(d$configuration == "tuned", 5, 0)
d$time_ms <- 1000000
d$valid <- TRUE
write.csv(d, args[[1]], row.names = FALSE)
' "${input}"

Rscript "${repo_root}/scripts/irace/analyze_test.R" "${input}" >/dev/null
[[ -s "${tmp_dir}/results_per_instance.csv" ]]
[[ -s "${tmp_dir}/results_summary.csv" ]]
grep -q '"wins",20' "${tmp_dir}/results_summary.csv"
