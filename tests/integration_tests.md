# Integration Test Suite for Zero Ground Multiplayer Shooter

## Test Environment Setup
- **Server**: Zero_Ground.exe running on localhost
- **Client**: Zero_Ground_client.exe connecting to server
- **Network**: Local network (127.0.0.1 or local IP)
- **Requirements**: SFML 2.6.1, Visual Studio 2022, Windows OS

## Test Execution Log

### Test 1: Server-Client Handshake Sequence
**Objective**: Verify complete TCP handshake between server and client

**Test Steps**:
1. Start server application
2. Verify server displays "THE SERVER IS RUNNING" in green (64pt)
3. Verify server shows local IP address
4. Verify server shows "Ожидание игрока..." (Waiting for player)
5. Start client application
6. Enter server IP address in client
7. Click "ПОДКЛЮЧИТЬСЯ К СЕРВЕРУ" (Connect to Server) button
8. Verify client receives ConnectPacket acknowledgment
9. Verify client receives MapDataPacket with wall data
10. Verify client receives initial player positions
11. Verify server updates status to "The player is connected, but not ready" (yellow)
12. Verify client displays "Соединение установлено" (Connection established)

**Expected Results**:
- ✓ TCP connection established on port 53000
- ✓ ConnectPacket validated (protocol version = 1)
- ✓ MapDataPacket sent with wallCount > 0 and < 10000
- ✓ All wall data transmitted successfully
- ✓ Initial positions sent: Server (25, 475), Client (475, 25)
- ✓ Server UI updates to show connected status
- ✓ Client UI shows connection success

**Status**: ⬜ Not Run | ✓ Passed | ✗ Failed

---

### Test 2: Ready Protocol Flow (Connection → Ready → Start)
**Objective**: Verify complete ready protocol sequence

**Test Steps**:
1. Complete Test 1 (establish connection)
2. On client, verify green "ГОТОВ" (READY) button appears (200x60px)
3. Click "ГОТОВ" button on client
4. Verify client sends ReadyPacket to server via TCP
5. Verify server receives ReadyPacket
6. Verify server updates status to "The player is connected and ready to play" (green)
7. Verify server displays green "PLAY" button (200x60px)
8. Click "PLAY" button on server
9. Verify server sends StartPacket to all connected clients
10. Verify client receives StartPacket
11. Verify both server and client transition to game screen simultaneously

**Expected Results**:
- ✓ ReadyPacket transmitted successfully
- ✓ Server UI updates to green "ready" status
- ✓ PLAY button appears on server
- ✓ StartPacket sent with timestamp
- ✓ Both applications transition to MainScreen state
- ✓ UDP listener thread starts on server (port 53001)
- ✓ UDP sender thread starts on client (port 53002)

**Status**: ⬜ Not Run | ✓ Passed | ✗ Failed

---

### Test 3: Position Synchronization During Gameplay
**Objective**: Verify UDP position synchronization at 20Hz

**Test Steps**:
1. Complete Test 2 (start game)
2. On server, press WASD keys to move green circle
3. Verify server position updates locally
4. Verify server sends PositionPacket via UDP at 20Hz (50ms intervals)
5. Verify client receives server position updates
6. Verify client renders server player (green circle, 20px radius)
7. On client, press WASD keys to move blue circle
8. Verify client position updates locally
9. Verify client sends PositionPacket via UDP at 20Hz
10. Verify server receives client position updates
11. Verify server renders client player (blue circle, 20px radius)
12. Verify positions are validated (0-500 range for x and y)
13. Verify invalid positions are rejected and logged

**Expected Results**:
- ✓ UDP packets sent at 20Hz frequency
- ✓ PositionPacket contains: x, y, isAlive, frameID, playerId
- ✓ Position validation: 0 ≤ x ≤ 500, 0 ≤ y ≤ 500
- ✓ Invalid packets logged and discarded
- ✓ Smooth interpolation applied for rendering
- ✓ Movement speed: 5 units/second
- ✓ Both players visible on both screens

**Status**: ⬜ Not Run | ✓ Passed | ✗ Failed

---

### Test 4: Collision Detection with Multiple Players
**Objective**: Verify collision detection works correctly with walls

**Test Steps**:
1. Complete Test 3 (establish position sync)
2. On server, move green circle toward a wall
3. Verify collision detected when player touches wall
4. Verify player stops immediately upon collision
5. Verify player pushed back 1 unit from wall
6. Verify player cannot pass through wall
7. On client, move blue circle toward a wall
8. Verify same collision behavior on client
9. Move both players to same area with walls
10. Verify both players collide with walls independently
11. Verify quadtree spatial partitioning used for collision queries
12. Verify collision resolution doesn't cause jitter or stuck states

**Expected Results**:
- ✓ Circle-rectangle collision detection accurate
- ✓ Player radius: 30px for local player
- ✓ Collision pushback: 1 unit in opposite direction
- ✓ Quadtree query area: player radius + 1 unit buffer
- ✓ Map boundaries enforced: [0, 500] for both x and y
- ✓ No wall penetration occurs
- ✓ Smooth collision resolution without stuttering

**Status**: ⬜ Not Run | ✓ Passed | ✗ Failed

---

### Test 5: Fog of War Visibility with Moving Players
**Objective**: Verify fog of war system correctly limits visibility

**Test Steps**:
1. Complete Test 3 (establish position sync)
2. On client, verify visibility radius is 25 units around blue circle
3. Move client player away from server player (> 25 units)
4. Verify server player (green circle) disappears from client view
5. Verify walls outside 25 unit radius are darkened (RGB: 100, 100, 100)
6. Verify walls inside 25 unit radius are normal (RGB: 150, 150, 150)
7. Verify black semi-transparent overlay (alpha: 200) outside radius
8. Move client player toward server player (< 25 units)
9. Verify server player becomes visible when within radius
10. Verify fog effect updates dynamically as player moves
11. Verify all walls always visible (but darkened outside radius)
12. On server, verify NO fog of war effect (full visibility)

**Expected Results**:
- ✓ Visibility radius: exactly 25 units
- ✓ Players outside radius: not rendered
- ✓ Players inside radius: fully visible
- ✓ Walls always visible with darkening effect
- ✓ Fog overlay: black with alpha 200
- ✓ Distance calculation accurate: sqrt(dx² + dy²)
- ✓ Server has full map visibility (no fog)
- ✓ Client fog updates in real-time

**Status**: ⬜ Not Run | ✓ Passed | ✗ Failed

---

### Test 6: Reconnection After Connection Loss
**Objective**: Verify reconnection mechanism after network failure

**Test Steps**:
1. Complete Test 3 (establish position sync)
2. Simulate connection loss (close server or disconnect network)
3. Wait 2 seconds for client to detect connection loss
4. Verify client displays "Соединение потеряно. Нажмите R для переподключения к [IP]"
5. Verify client transitions to ConnectionLost state
6. Restart server (if closed)
7. On client, press 'R' key
8. Verify client attempts to reconnect to last known IP
9. Verify reconnection follows same handshake sequence (Test 1)
10. Verify game state restored after successful reconnection
11. Test connection loss during different states:
    - During connection screen
    - During ready/waiting state
    - During active gameplay

**Expected Results**:
- ✓ Connection loss detected after 2 seconds without UDP packets
- ✓ Reconnection screen displayed with server IP
- ✓ 'R' key triggers reconnection attempt
- ✓ Full handshake repeated on reconnection
- ✓ Game state restored correctly
- ✓ Error logged: "Connection lost with server: [IP]"
- ✓ Reconnection works from any state

**Status**: ⬜ Not Run | ✓ Passed | ✗ Failed

---

### Test 7: Fullscreen Mode Toggle
**Objective**: Verify fullscreen toggle works on both server and client

**Test Steps**:
1. Start server in fullscreen mode (default)
2. Verify server window is fullscreen
3. Press Escape key on server
4. Verify server switches to windowed mode (800x600)
5. Press Escape key again on server
6. Verify server returns to fullscreen mode
7. Verify UI elements re-center after each toggle
8. Start client in fullscreen mode (default)
9. Verify client window is fullscreen
10. Press Escape key on client
11. Verify client switches to windowed mode (800x600)
12. Press Escape key again on client
13. Verify client returns to fullscreen mode
14. Verify UI elements re-center after each toggle
15. Test toggle during different states:
    - Start screen (server)
    - Connection screen (client)
    - Game screen (both)

**Expected Results**:
- ✓ Default mode: fullscreen (sf::Style::Fullscreen)
- ✓ Windowed mode: 800x600 (sf::Style::Resize | sf::Style::Close)
- ✓ Escape key toggles between modes
- ✓ UI elements properly centered after resize
- ✓ Game continues without interruption
- ✓ Toggle works in all application states
- ✓ No rendering artifacts or crashes

**Status**: ⬜ Not Run | ✓ Passed | ✗ Failed

---

## Additional Integration Scenarios

### Test 8: Network Culling (50 Unit Radius)
**Objective**: Verify server only sends player data within 50 unit radius

**Test Steps**:
1. Complete Test 3 (establish position sync)
2. Position server player at (250, 250) - center of map
3. Position client player at (250, 250) - same location
4. Verify server sends client position data
5. Move client player to (350, 250) - 100 units away
6. Verify server stops sending client position data (> 50 units)
7. Move client player to (280, 250) - 30 units away
8. Verify server resumes sending client position data (< 50 units)

**Expected Results**:
- ✓ Network culling radius: 50 units
- ✓ Players outside radius: not included in UDP packets
- ✓ Players inside radius: included in UDP packets
- ✓ Bandwidth optimization working correctly

**Status**: ⬜ Not Run | ✓ Passed | ✗ Failed

---

### Test 9: Map Generation and Connectivity
**Objective**: Verify generated map is valid and playable

**Test Steps**:
1. Start server multiple times (10+ iterations)
2. For each start, verify map generation succeeds
3. Verify wall count between 15-25 walls
4. Verify each wall has dimensions 3-20 units (length) × 3 units (thickness)
5. Verify walls don't overlap spawn points:
   - Server spawn: (0, 450) to (50, 500)
   - Client spawn: (450, 0) to (500, 50)
6. Verify BFS path exists between spawns
7. Verify quadtree built successfully
8. If generation fails, verify retry up to 10 attempts
9. If all attempts fail, verify error message and graceful exit

**Expected Results**:
- ✓ Map generation success rate > 90%
- ✓ Wall count: 15-25 walls
- ✓ Wall dimensions: 3-20 length, 3 thickness
- ✓ Spawn points clear of walls
- ✓ BFS connectivity validated
- ✓ Quadtree depth ≤ 5, max 10 walls per node
- ✓ Generation time < 100ms
- ✓ Error handling for failed generation

**Status**: ⬜ Not Run | ✓ Passed | ✗ Failed

---

### Test 10: Performance Under Load
**Objective**: Verify performance meets requirements (55+ FPS, <40% CPU)

**Test Steps**:
1. Complete Test 3 (establish position sync)
2. Monitor FPS on both server and client
3. Verify FPS ≥ 55 on both applications
4. Move both players continuously for 60 seconds
5. Verify FPS remains stable
6. Monitor CPU usage (approximate)
7. Verify CPU usage < 40% with 2 players
8. Check for performance warnings in logs
9. Verify frame time < 18ms (for 55+ FPS)
10. Test with maximum walls (25 walls)

**Expected Results**:
- ✓ FPS ≥ 55 on Intel i3-8100, 8GB RAM
- ✓ CPU usage < 40% with 2 players
- ✓ No performance warnings logged
- ✓ Smooth gameplay without stuttering
- ✓ Interpolation provides smooth movement
- ✓ Quadtree optimization effective

**Status**: ⬜ Not Run | ✓ Passed | ✗ Failed

---

## Test Execution Summary

| Test # | Test Name | Status | Notes |
|--------|-----------|--------|-------|
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

---

## Known Issues and Limitations

1. **Manual Testing Required**: These tests require manual execution as there's no automated test framework for SFML GUI applications
2. **Network Timing**: Some tests may be sensitive to network latency
3. **Performance Metrics**: CPU usage is estimated, not precisely measured
4. **Single Client**: Tests assume one client connection; multi-client scenarios not covered

---

## Test Execution Instructions

### Prerequisites
1. Build both Zero_Ground.exe and Zero_Ground_client.exe in Visual Studio 2022
2. Ensure arial.ttf font file is present in both executable directories
3. Ensure all SFML 2.6.1 DLLs are present
4. Have two monitors or two machines for simultaneous testing

### Execution Steps
1. Start with Test 1 and proceed sequentially
2. Mark each test as ✓ (Passed) or ✗ (Failed) after execution
3. Document any failures in the Notes column
4. For failed tests, capture screenshots and error logs
5. Re-test after fixes are applied

### Logging
- Server logs: Check console output from Zero_Ground.exe
- Client logs: Check console output from Zero_Ground_client.exe
- Look for [ERROR], [WARNING], and [INFO] messages
- Network errors logged with operation and status details

---

## Automated Test Script (Future Enhancement)

For future automation, consider:
1. **Headless Testing**: Mock SFML window for automated testing
2. **Network Simulation**: Use loopback with controlled latency/packet loss
3. **Test Framework**: Integrate Google Test or Catch2 for C++
4. **CI/CD Integration**: Run tests on every commit
5. **Performance Profiling**: Use Visual Studio Profiler for accurate metrics
