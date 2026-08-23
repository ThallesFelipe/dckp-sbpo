#!/usr/bin/env bash

set -euo pipefail

repo_root="$1"

if ! command -v Rscript >/dev/null 2>&1 || \
   ! Rscript -e 'quit(status = if (requireNamespace("irace", quietly = TRUE)) 0 else 77)' >/dev/null 2>&1; then
    echo "irace R package is not available; skipping scenario integration test."
    exit 77
fi

"${repo_root}/scripts/irace/run.sh" --check-fast
