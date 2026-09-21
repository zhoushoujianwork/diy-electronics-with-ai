#!/bin/sh
set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
main="$here/../firmware/main"
out="${TMPDIR:-/tmp}/sticks3-gps-motobox-tests-$$"
trap 'rm -rf "$out"' EXIT
mkdir -p "$out"

cc -std=c11 -Wall -Wextra -Werror -I"$main" \
  "$here/test_gnss_parser.c" "$main/gnss_parser.c" -lm -o "$out/test_gnss_parser"
cc -std=c11 -Wall -Wextra -Werror -I"$main" \
  "$here/test_telemetry_queue.c" "$main/telemetry_queue.c" -o "$out/test_telemetry_queue"
cc -std=c11 -Wall -Wextra -Werror -I"$main" \
  "$here/test_telemetry_payload.c" "$main/telemetry_payload.c" -o "$out/test_telemetry_payload"

"$out/test_gnss_parser"
"$out/test_telemetry_queue"
"$out/test_telemetry_payload"
