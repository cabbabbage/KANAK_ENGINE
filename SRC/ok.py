#!/usr/bin/env python3
import os
import shutil

def delete_cache_dirs(root_dir='.'):
    """
    Recursively walk through root_dir and delete any directory named 'cache'.
    """
    for dirpath, dirnames, filenames in os.walk(root_dir, topdown=True):
        # Make a copy of dirnames to iterate so we can modify dirnames in-place
        for d in list(dirnames):
            if d == 'cache':
                full_path = os.path.join(dirpath, d)
                print(f"Deleting cache directory: {full_path}")
                shutil.rmtree(full_path)
                # Remove from dirnames so os.walk won't recurse into it
                dirnames.remove(d)

if __name__ == "__main__":
    delete_cache_dirs()
