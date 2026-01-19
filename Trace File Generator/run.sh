#!/usr/bin/env bash

# This code is provided solely for the personal and private use of students
# taking the CSC369H course at the University of Toronto. Copying for purposes
# other than this use is expressly prohibited. All forms of distribution of
# this code, including but not limited to public repositories on GitHub,
# GitLab, Bitbucket, or any other online platform, whether as given or with
# any changes, are expressly prohibited.
#
# Author: Louis Ryan Tan
#
# This script generates a multiprocess trace file from an executable that is in
# BUILD_DIR, which defaults to `build` if not specified.
#
set -euo pipefail

# --- Usage check ---
if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <filename> [args...]"
    exit 1
fi

# --- Setup variables ---
FILENAME="$1"
shift
ARGS=("$@")  # Remaining args (if any)

BUILD_DIR=$(readlink -f build)
RUN_DIR="runs/${FILENAME}"
RAW_DIR="${RUN_DIR}/raw"
REF_DIR="${RUN_DIR}/refs"
TRACE_DIR="traces"

# --- Create directories ---
mkdir -p "$RAW_DIR" "$REF_DIR" "$TRACE_DIR"


# --- Generate raw trace ---
./gen_rawtrace.sh "$RAW_DIR" "${BUILD_DIR}/${FILENAME}" "${ARGS[@]}"

# --- Clean up old refs ---
rm -rf "$REF_DIR"/*

# --- Simplify trace ---
./simplify_trace \
    -i "$RAW_DIR" \
    -m "${RUN_DIR}/marker" \
    -o "$REF_DIR" \
    -s 16 \
    -b 4

# --- Schedule into .mref ---
./schedule.py \
    --output "${TRACE_DIR}/${FILENAME}.mref" \
    --refs "$REF_DIR"
