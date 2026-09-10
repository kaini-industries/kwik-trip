"""Generate a per-environment catalog; fail builds on invalid hardware profiles."""
Import("env")
import sys
from pathlib import Path

root = Path(env.subst("$PROJECT_DIR"))
sys.path.insert(0, str(root / "tools"))
from etaglib import generated_header, load_hosts, load_profiles

hosts = load_hosts(root)
host = next((h for h in hosts if h["id"] == env.subst("$PIOENV")), None)
if host is None:
    raise ValueError("Add this environment to config/hosts.json before building")
if env.BoardConfig().id != host["board"]:
    raise ValueError("PlatformIO board and host profile disagree")
header = generated_header(host, load_profiles(root))
generated = Path(env.subst("$BUILD_DIR")) / "generated"
generated.mkdir(parents=True, exist_ok=True)
path = generated / "etag_catalog.h"
if not path.exists() or path.read_text() != header:
    path.write_text(header)
env.Append(CPPPATH=[str(generated)])
