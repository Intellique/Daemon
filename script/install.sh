#! /bin/bash

while read src dest; do
	for file in $(eval echo $src); do
		fake_file=$(echo -n $file | perl -pe 's!^locale/(.*)\.(.*)\.mo$!locale/$2/LC_MESSAGES/$1.mo!')
		input_dir=$(dirname $fake_file)

		if [ -z "$dest" ]; then
			output_dir=$1/${input_dir%.}
		else
			output_dir=$1/$dest
		fi

		if [ ! -d "$output_dir" ]; then
			mkdir -vp $output_dir
		fi

		cp -av "$file" "$output_dir"

		printf "dest: %s, file: %s\n" "$output_dir" "$file"
	done
done < install.paths

