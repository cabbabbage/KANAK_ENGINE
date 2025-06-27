import os
import shutil

# Find and copy 0.png 100 times with no zero-padding
src = "0.png"
if not os.path.isfile(src):
    raise FileNotFoundError("You absolute goblin — there's no 0.png in this folder.")

for i in range(100):
    dst = f"{i}.png"
    shutil.copyfile(src, dst)

print("✅ Duplicated 0.png into 100 frames (0.png to 99.png)")
