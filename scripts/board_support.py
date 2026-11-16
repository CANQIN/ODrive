"""Shared discovery for board generation and validation commands."""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BOARDS = ROOT / "Firmware/Boards"


def available_boards():
    return sorted(p.parent.name for p in BOARDS.glob("*/board.json")
                  if (p.parent / "board.cmake").is_file())


def load_board(name):
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", name) or name not in available_boards():
        raise ValueError(f"Unknown board {name!r}; available: {', '.join(available_boards())}")
    directory = BOARDS / name
    manifest = json.loads((directory / "board.json").read_text(encoding="utf-8"))
    project = board_path(directory, manifest["cube_project"])
    return directory, manifest, project


def board_path(directory, relative):
    path = (directory / relative).resolve()
    if directory.resolve() not in path.parents:
        raise ValueError("Generated artifacts must stay inside their board/project directory")
    return path
