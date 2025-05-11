# pages/on_collision.py

from pages.base import AnimAreaPage

class OnCollisionPage(AnimAreaPage):
    def __init__(self, parent):
        super().__init__(
            parent,
            title="On-Collision Settings",
            json_enabled_key="is_collidable",
            json_anim_key="collision_animation",
            json_area_key="collision_area",
            folder_name="collision",
            label_singular="Collidable"
        )
