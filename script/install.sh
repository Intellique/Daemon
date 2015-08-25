#! /bin/bash

while read line; do
  for file in $(eval echo $line); do
    mkdir -p debian/tmp/$(dirname $file)
    cp -a $file debian/tmp/$(dirname $file)
  done
done < install.paths

