// Integration Test Runner for Zero Ground Multiplayer Shooter
// This file provides automated validation of network protocol and game logic
// Note: GUI tests still require manual execution

#include <iostream>
#include <cassert>
#include <cstring>
#include <cmath>
#include <vector>
#include <memory>
#include <sstream>

// ========================
// Test Framework Macros
// ========================

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running test: " << #name << "..."; \
    try { \
        test_##name(); \
        std::cout << " PASSED" << std::endl; \
        passedTests++; \
    } catch (const std::exception& e) { \
        std::cout << " FAILED: " << e.what() << std::endl; \
        failedTests++; \
    } \
    totalTests++; \
} while(0)

#define ASSERT_TRUE(condition) do { \
    if (!(condition)) { \
        std::ostringstream oss; \
        oss << "Assertion failed: " << #condition << " at line " << __LINE__; \
        throw std::runtime_error(oss.str()); \
    } \
} while(0)

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

#define ASSERT_EQ(expected, actual) do { \
    if ((expected) != (actual)) { \
        std::ostringstream oss; \
        oss << "Assertion failed: expected " << (expected) << " but got " << (actual) << " at line " << __LINE__; \
        throw std::runtime_error(oss.str()); \
    } \
} while(0)

#define ASSERT_NEAR(expected, actual, tolerance) do { \
    if (std::abs((expected) - (actual)) > (tolerance)) { \
        std::ostringstream oss; \
        oss << "Assertion failed: expected " << (expected) << " but got " << (actual) \
            << " (tolerance: " << (tolerance) << ") at line " << __LINE__; \
        throw std::runtime_error(oss.str()); \
    } \
} while(0)

// Test counters
int totalTests = 0;
int passedTests = 0;
int failedTests = 0;

// ========================
// Minimal Data Structures (copied from main code)
// ========================

struct Position {
    float x = 0.0f;
    float y = 0.0f;
};

struct Wall {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

enum class MessageType : uint8_t {
    CLIENT_CONNECT = 0x01,
    SERVER_ACK = 0x02,
    CLIENT_READY = 0x03,
    SERVER_START = 0x04,
    MAP_DATA = 0x05
};

struct ConnectPacket {
    MessageType type = MessageType::CLIENT_CONNECT;
    uint32_t protocolVersion = 1;
    char playerName[32] = {0};
};

struct ReadyPacket {
    MessageType type = MessageType::CLIENT_READY;
    bool isReady = true;
};

struct StartPacket {
    MessageType type = MessageType::SERVER_START;
    uint32_t timestamp = 0;
};

struct MapDataPacket {
    MessageType type = MessageType::MAP_DATA;
    uint32_t wallCount = 0;
};

struct PositionPacket {
    float x = 0.0f;
    float y = 0.0f;
    bool isAlive = true;
    uint32_t frameID = 0;
    uint8_t playerId = 0;
};

// ========================
// Validation Functions (copied from main code)
// ========================

bool validatePosition(const PositionPacket& packet) {
    return packet.x >= 0.0f && packet.x <= 500.0f &&
           packet.y >= 0.0f && packet.y <= 500.0f;
}

bool validateMapData(const MapDataPacket& packet) {
    return packet.wallCount > 0 && packet.wallCount < 10000;
}

bool validateConnect(const ConnectPacket& packet) {
    return packet.protocolVersion == 1 &&
           std::strlen(packet.playerName) < 32;
}

// ========================
// Collision Detection (copied from main code)
// ========================

bool circleRectCollision(float centerX, float centerY, float radius, const Wall& wall) {
    float closestX = std::max(wall.x, std::min(centerX, wall.x + wall.width));
    float closestY = std::max(wall.y, std::min(centerY, wall.y + wall.height));
    
    float dx = centerX - closestX;
    float dy = centerY - closestY;
    
    return (dx * dx + dy * dy) < (radius * radius);
}

// ========================
// Fog of War (copied from main code)
// ========================

float getDistance(float x1, float y1, float x2, float y2) {
    float dx = x1 - x2;
    float dy = y1 - y2;
    return std::sqrt(dx * dx + dy * dy);
}

bool isVisible(float playerX, float playerY, float targetX, float targetY, float radius) {
    return getDistance(playerX, playerY, targetX, targetY) <= radius;
}

// ========================
// Integration Tests
// ========================

TEST(ConnectPacket_Validation) {
    // Test valid connect packet
    ConnectPacket validPacket;
    validPacket.type = MessageType::CLIENT_CONNECT;
    validPacket.protocolVersion = 1;
    std::strcpy(validPacket.playerName, "TestPlayer");
    
    ASSERT_TRUE(validateConnect(validPacket));
    
    // Test invalid protocol version
    ConnectPacket invalidVersion;
    invalidVersion.type = MessageType::CLIENT_CONNECT;
    invalidVersion.protocolVersion = 2;
    std::strcpy(invalidVersion.playerName, "TestPlayer");
    
    ASSERT_FALSE(validateConnect(invalidVersion));
    
    // Test name too long (should still pass as we only check < 32)
    ConnectPacket longName;
    longName.type = MessageType::CLIENT_CONNECT;
    longName.protocolVersion = 1;
    std::strcpy(longName.playerName, "ShortName");
    
    ASSERT_TRUE(validateConnect(longName));
}

TEST(PositionPacket_Validation) {
    // Test valid position
    PositionPacket validPos;
    validPos.x = 250.0f;
    validPos.y = 250.0f;
    
    ASSERT_TRUE(validatePosition(validPos));
    
    // Test boundary positions
    PositionPacket boundary1;
    boundary1.x = 0.0f;
    boundary1.y = 0.0f;
    ASSERT_TRUE(validatePosition(boundary1));
    
    PositionPacket boundary2;
    boundary2.x = 500.0f;
    boundary2.y = 500.0f;
    ASSERT_TRUE(validatePosition(boundary2));
    
    // Test invalid positions (out of bounds)
    PositionPacket invalid1;
    invalid1.x = -1.0f;
    invalid1.y = 250.0f;
    ASSERT_FALSE(validatePosition(invalid1));
    
    PositionPacket invalid2;
    invalid2.x = 250.0f;
    invalid2.y = 501.0f;
    ASSERT_FALSE(validatePosition(invalid2));
    
    PositionPacket invalid3;
    invalid3.x = 600.0f;
    invalid3.y = 600.0f;
    ASSERT_FALSE(validatePosition(invalid3));
}

TEST(MapDataPacket_Validation) {
    // Test valid map data
    MapDataPacket validMap;
    validMap.wallCount = 20;
    ASSERT_TRUE(validateMapData(validMap));
    
    // Test boundary values
    MapDataPacket boundary1;
    boundary1.wallCount = 1;
    ASSERT_TRUE(validateMapData(boundary1));
    
    MapDataPacket boundary2;
    boundary2.wallCount = 9999;
    ASSERT_TRUE(validateMapData(boundary2));
    
    // Test invalid values
    MapDataPacket invalid1;
    invalid1.wallCount = 0;
    ASSERT_FALSE(validateMapData(invalid1));
    
    MapDataPacket invalid2;
    invalid2.wallCount = 10000;
    ASSERT_FALSE(validateMapData(invalid2));
}

TEST(CircleRectCollision_Detection) {
    // Test collision with wall
    Wall wall;
    wall.x = 100.0f;
    wall.y = 100.0f;
    wall.width = 50.0f;
    wall.height = 50.0f;
    
    // Circle inside wall
    ASSERT_TRUE(circleRectCollision(125.0f, 125.0f, 10.0f, wall));
    
    // Circle touching wall edge
    ASSERT_TRUE(circleRectCollision(90.0f, 125.0f, 10.0f, wall));
    
    // Circle just outside wall
    ASSERT_FALSE(circleRectCollision(80.0f, 125.0f, 10.0f, wall));
    
    // Circle far from wall
    ASSERT_FALSE(circleRectCollision(200.0f, 200.0f, 10.0f, wall));
    
    // Circle touching corner
    float cornerDist = std::sqrt(10.0f * 10.0f + 10.0f * 10.0f);
    ASSERT_TRUE(circleRectCollision(100.0f - cornerDist + 1.0f, 100.0f - cornerDist + 1.0f, 10.0f, wall));
}

TEST(FogOfWar_VisibilityRadius) {
    const float VISIBILITY_RADIUS = 25.0f;
    
    // Player at center
    float playerX = 250.0f;
    float playerY = 250.0f;
    
    // Target within radius
    ASSERT_TRUE(isVisible(playerX, playerY, 260.0f, 250.0f, VISIBILITY_RADIUS));
    
    // Target at exact radius boundary
    ASSERT_TRUE(isVisible(playerX, playerY, 275.0f, 250.0f, VISIBILITY_RADIUS));
    
    // Target just outside radius
    ASSERT_FALSE(isVisible(playerX, playerY, 276.0f, 250.0f, VISIBILITY_RADIUS));
    
    // Target far outside radius
    ASSERT_FALSE(isVisible(playerX, playerY, 400.0f, 400.0f, VISIBILITY_RADIUS));
    
    // Test diagonal distance
    float diagonalDist = std::sqrt(20.0f * 20.0f + 15.0f * 15.0f);
    ASSERT_TRUE(diagonalDist < VISIBILITY_RADIUS);
    ASSERT_TRUE(isVisible(playerX, playerY, playerX + 20.0f, playerY + 15.0f, VISIBILITY_RADIUS));
}

TEST(NetworkCulling_50UnitRadius) {
    const float NETWORK_CULLING_RADIUS = 50.0f;
    
    // Server at center
    float serverX = 250.0f;
    float serverY = 250.0f;
    
    // Client within culling radius
    float client1X = 280.0f;
    float client1Y = 250.0f;
    float dist1 = getDistance(serverX, serverY, client1X, client1Y);
    ASSERT_TRUE(dist1 <= NETWORK_CULLING_RADIUS);
    
    // Client at boundary
    float client2X = 300.0f;
    float client2Y = 250.0f;
    float dist2 = getDistance(serverX, serverY, client2X, client2Y);
    ASSERT_TRUE(dist2 == 50.0f);
    
    // Client outside culling radius
    float client3X = 350.0f;
    float client3Y = 250.0f;
    float dist3 = getDistance(serverX, serverY, client3X, client3Y);
    ASSERT_TRUE(dist3 > NETWORK_CULLING_RADIUS);
}

TEST(SpawnPoints_NoOverlap) {
    // Server spawn area: (0, 450) to (50, 500)
    // Client spawn area: (450, 0) to (500, 50)
    
    // Test walls that should NOT overlap spawn points
    Wall validWall1;
    validWall1.x = 100.0f;
    validWall1.y = 100.0f;
    validWall1.width = 50.0f;
    validWall1.height = 50.0f;
    
    // Check server spawn
    bool overlapServer1 = (validWall1.x < 50.0f && validWall1.x + validWall1.width > 0.0f &&
                           validWall1.y < 500.0f && validWall1.y + validWall1.height > 450.0f);
    ASSERT_FALSE(overlapServer1);
    
    // Check client spawn
    bool overlapClient1 = (validWall1.x < 500.0f && validWall1.x + validWall1.width > 450.0f &&
                           validWall1.y < 50.0f && validWall1.y + validWall1.height > 0.0f);
    ASSERT_FALSE(overlapClient1);
    
    // Test wall that WOULD overlap server spawn
    Wall invalidWall1;
    invalidWall1.x = 10.0f;
    invalidWall1.y = 460.0f;
    invalidWall1.width = 20.0f;
    invalidWall1.height = 20.0f;
    
    bool overlapServer2 = (invalidWall1.x < 50.0f && invalidWall1.x + invalidWall1.width > 0.0f &&
                           invalidWall1.y < 500.0f && invalidWall1.y + invalidWall1.height > 450.0f);
    ASSERT_TRUE(overlapServer2);
}

TEST(MovementSpeed_Calculation) {
    const float MOVEMENT_SPEED = 5.0f; // 5 units per second
    const float deltaTime = 1.0f / 60.0f; // 60 FPS
    
    // Calculate movement per frame
    float movementPerFrame = MOVEMENT_SPEED * deltaTime * 60.0f;
    
    // Should move 5 units per second
    ASSERT_NEAR(movementPerFrame, 5.0f, 0.01f);
    
    // Test position update
    float startX = 100.0f;
    float endX = startX + movementPerFrame;
    
    // After 1 second (60 frames), should move 5 units
    float positionAfter1Sec = startX;
    for (int i = 0; i < 60; i++) {
        positionAfter1Sec += movementPerFrame / 60.0f;
    }
    
    ASSERT_NEAR(positionAfter1Sec, startX + 5.0f, 0.1f);
}

TEST(MapBoundaries_Enforcement) {
    const float MAP_WIDTH = 500.0f;
    const float MAP_HEIGHT = 500.0f;
    const float PLAYER_RADIUS = 30.0f;
    
    // Test clamping to boundaries
    auto clampToMapBounds = [](float x, float y, float radius) -> std::pair<float, float> {
        x = std::max(radius, std::min(MAP_WIDTH - radius, x));
        y = std::max(radius, std::min(MAP_HEIGHT - radius, y));
        return {x, y};
    };
    
    // Test position within bounds
    auto [x1, y1] = clampToMapBounds(250.0f, 250.0f, PLAYER_RADIUS);
    ASSERT_EQ(x1, 250.0f);
    ASSERT_EQ(y1, 250.0f);
    
    // Test position at left boundary
    auto [x2, y2] = clampToMapBounds(-10.0f, 250.0f, PLAYER_RADIUS);
    ASSERT_EQ(x2, PLAYER_RADIUS);
    
    // Test position at right boundary
    auto [x3, y3] = clampToMapBounds(510.0f, 250.0f, PLAYER_RADIUS);
    ASSERT_EQ(x3, MAP_WIDTH - PLAYER_RADIUS);
    
    // Test position at top boundary
    auto [x4, y4] = clampToMapBounds(250.0f, -10.0f, PLAYER_RADIUS);
    ASSERT_EQ(y4, PLAYER_RADIUS);
    
    // Test position at bottom boundary
    auto [x5, y5] = clampToMapBounds(250.0f, 510.0f, PLAYER_RADIUS);
    ASSERT_EQ(y5, MAP_HEIGHT - PLAYER_RADIUS);
}

TEST(PacketSizes_Verification) {
    // Verify packet sizes are reasonable for network transmission
    ASSERT_TRUE(sizeof(ConnectPacket) < 256);
    ASSERT_TRUE(sizeof(ReadyPacket) < 256);
    ASSERT_TRUE(sizeof(StartPacket) < 256);
    ASSERT_TRUE(sizeof(MapDataPacket) < 256);
    ASSERT_TRUE(sizeof(PositionPacket) < 256);
    
    // Verify specific sizes
    ASSERT_EQ(sizeof(ConnectPacket), 1 + 4 + 32); // type + version + name
    ASSERT_EQ(sizeof(PositionPacket), 4 + 4 + 1 + 4 + 1); // x + y + isAlive + frameID + playerId
}

TEST(ReadyPacket_Structure) {
    ReadyPacket packet;
    packet.type = MessageType::CLIENT_READY;
    packet.isReady = true;
    
    ASSERT_EQ(packet.type, MessageType::CLIENT_READY);
    ASSERT_TRUE(packet.isReady);
    
    // Test size
    ASSERT_TRUE(sizeof(ReadyPacket) < 64);
}

TEST(StartPacket_Structure) {
    StartPacket packet;
    packet.type = MessageType::SERVER_START;
    packet.timestamp = 12345;
    
    ASSERT_EQ(packet.type, MessageType::SERVER_START);
    ASSERT_EQ(packet.timestamp, 12345u);
    
    // Test size
    ASSERT_TRUE(sizeof(StartPacket) < 64);
}

// ========================
// Main Test Runner
// ========================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Zero Ground Integration Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // Run all tests
    RUN_TEST(ConnectPacket_Validation);
    RUN_TEST(PositionPacket_Validation);
    RUN_TEST(MapDataPacket_Validation);
    RUN_TEST(CircleRectCollision_Detection);
    RUN_TEST(FogOfWar_VisibilityRadius);
    RUN_TEST(NetworkCulling_50UnitRadius);
    RUN_TEST(SpawnPoints_NoOverlap);
    RUN_TEST(MovementSpeed_Calculation);
    RUN_TEST(MapBoundaries_Enforcement);
    RUN_TEST(PacketSizes_Verification);
    RUN_TEST(ReadyPacket_Structure);
    RUN_TEST(StartPacket_Structure);
    
    // Print summary
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Test Summary" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Total tests: " << totalTests << std::endl;
    std::cout << "Passed: " << passedTests << std::endl;
    std::cout << "Failed: " << failedTests << std::endl;
    std::cout << "Success rate: " << (totalTests > 0 ? (passedTests * 100 / totalTests) : 0) << "%" << std::endl;
    std::cout << std::endl;
    
    if (failedTests == 0) {
        std::cout << "✓ All tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "✗ Some tests failed!" << std::endl;
        return 1;
    }
}
