#!/usr/bin/env python3
# setup.py

import subprocess
import sys

def main():
    try:
        subprocess.run([sys.executable, "-m", "pip", "install", "--upgrade", "pip"], check=True)
        subprocess.run([sys.executable, "-m", "pip", "install", "-r", "requirements.txt"], check=True)
        print("\n All dependencies installed successfully.")
    except subprocess.CalledProcessError as e:
        print(f"\n Installation failed: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
