#!/usr/bin/env bash

set -euo pipefail

error() {
    echo "target-runner: $*" >&2
    exit 1
}

# irace passes four fixed arguments when capping is disabled. Everything after
# them is the active target configuration.
[[ $# -ge 4 ]] || error "expected configuration ID, instance ID, seed and instance"

configuration_id="$1"
instance_id="$2"
seed="$3"
instance_path="$4"
shift 4

# The scenario always has five unconditional parameters, each represented by
# a switch/value pair. Reject a malformed irace invocation instead of silently
# racing a different algorithm.
[[ $# -eq 10 ]] || error "configuration ${configuration_id} has $# arguments; expected 10"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BINARY="${REPO_ROOT}/build/dckp_sbpo"

[[ -x "${BINARY}" ]] || error "binary not found at ${BINARY}; run make build first"
[[ -f "${instance_path}" ]] || error "instance ${instance_id} not found: ${instance_path}"
[[ "${seed}" =~ ^[0-9]+$ ]] || error "invalid seed from irace: ${seed}"

# Production values are fixed by the article protocol. The two environment
# overrides are used only by automated checks; run.sh removes them for tuning.
TIME_LIMIT_MS="${DCKP_IRACE_TIME_LIMIT_MS:-1000000}"
WATCHDOG_SECONDS="${DCKP_IRACE_TIMEOUT_SECONDS:-1010}"

if ! output="$(timeout --signal=TERM --kill-after=5s "${WATCHDOG_SECONDS}s" \
    "${BINARY}" "${instance_path}" --algo ILS --time-limit "${TIME_LIMIT_MS}" \
    --seed "${seed}" --csv "$@")"; then
    error "execution failed (configuration=${configuration_id}, instance=${instance_id}, seed=${seed})"
fi

[[ -n "${output}" && "${output}" != *$'\n'* ]] || error "target emitted malformed multiline output"

IFS=, read -r out_instance out_seed profit weight capacity start_time end_time time_ms valid algorithm extra <<<"${output}"

expected_instance="$(basename "${instance_path}")"
expected_instance="${expected_instance%.txt}"
[[ -z "${extra}" ]] || error "target emitted extra CSV fields"
[[ "${out_instance}" == "${expected_instance}" ]] || error "target returned instance ${out_instance}, expected ${expected_instance}"
[[ "${out_seed}" == "${seed}" ]] || error "target returned seed ${out_seed}, expected ${seed}"
[[ "${profit}" =~ ^[0-9]+$ ]] || error "target returned malformed profit: ${profit}"
[[ "${weight}" =~ ^[0-9]+$ && "${capacity}" =~ ^[0-9]+$ ]] || error "target returned malformed weight or capacity"
[[ -n "${start_time}" && -n "${end_time}" && "${time_ms}" =~ ^[0-9]+$ ]] || error "target returned malformed timing data"
[[ "${valid}" == "true" ]] || error "target returned an invalid solution"
[[ "${algorithm}" == "ILS" ]] || error "target returned algorithm ${algorithm}, expected ILS"

# irace minimizes costs; negating profit therefore maximizes solution quality.
printf '%s\n' "-${profit}"
