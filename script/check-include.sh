#! /bin/bash

SCRIPT_DIR=$(dirname "$0")/check-include
SCRIPT_NAME=$1
shift

gcc -c -o /tmp/a.o "$SCRIPT_DIR/$SCRIPT_NAME" 2> /dev/null
[[ "$?" -eq 0 ]] && exit

for i in "$@"; do
	gcc -c -o /tmp/a.o "$SCRIPT_DIR/$SCRIPT_NAME" $i 2> /dev/null
	[[ "$?" -eq 0 ]] && (echo "$i"; exit 0)
done

