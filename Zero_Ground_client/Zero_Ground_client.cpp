#include <SFML/Network.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <thread>
#include <mutex>
#include <sstream>
#include <memory>
#include <vector>
#include <cstring>
#include <queue>

// ========================
// Constants for the new cell-based map system
// ========================
const float MAP_SIZE = 5010.0f;
const float CELL_SIZE = 30.0f;
const int GRID_SIZE = 167;  // 5010 / 30 = 167
const float PLAYER_SIZE = 10.0f;
const float WALL_WIDTH = 12.0f;
const float WALL_LENGTH = 30.0f;

// ========================
// Cell structure for grid-based map
// ========================
// Each cell can have walls on its four sides
// Walls are centered on cell boundaries (WALL_WIDTH/2 on each side)
struct Cell {
    bool topWall = false;
    bool rightWall = false;
    bool bottomWall = false;
    bool leftWall = false;
};

enum class ClientState { ConnectScreen, Connected, WaitingForStart, MainScreen, ErrorScreen, ConnectionLost };

// ========================
// Basic Data Structures
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

struct Player {
    uint32_t id = 0;
    sf::IpAddress ipAddress;
    float x = 0.0f;
    float y = 0.0f;
    float previousX = 0.0f;
    float previousY = 0.0f;
    float health = 100.0f;
    int score = 0;
    bool isAlive = true;
    bool isReady = false;
    sf::Color color = sf::Color::Blue;
    
    float getInterpolatedX(float alpha) const {
        return previousX + (x - previousX) * alpha;
    }
    
    float getInterpolatedY(float alpha) const {
        return previousY + (y - previousY) * alpha;
    }
};

// Forward declaration
struct Quadtree;

struct GameMap {
    std::vector<Wall> walls;
    const float width = 500.0f;
    const float height = 500.0f;
    std::unique_ptr<Quadtree> spatialIndex;
};

// ========================
// Quadtree for Spatial Partitioning
// ========================

struct Quadtree {
    sf::FloatRect bounds;
    std::vector<Wall*> walls;
    std::unique_ptr<Quadtree> children[4];
    const int MAX_WALLS = 10;
    const int MAX_DEPTH = 5;
    
    Quadtree(sf::FloatRect bounds) : bounds(bounds) {}
    
    // Insert a wall into the quadtree
    void insert(Wall* wall, int depth = 0) {
        // Check if wall intersects this node's bounds
        sf::FloatRect wallBounds(wall->x, wall->y, wall->width, wall->height);
        if (!bounds.intersects(wallBounds)) {
            return;
        }
        
        // If we haven't subdivided yet and we're under limits, add to this node
        if (children[0] == nullptr) {
            walls.push_back(wall);
            
            // Check if we need to subdivide
            if (walls.size() > MAX_WALLS && depth < MAX_DEPTH) {
                subdivide();
                
                // Redistribute walls to children
                std::vector<Wall*> remainingWalls;
                for (Wall* w : walls) {
                    bool addedToChild = false;
                    for (int i = 0; i < 4; ++i) {
                        sf::FloatRect wBounds(w->x, w->y, w->width, w->height);
                        if (children[i]->bounds.intersects(wBounds)) {
                            children[i]->insert(w, depth + 1);
                            addedToChild = true;
                        }
                    }
                    // Keep walls that span multiple children in parent
                    if (!addedToChild) {
                        remainingWalls.push_back(w);
                    }
                }
                walls = remainingWalls;
            }
        } else {
            // Already subdivided, try to insert into children
            bool addedToChild = false;
            for (int i = 0; i < 4; ++i) {
                if (children[i]->bounds.intersects(wallBounds)) {
                    children[i]->insert(wall, depth + 1);
                    addedToChild = true;
                }
            }
            // If wall spans multiple children, keep it in parent
            if (!addedToChild) {
                walls.push_back(wall);
            }
        }
    }
    
    // Query walls in a given area
    std::vector<Wall*> query(sf::FloatRect area) const {
        std::vector<Wall*> result;
        
        // Check if area intersects this node
        if (!bounds.intersects(area)) {
            return result;
        }
        
        // Add walls from this node that intersect the area
        for (Wall* wall : walls) {
            sf::FloatRect wallBounds(wall->x, wall->y, wall->width, wall->height);
            if (area.intersects(wallBounds)) {
                result.push_back(wall);
            }
        }
        
        // Query children if they exist
        if (children[0] != nullptr) {
            for (int i = 0; i < 4; ++i) {
                std::vector<Wall*> childResults = children[i]->query(area);
                result.insert(result.end(), childResults.begin(), childResults.end());
            }
        }
        
        return result;
    }
    
private:
    void subdivide() {
        float halfWidth = bounds.width / 2.0f;
        float halfHeight = bounds.height / 2.0f;
        float x = bounds.left;
        float y = bounds.top;
        
        // Create four children: top-left, top-right, bottom-left, bottom-right
        children[0] = std::make_unique<Quadtree>(sf::FloatRect(x, y, halfWidth, halfHeight));
        children[1] = std::make_unique<Quadtree>(sf::FloatRect(x + halfWidth, y, halfWidth, halfHeight));
        children[2] = std::make_unique<Quadtree>(sf::FloatRect(x, y + halfHeight, halfWidth, halfHeight));
        children[3] = std::make_unique<Quadtree>(sf::FloatRect(x + halfWidth, y + halfHeight, halfWidth, halfHeight));
    }
};

// ========================
// Network Protocol
// ========================

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
    // Followed by wallCount * Wall structures
};

struct PositionPacket {
    float x = 0.0f;
    float y = 0.0f;
    bool isAlive = true;
    uint32_t frameID = 0;
    uint8_t playerId = 0;
};

// ========================
// Error Handling and Logging Functions
// ========================

class ErrorHandler {
public:
    // Handle invalid packet errors
    static void handleInvalidPacket(const std::string& reason, const std::string& source = "") {
        std::cerr << "[ERROR] Invalid packet received";
        if (!source.empty()) {
            std::cerr << " from " << source;
        }
        std::cerr << ": " << reason << std::endl;
        std::cerr << "[INFO] Packet discarded, continuing operation" << std::endl;
    }
    
    // Handle connection loss
    static void handleConnectionLost(const std::string& serverIP) {
        std::cerr << "[ERROR] Connection lost with server: " << serverIP << std::endl;
        std::cerr << "[INFO] Displaying reconnection screen" << std::endl;
    }
    
    // Log network errors
    static void logNetworkError(const std::string& operation, const std::string& details = "") {
        std::cerr << "[NETWORK ERROR] Operation: " << operation;
        if (!details.empty()) {
            std::cerr << " - Details: " << details;
        }
        std::cerr << std::endl;
    }
    
    // Log TCP errors
    static void logTCPError(const std::string& operation, sf::Socket::Status status, const std::string& serverIP = "") {
        std::cerr << "[TCP ERROR] Operation: " << operation;
        if (!serverIP.empty()) {
            std::cerr << " - Server: " << serverIP;
        }
        std::cerr << " - Status: ";
        
        switch (status) {
            case sf::Socket::Done:
                std::cerr << "Done (unexpected in error handler)";
                break;
            case sf::Socket::NotReady:
                std::cerr << "Not Ready";
                break;
            case sf::Socket::Partial:
                std::cerr << "Partial";
                break;
            case sf::Socket::Disconnected:
                std::cerr << "Disconnected";
                break;
            case sf::Socket::Error:
                std::cerr << "Error";
                break;
            default:
                std::cerr << "Unknown";
                break;
        }
        std::cerr << std::endl;
    }
    
    // Log UDP errors
    static void logUDPError(const std::string& operation, const std::string& details = "") {
        std::cerr << "[UDP ERROR] Operation: " << operation;
        if (!details.empty()) {
            std::cerr << " - Details: " << details;
        }
        std::cerr << std::endl;
    }
    
    // Log general info
    static void logInfo(const std::string& message) {
        std::cout << "[INFO] " << message << std::endl;
    }
    
    // Log warnings
    static void logWarning(const std::string& message) {
        std::cerr << "[WARNING] " << message << std::endl;
    }
};

// ========================
// Packet Validation Functions
// ========================

bool validatePosition(const PositionPacket& packet) {
    bool valid = packet.x >= 0.0f && packet.x <= MAP_SIZE &&
                 packet.y >= 0.0f && packet.y <= MAP_SIZE;
    
    if (!valid) {
        std::ostringstream oss;
        oss << "Position out of bounds: (" << packet.x << ", " << packet.y << ")";
        ErrorHandler::handleInvalidPacket(oss.str());
    }
    
    return valid;
}

bool validateMapData(const MapDataPacket& packet) {
    bool valid = packet.wallCount > 0 && packet.wallCount < 10000;
    
    if (!valid) {
        std::ostringstream oss;
        oss << "Invalid wall count: " << packet.wallCount;
        ErrorHandler::handleInvalidPacket(oss.str());
    }
    
    return valid;
}

bool validateConnect(const ConnectPacket& packet) {
    bool valid = packet.protocolVersion == 1 &&
                 std::strlen(packet.playerName) < 32;
    
    if (!valid) {
        std::ostringstream oss;
        oss << "Invalid connect packet - Protocol version: " << packet.protocolVersion 
            << ", Name length: " << std::strlen(packet.playerName);
        ErrorHandler::handleInvalidPacket(oss.str());
    }
    
    return valid;
}

// ========================
// Collision Detection System
// ========================

// Check if a circle collides with a rectangle
bool circleRectCollision(sf::Vector2f center, float radius, const Wall& wall) {
    // Find the closest point on the rectangle to the circle center
    float closestX = std::max(wall.x, std::min(center.x, wall.x + wall.width));
    float closestY = std::max(wall.y, std::min(center.y, wall.y + wall.height));
    
    float dx = center.x - closestX;
    float dy = center.y - closestY;
    
    return (dx * dx + dy * dy) < (radius * radius);
}

// Resolve collision and return corrected position
sf::Vector2f resolveCollision(sf::Vector2f oldPos, sf::Vector2f newPos, float radius, const GameMap& map) {
    // Query nearby walls using quadtree
    sf::FloatRect queryArea(
        newPos.x - radius - 1.0f,
        newPos.y - radius - 1.0f,
        radius * 2 + 2.0f,
        radius * 2 + 2.0f
    );
    
    if (map.spatialIndex) {
        auto nearbyWalls = map.spatialIndex->query(queryArea);
        
        for (const Wall* wall : nearbyWalls) {
            if (circleRectCollision(newPos, radius, *wall)) {
                // Calculate penetration depth and push back
                float closestX = std::max(wall->x, std::min(newPos.x, wall->x + wall->width));
                float closestY = std::max(wall->y, std::min(newPos.y, wall->y + wall->height));
                
                float dx = newPos.x - closestX;
                float dy = newPos.y - closestY;
                float distance = std::sqrt(dx * dx + dy * dy);
                
                if (distance < 0.001f) {
                    // Player is exactly on the wall edge, use old position
                    return oldPos;
                }
                
                // Calculate how much to push back (radius - distance)
                float penetration = radius - distance;
                float pushX = (dx / distance) * penetration;
                float pushY = (dy / distance) * penetration;
                
                return sf::Vector2f(newPos.x + pushX, newPos.y + pushY);
            }
        }
    }
    
    return newPos; // No collision
}

// Clamp position to map boundaries [0, MAP_SIZE]
// NOTE: This function is deprecated - collision system now handles boundary clamping
// Kept for backward compatibility but not used in new cell-based system
sf::Vector2f clampToMapBounds(sf::Vector2f pos, float radius) {
    pos.x = std::max(radius, std::min(MAP_SIZE - radius, pos.x));
    pos.y = std::max(radius, std::min(MAP_SIZE - radius, pos.y));
    return pos;
}

// ========================
// Movement and Interpolation Constants
// ========================

const float MOVEMENT_SPEED = 3.0f; // 5 units per second

// Linear interpolation function for smooth position transitions
float lerp(float start, float end, float alpha) {
    return start + (end - start) * alpha;
}

sf::Vector2f lerpPosition(sf::Vector2f start, sf::Vector2f end, float alpha) {
    return sf::Vector2f(lerp(start.x, end.x, alpha), lerp(start.y, end.y, alpha));
}

// ========================
// Performance Monitoring System
// ========================

class PerformanceMonitor {
public:
    PerformanceMonitor() : frameCount_(0), elapsedTime_(0.0f), currentFPS_(0.0f) {}
    
    // Update performance metrics each frame
    void update(float deltaTime, size_t playerCount, size_t wallCount) {
        frameCount_++;
        elapsedTime_ += deltaTime;
        
        // Calculate FPS every 1 second window
        if (elapsedTime_ >= 1.0f) {
            currentFPS_ = frameCount_ / elapsedTime_;
            
            // Calculate frame time
            float frameTime = (elapsedTime_ / frameCount_) * 1000.0f;
            
            // Calculate game thread load (percentage of frame budget used)
            float gameThreadLoad = (frameTime / 16.67f) * 100.0f;
            
            // Estimate actual CPU usage (more conservative)
            float estimatedCPUUsage = (gameThreadLoad / 100.0f) * 40.0f;
            
            // Log performance metrics
            std::cout << "\n=== CLIENT PERFORMANCE METRICS ===" << std::endl;
            std::cout << "FPS: " << currentFPS_ << " (target: 55+)" << std::endl;
            std::cout << "Frame Time: " << frameTime << "ms" << std::endl;
            std::cout << "Players: " << playerCount << std::endl;
            std::cout << "Walls: " << wallCount << std::endl;
            std::cout << "Game Thread Load: " << gameThreadLoad << "% of frame budget" << std::endl;
            std::cout << "Estimated CPU Usage: " << estimatedCPUUsage << "% (target: <40%)" << std::endl;
            
            // Add build type hint if performance is lower
            if (gameThreadLoad > 100.0f) {
                std::cout << "Note: Running Debug build? Release build typically 30-50% faster" << std::endl;
            }
            
            std::cout << "====================================\n" << std::endl;
            
            // Log warning if FPS drops below 55
            if (currentFPS_ < 55.0f) {
                logPerformanceWarning(playerCount, wallCount, gameThreadLoad);
            }
            
            // Log warning if game thread load is too high
            if (gameThreadLoad > 110.0f) {
                std::cerr << "[WARNING] Game thread load exceeds frame budget: " 
                         << gameThreadLoad << "%" << std::endl;
                std::cerr << "  This means frames are taking longer than 16.67ms" << std::endl;
                std::cerr << "  Consider optimizing or using Release build" << std::endl;
            }
            
            // Reset counters for next window
            frameCount_ = 0;
            elapsedTime_ = 0.0f;
        }
    }
    
    // Get current FPS
    float getCurrentFPS() const {
        return currentFPS_;
    }
    
private:
    void logPerformanceWarning(size_t playerCount, size_t wallCount, float gameThreadLoad) {
        std::cerr << "[WARNING] Performance degradation detected!" << std::endl;
        std::cerr << "  FPS: " << currentFPS_ << " (target: 55+)" << std::endl;
        std::cerr << "  Players: " << playerCount << std::endl;
        std::cerr << "  Walls: " << wallCount << std::endl;
        
        float frameTime = (elapsedTime_ / frameCount_) * 1000.0f;
        std::cerr << "  Avg Frame Time: " << frameTime << "ms (target: 16.67ms)" << std::endl;
        std::cerr << "  Game Thread Load: " << gameThreadLoad << "% of frame budget" << std::endl;
        
        // Provide helpful suggestions
        if (gameThreadLoad > 100.0f) {
            std::cerr << "  Suggestion: Try Release build for better performance" << std::endl;
        }
    }
    
    int frameCount_;
    float elapsedTime_;
    float currentFPS_;
};

// ========================
// Fog of War System
// ========================

const float VISIBILITY_RADIUS = 50.0f; // 50 units visibility radius

// Calculate distance between two points
float getDistance(sf::Vector2f a, sf::Vector2f b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// Check if a point is within visibility radius
bool isVisible(sf::Vector2f playerPos, sf::Vector2f targetPos, float radius) {
    return getDistance(playerPos, targetPos) <= radius;
}

// Render fog of war effect
void renderFogOfWar(sf::RenderWindow& window, sf::Vector2f playerPos, 
                    const std::vector<Wall>& walls, sf::Vector2f serverPos, 
                    bool serverConnected, float scaleX, float scaleY) {
    
    // Draw all walls with fog effect
    for (const auto& wall : walls) {
        sf::RectangleShape wallShape(sf::Vector2f(wall.width * scaleX, wall.height * scaleY));
        wallShape.setPosition(wall.x * scaleX, wall.y * scaleY);
        
        // Calculate distance from player to wall center
        sf::Vector2f wallCenter(wall.x + wall.width / 2.0f, wall.y + wall.height / 2.0f);
        float distance = getDistance(playerPos, wallCenter);
        
        // Apply darkening effect if outside visibility radius
        if (distance > VISIBILITY_RADIUS) {
            wallShape.setFillColor(sf::Color(100, 100, 100)); // Darkened gray
        } else {
            wallShape.setFillColor(sf::Color(150, 150, 150)); // Normal gray
        }
        
        // Add outline to see exact hitbox
        wallShape.setOutlineColor(sf::Color(200, 200, 200));
        wallShape.setOutlineThickness(1.0f);
        
        window.draw(wallShape);
    }
    
    // Draw server player only if within visibility radius
    if (serverConnected && isVisible(playerPos, serverPos, VISIBILITY_RADIUS)) {
        sf::CircleShape serverCircle(10.0f);
        serverCircle.setFillColor(sf::Color(0, 200, 0, 200));
        serverCircle.setOutlineColor(sf::Color(0, 100, 0));
        serverCircle.setOutlineThickness(2.0f);
        serverCircle.setPosition(
            serverPos.x * scaleX - 10.0f,
            serverPos.y * scaleY - 10.0f
        );
        window.draw(serverCircle);
    }
}

// Apply fog overlay with circular cutout around player
void applyFogOverlay(sf::RenderWindow& window, sf::Vector2f playerScreenPos, float scaleX, float scaleY) {
    sf::Vector2u windowSize = window.getSize();
    
    // Create vertex array for fog overlay with circular cutout
    // We'll use a simple approach: draw multiple rectangles to simulate fog
    // For a more sophisticated approach, a shader would be ideal
    
    const int segments = 32; // Number of segments for circular approximation
    const float visibilityRadiusScreen = VISIBILITY_RADIUS * std::min(scaleX, scaleY);
    
    // Create a render texture for the fog mask
    sf::RenderTexture fogTexture;
    if (!fogTexture.create(windowSize.x, windowSize.y)) {
        std::cerr << "[ERROR] Failed to create fog render texture" << std::endl;
        return;
    }
    
    // Clear with transparent
    fogTexture.clear(sf::Color::Transparent);
    
    // Draw full-screen fog
    sf::RectangleShape fullFog(sf::Vector2f(windowSize.x, windowSize.y));
    fullFog.setFillColor(sf::Color(0, 0, 0, 200)); // Black with alpha 200
    fogTexture.draw(fullFog);
    
    // Draw clear circle around player using blend mode
    sf::CircleShape clearCircle(visibilityRadiusScreen);
    clearCircle.setOrigin(visibilityRadiusScreen, visibilityRadiusScreen);
    clearCircle.setPosition(playerScreenPos);
    clearCircle.setFillColor(sf::Color(0, 0, 0, 0)); // Fully transparent
    
    // Use subtract blend mode to create cutout
    sf::RenderStates states;
    states.blendMode = sf::BlendMode(
        sf::BlendMode::Zero,           // Source factor
        sf::BlendMode::One,            // Destination factor
        sf::BlendMode::ReverseSubtract // Blend equation
    );
    
    fogTexture.draw(clearCircle, states);
    fogTexture.display();
    
    // Draw the fog texture to the window
    sf::Sprite fogSprite(fogTexture.getTexture());
    window.draw(fogSprite);
}

// ========================
// NEW: Dynamic Camera System
// ========================

// Update camera to follow the player
// The camera keeps the player centered on screen while respecting map boundaries
//
// CAMERA BEHAVIOR:
// - Player is always centered on screen (when not near edges)
// - Camera smoothly follows player movement
// - Camera stops at map edges to prevent showing out-of-bounds areas
//
// BOUNDARY CLAMPING:
// When the player is near the edge of the map, we need to prevent the camera
// from showing areas outside the map (which would appear as black space).
// We do this by clamping the camera center to stay within valid bounds:
// - Minimum X: halfWidth (so left edge of view doesn't go below 0)
// - Maximum X: MAP_SIZE - halfWidth (so right edge doesn't exceed MAP_SIZE)
// - Same logic applies for Y axis
//
// This creates a smooth camera that follows the player everywhere except
// at the map edges, where it stops to prevent showing out-of-bounds areas.
void updateCamera(sf::RenderWindow& window, sf::Vector2f playerPosition) {
    // Create a view with the window's dimensions
    sf::View view;
    view.setSize(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y));
    
    // Calculate half dimensions for boundary clamping
    float halfWidth = window.getSize().x / 2.0f;
    float halfHeight = window.getSize().y / 2.0f;
    
    // Start with player position as center
    sf::Vector2f center = playerPosition;
    
    // Clamp camera center to map boundaries
    // This prevents the camera from showing areas outside the map
    center.x = std::max(halfWidth, std::min(center.x, MAP_SIZE - halfWidth));
    center.y = std::max(halfHeight, std::min(center.y, MAP_SIZE - halfHeight));
    
    // Set the clamped center
    view.setCenter(center);
    
    // Apply the view to the window
    // All subsequent draw calls will use this view's coordinate system
    window.setView(view);
}

// ========================
// NEW: Optimized Visible Wall Rendering with Fog of War
// ========================

// Render only visible walls around the player with fog of war effect
// This function implements:
// 1. Cell-based visibility culling (only render walls within ~10 cells)
// 2. Caching of visible bounds (recalculate only when player changes cell)
// 3. Fog of war darkening for walls outside visibility radius
// 4. Debug output for rendered wall count
//
// PERFORMANCE OPTIMIZATION:
// - Static variables cache the last player cell and visible bounds
// - Bounds are recalculated only when player moves to a different cell
// - This reduces computation from every frame to only when needed
//
// WALL POSITIONING:
// - Walls are centered on cell boundaries
// - topWall: centered on top edge (y - WALL_WIDTH/2)
// - rightWall: centered on right edge (x + CELL_SIZE - WALL_WIDTH/2)
// - bottomWall: centered on bottom edge (y + CELL_SIZE - WALL_WIDTH/2)
// - leftWall: centered on left edge (x - WALL_WIDTH/2)
//
// FOG OF WAR:
// - Walls within VISIBILITY_RADIUS: normal gray (150, 150, 150)
// - Walls outside VISIBILITY_RADIUS: darkened gray (100, 100, 100)
void renderVisibleWalls(sf::RenderWindow& window, sf::Vector2f playerPosition, 
                       const std::vector<std::vector<Cell>>& grid) {
    // Calculate current player cell
    int playerCellX = static_cast<int>(playerPosition.x / CELL_SIZE);
    int playerCellY = static_cast<int>(playerPosition.y / CELL_SIZE);
    
    // Cache visible bounds to avoid recalculating every frame
    // These static variables persist between function calls
    static int lastPlayerCellX = -1;
    static int lastPlayerCellY = -1;
    static int startX, startY, endX, endY;
    
    // Recalculate bounds only if player moved to a different cell
    if (playerCellX != lastPlayerCellX || playerCellY != lastPlayerCellY) {
        // Calculate visible area: 10 cells in each direction (~300 pixels)
        startX = std::max(0, playerCellX - 10);
        startY = std::max(0, playerCellY - 10);
        endX = std::min(GRID_SIZE - 1, playerCellX + 10);
        endY = std::min(GRID_SIZE - 1, playerCellY + 10);
        
        // Update cached cell position
        lastPlayerCellX = playerCellX;
        lastPlayerCellY = playerCellY;
    }
    
    int wallCount = 0;
    
    // Iterate through visible cells and render their walls
    for (int i = startX; i <= endX; i++) {
        for (int j = startY; j <= endY; j++) {
            // Calculate cell position in world coordinates
            float x = i * CELL_SIZE;
            float y = j * CELL_SIZE;
            
            // Calculate cell center for fog of war distance check
            sf::Vector2f cellCenter(x + CELL_SIZE / 2.0f, y + CELL_SIZE / 2.0f);
            float distanceToPlayer = getDistance(playerPosition, cellCenter);
            
            // Determine wall color based on visibility
            sf::Color wallColor;
            if (distanceToPlayer <= VISIBILITY_RADIUS) {
                wallColor = sf::Color(150, 150, 150); // Normal gray - visible
            } else {
                wallColor = sf::Color(100, 100, 100); // Darkened gray - fog of war
            }
            
            // Render topWall (horizontal wall on top edge of cell)
            // Wall is centered on the boundary: 6px above, 6px below
            if (grid[i][j].topWall) {
                sf::RectangleShape wall(sf::Vector2f(WALL_LENGTH, WALL_WIDTH));
                wall.setPosition(x, y - WALL_WIDTH / 2.0f);
                wall.setFillColor(wallColor);
                window.draw(wall);
                wallCount++;
            }
            
            // Render rightWall (vertical wall on right edge of cell)
            // Wall is centered on the boundary: 6px left, 6px right
            if (grid[i][j].rightWall) {
                sf::RectangleShape wall(sf::Vector2f(WALL_WIDTH, WALL_LENGTH));
                wall.setPosition(x + CELL_SIZE - WALL_WIDTH / 2.0f, y);
                wall.setFillColor(wallColor);
                window.draw(wall);
                wallCount++;
            }
            
            // Render bottomWall (horizontal wall on bottom edge of cell)
            // Wall is centered on the boundary: 6px above, 6px below
            if (grid[i][j].bottomWall) {
                sf::RectangleShape wall(sf::Vector2f(WALL_LENGTH, WALL_WIDTH));
                wall.setPosition(x, y + CELL_SIZE - WALL_WIDTH / 2.0f);
                wall.setFillColor(wallColor);
                window.draw(wall);
                wallCount++;
            }
            
            // Render leftWall (vertical wall on left edge of cell)
            // Wall is centered on the boundary: 6px left, 6px right
            if (grid[i][j].leftWall) {
                sf::RectangleShape wall(sf::Vector2f(WALL_WIDTH, WALL_LENGTH));
                wall.setPosition(x - WALL_WIDTH / 2.0f, y);
                wall.setFillColor(wallColor);
                window.draw(wall);
                wallCount++;
            }
        }
    }
    
    // Debug output: log wall count in debug builds only
    #ifdef _DEBUG
    std::cout << "Walls rendered: " << wallCount << std::endl;
    #endif
}

// ========================
// NEW: Cell-Based Collision System
// ========================

// Resolve collision using cell-based grid system
// This function checks for collisions between the player and walls in nearby cells.
//
// ALGORITHM:
// 1. Create player bounding box at new position
// 2. Calculate which cell the player is in
// 3. Check walls in 3×3 grid of cells around player (±1 cell radius)
// 4. If collision detected, push player back 1 pixel toward old position
// 5. Clamp position to map boundaries
//
// COLLISION DETECTION:
// - Uses AABB (Axis-Aligned Bounding Box) intersection test
// - Player is a square: PLAYER_SIZE × PLAYER_SIZE (10×10 pixels)
// - Walls are rectangles centered on cell boundaries
// - Checks all 4 possible walls per cell (top, right, bottom, left)
//
// COLLISION RESPONSE:
// - Simple pushback: move 1 pixel in direction opposite to movement
// - This prevents player from getting stuck in walls
// - Maintains smooth movement while preventing wall penetration
//
// PERFORMANCE:
// - Only checks 3×3 = 9 cells maximum
// - Each cell has 0-4 walls to check
// - Typical case: ~10-15 wall checks (most cells don't have all 4 walls)
// - Target: < 0.1ms per collision check
sf::Vector2f resolveCollisionCellBased(sf::Vector2f oldPos, sf::Vector2f newPos, const std::vector<std::vector<Cell>>& grid) {
    // Step 1: Create bounding box for player at new position
    // Player is centered at newPos, so we offset by PLAYER_SIZE/2
    sf::FloatRect playerRect(
        newPos.x - PLAYER_SIZE / 2.0f,
        newPos.y - PLAYER_SIZE / 2.0f,
        PLAYER_SIZE,
        PLAYER_SIZE
    );
    
    // Step 2: Calculate which cell the player is in
    int playerCellX = static_cast<int>(newPos.x / CELL_SIZE);
    int playerCellY = static_cast<int>(newPos.y / CELL_SIZE);
    
    // Step 3: Calculate boundaries for checking nearby cells (±1 cell radius)
    // This creates a 3×3 grid of cells to check
    int startX = std::max(0, playerCellX - 1);
    int startY = std::max(0, playerCellY - 1);
    int endX = std::min(GRID_SIZE - 1, playerCellX + 1);
    int endY = std::min(GRID_SIZE - 1, playerCellY + 1);
    
    // Flag to track if any collision was detected
    bool collision = false;
    
    // Step 4: Iterate through nearby cells and check for wall collisions
    for (int i = startX; i <= endX; i++) {
        for (int j = startY; j <= endY; j++) {
            // Calculate world position of this cell's top-left corner
            float x = i * CELL_SIZE;
            float y = j * CELL_SIZE;
            
            // Check collision with topWall (horizontal wall on top boundary)
            // Wall is centered on boundary: position - WALL_WIDTH/2
            if (grid[i][j].topWall) {
                sf::FloatRect wallRect(x, y - WALL_WIDTH / 2.0f, WALL_LENGTH, WALL_WIDTH);
                if (playerRect.intersects(wallRect)) {
                    collision = true;
                    break;
                }
            }
            
            // Check collision with rightWall (vertical wall on right boundary)
            if (grid[i][j].rightWall) {
                sf::FloatRect wallRect(x + CELL_SIZE - WALL_WIDTH / 2.0f, y, WALL_WIDTH, WALL_LENGTH);
                if (playerRect.intersects(wallRect)) {
                    collision = true;
                    break;
                }
            }
            
            // Check collision with bottomWall (horizontal wall on bottom boundary)
            if (grid[i][j].bottomWall) {
                sf::FloatRect wallRect(x, y + CELL_SIZE - WALL_WIDTH / 2.0f, WALL_LENGTH, WALL_WIDTH);
                if (playerRect.intersects(wallRect)) {
                    collision = true;
                    break;
                }
            }
            
            // Check collision with leftWall (vertical wall on left boundary)
            if (grid[i][j].leftWall) {
                sf::FloatRect wallRect(x - WALL_WIDTH / 2.0f, y, WALL_WIDTH, WALL_LENGTH);
                if (playerRect.intersects(wallRect)) {
                    collision = true;
                    break;
                }
            }
        }
        
        // Break outer loop if collision detected
        if (collision) break;
    }
    
    // Step 5: Handle collision response
    if (collision) {
        // Calculate direction vector from new position back to old position
        sf::Vector2f direction = oldPos - newPos;
        
        // Calculate length of direction vector
        float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
        
        // Normalize direction vector (make it length 1) and push back 1 pixel
        if (length > 0.001f) {
            direction /= length;  // Normalize
            sf::Vector2f correctedPos = oldPos + direction * 1.0f;
            
            // Clamp to map boundaries
            correctedPos.x = std::max(PLAYER_SIZE / 2.0f, std::min(correctedPos.x, MAP_SIZE - PLAYER_SIZE / 2.0f));
            correctedPos.y = std::max(PLAYER_SIZE / 2.0f, std::min(correctedPos.y, MAP_SIZE - PLAYER_SIZE / 2.0f));
            
            return correctedPos;
        }
        
        // If direction vector is too small (player barely moved), just return old position
        return oldPos;
    }
    
    // Step 6: No collision detected, clamp new position to map boundaries and return
    newPos.x = std::max(PLAYER_SIZE / 2.0f, std::min(newPos.x, MAP_SIZE - PLAYER_SIZE / 2.0f));
    newPos.y = std::max(PLAYER_SIZE / 2.0f, std::min(newPos.y, MAP_SIZE - PLAYER_SIZE / 2.0f));
    
    return newPos;
}

std::mutex mutex;
Position clientPos = { 4750.0f, 250.0f }; // Client spawn position (top-right corner of 5000×5000 map)
Position clientPosPrevious = { 4750.0f, 250.0f }; // Previous position for interpolation
Position serverPos = { 250.0f, 4750.0f }; // Server spawn position (bottom-left corner of 5000×5000 map)
bool serverConnected = false;
std::string serverIP = "127.0.0.1";
GameMap clientGameMap; // Store map data received from server
std::unique_ptr<sf::TcpSocket> tcpSocket; // TCP socket for connection
std::string connectionMessage = "";
sf::Color connectionMessageColor = sf::Color::White;
bool udpRunning = true; // Flag to control UDP thread
sf::Clock lastPacketReceived; // Track last received packet for connection loss detection
uint32_t currentFrameID = 0; // Frame counter for position packets

// Grid for cell-based map system (global for easy access from handshake)
std::vector<std::vector<Cell>> grid(GRID_SIZE, std::vector<Cell>(GRID_SIZE));

// HUD variables
float clientHealth = 100.0f; // Client player health (0-100)
int clientScore = 0; // Client player score
bool clientIsAlive = true; // Client player alive status

// ========================
// Map Deserialization Functions
// ========================

// Deserialize map data from byte buffer into grid
// Parameters:
//   buffer - Byte buffer containing serialized grid data
//   grid - The grid to populate with deserialized data
//
// PROTOCOL:
// The buffer contains GRID_SIZE × GRID_SIZE Cell structures serialized row by row.
// Each Cell is sizeof(Cell) bytes (4 bytes for 4 booleans).
// Total size: 167 × 167 × 4 = 111,556 bytes (~109 KB)
//
// ALGORITHM:
// 1. Copy data from buffer into grid row by row
// 2. Use std::memcpy for efficient memory copy
// 3. Grid must be pre-allocated to GRID_SIZE × GRID_SIZE
void deserializeMap(const std::vector<char>& buffer, std::vector<std::vector<Cell>>& grid) {
    // Calculate expected size
    size_t expectedSize = GRID_SIZE * GRID_SIZE * sizeof(Cell);
    
    if (buffer.size() != expectedSize) {
        std::cerr << "[ERROR] Buffer size mismatch in deserializeMap: expected " 
                  << expectedSize << " bytes, got " << buffer.size() << " bytes" << std::endl;
        return;
    }
    
    // Copy buffer data into grid row by row
    size_t offset = 0;
    for (int i = 0; i < GRID_SIZE; i++) {
        std::memcpy(grid[i].data(), buffer.data() + offset, GRID_SIZE * sizeof(Cell));
        offset += GRID_SIZE * sizeof(Cell);
    }
    
    std::cout << "[INFO] Map deserialized: " << buffer.size() << " bytes (" 
              << (buffer.size() / 1024) << " KB)" << std::endl;
}

// Receive map data from server via TCP
// Parameters:
//   serverSocket - TCP socket connected to the server
//   grid - The grid to populate with received map data
//
// Returns: true if successful, false if any error occurred
//
// PROTOCOL:
// 1. Receive data size as uint32_t (4 bytes)
// 2. Receive serialized map data (variable size, ~109 KB)
// 3. Deserialize data into grid
//
// ERROR HANDLING:
// - Validates received data size
// - Logs errors using ErrorHandler
// - Returns false on any transmission error
//
// PERFORMANCE:
// - TCP ensures reliable delivery
// - Typical receive time: 10-50ms on LAN, 50-200ms on internet
// - Blocking operation: will wait until all data is received
bool receiveMapFromServer(sf::TcpSocket& serverSocket, std::vector<std::vector<Cell>>& grid) {
    std::cout << "[INFO] Waiting to receive map from server..." << std::endl;
    
    // Step 1: Receive the size of the data (4 bytes)
    uint32_t dataSize = 0;
    std::size_t received = 0;
    
    sf::Socket::Status sizeStatus = serverSocket.receive(&dataSize, sizeof(dataSize), received);
    if (sizeStatus != sf::Socket::Done) {
        ErrorHandler::logTCPError("Receive map data size", sizeStatus, 
                                 serverSocket.getRemoteAddress().toString());
        return false;
    }
    
    if (received != sizeof(dataSize)) {
        std::ostringstream oss;
        oss << "Map data size packet mismatch - expected " << sizeof(dataSize) 
            << " bytes, got " << received;
        ErrorHandler::handleInvalidPacket(oss.str(), serverSocket.getRemoteAddress().toString());
        return false;
    }
    
    std::cout << "[INFO] Map data size received: " << dataSize << " bytes" << std::endl;
    
    // Validate data size
    size_t expectedSize = GRID_SIZE * GRID_SIZE * sizeof(Cell);
    if (dataSize != expectedSize) {
        std::ostringstream oss;
        oss << "Invalid map data size - expected " << expectedSize 
            << " bytes, got " << dataSize;
        ErrorHandler::handleInvalidPacket(oss.str(), serverSocket.getRemoteAddress().toString());
        return false;
    }
    
    // Step 2: Receive the actual map data
    std::vector<char> mapData(dataSize);
    std::cout << "[INFO] Receiving map data..." << std::endl;
    
    sf::Socket::Status dataStatus = serverSocket.receive(mapData.data(), dataSize, received);
    if (dataStatus != sf::Socket::Done) {
        ErrorHandler::logTCPError("Receive map data", dataStatus, 
                                 serverSocket.getRemoteAddress().toString());
        return false;
    }
    
    if (received != dataSize) {
        std::ostringstream oss;
        oss << "Map data packet mismatch - expected " << dataSize 
            << " bytes, got " << received;
        ErrorHandler::handleInvalidPacket(oss.str(), serverSocket.getRemoteAddress().toString());
        return false;
    }
    
    std::cout << "[INFO] Map data received successfully" << std::endl;
    
    // Step 3: Deserialize the map data into grid
    deserializeMap(mapData, grid);
    
    std::cout << "[INFO] Map successfully received and deserialized from server" << std::endl;
    return true;
}

// Function to perform TCP connection and handshake
bool performTCPHandshake(const std::string& ip) {
    ErrorHandler::logInfo("=== Starting TCP Handshake ===");
    ErrorHandler::logInfo("Attempting TCP connection to " + ip + ":53000");
    
    // Create TCP socket
    tcpSocket = std::make_unique<sf::TcpSocket>();
    tcpSocket->setBlocking(true);
    
    ErrorHandler::logInfo("TCP socket created, attempting connection...");
    
    // Attempt to connect with 3 second timeout
    sf::Socket::Status status = tcpSocket->connect(ip, 53000, sf::seconds(3.0f));
    
    ErrorHandler::logInfo("Connection attempt completed with status: " + std::to_string(static_cast<int>(status)));
    
    if (status != sf::Socket::Done) {
        ErrorHandler::logTCPError("Connect to server", status, ip);
        ErrorHandler::logNetworkError("TCP Connection", "Failed to connect to " + ip + ":53000");
        connectionMessage = "THE SERVER IS UNAVAILABLE OR IP IS INVALID";
        connectionMessageColor = sf::Color::Red;
        tcpSocket.reset();
        return false;
    }
    
    ErrorHandler::logInfo("TCP connection established successfully!");
    
    // Send ConnectPacket
    ConnectPacket connectPacket;
    connectPacket.type = MessageType::CLIENT_CONNECT;
    connectPacket.protocolVersion = 1;
    strncpy_s(connectPacket.playerName, sizeof(connectPacket.playerName), "Client", _TRUNCATE);
    
    ErrorHandler::logInfo("Sending ConnectPacket to server...");
    sf::Socket::Status sendStatus = tcpSocket->send(&connectPacket, sizeof(ConnectPacket));
    ErrorHandler::logInfo("Send status: " + std::to_string(static_cast<int>(sendStatus)));
    
    if (sendStatus != sf::Socket::Done) {
        ErrorHandler::logTCPError("Send ConnectPacket", sendStatus, ip);
        connectionMessage = "Failed to send connection packet";
        connectionMessageColor = sf::Color::Red;
        tcpSocket.reset();
        return false;
    }
    
    ErrorHandler::logInfo("ConnectPacket sent successfully");
    
    // Receive grid-based map from server
    ErrorHandler::logInfo("Waiting to receive grid-based map from server...");
    
    if (!receiveMapFromServer(*tcpSocket, grid)) {
        connectionMessage = "Failed to receive map data";
        connectionMessageColor = sf::Color::Red;
        tcpSocket.reset();
        return false;
    }
    
    ErrorHandler::logInfo("Grid-based map received successfully from server");
    
    // Receive initial server position
    PositionPacket serverPosPacket;
    std::size_t received = 0;
    sf::Socket::Status serverPosStatus = tcpSocket->receive(&serverPosPacket, sizeof(PositionPacket), received);
    
    if (serverPosStatus != sf::Socket::Done) {
        ErrorHandler::logTCPError("Receive server initial position", serverPosStatus, ip);
        connectionMessage = "Failed to receive initial positions";
        connectionMessageColor = sf::Color::Red;
        tcpSocket.reset();
        return false;
    }
    
    if (received != sizeof(PositionPacket)) {
        std::ostringstream oss;
        oss << "Server position packet size mismatch - expected " << sizeof(PositionPacket) 
            << " bytes, got " << received;
        ErrorHandler::handleInvalidPacket(oss.str(), ip);
    } else if (validatePosition(serverPosPacket)) {
        serverPos.x = serverPosPacket.x;
        serverPos.y = serverPosPacket.y;
        ErrorHandler::logInfo("Server initial position: (" + std::to_string(serverPos.x) + 
                             ", " + std::to_string(serverPos.y) + ")");
    }
    
    // Receive initial client position
    PositionPacket clientPosPacket;
    sf::Socket::Status clientPosStatus = tcpSocket->receive(&clientPosPacket, sizeof(PositionPacket), received);
    
    if (clientPosStatus != sf::Socket::Done) {
        ErrorHandler::logTCPError("Receive client initial position", clientPosStatus, ip);
        connectionMessage = "Failed to receive initial positions";
        connectionMessageColor = sf::Color::Red;
        tcpSocket.reset();
        return false;
    }
    
    if (received != sizeof(PositionPacket)) {
        std::ostringstream oss;
        oss << "Client position packet size mismatch - expected " << sizeof(PositionPacket) 
            << " bytes, got " << received;
        ErrorHandler::handleInvalidPacket(oss.str(), ip);
    } else if (validatePosition(clientPosPacket)) {
        clientPos.x = clientPosPacket.x;
        clientPos.y = clientPosPacket.y;
        clientPosPrevious.x = clientPosPacket.x;
        clientPosPrevious.y = clientPosPacket.y;
        ErrorHandler::logInfo("Client initial position: (" + std::to_string(clientPos.x) + 
                             ", " + std::to_string(clientPos.y) + ")");
    }
    
    connectionMessage = "Connection established";
    connectionMessageColor = sf::Color::Green;
    ErrorHandler::logInfo("TCP handshake completed successfully");
    
    return true;
}

void udpThread(std::unique_ptr<sf::UdpSocket> socket, const std::string& ip) {
    // Bind UDP socket to port 53002
    sf::Socket::Status bindStatus = socket->bind(53002);
    if (bindStatus != sf::Socket::Done) {
        ErrorHandler::logUDPError("Bind UDP socket to port 53002", "Failed to bind");
        return;
    }
    
    ErrorHandler::logInfo("UDP socket bound to port 53002");
    socket->setBlocking(false); // Non-blocking for receive operations
    
    sf::Clock sendClock; // Clock for 20Hz send rate
    const float sendInterval = 1.0f / 20.0f; // 50ms = 20Hz
    
    while (udpRunning) {
        // Send position at 20Hz
        if (sendClock.getElapsedTime().asSeconds() >= sendInterval) {
            PositionPacket outPacket;
            
            {
                std::lock_guard<std::mutex> lock(mutex);
                outPacket.x = clientPos.x;
                outPacket.y = clientPos.y;
                outPacket.isAlive = true;
                outPacket.frameID = currentFrameID++;
                outPacket.playerId = 1; // Client is player 1, server is player 0
            }
            
            // Send position to server
            sf::Socket::Status sendStatus = socket->send(&outPacket, sizeof(PositionPacket), sf::IpAddress(ip), 53001);
            if (sendStatus != sf::Socket::Done && sendStatus != sf::Socket::NotReady) {
                ErrorHandler::logUDPError("Send position packet", "Failed to send to server");
            }
            
            sendClock.restart();
        }
        
        // Receive positions from server (non-blocking)
        PositionPacket inPacket;
        std::size_t received;
        sf::IpAddress sender;
        unsigned short port;
        
        sf::Socket::Status status = socket->receive(&inPacket, sizeof(PositionPacket), received, sender, port);
        
        if (status == sf::Socket::Done) {
            // Validate packet
            if (received == sizeof(PositionPacket)) {
                if (sender == sf::IpAddress(ip)) {
                    if (validatePosition(inPacket)) {
                        std::lock_guard<std::mutex> lock(mutex);
                        
                        // Update server position
                        if (inPacket.playerId == 0) { // Server is player 0
                            serverPos.x = inPacket.x;
                            serverPos.y = inPacket.y;
                            serverConnected = true;
                            lastPacketReceived.restart(); // Reset timeout timer
                        }
                    }
                    // Note: validatePosition already logs the error if invalid
                } else {
                    ErrorHandler::handleInvalidPacket("Packet from unexpected sender: " + sender.toString(), ip);
                }
            } else {
                std::ostringstream oss;
                oss << "Position packet size mismatch - expected " << sizeof(PositionPacket) 
                    << " bytes, got " << received;
                ErrorHandler::handleInvalidPacket(oss.str(), ip);
            }
        } else if (status != sf::Socket::NotReady) {
            ErrorHandler::logUDPError("Receive position packet", "Socket error occurred");
        }
        
        // Check for connection loss (2 second timeout)
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (serverConnected && lastPacketReceived.getElapsedTime().asSeconds() > 2.0f) {
                ErrorHandler::handleConnectionLost(ip);
                serverConnected = false;
                // Connection loss will be handled in main loop
            }
        }
        
        // Small sleep to prevent busy-waiting
        sf::sleep(sf::milliseconds(10));
    }
    
    ErrorHandler::logInfo("UDP thread terminated");
}

bool isButtonClicked(const sf::RectangleShape& button, const sf::Event& event, const sf::RenderWindow& window) {
    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2i mousePos = sf::Mouse::getPosition(window);
        if (button.getGlobalBounds().contains(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y))) {
            return true;
        }
    }
    return false;
}

void toggleFullscreen(sf::RenderWindow& window, bool& isFullscreen, const sf::VideoMode& desktopMode) {
    isFullscreen = !isFullscreen;
    sf::Uint32 style = isFullscreen ? sf::Style::Fullscreen : sf::Style::Resize | sf::Style::Close;
    sf::VideoMode mode = isFullscreen ? desktopMode : sf::VideoMode(800, 600);

    window.create(mode, isFullscreen ? "Client" : "Client (Windowed)", style);
    window.setFramerateLimit(60);
}

int main() {
    sf::VideoMode desktopMode = sf::VideoMode::getDesktopMode();
    sf::RenderWindow window(desktopMode, "Client", sf::Style::Fullscreen);
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.loadFromFile("arial.ttf")) {
        std::cerr << "Failed to load font!" << std::endl;
        return -1;
    }

    bool isFullscreen = true;

    ClientState state = ClientState::ConnectScreen;
    std::string inputIP = "127.0.0.1";
    bool inputActive = false;
    bool udpThreadStarted = false;
    std::unique_ptr<sf::UdpSocket> udpSocket = std::make_unique<sf::UdpSocket>();
    std::thread udpWorker;
    sf::Clock errorTimer;

    // �������� ������ �����������
    sf::Text ipLabel;
    ipLabel.setFont(font);
    ipLabel.setString("SERVER IP ADDRESS:");
    ipLabel.setCharacterSize(32);
    ipLabel.setFillColor(sf::Color::White);

    sf::RectangleShape ipField(sf::Vector2f(400, 50));
    ipField.setFillColor(sf::Color(50, 50, 50));
    ipField.setOutlineColor(sf::Color(100, 100, 100));
    ipField.setOutlineThickness(2.0f);

    sf::Text ipText;
    ipText.setFont(font);
    ipText.setString(inputIP);
    ipText.setCharacterSize(28);
    ipText.setFillColor(sf::Color::White);

    sf::RectangleShape connectButton(sf::Vector2f(300, 70));
    connectButton.setFillColor(sf::Color(0, 150, 0));

    sf::Text connectText;
    connectText.setFont(font);
    connectText.setString("CONNECT TO THE SERVER");
    connectText.setCharacterSize(32);
    connectText.setFillColor(sf::Color::White);

    sf::Text errorText;
    errorText.setFont(font);
    errorText.setString("THE SERVER IS UNAVAILABLE OR THE IP IS INVALID");
    errorText.setCharacterSize(28);
    errorText.setFillColor(sf::Color::Red);
    
    // Connection success message
    sf::Text connectionSuccessText;
    connectionSuccessText.setFont(font);
    connectionSuccessText.setString("Connection established");
    connectionSuccessText.setCharacterSize(32);
    connectionSuccessText.setFillColor(sf::Color::Green);
    
    // Ready button (200x60px)
    sf::RectangleShape readyButton(sf::Vector2f(200, 60));
    readyButton.setFillColor(sf::Color(0, 150, 0));
    
    sf::Text readyText;
    readyText.setFont(font);
    readyText.setString("READY");
    readyText.setCharacterSize(32);
    readyText.setFillColor(sf::Color::White);
    
    // Waiting for start message
    sf::Text waitingStartText;
    waitingStartText.setFont(font);
    waitingStartText.setString("Waitind for the start...");
    waitingStartText.setCharacterSize(28);
    waitingStartText.setFillColor(sf::Color::Yellow);

    // �������� ��������� ������
    sf::CircleShape clientCircle(30.0f);
    clientCircle.setFillColor(sf::Color::Blue);
    clientCircle.setOutlineColor(sf::Color(0, 0, 100));
    clientCircle.setOutlineThickness(3.0f);
    clientCircle.setPosition(clientPos.x - 30.0f, clientPos.y - 30.0f);

    sf::CircleShape serverCircle(10.0f);
    serverCircle.setFillColor(sf::Color(0, 200, 0, 200));
    serverCircle.setOutlineColor(sf::Color(0, 100, 0));
    serverCircle.setOutlineThickness(2.0f);
    
    // Clock for delta time calculation
    sf::Clock deltaClock;
    float interpolationAlpha = 0.0f;
    
    // Performance monitoring
    PerformanceMonitor perfMonitor;

    // ������� ��� ������������� ���������
    auto centerElements = [&]() {
        sf::Vector2u windowSize = window.getSize();

        ipLabel.setPosition(windowSize.x / 2 - ipLabel.getLocalBounds().width / 2,
            windowSize.y / 2 - 150);

        ipField.setPosition(windowSize.x / 2 - 200, windowSize.y / 2 - 50);
        ipText.setPosition(windowSize.x / 2 - 190, windowSize.y / 2 - 45);

        connectButton.setPosition(windowSize.x / 2 - 150, windowSize.y / 2 + 50);
        connectText.setPosition(windowSize.x / 2 - connectText.getLocalBounds().width / 2,
            windowSize.y / 2 + 50 + 10);

        errorText.setPosition(windowSize.x / 2 - errorText.getLocalBounds().width / 2,
            windowSize.y - 100);
            
        connectionSuccessText.setPosition(windowSize.x / 2 - connectionSuccessText.getLocalBounds().width / 2,
            windowSize.y / 2 - 50);
            
        readyButton.setPosition(windowSize.x / 2 - 100, windowSize.y / 2 + 50);
        readyText.setPosition(windowSize.x / 2 - readyText.getLocalBounds().width / 2,
            windowSize.y / 2 + 50 + 10);
            
        waitingStartText.setPosition(windowSize.x / 2 - waitingStartText.getLocalBounds().width / 2,
            windowSize.y / 2 + 50);
        };

    centerElements();

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();

            // ��������� Esc ��� ������������ ������
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                toggleFullscreen(window, isFullscreen, desktopMode);
                centerElements();
            }

            if (state == ClientState::ConnectScreen) {
                // ��������� ����� �� ���� �����
                if (event.type == sf::Event::MouseButtonPressed) {
                    sf::Vector2i mousePos = sf::Mouse::getPosition(window);
                    inputActive = ipField.getGlobalBounds().contains(
                        static_cast<float>(mousePos.x),
                        static_cast<float>(mousePos.y)
                    );
                }

                // ��������� ����� � ����������
                if (inputActive) {
                    if (event.type == sf::Event::KeyPressed) {
                        if (event.key.code == sf::Keyboard::BackSpace && !inputIP.empty()) {
                            inputIP.pop_back();
                            ipText.setString(inputIP);
                        }
                    }
                    else if (event.type == sf::Event::TextEntered) {
                        if (event.text.unicode < 128) {
                            char c = static_cast<char>(event.text.unicode);
                            if (c == '\b') { // Backspace � ��������� �������
                                if (!inputIP.empty()) {
                                    inputIP.pop_back();
                                    ipText.setString(inputIP);
                                }
                            }
                            else if (c == '.' || (c >= '0' && c <= '9')) {
                                if (inputIP.size() < 15) {
                                    inputIP += c;
                                    ipText.setString(inputIP);
                                }
                            }
                            else if (c == '\r' || c == '\n') { // Enter
                                // Perform TCP handshake when Enter is pressed
                                if (performTCPHandshake(inputIP)) {
                                    serverIP = inputIP;
                                    state = ClientState::Connected;
                                }
                                else {
                                    state = ClientState::ErrorScreen;
                                    errorTimer.restart();
                                }
                            }
                        }
                    }
                }

                // ��������� ������� ������ �����������
                if (isButtonClicked(connectButton, event, window)) {
                    // Perform TCP handshake
                    if (performTCPHandshake(inputIP)) {
                        serverIP = inputIP;
                        state = ClientState::Connected;
                    }
                    else {
                        state = ClientState::ErrorScreen;
                        errorTimer.restart();
                    }
                }
            }
            else if (state == ClientState::Connected) {
                // Handle READY button click
                if (isButtonClicked(readyButton, event, window)) {
                    ErrorHandler::logInfo("=== READY Button Clicked ===");
                    
                    // Send ReadyPacket to server
                    ReadyPacket readyPacket;
                    readyPacket.type = MessageType::CLIENT_READY;
                    readyPacket.isReady = true;
                    
                    if (tcpSocket) {
                        ErrorHandler::logInfo("Sending ReadyPacket to server...");
                        ErrorHandler::logInfo("Packet size: " + std::to_string(sizeof(ReadyPacket)) + " bytes");
                        
                        sf::Socket::Status sendStatus = tcpSocket->send(&readyPacket, sizeof(ReadyPacket));
                        ErrorHandler::logInfo("Send status: " + std::to_string(static_cast<int>(sendStatus)));
                        
                        if (sendStatus == sf::Socket::Done) {
                            ErrorHandler::logInfo("✓ ReadyPacket sent successfully to server");
                            ErrorHandler::logInfo("Transitioning to WaitingForStart state");
                            state = ClientState::WaitingForStart;
                        } else {
                            ErrorHandler::logTCPError("Send ReadyPacket", sendStatus, serverIP);
                        }
                    } else {
                        ErrorHandler::logNetworkError("Send ReadyPacket", "TCP socket is null");
                    }
                }
            }
        }

        window.clear(sf::Color::Black);

        if (state == ClientState::ConnectScreen) {
            window.draw(ipLabel);
            window.draw(ipField);
            window.draw(ipText);
            window.draw(connectButton);
            window.draw(connectText);

            // ���������� ��������� ��������� ���� �����
            if (inputActive) {
                ipField.setOutlineColor(sf::Color::Green);
                ipField.setOutlineThickness(3.0f);
            }
            else {
                ipField.setOutlineColor(sf::Color(100, 100, 100));
                ipField.setOutlineThickness(2.0f);
            }
        }
        else if (state == ClientState::ErrorScreen) {
            window.draw(ipLabel);
            window.draw(ipField);
            window.draw(ipText);
            window.draw(connectButton);
            window.draw(connectText);
            window.draw(errorText);

            if (errorTimer.getElapsedTime().asSeconds() > 3.0f) {
                state = ClientState::ConnectScreen;
            }
        }
        else if (state == ClientState::Connected) {
            // Display connection success message
            connectionSuccessText.setString(connectionMessage);
            connectionSuccessText.setFillColor(connectionMessageColor);
            
            // Recenter the text
            sf::Vector2u windowSize = window.getSize();
            connectionSuccessText.setPosition(
                windowSize.x / 2 - connectionSuccessText.getLocalBounds().width / 2,
                windowSize.y / 2 - 50
            );
            
            window.draw(connectionSuccessText);
            
            // Display READY button (200x60px)
            window.draw(readyButton);
            window.draw(readyText);
        }
        else if (state == ClientState::WaitingForStart) {
            // Display "Waiting for game start..." message
            sf::Vector2u windowSize = window.getSize();
            waitingStartText.setPosition(
                windowSize.x / 2 - waitingStartText.getLocalBounds().width / 2,
                windowSize.y / 2
            );
            
            window.draw(waitingStartText);
            
            // Check for StartPacket from server (non-blocking)
            if (tcpSocket) {
                tcpSocket->setBlocking(false);
                StartPacket startPacket;
                std::size_t received = 0;
                
                sf::Socket::Status receiveStatus = tcpSocket->receive(&startPacket, sizeof(StartPacket), received);
                
                if (receiveStatus == sf::Socket::Done) {
                    ErrorHandler::logInfo("=== Received Data from Server ===");
                    ErrorHandler::logInfo("Received bytes: " + std::to_string(received));
                    ErrorHandler::logInfo("Expected bytes: " + std::to_string(sizeof(StartPacket)));
                    
                    if (received == sizeof(StartPacket)) {
                        ErrorHandler::logInfo("Packet type: " + std::to_string(static_cast<int>(startPacket.type)));
                        ErrorHandler::logInfo("Expected type: " + std::to_string(static_cast<int>(MessageType::SERVER_START)));
                        
                        if (startPacket.type == MessageType::SERVER_START) {
                            ErrorHandler::logInfo("✓ Valid StartPacket received from server!");
                            
                            // Start UDP thread for position synchronization
                            if (!udpThreadStarted) {
                                lastPacketReceived.restart(); // Initialize connection timeout timer
                                
                                // Set serverConnected to true before starting UDP thread
                                // This prevents false "connection lost" detection immediately after starting
                                {
                                    std::lock_guard<std::mutex> lock(mutex);
                                    serverConnected = true;
                                }
                                
                                udpWorker = std::thread(udpThread, std::move(udpSocket), serverIP);
                                udpThreadStarted = true;
                                ErrorHandler::logInfo("UDP thread started for position synchronization at 20Hz");
                            }
                            
                            // Transition to main game screen
                            state = ClientState::MainScreen;
                            deltaClock.restart(); // Reset delta clock for game loop
                            ErrorHandler::logInfo("Transitioning to main game screen");
                        } else {
                            ErrorHandler::handleInvalidPacket("StartPacket type mismatch", serverIP);
                        }
                    } else {
                        std::ostringstream oss;
                        oss << "StartPacket size mismatch - expected " << sizeof(StartPacket) 
                            << " bytes, got " << received;
                        ErrorHandler::handleInvalidPacket(oss.str(), serverIP);
                    }
                } else if (receiveStatus == sf::Socket::Disconnected) {
                    ErrorHandler::logWarning("Socket disconnected while waiting for StartPacket");
                    ErrorHandler::handleConnectionLost(serverIP);
                    state = ClientState::ConnectionLost;
                } else if (receiveStatus != sf::Socket::NotReady) {
                    // Only log non-NotReady errors to avoid spam
                    static sf::Clock errorLogClock;
                    if (errorLogClock.getElapsedTime().asSeconds() > 5.0f) {
                        ErrorHandler::logTCPError("Receive StartPacket", receiveStatus, serverIP);
                        errorLogClock.restart();
                    }
                }
                
                tcpSocket->setBlocking(true);
            } else {
                ErrorHandler::logWarning("TCP socket is null while waiting for StartPacket");
            }
        }
        else if (state == ClientState::MainScreen) {
            // Check for connection loss
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (!serverConnected && udpThreadStarted) {
                    ErrorHandler::logWarning("Connection lost detected in main loop");
                    state = ClientState::ConnectionLost;
                }
            }
            
            // Calculate delta time for frame-independent movement
            float deltaTime = deltaClock.restart().asSeconds();
            
            // Update performance monitoring
            size_t playerCount = serverConnected ? 2 : 1; // Client + server (if connected)
            // Count walls in the grid (approximate - each cell can have 0-4 walls)
            size_t wallCount = 0;
            for (int i = 0; i < GRID_SIZE; i++) {
                for (int j = 0; j < GRID_SIZE; j++) {
                    if (grid[i][j].topWall) wallCount++;
                    if (grid[i][j].rightWall) wallCount++;
                    if (grid[i][j].bottomWall) wallCount++;
                    if (grid[i][j].leftWall) wallCount++;
                }
            }
            perfMonitor.update(deltaTime, playerCount, wallCount);
            
            // Handle client player movement (input isolation - client controls only blue circle)
            if (window.hasFocus()) {
                // Store previous position for interpolation
                clientPosPrevious.x = clientPos.x;
                clientPosPrevious.y = clientPos.y;
                
                sf::Vector2f oldPos(clientPos.x, clientPos.y);
                sf::Vector2f newPos = oldPos;
                
                // Handle WASD input with delta time for frame-independent movement
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) newPos.y -= MOVEMENT_SPEED * deltaTime * 60.0f;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) newPos.y += MOVEMENT_SPEED * deltaTime * 60.0f;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) newPos.x -= MOVEMENT_SPEED * deltaTime * 60.0f;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) newPos.x += MOVEMENT_SPEED * deltaTime * 60.0f;
                
                // Apply cell-based collision detection with walls
                // The new system uses the grid directly instead of the old GameMap
                newPos = resolveCollisionCellBased(oldPos, newPos, grid);
                
                // Update client position
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    clientPos.x = newPos.x;
                    clientPos.y = newPos.y;
                }
            }
            
            // Update interpolation alpha for smooth rendering
            interpolationAlpha += deltaTime * 10.0f; // Adjust multiplier for smoothness
            if (interpolationAlpha > 1.0f) interpolationAlpha = 1.0f;
            
            // Use interpolated position for rendering smooth movement
            sf::Vector2f renderPos = lerpPosition(
                sf::Vector2f(clientPosPrevious.x, clientPosPrevious.y),
                sf::Vector2f(clientPos.x, clientPos.y),
                interpolationAlpha
            );
            
            // Update camera to follow the local player (blue circle)
            // This must be called before any rendering to ensure the view is set correctly
            updateCamera(window, renderPos);
            
            // Get window size for UI rendering later
            sf::Vector2u windowSize = window.getSize();
            
            // NOTE: Scaling is no longer needed with the new camera system
            // The camera view handles coordinate transformation automatically
            // All rendering now uses world coordinates directly (0-5000 range)
            
            // Get server position for rendering
            sf::Vector2f currentServerPos;
            bool isServerConnected = false;
            {
                std::lock_guard<std::mutex> lock(mutex);
                currentServerPos = sf::Vector2f(serverPos.x, serverPos.y);
                isServerConnected = serverConnected;
            }
            
            // NEW: Render visible walls using cell-based system with fog of war
            // This replaces the old fog of war rendering
            renderVisibleWalls(window, sf::Vector2f(clientPos.x, clientPos.y), grid);
            
            // Draw server player (green circle) only if within visibility radius
            if (isServerConnected && isVisible(sf::Vector2f(clientPos.x, clientPos.y), currentServerPos, VISIBILITY_RADIUS)) {
                sf::CircleShape serverCircle(10.0f);
                serverCircle.setFillColor(sf::Color(0, 200, 0, 200));
                serverCircle.setOutlineColor(sf::Color(0, 100, 0));
                serverCircle.setOutlineThickness(2.0f);
                serverCircle.setPosition(
                    currentServerPos.x - 10.0f,
                    currentServerPos.y - 10.0f
                );
                window.draw(serverCircle);
            }
            
            // Draw local client player as blue circle (radius 10px) - always visible
            clientCircle.setRadius(10.0f);
            clientCircle.setFillColor(sf::Color::Blue);
            clientCircle.setOutlineColor(sf::Color(0, 0, 100));
            clientCircle.setOutlineThickness(3.0f);
            clientCircle.setPosition(renderPos.x - 10.0f, renderPos.y - 10.0f);
            window.draw(clientCircle);
            
            // NOTE: Fog overlay is now integrated into renderVisibleWalls
            // The fog of war effect is applied directly to walls based on distance
            // No need for separate overlay anymore
            
            // Reset to default view for UI rendering (HUD should be fixed on screen)
            window.setView(window.getDefaultView());
            
            // Draw score in top-left corner
            sf::Text scoreText;
            scoreText.setFont(font);
            scoreText.setString("Score: " + std::to_string(clientScore));
            scoreText.setCharacterSize(28);
            scoreText.setFillColor(sf::Color::White);
            scoreText.setPosition(20.0f, 20.0f);
            window.draw(scoreText);
            
            // Draw health next to score
            sf::Text healthText;
            healthText.setFont(font);
            healthText.setString("Health: " + std::to_string(static_cast<int>(clientHealth)) + "/100");
            healthText.setCharacterSize(28);
            healthText.setFillColor(sf::Color::Green);
            healthText.setPosition(20.0f, 60.0f);
            window.draw(healthText);
            
            // Apply screen darkening effect if player is dead
            if (!clientIsAlive) {
                sf::RectangleShape deathOverlay(sf::Vector2f(windowSize.x, windowSize.y));
                deathOverlay.setFillColor(sf::Color(0, 0, 0, 180)); // Dark semi-transparent overlay
                window.draw(deathOverlay);
            }
        }
        else if (state == ClientState::ConnectionLost) {
            // Display connection lost screen
            sf::Text connectionLostText;
            connectionLostText.setFont(font);
            connectionLostText.setString("Connection lost. Press R to reconnect to " + serverIP);
            connectionLostText.setCharacterSize(32);
            connectionLostText.setFillColor(sf::Color::Red);
            
            sf::Vector2u windowSize = window.getSize();
            connectionLostText.setPosition(
                windowSize.x / 2 - connectionLostText.getLocalBounds().width / 2,
                windowSize.y / 2
            );
            
            window.draw(connectionLostText);
            
            // Handle R key for reconnection
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::R)) {
                ErrorHandler::logInfo("Attempting to reconnect to " + serverIP);
                
                // Stop UDP thread
                if (udpThreadStarted) {
                    udpRunning = false;
                    if (udpWorker.joinable()) {
                        udpWorker.join();
                    }
                    udpThreadStarted = false;
                    ErrorHandler::logInfo("UDP thread stopped for reconnection");
                }
                
                // Reset connection state
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    serverConnected = false;
                }
                
                // Reset UDP socket
                udpSocket = std::make_unique<sf::UdpSocket>();
                udpRunning = true;
                
                // Attempt TCP reconnection
                if (performTCPHandshake(serverIP)) {
                    state = ClientState::Connected;
                    ErrorHandler::logInfo("Reconnection successful");
                }
                else {
                    state = ClientState::ErrorScreen;
                    errorTimer.restart();
                    ErrorHandler::logNetworkError("Reconnection", "Failed to reconnect to " + serverIP);
                }
            }
        }

        window.display();
        sf::sleep(sf::milliseconds(16));
    }

    // Clean up UDP thread
    if (udpThreadStarted) {
        udpRunning = false; // Signal thread to stop
        if (udpWorker.joinable()) {
            udpWorker.join();
        }
        ErrorHandler::logInfo("UDP thread stopped");
    }

    return 0;
}