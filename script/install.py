#! /usr/bin/python3

from glob import glob
from os import getenv, makedirs, walk
from os.path import dirname, isdir, join
import shutil
import sys

triplet = None
for t in ('DEB_HOST_MULTIARCH', ):
    triplet = getenv(t)
    if triplet is not None:
        break

fd = open('install.paths', 'r')

for line in fd:
    (src, dest, *has_triplet) = list(filter(len, line.rstrip('\n').split('\t')))

    if isdir(src):
        for src_dir, _, src_files in walk(src):
            dest_dir = join(sys.argv[1], dest, src_dir)
            makedirs(dest_dir, exist_ok=True)
            for files in src_files:
                shutil.copy(join(src_dir, files), dest_dir, follow_symlinks=False)
    else:
        for src_file in glob(src):
            if len(has_triplet) > 0 and triplet is not None:
                dest_dir = join(sys.argv[1], dest, dirname(src_file), triplet)
            else:
                dest_dir = join(sys.argv[1], dest, dirname(src_file))

            makedirs(dest_dir, exist_ok=True)
            shutil.copy(src_file, dest_dir, follow_symlinks=False)

fd.close()
