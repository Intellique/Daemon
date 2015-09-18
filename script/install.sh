#! /bin/bash

while read src dest strip; do
	output_dir=$1
	if [ -n "$dest" ]; then
		output_dir=$output_dir/$dest
	fi

	if [ ! -d "$output_dir" ]; then
		mkdir -vp "$output_dir"
	fi

	if [ -d "$src" ]; then
		cp -av "$src" "$output_dir"
	else
		for file in $(eval echo $src); do
			if [ -n "$strip" ]; then
				src_dir=$(dirname $file)

				new_dir="$output_dir"${src_dir:${#strip}}
				if [ ! -d "$new_dir" ]; then
					mkdir -vp "$new_dir"
				fi

				cp -av "$file" "$new_dir"
			else
				cp -av "$file" "$output_dir"
			fi
		done
	fi
done < install.paths

