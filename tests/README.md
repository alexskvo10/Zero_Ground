# Zero Ground Integration Test Suite

## Overview

This directory contains integration tests for the Zero Ground multiplayer shooter game. The tests are divided into two categories:

1. **Automated Unit/Integration Tests** (`run_integration_tests.cpp`) - Tests that can be run automatically without GUI
2. **Manual Integration Tests** (`integration_tests.md`) - Comprehensive test scenarios requiring manual execution with the full application

## Running Automated Tests

### Prerequisites
- Visual Studio 2022
- C++17 compiler
- Windows OS

### Compilation

#### Using Visual Studio Command Prompt:
```cmd
cl /EHsc /std:c++17 run_integration_tests.cpp /Fe:run_integration_tests.exe
```

#### Using g++ (MinGW):
```cmd
g++ -std=c++17 run_integration_tests.cpp -o run_integration_tests.exe
```

### Execution
```cmd
run_integration_tests.exe
```

### Expected Output
```
========================================
Zero Ground Integration Test Suite
========================================

Running test: ConnectPacket_Validation... PASSED
Running test: PositionPacket_Validation... PASSED
Running test: MapDataPacket_Validation... PASSED
Running test: CircleRectCollision_Detection... PASSED
Running test: FogOfWar_VisibilityRadius... PASSED
Running test: NetworkCulling_50UnitRadius... PASSED
Running test: SpawnPoints_NoOverlap... PASSED
Running test: MovementSpeed_Calculation... PASSED
Running test: MapBoundaries_Enforcement... PASSED
Running test: PacketSizes_Verification... PASSED
Running test: ReadyPacket_Structure... PASSED
Running test: StartPacket_Structure... PASSED

========================================
Test Summary
========================================
Total tests: 12
Passed: 12
Failed: 0
Success rate: 100%

✓ All tests passed!
```

## Running Manual Integration Tests

Manual tests require running the actual server and client applications. Follow the test procedures in `integration_tests.md`:

1. Build both `Zero_Ground.exe` and `Zero_Ground_client.exe`
2. Open `integration_tests.md`
3. Execute each test scenario sequentially
4. Mark tests as passed/failed
5. Document any issues found

### Key Manual Test Scenarios

- **Test 1**: Server-Client Handshake Sequence
- **Test 2**: Ready Protocol Flow (Connection → Ready → Start)
- **Test 3**: Position Synchronization During Gameplay
- **Test 4**: Collision Detection with Multiple Players
- **Test 5**: Fog of War Visibility with Moving Players
- **Test 6**: Reconnection After Connection Loss
- **Test 7**: Fullscreen Mode Toggle
- **Test 8**: Network Culling (50 Unit Radius)
- **Test 9**: Map Generation and Connectivity
- **Test 10**: Performance Under Load

## Test Coverage

### Automated Tests Cover:
- ✓ Network packet validation
- ✓ Position boundary checking
- ✓ Collision detection algorithms
- ✓ Fog of war visibility calculations
- ✓ Network culling logic
- ✓ Spawn point validation
- ✓ Movement speed calculations
- ✓ Map boundary enforcement
- ✓ Packet structure verification

### Manual Tests Cover:
- ✓ TCP/UDP network communication
- ✓ Multi-threaded synchronization
- ✓ GUI rendering and interaction
- ✓ User input handling
- ✓ Connection loss and recovery
- ✓ Fullscreen mode toggling
- ✓ End-to-end gameplay flow
- ✓ Performance under load

## Continuous Integration

To integrate these tests into a CI/CD pipeline:

1. Add automated tests to build script:
```cmd
cl /EHsc /std:c++17 tests\run_integration_tests.cpp /Fe:tests\run_integration_tests.exe
tests\run_integration_tests.exe
if %ERRORLEVEL% NEQ 0 exit /b 1
```

2. Run manual tests on staging environment before release

3. Monitor test results and update test suite as needed

## Troubleshooting

### Compilation Errors
- Ensure C++17 support is enabled
- Check that all required headers are available
- Verify compiler path is correct

### Test Failures
- Check that game constants match between test and main code
- Verify packet structures haven't changed
- Review error messages for specific failure details

### Manual Test Issues
- Ensure both executables are built successfully
- Check that arial.ttf font file is present
- Verify SFML DLLs are in the correct directories
- Test on target hardware (Intel i3-8100, 8GB RAM)

## Contributing

When adding new features:

1. Add corresponding automated tests to `run_integration_tests.cpp`
2. Add manual test scenarios to `integration_tests.md`
3. Run all tests before committing
4. Update this README if test procedures change

## Test Maintenance

- Review and update tests when requirements change
- Add regression tests for any bugs found
- Keep test documentation synchronized with code
- Archive old test results for historical reference
