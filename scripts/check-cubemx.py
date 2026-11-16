"""Validate a board profile and its regenerated CubeMX configuration."""
import argparse
import runpy
from board_support import available_boards, load_board


def check_board(name):
    board, manifest, project = load_board(name)
    expected = {"MxCube.Version": manifest["cube_version"],
                "ProjectManager.FirmwarePackage": manifest["firmware_package"],
                "Mcu.CPN": manifest["part"], "Mcu.Family": manifest["family"],
                "ProjectManager.KeepUserCode": "true"}
    ioc = dict(line.split("=", 1) for line in project.read_text(encoding="utf-8").splitlines()
               if "=" in line and not line.startswith("#"))
    for key, value in expected.items():
        if ioc.get(key) != value:
            raise ValueError(f"{project}: {key} expected {value!r}, got {ioc.get(key)!r}")
    for path in (board / "board.cmake", board / "verify.py", board / manifest["linker_script"],
                 project.parent / manifest["startup"]):
        if not path.is_file():
            raise ValueError(f"Missing board artifact: {path}")
    runpy.run_path(str(board / "verify.py"), run_name="__main__")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--board", default="v3.6-56V")
    parser.add_argument("--all", action="store_true", help="Check every implemented board profile")
    args = parser.parse_args()
    for board_name in available_boards() if args.all else [args.board]:
        check_board(board_name)
