#!/usr/bin/env bash
set -eo pipefail

TARGETS=()
BAZEL_ARGS=()

for arg in "$@"; do
    if [[ "$arg" == -* ]]; then
        BAZEL_ARGS+=("$arg")
    else
        TARGETS+=("$arg")
    fi
done

if [[ ${#TARGETS[@]} -eq 0 ]]; then
    TARGETS=("//...")
fi

buildevents=$(mktemp)
trap 'rm -f "$buildevents"' EXIT

bazel build --config=lint "${BAZEL_ARGS[@]}" "--build_event_json_file=$buildevents" "${TARGETS[@]}"

has_findings=0

if command -v jq >/dev/null 2>&1; then
    while IFS= read -r uri; do
        path="${uri#file://}"
        exit_code_path="${path}.exit_code"
        if [[ -f "$exit_code_path" ]]; then
            if [[ "$(cat "$exit_code_path")" != "0" ]]; then
                cat "$path"
                has_findings=1
            fi
        elif [[ -n "$path" && -s "$path" ]]; then
            cat "$path"
            has_findings=1
        fi
    done < <(jq -r '.namedSetOfFiles.files[]? | select(.name | endswith(".AspectRulesLintClangTidy.out")) | .uri' "$buildevents")
else
    while IFS= read -r report; do
        exit_code_path="${report}.exit_code"
        if [[ -f "$exit_code_path" ]]; then
            if [[ "$(cat "$exit_code_path")" != "0" ]]; then
                cat "$report"
                has_findings=1
            fi
        elif [[ -s "$report" ]]; then
            cat "$report"
            has_findings=1
        fi
    done < <(find out/bin -maxdepth 2 -name "*.AspectRulesLintClangTidy.out" 2>/dev/null)
fi

exit $has_findings
