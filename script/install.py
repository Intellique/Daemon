#! /usr/bin/python3

from glob import glob
from os import getenv, makedirs, walk
from os.path import dirname, isdir, isfile, join
import shutil
import sys

triplet = None
for t in ('DEB_HOST_MULTIARCH', ):
    triplet = getenv(t)
    if triplet is not None:
        break

fd = open('install.paths', 'r')

for line in fd:
    parts = list(filter(len, line.rstrip('\n').split('\t')))
    if len(parts) == 3:
        (src, dest, sub_dest) = parts
    else:
        (src, dest) = parts
        sub_dest = None

    (src_dir, sub_file) = src.split('|')
    src = join(src_dir, sub_file)

    if isdir(src):
        for src_dir, _, src_files in walk(src):
            dest_dir = join(sys.argv[1], dest, src_dir)
            makedirs(dest_dir, exist_ok=True)
            for files in src_files:
                src_file = join(src_dir, files)
                print('copy "%s" to "%s"' % (src_file, dest_dir))
                shutil.copy(src_file, dest_dir, follow_symlinks=False)
    else:
        for src_file in glob(src):
            if len(src_dir) > 0:
                dir_to_add = dirname(src_file[len(src_dir) + 1:])
            else:
                dir_to_add = dirname(src_file)

            if sub_dest is None:
                if triplet is not None and dest.endswith('/lib'):
                    dest_dir = join(sys.argv[1], dest, triplet, dir_to_add)
                else:
                    dest_dir = join(sys.argv[1], dest, dir_to_add)
            else:
                if triplet is not None and dest.endswith('/lib'):
                    dest_dir = join(sys.argv[1], dest, triplet, sub_dest, dir_to_add)
                else:
                    dest_dir = join(sys.argv[1], dest, sub_dest, dir_to_add)

            makedirs(dest_dir, exist_ok=True)
            if isfile(src_file):
                print('copy "%s" to "%s"' % (src_file, dest_dir))
                shutil.copy(src_file, dest_dir, follow_symlinks=False)

fd.close()
