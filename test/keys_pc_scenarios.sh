#!/bin/sh
# build-pc/keys-pc-test through a list of made up keyboard scenarios, prints what came out
# usage: sh test/keys_pc_scenarios.sh build-pc/keys-pc-test > out, make test diffs it with test/keys_pc.expected
B=$(realpath "$1")
D=$(mktemp -d) || exit 1
trap 'rm -rf "$D"' EXIT
cd "$D" || exit 1
run() { echo "== $*"; "$B" "$@" quit; }

run k:lshift u:lshift
run k:lalt u:lalt
run k:lctrl u:lctrl
run k:lshift t:a u:lshift
run mods:s k:a t:A u:a
run mods:s k:9 t:'(' u:9
run mods:s k:= t:+ u:=
run k:lshift mods:s k:left u:left u:lshift
run mods:a k:x u:x
run mods:a k:= u:=
run mods:a k:enter u:enter
run mods:c k:c u:c k:x k:v k:z k:a
run mods:c k:q t:q
run mods:c k:enter
run k:kp5 t:5 k:kp5 u:kp5 k:kpplus t:+ k:kpenter k:kpdot k:kpminus k:kpmul k:kpdiv
run mods:a k:kp7
run k:bs k:del k:enter k:left k:right k:up k:down k:tab k:home k:end
run k:f1 k:f2 k:f3 k:f4 k:f5
run k:esc
run mods:c k:esc
run k:7 t:7 k:a t:a k:= t:= t:'~' t:'<' t:'>' t:'!' t:'%' t:'ö'
run t:x t:y t:'(' t:')' t:, t:. t:'^' t:'*' t:/ t:-
run m:down:10,20 m:move:30,40 m:move:31,41 m:drag:50,60 m:drag:51,61 m:move:5,5 m:leave
run mods:s m:down:10,20 m:drag:30,30
run wheel:1 wheel:-1 wheel:0
run focus k:lshift focus u:lshift
run k:lshift k:lshift u:lshift
run k:lctrl mods:c k:c u:c u:lctrl
run k:lalt mods:a t:x u:lalt
run k:lshift t:A u:lshift
run mods:a k:1 k:2 k:3 k:4 k:5 k:6
run k:caps u:caps k:lshift u:lshift
run mods:a k:lalt mods:a k:enter u:enter u:lalt
run k:f11 u:f11
