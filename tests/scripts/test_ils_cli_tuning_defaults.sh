#!/usr/bin/env bash

set -euo pipefail

binary="$1"
instance="$2"

default_output="$("${binary}" "${instance}" --algo ILS --time-limit 10 --seed 42 --csv)"
explicit_output="$("${binary}" "${instance}" --algo ILS --time-limit 10 --seed 42 --csv \
    --ils-perturbation-strength 4 --vnd-add 1 --vnd-swap-1-1 1 \
    --vnd-swap-2-1 1 --vnd-swap-1-2 1)"

IFS=, read -r default_instance default_seed default_profit _ _ _ _ _ default_valid default_algorithm <<<"${default_output}"
IFS=, read -r explicit_instance explicit_seed explicit_profit _ _ _ _ _ explicit_valid explicit_algorithm <<<"${explicit_output}"

[[ "${default_instance}" == "1I1" && "${explicit_instance}" == "1I1" ]]
[[ "${default_seed}" == "42" && "${explicit_seed}" == "42" ]]
[[ "${default_profit}" =~ ^[0-9]+$ && "${explicit_profit}" =~ ^[0-9]+$ ]]
[[ "${default_valid}" == "true" && "${explicit_valid}" == "true" ]]
[[ "${default_algorithm}" == "ILS" && "${explicit_algorithm}" == "ILS" ]]

help_output="$("${binary}" --help)"
grep -q -- '--ils-perturbation-strength: integer in \[1, 8\] (default: 4)' <<<"${help_output}"
grep -q -- '--vnd-add: 0 | 1 (default: 1)' <<<"${help_output}"
