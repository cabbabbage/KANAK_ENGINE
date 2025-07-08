# file: map_renderer.py

import os
import json
import math
import random
from colorsys import hsv_to_rgb

class MapRenderer:
    def __init__(self, rooms_dir, preview_canvas, preview_size, layer_widgets, factor):
        self.rooms_dir = rooms_dir
        self.preview_canvas = preview_canvas
        self.PREVIEW_SIZE = preview_size
        self.layer_widgets = layer_widgets
        self.factor = factor
        self._suspend_save = False

    def calculate_radii(self):
        self._suspend_save = True
        try:
            shapes_meta = []
            largest_sizes = []
            all_rooms = []
            global_room_lookup = {}

            def load_room_shape(name):
                if name in global_room_lookup:
                    return global_room_lookup[name]

                p = os.path.join(self.rooms_dir, f"{name}.json")
                if not os.path.exists(p):
                    return None
                try:
                    with open(p, 'r') as f:
                        info = json.load(f)
                except Exception:
                    return None

                geo = info.get("geometry", "square").lower()
                w, h = info.get("max_width", 0), info.get("max_height", 0)

                shape = {
                    "room_name": name,
                    "type": "circle" if geo == "circle" else "square",
                    "diameter": w if geo == "circle" else None,
                    "width": w if geo != "circle" else None,
                    "height": h if geo != "circle" else None,
                    "min_instances": 0,
                    "max_instances": 0,
                    "required_children": []
                }
                global_room_lookup[name] = shape
                return shape

            # First pass: collect rooms from each layer widget
            for layer in self.layer_widgets:
                for room_widget in layer.rooms:
                    name = room_widget.room_name
                    shape = load_room_shape(name)
                    if not shape:
                        continue

                    shape["min_instances"] = room_widget.min_var.get()
                    shape["max_instances"] = room_widget.max_var.get()
                    shape["required_children"] = getattr(room_widget, "required_children", [])

                    all_rooms.append(shape)

            # Second pass: resolve required children for each shape
            for shape in all_rooms:
                resolved = []
                for child_name in shape.get("required_children", []):
                    child_shape = load_room_shape(child_name)
                    if child_shape:
                        resolved.append(child_shape)
                shape["required_children"] = resolved

            # Build shapes_meta by layer and find max size for radius calculation
            for layer in self.layer_widgets:
                layer_meta = []
                max_span = 0
                for room_widget in layer.rooms:
                    shape = global_room_lookup.get(room_widget.room_name)
                    if not shape:
                        continue
                    if shape["type"] == "circle":
                        span = shape["diameter"]
                    else:
                        span = math.hypot(shape["width"], shape["height"])
                    max_span = max(max_span, span)
                    layer_meta.append(shape)
                largest_sizes.append(max_span)
                shapes_meta.append(layer_meta)

            # Debug print
            for idx, layer in enumerate(shapes_meta):
                for room in layer:
                    print(f"[DEBUG] Layer {idx} Room '{room['room_name']}' required_children: {[c['room_name'] for c in room.get('required_children', [])]}")

            if not largest_sizes or not shapes_meta or all(s == 0 for s in largest_sizes):
                print("No valid room geometry found for radius calculation.")
                return

            radii = []
            prev_r = 0
            for i, size in enumerate(largest_sizes):
                if i == 0:
                    r = 0
                else:
                    r = prev_r + 1.2 * ((size / 2.0) + (largest_sizes[i - 1] / 2.0))
                radii.append(r)
                prev_r = r

            self.layer_radii = radii
            for i, layer in enumerate(self.layer_widgets):
                if i < len(radii):
                    layer.set_radius(radii[i])

            # We need roominfo_lookup for required_children info, assume from widgets:
            roominfo_lookup = {}
            for layer in self.layer_widgets:
                for room_widget in layer.rooms:
                    roominfo_lookup[room_widget.room_name] = room_widget

            self.render_preview(radii, shapes_meta, radii[-1] + (largest_sizes[-1] / 2.0) + 540, global_room_lookup, roominfo_lookup)

        finally:
            self._suspend_save = False


    def room_type_to_color(self, name):
        hue = (hash(name) % 360) / 360.0
        r, g, b = hsv_to_rgb(hue, 0.4, 0.95)
        return '#{:02x}{:02x}{:02x}'.format(int(r*255), int(g*255), int(b*255))

    def render_preview(self, radii, shapes_meta, map_radius_actual, global_room_lookup, roominfo_lookup):
        print("Generating Map Preview:")
        C = self.PREVIEW_SIZE // 2
        self.preview_canvas.delete("all")
        scale = C / map_radius_actual

        self.preview_canvas.create_oval(0, 0, self.PREVIEW_SIZE, self.PREVIEW_SIZE, outline="black", width=3)
        self.preview_canvas.create_text(C, self.PREVIEW_SIZE - 20, text=f"Map radius: {int(map_radius_actual)}", font=("Segoe UI", 12, "bold"))

        # Root layer, single root room
        root_shape = shapes_meta[0][0]
        parents = [{
            "pos": (C, C),
            "layer": 0,
            "shape": root_shape,
            "sector_start": 0.0,
            "sector_size": 2 * math.pi
        }]
        placed_shapes = [parents[0]]
        self._draw_shape(C, C, root_shape, self.room_type_to_color(root_shape["room_name"]), self._ring_color(0), scale)

        for i in range(1, len(radii)):
            ring_px = radii[i] * scale
            col = self._ring_color(i)
            next_meta = shapes_meta[i]
            self.preview_canvas.create_oval(C - ring_px, C - ring_px, C + ring_px, C + ring_px, outline=col, width=2)

            layer_widget = self.layer_widgets[i]
            min_rooms = layer_widget.min_rooms_var.get()
            max_rooms = layer_widget.max_rooms_var.get()
            room_pool = self.get_children_from_layer(next_meta, min_rooms, max_rooms)

            new_parents = []
            evenly_spaced = (i == 1)
            even_slice_size = (2 * math.pi / len(room_pool)) if evenly_spaced and room_pool else None
            global_angle_offset = 0.0

            # Assign rooms evenly to parents
            assignments = room_pool[:]
            parent_counts = {p["pos"]: 0 for p in parents}
            parent_room_map = {p["pos"]: [] for p in parents}

            while assignments:
                # Find parents with least assignments
                min_count = min(parent_counts.values())
                eligible_parents = [p for p in parents if parent_counts[p["pos"]] == min_count]
                for p in eligible_parents:
                    if not assignments:
                        break
                    room = assignments.pop()
                    parent_room_map[p["pos"]].append(room)
                    parent_counts[p["pos"]] += 1

            for parent in parents:
                p_x, p_y = parent["pos"]
                sector_start = parent["sector_start"]
                sector_size = parent["sector_size"]

                parent_name = parent["shape"]["room_name"]
                roominfo = roominfo_lookup.get(parent_name)
                required_children = []
                if roominfo:
                    for name in getattr(roominfo, "required_children", []):
                        shape = global_room_lookup.get(name)
                        if shape:
                            required_children.append(shape)

                assigned_rooms = parent_room_map[parent["pos"]]
                combined_rooms = required_children + assigned_rooms

                print(f"[DEBUG] Parent '{parent_name}' has children: {[r['room_name'] for r in combined_rooms]}")

                if not combined_rooms:
                    continue

                slice_size = sector_size / len(combined_rooms)
                buffer_angle = slice_size * 0.05

                for idx, room in enumerate(combined_rooms):
                    fill = self.room_type_to_color(room["room_name"])
                    child_sector_start = sector_start + idx * slice_size + buffer_angle
                    child_sector_size = slice_size - 2 * buffer_angle

                    if evenly_spaced:
                        angle = global_angle_offset + even_slice_size / 2
                        global_angle_offset += even_slice_size
                        cx = C + ring_px * math.cos(angle)
                        cy = C + ring_px * math.sin(angle)
                    else:
                        placed = False
                        for attempt in range(100):
                            angle = child_sector_start + random.random() * child_sector_size
                            cx = C + ring_px * math.cos(angle)
                            cy = C + ring_px * math.sin(angle)
                            if not self._does_overlap(cx, cy, room, placed_shapes, scale):
                                placed = True
                                break
                        if not placed:
                            print(f"[DEBUG] Failed to place room '{room['room_name']}' on layer {i} due to overlap.")
                            continue

                    self._draw_shape(cx, cy, room, fill, col, scale)
                    self.preview_canvas.create_line(p_x, p_y, cx, cy, fill="black")
                    new_parents.append({
                        "pos": (cx, cy),
                        "layer": i,
                        "shape": room,
                        "sector_start": child_sector_start,
                        "sector_size": child_sector_size
                    })
                    placed_shapes.append({"pos": (cx, cy), "shape": room})

            parents = new_parents

        # Draw Color Legend
        legend_items = {}
        for placed in placed_shapes:
            name = placed["shape"]["room_name"]
            legend_items[name] = self.room_type_to_color(name)

        sorted_items = sorted(legend_items.items())
        key_y = self.PREVIEW_SIZE - 45
        key_x = 20
        box_size = 12
        spacing = 100

        for name, color in sorted_items:
            self.preview_canvas.create_rectangle(key_x, key_y, key_x + box_size, key_y + box_size, fill=color, outline="black")
            self.preview_canvas.create_text(key_x + box_size + 5, key_y + box_size // 2, text=name, anchor="w", font=("Segoe UI", 9))
            key_x += spacing


    def get_children_from_layer(self, next_meta, min_rooms, max_rooms):
        # Calculate total number of rooms to generate
        target = random.randint(min_rooms, max_rooms)

        # Start with minimum required rooms
        pool = []
        expandable = []
        for room in next_meta:
            pool.extend([room] * room["min_instances"])
            extra = room["max_instances"] - room["min_instances"]
            expandable.extend([room] * extra)

        # Add more from expandable pool until target met
        while len(pool) < target and expandable:
            choice = random.choice(expandable)
            pool.append(choice)
            expandable.remove(choice)

        random.shuffle(pool)
        return pool


    def _draw_shape(self, x, y, shape, color_fill, color_outline, scale):
        if shape["type"] == "circle":
            d = shape["diameter"] * scale
            self.preview_canvas.create_oval(x - d / 2, y - d / 2, x + d / 2, y + d / 2, fill=color_fill, outline=color_outline)
        else:
            w = shape["width"] * scale
            h = shape["height"] * scale
            self.preview_canvas.create_rectangle(x - w / 2, y - h / 2, x + w / 2, y + h / 2, fill=color_fill, outline=color_outline)

    def _does_overlap(self, x, y, shape, placed, scale):
        buffer_radius = 300 * scale
        r1 = shape["diameter"] * scale / 2 if shape["type"] == "circle" else math.hypot(shape["width"], shape["height"]) * scale / 2
        for other in placed:
            ox, oy = other["pos"]
            oshape = other["shape"]
            r2 = oshape["diameter"] * scale / 2 if oshape["type"] == "circle" else math.hypot(oshape["width"], oshape["height"]) * scale / 2
            if math.hypot(x - ox, y - oy) < r1 + r2 + buffer_radius:
                return True
        return False

    def _ring_color(self, lvl):
        hue = (lvl * 0.13) % 1.0
        r, g, b = hsv_to_rgb(hue, 0.6, 1.0)
        return f'#{int(r*255):02x}{int(g*255):02x}{int(b*255):02x}'
