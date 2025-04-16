#!user/bin/python
# -*- coding: utf-8 -*-
import sys, os, shutil

from_dir = sys.argv[1]
to_dir = sys.argv[2]


os.makedirs(to_dir, exist_ok=True)
shutil.copytree(from_dir, to_dir, dirs_exist_ok=True)

