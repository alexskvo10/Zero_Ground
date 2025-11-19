# Zero Ground Integration Test Execution Guide

## Purpose

This guide provides step-by-step instructions for executing the complete integration test suite for the Zero Ground multiplayer shooter game. Follow this guide to ensure all game functionality is working correctly before deployment.

## Prerequisites

### Software Requirements
- ✓ Visual Studio 2022 with C++ tools installed
- ✓ SFML 2.6.1 libraries and DLLs
- ✓ Windows 10/11 operating system
- ✓ arial.ttf font file in executable directories

### Hardware Requirements (for performance testing)
- Intel i3-8100 or equivalent
- 8GB RAM minimum
- Network adapter (for local network testing)

### Build Requirements
1. Build `Zero_Ground.exe` (server) in Visual Studio 2022
2. Build `Zero_Ground_client.exe` (client) in Visual Studio 2022
3. Ensure all SFML DLLs are copied to executable directories
4. Verify arial.ttf is present in both directories

## Test Execution Phases

### Phase 1: Automated Unit Tests (15 minutes)

#### Step 1: Compile Test Runner
```cmd
cd tests
compile_and_run_tests.bat
```

#### Step 2: Verify Test Results
Expected output:
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

#### Step 3: Document Results
- [ ] All 12 automated tests passed
- [ ] No compilation errors
- [ ] Test execution time < 1 second
- [ ] No memory leaks detected

**If any tests fail**: Stop and investigate before proceeding to manual tests.

---

### Phase 2: Manual Integration Tests (60-90 minutes)

#### Setup
1. Open two command prompt windows
2. Navigate to `Zero_Ground` directory in first window
3. Navigate to `Zero_Ground_client` directory in second window
4. Have `tests/integration_tests.md` open for reference

#### Test 1: Server-Client Handshake (10 minutes)

**Execution Steps**:
1. In first window, run: `Zero_Ground.exe`
2. Verify server startup screen displays correctly
3. Note the server IP address shown
4. In second window, run: `Zero_Ground_client.exe`
5. Enter server IP in client
6. Click "ПОДКЛЮЧИТЬСЯ К СЕРВЕРУ"
7. Observe connection sequence

**Validation Checklist**:
- [ ] Server shows "THE SERVER IS RUNNING" in green (64pt)
- [ ] Server displays valid IP address (not 127.0.0.1 or 0.0.0.0)
- [ ] Server shows "Ожидание игрока..." initially
- [ ] Client connection screen displays correctly
- [ ] Client accepts IP input (digits and dots only, max 15 chars)
- [ ] Connection succeeds within 3 seconds
- [ ] Server updates to "The player is connected, but not ready" (yellow)
- [ ] Client shows "Соединение установлено" (green)
- [ ] No error messages in console logs

**Result**: ⬜ Not Run | ✓ Passed | ✗ Failed

**Notes**: _______________________________________________

---

#### Test 2: Ready Protocol Flow (5 minutes)

**Execution Steps**:
1. Continue from Test 1 (connection established)
2. On client, click green "ГОТОВ" button
3. Observe server UI update
4. On server, click green "PLAY" button
5. Observe transition to game screen on both applications

**Validation Checklist**:
- [ ] "ГОТОВ" button appears on client (200x60px, green)
- [ ] Server updates to "The player is connected and ready to play" (green)
- [ ] "PLAY" button appears on server (200x60px, green)
- [ ] Both applications transition to game screen simultaneously
- [ ] Game map renders on both screens
- [ ] Server player (green circle) visible on server
- [ ] Client player (blue circle) visible on client
- [ ] No crashes or freezes during transition

**Result**: ⬜ Not Run | ✓ Passed | ✗ Failed

**Notes**: _______________________________________________

---

#### Test 3: Position Synchronization (15 minutes)

**Execution Steps**:
1. Continue from Test 2 (game started)
2. On server, press W/A/S/D keys to move green circle
3. Observe movement on both server and client screens
4. On client, press W/A/S/D keys to move blue circle
5. Observe movement on both server and client screens
6. Move both players simultaneously
7. Monitor console logs for position updates

**Validation Checklist**:
- [ ] Server player moves smoothly on server screen
- [ ] Server player visible on client screen (if within 25 units)
- [ ] Client player moves smoothly on client screen
- [ ] Client player visible on server screen
- [ ] Movement speed feels consistent (5 units/second)
- [ ] No jitter or stuttering in movement
- [ ] Position updates logged at ~20Hz (check console)
- [ ] No position validation errors in logs
- [ ] Interpolation provides smooth rendering
- [ ] Both players can move independently

**Result**: ⬜ Not Run | ✓ Passed | ✗ Failed

**Notes**: _______________________________________________

---

#### Test 4: Collision Detection (15 minutes)

**Execution Steps**:
1. Continue from Test 3 (position sync working)
2. On server, move green circle toward a wall
3. Verify collision behavior
4. Try to move through wall from different angles
5. On client, move blue circle toward walls
6. Verify same collision behavior
7. Test collision at map boundaries (edges)

**Validation Checklist**:
- [ ] Player stops immediately when touching wall
- [ ] Player pushed back 1 unit from wall
- [ ] Player cannot pass through walls
- [ ] Collision works from all angles
- [ ] Map boundaries enforced (0-500 range)
- [ ] No wall penetration occurs
- [ ] No stuck states or jitter at walls
- [ ] Collision detection consistent on both server and client
- [ ] Quadtree optimization working (check performance)
- [ ] Both players collide with walls independently

**Result**: ⬜ Not Run | ✓ Passed | ✗ Failed

**Notes**: _______________________________________________

---

#### Test 5: Fog of War Visibility (15 minutes)

**Execution Steps**:
1. Continue from Test 4 (collision working)
2. On client, position blue circle near server player
3. Verify server player (green circle) is visible
4. Move client player away from server player (> 25 units)
5. Verify server player disappears
6. Move client player back toward server player (< 25 units)
7. Verify server player reappears
8. Observe wall darkening effect as you move

**Validation Checklist**:
- [ ] Visibility radius is 25 units around client player
- [ ] Server player visible when within 25 units
- [ ] Server player invisible when beyond 25 units
- [ ] Walls always visible (never completely hidden)
- [ ] Walls darkened outside 25 unit radius (RGB: 100,100,100)
- [ ] Walls normal inside 25 unit radius (RGB: 150,150,150)
- [ ] Black semi-transparent overlay outside radius (alpha: 200)
- [ ] Fog effect updates smoothly as player moves
- [ ] Server has full visibility (no fog effect)
- [ ] Distance calculation accurate

**Result**: ⬜ Not Run | ✓ Passed | ✗ Failed

**Notes**: _______________________________________________

---

#### Test 6: Reconnection After Connection Loss (10 minutes)

**Execution Steps**:
1. Continue from Test 5 (fog of war working)
2. Close server application (simulate connection loss)
3. Wait 2 seconds on client
4. Verify client displays connection lost screen
5. Restart server application
6. On client, press 'R' key
7. Verify reconnection sequence
8. Test game functionality after reconnection

**Validation Checklist**:
- [ ] Client detects connection loss within 2 seconds
- [ ] Client displays "Соединение потеряно. Нажмите R для переподключения к [IP]"
- [ ] Client shows last known server IP
- [ ] 'R' key triggers reconnection attempt
- [ ] Reconnection follows full handshake sequence
- [ ] Map data re-transmitted successfully
- [ ] Game state restored after reconnection
- [ ] Position synchronization resumes
- [ ] No crashes during reconnection
- [ ] Error logged: "Connection lost with server: [IP]"

**Result**: ⬜ Not Run | ✓ Passed | ✗ Failed

**Notes**: _______________________________________________

---

#### Test 7: Fullscreen Mode Toggle (5 minutes)

**Execution Steps**:
1. Start fresh server and client applications
2. Verify both start in fullscreen mode
3. On server, press Escape key
4. Verify server switches to windowed mode
5. Press Escape again on server
6. Verify server returns to fullscreen
7. Repeat steps 3-6 on client
8. Test toggle during gameplay

**Validation Checklist**:
- [ ] Server starts in fullscreen mode
- [ ] Client starts in fullscreen mode
- [ ] Escape key toggles to windowed mode (800x600)
- [ ] Escape key toggles back to fullscreen
- [ ] UI elements re-center after each toggle
- [ ] No rendering artifacts during toggle
- [ ] Game continues without interruption
- [ ] Toggle works on start screen
- [ ] Toggle works during gameplay
- [ ] No crashes or freezes

**Result**: ⬜ Not Run | ✓ Passed | ✗ Failed

**Notes**: _______________________________________________

---

#### Test 8: Network Culling (10 minutes)

**Execution Steps**:
1. Start server and client, establish connection
2. Position server player at (250, 250) - center
3. Position client player at (250, 250) - same location
4. Verify position updates being sent (check console)
5. Move client player to (350, 250) - 100 units away
6. Verify position updates stop (check console)
7. Move client player to (280, 250) - 30 units away
8. Verify position updates resume

**Validation Checklist**:
- [ ] Position updates sent when players within 50 units
- [ ] Position updates stop when players beyond 50 units
- [ ] Network culling radius is exactly 50 units
- [ ] Bandwidth optimization working
- [ ] No errors when culling activates
- [ ] Position sync resumes when back in range
- [ ] Distance calculation accurate
- [ ] Console logs show culling behavior

**Result**: ⬜ Not Run | ✓ Passed | ✗ Failed

**Notes**: _______________________________________________

---

#### Test 9: Map Generation (10 minutes)

**Execution Steps**:
1. Close server if running
2. Start server 10 times (restart each time)
3. For each start, verify map generation succeeds
4. Observe wall count and distribution
5. Verify spawn points are clear
6. Check console logs for generation details

**Validation Checklist**:
- [ ] Map generation succeeds on all 10 attempts
- [ ] Wall count between 15-25 walls each time
- [ ] Walls have varied positions and orientations
- [ ] Server spawn area (0,450)-(50,500) is clear
- [ ] Client spawn area (450,0)-(500,50) is clear
- [ ] BFS connectivity check passes
- [ ] Quadtree built successfully
- [ ] Generation time < 100ms (check logs)
- [ ] No generation errors in console
- [ ] Map coverage appears reasonable (visual check)

**Result**: ⬜ Not Run | ✓ Passed | ✗ Failed

**Notes**: _______________________________________________

---

#### Test 10: Performance Under Load (15 minutes)

**Execution Steps**:
1. Start server and client, establish connection
2. Start game and move both players continuously
3. Monitor FPS on both applications (visual check)
4. Continue for 60 seconds
5. Check console logs for performance warnings
6. Monitor CPU usage in Task Manager
7. Test with maximum walls (restart until 25 walls generated)

**Validation Checklist**:
- [ ] Server FPS ≥ 55 throughout test
- [ ] Client FPS ≥ 55 throughout test
- [ ] No visible stuttering or lag
- [ ] CPU usage < 40% on target hardware
- [ ] No performance warnings in console logs
- [ ] Frame time < 18ms (for 55+ FPS)
- [ ] Smooth gameplay with 25 walls
- [ ] Interpolation working smoothly
- [ ] Quadtree optimization effective
- [ ] No memory leaks (stable memory usage)

**Result**: ⬜ Not Run | ✓ Passed | ✗ Failed

**Notes**: _______________________________________________

---

## Phase 3: Test Summary and Reporting

### Test Results Summary

| Test # | Test Name | Status | Issues Found |
|--------|-----------|--------|--------------|
| Auto | Automated Unit Tests | ⬜ | |
| 1 | Server-Client Handshake | ⬜ | |
| 2 | Ready Protocol Flow | ⬜ | |
| 3 | Position Synchronization | ⬜ | |
| 4 | Collision Detection | ⬜ | |
| 5 | Fog of War Visibility | ⬜ | |
| 6 | Reconnection | ⬜ | |
| 7 | Fullscreen Toggle | ⬜ | |
| 8 | Network Culling | ⬜ | |
| 9 | Map Generation | ⬜ | |
| 10 | Performance | ⬜ | |

**Overall Status**: ⬜ Not Started | 🔄 In Progress | ✓ All Passed | ✗ Some Failed

**Total Tests**: 11 (1 automated suite + 10 manual tests)
**Passed**: _____
**Failed**: _____
**Success Rate**: _____%

### Issues Found

#### Critical Issues (Blocking Release)
1. _______________________________________________
2. _______________________________________________
3. _______________________________________________

#### Major Issues (Should Fix Before Release)
1. _______________________________________________
2. _______________________________________________
3. _______________________________________________

#### Minor Issues (Can Fix After Release)
1. _______________________________________________
2. _______________________________________________
3. _______________________________________________

### Recommendations

#### Before Release
- [ ] All critical issues resolved
- [ ] All major issues resolved or documented
- [ ] Performance meets requirements (55+ FPS, <40% CPU)
- [ ] No crashes or data loss scenarios
- [ ] Network protocol stable and validated

#### Post-Release Monitoring
- [ ] Monitor user reports for edge cases
- [ ] Track performance metrics in production
- [ ] Collect feedback on gameplay experience
- [ ] Plan fixes for minor issues in next update

### Sign-Off

**Tester Name**: _____________________
**Date**: _____________________
**Signature**: _____________________

**Approval**: ⬜ Approved for Release | ⬜ Needs Fixes | ⬜ Rejected

**Comments**: _______________________________________________
_______________________________________________
_______________________________________________

---

## Appendix A: Troubleshooting Common Issues

### Issue: Server won't start
**Solution**: Check that port 53000 and 53001 are not in use. Close any other instances.

### Issue: Client can't connect
**Solution**: Verify server IP is correct. Check firewall settings. Ensure server is running.

### Issue: Position sync not working
**Solution**: Check UDP port 53001 (server) and 53002 (client) are open. Verify network connectivity.

### Issue: Low FPS
**Solution**: Check CPU usage. Verify hardware meets requirements. Close background applications.

### Issue: Collision detection not working
**Solution**: Verify map generated successfully. Check quadtree built correctly. Review console logs.

### Issue: Fog of war not rendering
**Solution**: Check visibility radius calculation. Verify rendering order. Check alpha blending.

---

## Appendix B: Console Log Analysis

### Normal Operation Logs
```
[INFO] Map generated successfully on attempt 1
[INFO] Total walls: 18
[INFO] Map coverage: 28.5%
[INFO] Quadtree built successfully
[INFO] TCP listener thread started on port 53000
[INFO] Ready listener thread started
[INFO] New client connection from 192.168.1.100
[INFO] Valid ConnectPacket received from 192.168.1.100
[INFO] Sent MapDataPacket header with 18 walls
[INFO] Sent all wall data to client
[INFO] Client 192.168.1.100 is ready
[INFO] UDP listener thread started for position synchronization
```

### Error Logs to Watch For
```
[ERROR] Invalid packet received from 192.168.1.100: Position out of bounds
[ERROR] Connection lost with client: 192.168.1.100
[TCP ERROR] Operation: Send StartPacket - Client: 192.168.1.100 - Status: Disconnected
[WARNING] Performance degradation detected! FPS: 48 (target: 55+)
[CRITICAL] Map generation failed after maximum attempts
```

---

## Appendix C: Performance Benchmarks

### Target Performance Metrics
- **FPS**: ≥ 55 on Intel i3-8100, 8GB RAM
- **CPU Usage**: < 40% with 2 players
- **Frame Time**: < 18ms average
- **Network Latency**: < 50ms on local network
- **Map Generation**: < 100ms
- **Memory Usage**: < 200MB per application

### Actual Performance (Fill in during testing)
- **Server FPS**: _____
- **Client FPS**: _____
- **Server CPU**: _____%
- **Client CPU**: _____%
- **Frame Time**: _____ms
- **Network Latency**: _____ms
- **Map Gen Time**: _____ms
- **Server Memory**: _____MB
- **Client Memory**: _____MB

---

**End of Test Execution Guide**
