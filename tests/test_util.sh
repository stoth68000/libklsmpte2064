#!/bin/sh

set -eu

total=0
passed=0
failed=0

check() {
	name="$1"
	shift
	total=$((total + 1))
	printf 'CHECK %d: %s ... ' "$total" "$name"
	if "$@" > "$tmpdir/check-output.txt" 2>&1; then
		passed=$((passed + 1))
		printf 'PASS\n'
	else
		failed=$((failed + 1))
		printf 'FAIL\n'
		cat "$tmpdir/check-output.txt"
	fi
}

tmpdir="${TMPDIR:-/tmp}/klsmpte2064-test.$$"
video="$tmpdir/video.yuv"
audio="$tmpdir/audio.s32le"

cleanup() {
	rm -rf "$tmpdir"
}
trap cleanup EXIT HUP INT TERM

mkdir -p "$tmpdir"

check "create raw YUV420P fixture" \
	dd if=/dev/zero of="$video" bs=1382400 count=3
check "create raw S32LE audio fixture" \
	dd if=/dev/zero of="$audio" bs=6400 count=3

check "run klsmpte2064_util" \
	../tools/klsmpte2064_util -i "$video" -I "$audio" -W 1280 -H 720

../tools/klsmpte2064_util -i "$video" -I "$audio" -W 1280 -H 720 > "$tmpdir/output.txt"

check "utility emits fingerprint sections" \
	grep "section" "$tmpdir/output.txt"
check "utility exits cleanly" \
	grep "Shutdown" "$tmpdir/output.txt"

printf 'Summary: total=%d passed=%d failed=%d\n' "$total" "$passed" "$failed"

if test "$failed" -ne 0; then
	exit 1
fi
