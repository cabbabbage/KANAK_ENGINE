import os
import json
import re

def delete_child_json_and_clean_info(root_dir):
    child_asset_pattern = re.compile(r'^child_assets_\d+\.json$')
    child_area_pattern = re.compile(r'^child_area_\d+\.json$')

    for dirpath, dirnames, filenames in os.walk(root_dir):
        # Step 1: Delete matching JSON files
        for filename in filenames:
            if child_asset_pattern.match(filename) or child_area_pattern.match(filename):
                full_path = os.path.join(dirpath, filename)
                try:
                    os.remove(full_path)
                    print(f"[Deleted] {full_path}")
                except Exception as e:
                    print(f"[Error] Failed to delete {full_path}: {e}")

        # Step 2: Clean "child_assets" from info.json
        if "info.json" in filenames:
            info_path = os.path.join(dirpath, "info.json")
            try:
                with open(info_path, 'r') as f:
                    data = json.load(f)
                if "child_assets" in data:
                    del data["child_assets"]
                    with open(info_path, 'w') as f:
                        json.dump(data, f, indent=2)
                    print(f"[Cleaned] Removed 'child_assets' from {info_path}")
            except Exception as e:
                print(f"[Error] Failed to process {info_path}: {e}")

if __name__ == "__main__":
    start_path = r"C:\Users\cal_m\OneDrive\Documents\GitHub\tarot_game\SRC"
    delete_child_json_and_clean_info(start_path)
