#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
RESULTS_DIR="${REPO_ROOT}/results/irace"

if command -v irace >/dev/null 2>&1; then
    IRACE_BIN="$(command -v irace)"
elif command -v Rscript >/dev/null 2>&1; then
    IRACE_BIN="$(Rscript -e 'cat(system.file("bin", "irace", package="irace"))' 2>/dev/null || true)"
else
    IRACE_BIN=""
fi

if [[ -z "${IRACE_BIN}" || ! -x "${IRACE_BIN}" ]]; then
    echo "irace is not installed or its executable could not be located." >&2
    echo "Install it with: R -q -e 'install.packages(\"irace\", repos=\"https://cloud.r-project.org\")'" >&2
    exit 1
fi

if [[ ! -x "${REPO_ROOT}/build/dckp_sbpo" ]]; then
    echo "Binary not found. Run make build first." >&2
    exit 1
fi

export LC_ALL=C
mkdir -p "${RESULTS_DIR}"

if [[ "${1:-}" == "--check-fast" ]]; then
    [[ $# -eq 1 ]] || { echo "--check-fast does not accept additional arguments" >&2; exit 2; }
    export DCKP_IRACE_TIME_LIMIT_MS=100
    export DCKP_IRACE_TIMEOUT_SECONDS=5
    set -- --check
else
    # Article executions must always use the fixed 1,000-second protocol.
    unset DCKP_IRACE_TIME_LIMIT_MS DCKP_IRACE_TIMEOUT_SECONDS || true
fi

IRACE_VERSION="$(Rscript -e 'cat(as.character(packageVersion("irace")))')"
echo "irace version: ${IRACE_VERSION}"
echo "scenario: ${SCRIPT_DIR}/scenario.txt"
echo "target time limit: ${DCKP_IRACE_TIME_LIMIT_MS:-1000000} ms"

WORKTREE_STATUS="$(git -C "${REPO_ROOT}" status --porcelain)"
if [[ -n "${WORKTREE_STATUS}" ]]; then
    echo "WARNING: the working tree is dirty; commit the experiment version before producing article results." >&2
fi

is_check=false
for argument in "$@"; do
    if [[ "${argument}" == "--check" || "${argument}" == "-c" ]]; then
        is_check=true
    fi
done

if [[ "${is_check}" == false ]]; then
    RUN_STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
    MANIFEST="${RESULTS_DIR}/tuning_${RUN_STAMP}_manifest.txt"
    LOG_FILE="${RESULTS_DIR}/tuning_${RUN_STAMP}.Rdata"
    {
        echo "timestamp_utc=${RUN_STAMP}"
        echo "git_commit=$(git -C "${REPO_ROOT}" rev-parse HEAD)"
        echo "worktree_dirty=$([[ -n "${WORKTREE_STATUS}" ]] && echo true || echo false)"
        echo "irace_version=${IRACE_VERSION}"
        echo "r_version=$(Rscript -e 'cat(as.character(getRversion()))')"
        echo "binary_sha256=$(sha256sum "${REPO_ROOT}/build/dckp_sbpo" | cut -d' ' -f1)"
        echo "scenario_sha256=$(sha256sum "${SCRIPT_DIR}/scenario.txt" | cut -d' ' -f1)"
        echo "parameters_sha256=$(sha256sum "${SCRIPT_DIR}/parameters.txt" | cut -d' ' -f1)"
        echo "system=$(uname -a)"
        echo "cpu=$(lscpu | sed -n 's/^Model name:[[:space:]]*//p' | head -n 1)"
        echo "max_experiments=1000"
        echo "time_limit_ms=1000000"
        echo "parallel=1"
    } > "${MANIFEST}"
    echo "manifest: ${MANIFEST}"
    echo "irace log: ${LOG_FILE}"
    set -- --log-file "${LOG_FILE}" "$@"
fi

cd "${SCRIPT_DIR}"
exec "${IRACE_BIN}" --scenario scenario.txt "$@"
