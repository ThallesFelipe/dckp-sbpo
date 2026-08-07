#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
JOBS="${DCKP_AUDIT_JOBS:-$(nproc)}"

cd "${REPO_ROOT}"

if [[ ! "${JOBS}" =~ ^[1-9][0-9]*$ ]]; then
    echo "DCKP_AUDIT_JOBS must be a positive integer." >&2
    exit 2
fi

stage() {
    printf '\n>>> %s\n' "$1"
}

configure_build() {
    local build_dir="$1"
    local compiler="$2"
    local build_type="$3"
    shift 3

    cmake --fresh -S . -B "${build_dir}" -G Ninja \
        -DCMAKE_BUILD_TYPE="${build_type}" \
        -DCMAKE_CXX_COMPILER="${compiler}" \
        -DDCKP_ENABLE_HARDENING=ON \
        -DDCKP_WARNINGS_AS_ERRORS=ON \
        "$@"
    cmake --build "${build_dir}" --parallel "${JOBS}" --clean-first
}

run_tests() {
    ctest --test-dir "$1" --output-on-failure
}

required_tools=(
    g++ clang++-18 clangd-18 clang-tidy-18 run-clang-tidy-18
    cmake ninja cppcheck shellcheck valgrind gcovr python3
)

for tool in "${required_tools[@]}"; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "Required audit tool not found: ${tool}" >&2
        exit 1
    fi
done

STATIC_LOG="$(mktemp)"
TIDY_LOG="$(mktemp)"
CLANGD_LOG="$(mktemp)"
CLANGD_FILTERED="$(mktemp)"
SMOKE_CSV="$(mktemp --suffix=.csv)"
SMOKE_LOG="$(mktemp)"
ILS_CSV="$(mktemp --suffix=.csv)"
ILS_LOG="$(mktemp)"

cleanup() {
    rm -f -- "${STATIC_LOG}" "${TIDY_LOG}" "${CLANGD_LOG}" \
        "${CLANGD_FILTERED}" "${SMOKE_CSV}" "${SMOKE_LOG}" \
        "${ILS_CSV}" "${ILS_LOG}"
}
trap cleanup EXIT

stage "GCC 13 Release, hardening and strict warnings"
configure_build build g++ Release \
    -DDCKP_ENABLE_SANITIZERS=OFF
run_tests build

stage "Clang 18 Release, hardening and strict warnings"
configure_build cmake-build-audit-clang clang++-18 Release \
    -DDCKP_ENABLE_SANITIZERS=OFF
run_tests cmake-build-audit-clang

stage "Clang 18 AddressSanitizer and UndefinedBehaviorSanitizer"
configure_build cmake-build-audit-sanitize clang++-18 Debug \
    -DDCKP_ENABLE_SANITIZERS=ON
ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1:check_initialization_order=1 \
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
    ctest --test-dir cmake-build-audit-sanitize --output-on-failure

stage "GCC static analyzer"
configure_build cmake-build-audit-analyzer g++ Debug \
    -DDCKP_ENABLE_SANITIZERS=OFF \
    "-DCMAKE_CXX_FLAGS=-fanalyzer -Wno-analyzer-use-of-uninitialized-value"
run_tests cmake-build-audit-analyzer

stage "Cppcheck and clang-tidy"
if ! cppcheck --enable=all --inconclusive --check-level=exhaustive \
    --std=c++23 --language=c++ --error-exitcode=1 \
    --suppress=missingIncludeSystem --suppress=checkersReport \
    -Isrc src tests >"${STATIC_LOG}" 2>&1; then
    cat "${STATIC_LOG}" >&2
    exit 1
fi
if ! run-clang-tidy-18 -quiet -p cmake-build-audit-clang \
    -extra-arg=-w >"${TIDY_LOG}" 2>&1; then
    cat "${TIDY_LOG}" >&2
    exit 1
fi
printf 'cppcheck: OK\nclang-tidy: OK\n'

stage "Shell scripts and Zed configuration"
shellcheck --severity=style dckp_sbpo scripts/run_experiments.sh scripts/audit.sh
bash -n dckp_sbpo scripts/run_experiments.sh scripts/audit.sh
python3 -m json.tool .zed/settings.json >/dev/null
test -s build/compile_commands.json

stage "clangd 18 project diagnostics"
while IFS= read -r source; do
    : >"${CLANGD_LOG}"
    : >"${CLANGD_FILTERED}"
    clangd_status=0
    clangd-18 --check="${source}" --enable-config --log=error \
        >"${CLANGD_LOG}" 2>&1 || clangd_status=$?
    grep -vF 'tweak: ExtractFunction ==> FAIL: Cannot extract break/continue without corresponding loop/switch statement.' \
        "${CLANGD_LOG}" >"${CLANGD_FILTERED}" || true
    if [[ -s "${CLANGD_FILTERED}" || ( ${clangd_status} -ne 0 && ! -s "${CLANGD_LOG}" ) ]]; then
        echo "clangd failed for ${source}:" >&2
        cat "${CLANGD_LOG}" >&2
        exit 1
    fi
done < <(find src tests -type f -name '*.cpp' -print | LC_ALL=C sort)
printf 'clangd: OK\n'

stage "Valgrind"
test_binary_count="$(find build/tests -maxdepth 1 -type f -name 'test_*' -executable | wc -l)"
if [[ "${test_binary_count}" -eq 0 ]]; then
    echo "No test binaries found for Valgrind." >&2
    exit 1
fi
find build/tests -maxdepth 1 -type f -name 'test_*' -executable -print0 \
    | xargs -0 -r -n1 valgrind --quiet --error-exitcode=1 \
        --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all \
        >/dev/null
valgrind --quiet --error-exitcode=1 --leak-check=full \
    --show-leak-kinds=all --errors-for-leak-kinds=all \
    ./build/dckp_sbpo \
    DCKP-instances/DCKP-instances-set-I-100/I1-I10/1I1 \
    --algo Greedy_MaxProfit --csv >/dev/null
printf 'Valgrind: %s test binaries and CLI clean\n' "${test_binary_count}"

stage "Coverage"
cmake --fresh -S . -B cmake-build-audit-coverage -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_COMPILER=g++ \
    -DCMAKE_CXX_FLAGS=--coverage \
    -DCMAKE_EXE_LINKER_FLAGS=--coverage \
    -DDCKP_ENABLE_SANITIZERS=OFF \
    -DDCKP_ENABLE_HARDENING=OFF \
    -DDCKP_WARNINGS_AS_ERRORS=ON
cmake --build cmake-build-audit-coverage --parallel "${JOBS}" --clean-first
run_tests cmake-build-audit-coverage
gcovr --root . --filter 'src/' --exclude 'src/main.cpp' \
    cmake-build-audit-coverage/CMakeFiles/dckp_core.dir \
    --txt cmake-build-audit-coverage/coverage.txt \
    --txt-summary --fail-under-line 80 --fail-under-branch 45

stage "All-instance experiment smoke test"
if ! bash scripts/run_experiments.sh \
    --algo Greedy_MaxProfit --time-limit 1 --runs 1 --base-seed 42 \
    --output "${SMOKE_CSV}" >"${SMOKE_LOG}" 2>&1; then
    cat "${SMOKE_LOG}" >&2
    exit 1
fi
smoke_rows="$(grep -Ec '^[^#[:space:]]' "${SMOKE_CSV}")"
valid_rows="$(grep -c ',true,Greedy_MaxProfit$' "${SMOKE_CSV}")"
unique_seeds="$(sed -n '2,101p' "${SMOKE_CSV}" | cut -d, -f3 | sort -u | wc -l)"
if [[ "${smoke_rows}" -ne 101 || "${valid_rows}" -ne 100 || "${unique_seeds}" -ne 100 ]]; then
    cat "${SMOKE_LOG}" >&2
    echo "Invalid all-instance smoke-test result." >&2
    exit 1
fi
printf '100 instances: valid CSV and 100 unique seeds\n'

stage "ILS iteration log"
./build/dckp_sbpo \
    DCKP-instances/DCKP-instances-set-I-100/I1-I10/10I3 \
    --algo ILS --time-limit 200 --seed 42 --csv --verbose \
    >"${ILS_CSV}" 2>"${ILS_LOG}"
if [[ "$(wc -l <"${ILS_CSV}")" -ne 1 ]] || \
    ! grep -q ',true,ILS$' "${ILS_CSV}" || \
    ! grep -Eq '^ILS iteration=[0-9]+ .*gain_percent=.*work_iterations=.*elapsed_ms=' "${ILS_LOG}" || \
    ! grep -Eq '^ILS end iterations=[0-9]+ improvements=[0-9]+ .*total_gain_percent=.*work_iterations=.*elapsed_ms=' "${ILS_LOG}"; then
    cat "${ILS_LOG}" >&2
    echo "ILS log validation failed." >&2
    exit 1
fi
printf 'ILS per-iteration and summary logs: OK\n'

stage "Audit complete"
printf 'All checks passed without project diagnostics.\n'
printf 'Coverage report: %s/cmake-build-audit-coverage/coverage.txt\n' "${REPO_ROOT}"
