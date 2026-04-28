---
title: "[MEDIUM] Python subprocess calls lack timeout for long-running commands"
severity: MEDIUM
domain: lint
lens: toolgate/lint
labels:
  - "audit:toolgate/lint"
---

## Summary
Python scripts use `subprocess.run()` and `subprocess.check_call()` without specifying a timeout, which can cause the scripts to hang indefinitely if the external command stalls.

**Files affected:**
- `tools/coredump.py` (line 53): `subprocess.run(cmd, capture_output=False)`
- `tools/pio_submodules.py` (line 19): `subprocess.check_call(cmd, cwd=str(project_dir))`

## Impact
Without a timeout, subprocess calls can hang indefinitely due to:
1. **Stuck external commands**: GDB, esptool, or git can hang waiting for input or a device
2. **No automatic recovery**: Scripts need manual interruption (Ctrl+C) to recover
3. **Poor UX**: Users may not realize the script is stuck vs. just slow

## Evidence
**tools/coredump.py (lines 48-57):**
```python
def run_command(cmd, description):
    """Run a command and handle errors."""
    print(f"\n>>> {description}")
    print(f"    Command: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=False)
    if result.returncode != 0:
        print(f"    FAILED with exit code {result.returncode}")
        return False
    return True
```

**tools/pio_submodules.py (lines 17-20):**
```python
def _update_submodules(project_dir: Path, rel_paths: list[str]) -> None:
    cmd = ["git", "submodule", "update", "--init", "--recursive", *rel_paths]
    subprocess.check_call(cmd, cwd=str(project_dir))
```

## Recommended Fix
Add timeout parameters to all subprocess calls:

**tools/coredump.py:**
```python
def run_command(cmd, description, timeout=300):
    """Run a command and handle errors."""
    print(f"\n>>> {description}")
    print(f"    Command: {' '.join(cmd)}")
    try:
        result = subprocess.run(cmd, capture_output=False, timeout=timeout)
        if result.returncode != 0:
            print(f"    FAILED with exit code {result.returncode}")
            return False
        return True
    except subprocess.TimeoutExpired:
        print(f"    TIMEOUT after {timeout}s")
        return False
```

**tools/pio_submodules.py:**
```python
def _update_submodules(project_dir: Path, rel_paths: list[str], timeout=600) -> None:
    cmd = ["git", "submodule", "update", "--init", "--recursive", *rel_paths]
    subprocess.check_call(cmd, cwd=str(project_dir), timeout=timeout)
```

## References
- subprocess.TimeoutExpired: https://docs.python.org/3/library/subprocess.html#subprocess.TimeoutExpired
- Best practices for subprocess: https://realpython.com/ditching-os-system-python/
