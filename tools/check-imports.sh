#!/bin/sh
# Verify that binaries can load on Windows XP SP0 (x86).
#
#   check-imports.sh <allowed-imports.txt> <binary>...
#
# For each binary:
#   - PE32 (not PE32+), OS version and subsystem version are exactly 5.01
#   - every imported dll!function appears in the allow list
#
# The allow list holds one "dll!function" per line (dll lower case);
# text after '#' is a comment. Exit status is non-zero on any violation.

set -u

OBJDUMP=${OBJDUMP:-objdump}

if [ $# -lt 2 ]; then
    echo "usage: $0 <allowed-imports.txt> <binary>..." >&2
    exit 2
fi

allow_file=$1
shift

# Normalised allow list in a temp file: "grep -f" with an empty file matches
# nothing, whereas an empty pattern string would match everything.
allowed_tmp=$(mktemp)
trap 'rm -f "$allowed_tmp"' EXIT
sed -e 's/#.*//' -e 's/[[:space:]]//g' "$allow_file" | grep -v '^$' | sort -u > "$allowed_tmp"

status=0

for bin in "$@"; do
    if ! dump=$("$OBJDUMP" -p "$bin"); then
        echo "$bin: objdump failed" >&2
        status=1
        continue
    fi

    # --- PE header ---
    field() { printf '%s\n' "$dump" | awk -v k="$1" '$1 == k { print $2; exit }'; }

    magic=$(field Magic)
    os_major=$(field MajorOSystemVersion)
    os_minor=$(field MinorOSystemVersion)
    ss_major=$(field MajorSubsystemVersion)
    ss_minor=$(field MinorSubsystemVersion)

    if [ "$magic" != "010b" ]; then
        echo "$bin: not PE32 (Magic=$magic)" >&2
        status=1
    fi
    if [ "$os_major.$os_minor" != "5.1" ]; then
        echo "$bin: OS version is $os_major.$os_minor, expected 5.1" >&2
        status=1
    fi
    if [ "$ss_major.$ss_minor" != "5.1" ]; then
        echo "$bin: subsystem version is $ss_major.$ss_minor, expected 5.1" >&2
        status=1
    fi

    # --- imports: "dll!function" (ordinal-only imports become dll!#ordinal) ---
    # binutils 2.47 rows:  <vma>  <ordinal|"<none>">  <hint>  <name>
    imports=$(printf '%s\n' "$dump" | awk '
        /^[[:space:]]*DLL Name:/ { dll = tolower($3); next }
        /^[[:space:]]*$/          { dll = ""; next }
        dll != "" && $1 ~ /^[0-9a-f]+$/ {
            if ($2 == "<none>") print dll "!" $4
            else                print dll "!#" $2
        }
    ' | sort -u)

    if [ -z "$imports" ]; then
        echo "$bin: no imports found (objdump output format changed?)" >&2
        status=1
        continue
    fi

    bad=$(printf '%s\n' "$imports" | grep -vxF -f "$allowed_tmp")
    if [ -n "$bad" ]; then
        echo "$bin: imports not in $allow_file:" >&2
        printf '  %s\n' $bad >&2
        status=1
    fi

    count=$(printf '%s\n' "$imports" | wc -l | tr -d ' ')
    echo "$bin: $count imports checked"
done

if [ $status -eq 0 ]; then
    echo "check-imports: OK"
fi
exit $status
