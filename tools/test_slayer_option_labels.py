#!/usr/bin/env python3
"""Slayer rows: each spinner must set the field its on-screen label names.

Both the PC XML and the Xbox ui.map label the widget named kill_in_order
KILL PENALTY (slayer_labels index 1) and the widget named kill_penalty
KILL IN ORDER (index 2), so the option table keys are crossed on purpose.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TABLE = (ROOT / "port/linux/game/menu_functions.c").read_text()
LABELS = ["DEATH BONUS", "KILL PENALTY", "KILL IN ORDER", "KILLS TO WIN", "TEAM PLAY"]
STRINGS = (ROOT / "tools/menu_sources/strings.xml").read_text()


def field(key):
    m = re.search(r'"%s",\s*_option_\w+,\s*VARIANT_FIELD\(game_engine_variant\.slayer\.(\w+)\)' % key, TABLE)
    assert m, key
    return m.group(1)


class SlayerOptionLabels(unittest.TestCase):
    def test_labels_order(self):
        block = re.search(r'slayer_edit/slayer_labels">(.*?)</strings>', STRINGS, re.S).group(1)
        self.assertEqual(re.findall(r'text="([^":]+):?"', block), LABELS)

    def test_kill_penalty_label_row_sets_penalty(self):
        # row kill_in_order shows slayer_labels[1], KILL PENALTY
        self.assertEqual(field("kill_in_order_spinner"), "no_kill_penalty")

    def test_kill_in_order_label_row_sets_order(self):
        # row kill_penalty shows slayer_labels[2], KILL IN ORDER
        self.assertEqual(field("kill_penalty_spinner"), "kill_in_order")


if __name__ == "__main__":
    unittest.main()
