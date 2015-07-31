#! /bin/bash

SCRIPT_DIR=$(dirname "$0")/check-link

gcc -o /tmp/a.out "$SCRIPT_DIR/$1" 2>/dev/null
[[ "$?" -eq 0 ]] && exit

gcc -o /tmp/a.out "$SCRIPT_DIR/$1" "$2" 2>/dev/null
[[ "$?" -eq 0 ]] && echo "$2"
