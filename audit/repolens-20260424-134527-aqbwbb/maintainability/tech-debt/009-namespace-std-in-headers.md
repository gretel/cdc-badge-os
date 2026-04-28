---
title: "[MEDIUM] using namespace std in header files pollutes global namespace"
severity: MEDIUM
domain: maintainability
lens: tech-debt/clean-code
labels:
  - clean-code
  - namespaces
  - headers
---

## Summary
Multiple header files in the CalEPD component use `using namespace std;` at file scope, which pollutes the global namespace for all files that include these headers. This is a common C++ anti-pattern that can cause naming conflicts and make code harder to maintain.

## Impact
- **Naming conflicts**: Common names like `distance`, `time`, `rank` can conflict with std versions
- **Hidden dependencies**: Files including these headers get std namespace without knowing
- ** harder to refactor**: Changes in std can affect more code than expected
- **Code quality**: Considered poor practice in professional C++ codebases

## Evidence

### Files with `using namespace std;` in headers:

**1. epdspi.h** - Line 6
```cpp
#include "esp_log.h"
#include <string>
#include <vector>
using namespace std;  // <-- Pollutes global namespace

class EpdSpi : public Adafruit_GFX {
    // ...
};
```

**2. color/wave5i7ColorVector.h** - Line 17
```cpp
#include "esp_log.h"
#include <string>
#include <epd7color.h>
#include <Adafruit_GFX.h>
#include <epdspi.h>
#include <color/wave7colors.h>
#include <vector>
using namespace std;  // <-- Pollutes global namespace

class Wave5i7Color : public Epd7Color {
    // ...
};
```

### Usage in source files (acceptable):
```cpp
// components/mod_nvsedit/src/NvsEditModule.cpp
using namespace cdc::ui;  // Local scope is OK
using namespace cdc::core;
```

### Correct pattern (scope-specific):
```cpp
void someFunction() {
    using namespace std;  // Limited to function scope
    vector<string> list;
}
```

## Recommended Fix

### Option 1 - Use explicit std:: prefix (recommended):
Replace `using namespace std;` with explicit qualification:

**Before:**
```cpp
#include <string>
#include <vector>
using namespace std;

class EpdSpi {
    string name;
    vector<int> values;
};
```

**After:**
```cpp
#include <string>
#include <vector>

class EpdSpi {
    std::string name;
    std::vector<int> values;
};
```

### Option 2 - Using declarations for specific names:
```cpp
#include <string>
#include <vector>

using std::string;
using std::vector;

class EpdSpi {
    string name;
    vector<int> values;
};
```

### Files to fix:
1. `components/CalEPD/include/epdspi.h` - Line 6
2. `components/CalEPD/include/color/wave5i7ColorVector.h` - Line 17

### Additional check:
Search for other occurrences:
```bash
grep -r "using namespace std;" components/CalEPD/include/
```

## References
- C++ Core Guidelines: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-using-declaration
- "Effective C++" by Scott Meyers, Item 24: Declare friend functions and using declarations
- Google C++ Style Guide: https://google.github.io/styleguide/cppguide.html#Namespaces
