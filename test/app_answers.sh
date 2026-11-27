#!/bin/sh
# the app's answers (FLX_ANSWER, a line each) to the commands in test/app_answers.cmds, one per line
# usage: sh test/app_answers.sh build-pc/fluxion-app > out, make test diffs it with test/app_answers.expected
B=$(realpath "$1")
CMDS=$(realpath "$(dirname "$0")/app_answers.cmds")
D=$(mktemp -d) || exit 1
trap 'rm -rf "$D"' EXIT
cd "$D" || exit 1
while IFS= read -r cmd; do
    case $cmd in ''|'#'*) continue ;; esac
    echo "== $cmd"
    FLX_ANSWER=1 "$B" --png o.png --scale 1 "clip:$cmd" "on+v" enter 2>&1 | grep -aE '^(answer|message)' | tr -c '[:print:]\n' '?'    # the integral sign and friends are bytes, not text
done < "$CMDS"
