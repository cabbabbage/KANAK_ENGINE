#!/usr/bin/env python3
import os
import json

def process_info_json(path):
    with open(path, 'r+', encoding='utf-8') as f:
        try:
            data = json.load(f)
        except json.JSONDecodeError:
            print(f"Skipping invalid JSON: {path}")
            return

        tags = data.get("tags")
        if isinstance(tags, list):
            if "any" not in tags:
                tags.append("any")
                # rewrite the file in-place
                f.seek(0)
                json.dump(data, f, indent=4)
                f.truncate()
                print(f"Updated tags in {path}")
            else:
                print(f"'any' already present in {path}")
        else:
            print(f"No \"tags\" list in {path}")

def main():
    for root, _, files in os.walk('.'):
        for name in files:
            if name == "info.json":
                process_info_json(os.path.join(root, name))

if __name__ == "__main__":
    main()
