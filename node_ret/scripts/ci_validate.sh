#!/bin/bash
# ci_validate.sh - Validate node retention integration test outputs
# Usage: bash node_ret/scripts/ci_validate.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ERRORS=0

check_file() {
    local file="$1"
    local desc="$2"
    if [ ! -s "$file" ]; then
        echo "FAIL: $desc - output file missing or empty: $file"
        ERRORS=$((ERRORS + 1))
    else
        echo "PASS: $desc - $(wc -c < "$file") bytes"
    fi
}

echo "=== Node Retention CI Validation ==="
echo ""

check_file "$SCRIPT_DIR/get_and_put_output.blif" "Basic Get/Put"
check_file "$SCRIPT_DIR/v3.blif" "Synthesis Pipeline (v3)"
check_file "$SCRIPT_DIR/output.blif" "Full Pipeline (abc.script)"

echo ""
if [ "$ERRORS" -gt 0 ]; then
    echo "FAILED: $ERRORS validation(s) failed"
    exit 1
else
    echo "All validations passed"
fi
