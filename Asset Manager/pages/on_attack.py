# pages/on_attack.py

from pages.base import AnimAreaPage

class OnAttackPage(AnimAreaPage):
    def __init__(self, parent):
        super().__init__(
            parent,
            title="On-Attack Settings",
            json_enabled_key="is_attackable",
            json_anim_key="attack_animation",
            json_area_key="attack_area",
            folder_name="attack",
            label_singular="Attackable"
        )
