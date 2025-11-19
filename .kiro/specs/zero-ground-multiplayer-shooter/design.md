# Design Document

## Overview

Zero Ground is a client-server multiplayer 2D shooter built with C++ and SFML 2.6.1. The architecture separates concerns into distinct layers: network transport (TCP/UDP), game state management, rendering, and input handling. The server acts as the authoritative source for game state, generating the map and coordinating player readiness before game start. Clients connect, synchronize state via UDP, and render their local view with fog of war effects.

The design emphasizes thread safety through mutex-protected shared state, network optimization through spatial culling, and smooth gameplay through interpolation. The ready-check system ensures coordinated game start, while the fog of war system creates strategic gameplay depth.

## Architecture

### High-Level Architecture

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
│  │ Map Generator│  │ Collision    │  │ Render       │      │
│  │ (BFS)        │  │ Detection    │  │ Engine       │      │
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
│  │ Input        │  │ Fog of War   │  │ Render       │      │
│  │ Handler      │  │ System       │  │ Engine       │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

**Server Components:**
- **TCP Listener**: Handles initial client connections, ready status, and game start signals
- **UDP Socket**: Broadcasts player positions at 20Hz to connected clients
- **Game State Manager**: Maintains authoritative player positions, health, and scores with mutex protection
- **Map Generator**: Creates procedural maps with BFS validation for connectivity
- **Collision Detection**: Uses quadtree spatial partitioning for efficient wall collision checks
- **Render Engine**: Displays server player (green circle) and connected clients (blue circles)

**Client Components:**
- **TCP Socket**: Receives map data, initial state, and game start signals
- **UDP Socket**: Sends local player position and receives other player positions
- **Local State Manager**: Maintains interpolated positions for smooth rendering
- **Input Handler**: Processes WASD input and sends position updates
- **Fog of War System**: Calculates visibility radius and applies darkening effects
- **Render Engine**: Displays local player (blue circle), visible enemies, and walls with fog effects

### State Machine Diagrams

**Server State Flow:**
```
┌─────────────┐
│ StartScreen │
│ (Show IP)   │
└──────┬──────┘
       │
       │ Client connects
       ▼
┌─────────────┐
│ WaitingReady│
│ (Yellow msg)│
└──────┬──────┘
       │
       │ Client sends READY
       ▼
┌─────────────┐
│ PlayerReady │
│ (Green msg) │
│ Show PLAY   │
└──────┬──────┘
       │
       │ Operator clicks PLAY
       ▼
┌─────────────┐
│ GameActive  │
│ (Main loop) │
└─────────────┘
```

**Client State Flow:**
```
┌─────────────┐
│ ConnectScreen│
│ (Input IP)  │
└──────┬──────┘
       │
       │ Click CONNECT / Press Enter
       ▼
┌─────────────┐
│ Connecting  │
│ (TCP test)  │
└──────┬──────┘
       │
       ├─ Fail ──► ErrorScreen (3s) ──► ConnectScreen
       │
       │ Success
       ▼
┌─────────────┐
│ Connected   │
│ Show READY  │
└──────┬──────┘
       │
       │ Click READY
       ▼
┌─────────────┐
│ WaitingStart│
│ (Yellow msg)│
└──────┬──────┘
       │
       │ Receive START signal
       ▼
┌─────────────┐
│ GameActive  │
│ (Main loop) │
└─────────────┘
```

## Components and Interfaces

### Network Protocol

**TCP Messages (Port 53000):**

```cpp
// Message types
enum class MessageType : uint8_t {
    CLIENT_CONNECT = 0x01,      // Client → Server
    SERVER_ACK = 0x02,          // Server → Client
    CLIENT_READY = 0x03,        // Client → Server
    SERVER_START = 0x04,        // Server → Client
    MAP_DATA = 0x05             // Server → Client
};

// Connection packet
struct ConnectPacket {
    MessageType type = MessageType::CLIENT_CONNECT;
    uint32_t protocolVersion = 1;
    char playerName[32] = {0};
};

// Ready packet
struct ReadyPacket {
    MessageType type = MessageType::CLIENT_READY;
    bool isReady = true;
};

// Game start packet
struct StartPacket {
    MessageType type = MessageType::SERVER_START;
    uint32_t timestamp;
};

// Map data packet
struct MapDataPacket {
    MessageType type = MessageType::MAP_DATA;
    uint32_t wallCount;
    // Followed by wallCount * Wall structures
};

struct Wall {
    float x, y;           // Top-left corner
    float width, height;
};
```

**UDP Messages (Port 53001):**

```cpp
// Position update packet (sent 20 times per second)
struct PositionPacket {
    float x, y;
    bool isAlive;
    uint32_t frameID;     // For lag compensation
    uint8_t playerId;
};
```

### Core Data Structures

```cpp
// Player representation
struct Player {
    uint32_t id;
    sf::IpAddress ipAddress;
    float x, y;
    float previousX, previousY;  // For interpolation
    float health = 100.0f;
    int score = 0;
    bool isAlive = true;
    bool isReady = false;
    sf::Color color;
    
    // Interpolation support
    float getInterpolatedX(float alpha) const {
        return previousX + (x - previousX) * alpha;
    }
    
    float getInterpolatedY(float alpha) const {
        return previousY + (y - previousY) * alpha;
    }
};

// Game map
struct GameMap {
    std::vector<Wall> walls;
    const float width = 1000.0f;
    const float height = 1000.0f;
    
    // Quadtree for collision optimization
    std::unique_ptr<Quadtree> spatialIndex;
    
    bool isValidPath(sf::Vector2f start, sf::Vector2f end) const;
    void generate();
};

// Quadtree node for spatial partitioning
struct Quadtree {
    sf::FloatRect bounds;
    std::vector<Wall*> walls;
    std::unique_ptr<Quadtree> children[4];
    const int MAX_WALLS = 10;
    const int MAX_DEPTH = 5;
    
    void insert(Wall* wall, int depth = 0);
    std::vector<Wall*> query(sf::FloatRect area) const;
};
```

### Map Generation Algorithm

```cpp
class MapGenerator {
public:
    GameMap generate() {
        for (int attempt = 0; attempt < 10; ++attempt) {
            GameMap map;
            generateWalls(map);
            
            if (validateConnectivity(map)) {
                map.spatialIndex = buildQuadtree(map.walls);
                return map;
            }
        }
        throw std::runtime_error("Failed to generate valid map after 10 attempts");
    }
    
private:
    void generateWalls(GameMap& map) {
        const float targetCoverage = 0.30f; // 30% average
        const float totalArea = 1000.0f * 1000.0f;
        float currentCoverage = 0.0f;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> posDist(0.0f, 1000.0f);
        std::uniform_real_distribution<float> sizeDist(2.0f, 8.0f);
        
        while (currentCoverage < targetCoverage * totalArea) {
            Wall wall;
            wall.x = posDist(gen);
            wall.y = posDist(gen);
            wall.width = sizeDist(gen);
            wall.height = sizeDist(gen);
            
            // Ensure wall doesn't overlap spawn points
            if (!overlapsSpawnPoint(wall)) {
                map.walls.push_back(wall);
                currentCoverage += wall.width * wall.height;
            }
        }
    }
    
    bool validateConnectivity(const GameMap& map) {
        // BFS from bottom-left to top-right
        sf::Vector2f start(50.0f, 950.0f);  // Server spawn
        sf::Vector2f end(950.0f, 50.0f);    // Client spawn
        
        return bfsPathExists(start, end, map);
    }
    
    bool bfsPathExists(sf::Vector2f start, sf::Vector2f end, const GameMap& map) {
        // Grid-based BFS with 10x10 unit cells
        const int gridSize = 100;
        std::vector<std::vector<bool>> visited(gridSize, std::vector<bool>(gridSize, false));
        std::queue<sf::Vector2i> queue;
        
        sf::Vector2i startCell(start.x / 10, start.y / 10);
        sf::Vector2i endCell(end.x / 10, end.y / 10);
        
        queue.push(startCell);
        visited[startCell.x][startCell.y] = true;
        
        const int dx[] = {0, 1, 0, -1};
        const int dy[] = {1, 0, -1, 0};
        
        while (!queue.empty()) {
            sf::Vector2i current = queue.front();
            queue.pop();
            
            if (current == endCell) return true;
            
            for (int i = 0; i < 4; ++i) {
                int nx = current.x + dx[i];
                int ny = current.y + dy[i];
                
                if (nx >= 0 && nx < gridSize && ny >= 0 && ny < gridSize &&
                    !visited[nx][ny] && !cellHasWall(nx, ny, map)) {
                    visited[nx][ny] = true;
                    queue.push(sf::Vector2i(nx, ny));
                }
            }
        }
        
        return false;
    }
    
    bool cellHasWall(int cellX, int cellY, const GameMap& map) {
        sf::FloatRect cellBounds(cellX * 10.0f, cellY * 10.0f, 10.0f, 10.0f);
        for (const auto& wall : map.walls) {
            sf::FloatRect wallBounds(wall.x, wall.y, wall.width, wall.height);
            if (cellBounds.intersects(wallBounds)) return true;
        }
        return false;
    }
    
    bool overlapsSpawnPoint(const Wall& wall) {
        sf::FloatRect wallBounds(wall.x, wall.y, wall.width, wall.height);
        sf::FloatRect serverSpawn(0.0f, 900.0f, 100.0f, 100.0f);
        sf::FloatRect clientSpawn(900.0f, 0.0f, 100.0f, 100.0f);
        
        return wallBounds.intersects(serverSpawn) || wallBounds.intersects(clientSpawn);
    }
};
```

### Collision Detection System

```cpp
class CollisionSystem {
public:
    CollisionSystem(const GameMap& map) : map_(map) {}
    
    // Returns adjusted position after collision resolution
    sf::Vector2f resolveCollision(sf::Vector2f oldPos, sf::Vector2f newPos, float radius) {
        // Query nearby walls using quadtree
        sf::FloatRect queryArea(
            newPos.x - radius - 1.0f,
            newPos.y - radius - 1.0f,
            radius * 2 + 2.0f,
            radius * 2 + 2.0f
        );
        
        auto nearbyWalls = map_.spatialIndex->query(queryArea);
        
        for (const Wall* wall : nearbyWalls) {
            if (circleRectCollision(newPos, radius, *wall)) {
                // Push back by 1 unit in opposite direction
                sf::Vector2f direction = oldPos - newPos;
                float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
                
                if (length > 0.001f) {
                    direction /= length;
                    return oldPos + direction * 1.0f;
                }
                
                return oldPos; // No movement if already colliding
            }
        }
        
        return newPos; // No collision
    }
    
private:
    bool circleRectCollision(sf::Vector2f center, float radius, const Wall& wall) {
        // Find closest point on rectangle to circle center
        float closestX = std::max(wall.x, std::min(center.x, wall.x + wall.width));
        float closestY = std::max(wall.y, std::min(center.y, wall.y + wall.height));
        
        float dx = center.x - closestX;
        float dy = center.y - closestY;
        
        return (dx * dx + dy * dy) < (radius * radius);
    }
    
    const GameMap& map_;
};
```

### Fog of War Rendering

```cpp
class FogOfWarRenderer {
public:
    void render(sf::RenderWindow& window, sf::Vector2f playerPos, 
                const std::vector<Player>& otherPlayers, const GameMap& map) {
        const float visibilityRadius = 25.0f;
        
        // Render all walls (dimmed outside radius)
        for (const auto& wall : map.walls) {
            sf::RectangleShape wallShape(sf::Vector2f(wall.width, wall.height));
            wallShape.setPosition(wall.x, wall.y);
            
            float distance = getDistance(playerPos, sf::Vector2f(wall.x, wall.y));
            if (distance > visibilityRadius) {
                wallShape.setFillColor(sf::Color(100, 100, 100)); // Dimmed
            } else {
                wallShape.setFillColor(sf::Color(150, 150, 150)); // Normal
            }
            
            window.draw(wallShape);
        }
        
        // Render only visible players
        for (const auto& player : otherPlayers) {
            float distance = getDistance(playerPos, sf::Vector2f(player.x, player.y));
            if (distance <= visibilityRadius) {
                sf::CircleShape playerShape(20.0f);
                playerShape.setPosition(player.x - 20.0f, player.y - 20.0f);
                playerShape.setFillColor(player.color);
                window.draw(playerShape);
            }
        }
        
        // Apply fog overlay
        sf::RectangleShape fogOverlay(sf::Vector2f(window.getSize()));
        fogOverlay.setFillColor(sf::Color(0, 0, 0, 200));
        
        // Create circular cutout using shader (simplified approach)
        // In actual implementation, use sf::Shader for proper circular fog
        window.draw(fogOverlay);
    }
    
private:
    float getDistance(sf::Vector2f a, sf::Vector2f b) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }
};
```

## Data Models

### Thread-Safe Game State

```cpp
class GameState {
public:
    // Thread-safe player access
    void updatePlayerPosition(uint32_t playerId, float x, float y) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (players_.count(playerId)) {
            players_[playerId].previousX = players_[playerId].x;
            players_[playerId].previousY = players_[playerId].y;
            players_[playerId].x = x;
            players_[playerId].y = y;
        }
    }
    
    Player getPlayer(uint32_t playerId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return players_.at(playerId);
    }
    
    std::vector<Player> getPlayersInRadius(sf::Vector2f center, float radius) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Player> result;
        
        for (const auto& [id, player] : players_) {
            float dx = player.x - center.x;
            float dy = player.y - center.y;
            float distance = std::sqrt(dx * dx + dy * dy);
            
            if (distance <= radius) {
                result.push_back(player);
            }
        }
        
        return result;
    }
    
    void setPlayerReady(uint32_t playerId, bool ready) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (players_.count(playerId)) {
            players_[playerId].isReady = ready;
        }
    }
    
    bool allPlayersReady() const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [id, player] : players_) {
            if (!player.isReady) return false;
        }
        return !players_.empty();
    }
    
private:
    mutable std::mutex mutex_;
    std::map<uint32_t, Player> players_;
    GameMap map_;
};
```

### Network Packet Validation

```cpp
class PacketValidator {
public:
    static bool validatePosition(const PositionPacket& packet) {
        return packet.x >= 0.0f && packet.x <= 1000.0f &&
               packet.y >= 0.0f && packet.y <= 1000.0f;
    }
    
    static bool validateMapData(const MapDataPacket& packet) {
        return packet.wallCount > 0 && packet.wallCount < 10000;
    }
    
    static bool validateConnect(const ConnectPacket& packet) {
        return packet.protocolVersion == 1 &&
               std::strlen(packet.playerName) < 32;
    }
};
```


## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system—essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property 1: Wall Dimensions Validity

*For any* generated map, all walls should have width between 2-8 units and height between 2-8 units.

**Validates: Requirements 1.2**

### Property 2: Map Coverage Constraint

*For any* generated map, the total area covered by walls should be between 25% and 35% of the total map area (1,000,000 square units).

**Validates: Requirements 1.3**

### Property 3: Spawn Point Connectivity

*For any* successfully generated map, a valid path must exist between the server spawn point (bottom-left) and client spawn point (top-right) as verified by BFS algorithm.

**Validates: Requirements 1.4**

### Property 4: Movement Speed Consistency

*For any* player position and movement direction, updating position over 1 second should result in displacement of exactly 5 units in that direction.

**Validates: Requirements 2.1**

### Property 5: Server Input Isolation

*For any* server WASD input, only the server player position should change, and all client player positions should remain unchanged.

**Validates: Requirements 2.2**

### Property 6: Client Input Isolation

*For any* client WASD input, only that specific client's player position should change, and all other player positions (server and other clients) should remain unchanged.

**Validates: Requirements 2.3**

### Property 7: Collision Stops Movement

*For any* player moving toward a wall, when collision is detected, the player's position should not penetrate the wall boundary.

**Validates: Requirements 2.4**

### Property 8: Collision Pushback Distance

*For any* collision between a player and a wall, the player should be pushed back exactly 1 unit from the collision point.

**Validates: Requirements 2.5**

### Property 9: Position Boundary Clamping

*For any* player position update, the resulting x and y coordinates should always be within the range [0, 1000].

**Validates: Requirements 2.6**

### Property 10: Visibility Radius Calculation

*For any* player position and another entity position, the entity should be considered visible if and only if the Euclidean distance between them is less than or equal to 25 units.

**Validates: Requirements 3.1, 3.3, 3.4**

### Property 11: Wall Rendering Completeness

*For any* game state, the number of walls rendered should equal the total number of walls in the map, regardless of player position.

**Validates: Requirements 3.5**

### Property 12: Wall Darkening by Distance

*For any* wall rendered outside the 25-unit visibility radius, its color brightness should be reduced compared to walls within the radius.

**Validates: Requirements 3.6**

### Property 13: Map Data Transmission Completeness

*For any* client connection, the map data packet sent by the server should contain all wall coordinates from the generated map.

**Validates: Requirements 4.3**

### Property 14: Player Position Transmission

*For any* client connection, the server should send initial positions for all currently connected players.

**Validates: Requirements 4.4**

### Property 15: UDP Packet Rate

*For any* 1-second time window during active gameplay, the server should send between 18-22 UDP position update packets (allowing 10% tolerance around 20 packets/second).

**Validates: Requirements 4.5**

### Property 16: Network Packet Structure

*For any* position update packet created, it should contain non-null values for x coordinate, y coordinate, alive status, and frame ID fields.

**Validates: Requirements 4.6**

### Property 17: Network Culling Inclusion

*For any* player within 50 units of another player, that player's data should be included in the network update packet sent to the other player.

**Validates: Requirements 4.7**

### Property 18: Network Culling Exclusion

*For any* player beyond 50 units of another player, that player's data should be excluded from the network update packet sent to the other player.

**Validates: Requirements 4.8**

### Property 19: Invalid Packet Discarding

*For any* network packet that fails validation (invalid coordinates, malformed data), the packet should be discarded without updating game state.

**Validates: Requirements 5.4**

### Property 20: Position Validation Bounds

*For any* received position packet, the validation function should return false if x or y coordinates are outside the range [0, 1000].

**Validates: Requirements 5.5**

### Property 21: Connection Confirmation Protocol

*For any* client TCP connection, a connection confirmation packet should be sent from client to server before any other communication.

**Validates: Requirements 8.1**

### Property 22: Ready Status Protocol

*For any* client ready button click, a ready status packet should be sent to the server via TCP.

**Validates: Requirements 8.3**

### Property 23: Game Start Broadcast

*For any* server PLAY button click, a game start signal should be sent to all connected clients via TCP.

**Validates: Requirements 8.5**

### Property 24: Thread-Safe Position Access

*For any* concurrent read and write operations on player positions, access should be protected by mutex lock to prevent data races.

**Validates: Requirements 10.1**

### Property 25: Thread-Safe Map Access

*For any* concurrent read and write operations on map data, access should be protected by mutex lock to prevent data races.

**Validates: Requirements 10.2**

### Property 26: Position Coordinate Validation

*For any* position data received from network, both x and y coordinates should be validated to be within [0, 1000] range before accepting.

**Validates: Requirements 10.3, 10.4**

### Property 27: Invalid Coordinate Rejection

*For any* position packet with coordinates outside valid range, the packet should be discarded and game state should remain unchanged.

**Validates: Requirements 10.5**

### Property 28: Linear Interpolation Bounds

*For any* two player positions (previous and target) and interpolation alpha value in [0, 1], the interpolated position should lie on the line segment between the two positions.

**Validates: Requirements 11.4**

## Error Handling

### Network Errors

**Connection Failures:**
- TCP connection timeout (3 seconds) triggers error screen with retry option
- Invalid IP address format displays error message for 3 seconds
- Connection lost detection after 2 seconds without UDP packets

**Packet Validation:**
```cpp
class ErrorHandler {
public:
    static void handleInvalidPacket(const std::string& reason) {
        std::cerr << "[ERROR] Invalid packet: " << reason << std::endl;
        // Packet is discarded, game continues
    }
    
    static void handleConnectionLost(const std::string& serverIP) {
        // Display reconnection screen
        showReconnectScreen(serverIP);
    }
    
    static void handleMapGenerationFailure() {
        std::cerr << "[FATAL] Failed to generate valid map after 10 attempts" << std::endl;
        // Display error message and exit gracefully
        exit(1);
    }
};
```

### Game State Errors

**Collision Edge Cases:**
- Player spawning inside wall: Push to nearest valid position
- Multiple simultaneous collisions: Resolve in order of detection
- Floating point precision errors: Use epsilon comparison (0.001f)

**Thread Safety:**
- All shared state access wrapped in mutex locks
- No direct pointer sharing between threads
- Copy data when passing between threads

### Performance Degradation

**Frame Rate Monitoring:**
```cpp
class PerformanceMonitor {
public:
    void update(float deltaTime) {
        frameCount_++;
        elapsedTime_ += deltaTime;
        
        if (elapsedTime_ >= 1.0f) {
            float fps = frameCount_ / elapsedTime_;
            
            if (fps < 55.0f) {
                logPerformanceWarning(fps);
            }
            
            frameCount_ = 0;
            elapsedTime_ = 0.0f;
        }
    }
    
private:
    void logPerformanceWarning(float fps) {
        std::cerr << "[WARNING] FPS dropped to " << fps << std::endl;
        // Log additional metrics: player count, wall count, etc.
    }
    
    int frameCount_ = 0;
    float elapsedTime_ = 0.0f;
};
```

## Testing Strategy

### Unit Testing

The project will use **Google Test (gtest)** framework for C++ unit testing. Unit tests will focus on:

**Core Logic Tests:**
- Map generation algorithm (wall placement, coverage calculation)
- BFS pathfinding validation
- Collision detection (circle-rectangle intersection)
- Position validation and boundary clamping
- Packet serialization/deserialization
- Interpolation calculations

**Example Unit Tests:**
```cpp
TEST(MapGeneratorTest, WallDimensionsAreValid) {
    MapGenerator generator;
    GameMap map = generator.generate();
    
    for (const auto& wall : map.walls) {
        EXPECT_GE(wall.width, 2.0f);
        EXPECT_LE(wall.width, 8.0f);
        EXPECT_GE(wall.height, 2.0f);
        EXPECT_LE(wall.height, 8.0f);
    }
}

TEST(CollisionSystemTest, PlayerStopsAtWall) {
    GameMap map;
    map.walls.push_back(Wall{100.0f, 100.0f, 10.0f, 10.0f});
    CollisionSystem collision(map);
    
    sf::Vector2f oldPos(90.0f, 105.0f);
    sf::Vector2f newPos(110.0f, 105.0f); // Moving into wall
    
    sf::Vector2f result = collision.resolveCollision(oldPos, newPos, 5.0f);
    
    EXPECT_LT(result.x, 95.0f); // Should be pushed back
}

TEST(PacketValidatorTest, RejectsOutOfBoundsPosition) {
    PositionPacket packet;
    packet.x = 1500.0f; // Out of bounds
    packet.y = 500.0f;
    
    EXPECT_FALSE(PacketValidator::validatePosition(packet));
}
```

### Property-Based Testing

The project will use **RapidCheck** library for property-based testing in C++. Property tests will verify universal properties across randomly generated inputs.

**Configuration:**
- Minimum 100 iterations per property test
- Each property test tagged with comment referencing design document property number

**Property Test Examples:**

```cpp
// Feature: zero-ground-multiplayer-shooter, Property 1: Wall Dimensions Validity
TEST(MapGeneratorPropertyTest, AllWallsHaveValidDimensions) {
    rc::check("all generated walls have dimensions between 2-8 units", []() {
        MapGenerator generator;
        GameMap map = generator.generate();
        
        for (const auto& wall : map.walls) {
            RC_ASSERT(wall.width >= 2.0f && wall.width <= 8.0f);
            RC_ASSERT(wall.height >= 2.0f && wall.height <= 8.0f);
        }
    });
}

// Feature: zero-ground-multiplayer-shooter, Property 2: Map Coverage Constraint
TEST(MapGeneratorPropertyTest, MapCoverageWithinBounds) {
    rc::check("total wall coverage is between 25-35%", []() {
        MapGenerator generator;
        GameMap map = generator.generate();
        
        float totalArea = 0.0f;
        for (const auto& wall : map.walls) {
            totalArea += wall.width * wall.height;
        }
        
        float coverage = totalArea / 1000000.0f;
        RC_ASSERT(coverage >= 0.25f && coverage <= 0.35f);
    });
}

// Feature: zero-ground-multiplayer-shooter, Property 9: Position Boundary Clamping
TEST(PlayerMovementPropertyTest, PositionAlwaysWithinBounds) {
    rc::check("player position is always clamped to map bounds", []() {
        float x = *rc::gen::inRange(-100.0f, 1100.0f);
        float y = *rc::gen::inRange(-100.0f, 1100.0f);
        
        sf::Vector2f pos = clampToMapBounds(sf::Vector2f(x, y));
        
        RC_ASSERT(pos.x >= 0.0f && pos.x <= 1000.0f);
        RC_ASSERT(pos.y >= 0.0f && pos.y <= 1000.0f);
    });
}

// Feature: zero-ground-multiplayer-shooter, Property 10: Visibility Radius Calculation
TEST(FogOfWarPropertyTest, VisibilityBasedOnDistance) {
    rc::check("entities within 25 units are visible, beyond are not", []() {
        sf::Vector2f playerPos = *rc::gen::pair(
            rc::gen::inRange(0.0f, 1000.0f),
            rc::gen::inRange(0.0f, 1000.0f)
        ).as<sf::Vector2f>();
        
        sf::Vector2f entityPos = *rc::gen::pair(
            rc::gen::inRange(0.0f, 1000.0f),
            rc::gen::inRange(0.0f, 1000.0f)
        ).as<sf::Vector2f>();
        
        float distance = getDistance(playerPos, entityPos);
        bool shouldBeVisible = distance <= 25.0f;
        bool isVisible = checkVisibility(playerPos, entityPos, 25.0f);
        
        RC_ASSERT(isVisible == shouldBeVisible);
    });
}

// Feature: zero-ground-multiplayer-shooter, Property 20: Position Validation Bounds
TEST(NetworkPropertyTest, InvalidCoordinatesRejected) {
    rc::check("position packets with out-of-bounds coordinates are rejected", []() {
        PositionPacket packet;
        packet.x = *rc::gen::inRange(-1000.0f, 2000.0f);
        packet.y = *rc::gen::inRange(-1000.0f, 2000.0f);
        
        bool isValid = PacketValidator::validatePosition(packet);
        bool shouldBeValid = (packet.x >= 0.0f && packet.x <= 1000.0f &&
                             packet.y >= 0.0f && packet.y <= 1000.0f);
        
        RC_ASSERT(isValid == shouldBeValid);
    });
}

// Feature: zero-ground-multiplayer-shooter, Property 28: Linear Interpolation Bounds
TEST(InterpolationPropertyTest, InterpolatedPositionBetweenEndpoints) {
    rc::check("interpolated position lies between previous and target", []() {
        sf::Vector2f prev = *rc::gen::pair(
            rc::gen::inRange(0.0f, 1000.0f),
            rc::gen::inRange(0.0f, 1000.0f)
        ).as<sf::Vector2f>();
        
        sf::Vector2f target = *rc::gen::pair(
            rc::gen::inRange(0.0f, 1000.0f),
            rc::gen::inRange(0.0f, 1000.0f)
        ).as<sf::Vector2f>();
        
        float alpha = *rc::gen::inRange(0.0f, 1.0f);
        
        sf::Vector2f interpolated = lerp(prev, target, alpha);
        
        // Check if interpolated point is on line segment
        float minX = std::min(prev.x, target.x);
        float maxX = std::max(prev.x, target.x);
        float minY = std::min(prev.y, target.y);
        float maxY = std::max(prev.y, target.y);
        
        RC_ASSERT(interpolated.x >= minX && interpolated.x <= maxX);
        RC_ASSERT(interpolated.y >= minY && interpolated.y <= maxY);
    });
}
```

### Integration Testing

Integration tests will verify component interactions:

**Network Integration:**
- Server-client handshake sequence
- Ready protocol flow (connect → ready → start)
- Position synchronization over UDP
- Map data transmission over TCP

**Game Loop Integration:**
- Input → Movement → Collision → Rendering pipeline
- Network receive → State update → Rendering pipeline
- Multi-threaded state access (UDP thread + main thread)

### Performance Testing

Performance tests will validate optimization requirements:

**Benchmarks:**
- Map generation time (should be < 100ms)
- Collision detection with 100+ walls (should be < 1ms per frame)
- Network packet serialization (should be < 0.1ms)
- Rendering 2 players + 100 walls (should maintain 60 FPS)

**Profiling:**
- CPU usage monitoring during 2-player gameplay
- Memory allocation tracking
- Network bandwidth measurement

### Test Execution Strategy

1. **Development Phase**: Run unit tests and property tests on every build
2. **Integration Phase**: Run integration tests after component completion
3. **Performance Phase**: Run benchmarks and profiling before release
4. **Continuous**: All tests run in CI pipeline on commit

**Test Coverage Goals:**
- Core logic: 90%+ code coverage
- Network protocol: 100% property coverage
- Collision system: 100% property coverage
- UI state machines: 80%+ coverage
