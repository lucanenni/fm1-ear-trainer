#!/bin/sh
# Builds and runs trainer/'s host-only unit tests (no hardware, no Felucca).
set -e
cd "$(dirname "$0")"
CC="${CC:-cc}"
OUT="/tmp/fm1_trainer_tests.$$"
trap 'rm -f "$OUT"' EXIT
"$CC" -std=c99 -Wall -Wextra -Werror -g -o "$OUT" \
    test_main.c ../trainer_rng.c ../trainer_interval.c ../trainer_chord.c ../trainer_degree.c \
    ../trainer_question.c ../trainer_session.c ../trainer_stats.c
"$OUT"
