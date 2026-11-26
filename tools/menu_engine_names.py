"""The names the game's own tables give its functions, read from its C source: shared by the menu generator
(`ce_menus.py`) and the menu-builder probe (`test_menu_builder_loads.py`), standard library only."""
from pathlib import Path
import re


def engine_names(source: Path, start: str, count: int) -> list:
    """the names a table of the game's functions gives them, in order"""
    text = source.read_text(encoding="latin-1")
    text = text[text.index(start):]
    return re.findall(r'"([^"\\]*)"', text[:text.index("}")])[:count]
