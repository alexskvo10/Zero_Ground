#include <SFML/Network.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <cstring>
#include <random>
#include <queue>
#include <cmath>
#include <ctime>
#include <atomic>

enum class ServerState { StartScreen, MainScreen };

// Global server state (atomic for thread safety)
std::atomic<ServerState> serverState(ServerState::StartScreen);

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
    float width = 500.0f;
    float height = 500.0f;
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
    static void handleConnectionLost(const std::string& clientIP) {
        std::cerr << "[ERROR] Connection lost with client: " << clientIP << std::endl;
        std::cerr << "[INFO] Client will need to reconnect" << std::endl;
    }
    
    // Handle map generation failure
    static void handleMapGenerationFailure() {
        std::cerr << "[CRITICAL] Map generation failed after maximum attempts" << std::endl;
        std::cerr << "[CRITICAL] Server cannot start without a valid map" << std::endl;
        std::cerr << "[CRITICAL] Exiting application..." << std::endl;
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
    static void logTCPError(const std::string& operation, sf::Socket::Status status, const std::string& clientIP = "") {
        std::cerr << "[TCP ERROR] Operation: " << operation;
        if (!clientIP.empty()) {
            std::cerr << " - Client: " << clientIP;
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
    bool valid = packet.x >= 0.0f && packet.x <= 500.0f &&
                 packet.y >= 0.0f && packet.y <= 500.0f;
    
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
// Map Generation Functions
// ========================

bool overlapsSpawnPoint(const Wall& wall) {
    sf::FloatRect wallBounds(wall.x, wall.y, wall.width, wall.height);
    sf::FloatRect serverSpawn(0.0f, 450.0f, 50.0f, 50.0f);   // Bottom-left corner
    sf::FloatRect clientSpawn(450.0f, 0.0f, 50.0f, 50.0f);   // Top-right corner
    
    return wallBounds.intersects(serverSpawn) || wallBounds.intersects(clientSpawn);
}

bool cellHasWall(int cellX, int cellY, const GameMap& map) {
    sf::FloatRect cellBounds(cellX * 10.0f, cellY * 10.0f, 10.0f, 10.0f);
    for (const auto& wall : map.walls) {
        sf::FloatRect wallBounds(wall.x, wall.y, wall.width, wall.height);
        if (cellBounds.intersects(wallBounds)) return true;
    }
    return false;
}

bool bfsPathExists(sf::Vector2f start, sf::Vector2f end, const GameMap& map) {
    // BFS on a grid with 10x10 unit cells
    const int gridSize = 50; // 500/10 = 50
    std::vector<std::vector<bool>> visited(gridSize, std::vector<bool>(gridSize, false));
    std::queue<sf::Vector2i> queue;
    
    sf::Vector2i startCell(static_cast<int>(start.x / 10), static_cast<int>(start.y / 10));
    sf::Vector2i endCell(static_cast<int>(end.x / 10), static_cast<int>(end.y / 10));
    
    // Clamp to valid grid bounds
    startCell.x = std::max(0, std::min(gridSize - 1, startCell.x));
    startCell.y = std::max(0, std::min(gridSize - 1, startCell.y));
    endCell.x = std::max(0, std::min(gridSize - 1, endCell.x));
    endCell.y = std::max(0, std::min(gridSize - 1, endCell.y));
    
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

bool validateConnectivity(const GameMap& map) {
    // BFS from bottom-left to top-right
    sf::Vector2f start(25.0f, 475.0f);  // Server spawn
    sf::Vector2f end(475.0f, 25.0f);    // Client spawn
    
    return bfsPathExists(start, end, map);
}

void generateWalls(GameMap& map) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> posDist(50.0f, 450.0f); // Avoid edges
    std::uniform_real_distribution<float> lengthDist(3.0f, 20.0f); // Wall length 3-20
    std::uniform_int_distribution<int> orientationDist(0, 1); // 0=horizontal, 1=vertical
    const float wallThickness = 3.0f; // Thickness of walls
    
    // Generate 15-25 random walls
    std::uniform_int_distribution<int> wallCountDist(15, 25);
    int numWalls = wallCountDist(gen);
    
    for (int i = 0; i < numWalls; ++i) {
        Wall wall;
        float length = lengthDist(gen);
        int orientation = orientationDist(gen);
        
        wall.x = posDist(gen);
        wall.y = posDist(gen);
        
        if (orientation == 0) {
            // Horizontal wall
            wall.width = length;
            wall.height = wallThickness;
        } else {
            // Vertical wall
            wall.width = wallThickness;
            wall.height = length;
        }
        
        // Ensure wall stays within bounds
        if (wall.x + wall.width > 500.0f) {
            wall.x = 500.0f - wall.width;
        }
        if (wall.y + wall.height > 500.0f) {
            wall.y = 500.0f - wall.height;
        }
        
        // Ensure wall doesn't overlap spawn points
        if (!overlapsSpawnPoint(wall)) {
            map.walls.push_back(wall);
        }
    }
}

std::unique_ptr<Quadtree> buildQuadtree(std::vector<Wall>& walls, float mapWidth, float mapHeight) {
    // Create root quadtree node covering entire map
    auto quadtree = std::make_unique<Quadtree>(sf::FloatRect(0.0f, 0.0f, mapWidth, mapHeight));
    
    // Insert all walls into the quadtree
    for (auto& wall : walls) {
        quadtree->insert(&wall);
    }
    
    std::cout << "[INFO] Quadtree built successfully" << std::endl;
    return quadtree;
}

GameMap generateMap() {
    for (int attempt = 0; attempt < 10; ++attempt) {
        GameMap map;
        generateWalls(map);
        
        if (validateConnectivity(map)) {
            ErrorHandler::logInfo("Map generated successfully on attempt " + std::to_string(attempt + 1));
            ErrorHandler::logInfo("Total walls: " + std::to_string(map.walls.size()));
            
            // Calculate actual coverage
            float totalCoverage = 0.0f;
            for (const auto& wall : map.walls) {
                totalCoverage += wall.width * wall.height;
            }
            float coveragePercent = (totalCoverage / (500.0f * 500.0f)) * 100.0f;
            
            std::ostringstream oss;
            oss << "Map coverage: " << coveragePercent << "%";
            ErrorHandler::logInfo(oss.str());
            
            // Build quadtree for spatial partitioning
            map.spatialIndex = buildQuadtree(map.walls, map.width, map.height);
            
            return map;
        }
        
        ErrorHandler::logWarning("Map generation attempt " + std::to_string(attempt + 1) + " failed connectivity check");
    }
    
    ErrorHandler::handleMapGenerationFailure();
    throw std::runtime_error("Failed to generate valid map after 10 attempts");
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
                // Push back 1 unit in the opposite direction
                sf::Vector2f direction = oldPos - newPos;
                float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
                
                if (length > 0.001f) {
                    direction /= length;
                    return oldPos + direction * 1.0f;
                }
                
                return oldPos; // No movement if already in collision
            }
        }
    }
    
    return newPos; // No collision
}

// Clamp position to map boundaries [0, 500]
sf::Vector2f clampToMapBounds(sf::Vector2f pos, float radius) {
    pos.x = std::max(radius, std::min(500.0f - radius, pos.x));
    pos.y = std::max(radius, std::min(500.0f - radius, pos.y));
    return pos;
}

// ========================
// Thread-Safe Game State Manager
// ========================

class GameState {
public:
    // Thread-safe update player position
    void updatePlayerPosition(uint32_t playerId, float x, float y) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (players_.count(playerId)) {
            players_[playerId].previousX = players_[playerId].x;
            players_[playerId].previousY = players_[playerId].y;
            players_[playerId].x = x;
            players_[playerId].y = y;
        }
    }
    
    // Thread-safe get player
    Player getPlayer(uint32_t playerId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (players_.count(playerId)) {
            return players_.at(playerId);
        }
        return Player(); // Return default player if not found
    }
    
    // Thread-safe get players within radius
    std::vector<Player> getPlayersInRadius(sf::Vector2f center, float radius) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Player> result;
        
        for (const auto& pair : players_) {
            const Player& player = pair.second;
            float dx = player.x - center.x;
            float dy = player.y - center.y;
            float distance = std::sqrt(dx * dx + dy * dy);
            
            if (distance <= radius) {
                result.push_back(player);
            }
        }
        
        return result;
    }
    
    // Thread-safe set player ready status
    void setPlayerReady(uint32_t playerId, bool ready) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (players_.count(playerId)) {
            players_[playerId].isReady = ready;
        }
    }
    
    // Thread-safe check if all players are ready
    bool allPlayersReady() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (players_.empty()) {
            return false;
        }
        
        for (const auto& pair : players_) {
            const Player& player = pair.second;
            if (!player.isReady) {
                return false;
            }
        }
        
        return true;
    }
    
    // Thread-safe add player
    void addPlayer(uint32_t playerId, const Player& player) {
        std::lock_guard<std::mutex> lock(mutex_);
        players_[playerId] = player;
    }
    
    // Thread-safe remove player
    void removePlayer(uint32_t playerId) {
        std::lock_guard<std::mutex> lock(mutex_);
        players_.erase(playerId);
    }
    
    // Thread-safe get all players
    std::map<uint32_t, Player> getAllPlayers() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return players_;
    }
    
    // Thread-safe check if player exists
    bool hasPlayer(uint32_t playerId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return players_.count(playerId) > 0;
    }
    
    // Thread-safe get player count
    size_t getPlayerCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return players_.size();
    }
    
private:
    mutable std::mutex mutex_;
    std::map<uint32_t, Player> players_;
};

// ========================
// Movement and Interpolation Constants
// ========================

const float MOVEMENT_SPEED = 5.0f; // 5 units per second
const float VISIBILITY_RADIUS = 25.0f; // 25 units visibility radius

// Linear interpolation function for smooth position transitions
float lerp(float start, float end, float alpha) {
    return start + (end - start) * alpha;
}

sf::Vector2f lerpPosition(sf::Vector2f start, sf::Vector2f end, float alpha) {
    return sf::Vector2f(lerp(start.x, end.x, alpha), lerp(start.y, end.y, alpha));
}

// ========================
// Fog of War System
// ========================

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
                    const std::vector<Wall>& walls, 
                    const std::map<sf::IpAddress, sf::CircleShape>& clientCircles,
                    const std::map<sf::IpAddress, Position>& clients,
                    float scaleX, float scaleY) {
    
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
        
        wallShape.setOutlineColor(sf::Color(100, 100, 100));
        wallShape.setOutlineThickness(1.0f);
        window.draw(wallShape);
    }
    
    // Draw client players only if within visibility radius
    for (const auto& pair : clients) {
        sf::IpAddress ip = pair.first;
        Position pos = pair.second;
        
        if (isVisible(playerPos, sf::Vector2f(pos.x, pos.y), VISIBILITY_RADIUS)) {
            // Find the circle shape for this client
            auto it = clientCircles.find(ip);
            if (it != clientCircles.end()) {
                window.draw(it->second);
            }
        }
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
            
            // Log warning if FPS drops below 55
            if (currentFPS_ < 55.0f) {
                logPerformanceWarning(playerCount, wallCount);
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
    void logPerformanceWarning(size_t playerCount, size_t wallCount) {
        std::cerr << "[WARNING] Performance degradation detected!" << std::endl;
        std::cerr << "  FPS: " << currentFPS_ << " (target: 55+)" << std::endl;
        std::cerr << "  Players: " << playerCount << std::endl;
        std::cerr << "  Walls: " << wallCount << std::endl;
        
        // Log CPU usage (Windows-specific approximation)
        // Note: Accurate CPU usage requires platform-specific APIs
        // This is a simplified metric based on frame time
        float frameTime = (elapsedTime_ / frameCount_) * 1000.0f; // ms per frame
        std::cerr << "  Avg Frame Time: " << frameTime << "ms" << std::endl;
        
        // Estimate CPU usage based on frame time (rough approximation)
        // At 60 FPS, each frame should take ~16.67ms
        // If it takes longer, we can estimate higher CPU usage
        float estimatedCPUUsage = (frameTime / 16.67f) * 100.0f;
        if (estimatedCPUUsage > 100.0f) estimatedCPUUsage = 100.0f;
        std::cerr << "  Estimated CPU Usage: " << estimatedCPUUsage << "%" << std::endl;
    }
    
    int frameCount_;
    float elapsedTime_;
    float currentFPS_;
};

// ========================
// Global State
// ========================

std::mutex mutex;
Position serverPos = { 25.0f, 475.0f }; // Server spawn position (bottom-left)
Position serverPosPrevious = { 25.0f, 475.0f }; // Previous position for interpolation
float serverHealth = 100.0f; // Server player health (0-100)
int serverScore = 0; // Server player score
bool serverIsAlive = true; // Server player alive status
std::map<sf::IpAddress, Position> clients;
GameMap gameMap;
GameState gameState; // Thread-safe game state manager

// Client connection state
struct ClientConnection {
    std::unique_ptr<sf::TcpSocket> socket;
    sf::IpAddress address;
    bool isReady = false;
    uint32_t playerId = 0;
};

std::vector<ClientConnection> connectedClients;
std::mutex clientsMutex;
std::string connectionStatus = "Waiting for player..."; // Waiting for player... in Russian
sf::Color connectionStatusColor = sf::Color::White;
bool showPlayButton = false;

// Thread to handle ready status from connected clients
void readyListenerThread() {
    ErrorHandler::logInfo("Ready listener thread started");
    
    while (true) {
        std::vector<ClientConnection*> clientsToCheck;
        
        // Get list of connected clients and clean up disconnected ones
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            
            // Remove disconnected clients
            connectedClients.erase(
                std::remove_if(connectedClients.begin(), connectedClients.end(),
                    [](const ClientConnection& client) {
                        if (!client.socket) {
                            ErrorHandler::logInfo("Removing client with null socket");
                            return true;
                        }
                        return false;
                    }),
                connectedClients.end()
            );
            
            for (auto& client : connectedClients) {
                if (!client.isReady && client.socket) {
                    clientsToCheck.push_back(&client);
                }
            }
            
            // Log client status periodically
            static sf::Clock logClock;
            if (logClock.getElapsedTime().asSeconds() > 5.0f && !connectedClients.empty()) {
                ErrorHandler::logInfo("Ready listener status: " + std::to_string(connectedClients.size()) + 
                                     " total clients, " + std::to_string(clientsToCheck.size()) + " waiting for ready");
                logClock.restart();
            }
        }
        
        // Check each client for ready packet
        for (auto* client : clientsToCheck) {
            if (client->socket) {
                // Save original blocking mode
                bool wasBlocking = client->socket->isBlocking();
                client->socket->setBlocking(false);
                
                ReadyPacket readyPacket;
                std::size_t received = 0;
                sf::Socket::Status status = client->socket->receive(&readyPacket, sizeof(ReadyPacket), received);
                
                // Restore original blocking mode
                client->socket->setBlocking(wasBlocking);
                
                if (status == sf::Socket::Done && received == sizeof(ReadyPacket)) {
                    if (readyPacket.type == MessageType::CLIENT_READY && readyPacket.isReady) {
                        ErrorHandler::logInfo("Client " + client->address.toString() + " is ready");
                        
                        bool serverInGame = false;
                        
                        // Update client ready status
                        {
                            std::lock_guard<std::mutex> lock(clientsMutex);
                            client->isReady = true;
                            
                            // Check if server is already in game
                            serverInGame = (serverState.load() == ServerState::MainScreen);
                            
                            // Update UI status
                            connectionStatus = "The player is connected and ready to play"; // Player connected and ready to play in Russian
                            connectionStatusColor = sf::Color::Green;
                            showPlayButton = true;
                            
                            ErrorHandler::logInfo("Updated UI to show player ready");
                            
                            // If server is already in game, send StartPacket immediately
                            if (serverInGame) {
                                ErrorHandler::logInfo("Server is already in game, sending StartPacket immediately");
                                
                                StartPacket startPacket;
                                startPacket.type = MessageType::SERVER_START;
                                startPacket.timestamp = static_cast<uint32_t>(std::time(nullptr));
                                
                                client->socket->setBlocking(true);
                                sf::Socket::Status sendStatus = client->socket->send(&startPacket, sizeof(StartPacket));
                                
                                if (sendStatus == sf::Socket::Done) {
                                    ErrorHandler::logInfo("✓ Sent StartPacket to reconnected client " + client->address.toString());
                                } else {
                                    ErrorHandler::logTCPError("Send StartPacket to reconnected client", sendStatus, client->address.toString());
                                }
                            }
                        }
                        
                        // Update GameState
                        gameState.setPlayerReady(client->playerId, true);
                    } else {
                        ErrorHandler::handleInvalidPacket("ReadyPacket validation failed", client->address.toString());
                    }
                } else if (status == sf::Socket::Done && received != sizeof(ReadyPacket)) {
                    std::ostringstream oss;
                    oss << "ReadyPacket size mismatch - expected " << sizeof(ReadyPacket) 
                        << " bytes, got " << received;
                    ErrorHandler::handleInvalidPacket(oss.str(), client->address.toString());
                } else if (status == sf::Socket::Disconnected) {
                    ErrorHandler::handleConnectionLost(client->address.toString());
                    // Mark socket as null so it will be removed in next iteration
                    std::lock_guard<std::mutex> lock(clientsMutex);
                    client->socket.reset();
                } else if (status != sf::Socket::NotReady && status != sf::Socket::Done) {
                    ErrorHandler::logTCPError("Receive ReadyPacket", status, client->address.toString());
                }
            }
        }
        
        sf::sleep(sf::milliseconds(50)); // Check every 50ms
    }
}

// TCP listener thread to handle client connections
void tcpListenerThread(sf::TcpListener* listener) {
    ErrorHandler::logInfo("=== TCP Listener Thread Started ===");
    ErrorHandler::logInfo("Listening on port 53000 for incoming connections");
    
    while (true) {
        auto clientSocket = std::make_unique<sf::TcpSocket>();
        
        sf::Socket::Status acceptStatus = listener->accept(*clientSocket);
        if (acceptStatus == sf::Socket::Done) {
            std::string clientIP = clientSocket->getRemoteAddress().toString();
            ErrorHandler::logInfo("=== New Client Connection Accepted ===");
            ErrorHandler::logInfo("Client IP: " + clientIP);
            
            // Set socket to blocking mode for reliable TCP communication
            clientSocket->setBlocking(true);
            
            // Receive ConnectPacket
            ErrorHandler::logInfo("Waiting for ConnectPacket from client...");
            ConnectPacket connectPacket;
            std::size_t received = 0;
            sf::Socket::Status receiveStatus = clientSocket->receive(&connectPacket, sizeof(ConnectPacket), received);
            ErrorHandler::logInfo("Receive status: " + std::to_string(static_cast<int>(receiveStatus)) + ", received bytes: " + std::to_string(received));
            
            if (receiveStatus == sf::Socket::Done) {
                if (received == sizeof(ConnectPacket)) {
                    if (validateConnect(connectPacket)) {
                        ErrorHandler::logInfo("Valid ConnectPacket received from " + clientIP);
                        ErrorHandler::logInfo("Player name: " + std::string(connectPacket.playerName));
                    
                    // Send MapDataPacket
                    MapDataPacket mapHeader;
                    mapHeader.type = MessageType::MAP_DATA;
                    mapHeader.wallCount = static_cast<uint32_t>(gameMap.walls.size());
                    
                    sf::Socket::Status sendStatus = clientSocket->send(&mapHeader, sizeof(MapDataPacket));
                    if (sendStatus == sf::Socket::Done) {
                        ErrorHandler::logInfo("Sent MapDataPacket header with " + std::to_string(mapHeader.wallCount) + " walls");
                        
                        // Send all wall data
                        bool wallSendSuccess = true;
                        for (const auto& wall : gameMap.walls) {
                            sf::Socket::Status wallStatus = clientSocket->send(&wall, sizeof(Wall));
                            if (wallStatus != sf::Socket::Done) {
                                ErrorHandler::logTCPError("Send wall data", wallStatus, clientIP);
                                wallSendSuccess = false;
                                break;
                            }
                        }
                        
                        if (wallSendSuccess) {
                            ErrorHandler::logInfo("Sent all wall data to client");
                        
                            // Send initial player positions
                            // First send server player position
                            PositionPacket serverPosPacket;
                            serverPosPacket.x = serverPos.x;
                            serverPosPacket.y = serverPos.y;
                            serverPosPacket.isAlive = true;
                            serverPosPacket.frameID = 0;
                            serverPosPacket.playerId = 0; // Server is player 0
                            
                            sf::Socket::Status serverPosStatus = clientSocket->send(&serverPosPacket, sizeof(PositionPacket));
                            if (serverPosStatus == sf::Socket::Done) {
                                ErrorHandler::logInfo("Sent server initial position to client");
                            } else {
                                ErrorHandler::logTCPError("Send server initial position", serverPosStatus, clientIP);
                            }
                            
                            // Send client's spawn position (top-right corner)
                            PositionPacket clientPosPacket;
                            clientPosPacket.x = 475.0f;
                            clientPosPacket.y = 25.0f;
                            clientPosPacket.isAlive = true;
                            clientPosPacket.frameID = 0;
                            clientPosPacket.playerId = 1; // Client is player 1
                            
                            sf::Socket::Status clientPosStatus = clientSocket->send(&clientPosPacket, sizeof(PositionPacket));
                            if (clientPosStatus == sf::Socket::Done) {
                                ErrorHandler::logInfo("Sent client initial position");
                            } else {
                                ErrorHandler::logTCPError("Send client initial position", clientPosStatus, clientIP);
                            }
                            
                            // Store client connection
                            {
                                std::lock_guard<std::mutex> lock(clientsMutex);
                                ClientConnection conn;
                                conn.socket = std::move(clientSocket);
                                conn.address = conn.socket->getRemoteAddress();
                                conn.isReady = false;
                                conn.playerId = static_cast<uint32_t>(connectedClients.size() + 1);
                                connectedClients.push_back(std::move(conn));
                                
                                // Update connection status
                                connectionStatus = "The player is connected, but not ready"; // Player connected but not ready in Russian
                                connectionStatusColor = sf::Color::Yellow;
                                
                                ErrorHandler::logInfo("Client added to connected clients list");
                            }
                        }
                    } else {
                        ErrorHandler::logTCPError("Send MapDataPacket", sendStatus, clientIP);
                    }
                    } else {
                        ErrorHandler::handleInvalidPacket("ConnectPacket validation failed", clientIP);
                    }
                } else {
                    ErrorHandler::handleInvalidPacket("ConnectPacket size mismatch - expected " + 
                        std::to_string(sizeof(ConnectPacket)) + " bytes, got " + std::to_string(received), clientIP);
                }
            } else {
                ErrorHandler::logTCPError("Receive ConnectPacket", receiveStatus, clientIP);
            }
        } else if (acceptStatus != sf::Socket::NotReady) {
            ErrorHandler::logTCPError("Accept client connection", acceptStatus);
        }
        
        sf::sleep(sf::milliseconds(100)); // Small delay to prevent busy waiting
    }
}

// UDP listener thread for position synchronization (20Hz)
void udpListenerThread(sf::UdpSocket* socket) {
    ErrorHandler::logInfo("UDP listener thread started on port 53001");
    
    // Bind UDP socket to port 53001
    sf::Socket::Status bindStatus = socket->bind(53001);
    if (bindStatus != sf::Socket::Done) {
        ErrorHandler::logUDPError("Bind UDP socket to port 53001", "Failed to bind");
        return;
    }
    
    socket->setBlocking(false);
    ErrorHandler::logInfo("UDP socket bound successfully to port 53001");
    
    sf::Clock updateClock;
    const float UPDATE_INTERVAL = 1.0f / 20.0f; // 20Hz = 50ms per update
    
    while (true) {
        // Receive position updates from clients
        PositionPacket receivedPacket;
        std::size_t received = 0;
        sf::IpAddress sender;
        unsigned short senderPort;
        
        sf::Socket::Status status = socket->receive(&receivedPacket, sizeof(PositionPacket), received, sender, senderPort);
        
        if (status == sf::Socket::Done && received == sizeof(PositionPacket)) {
            // Validate received position
            if (validatePosition(receivedPacket)) {
                // Update GameState with validated position
                gameState.updatePlayerPosition(receivedPacket.playerId, receivedPacket.x, receivedPacket.y);
                
                // Also update legacy clients map for backward compatibility
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    Position pos;
                    pos.x = receivedPacket.x;
                    pos.y = receivedPacket.y;
                    clients[sender] = pos;
                }
            }
            // Note: validatePosition already logs the error if invalid
        } else if (status == sf::Socket::Done && received != sizeof(PositionPacket)) {
            std::ostringstream oss;
            oss << "Position packet size mismatch from " << sender.toString() 
                << " - expected " << sizeof(PositionPacket) << " bytes, got " << received;
            ErrorHandler::handleInvalidPacket(oss.str());
        } else if (status != sf::Socket::NotReady && status != sf::Socket::Done) {
            ErrorHandler::logUDPError("Receive position packet", "Socket error occurred");
        }
        
        // Send position updates to clients at 20Hz
        if (updateClock.getElapsedTime().asSeconds() >= UPDATE_INTERVAL) {
            updateClock.restart();
            
            // Get list of connected clients
            std::vector<ClientConnection> clientsCopy;
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                for (const auto& client : connectedClients) {
                    if (client.socket && client.isReady) {
                        clientsCopy.push_back(ClientConnection{
                            nullptr, // Don't copy socket
                            client.address,
                            client.isReady,
                            client.playerId
                        });
                    }
                }
            }
            
            // Send server position to each client
            for (const auto& client : clientsCopy) {
                // Prepare server position packet
                PositionPacket serverPacket;
                serverPacket.x = serverPos.x;
                serverPacket.y = serverPos.y;
                serverPacket.isAlive = true;
                serverPacket.frameID = static_cast<uint32_t>(std::time(nullptr));
                serverPacket.playerId = 0; // Server is player 0
                
                // Send server position
                socket->send(&serverPacket, sizeof(PositionPacket), client.address, 53002);
                
                // Implement network culling: only send players within 50 units radius
                const float NETWORK_CULLING_RADIUS = 50.0f;
                sf::Vector2f serverPosition(serverPos.x, serverPos.y);
                
                // Get players within radius
                std::vector<Player> nearbyPlayers = gameState.getPlayersInRadius(serverPosition, NETWORK_CULLING_RADIUS);
                
                // Send each nearby player's position
                for (const auto& player : nearbyPlayers) {
                    if (player.id != 0 && player.id != client.playerId) { // Don't send server or client's own position
                        PositionPacket playerPacket;
                        playerPacket.x = player.x;
                        playerPacket.y = player.y;
                        playerPacket.isAlive = player.isAlive;
                        playerPacket.frameID = static_cast<uint32_t>(std::time(nullptr));
                        playerPacket.playerId = static_cast<uint8_t>(player.id);
                        
                        socket->send(&playerPacket, sizeof(PositionPacket), client.address, 53002);
                    }
                }
            }
        }
        
        sf::sleep(sf::milliseconds(10)); // Small sleep to prevent busy waiting
    }
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

    window.create(mode, isFullscreen ? "Server" : "Server (Windowed)", style);
    window.setFramerateLimit(60);
}

int main() {
    // Generate map at startup
    try {
        gameMap = generateMap();
    } catch (const std::exception& e) {
        ErrorHandler::logNetworkError("Map generation", e.what());
        ErrorHandler::handleMapGenerationFailure();
        return -1;
    }
    
    sf::TcpListener tcpListener;
    ErrorHandler::logInfo("=== Starting TCP Server ===");
    ErrorHandler::logInfo("Attempting to bind to port 53000...");
    sf::Socket::Status listenStatus = tcpListener.listen(53000);
    if (listenStatus != sf::Socket::Done) {
        ErrorHandler::logTCPError("Start TCP listener on port 53000", listenStatus);
        ErrorHandler::logNetworkError("TCP Server Startup", "Failed to bind to port 53000");
        return -1;
    }
    ErrorHandler::logInfo("Successfully bound to port 53000");
    ErrorHandler::logInfo("Server is now listening for connections on 0.0.0.0:53000");
    tcpListener.setBlocking(false);
    
    // Start TCP listener thread
    std::thread tcpListenerWorker(tcpListenerThread, &tcpListener);
    ErrorHandler::logInfo("TCP listener thread started");
    
    // Start ready listener thread
    std::thread readyListenerWorker(readyListenerThread);
    ErrorHandler::logInfo("Ready listener thread started");

    sf::VideoMode desktopMode = sf::VideoMode::getDesktopMode();
    sf::RenderWindow window(desktopMode, "Server", sf::Style::Fullscreen);
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.loadFromFile("arial.ttf")) {
        std::cerr << "Failed to load font!" << std::endl;
        return -1;
    }

    bool isFullscreen = true;
    // Use global serverState instead of local state
    sf::UdpSocket udpSocket;
    std::thread udpWorker;
    bool udpThreadStarted = false;

    // Get local IP address
    sf::IpAddress localIP = sf::IpAddress::getLocalAddress();
    std::string ipString = localIP.toString();
    
    // Handle cases where IP is 127.0.0.1 or 0.0.0.0
    if (ipString == "0.0.0.0" || ipString == "127.0.0.1") {
        // Try to get public IP by connecting to a public server
        sf::IpAddress publicIP = sf::IpAddress::getPublicAddress(sf::seconds(2));
        if (publicIP != sf::IpAddress::None) {
            ipString = publicIP.toString();
        } else {
            ipString = "IP is unavailable"; // IP not available in Russian
        }
    }

    // Create startup screen texts
    sf::Text startText;
    startText.setFont(font);
    startText.setString("THE SERVER IS RUNNING"); // SERVER STARTED in Russian
    startText.setCharacterSize(64);
    startText.setFillColor(sf::Color::Green);

    sf::Text ipText;
    ipText.setFont(font);
    ipText.setString("Server IP: " + ipString); // Server IP in Russian
    ipText.setCharacterSize(32);
    ipText.setFillColor(sf::Color::White);

    sf::Text statusText;
    statusText.setFont(font);
    statusText.setString(connectionStatus);
    statusText.setCharacterSize(28);
    statusText.setFillColor(connectionStatusColor);

    sf::RectangleShape nextButton(sf::Vector2f(200, 60));
    nextButton.setFillColor(sf::Color(0, 150, 0));

    sf::Text buttonText;
    buttonText.setFont(font);
    buttonText.setString("NEXT");
    buttonText.setCharacterSize(32);
    buttonText.setFillColor(sf::Color::White);
    
    // PLAY button for when player is ready
    sf::RectangleShape playButton(sf::Vector2f(200, 60));
    playButton.setFillColor(sf::Color(0, 200, 0));
    
    sf::Text playButtonText;
    playButtonText.setFont(font);
    playButtonText.setString("PLAY"); // PLAY in Russian
    playButtonText.setCharacterSize(32);
    playButtonText.setFillColor(sf::Color::White);

    // �������� ��������� ������
    sf::CircleShape serverCircle(30.0f);
    serverCircle.setFillColor(sf::Color::Green);
    serverCircle.setOutlineColor(sf::Color(0, 100, 0));
    serverCircle.setOutlineThickness(3.0f);
    serverCircle.setPosition(serverPos.x - 30.0f, serverPos.y - 30.0f);

    std::map<sf::IpAddress, sf::CircleShape> clientCircles;
    
    // Clock for delta time calculation
    sf::Clock deltaClock;
    float interpolationAlpha = 0.0f;
    
    // Performance monitoring
    PerformanceMonitor perfMonitor;

    // Function to center UI elements on window resize
    auto centerElements = [&]() {
        sf::Vector2u windowSize = window.getSize();

        // Center "СЕРВЕР ЗАПУЩЕН" text
        startText.setPosition(windowSize.x / 2 - startText.getLocalBounds().width / 2,
            windowSize.y / 2 - 150);

        // Center "IP сервера:" text
        ipText.setPosition(windowSize.x / 2 - ipText.getLocalBounds().width / 2,
            windowSize.y / 2 - 50);

        // Center status text (will be updated dynamically)
        statusText.setPosition(windowSize.x / 2 - statusText.getLocalBounds().width / 2,
            windowSize.y / 2 + 20);

        // Center NEXT button (legacy, not used in final version)
        nextButton.setPosition(windowSize.x / 2 - 100, windowSize.y / 2 + 100);
        buttonText.setPosition(windowSize.x / 2 - buttonText.getLocalBounds().width / 2,
            windowSize.y / 2 + 100 + 10);
            
        // Center PLAY button
        playButton.setPosition(windowSize.x / 2 - 100, windowSize.y / 2 + 100);
        playButtonText.setPosition(windowSize.x / 2 - playButtonText.getLocalBounds().width / 2,
            windowSize.y / 2 + 100 + 10);
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

            if (serverState.load() == ServerState::StartScreen) {
                // Check if PLAY button is clicked (when player is ready)
                if (showPlayButton && isButtonClicked(playButton, event, window)) {
                    ErrorHandler::logInfo("=== PLAY Button Clicked ===");
                    ErrorHandler::logInfo("Preparing to send StartPacket to all ready clients");
                    
                    // Send StartPacket to all connected clients
                    {
                        std::lock_guard<std::mutex> lock(clientsMutex);
                        ErrorHandler::logInfo("Total connected clients: " + std::to_string(connectedClients.size()));
                        
                        StartPacket startPacket;
                        startPacket.type = MessageType::SERVER_START;
                        startPacket.timestamp = static_cast<uint32_t>(std::time(nullptr));
                        
                        int readyCount = 0;
                        int sentCount = 0;
                        
                        for (auto& client : connectedClients) {
                            ErrorHandler::logInfo("Checking client " + client.address.toString() + 
                                                 " - Socket valid: " + (client.socket ? "yes" : "no") + 
                                                 ", Ready: " + (client.isReady ? "yes" : "no"));
                            
                            if (client.socket && client.isReady) {
                                readyCount++;
                                client.socket->setBlocking(true);
                                sf::Socket::Status sendStatus = client.socket->send(&startPacket, sizeof(StartPacket));
                                if (sendStatus == sf::Socket::Done) {
                                    sentCount++;
                                    ErrorHandler::logInfo("✓ Successfully sent StartPacket to client " + client.address.toString());
                                } else {
                                    ErrorHandler::logTCPError("Send StartPacket", sendStatus, client.address.toString());
                                }
                            } else {
                                if (!client.socket) {
                                    ErrorHandler::logWarning("Client " + client.address.toString() + " has null socket");
                                }
                                if (!client.isReady) {
                                    ErrorHandler::logWarning("Client " + client.address.toString() + " is not ready");
                                }
                            }
                        }
                        
                        ErrorHandler::logInfo("StartPacket send summary: " + std::to_string(sentCount) + 
                                             " sent out of " + std::to_string(readyCount) + " ready clients");
                    }
                    
                    // Transition to main game screen
                    serverState.store(ServerState::MainScreen);
                    ErrorHandler::logInfo("Server transitioning to game screen");

                    // Start UDP listener thread for position synchronization
                    if (!udpThreadStarted) {
                        udpWorker = std::thread(udpListenerThread, &udpSocket);
                        udpThreadStarted = true;
                        ErrorHandler::logInfo("UDP listener thread started for position synchronization");
                    }
                }
            }
        }

        window.clear(sf::Color::Black);

        if (serverState.load() == ServerState::StartScreen) {
            // Update status text from global state
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                statusText.setString(connectionStatus);
                statusText.setFillColor(connectionStatusColor);
                statusText.setPosition(window.getSize().x / 2 - statusText.getLocalBounds().width / 2,
                    window.getSize().y / 2 + 20);
            }
            
            window.draw(startText);
            window.draw(ipText);
            window.draw(statusText);
            
            // Show PLAY button if player is ready, otherwise show nothing (waiting for player)
            if (showPlayButton) {
                window.draw(playButton);
                window.draw(playButtonText);
            }
        }
        else if (serverState.load() == ServerState::MainScreen) {
            // Calculate delta time for frame-independent movement
            float deltaTime = deltaClock.restart().asSeconds();
            
            // Update performance monitoring
            size_t playerCount = gameState.getPlayerCount() + 1; // +1 for server player
            size_t wallCount = gameMap.walls.size();
            perfMonitor.update(deltaTime, playerCount, wallCount);
            
            // ���������� ��������� ������
            if (window.hasFocus()) {
                // Store previous position for interpolation
                serverPosPrevious.x = serverPos.x;
                serverPosPrevious.y = serverPos.y;
                
                sf::Vector2f oldPos(serverPos.x, serverPos.y);
                sf::Vector2f newPos = oldPos;
                
                // Handle WASD input with delta time for frame-independent movement
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) newPos.y -= MOVEMENT_SPEED * deltaTime * 60.0f;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) newPos.y += MOVEMENT_SPEED * deltaTime * 60.0f;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) newPos.x -= MOVEMENT_SPEED * deltaTime * 60.0f;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) newPos.x += MOVEMENT_SPEED * deltaTime * 60.0f;
                
                // Apply collision detection with walls
                const float playerRadius = 30.0f;
                newPos = resolveCollision(oldPos, newPos, playerRadius, gameMap);
                
                // Clamp to map boundaries [0, 500]
                newPos = clampToMapBounds(newPos, playerRadius);
                
                serverPos.x = newPos.x;
                serverPos.y = newPos.y;
            }
            
            // Update interpolation alpha for smooth rendering
            interpolationAlpha += deltaTime * 10.0f; // Adjust multiplier for smoothness
            if (interpolationAlpha > 1.0f) interpolationAlpha = 1.0f;
            
            // Use interpolated position for rendering
            sf::Vector2f renderPos = lerpPosition(
                sf::Vector2f(serverPosPrevious.x, serverPosPrevious.y),
                sf::Vector2f(serverPos.x, serverPos.y),
                interpolationAlpha
            );
            
            // Scale position for rendering (map is 500x500, window might be different)
            sf::Vector2u windowSize = window.getSize();
            float scaleX = windowSize.x / 500.0f;
            float scaleY = windowSize.y / 500.0f;
            
            serverCircle.setPosition(renderPos.x * scaleX - 30.0f, renderPos.y * scaleY - 30.0f);

            // Update and render connected clients with interpolation
            {
                std::lock_guard<std::mutex> lock(mutex);
                
                // Get all players from GameState for interpolated positions
                auto allPlayers = gameState.getAllPlayers();
                
                for (auto& pair : clients) {
                    sf::IpAddress ip = pair.first;
                    Position pos = pair.second;

                    if (clientCircles.find(ip) == clientCircles.end()) {
                        clientCircles[ip] = sf::CircleShape(20.0f);
                        clientCircles[ip].setFillColor(sf::Color::Blue);
                        clientCircles[ip].setOutlineColor(sf::Color(0, 0, 100));
                        clientCircles[ip].setOutlineThickness(2.0f);
                    }

                    // Use interpolated position if available from GameState
                    sf::Vector2f renderClientPos(pos.x, pos.y);
                    
                    // Try to find this client in GameState for interpolation
                    for (const auto& pair : allPlayers) {
                        const Player& player = pair.second;
                        if (player.ipAddress == ip) {
                            // Use interpolated position for smooth movement
                            renderClientPos.x = player.getInterpolatedX(interpolationAlpha);
                            renderClientPos.y = player.getInterpolatedY(interpolationAlpha);
                            break;
                        }
                    }
                    
                    // Scale position for rendering
                    clientCircles[ip].setPosition(
                        renderClientPos.x * scaleX - 20.0f,
                        renderClientPos.y * scaleY - 20.0f
                    );
                }
            }

            // Render walls with fog of war effects and client players (blue circles, 20px) if visible
            renderFogOfWar(window, sf::Vector2f(serverPos.x, serverPos.y), 
                          gameMap.walls, clientCircles, clients, scaleX, scaleY);
            
            // Draw server player as green circle (radius 30px) - always visible
            window.draw(serverCircle);
            
            // Draw health bar above server player (green rectangle, 100 HP max)
            const float healthBarWidth = 60.0f; // Maximum width
            const float healthBarHeight = 8.0f;
            const float healthBarOffsetY = 45.0f; // Distance above player
            
            // Calculate current health bar width based on health percentage
            float healthPercentage = serverHealth / 100.0f;
            float currentHealthBarWidth = healthBarWidth * healthPercentage;
            
            // Health bar background (dark red)
            sf::RectangleShape healthBarBg(sf::Vector2f(healthBarWidth, healthBarHeight));
            healthBarBg.setFillColor(sf::Color(100, 0, 0));
            healthBarBg.setPosition(
                renderPos.x * scaleX - healthBarWidth / 2.0f,
                renderPos.y * scaleY - healthBarOffsetY
            );
            window.draw(healthBarBg);
            
            // Health bar foreground (green)
            if (currentHealthBarWidth > 0.0f) {
                sf::RectangleShape healthBar(sf::Vector2f(currentHealthBarWidth, healthBarHeight));
                healthBar.setFillColor(sf::Color::Green);
                healthBar.setPosition(
                    renderPos.x * scaleX - healthBarWidth / 2.0f,
                    renderPos.y * scaleY - healthBarOffsetY
                );
                window.draw(healthBar);
            }
            
            // Apply fog overlay with circular cutout around server player
            sf::Vector2f playerScreenPos(renderPos.x * scaleX, renderPos.y * scaleY);
            applyFogOverlay(window, playerScreenPos, scaleX, scaleY);
            
            // Draw score in top-left corner
            sf::Text scoreText;
            scoreText.setFont(font);
            scoreText.setString("Score: " + std::to_string(serverScore));
            scoreText.setCharacterSize(28);
            scoreText.setFillColor(sf::Color::White);
            scoreText.setPosition(20.0f, 20.0f);
            window.draw(scoreText);
            
            // Apply screen darkening effect if server player is dead
            if (!serverIsAlive) {
                sf::RectangleShape deathOverlay(sf::Vector2f(windowSize.x, windowSize.y));
                deathOverlay.setFillColor(sf::Color(0, 0, 0, 180)); // Dark semi-transparent overlay
                window.draw(deathOverlay);
            }
        }

        window.display();
        sf::sleep(sf::milliseconds(16));
    }

    // ��������� ������ UDP
    if (udpThreadStarted) {
        udpWorker.join();
    }

    return 0;
}