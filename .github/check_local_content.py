"""Keep private documentation and tools out of the shared repository."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
LOCAL_ROOTS = ("docs", "project/tools", "tools")


def git(root: Path, *args: str) -> subprocess.CompletedProcess:
    return subprocess.run(["git", "-C", str(root), *args], capture_output=True,
                          text=True, encoding="utf-8", check=True)


def check(root: Path) -> list[str]:
    errors = []
    rules = (root / ".gitignore").read_text(encoding="utf-8-sig").splitlines()
    for directory in LOCAL_ROOTS:
        if f"/{directory}/" not in rules:
            errors.append(f"Missing directory exclusion: /{directory}/")
    # Folder exclusions must not acquire file-by-file public exceptions again.
    for rule in rules:
        if rule.startswith("!") and any(
                rule[1:].lstrip("/").startswith(directory + "/")
                for directory in LOCAL_ROOTS):
            errors.append(f"Local directory has a public exception: {rule}")
    paths = git(root, "ls-files", "-z", "--", *LOCAL_ROOTS).stdout.split("\0")
    errors.extend(f"Tracked local-only file: {path}" for path in paths if path)
    for directory in LOCAL_ROOTS:
        for suffix in ("new.md", "new.py", "new.cpp", "nested/deep/new.md"):
            path = f"{directory}/{suffix}"
            result = subprocess.run(["git", "-C", str(root), "check-ignore",
                                     "--no-index", "-q", path])
            if result.returncode != 0:
                errors.append(f"Future local file is not excluded: {path}")
    return errors


if __name__ == "__main__":
    failures = check(ROOT)
    if failures:
        print("\n".join(failures))
        raise SystemExit(1)
    print("PASS: docs/, project/tools/ and tools/ are local only")
