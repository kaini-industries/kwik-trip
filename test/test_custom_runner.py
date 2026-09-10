"""Keep Unity's normal runner while honoring the exact library pin in lib_deps."""
from platformio.public import UnityTestRunner


class CustomTestRunner(UnityTestRunner):
    # The built-in runner otherwise adds its own floating Unity dependency.
    EXTRA_LIB_DEPS = None
