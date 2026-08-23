#!/usr/bin/env bash

set -euo pipefail

usage() {
    echo "Usage: $(basename "$0") PERTURBATION VND_ADD VND_SWAP_1_1 VND_SWAP_2_1 VND_SWAP_1_2" >&2
}

[[ $# -eq 5 ]] || { usage; exit 2; }

perturbation="$1"
vnd_add="$2"
vnd_swap_1_1="$3"
vnd_swap_2_1="$4"
vnd_swap_1_2="$5"

[[ "${perturbation}" =~ ^[1-8]$ ]] || { echo "PERTURBATION must be in [1, 8]." >&2; exit 2; }
for flag in "${vnd_add}" "${vnd_swap_1_1}" "${vnd_swap_2_1}" "${vnd_swap_1_2}"; do
    [[ "${flag}" =~ ^[01]$ ]] || { echo "VND flags must be 0 or 1." >&2; exit 2; }
done
if [[ "${vnd_add}${vnd_swap_1_1}${vnd_swap_2_1}${vnd_swap_1_2}" == "0000" ]]; then
    echo "At least one VND neighborhood must be enabled." >&2
    exit 2
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BINARY="${REPO_ROOT}/build/dckp_sbpo"
OUTPUT_DIR="${REPO_ROOT}/results/irace"
OUTPUT="${OUTPUT_DIR}/test_$(date -u +%Y%m%dT%H%M%SZ).csv"

[[ -x "${BINARY}" ]] || { echo "Binary not found at ${BINARY}. Run make build first." >&2; exit 1; }
command -v Rscript >/dev/null 2>&1 || { echo "Rscript is required for the final analysis." >&2; exit 1; }

mkdir -p "${OUTPUT_DIR}"
echo "configuration,instance,run,seed,profit,weight,capacity,start_time,end_time,time_ms,valid,algorithm,perturbation_strength,vnd_add,vnd_swap_1_1,vnd_swap_2_1,vnd_swap_1_2" > "${OUTPUT}"

config_names=(baseline tuned)
perturbations=(4 "${perturbation}")
adds=(1 "${vnd_add}")
swaps_1_1=(1 "${vnd_swap_1_1}")
swaps_2_1=(1 "${vnd_swap_2_1}")
swaps_1_2=(1 "${vnd_swap_1_2}")

instance_index=0
completed=0
total_runs=200

run_configuration() {
    local config_index="$1"
    local instance_path="$2"
    local expected_instance="$3"
    local run_number="$4"
    local seed="$5"
    local output

    if ! output="$(timeout --signal=TERM --kill-after=5s 1010s \
        "${BINARY}" "${instance_path}" --algo ILS --time-limit 1000000 --seed "${seed}" --csv \
        --ils-perturbation-strength "${perturbations[config_index]}" \
        --vnd-add "${adds[config_index]}" \
        --vnd-swap-1-1 "${swaps_1_1[config_index]}" \
        --vnd-swap-2-1 "${swaps_2_1[config_index]}" \
        --vnd-swap-1-2 "${swaps_1_2[config_index]}")"; then
        echo "Execution failed for ${expected_instance}, run=${run_number}, configuration=${config_names[config_index]}." >&2
        exit 1
    fi

    [[ -n "${output}" && "${output}" != *$'\n'* ]] || { echo "Malformed target output." >&2; exit 1; }
    IFS=, read -r out_instance out_seed profit weight capacity start_time end_time time_ms valid algorithm extra <<<"${output}"
    [[ -z "${extra}" && "${out_instance}" == "${expected_instance}" && "${out_seed}" == "${seed}" ]] || {
        echo "Mismatched CSV output for ${expected_instance}, seed=${seed}." >&2
        exit 1
    }
    [[ "${profit}" =~ ^[0-9]+$ && "${time_ms}" =~ ^[0-9]+$ && "${valid}" == "true" && "${algorithm}" == "ILS" ]] || {
        echo "Invalid CSV output for ${expected_instance}, seed=${seed}." >&2
        exit 1
    }

    echo "${config_names[config_index]},${out_instance},${run_number},${out_seed},${profit},${weight},${capacity},${start_time},${end_time},${time_ms},${valid},${algorithm},${perturbations[config_index]},${adds[config_index]},${swaps_1_1[config_index]},${swaps_2_1[config_index]},${swaps_1_2[config_index]}" >> "${OUTPUT}"
    completed=$((completed + 1))
    printf '[%d/%d] %s run=%d seed=%d configuration=%s profit=%s\n' \
        "${completed}" "${total_runs}" "${expected_instance}" "${run_number}" "${seed}" \
        "${config_names[config_index]}" "${profit}"
}

while IFS= read -r relative_instance; do
    [[ -z "${relative_instance}" ]] && continue
    instance_index=$((instance_index + 1))
    instance_path="${SCRIPT_DIR}/${relative_instance}"
    expected_instance="$(basename "${instance_path}")"
    expected_instance="${expected_instance%.txt}"
    [[ -f "${instance_path}" ]] || { echo "Missing test instance: ${instance_path}" >&2; exit 1; }

    for run_number in 1 2 3 4 5; do
        seed=$((42 + (instance_index - 1) * 5 + run_number - 1))
        # Alternate execution order to reduce systematic temporal bias while
        # retaining identical instance/seed pairs for both configurations.
        if ((run_number % 2 == 1)); then
            run_configuration 0 "${instance_path}" "${expected_instance}" "${run_number}" "${seed}"
            run_configuration 1 "${instance_path}" "${expected_instance}" "${run_number}" "${seed}"
        else
            run_configuration 1 "${instance_path}" "${expected_instance}" "${run_number}" "${seed}"
            run_configuration 0 "${instance_path}" "${expected_instance}" "${run_number}" "${seed}"
        fi
    done
done < "${SCRIPT_DIR}/test-instances.txt"

[[ "${instance_index}" -eq 20 && "${completed}" -eq "${total_runs}" ]] || {
    echo "Expected 20 instances and 200 results; got ${instance_index} and ${completed}." >&2
    exit 1
}

Rscript "${SCRIPT_DIR}/analyze_test.R" "${OUTPUT}"
echo "Test CSV written to: ${OUTPUT}"
