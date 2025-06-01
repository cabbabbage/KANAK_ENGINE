#!/usr/bin/env python3
import os
import json
import random

# Path to the master map.json
MAP_JSON_PATH = r"C:\Users\cal_m\OneDrive\Documents\GitHub\tarot_game\SRC\Map.json"
# Directories to write individual JSONs under the engine folder
ROOMS_DIR = os.path.join("engine", "rooms")
TRAILS_DIR = os.path.join("engine", "trails")

def main():
    # Load the master map.json
    with open(MAP_JSON_PATH, "r") as f:
        config = json.load(f)

    # Extract the room-count bounds
    min_rooms = config.get("min_rooms", 1)
    max_rooms = config.get("max_rooms", min_rooms)
    # Extract the master asset definitions, excluding any named "Davey"
    master_assets = [a for a in config.get("assets", []) if a.get("name") != "Davey"]

    if not master_assets:
        raise RuntimeError("No valid 'assets' entries found in map.json.")

    # Determine how many room files to create
    room_count = random.randint(min_rooms, max_rooms)

    # Ensure the engine/rooms directory exists
    os.makedirs(ROOMS_DIR, exist_ok=True)

    for room_id in range(room_count):
        room_assets = []
        # We will put exactly 10 entries in each room's "assets" array
        for _ in range(10):
            # Choose one of the master asset entries at random
            base = random.choice(master_assets)
            name = base["name"]
            is_active = base["is_active"]

            orig_min = base.get("min_number", 0)
            orig_max = base.get("max_number", orig_min)

            new_min = random.randint(orig_min, orig_max)
            new_max = random.randint(new_min, orig_max)

            room_assets.append({
                "name": name,
                "min_number": new_min,
                "max_number": new_max,
                "is_active": is_active
            })

        room_data = {"assets": room_assets}
        room_file = os.path.join(ROOMS_DIR, f"room_{room_id}.json")
        with open(room_file, "w") as rf:
            json.dump(room_data, rf, indent=2)

    print(f"Generated {room_count} room JSONs in '{ROOMS_DIR}/'")

    # Now create 2 trail JSONs with grass and rocks (no "Davey")
    os.makedirs(TRAILS_DIR, exist_ok=True)

    # Filter grass asset names from master_assets
    grass_names = [a["name"] for a in master_assets if "Grass" in a["name"]]
    if not grass_names:
        grass_names = ["Grass_1_FPS_2"]

    for trail_id in range(2):
        grass_name = random.choice(grass_names)
        grass_min = random.randint(50, 150)
        grass_max = random.randint(grass_min, 200)
        grass_entry = {
            "name": grass_name,
            "min_number": grass_min,
            "max_number": grass_max,
            "is_active": True
        }

        rock_min = random.randint(20, 80)
        rock_max = random.randint(rock_min, 120)
        rock_entry = {
            "name": "Rock",
            "min_number": rock_min,
            "max_number": rock_max,
            "is_active": True
        }

        trail_assets = [grass_entry, rock_entry]
        trail_data = {"assets": trail_assets}
        trail_file = os.path.join(TRAILS_DIR, f"trail_{trail_id}.json")
        with open(trail_file, "w") as tf:
            json.dump(trail_data, tf, indent=2)

    print(f"Generated 2 trail JSONs in '{TRAILS_DIR}/'")

if __name__ == "__main__":
    main()
