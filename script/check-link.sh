#! /bin/bash

SCRIPT_DIR=$(dirname "$0")/check-link
SCRIPT_NAME=$1
shift

gcc -o /tmp/a.out "$SCRIPT_DIR/$SCRIPT_NAME" 2>/dev/null
[[ "$?" -eq 0 ]] && exit

for i in "$@"; do
	gcc -o /tmp/a.out "$SCRIPT_DIR/$SCRIPT_NAME" "$i" 2> /dev/null
	[[ "$?" -eq 0 ]] && (echo "$i"; exit 0)
done

