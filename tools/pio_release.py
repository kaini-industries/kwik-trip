"""PlatformIO package target; uses the same images/settings as the USB uploader."""
Import("env")
import sys
from pathlib import Path

root = Path(env.subst("$PROJECT_DIR"))
sys.path.insert(0, str(root / "tools"))
from cardputer_release import package, source_inputs

inputs = source_inputs(root)


def build_package(source, target, env):
    if source_inputs(root) != inputs:
        raise ValueError("Build inputs changed during compilation; rebuild before packaging")
    if (root / "config/platformio/local.ini").exists():
        raise ValueError("Remove local.ini overrides before packaging distribution firmware")
    images = [(int(str(offset), 0), Path(env.subst(str(path))))
              for offset, path in env.get("FLASH_EXTRA_IMAGES", [])]
    images.append((int(env.subst("$ESP32_APP_OFFSET"), 0),
                   Path(env.subst("$BUILD_DIR/${PROGNAME}.bin"))))
    package(root, Path(env.subst("$BUILD_DIR")) / "release", images, {
        "chip": env.BoardConfig().get("build.mcu"),
        "board": env.BoardConfig().id,
        "flash_size": env.BoardConfig().get("upload.flash_size"),
        "flash_mode": env.subst("${__get_board_flash_mode(__env__)}"),
        "flash_freq": env.subst("${__get_board_f_image(__env__)}"),
    })


env.AddCustomTarget(
    name="package",
    dependencies=["$BUILD_DIR/${PROGNAME}.bin"],
    actions=[build_package],
    title="Package Cardputer Advance firmware",
    description="Create and verify factory and Launcher application images; never uploads",
)
