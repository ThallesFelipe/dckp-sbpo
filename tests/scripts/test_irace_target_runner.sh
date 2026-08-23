#!/usr/bin/env bash

set -euo pipefail

repo_root="$1"
instance="$2"
irace_dir="${repo_root}/scripts/irace"

train_count="$(awk 'NF { ++n } END { print n + 0 }' "${irace_dir}/train-instances.txt")"
test_count="$(awk 'NF { ++n } END { print n + 0 }' "${irace_dir}/test-instances.txt")"
[[ "${train_count}" -eq 80 ]]
[[ "${test_count}" -eq 20 ]]

if comm -12 <(LC_ALL=C sort "${irace_dir}/train-instances.txt") \
    <(LC_ALL=C sort "${irace_dir}/test-instances.txt") | grep -q .; then
    echo "Training and test instance lists overlap." >&2
    exit 1
fi

cost="$(DCKP_IRACE_TIME_LIMIT_MS=100 DCKP_IRACE_TIMEOUT_SECONDS=5 \
    "${irace_dir}/target-runner.sh" 1 1 42 "${instance}" \
    --ils-perturbation-strength 4 --vnd-add 1 --vnd-swap-1-1 1 \
    --vnd-swap-2-1 1 --vnd-swap-1-2 1)"
[[ "${cost}" =~ ^-[0-9]+$ ]]

grep -q '^\[forbidden\]$' "${irace_dir}/parameters.txt"
if grep -q '^forbiddenFile' "${irace_dir}/scenario.txt"; then
    echo "Removed irace scenario option forbiddenFile is still present." >&2
    exit 1
fi
grep -q '^blockSize = 20$' "${irace_dir}/scenario.txt"

for variant in 1 2 3 4; do
    start=$(((variant - 1) * 20 + 1))
    end=$((variant * 20))
    count="$(sed -n "${start},${end}p" "${irace_dir}/train-instances.txt" \
        | grep -Ec "/[0-9]+I${variant}(\\.txt)?$")"
    [[ "${count}" -eq 20 ]]
done

while IFS= read -r relative_instance; do
    [[ -f "${irace_dir}/${relative_instance}" ]]
done < "${irace_dir}/train-instances.txt"
while IFS= read -r relative_instance; do
    [[ -f "${irace_dir}/${relative_instance}" ]]
done < "${irace_dir}/test-instances.txt"
