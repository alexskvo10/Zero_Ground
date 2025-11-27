# Zero_Ground
Zero Ground isn't just an arena. It's an abandoned zone.

After the catastrophe when countries destroyed each other in the "Silent War," only five underground arenas remain—built by robotic engineers to "filter out the survivors."

Players aren't soldiers. Not mercenaries.

They are the remnants of humanity.

Some came here by mistake.
Some came here voluntarily.
Some came here to forget.

There are no rules in Zero Ground.
No teams.
No salvation.

Only bullets.
Only walls.
Only you.

And the earth.

The ground that never forgets.

---

## Technical Overview

Zero Ground is a multiplayer 2D top-down shooter built with C++ and SFML 2.6.1. The game features:

- **Client-Server Architecture**: Authoritative server with UDP position synchronization
- **Dynamic Map Generation**: Procedurally generated 5000×5000 pixel maps with cell-based wall system
- **Cell-Based Wall System**: 167×167 grid of 30-pixel cells with probabilistic wall generation
- **BFS Connectivity Validation**: Ensures all spawn points are reachable before game starts
- **Dynamic Camera System**: Smooth camera following with optimized viewport culling
- **Fog of War**: Limited visibility system for strategic gameplay
- **Weapon Shop System**: Purchase weapons and ammo with detailed tooltips showing full stats and compatibility
- **Optimized Performance**: Cell-based spatial partitioning for efficient collision detection and rendering
- **Real-time Monitoring**: Comprehensive performance metrics and profiling

## Performance

The game is optimized to run on modest hardware:
- **Target Hardware**: Intel i3-8100, 8GB RAM
- **Target FPS**: 55+ (maintains 60 FPS)
- **Map Generation**: < 100ms
- **Collision Detection**: < 1ms per frame
- **CPU Usage**: < 40% with 2 players

### Performance Testing

Comprehensive performance monitoring is built into the server:

```cmd
# Run performance tests
cd tests
run_performance_tests.bat
```

The server logs real-time metrics every second:
- FPS and frame time
- Collision detection time
- CPU usage estimation
- Network bandwidth (sent/received)
- Player and wall counts

For detailed performance testing procedures, see:
- `tests/PERFORMANCE_TESTING_GUIDE.md` - Complete testing guide
- `tests/PERFORMANCE_TEST_RESULTS.md` - Results and analysis

## Building and Running

### Prerequisites
- Visual Studio 2022 with C++ development tools
- SFML 2.6.1 (already configured)
- Windows 10/11

### Quick Start

1. **Build the projects:**
   - Open `Zero_Ground/Zero_Ground.sln` in Visual Studio
   - Build in Debug or Release configuration (x64)

2. **Start the server:**
   ```cmd
   Zero_Ground\x64\Debug\Zero_Ground.exe
   ```

3. **Connect client(s):**
   ```cmd
   Zero_Ground_client\x64\Debug\Zero_Ground_client.exe
   ```

4. **Play:**
   - Enter server IP on client
   - Click READY on client
   - Click PLAY on server
   - Use WASD to move
   - Press ESC to toggle fullscreen

## Testing

### Integration Tests
```cmd
cd tests
compile_and_run_tests.bat
```

See `tests/INTEGRATION_TEST_SUMMARY.md` for details.

### Performance Tests
```cmd
cd tests
run_performance_tests.bat
```

See `tests/PERFORMANCE_TESTING_GUIDE.md` for detailed procedures.

## Architecture

### High-Level Overview

```
┌─────────────────────────────────────────────────────────────┐
│                         SERVER                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ TCP Listener │  │ UDP Socket   │  │ Game State   │      │
│  │ (Port 53000) │  │ (Port 53001) │  │ Manager      │      │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
│         │                  │                  │              │
│         └──────────────────┴──────────────────┘              │
│                            │                                 │
│         ┌──────────────────┴──────────────────┐             │
│         │                                      │             │
│  ┌──────▼───────┐  ┌──────────────┐  ┌───────▼──────┐      │
│  │ Map Generator│  │ Collision    │  │ Rendering    │      │
│  │ (BFS)        │  │ (Quadtree)   │  │ Engine       │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                            ▲
                            │ TCP/UDP
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                         CLIENT                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ TCP Socket   │  │ UDP Socket   │  │ Local State  │      │
│  │ (Port 53000) │  │ (Port 53002) │  │ Manager      │      │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
│         │                  │                  │              │
│         └──────────────────┴──────────────────┘              │
│                            │                                 │
│         ┌──────────────────┴──────────────────┐             │
│         │                                      │             │
│  ┌──────▼───────┐  ┌──────────────┐  ┌───────▼──────┐      │
│  │ Input Handler│  │ Fog of War   │  │ Rendering    │      │
│  │              │  │ System       │  │ Engine       │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

**Server Components:**
- **TCP Listener (Port 53000)**: Handles client connections, ready status, and game start signals
- **UDP Socket (Port 53001)**: Broadcasts player positions at 20Hz to all connected clients
- **Game State Manager**: Thread-safe storage of player positions, health, and scores
- **Map Generator**: Creates procedurally generated maps with BFS connectivity validation
- **Collision System**: Uses quadtree spatial partitioning for efficient wall collision detection
- **Rendering Engine**: Displays server player (green circle) and connected clients (blue circles)

**Client Components:**
- **TCP Socket (Port 53000)**: Receives map data, initial state, and game start signals
- **UDP Socket (Port 53002)**: Sends local player position and receives other players' positions
- **Local State Manager**: Maintains interpolated positions for smooth rendering
- **Input Handler**: Processes WASD input and sends position updates
- **Fog of War System**: Calculates 25-unit visibility radius and applies darkening effects
- **Rendering Engine**: Displays local player (blue circle), visible enemies, and walls

### Key Algorithms

**Cell-Based Map System:**
- Map size: 5000×5000 pixels divided into 167×167 cells (30 pixels each)
- Each cell can have 0-2 walls on its borders (top, right, bottom, left)
- Wall dimensions: 12 pixels wide × 30 pixels long
- Probabilistic generation: 60% chance for 1 wall, 25% for 2 walls, 15% for 0 walls
- Only cells where (i+j) % 2 == 1 can have walls (checkerboard pattern)
- Performance: < 100ms generation time, < 1ms BFS validation

**Dynamic Camera System:**
- Camera follows player position smoothly
- Viewport clamped to map boundaries [0, 5000]
- Optimized rendering: only draws walls within 21×21 cell radius around player
- Cached visible cell boundaries (recalculated only when player changes cells)
- Typical rendering: 400-500 walls instead of 10,000+ total walls
- Performance: 60+ FPS maintained with dynamic camera

**BFS Connectivity Validation:**
- Validates path exists between spawn points (250, 4750) and (4750, 250)
- Checks wall barriers between adjacent cells
- Guarantees map connectivity before game starts
- Maximum 10 generation attempts with fallback
- Performance: < 1ms for typical maps

**Cell-Based Collision Detection:**
- Player collision checks only walls in 3×3 cell radius
- Walls centered on cell borders for accurate collision
- Circle-rectangle collision with 1-pixel pushback
- Boundary enforcement: player kept within [PLAYER_SIZE/2, MAP_SIZE - PLAYER_SIZE/2]
- Performance: O(1) constant time per frame

**Circle-Rectangle Collision:**
- Finds closest point on rectangle to circle center
- Compares squared distances (avoids sqrt)
- Handles all cases: sides, corners, containment
- Performance: O(1) constant time

## Network Protocol

### TCP Messages (Port 53000)

Used for connection handshake, ready status, and game start coordination.

**Message Types:**
```cpp
enum class MessageType : uint8_t {
    CLIENT_CONNECT = 0x01,  // Client → Server: Initial connection
    SERVER_ACK = 0x02,      // Server → Client: Connection acknowledged
    CLIENT_READY = 0x03,    // Client → Server: Player ready to start
    SERVER_START = 0x04,    // Server → Client: Game starting
    MAP_DATA = 0x05         // Server → Client: Map wall data
};
```

**Connection Flow:**
1. **Client → Server**: `ConnectPacket` (protocol version, player name)
2. **Server → Client**: `MapDataPacket` (wall count + wall data)
3. **Server → Client**: Initial player positions
4. **Client → Server**: `ReadyPacket` (ready status)
5. **Server → Client**: `StartPacket` (game start signal)

**Packet Structures:**
```cpp
// Connection request
struct ConnectPacket {
    MessageType type = CLIENT_CONNECT;
    uint32_t protocolVersion = 1;
    char playerName[32];
};

// Ready status
struct ReadyPacket {
    MessageType type = CLIENT_READY;
    bool isReady = true;
};

// Game start signal
struct StartPacket {
    MessageType type = SERVER_START;
    uint32_t timestamp;
};

// Map data header (followed by Wall structures)
struct MapDataPacket {
    MessageType type = MAP_DATA;
    uint32_t wallCount;
};

struct Wall {
    float x, y;           // Top-left corner
    float width, height;  // Dimensions
};
```

### UDP Messages (Ports 53001/53002)

Used for real-time position synchronization at 20Hz (50ms intervals).

**Position Packet:**
```cpp
struct PositionPacket {
    float x, y;           // Player position (0-500 range)
    bool isAlive;         // Alive status
    uint32_t frameID;     // Frame identifier for lag compensation
    uint8_t playerId;     // Player identifier (0 = server, 1+ = clients)
};
```

**Update Flow:**
- **Client → Server (Port 53001)**: Local player position every 50ms
- **Server → Clients (Port 53002)**: Server position + nearby players every 50ms

**Network Optimization:**
- **Culling Radius**: Server only sends players within 50 units
- **Visibility Radius**: Clients only render players within 25 units
- **Interpolation**: Smooth movement between position updates
- **Validation**: All positions checked for valid range [0, 500]

### Protocol Validation

All packets are validated before processing:
- **Position validation**: Coordinates must be in [0, 500] range
- **Protocol version**: Must match server version (currently 1)
- **Packet size**: Must match expected struct size
- **Player name**: Must be < 32 characters

Invalid packets are logged and discarded without affecting game state.

## Building and Running

### Prerequisites

**Required Software:**
- **Visual Studio 2022** with C++ development tools
  - Desktop development with C++ workload
  - Windows 10 SDK
- **SFML 2.6.1** (already configured in project)
- **Windows 10/11** (64-bit)

**Hardware Requirements:**
- **Minimum**: Intel i3-8100 or equivalent, 8GB RAM
- **Recommended**: Intel i5 or better, 16GB RAM
- **Graphics**: Any GPU with OpenGL 2.0+ support

### SFML 2.6.1 Setup

The project is pre-configured with SFML 2.6.1. If you need to reconfigure:

1. **Download SFML 2.6.1** for Visual C++ 17 (2022) - 64-bit
   - https://www.sfml-dev.org/download/sfml/2.6.1/

2. **Extract SFML** to a known location (e.g., `C:\SFML-2.6.1`)

3. **Configure Visual Studio Project:**
   - Right-click project → Properties
   - Configuration: All Configurations, Platform: x64
   - **C/C++ → General → Additional Include Directories:**
     - Add: `C:\SFML-2.6.1\include`
   - **Linker → General → Additional Library Directories:**
     - Add: `C:\SFML-2.6.1\lib`
   - **Linker → Input → Additional Dependencies:**
     - Debug: `sfml-graphics-d.lib;sfml-window-d.lib;sfml-system-d.lib;sfml-network-d.lib`
     - Release: `sfml-graphics.lib;sfml-window.lib;sfml-system.lib;sfml-network.lib`

4. **Copy DLL files** to executable directory:
   - From `C:\SFML-2.6.1\bin\` copy all `.dll` files
   - To `Zero_Ground\x64\Debug\` and `Zero_Ground_client\x64\Debug\`

5. **Copy font file** (`arial.ttf`) to both executable directories

### Build Instructions

**Option 1: Visual Studio GUI**

1. Open `Zero_Ground\Zero_Ground.sln` in Visual Studio 2022
2. Select configuration:
   - **Debug**: Easier debugging, slower performance
   - **Release**: Optimized, 30-50% faster (recommended for testing)
3. Select platform: **x64** (required)
4. Build → Build Solution (or press F7)
5. Executables will be in:
   - `Zero_Ground\x64\Debug\Zero_Ground.exe` (server)
   - `Zero_Ground_client\x64\Debug\Zero_Ground_client.exe` (client)

**Option 2: Command Line (MSBuild)**

```cmd
# Open Developer Command Prompt for VS 2022

# Build Debug configuration
msbuild Zero_Ground\Zero_Ground.sln /p:Configuration=Debug /p:Platform=x64

# Build Release configuration (recommended)
msbuild Zero_Ground\Zero_Ground.sln /p:Configuration=Release /p:Platform=x64
```

### Running the Game

**Step 1: Start the Server**
```cmd
cd Zero_Ground\x64\Debug
Zero_Ground.exe
```

The server will:
- Generate a random map (takes < 100ms)
- Display "SERVER RUNNING" screen
- Show server IP address
- Listen on ports 53000 (TCP) and 53001 (UDP)

**Step 2: Connect Client(s)**
```cmd
cd Zero_Ground_client\x64\Debug
Zero_Ground_client.exe
```

On the client:
1. Enter server IP address (shown on server screen)
2. Click "CONNECT TO SERVER" or press Enter
3. Wait for "Connection established" message
4. Click "READY" button

**Step 3: Start the Game**

On the server:
1. Wait for "Player connected and ready to play" (green text)
2. Click green "PLAY" button
3. Game starts for both server and client

**Step 4: Play**
- **Movement**: WASD keys
- **Shop**: Press B near a shop (red square) to open weapon shop
- **Weapon Tooltips**: Hover mouse over any weapon in shop to see detailed stats
- **Fullscreen Toggle**: ESC key
- **Reconnect**: If disconnected, press R on client

### Controls

**Server Player (Green Circle):**
- W: Move up
- S: Move down
- A: Move left
- D: Move right
- ESC: Toggle fullscreen/windowed mode

**Client Player (Blue Circle):**
- W: Move up
- S: Move down
- A: Move left
- D: Move right
- B: Open/close weapon shop (when near a shop)
- E: Open/close inventory
- 1-4: Switch between weapon slots
- Mouse: Aim and shoot
- R: Reload weapon
- ESC: Toggle fullscreen/windowed mode
- R: Reconnect after connection loss

**Weapon Shop:**
- Hover mouse over any weapon to see detailed stats tooltip
- Hover mouse over any ammo to see price, quantity, and compatible weapons
- Click on weapon to purchase (if you have enough money and inventory space)
- Click on ammo to purchase (if you have compatible weapon and enough money)
- Weapon tooltip shows: damage, magazine size, reserve ammo, range, bullet speed, reload time, movement speed, fire mode
- Ammo tooltip shows: price, quantity per purchase, list of compatible weapons

### Troubleshooting

**"Failed to bind to port 53000"**
- Another instance is already running
- Close all Zero_Ground processes and try again
- Check if another application is using port 53000

**"Server unavailable or invalid IP"**
- Verify server is running and showing IP address
- Check firewall settings (allow ports 53000, 53001, 53002)
- Try using 127.0.0.1 if running on same machine

**Low FPS / Performance Issues**
- Build in Release configuration (30-50% faster than Debug)
- Close other applications to free CPU/RAM
- Check performance metrics in server console
- See `tests/PERFORMANCE_TESTING_GUIDE.md` for profiling

**Font not loading**
- Ensure `arial.ttf` is in the same directory as the executable
- Copy from project root to `x64\Debug\` or `x64\Release\`

## Known Limitations and Future Improvements

### Current Limitations

**Networking:**
- **Single Client Support**: Currently optimized for 1 server + 1 client
  - Multiple clients can connect but may experience synchronization issues
  - Future: Implement proper multi-client state management
- **No Lag Compensation**: Position updates are sent as-is without prediction
  - High latency (>100ms) causes visible stuttering
  - Future: Implement client-side prediction and server reconciliation
- **No Packet Loss Handling**: UDP packets are fire-and-forget
  - Lost packets cause temporary position desync
  - Future: Implement redundant position data or TCP fallback
- **LAN Only**: No NAT traversal or public server support
  - Players must be on same local network
  - Future: Implement STUN/TURN for internet play

**Gameplay:**
- **No Combat System**: Players can move but not shoot or damage each other
  - Health bar and score are displayed but not functional
  - Future: Implement shooting, hit detection, and damage system
- **No Respawn**: Death state is visual only, no respawn mechanism
  - Future: Implement respawn timer and spawn point selection
- **Static Map**: Map is generated once at server start
  - No dynamic obstacles or destructible environment
  - Future: Implement map variations or procedural changes during gameplay
- **No Game Modes**: Only free-for-all movement, no objectives
  - Future: Add capture points, team modes, or survival modes

**Performance:**
- **Debug Build Performance**: Debug builds run 30-50% slower than Release
  - Always use Release build for performance testing
  - Debug builds may not meet 55 FPS target on minimum hardware
- **Single-Threaded Rendering**: Rendering happens on main thread
  - Future: Move rendering to separate thread for better CPU utilization
- **No GPU Acceleration**: Fog of war uses CPU-based rendering
  - Future: Implement shader-based fog of war for better performance

**User Experience:**
- **No In-Game Chat**: Players cannot communicate during gameplay
  - Future: Add text chat or voice communication
- **No Player Names**: Players are only identified by color
  - Future: Display player names above characters
- **No Settings Menu**: All settings are hardcoded
  - Future: Add graphics, audio, and control customization
- **No Reconnect on Server**: Only clients can reconnect
  - Server crash requires full restart
  - Future: Implement server state persistence

### Planned Improvements

**High Priority:**
1. **Combat System**: Shooting, hit detection, damage, and death
2. **Multi-Client Support**: Proper synchronization for 2-8 players
3. **Lag Compensation**: Client-side prediction and server reconciliation
4. **Settings Menu**: Graphics quality, controls, audio volume

**Medium Priority:**
5. **Game Modes**: Team deathmatch, capture the flag, survival
6. **Player Customization**: Names, colors, character skins
7. **In-Game Chat**: Text communication between players
8. **Map Variations**: Multiple map layouts and themes

**Low Priority:**
9. **Internet Play**: NAT traversal, dedicated servers, matchmaking
10. **Replay System**: Record and playback gameplay
11. **Statistics**: Kill/death ratio, accuracy, playtime tracking
12. **Achievements**: Unlock system for milestones

### Contributing

If you'd like to contribute to addressing these limitations:
1. Check the specification documents in `.kiro/specs/zero-ground-multiplayer-shooter/`
2. Review the design document for architecture details
3. Follow the existing code style and structure
4. Test thoroughly with both Debug and Release builds
5. Update documentation for any new features

## Documentation

### Performance and Testing
- `tests/PERFORMANCE_TESTING_GUIDE.md` - Performance testing procedures
- `tests/PERFORMANCE_TEST_RESULTS.md` - Performance analysis and results
- `tests/INTEGRATION_TEST_SUMMARY.md` - Integration test documentation
- `tests/TEST_EXECUTION_GUIDE.md` - Test execution instructions

### Features
- `WEAPON_TOOLTIP_FEATURE.md` - Weapon tooltip system documentation
- `WEAPON_TOOLTIP_TESTING.md` - Tooltip testing guide
- `WEAPON_TOOLTIP_VISUAL_GUIDE.txt` - Visual design reference
- `ПОДСКАЗКИ_ОРУЖИЯ.md` - User guide in Russian
- `CHANGELOG_WEAPON_TOOLTIP.md` - Tooltip feature changelog
- `AMMO_TOOLTIP_FEATURE.md` - Ammo tooltip system documentation
- `AMMO_TOOLTIP_TESTING.md` - Ammo tooltip testing guide
- `AMMO_TOOLTIP_VISUAL_GUIDE.txt` - Ammo tooltip visual design reference
- `ПОДСКАЗКИ_ПАТРОНОВ.md` - Ammo tooltip user guide in Russian
- `CHANGELOG_AMMO_TOOLTIP.md` - Ammo tooltip feature changelog
- `QUICK_START_AMMO_TOOLTIP.txt` - Quick start guide for ammo tooltips

### Specifications
- `.kiro/specs/zero-ground-multiplayer-shooter/` - Complete specification documents
  - `requirements.md` - Detailed requirements with acceptance criteria
  - `design.md` - Architecture and implementation design
  - `tasks.md` - Implementation task breakdown
- `.kiro/specs/weapon-shop-system/` - Weapon shop system specifications
