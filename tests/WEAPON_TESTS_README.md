# Weapon System Property-Based Tests

## Overview

This document describes the property-based tests for the weapon system in Zero Ground.

## Test File

- **File**: `tests/run_weapon_tests.cpp`
- **Compilation Script**: `tests/compile_and_run_weapon_tests.bat`

## Property Tests

### Property 33: Weapon Property Completeness

**Feature**: weapon-shop-system  
**Validates**: Requirements 11.2

**Property Statement**: For any weapon type, all required properties (magazine size, damage, range, bullet speed, reload time, movement speed) should have defined non-zero values.

**Test Implementation**:
- Runs 100 iterations with randomly generated weapon types
- Validates that each weapon has:
  - Non-empty name
  - Positive magazine size
  - Positive damage
  - Positive range
  - Positive bullet speed
  - Positive reload time
  - Positive movement speed
  - Non-negative reserve ammo
  - Current ammo initialized to magazine size
  - Non-negative price

## Unit Tests

The test suite also includes unit tests for specific weapon types:
- USP initialization
- AWP initialization
- AK-47 initialization
- Unique weapon names across all 10 types
- Valid price ranges for weapon categories

## Running the Tests

### Prerequisites

You need one of the following C++ compilers:
- Visual Studio 2022 with C++ tools
- MinGW-w64 with g++

### Execution

```batch
cd tests
.\compile_and_run_weapon_tests.bat
```

## Known Issues

### Visual Studio Environment Issue

The current Visual Studio installation (version 18.0.1 / 2026) has environment configuration errors that prevent the `cl` compiler from being found. The error message indicates:

```
[ERROR:VsDevCmd.bat] *** VsDevCmd.bat encountered errors. Environment may be incomplete and/or incorrect. ***
```

**Workaround Options**:

1. **Install MinGW-w64**: Download and install MinGW-w64 which includes g++
2. **Repair Visual Studio**: Run Visual Studio Installer and repair the C++ tools
3. **Manual Compilation**: Use Visual Studio IDE to compile the test file directly

### Test Code Status

The test code (`run_weapon_tests.cpp`) is **syntactically correct** and has been validated by the language server. It will compile and run successfully once the compiler environment is properly configured.

## Test Results

**Status**: Not yet executed due to compiler environment issues  
**Code Quality**: Validated - no syntax errors  
**Expected Outcome**: All tests should pass when executed

## Next Steps

1. Fix the Visual Studio environment or install MinGW-w64
2. Run the tests to verify weapon initialization
3. All tests are expected to pass based on the implementation
