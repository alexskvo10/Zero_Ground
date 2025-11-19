#include <SFML/Network.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <thread>
#include <mutex>
#include <sstream>
#include <memory>
#include <vector>
#include <cstring>

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
// Movement and Interpolation Constants
// ========================

const float MOVEMENT_SPEED = 5.0f; // 5 units per second

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

const float VISIBILITY_RADIUS = 25.0f; // 25 units visibility radius

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
        
        window.draw(wallShape);
    }
    
    // Draw server player only if within visibility radius
    if (serverConnected && isVisible(playerPos, serverPos, VISIBILITY_RADIUS)) {
        sf::CircleShape serverCircle(20.0f);
        serverCircle.setFillColor(sf::Color(0, 200, 0, 200));
        serverCircle.setOutlineColor(sf::Color(0, 100, 0));
        serverCircle.setOutlineThickness(2.0f);
        serverCircle.setPosition(
            serverPos.x * scaleX - 20.0f,
            serverPos.y * scaleY - 20.0f
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

std::mutex mutex;
Position clientPos = { 475.0f, 25.0f }; // Client spawn position (top-right)
Position clientPosPrevious = { 475.0f, 25.0f }; // Previous position for interpolation
Position serverPos = { 25.0f, 475.0f }; // Server spawn position
bool serverConnected = false;
std::string serverIP = "127.0.0.1";
GameMap clientGameMap; // Store map data received from server
std::unique_ptr<sf::TcpSocket> tcpSocket; // TCP socket for connection
std::string connectionMessage = "";
sf::Color connectionMessageColor = sf::Color::White;
bool udpRunning = true; // Flag to control UDP thread
sf::Clock lastPacketReceived; // Track last received packet for connection loss detection
uint32_t currentFrameID = 0; // Frame counter for position packets

// HUD variables
float clientHealth = 100.0f; // Client player health (0-100)
int clientScore = 0; // Client player score
bool clientIsAlive = true; // Client player alive status

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
    
    // Receive MapDataPacket header with timeout
    ErrorHandler::logInfo("Waiting for MapDataPacket from server...");
    
    // Set a timeout for receiving data
    tcpSocket->setBlocking(false);
    sf::Clock timeoutClock;
    const float timeout = 5.0f; // 5 second timeout
    
    MapDataPacket mapHeader;
    std::size_t received = 0;
    sf::Socket::Status receiveStatus = sf::Socket::NotReady;
    
    while (timeoutClock.getElapsedTime().asSeconds() < timeout) {
        receiveStatus = tcpSocket->receive(&mapHeader, sizeof(MapDataPacket), received);
        
        if (receiveStatus == sf::Socket::Done) {
            break;
        } else if (receiveStatus == sf::Socket::Disconnected || receiveStatus == sf::Socket::Error) {
            break;
        }
        
        sf::sleep(sf::milliseconds(10));
    }
    
    tcpSocket->setBlocking(true);
    
    ErrorHandler::logInfo("Receive status: " + std::to_string(static_cast<int>(receiveStatus)) + ", received bytes: " + std::to_string(received));
    
    if (receiveStatus != sf::Socket::Done) {
        if (timeoutClock.getElapsedTime().asSeconds() >= timeout) {
            ErrorHandler::logNetworkError("Receive MapDataPacket", "Timeout after " + std::to_string(timeout) + " seconds");
        }
        ErrorHandler::logTCPError("Receive MapDataPacket", receiveStatus, ip);
        connectionMessage = "Failed to receive map data";
        connectionMessageColor = sf::Color::Red;
        tcpSocket.reset();
        return false;
    }
    
    if (received != sizeof(MapDataPacket)) {
        std::ostringstream oss;
        oss << "MapDataPacket size mismatch - expected " << sizeof(MapDataPacket) 
            << " bytes, got " << received;
        ErrorHandler::handleInvalidPacket(oss.str(), ip);
        connectionMessage = "Invalid map data received";
        connectionMessageColor = sf::Color::Red;
        tcpSocket.reset();
        return false;
    }
    
    if (!validateMapData(mapHeader)) {
        connectionMessage = "Invalid map data received";
        connectionMessageColor = sf::Color::Red;
        tcpSocket.reset();
        return false;
    }
    
    ErrorHandler::logInfo("MapDataPacket received with " + std::to_string(mapHeader.wallCount) + " walls");
    
    // Receive all wall data
    clientGameMap.walls.clear();
    for (uint32_t i = 0; i < mapHeader.wallCount; ++i) {
        Wall wall;
        sf::Socket::Status wallStatus = tcpSocket->receive(&wall, sizeof(Wall), received);
        
        if (wallStatus != sf::Socket::Done) {
            ErrorHandler::logTCPError("Receive wall data", wallStatus, ip);
            connectionMessage = "Failed to receive wall data";
            connectionMessageColor = sf::Color::Red;
            tcpSocket.reset();
            return false;
        }
        
        if (received != sizeof(Wall)) {
            std::ostringstream oss;
            oss << "Wall data size mismatch - expected " << sizeof(Wall) 
                << " bytes, got " << received;
            ErrorHandler::handleInvalidPacket(oss.str(), ip);
            connectionMessage = "Failed to receive wall data";
            connectionMessageColor = sf::Color::Red;
            tcpSocket.reset();
            return false;
        }
        
        clientGameMap.walls.push_back(wall);
    }
    
    ErrorHandler::logInfo("Received all " + std::to_string(clientGameMap.walls.size()) + " walls");
    
    // Build quadtree for collision detection
    clientGameMap.spatialIndex = std::make_unique<Quadtree>(sf::FloatRect(0, 0, 500.0f, 500.0f));
    for (auto& wall : clientGameMap.walls) {
        clientGameMap.spatialIndex->insert(&wall);
    }
    ErrorHandler::logInfo("Quadtree built for collision detection");
    
    // Receive initial server position
    PositionPacket serverPosPacket;
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

    sf::CircleShape serverCircle(20.0f);
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
            size_t wallCount = clientGameMap.walls.size();
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
                
                // Apply collision detection with walls (radius 30px for client)
                const float playerRadius = 30.0f;
                newPos = resolveCollision(oldPos, newPos, playerRadius, clientGameMap);
                
                // Clamp to map boundaries [0, 500]
                newPos = clampToMapBounds(newPos, playerRadius);
                
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
            
            // Scale position for rendering (map is 500x500, window might be different)
            sf::Vector2u windowSize = window.getSize();
            float scaleX = windowSize.x / 500.0f;
            float scaleY = windowSize.y / 500.0f;
            
            // Get server position for fog of war rendering
            sf::Vector2f currentServerPos;
            bool isServerConnected = false;
            {
                std::lock_guard<std::mutex> lock(mutex);
                currentServerPos = sf::Vector2f(serverPos.x, serverPos.y);
                isServerConnected = serverConnected;
            }
            
            // Render walls with fog of war effects and server player (green circle, 20px) if visible
            renderFogOfWar(window, sf::Vector2f(clientPos.x, clientPos.y), 
                          clientGameMap.walls, currentServerPos, 
                          isServerConnected, scaleX, scaleY);
            
            // Draw local client player as blue circle (radius 30px) - always visible
            clientCircle.setRadius(30.0f);
            clientCircle.setFillColor(sf::Color::Blue);
            clientCircle.setOutlineColor(sf::Color(0, 0, 100));
            clientCircle.setOutlineThickness(3.0f);
            clientCircle.setPosition(renderPos.x * scaleX - 30.0f, renderPos.y * scaleY - 30.0f);
            window.draw(clientCircle);
            
            // Draw health bar above player (green rectangle, 100 HP max)
            const float healthBarWidth = 60.0f; // Maximum width
            const float healthBarHeight = 8.0f;
            const float healthBarOffsetY = 45.0f; // Distance above player
            
            // Calculate current health bar width based on health percentage
            float healthPercentage = clientHealth / 100.0f;
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
            
            // Apply fog overlay with circular cutout around player
            sf::Vector2f playerScreenPos(renderPos.x * scaleX, renderPos.y * scaleY);
            applyFogOverlay(window, playerScreenPos, scaleX, scaleY);
            
            // Draw score in top-left corner
            sf::Text scoreText;
            scoreText.setFont(font);
            scoreText.setString("Score: " + std::to_string(clientScore));
            scoreText.setCharacterSize(28);
            scoreText.setFillColor(sf::Color::White);
            scoreText.setPosition(20.0f, 20.0f);
            window.draw(scoreText);
            
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