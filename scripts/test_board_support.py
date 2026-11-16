"""Exercise profile discovery/selection without changing the repository's boards."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import board_support


class BoardProfiles(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.boards = self.root / "Firmware/Boards"
        self.boards.mkdir(parents=True)
        self.patch = patch.object(board_support, "BOARDS", self.boards)
        self.patch.start()
        self.addCleanup(self.patch.stop)
        self.manifest = json.loads((board_support.ROOT /
            "Firmware/Boards/v3.6-56V/board.json").read_text(encoding="utf-8"))
        for name in ("v3.6-56V", "new-board"):
            directory = self.boards / name
            directory.mkdir()
            (directory / "board.json").write_text(json.dumps(self.manifest), encoding="utf-8")
            (directory / "board.cmake").write_text("# Profile fixture\n", encoding="utf-8")
        (self.root / "cmake").mkdir()
        shutil.copyfile(board_support.ROOT / "cmake/board-version.cmake",
                        self.root / "cmake/board-version.cmake")
        self.runner = self.root / "select.cmake"
        self.runner.write_text('cmake_minimum_required(VERSION 3.20)\n'
            'include("${CMAKE_CURRENT_LIST_DIR}/cmake/board-version.cmake")\n'
            'message(STATUS "Selected=${ODRIVE_BOARD_VERSION};MCU=${odrive_mcu}")\n', encoding="utf-8")

    def select(self, name):
        return subprocess.run(["cmake", f"-DODRIVE_BOARD_VERSION={name}", "-P", str(self.runner)],
                              capture_output=True, text=True, encoding="utf-8", errors="replace")

    def test_new_profile_is_discovered_by_python_and_cmake(self):
        self.assertEqual(board_support.available_boards(), ["new-board", "v3.6-56V"])
        directory, manifest, project = board_support.load_board("new-board")
        self.assertEqual(project, directory / manifest["cube_project"])
        result = self.select("new-board")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Selected=new-board;MCU=STM32F405xx", result.stdout)

    def test_unknown_and_incomplete_profiles_fail_before_build(self):
        (self.boards / "new-board/board.cmake").unlink()
        for name in ("new-board", "missing", "../v3.6-56V"):
            with self.assertRaises(ValueError):
                board_support.load_board(name)
            result = self.select(name)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Unknown board", result.stderr)

    def test_project_cannot_escape_profile(self):
        self.manifest["cube_project"] = "../../outside.ioc"
        (self.boards / "new-board/board.json").write_text(json.dumps(self.manifest), encoding="utf-8")
        with self.assertRaises(ValueError):
            board_support.load_board("new-board")


if __name__ == "__main__":
    unittest.main()
