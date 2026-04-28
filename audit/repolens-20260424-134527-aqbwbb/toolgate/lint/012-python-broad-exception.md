---
title: "[HIGH] Python script uses broad exception handling that swallows errors"
severity: HIGH
domain: lint
lens: toolgate/lint
labels:
  - "audit:toolgate/lint"
---

## Summary
The `ble_serial.py` script uses `except Exception:` with a bare `pass` statement, which silently swallows all exceptions without any logging or debugging information.

**Files affected:**
- `tools/ble_serial.py` (lines 96-100)

## Impact
Silent exception handling makes debugging difficult:
1. **Hidden errors**: UTF-8 decode errors or I/O issues are silently ignored
2. **No diagnostics**: Users can't tell why output looks wrong
3. **Hard to debug**: Developers need to add manual logging to find issues

## Evidence
**tools/ble_serial.py (lines 93-101):**
```python
def on_notify(_sender, data: bytearray):
    """Handle incoming data from the badge."""
    try:
        text = data.decode("utf-8", errors="replace")
        sys.stdout.write(text)
        sys.stdout.flush()
    except Exception:
        pass
```

The `except Exception: pass` block catches all exceptions (including `KeyboardInterrupt`, `SystemExit`, etc.) and does nothing.

## Recommended Fix
Replace broad exception handling with specific exceptions and proper logging:

```python
def on_notify(_sender, data: bytearray):
    """Handle incoming data from the badge."""
    try:
        text = data.decode("utf-8", errors="replace")
        sys.stdout.write(text)
        sys.stdout.flush()
    except (UnicodeDecodeError, OSError):
        # Log specific errors for debugging
        import sys
        sys.stderr.write(f"[RX Error: {sys.exc_info()[1]]}\n")
        sys.stderr.flush()
    except Exception:
        # Catch-all for unexpected errors, but log them
        import sys
        sys.stderr.write(f"[RX Error: {sys.exc_info()[1]]}\n")
        sys.stderr.flush()
```

Or use logging module for better control:
```python
import logging

def on_notify(_sender, data: bytearray):
    """Handle incoming data from the badge."""
    try:
        text = data.decode("utf-8", errors="replace")
        sys.stdout.write(text)
        sys.stdout.flush()
    except UnicodeDecodeError as e:
        logging.debug(f"Decode error: {e}")
    except OSError as e:
        logging.debug(f"I/O error: {e}")
```

## References
- Exception handling best practices: https://docs.python.org/3/tutorial/errors.html#handling-exceptions
- Don't use bare except: https://realpython.com/python-exceptions/#the-try-block
