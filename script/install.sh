#! /bin/bash

while read line; do
	for file in $(eval echo $line); do
		output_file=debian/tmp/$(echo $file | perl -pe 's!^locale/(.*)\.(.*)\.mo$!locale/$2/LC_MESSAGES/$1.mo!')

		mkdir -p $(dirname $output_file)
		cp -a $file $(dirname $output_file)
	done
done < install.paths

