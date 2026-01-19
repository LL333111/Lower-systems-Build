#!/bin/env bash

# This code is provided solely for the personal and private use of students
# taking the CSC369H course at the University of Toronto. Copying for purposes
# other than this use is expressly prohibited. All forms of distribution of
# this code, including but not limited to public repositories on GitHub,
# GitLab, Bitbucket, or any other online platform, whether as given or with
# any changes, are expressly prohibited.
#
# Author: Louis Ryan Tan

set -euo pipefail

# ensure at least output dir and executable are provided
if [[ $# -lt 2 ]]; then
    echo "Usage: $0 <output_dir> <executable_path> [args...]"
    exit 1
fi

outdir=$1
exe_path=$2
shift 2
exe_args=("$@")

# Create output directory if it doesn't exist
if [[ ! -d "$outdir" ]]; then
    mkdir -p "$outdir"
fi

# Safely clear contents of output directory
# Only delete if directory is non-empty AND not root or empty path
if [[ -n "$(ls -A "$outdir" 2>/dev/null || true)" ]]; then
    rm -rf "${outdir:?}/"*
fi

# Disable ASLR + run valgrind lackey
# Detect architecture for setarch
arch=$(uname -m)

setarch "$arch" -R valgrind \
    --tool=lackey \
    --trace-mem=yes \
    --trace-children=yes \
    --log-file="${outdir}/%p.log" \
    "$exe_path" "${exe_args[@]}"
