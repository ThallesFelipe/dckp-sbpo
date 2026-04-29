#!/usr/bin/env bash

set -euo pipefail

ALGO="ILS"
TIME_LIMIT_MS=600000
RUNS=5
OUTPUT=""
GLOBAL_START_MS="$(date +%s%3N)"

usage() {
    cat <<EOF
Usage: $(basename "$0") [--algo NAME] [--time-limit MS] [--runs N] [--output CSV]

Defaults:
  --algo       ILS
  --time-limit 600000  (milliseconds per run)
  --runs       5       (runs per instance with distinct seeds)
  --output     results/results_<algo>_<timestamp>.csv

Iterates over every instance under
DCKP-instances/DCKP-instances-set-I-100/{I1-I10,I11-I20}/ and writes a CSV.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --algo)         ALGO="$2"; shift 2 ;;
        --time-limit)   TIME_LIMIT_MS="$2"; shift 2 ;;
        --runs)         RUNS="$2"; shift 2 ;;
        --output)       OUTPUT="$2"; shift 2 ;;
        -h|--help)      usage; exit 0 ;;
        *)              echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

format_duration_ms() {
    local duration_ms="$1"
    local total_seconds=$((duration_ms / 1000))
    local minutes=$((total_seconds / 60))
    local seconds=$((total_seconds % 60))
    local millis=$((duration_ms % 1000))

    printf "%dm %02ds.%03d" "${minutes}" "${seconds}" "${millis}"
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="build"
INSTANCE_ROOT="${REPO_ROOT}/DCKP-instances/DCKP-instances-set-I-100"
RESULTS_DIR="${REPO_ROOT}/results"
BINARY="${REPO_ROOT}/${BUILD_DIR}/dckp_sbpo"

echo ">>> Building unified WSL target in ${BUILD_DIR}/ ..."
( cd "${REPO_ROOT}" && make build )

if [[ ! -x "${BINARY}" ]]; then
    echo "Unified build did not produce ${BINARY}" >&2
    exit 1
fi

mkdir -p "${RESULTS_DIR}"
if [[ -z "${OUTPUT}" ]]; then
    TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
    OUTPUT="${RESULTS_DIR}/results_${ALGO}_${TIMESTAMP}.csv"
fi
mkdir -p "$(dirname "${OUTPUT}")"

echo "instance,run,seed,profit,weight,capacity,start_time,end_time,time_ms,valid,algorithm" > "${OUTPUT}"

if [[ ! -d "${INSTANCE_ROOT}" ]]; then
    echo "Instance root not found: ${INSTANCE_ROOT}" >&2
    exit 1
fi

mapfile -t INSTANCES < <(find "${INSTANCE_ROOT}" -type f \
    | LC_ALL=C sort)

TOTAL_INSTANCES=${#INSTANCES[@]}
if [[ "${TOTAL_INSTANCES}" -eq 0 ]]; then
    echo "No instance files found under ${INSTANCE_ROOT}" >&2
    exit 1
fi

classify_group() {
    local base="$1"
    if [[ "${base}" =~ ^([1-9]|10)I[1-5]$ ]]; then
        echo "1"
    elif [[ "${base}" =~ ^(1[1-9]|20)I[1-5]$ ]]; then
        echo "2"
    else
        echo "?"
    fi
}

algorithm_start_ms="$(date +%s%3N)"
idx=0
for instance_path in "${INSTANCES[@]}"; do
    idx=$((idx + 1))
    base="$(basename "${instance_path}")"
    stem="${base%.txt}"

    for run in $(seq 1 "${RUNS}"); do
        seed="${RANDOM}"

        set +e
        output="$("${BINARY}" "${instance_path}" \
            --algo "${ALGO}" \
            --time-limit "${TIME_LIMIT_MS}" \
            --seed "${seed}" \
            --csv 2>/dev/null)"
        rc=$?
        set -e

        if [[ "${rc}" -ne 0 || -z "${output}" ]]; then
            echo "Experiment failed for ${stem} (run=${run}, seed=${seed})." >&2
            exit 1
        fi

        IFS=, read -r out_instance out_seed profit weight capacity start_time end_time time_ms valid algorithm <<<"${output}"
        if [[ -z "${out_instance}" || -z "${out_seed}" || -z "${profit}" || -z "${weight}" \
           || -z "${capacity}" || -z "${start_time}" || -z "${end_time}" \
           || -z "${time_ms}" || -z "${valid}" || -z "${algorithm}" ]]; then
            echo "Malformed CSV output for ${stem} (run=${run}, seed=${seed})." >&2
            echo "Raw output: ${output}" >&2
            exit 1
        fi
        if [[ "${out_instance}" != "${stem}" || "${out_seed}" != "${seed}" ]]; then
            echo "Mismatched CSV output for ${stem} (run=${run}, seed=${seed})." >&2
            echo "Raw output: ${output}" >&2
            exit 1
        fi

        echo "${out_instance},${run},${out_seed},${profit},${weight},${capacity},${start_time},${end_time},${time_ms},${valid},${algorithm}" \
            >> "${OUTPUT}"

        printf "[inst %d/%d | run %d/%d] %s -> profit=%s (%sms, valid=%s)\n" \
            "${idx}" "${TOTAL_INSTANCES}" "${run}" "${RUNS}" \
            "${out_instance}" "${profit}" "${time_ms}" "${valid}"
    done
done

algorithm_end_ms="$(date +%s%3N)"
algorithm_elapsed_ms=$((algorithm_end_ms - algorithm_start_ms))
echo ""
echo "Algorithm ${ALGO} elapsed time: $(format_duration_ms "${algorithm_elapsed_ms}")"

STATS="$(awk -F',' '
    function median(arr, n,    mid) {
        if (n == 0) return 0
        # Simple insertion sort — n is small (at most runs * 50).
        for (i = 2; i <= n; ++i) {
            key = arr[i]; j = i - 1
            while (j >= 1 && arr[j] > key) { arr[j+1] = arr[j]; --j }
            arr[j+1] = key
        }
        if (n % 2 == 1) return arr[(n+1)/2]
        mid = n / 2
        return (arr[mid] + arr[mid+1]) / 2
    }
    function stddev(arr, n, mean,   s) {
        if (n <= 1) return 0
        s = 0
        for (i = 1; i <= n; ++i) s += (arr[i] - mean) * (arr[i] - mean)
        return sqrt(s / (n - 1))
    }
    function group_of(inst,    m) {
        # [1-9]I[1-5] or 10I[1-5]  -> group 1
        # 11I[1-5] .. 20I[1-5]      -> group 2
        if (match(inst, /^([1-9]|10)I[1-5]$/)) return 1
        if (match(inst, /^(1[1-9]|20)I[1-5]$/)) return 2
        return 0
    }
    NR == 1 { next }  # skip header
    {
        inst = $1; profit = $4 + 0; time_ms = $9 + 0; valid = $10
        g = group_of(inst)

        # Overall
        ++nA; pA[nA] = profit; tA += time_ms; if (profit > bA) bA = profit
        if (valid != "true") ++invalid

        if (g == 1) {
            ++n1; p1[n1] = profit; t1 += time_ms; if (profit > b1) b1 = profit
        } else if (g == 2) {
            ++n2; p2[n2] = profit; t2 += time_ms; if (profit > b2) b2 = profit
        }
    }
    END {
        s1 = 0; for (i = 1; i <= n1; ++i) s1 += p1[i]
        s2 = 0; for (i = 1; i <= n2; ++i) s2 += p2[i]
        sA = 0; for (i = 1; i <= nA; ++i) sA += pA[i]

        m1 = (n1 > 0) ? s1 / n1 : 0
        m2 = (n2 > 0) ? s2 / n2 : 0
        mA = (nA > 0) ? sA / nA : 0

        med1 = median(p1, n1)
        med2 = median(p2, n2)
        medA = median(pA, nA)

        sd1 = stddev(p1, n1, m1)
        sd2 = stddev(p2, n2, m2)
        sdA = stddev(pA, nA, mA)

        mt1 = (n1 > 0) ? t1 / n1 : 0
        mt2 = (n2 > 0) ? t2 / n2 : 0
        mtA = (nA > 0) ? tA / nA : 0

        printf("g1_n=%d g1_best=%d g1_mean=%.2f g1_median=%.2f g1_stddev=%.2f g1_mean_time=%.2f\n", n1, b1, m1, med1, sd1, mt1)
        printf("g2_n=%d g2_best=%d g2_mean=%.2f g2_median=%.2f g2_stddev=%.2f g2_mean_time=%.2f\n", n2, b2, m2, med2, sd2, mt2)
        printf("all_n=%d all_best=%d all_mean=%.2f all_median=%.2f all_stddev=%.2f all_mean_time=%.2f\n", nA, bA, mA, medA, sdA, mtA)
        printf("invalid=%d\n", invalid + 0)
    }
' "${OUTPUT}")"

get_val() { echo "${STATS}" | tr ' ' '\n' | grep "^$1=" | cut -d= -f2; }

g1_best="$(get_val g1_best)";   g1_mean="$(get_val g1_mean)"
g1_median="$(get_val g1_median)"; g1_stddev="$(get_val g1_stddev)"
g1_mean_time="$(get_val g1_mean_time)"

g2_best="$(get_val g2_best)";   g2_mean="$(get_val g2_mean)"
g2_median="$(get_val g2_median)"; g2_stddev="$(get_val g2_stddev)"
g2_mean_time="$(get_val g2_mean_time)"

all_best="$(get_val all_best)"; all_mean="$(get_val all_mean)"
all_median="$(get_val all_median)"; all_stddev="$(get_val all_stddev)"
all_mean_time="$(get_val all_mean_time)"

invalid_count="$(get_val invalid)"

{
    echo ""
    echo "=== Group 1 (1I1-10I5) ==="
    echo "  best_profit : ${g1_best}"
    echo "  mean_profit : ${g1_mean}"
    echo "  median      : ${g1_median}"
    echo "  stddev      : ${g1_stddev}"
    echo "  mean_time_ms: ${g1_mean_time}"
    echo "=== Group 2 (11I1-20I5) ==="
    echo "  best_profit : ${g2_best}"
    echo "  mean_profit : ${g2_mean}"
    echo "  median      : ${g2_median}"
    echo "  stddev      : ${g2_stddev}"
    echo "  mean_time_ms: ${g2_mean_time}"
    echo "=== Overall ==="
    echo "  best_profit : ${all_best}"
    echo "  mean_profit : ${all_mean}"
    echo "  median      : ${all_median}"
    echo "  stddev      : ${all_stddev}"
    echo "  mean_time_ms: ${all_mean_time}"
    echo "  invalid     : ${invalid_count}"
}

{
    echo "# --- Group 1 (1I1-10I5, 50 instances) ---"
    echo "# group1_best_profit=${g1_best}"
    echo "# group1_mean_profit=${g1_mean}"
    echo "# group1_median_profit=${g1_median}"
    echo "# group1_stddev_profit=${g1_stddev}"
    echo "# group1_mean_time_ms=${g1_mean_time}"
    echo "# --- Group 2 (11I1-20I5, 50 instances) ---"
    echo "# group2_best_profit=${g2_best}"
    echo "# group2_mean_profit=${g2_mean}"
    echo "# group2_median_profit=${g2_median}"
    echo "# group2_stddev_profit=${g2_stddev}"
    echo "# group2_mean_time_ms=${g2_mean_time}"
    echo "# --- Overall ---"
    echo "# overall_best_profit=${all_best}"
    echo "# overall_mean_profit=${all_mean}"
    echo "# overall_median_profit=${all_median}"
    echo "# overall_stddev_profit=${all_stddev}"
    echo "# overall_mean_time_ms=${all_mean_time}"
    echo "# invalid_count=${invalid_count}"
} >> "${OUTPUT}"

if command -v realpath >/dev/null 2>&1; then
    ABS_OUTPUT="$(realpath "${OUTPUT}")"
else
    ABS_OUTPUT="$(cd "$(dirname "${OUTPUT}")" && pwd)/$(basename "${OUTPUT}")"
fi

echo ""
echo "CSV written to: ${ABS_OUTPUT}"

GLOBAL_END_MS="$(date +%s%3N)"
GLOBAL_ELAPSED_MS=$((GLOBAL_END_MS - GLOBAL_START_MS))
echo "Total experiment battery time: $(format_duration_ms "${GLOBAL_ELAPSED_MS}")"
