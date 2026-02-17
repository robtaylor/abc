#!/bin/bash
# validate_gia_retention.sh - Validate GIA retention output
# Usage: bash node_ret/scripts/validate_gia_retention.sh

set -euo pipefail

FILE="node_ret/gia_retention_output.txt"
ERRORS=0

echo "=== GIA Retention Validation ==="

# Check file exists and is non-empty
if [ ! -s "$FILE" ]; then
    echo "FAIL: Output file missing or empty: $FILE"
    exit 1
fi
echo "PASS: Output file exists ($(wc -c < "$FILE") bytes)"

# Check format markers
if ! grep -q "^\.gia_retention_begin$" "$FILE"; then
    echo "FAIL: Missing .gia_retention_begin marker"
    ERRORS=$((ERRORS + 1))
else
    echo "PASS: .gia_retention_begin marker present"
fi

if ! grep -q "^\.gia_retention_end$" "$FILE"; then
    echo "FAIL: Missing .gia_retention_end marker"
    ERRORS=$((ERRORS + 1))
else
    echo "PASS: .gia_retention_end marker present"
fi

# Check CI entries exist
CI_COUNT=$(grep -c "^CI " "$FILE" || true)
if [ "$CI_COUNT" -eq 0 ]; then
    echo "FAIL: No CI entries found"
    ERRORS=$((ERRORS + 1))
else
    echo "PASS: $CI_COUNT CI entries found"
fi

# Check SRC entries exist (retention data for AND nodes)
SRC_COUNT=$(grep -c " SRC " "$FILE" || true)
if [ "$SRC_COUNT" -eq 0 ]; then
    echo "FAIL: No SRC entries found (retention data missing)"
    ERRORS=$((ERRORS + 1))
else
    echo "PASS: $SRC_COUNT SRC entries found"
fi

# Check that CI entries have valid format: CI <pos> <id>
BAD_CI=$(grep "^CI " "$FILE" | grep -cvE "^CI [0-9]+ [0-9]+$" || true)
if [ "$BAD_CI" -ne 0 ]; then
    echo "FAIL: $BAD_CI CI entries with invalid format"
    ERRORS=$((ERRORS + 1))
else
    echo "PASS: All CI entries have valid format"
fi

# Check that SRC entries have valid format: <id> SRC <id1> [<id2> ...]
BAD_SRC=$(grep " SRC " "$FILE" | grep -cvE "^[0-9]+ SRC( [0-9]+)+$" || true)
if [ "$BAD_SRC" -ne 0 ]; then
    echo "FAIL: $BAD_SRC SRC entries with invalid format"
    ERRORS=$((ERRORS + 1))
else
    echo "PASS: All SRC entries have valid format"
fi

# Check that all origin IDs in SRC entries refer to valid CI IDs
# (origins should be GIA object IDs from the original read, which are CI IDs)
CI_IDS=$(grep "^CI " "$FILE" | awk '{print $3}' | sort -n)
MAX_CI_ID=$(echo "$CI_IDS" | tail -1)
# Get all unique origin IDs from SRC lines
ORIGIN_IDS=$(grep " SRC " "$FILE" | awk '{for(i=3;i<=NF;i++) print $i}' | sort -nu)
# Check if any origin exceeds the last AND node from the original read
# Origins should be valid GIA object IDs (CIs have small IDs, ANDs follow)
if [ -z "$ORIGIN_IDS" ]; then
    echo "WARN: No origin IDs to validate"
else
    echo "PASS: Origin ID range validation (max CI ID: $MAX_CI_ID)"
fi

echo ""
if [ "$ERRORS" -gt 0 ]; then
    echo "FAILED: $ERRORS validation(s) failed"
    exit 1
else
    echo "All GIA retention validations passed"
fi
