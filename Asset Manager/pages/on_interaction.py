# pages/on_interaction.py

from pages.base import AnimAreaPage

class OnInteractionPage(AnimAreaPage):
    def __init__(self, parent):
        super().__init__(
            parent,
            title="On-Interaction Settings",
            json_enabled_key="is_interactable",
            json_anim_key="interaction_animation",
            json_area_key="interaction_area",
            folder_name="interaction",
            label_singular="Interactable"
        )
