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

// Global icon image (needs to persist for window lifetime)
sf::Image g_serverWindowIcon;

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
// NEW: Dynamic Map System (5000x5000)
// ========================

// Constants for the new cell-based map system
const float MAP_SIZE = 5100.0f;
const float CELL_SIZE = 100.0f;
const int GRID_SIZE = 51;  // 5100 / 100 = 51
const float PLAYER_SIZE = 20.0f;  // Diameter (radius = 10px)
const float WALL_WIDTH = 12.0f;
const float WALL_LENGTH = 100.0f;

// Fog of War visibility ranges
const float FOG_RANGE_1 = 210.0f;   // 100% visibility
const float FOG_RANGE_2 = 510.0f;   // 60% visibility
const float FOG_RANGE_3 = 930.0f;   // 40% visibility
const float FOG_RANGE_4 = 1020.0f;  // 20% visibility

// Wall types
enum class WallType : uint8_t {
    None = 0,      // No wall
    Concrete = 1,  // Concrete wall (gray)
    Wood = 2       // Wooden wall (brown)
};

// Cell structure for grid-based map
// Each cell can have walls on its four sides
// Walls are centered on cell boundaries (WALL_WIDTH/2 on each side)
struct Cell {
    WallType topWall = WallType::None;
    WallType rightWall = WallType::None;
    WallType bottomWall = WallType::None;
    WallType leftWall = WallType::None;
};

// ========================
// Fog of War System
// ========================

// Calculate alpha (transparency) based on distance from player
// Returns value from 0 (invisible) to 255 (fully visible)
sf::Uint8 calculateFogAlpha(float distance) {
    if (distance <= FOG_RANGE_1) {
        return 255;  // 100% visibility
    } else if (distance <= FOG_RANGE_2) {
        return 153;  // 60% visibility (255 * 0.6)
    } else if (distance <= FOG_RANGE_3) {
        return 102;  // 40% visibility (255 * 0.4)
    } else if (distance <= FOG_RANGE_4) {
        return 51;   // 20% visibility (255 * 0.2)
    } else {
        return 0;    // Invisible beyond FOG_RANGE_4
    }
}

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
        std::cerr << "\n========================================" << std::endl;
        std::cerr << "[CRITICAL ERROR] Map Generation Failed" << std::endl;
        std::cerr << "========================================" << std::endl;
        std::cerr << "Failed to generate a valid map after 10 attempts." << std::endl;
        std::cerr << "The map generation algorithm could not create a map" << std::endl;
        std::cerr << "where both spawn points are reachable from each other." << std::endl;
        std::cerr << "\nPossible causes:" << std::endl;
        std::cerr << "  - Too many walls blocking paths" << std::endl;
        std::cerr << "  - Random generation created isolated areas" << std::endl;
        std::cerr << "\nAction required:" << std::endl;
        std::cerr << "  - Restart the server to try again" << std::endl;
        std::cerr << "  - If problem persists, adjust wall generation probabilities" << std::endl;
        std::cerr << "========================================\n" << std::endl;
        
        // Exit with error code
        exit(1);
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
// NEW: Cell-Based Map Generation Functions
// ========================

// Set a wall on a specific side of a cell
// side: 0=top, 1=right, 2=bottom, 3=left
// type: WallType to set (Concrete or Wood)
void setWall(Cell& cell, int side, WallType type) {
    switch (side) {
        case 0: cell.topWall = type; break;
        case 1: cell.rightWall = type; break;
        case 2: cell.bottomWall = type; break;
        case 3: cell.leftWall = type; break;
    }
}

// Generate map using probabilistic algorithm
// Only cells where (i+j)%2==1 can have walls
// Probabilities: 60% - 1 wall, 25% - 2 walls, 15% - 0 walls
// Wall types: 70% concrete, 30% wood
void generateMap(std::vector<std::vector<Cell>>& grid) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> probDist(0, 99);  // 0-99 for percentages
    std::uniform_int_distribution<int> sideDist(0, 3);   // 0-3 for sides
    std::uniform_int_distribution<int> typeDist(0, 99);  // 0-99 for wall type
    
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            // Check condition: only cells where (i+j)%2==1 can generate walls
            if ((i + j) % 2 == 1) {
                int probability = probDist(gen);
                
                if (probability < 60) {
                    // 60% probability - create one wall on a random side
                    int side = sideDist(gen);
                    // Determine wall type: 70% concrete, 30% wood
                    WallType type = (typeDist(gen) < 70) ? WallType::Concrete : WallType::Wood;
                    setWall(grid[i][j], side, type);
                }
                else if (probability < 85) {
                    // 25% probability (60-84) - create two walls on different sides
                    int side1 = sideDist(gen);
                    int side2 = sideDist(gen);
                    
                    // Ensure the two sides are different
                    while (side2 == side1) {
                        side2 = sideDist(gen);
                    }
                    
                    // Each wall gets its own type
                    WallType type1 = (typeDist(gen) < 70) ? WallType::Concrete : WallType::Wood;
                    WallType type2 = (typeDist(gen) < 70) ? WallType::Concrete : WallType::Wood;
                    setWall(grid[i][j], side1, type1);
                    setWall(grid[i][j], side2, type2);
                }
                // 15% probability (85-99) - no walls (do nothing)
            }
        }
    }
}

// ========================
// NEW: BFS Path Validation Functions
// ========================

// Check if movement is possible from one cell to another
// Parameters:
//   from - Starting cell coordinates
//   to - Destination cell coordinates
//   grid - The map grid containing wall information
// Returns: true if movement is possible (no wall blocking), false otherwise
bool canMove(sf::Vector2i from, sf::Vector2i to, const std::vector<std::vector<Cell>>& grid) {
    // Calculate direction of movement
    int dx = to.x - from.x;
    int dy = to.y - from.y;
    
    // Check walls based on direction
    if (dx == 1) {
        // Moving right: check if there's a right wall in the 'from' cell
        return grid[from.x][from.y].rightWall == WallType::None;
    }
    else if (dx == -1) {
        // Moving left: check if there's a left wall in the 'from' cell
        return grid[from.x][from.y].leftWall == WallType::None;
    }
    else if (dy == 1) {
        // Moving down: check if there's a bottom wall in the 'from' cell
        return grid[from.x][from.y].bottomWall == WallType::None;
    }
    else if (dy == -1) {
        // Moving up: check if there's a top wall in the 'from' cell
        return grid[from.x][from.y].topWall == WallType::None;
    }
    
    // No movement or invalid movement
    return false;
}

// BFS algorithm to check if a path exists between two points
// Parameters:
//   start - Starting position in world coordinates (pixels)
//   end - Ending position in world coordinates (pixels)
//   grid - The map grid containing wall information
// Returns: true if a path exists, false otherwise
//
// BREADTH-FIRST SEARCH (BFS) ALGORITHM EXPLANATION:
// BFS explores all neighbors at the current depth before moving to the next depth level.
// It guarantees finding a path if one exists.
//
// How it works for the cell-based map:
// 1. Convert world coordinates to grid cell coordinates
// 2. Start from the starting cell
// 3. Explore all adjacent cells (up, down, left, right) that are reachable (no walls blocking)
// 4. Mark each visited cell to avoid revisiting
// 5. Continue until we reach the end cell or exhaust all paths
// 6. If we reach the end, a path exists
//
// Performance: O(n) where n = number of grid cells (167x167 = 27,889 cells max)
// Typical runtime: < 10ms for most maps
bool isPathExists(sf::Vector2i start, sf::Vector2i end, const std::vector<std::vector<Cell>>& grid) {
    // Create visited array to track explored cells
    std::vector<std::vector<bool>> visited(GRID_SIZE, std::vector<bool>(GRID_SIZE, false));
    
    // Queue for BFS traversal
    std::queue<sf::Vector2i> queue;
    
    // Convert world coordinates to grid cell coordinates
    sf::Vector2i startCell(static_cast<int>(start.x / CELL_SIZE), static_cast<int>(start.y / CELL_SIZE));
    sf::Vector2i endCell(static_cast<int>(end.x / CELL_SIZE), static_cast<int>(end.y / CELL_SIZE));
    
    // Clamp to valid grid bounds to prevent out-of-bounds access
    startCell.x = std::max(0, std::min(GRID_SIZE - 1, startCell.x));
    startCell.y = std::max(0, std::min(GRID_SIZE - 1, startCell.y));
    endCell.x = std::max(0, std::min(GRID_SIZE - 1, endCell.x));
    endCell.y = std::max(0, std::min(GRID_SIZE - 1, endCell.y));
    
    // Initialize BFS: add start cell to queue and mark as visited
    queue.push(startCell);
    visited[startCell.x][startCell.y] = true;
    
    // Direction vectors for 4-directional movement (up, right, down, left)
    const int dx[] = {0, 1, 0, -1};
    const int dy[] = {-1, 0, 1, 0};
    
    // BFS main loop: explore all reachable cells
    while (!queue.empty()) {
        sf::Vector2i current = queue.front();
        queue.pop();
        
        // Success: we reached the destination
        if (current == endCell) {
            return true;
        }
        
        // Explore all 4 adjacent cells
        for (int i = 0; i < 4; ++i) {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];
            
            // Check if neighbor is valid: within bounds and not visited
            if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE && !visited[nx][ny]) {
                // Check if we can move from current cell to neighbor cell (no wall blocking)
                sf::Vector2i neighbor(nx, ny);
                if (canMove(current, neighbor, grid)) {
                    visited[nx][ny] = true;
                    queue.push(neighbor);
                }
            }
        }
    }
    
    // Failed: no path exists between start and end
    return false;
}

// ========================
// NEW: Map Generation with Retry Logic
// ========================

// Generate a valid map with retry logic
// Attempts up to 10 times to generate a map where both spawn points are reachable
// Parameters:
//   grid - Reference to the grid to populate with walls
// Returns: true if successful, false if all attempts failed
//
// ALGORITHM:
// 1. Clear the grid (set all walls to false)
// 2. Generate walls using probabilistic algorithm
// 3. Check if path exists between spawn points using BFS
// 4. If path exists, success! Return true
// 5. If no path, try again (up to 10 attempts)
// 6. If all attempts fail, call handleMapGenerationFailure() and exit
//
// SPAWN POINTS:
// - Server spawn: (250, 4750) - bottom-left area of 5000x5000 map
// - Client spawn: (4750, 250) - top-right area of 5000x5000 map
// These are far apart to ensure interesting gameplay
bool generateValidMap(std::vector<std::vector<Cell>>& grid) {
    const int MAX_ATTEMPTS = 10;
    
    std::cout << "\n=== Starting Map Generation ===" << std::endl;
    std::cout << "Map size: " << MAP_SIZE << "x" << MAP_SIZE << " pixels" << std::endl;
    std::cout << "Grid size: " << GRID_SIZE << "x" << GRID_SIZE << " cells" << std::endl;
    std::cout << "Cell size: " << CELL_SIZE << " pixels" << std::endl;
    std::cout << "Maximum attempts: " << MAX_ATTEMPTS << std::endl;
    
    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        std::cout << "\n--- Attempt " << (attempt + 1) << " of " << MAX_ATTEMPTS << " ---" << std::endl;
        
        // Step 1: Clear the grid before each attempt
        std::cout << "Clearing grid..." << std::endl;
        for (int i = 0; i < GRID_SIZE; i++) {
            for (int j = 0; j < GRID_SIZE; j++) {
                grid[i][j] = Cell(); // Reset to default (all walls false)
            }
        }
        
        // Step 2: Generate walls using probabilistic algorithm
        std::cout << "Generating walls..." << std::endl;
        generateMap(grid);
        
        // Count generated walls for logging
        int wallCount = 0;
        int concreteCount = 0;
        int woodCount = 0;
        for (int i = 0; i < GRID_SIZE; i++) {
            for (int j = 0; j < GRID_SIZE; j++) {
                if (grid[i][j].topWall != WallType::None) {
                    wallCount++;
                    if (grid[i][j].topWall == WallType::Concrete) concreteCount++;
                    else woodCount++;
                }
                if (grid[i][j].rightWall != WallType::None) {
                    wallCount++;
                    if (grid[i][j].rightWall == WallType::Concrete) concreteCount++;
                    else woodCount++;
                }
                if (grid[i][j].bottomWall != WallType::None) {
                    wallCount++;
                    if (grid[i][j].bottomWall == WallType::Concrete) concreteCount++;
                    else woodCount++;
                }
                if (grid[i][j].leftWall != WallType::None) {
                    wallCount++;
                    if (grid[i][j].leftWall == WallType::Concrete) concreteCount++;
                    else woodCount++;
                }
            }
        }
        std::cout << "Generated " << wallCount << " walls (Concrete: " << concreteCount 
                  << ", Wood: " << woodCount << ")" << std::endl;
        std::cout << "Generated " << wallCount << " walls" << std::endl;
        
        // Step 3: Check if path exists between spawn points
        std::cout << "Validating connectivity..." << std::endl;
        sf::Vector2i serverSpawn(250, 4850);  // Bottom-left area
        sf::Vector2i clientSpawn(4850, 250);  // Top-right area
        
        std::cout << "Server spawn: (" << serverSpawn.x << ", " << serverSpawn.y << ")" << std::endl;
        std::cout << "Client spawn: (" << clientSpawn.x << ", " << clientSpawn.y << ")" << std::endl;
        
        if (isPathExists(serverSpawn, clientSpawn, grid)) {
            // Success! Path exists between spawn points
            std::cout << "\n✓ SUCCESS! Map generated successfully on attempt " << (attempt + 1) << std::endl;
            std::cout << "Path exists between spawn points" << std::endl;
            std::cout << "Total walls: " << wallCount << std::endl;
            std::cout << "================================\n" << std::endl;
            return true;
        }
        
        // Failed: no path exists, will retry
        std::cout << "✗ Failed: No path exists between spawn points" << std::endl;
        std::cout << "Regenerating..." << std::endl;
    }
    
    // All attempts failed
    std::cerr << "\n✗ FAILURE: All " << MAX_ATTEMPTS << " attempts exhausted" << std::endl;
    std::cerr << "Could not generate a valid map" << std::endl;
    
    // Call error handler which will exit the application
    ErrorHandler::handleMapGenerationFailure();
    
    // This line should never be reached due to exit() in ErrorHandler::handleMapGenerationFailure()
    return false;
}

// ========================
// Quadtree for Spatial Partitioning
// ========================
// 
// QUADTREE ALGORITHM EXPLANATION:
// A quadtree is a tree data structure where each internal node has exactly four children.
// It's used for spatial partitioning to efficiently query objects in a 2D space.
// 
// How it works:
// 1. Start with a root node covering the entire map (500x500 units)
// 2. When a node contains more than MAX_WALLS (10) walls and depth < MAX_DEPTH (5):
//    - Subdivide the node into 4 equal quadrants (children)
//    - Redistribute walls to appropriate children
// 3. Walls that span multiple quadrants remain in the parent node
// 4. Query operations only search relevant quadrants, dramatically reducing checks
// 
// Performance benefit:
// - Without quadtree: O(n) collision checks per player (check all walls)
// - With quadtree: O(log n) collision checks per player (only check nearby walls)
// - For 20 walls: reduces from 20 checks to ~3-5 checks per collision detection
//
struct Quadtree {
    sf::FloatRect bounds;
    std::vector<Wall*> walls;
    std::unique_ptr<Quadtree> children[4];
    const int MAX_WALLS = 10;
    const int MAX_DEPTH = 5;
    
    Quadtree(sf::FloatRect bounds) : bounds(bounds) {}
    
    // Insert a wall into the quadtree
    // Parameters:
    //   wall - Pointer to the wall to insert
    //   depth - Current depth in the tree (0 = root)
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
            if (walls.size() > static_cast<size_t>(MAX_WALLS) && depth < MAX_DEPTH) {
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

// BREADTH-FIRST SEARCH (BFS) ALGORITHM EXPLANATION:
// BFS is a graph traversal algorithm that explores all neighbors at the current depth
// before moving to nodes at the next depth level. It guarantees finding the shortest path.
//
// How it works for map validation:
// 1. Divide the 500x500 map into a 50x50 grid (10 units per cell)
// 2. Start from server spawn point (bottom-left corner)
// 3. Explore all adjacent cells (up, down, left, right) that don't contain walls
// 4. Mark each visited cell to avoid revisiting
// 5. Continue until we reach client spawn point (top-right corner) or exhaust all paths
// 6. If we reach the end, the map is valid (players can reach each other)
//
// Why BFS instead of DFS?
// - BFS guarantees finding a path if one exists
// - BFS finds the shortest path (though we only care about existence)
// - BFS is more predictable for this use case
//
// Performance: O(n) where n = number of grid cells (50x50 = 2500 cells max)
// Typical runtime: < 1ms for most maps
bool bfsPathExists(sf::Vector2f start, sf::Vector2f end, const GameMap& map) {
    // BFS on a grid with 10x10 unit cells
    const int gridSize = 50; // 500/10 = 50
    std::vector<std::vector<bool>> visited(gridSize, std::vector<bool>(gridSize, false));
    std::queue<sf::Vector2i> queue;
    
    // Convert world coordinates to grid coordinates
    sf::Vector2i startCell(static_cast<int>(start.x / 10), static_cast<int>(start.y / 10));
    sf::Vector2i endCell(static_cast<int>(end.x / 10), static_cast<int>(end.y / 10));
    
    // Clamp to valid grid bounds to prevent out-of-bounds access
    startCell.x = std::max(0, std::min(gridSize - 1, startCell.x));
    startCell.y = std::max(0, std::min(gridSize - 1, startCell.y));
    endCell.x = std::max(0, std::min(gridSize - 1, endCell.x));
    endCell.y = std::max(0, std::min(gridSize - 1, endCell.y));
    
    // Initialize BFS: add start cell to queue and mark as visited
    queue.push(startCell);
    visited[startCell.x][startCell.y] = true;
    
    // Direction vectors for 4-directional movement (right, down, left, up)
    const int dx[] = {0, 1, 0, -1};
    const int dy[] = {1, 0, -1, 0};
    
    // BFS main loop: explore all reachable cells
    while (!queue.empty()) {
        sf::Vector2i current = queue.front();
        queue.pop();
        
        // Success: we reached the destination
        if (current == endCell) return true;
        
        // Explore all 4 adjacent cells
        for (int i = 0; i < 4; ++i) {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];
            
            // Check if neighbor is valid: within bounds, not visited, and no wall
            if (nx >= 0 && nx < gridSize && ny >= 0 && ny < gridSize &&
                !visited[nx][ny] && !cellHasWall(nx, ny, map)) {
                visited[nx][ny] = true;
                queue.push(sf::Vector2i(nx, ny));
            }
        }
    }
    
    // Failed: no path exists between start and end
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
    std::uniform_real_distribution<float> lengthDist(8.0f, 60.0f); // Wall length 8-60 pixels
    std::uniform_real_distribution<float> thicknessDist(8.0f, 12.0f); // Wall thickness 8-12 pixels
    std::uniform_int_distribution<int> orientationDist(0, 1); // 0=horizontal, 1=vertical
    
    // Generate 15-25 random walls
    std::uniform_int_distribution<int> wallCountDist(15, 25);
    int numWalls = wallCountDist(gen);
    
    for (int i = 0; i < numWalls; ++i) {
        Wall wall;
        float length = lengthDist(gen);
        float thickness = thicknessDist(gen);
        int orientation = orientationDist(gen);
        
        wall.x = posDist(gen);
        wall.y = posDist(gen);
        
        if (orientation == 0) {
            // Horizontal wall
            wall.width = length;
            wall.height = thickness;
        } else {
            // Vertical wall
            wall.width = thickness;
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
    sf::Clock mapGenClock;
    
    for (int attempt = 0; attempt < 10; ++attempt) {
        GameMap map;
        generateWalls(map);
        
        if (validateConnectivity(map)) {
            float mapGenTime = mapGenClock.getElapsedTime().asMilliseconds();
            
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
            
            // Log map generation performance
            std::cout << "\n=== MAP GENERATION PERFORMANCE ===" << std::endl;
            std::cout << "Total Time: " << mapGenTime << "ms (target: <100ms)" << std::endl;
            std::cout << "Attempts: " << (attempt + 1) << std::endl;
            std::cout << "Walls Generated: " << map.walls.size() << std::endl;
            std::cout << "Coverage: " << coveragePercent << "%" << std::endl;
            
            if (mapGenTime > 100.0f) {
                std::cerr << "[WARNING] Map generation exceeded 100ms target!" << std::endl;
            } else {
                std::cout << "[SUCCESS] Map generation within target time" << std::endl;
            }
            std::cout << "===================================\n" << std::endl;
            
            return map;
        }
        
        ErrorHandler::logWarning("Map generation attempt " + std::to_string(attempt + 1) + " failed connectivity check");
    }
    
    ErrorHandler::handleMapGenerationFailure();
    throw std::runtime_error("Failed to generate valid map after 10 attempts");
}

// ========================
// Performance Monitoring System (moved here for use in collision detection)
// ========================

class PerformanceMonitor {
public:
    PerformanceMonitor() : frameCount_(0), elapsedTime_(0.0f), currentFPS_(0.0f),
                          totalCollisionTime_(0.0f), collisionSamples_(0),
                          totalNetworkBytesSent_(0), totalNetworkBytesReceived_(0),
                          networkSampleTime_(0.0f) {}
    
    // Update performance metrics each frame
    void update(float deltaTime, size_t playerCount, size_t wallCount) {
        frameCount_++;
        elapsedTime_ += deltaTime;
        networkSampleTime_ += deltaTime;
        
        // Calculate FPS every 1 second window
        if (elapsedTime_ >= 1.0f) {
            currentFPS_ = frameCount_ / elapsedTime_;
            
            // Calculate average collision detection time
            float avgCollisionTime = (collisionSamples_ > 0) ? 
                (totalCollisionTime_ / collisionSamples_) * 1000.0f : 0.0f; // Convert to ms
            
            // Calculate network bandwidth (bytes per second)
            float networkBandwidthSent = totalNetworkBytesSent_ / networkSampleTime_;
            float networkBandwidthReceived = totalNetworkBytesReceived_ / networkSampleTime_;
            
            // Log performance metrics
            std::cout << "\n=== PERFORMANCE METRICS ===" << std::endl;
            std::cout << "FPS: " << currentFPS_ << " (target: 55+)" << std::endl;
            std::cout << "Frame Time: " << (elapsedTime_ / frameCount_) * 1000.0f << "ms" << std::endl;
            std::cout << "Players: " << playerCount << std::endl;
            std::cout << "Walls: " << wallCount << std::endl;
            std::cout << "Avg Collision Detection: " << avgCollisionTime << "ms (target: <1ms)" << std::endl;
            std::cout << "Network Bandwidth Sent: " << networkBandwidthSent << " bytes/sec" << std::endl;
            std::cout << "Network Bandwidth Received: " << networkBandwidthReceived << " bytes/sec" << std::endl;
            
            // Calculate game thread load (more accurate than simple CPU usage)
            float frameTime = (elapsedTime_ / frameCount_) * 1000.0f;
            
            // Game thread load: percentage of frame budget used
            // At 60 FPS, we have 16.67ms per frame
            // This shows how much of that budget we're using
            float gameThreadLoad = (frameTime / 16.67f) * 100.0f;
            
            // Estimate actual CPU usage (more conservative)
            // Assumes game uses ~40% CPU at full frame budget
            float estimatedCPUUsage = (gameThreadLoad / 100.0f) * 40.0f;
            
            // Display both metrics for better understanding
            std::cout << "Game Thread Load: " << gameThreadLoad << "% of frame budget" << std::endl;
            std::cout << "Estimated CPU Usage: " << estimatedCPUUsage << "% (target: <40%)" << std::endl;
            
            // Add build type hint if performance is lower
            if (gameThreadLoad > 100.0f) {
                std::cout << "Note: Running Debug build? Release build typically 30-50% faster" << std::endl;
            }
            
            std::cout << "==========================\n" << std::endl;
            
            // Log warning if FPS drops below 55
            if (currentFPS_ < 55.0f) {
                logPerformanceWarning(playerCount, wallCount, avgCollisionTime, gameThreadLoad);
            }
            
            // Log warning if collision detection is too slow
            if (avgCollisionTime > 1.0f) {
                std::cerr << "[WARNING] Collision detection exceeds 1ms target: " 
                         << avgCollisionTime << "ms" << std::endl;
            }
            
            // Log warning if game thread load is too high (not CPU usage)
            if (gameThreadLoad > 110.0f) {
                std::cerr << "[WARNING] Game thread load exceeds frame budget: " 
                         << gameThreadLoad << "%" << std::endl;
                std::cerr << "  This means frames are taking longer than 16.67ms" << std::endl;
                std::cerr << "  Consider optimizing or using Release build" << std::endl;
            }
            
            // Reset counters for next window
            frameCount_ = 0;
            elapsedTime_ = 0.0f;
            totalCollisionTime_ = 0.0f;
            collisionSamples_ = 0;
            totalNetworkBytesSent_ = 0;
            totalNetworkBytesReceived_ = 0;
            networkSampleTime_ = 0.0f;
        }
    }
    
    // Record collision detection time
    void recordCollisionTime(float timeInSeconds) {
        totalCollisionTime_ += timeInSeconds;
        collisionSamples_++;
    }
    
    // Record network traffic
    void recordNetworkSent(size_t bytes) {
        totalNetworkBytesSent_ += bytes;
    }
    
    void recordNetworkReceived(size_t bytes) {
        totalNetworkBytesReceived_ += bytes;
    }
    
    // Get current FPS
    float getCurrentFPS() const {
        return currentFPS_;
    }
    
private:
    void logPerformanceWarning(size_t playerCount, size_t wallCount, float avgCollisionTime, float gameThreadLoad) {
        std::cerr << "[WARNING] Performance degradation detected!" << std::endl;
        std::cerr << "  FPS: " << currentFPS_ << " (target: 55+)" << std::endl;
        std::cerr << "  Players: " << playerCount << std::endl;
        std::cerr << "  Walls: " << wallCount << std::endl;
        std::cerr << "  Avg Collision Time: " << avgCollisionTime << "ms (target: <1ms)" << std::endl;
        
        float frameTime = (elapsedTime_ / frameCount_) * 1000.0f;
        std::cerr << "  Avg Frame Time: " << frameTime << "ms (target: 16.67ms)" << std::endl;
        std::cerr << "  Game Thread Load: " << gameThreadLoad << "% of frame budget" << std::endl;
        
        // Provide helpful suggestions
        if (gameThreadLoad > 100.0f) {
            std::cerr << "  Suggestion: Try Release build for better performance" << std::endl;
        }
        if (avgCollisionTime > 0.5f) {
            std::cerr << "  Suggestion: Check Quadtree optimization" << std::endl;
        }
    }
    
    int frameCount_;
    float elapsedTime_;
    float currentFPS_;
    float totalCollisionTime_;
    int collisionSamples_;
    size_t totalNetworkBytesSent_;
    size_t totalNetworkBytesReceived_;
    float networkSampleTime_;
};

// ========================
// Collision Detection System
// ========================
//
// CIRCLE-RECTANGLE COLLISION ALGORITHM:
// Players are represented as circles (radius 30 units), walls as rectangles.
// This algorithm efficiently detects if a circle intersects with a rectangle.
//
// How it works:
// 1. Find the closest point on the rectangle to the circle's center
//    - Clamp circle center X to rectangle's X range [wall.x, wall.x + width]
//    - Clamp circle center Y to rectangle's Y range [wall.y, wall.y + height]
// 2. Calculate distance from circle center to this closest point
// 3. If distance < radius, there's a collision
//
// Why this works:
// - If circle center is inside rectangle, closest point = center (distance = 0)
// - If circle center is outside, closest point is on rectangle edge or corner
// - This handles all cases: side collisions, corner collisions, and containment
//
// Performance: O(1) - constant time, very fast
//

// Check if a circle collides with a rectangle
bool circleRectCollision(sf::Vector2f center, float radius, const Wall& wall) {
    // Find the closest point on the rectangle to the circle center
    // std::max/min clamps the value to the rectangle's bounds
    float closestX = std::max(wall.x, std::min(center.x, wall.x + wall.width));
    float closestY = std::max(wall.y, std::min(center.y, wall.y + wall.height));
    
    // Calculate squared distance from circle center to closest point
    float dx = center.x - closestX;
    float dy = center.y - closestY;
    
    // Compare squared distances to avoid expensive sqrt() operation
    return (dx * dx + dy * dy) < (radius * radius);
}

// Resolve collision and return corrected position
sf::Vector2f resolveCollision(sf::Vector2f oldPos, sf::Vector2f newPos, float radius, const GameMap& map, PerformanceMonitor* perfMonitor = nullptr) {
    sf::Clock collisionClock;
    
    // Query nearby walls using quadtree
    sf::FloatRect queryArea(
        newPos.x - radius - 1.0f,
        newPos.y - radius - 1.0f,
        radius * 2 + 2.0f,
        radius * 2 + 2.0f
    );
    
    sf::Vector2f result = newPos;
    
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
                    result = oldPos;
                    break;
                }
                
                // Calculate how much to push back (radius - distance)
                float penetration = radius - distance;
                float pushX = (dx / distance) * penetration;
                float pushY = (dy / distance) * penetration;
                
                result = sf::Vector2f(newPos.x + pushX, newPos.y + pushY);
                break;
            }
        }
    }
    
    // Record collision detection time
    if (perfMonitor) {
        float collisionTime = collisionClock.getElapsedTime().asSeconds();
        perfMonitor->recordCollisionTime(collisionTime);
    }
    
    return result;
}

// Clamp position to map boundaries [0, 500]
sf::Vector2f clampToMapBounds(sf::Vector2f pos, float radius) {
    pos.x = std::max(radius, std::min(500.0f - radius, pos.x));
    pos.y = std::max(radius, std::min(500.0f - radius, pos.y));
    return pos;
}

// ========================
// NEW: Dynamic Camera System
// ========================

// Update camera to follow player position
// The camera keeps the player centered on screen while respecting map boundaries
// Parameters:
//   window - The render window to apply the view to
//   playerPosition - Current position of the player in world coordinates
//
// ALGORITHM:
// 1. Create a view with the same size as the window
// 2. Set the view center to the player's position
// 3. Clamp the camera to map boundaries to prevent showing areas outside the map
// 4. Apply the view to the window
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

// ========================
// Rounded Rectangle Helper
// ========================

// Create a rounded rectangle shape
// Parameters:
//   size - Size of the rectangle (width, height)
//   radius - Radius of the rounded corners
//   pointCount - Number of points per corner (higher = smoother, default = 8)
// Returns: ConvexShape representing a rounded rectangle
sf::ConvexShape createRoundedRectangle(sf::Vector2f size, float radius, unsigned int pointCount = 8) {
    // Clamp radius to not exceed half of the smaller dimension
    radius = std::min(radius, std::min(size.x, size.y) / 2.0f);
    
    // Total points: 4 corners * pointCount points per corner
    sf::ConvexShape shape;
    shape.setPointCount(pointCount * 4);
    
    // Helper lambda to add corner points
    auto addCorner = [&](unsigned int startIndex, sf::Vector2f center, float startAngle) {
        for (unsigned int i = 0; i < pointCount; ++i) {
            float angle = startAngle + (i * 90.0f / (pointCount - 1)) * 3.14159f / 180.0f;
            float x = center.x + radius * std::cos(angle);
            float y = center.y + radius * std::sin(angle);
            shape.setPoint(startIndex + i, sf::Vector2f(x, y));
        }
    };
    
    // Top-left corner (180° to 270°)
    addCorner(0, sf::Vector2f(radius, radius), 180.0f);
    
    // Top-right corner (270° to 360°)
    addCorner(pointCount, sf::Vector2f(size.x - radius, radius), 270.0f);
    
    // Bottom-right corner (0° to 90°)
    addCorner(pointCount * 2, sf::Vector2f(size.x - radius, size.y - radius), 0.0f);
    
    // Bottom-left corner (90° to 180°)
    addCorner(pointCount * 3, sf::Vector2f(radius, size.y - radius), 90.0f);
    
    return shape;
}

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
// NEW: Optimized Rendering System
// ========================

// Render only visible walls within camera's viewport
// Uses camera-based visibility culling for accurate rendering
// Parameters:
//   window - The render window to draw walls to
//   playerPosition - Current position of the player in world coordinates (unused but kept for compatibility)
//   grid - The cell grid containing wall information
//
// ALGORITHM:
// 1. Get current camera view to determine visible area
// 2. Calculate visible world bounds with padding
// 3. Convert to cell coordinates
// 4. Render walls only in the visible range
// 5. Center walls on cell boundaries (position - WALL_WIDTH/2)
//
// CAMERA-BASED CULLING:
// Instead of rendering based on player position, we render based on what the
// camera can actually see. This fixes the issue where walls disappear when
// the player is near map edges (camera stops but player position continues).
//
// ========================
// NEW: Fog of War Background Rendering
// ========================

// Render background with smooth fog of war gradient effect
// The background gets darker the further it is from the player (pixel-based gradient)
void renderFoggedBackground(sf::RenderWindow& window, sf::Vector2f playerPosition) {
    // Get current view to determine visible area
    sf::View currentView = window.getView();
    sf::Vector2f viewCenter = currentView.getCenter();
    sf::Vector2f viewSize = currentView.getSize();
    
    // Calculate visible world bounds with padding
    float padding = 200.0f;
    float minX = std::max(0.0f, viewCenter.x - viewSize.x / 2.0f - padding);
    float maxX = std::min(MAP_SIZE, viewCenter.x + viewSize.x / 2.0f + padding);
    float minY = std::max(0.0f, viewCenter.y - viewSize.y / 2.0f - padding);
    float maxY = std::min(MAP_SIZE, viewCenter.y + viewSize.y / 2.0f + padding);
    
    // Base background color (136, 101, 56)
    const sf::Color baseColor(136, 101, 56);
    
    // OPTIMIZED: Use larger chunks (50x50) for better performance
    // This reduces draw calls from ~10000 to ~400 per frame
    const float chunkSize = 50.0f;
    
    for (float x = minX; x < maxX; x += chunkSize) {
        for (float y = minY; y < maxY; y += chunkSize) {
            // Calculate center of chunk
            float chunkCenterX = x + chunkSize / 2.0f;
            float chunkCenterY = y + chunkSize / 2.0f;
            
            // Calculate distance from player to chunk center
            float dx = chunkCenterX - playerPosition.x;
            float dy = chunkCenterY - playerPosition.y;
            float distance = std::sqrt(dx * dx + dy * dy);
            
            // Calculate fog alpha (smooth gradient)
            sf::Uint8 alpha = calculateFogAlpha(distance);
            
            // Apply fog to background color
            sf::Color foggedColor(baseColor.r, baseColor.g, baseColor.b, alpha);
            
            // Create and draw background rectangle
            sf::RectangleShape bgRect(sf::Vector2f(chunkSize, chunkSize));
            bgRect.setPosition(x, y);
            bgRect.setFillColor(foggedColor);
            window.draw(bgRect);
        }
    }
}

// ========================
// NEW: Fog Overlay Effect
// ========================

// Render fog overlay that darkens everything far from player
// This creates a smooth vignette effect
void renderFogOverlay(sf::RenderWindow& window, sf::Vector2f playerPosition) {
    // Get current view
    sf::View currentView = window.getView();
    sf::Vector2f viewCenter = currentView.getCenter();
    sf::Vector2f viewSize = currentView.getSize();
    
    // Calculate visible world bounds
    float minX = viewCenter.x - viewSize.x / 2.0f;
    float maxX = viewCenter.x + viewSize.x / 2.0f;
    float minY = viewCenter.y - viewSize.y / 2.0f;
    float maxY = viewCenter.y + viewSize.y / 2.0f;
    
    // Render fog overlay in larger chunks for smooth gradient
    const float chunkSize = 100.0f;
    
    for (float x = minX; x < maxX; x += chunkSize) {
        for (float y = minY; y < maxY; y += chunkSize) {
            // Calculate center of chunk
            float chunkCenterX = x + chunkSize / 2.0f;
            float chunkCenterY = y + chunkSize / 2.0f;
            
            // Calculate distance from player
            float dx = chunkCenterX - playerPosition.x;
            float dy = chunkCenterY - playerPosition.y;
            float distance = std::sqrt(dx * dx + dy * dy);
            
            // Calculate fog darkness (inverse of visibility)
            // Close = transparent, far = dark
            sf::Uint8 darkness = 255 - calculateFogAlpha(distance);
            
            // Apply black fog overlay
            sf::Color fogColor(0, 0, 0, darkness);
            
            sf::RectangleShape fogRect(sf::Vector2f(chunkSize, chunkSize));
            fogRect.setPosition(x, y);
            fogRect.setFillColor(fogColor);
            window.draw(fogRect);
        }
    }
}

// VISIBLE RANGE:
// We calculate the visible area from the camera view and add 2 cells of padding
// to prevent walls from popping in/out at screen edges.
//
// WALL POSITIONING:
// Walls are centered on cell boundaries to ensure they align properly:
// - A 12-pixel wide wall on a boundary extends 6 pixels into each adjacent cell
// - This is why we use: position - WALL_WIDTH/2 for positioning
void renderVisibleWalls(sf::RenderWindow& window, sf::Vector2f playerPosition, const std::vector<std::vector<Cell>>& grid) {
    // Get current view to determine visible area
    sf::View currentView = window.getView();
    sf::Vector2f viewCenter = currentView.getCenter();
    sf::Vector2f viewSize = currentView.getSize();
    
    // Calculate visible world bounds with padding
    float padding = CELL_SIZE * 2.0f; // 2 cells padding to avoid pop-in
    float minX = viewCenter.x - viewSize.x / 2.0f - padding;
    float maxX = viewCenter.x + viewSize.x / 2.0f + padding;
    float minY = viewCenter.y - viewSize.y / 2.0f - padding;
    float maxY = viewCenter.y + viewSize.y / 2.0f + padding;
    
    // Convert to cell coordinates
    int startX = std::max(0, static_cast<int>(minX / CELL_SIZE));
    int startY = std::max(0, static_cast<int>(minY / CELL_SIZE));
    int endX = std::min(GRID_SIZE - 1, static_cast<int>(maxX / CELL_SIZE));
    int endY = std::min(GRID_SIZE - 1, static_cast<int>(maxY / CELL_SIZE));
    
    // Counter for rendered walls (for performance monitoring)
    int wallCount = 0;
    
    // Wall colors
    sf::Color concreteColor(150, 150, 150);  // Gray for concrete
    sf::Color woodColor(139, 90, 43);        // Brown for wood
    
    // Corner radius for rounded walls
    const float cornerRadius = 3.0f;
    
    // PERFORMANCE OPTIMIZATION: Cache wall shapes
    // Create shapes once and reuse them by only changing position and color
    // This avoids expensive shape creation every frame for every wall
    static sf::ConvexShape horizontalWall = createRoundedRectangle(sf::Vector2f(WALL_LENGTH, WALL_WIDTH), cornerRadius);
    static sf::ConvexShape verticalWall = createRoundedRectangle(sf::Vector2f(WALL_WIDTH, WALL_LENGTH), cornerRadius);
    static bool shapesInitialized = false;
    
    if (!shapesInitialized) {
        shapesInitialized = true;
    }
    
    // Iterate through visible cells and render their walls with smooth gradient
    for (int i = startX; i <= endX; i++) {
        for (int j = startY; j <= endY; j++) {
            // Calculate world position of this cell's top-left corner
            float x = i * CELL_SIZE;
            float y = j * CELL_SIZE;
            
            // Render topWall (horizontal wall on top boundary of cell)
            if (grid[i][j].topWall != WallType::None) {
                // Calculate center of this specific wall for smooth gradient
                float wallCenterX = x + WALL_LENGTH / 2.0f;
                float wallCenterY = y;
                float dx = wallCenterX - playerPosition.x;
                float dy = wallCenterY - playerPosition.y;
                float distance = std::sqrt(dx * dx + dy * dy);
                sf::Uint8 alpha = calculateFogAlpha(distance);
                
                if (alpha > 0) {
                    // Choose color based on wall type
                    sf::Color baseColor = (grid[i][j].topWall == WallType::Concrete) ? concreteColor : woodColor;
                    horizontalWall.setFillColor(sf::Color(baseColor.r, baseColor.g, baseColor.b, alpha));
                    horizontalWall.setPosition(x, y - WALL_WIDTH / 2.0f);
                    window.draw(horizontalWall);
                    wallCount++;
                }
            }
            
            // Render rightWall (vertical wall on right boundary of cell)
            if (grid[i][j].rightWall != WallType::None) {
                // Calculate center of this specific wall for smooth gradient
                float wallCenterX = x + CELL_SIZE;
                float wallCenterY = y + WALL_LENGTH / 2.0f;
                float dx = wallCenterX - playerPosition.x;
                float dy = wallCenterY - playerPosition.y;
                float distance = std::sqrt(dx * dx + dy * dy);
                sf::Uint8 alpha = calculateFogAlpha(distance);
                
                if (alpha > 0) {
                    // Choose color based on wall type
                    sf::Color baseColor = (grid[i][j].rightWall == WallType::Concrete) ? concreteColor : woodColor;
                    verticalWall.setFillColor(sf::Color(baseColor.r, baseColor.g, baseColor.b, alpha));
                    verticalWall.setPosition(x + CELL_SIZE - WALL_WIDTH / 2.0f, y);
                    window.draw(verticalWall);
                    wallCount++;
                }
            }
            
            // Render bottomWall (horizontal wall on bottom boundary of cell)
            if (grid[i][j].bottomWall != WallType::None) {
                // Calculate center of this specific wall for smooth gradient
                float wallCenterX = x + WALL_LENGTH / 2.0f;
                float wallCenterY = y + CELL_SIZE;
                float dx = wallCenterX - playerPosition.x;
                float dy = wallCenterY - playerPosition.y;
                float distance = std::sqrt(dx * dx + dy * dy);
                sf::Uint8 alpha = calculateFogAlpha(distance);
                
                if (alpha > 0) {
                    // Choose color based on wall type
                    sf::Color baseColor = (grid[i][j].bottomWall == WallType::Concrete) ? concreteColor : woodColor;
                    horizontalWall.setFillColor(sf::Color(baseColor.r, baseColor.g, baseColor.b, alpha));
                    horizontalWall.setPosition(x, y + CELL_SIZE - WALL_WIDTH / 2.0f);
                    window.draw(horizontalWall);
                    wallCount++;
                }
            }
            
            // Render leftWall (vertical wall on left boundary of cell)
            if (grid[i][j].leftWall != WallType::None) {
                // Calculate center of this specific wall for smooth gradient
                float wallCenterX = x;
                float wallCenterY = y + WALL_LENGTH / 2.0f;
                float dx = wallCenterX - playerPosition.x;
                float dy = wallCenterY - playerPosition.y;
                float distance = std::sqrt(dx * dx + dy * dy);
                sf::Uint8 alpha = calculateFogAlpha(distance);
                
                if (alpha > 0) {
                    // Choose color based on wall type
                    sf::Color baseColor = (grid[i][j].leftWall == WallType::Concrete) ? concreteColor : woodColor;
                    verticalWall.setFillColor(sf::Color(baseColor.r, baseColor.g, baseColor.b, alpha));
                    verticalWall.setPosition(x - WALL_WIDTH / 2.0f, y);
                    window.draw(verticalWall);
                    wallCount++;
                }
            }
        }
    }
    
    // Step 6: Debug output in debug builds only
    #ifdef _DEBUG
    std::cout << "Walls rendered: " << wallCount << std::endl;
    #endif
}

// ========================
// NEW: Cell-Based Collision System with Sliding
// ========================

// Helper function to check if a position collides with walls
bool checkCollision(sf::Vector2f pos, const std::vector<std::vector<Cell>>& grid) {
    sf::FloatRect playerRect(
        pos.x - PLAYER_SIZE / 2.0f,
        pos.y - PLAYER_SIZE / 2.0f,
        PLAYER_SIZE,
        PLAYER_SIZE
    );
    
    int playerCellX = static_cast<int>(pos.x / CELL_SIZE);
    int playerCellY = static_cast<int>(pos.y / CELL_SIZE);
    
    int startX = std::max(0, playerCellX - 1);
    int startY = std::max(0, playerCellY - 1);
    int endX = std::min(GRID_SIZE - 1, playerCellX + 1);
    int endY = std::min(GRID_SIZE - 1, playerCellY + 1);
    
    for (int i = startX; i <= endX; i++) {
        for (int j = startY; j <= endY; j++) {
            float x = i * CELL_SIZE;
            float y = j * CELL_SIZE;
            
            if (grid[i][j].topWall != WallType::None) {
                sf::FloatRect wallRect(x, y - WALL_WIDTH / 2.0f, WALL_LENGTH, WALL_WIDTH);
                if (playerRect.intersects(wallRect)) return true;
            }
            
            if (grid[i][j].rightWall != WallType::None) {
                sf::FloatRect wallRect(x + CELL_SIZE - WALL_WIDTH / 2.0f, y, WALL_WIDTH, WALL_LENGTH);
                if (playerRect.intersects(wallRect)) return true;
            }
            
            if (grid[i][j].bottomWall != WallType::None) {
                sf::FloatRect wallRect(x, y + CELL_SIZE - WALL_WIDTH / 2.0f, WALL_LENGTH, WALL_WIDTH);
                if (playerRect.intersects(wallRect)) return true;
            }
            
            if (grid[i][j].leftWall != WallType::None) {
                sf::FloatRect wallRect(x - WALL_WIDTH / 2.0f, y, WALL_WIDTH, WALL_LENGTH);
                if (playerRect.intersects(wallRect)) return true;
            }
        }
    }
    
    return false;
}

// Resolve collision with wall sliding
// This function implements smooth wall sliding when player moves diagonally into a wall
//
// ALGORITHM:
// 1. Check if new position collides
// 2. If no collision, return new position
// 3. If collision, try sliding along X axis (keep Y from newPos, X from oldPos)
// 4. If X-slide works, return that position
// 5. If X-slide fails, try sliding along Y axis (keep X from newPos, Y from oldPos)
// 6. If Y-slide works, return that position
// 7. If both fail, return old position (stuck against corner)
//
// SLIDING BEHAVIOR:
// - When moving diagonally into a horizontal wall, player slides horizontally
// - When moving diagonally into a vertical wall, player slides vertically
// - This creates smooth, intuitive movement along walls
// - No more getting stuck when pressing two keys at once
//
// PERFORMANCE:
// - Maximum 3 collision checks per frame (newPos, slideX, slideY)
// - Each check examines ~10-15 walls in nearby cells
// - Target: < 0.3ms per collision resolution
sf::Vector2f resolveCollisionCellBased(sf::Vector2f oldPos, sf::Vector2f newPos, const std::vector<std::vector<Cell>>& grid) {
    // Step 1: Check if new position collides
    if (!checkCollision(newPos, grid)) {
        // No collision, clamp to map boundaries and return
        newPos.x = std::max(PLAYER_SIZE / 2.0f, std::min(newPos.x, MAP_SIZE - PLAYER_SIZE / 2.0f));
        newPos.y = std::max(PLAYER_SIZE / 2.0f, std::min(newPos.y, MAP_SIZE - PLAYER_SIZE / 2.0f));
        return newPos;
    }
    
    // Step 2: Collision detected, try sliding along X axis
    // Keep the new X position, but use old Y position
    sf::Vector2f slideX(newPos.x, oldPos.y);
    if (!checkCollision(slideX, grid)) {
        // X-axis slide works! Player slides horizontally along the wall
        slideX.x = std::max(PLAYER_SIZE / 2.0f, std::min(slideX.x, MAP_SIZE - PLAYER_SIZE / 2.0f));
        slideX.y = std::max(PLAYER_SIZE / 2.0f, std::min(slideX.y, MAP_SIZE - PLAYER_SIZE / 2.0f));
        return slideX;
    }
    
    // Step 3: X-axis slide failed, try sliding along Y axis
    // Keep the new Y position, but use old X position
    sf::Vector2f slideY(oldPos.x, newPos.y);
    if (!checkCollision(slideY, grid)) {
        // Y-axis slide works! Player slides vertically along the wall
        slideY.x = std::max(PLAYER_SIZE / 2.0f, std::min(slideY.x, MAP_SIZE - PLAYER_SIZE / 2.0f));
        slideY.y = std::max(PLAYER_SIZE / 2.0f, std::min(slideY.y, MAP_SIZE - PLAYER_SIZE / 2.0f));
        return slideY;
    }
    
    // Step 4: Both slides failed (stuck in corner), return old position
    return oldPos;
}

// ========================
// NEW: Map Serialization Functions
// ========================

// Serialize the cell-based grid map into a byte buffer for network transmission
// Parameters:
//   grid - The cell grid to serialize
//   buffer - Output buffer to store serialized data
//
// SERIALIZATION FORMAT:
// The grid is serialized as a contiguous block of memory containing all Cell structures.
// Each Cell is 4 bytes (4 booleans), and we have GRID_SIZE × GRID_SIZE cells.
// Total size: 167 × 167 × 4 = 111,556 bytes (~109 KB)
//
// ALGORITHM:
// 1. Resize buffer to fit the entire grid
// 2. Use std::memcpy to copy grid data directly into buffer
// 3. This is efficient because Cell structure is POD (Plain Old Data)
//
// PERFORMANCE:
// - Memory copy operation: O(n) where n = grid size
// - Typical time: < 1ms for 109 KB
// - Network transmission time: ~10-50ms depending on connection
void serializeMap(const std::vector<std::vector<Cell>>& grid, std::vector<char>& buffer) {
    // Calculate total size needed for serialization
    // Each Cell is sizeof(Cell) bytes, and we have GRID_SIZE × GRID_SIZE cells
    size_t totalSize = GRID_SIZE * GRID_SIZE * sizeof(Cell);
    
    // Resize buffer to fit all data
    buffer.resize(totalSize);
    
    // Copy grid data into buffer
    // We need to copy row by row because std::vector<std::vector<>> is not contiguous
    size_t offset = 0;
    for (int i = 0; i < GRID_SIZE; i++) {
        // Copy entire row at once
        std::memcpy(buffer.data() + offset, grid[i].data(), GRID_SIZE * sizeof(Cell));
        offset += GRID_SIZE * sizeof(Cell);
    }
    
    std::cout << "[INFO] Map serialized: " << totalSize << " bytes (" 
              << (totalSize / 1024) << " KB)" << std::endl;
}

// Send the serialized map to a connected client via TCP
// Parameters:
//   clientSocket - TCP socket connected to the client
//   grid - The cell grid to send
// Returns: true if successful, false if any error occurred
//
// PROTOCOL:
// 1. Send data size as uint32_t (4 bytes)
// 2. Send serialized map data (variable size, ~109 KB)
//
// ERROR HANDLING:
// - Logs errors using ErrorHandler
// - Returns false on any transmission error
// - Client should disconnect and retry if this fails
//
// PERFORMANCE:
// - TCP ensures reliable delivery (retransmits if packets lost)
// - Typical transmission time: 10-50ms on LAN, 50-200ms on internet
// - Blocking operation: thread will wait until all data is sent
bool sendMapToClient(sf::TcpSocket& clientSocket, const std::vector<std::vector<Cell>>& grid) {
    std::cout << "[INFO] Preparing to send map to client..." << std::endl;
    
    // Step 1: Serialize the map into a byte buffer
    std::vector<char> mapData;
    serializeMap(grid, mapData);
    
    // Step 2: Send the size of the data first (4 bytes)
    uint32_t dataSize = static_cast<uint32_t>(mapData.size());
    std::cout << "[INFO] Sending map data size: " << dataSize << " bytes" << std::endl;
    
    sf::Socket::Status sizeStatus = clientSocket.send(&dataSize, sizeof(dataSize));
    if (sizeStatus != sf::Socket::Done) {
        ErrorHandler::logTCPError("Send map data size", sizeStatus, 
                                 clientSocket.getRemoteAddress().toString());
        return false;
    }
    
    std::cout << "[INFO] Map data size sent successfully" << std::endl;
    
    // Step 3: Send the actual map data
    std::cout << "[INFO] Sending map data..." << std::endl;
    
    sf::Socket::Status dataStatus = clientSocket.send(mapData.data(), mapData.size());
    if (dataStatus != sf::Socket::Done) {
        ErrorHandler::logTCPError("Send map data", dataStatus, 
                                 clientSocket.getRemoteAddress().toString());
        return false;
    }
    
    std::cout << "[INFO] Map data sent successfully to client" << std::endl;
    return true;
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

const float MOVEMENT_SPEED = 3.0f; // 3 units per second

// Linear interpolation function for smooth position transitions
float lerp(float start, float end, float alpha) {
    return start + (end - start) * alpha;
}

sf::Vector2f lerpPosition(sf::Vector2f start, sf::Vector2f end, float alpha) {
    return sf::Vector2f(lerp(start.x, end.x, alpha), lerp(start.y, end.y, alpha));
}

// ========================
// Global State
// ========================

std::mutex mutex;
Position serverPos = { 250.0f, 4850.0f }; // Server spawn position (bottom-left area of 5100x5100 map)
Position serverPosPrevious = { 250.0f, 4850.0f }; // Previous position for interpolation
float serverHealth = 100.0f; // Server player health (0-100)
int serverScore = 0; // Server player score
bool serverIsAlive = true; // Server player alive status
std::map<sf::IpAddress, Position> clients;
Position clientPos = { 4850.0f, 250.0f }; // Client position (for interpolation)
Position clientPosPrevious = { 4850.0f, 250.0f }; // Previous client position
Position clientPosTarget = { 4850.0f, 250.0f }; // Target client position (latest received)
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
void tcpListenerThread(sf::TcpListener* listener, const std::vector<std::vector<Cell>>* grid) {
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
                    
                    // Send cell-based map data using new serialization
                    if (sendMapToClient(*clientSocket, *grid)) {
                        ErrorHandler::logInfo("Successfully sent cell-based map to client");
                        
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
                        
                        // Send client's spawn position (top-right area of 5000x5000 map)
                        PositionPacket clientPosPacket;
                        clientPosPacket.x = 4750.0f;
                        clientPosPacket.y = 250.0f;
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
                    } else {
                        ErrorHandler::logTCPError("Send cell-based map data", sf::Socket::Error, clientIP);
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
void udpListenerThread(sf::UdpSocket* socket, PerformanceMonitor* perfMonitor) {
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
            // Track network bandwidth
            if (perfMonitor) {
                perfMonitor->recordNetworkReceived(received);
            }
            
            // Validate received position
            if (validatePosition(receivedPacket)) {
                // Update GameState with validated position
                gameState.updatePlayerPosition(receivedPacket.playerId, receivedPacket.x, receivedPacket.y);
                
                // Update client position with interpolation support
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    
                    // Store previous position for interpolation
                    clientPosPrevious.x = clientPosTarget.x;
                    clientPosPrevious.y = clientPosTarget.y;
                    
                    // Update target position (latest received)
                    clientPosTarget.x = receivedPacket.x;
                    clientPosTarget.y = receivedPacket.y;
                    
                    // Also update legacy clients map for backward compatibility
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
                
                // Track network bandwidth
                if (perfMonitor) {
                    perfMonitor->recordNetworkSent(sizeof(PositionPacket));
                }
                
                // Implement network culling: only send players within 25*CELL_SIZE (750 pixels) radius
                const float NETWORK_CULLING_RADIUS = 25.0f * CELL_SIZE;
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
                        
                        // Track network bandwidth
                        if (perfMonitor) {
                            perfMonitor->recordNetworkSent(sizeof(PositionPacket));
                        }
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
    
    // Restore window icon after recreation
    if (g_serverWindowIcon.getSize().x > 0) {
        window.setIcon(g_serverWindowIcon.getSize().x, g_serverWindowIcon.getSize().y, g_serverWindowIcon.getPixelsPtr());
    }
    
    // Reset view to default after window recreation
    // This ensures proper scaling when switching between fullscreen and windowed mode
    window.setView(window.getDefaultView());
}

int main() {
    // NEW: Grid for cell-based map system
    std::vector<std::vector<Cell>> grid(GRID_SIZE, std::vector<Cell>(GRID_SIZE));
    
    // Generate map at startup using new cell-based system with retry logic
    std::cout << "\n=== Server Startup: Map Generation ===" << std::endl;
    if (!generateValidMap(grid)) {
        // generateValidMap() calls handleMapGenerationFailure() which exits
        // This code should never be reached, but included for safety
        std::cerr << "[CRITICAL] Map generation failed, exiting..." << std::endl;
        return -1;
    }
    std::cout << "Map generation complete, server ready to start\n" << std::endl;
    
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
    
    // Start TCP listener thread (pass grid pointer for map synchronization)
    std::thread tcpListenerWorker(tcpListenerThread, &tcpListener, &grid);
    ErrorHandler::logInfo("TCP listener thread started");
    
    // Start ready listener thread
    std::thread readyListenerWorker(readyListenerThread);
    ErrorHandler::logInfo("Ready listener thread started");

    sf::VideoMode desktopMode = sf::VideoMode::getDesktopMode();
    sf::RenderWindow window(desktopMode, "Server", sf::Style::Fullscreen);
    window.setFramerateLimit(60);
    
    // Load and set window icon
    if (g_serverWindowIcon.loadFromFile("Icon.png")) {
        window.setIcon(g_serverWindowIcon.getSize().x, g_serverWindowIcon.getSize().y, g_serverWindowIcon.getPixelsPtr());
    } else {
        std::cerr << "Warning: Failed to load icon!" << std::endl;
    }
    
    // Store window size for later use
    sf::Vector2u windowSize = window.getSize();

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
    
    // Inventory system
    const int INVENTORY_SLOTS = 6;
    const float INVENTORY_SLOT_SIZE = 100.0f;
    const float INVENTORY_PADDING = 10.0f;
    bool inventoryOpen = false;
    float inventoryAnimationProgress = 0.0f; // 0.0 = closed, 1.0 = fully open
    sf::Clock inventoryAnimationClock;

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
    sf::CircleShape serverCircle(PLAYER_SIZE / 2.0f);
    serverCircle.setFillColor(sf::Color::Green);
    serverCircle.setOutlineColor(sf::Color(0, 100, 0));
    serverCircle.setOutlineThickness(3.0f);
    serverCircle.setPosition(serverPos.x - PLAYER_SIZE / 2.0f, serverPos.y - PLAYER_SIZE / 2.0f);

    std::map<sf::IpAddress, sf::CircleShape> clientCircles;
    
    // Clock for delta time calculation
    sf::Clock deltaClock;
    float interpolationAlpha = 0.0f;
    
    // Performance monitoring
    PerformanceMonitor perfMonitor;

    // Function to center UI elements on window resize
    auto centerElements = [&]() {
        sf::Vector2u winSize = window.getSize();

        // Center "СЕРВЕР ЗАПУЩЕН" text
        startText.setPosition(static_cast<float>(winSize.x) / 2.0f - startText.getLocalBounds().width / 2.0f,
            static_cast<float>(winSize.y) / 2.0f - 150.0f);

        // Center "IP сервера:" text
        ipText.setPosition(static_cast<float>(winSize.x) / 2.0f - ipText.getLocalBounds().width / 2.0f,
            static_cast<float>(winSize.y) / 2.0f - 50.0f);

        // Center status text (will be updated dynamically)
        statusText.setPosition(static_cast<float>(winSize.x) / 2.0f - statusText.getLocalBounds().width / 2.0f,
            static_cast<float>(winSize.y) / 2.0f + 20.0f);

        // Center NEXT button (legacy, not used in final version)
        nextButton.setPosition(static_cast<float>(winSize.x) / 2.0f - 100.0f, static_cast<float>(winSize.y) / 2.0f + 100.0f);
        buttonText.setPosition(static_cast<float>(winSize.x) / 2.0f - buttonText.getLocalBounds().width / 2.0f,
            static_cast<float>(winSize.y) / 2.0f + 100.0f + 10.0f);
            
        // Center PLAY button
        playButton.setPosition(static_cast<float>(winSize.x) / 2.0f - 100.0f, static_cast<float>(winSize.y) / 2.0f + 100.0f);
        playButtonText.setPosition(static_cast<float>(winSize.x) / 2.0f - playButtonText.getLocalBounds().width / 2.0f,
            static_cast<float>(winSize.y) / 2.0f + 100.0f + 10.0f);
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
            
            // Toggle inventory with E key (works in both English and Russian layouts)
            if (event.type == sf::Event::KeyPressed && serverState.load() == ServerState::MainScreen) {
                // E key on English layout or У key on Russian layout (same physical key)
                if (event.key.code == sf::Keyboard::E) {
                    inventoryOpen = !inventoryOpen;
                    inventoryAnimationClock.restart(); // Start animation
                    ErrorHandler::logInfo(inventoryOpen ? "Inventory opened" : "Inventory closed");
                }
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
                        udpWorker = std::thread(udpListenerThread, &udpSocket, &perfMonitor);
                        udpThreadStarted = true;
                        ErrorHandler::logInfo("UDP listener thread started for position synchronization");
                    }
                }
            }
        }

        // Clear window with black for menu screens, fogged background will be drawn in MainScreen
        window.clear(sf::Color::Black);

        if (serverState.load() == ServerState::StartScreen) {
            // Update status text from global state
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                statusText.setString(connectionStatus);
                statusText.setFillColor(connectionStatusColor);
                statusText.setPosition(static_cast<float>(window.getSize().x) / 2.0f - statusText.getLocalBounds().width / 2.0f,
                    static_cast<float>(window.getSize().y) / 2.0f + 20.0f);
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
            // Count walls in the grid for performance monitoring
            size_t wallCount = 0;
            for (int i = 0; i < GRID_SIZE; i++) {
                for (int j = 0; j < GRID_SIZE; j++) {
                    if (grid[i][j].topWall != WallType::None) wallCount++;
                    if (grid[i][j].rightWall != WallType::None) wallCount++;
                    if (grid[i][j].bottomWall != WallType::None) wallCount++;
                    if (grid[i][j].leftWall != WallType::None) wallCount++;
                }
            }
            perfMonitor.update(deltaTime, playerCount, wallCount);
            
            // NEW: Update camera to follow server player
            // This must be called before any rendering to ensure the view is set correctly
            updateCamera(window, sf::Vector2f(serverPos.x, serverPos.y));
            
            // Render fogged background (must be after camera update)
            renderFoggedBackground(window, sf::Vector2f(serverPos.x, serverPos.y));
            
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
                
                // NEW: Apply cell-based collision detection
                newPos = resolveCollisionCellBased(oldPos, newPos, grid);
                
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
            
            // Render visible walls using cell-based system
            renderVisibleWalls(window, sf::Vector2f(serverPos.x, serverPos.y), grid);
            
            // Update and render connected clients with interpolation
            {
                std::lock_guard<std::mutex> lock(mutex);
                
                // Interpolate client position for smooth movement
                // This prevents jerky movement when receiving position updates at 20Hz
                const float clientInterpolationSpeed = 15.0f; // Higher = faster catch-up
                float clientAlpha = std::min(1.0f, deltaTime * clientInterpolationSpeed);
                
                clientPos.x = lerp(clientPos.x, clientPosTarget.x, clientAlpha);
                clientPos.y = lerp(clientPos.y, clientPosTarget.y, clientAlpha);
                
                // Get all players from GameState for interpolated positions
                auto allPlayers = gameState.getAllPlayers();
                
                for (auto& pair : clients) {
                    sf::IpAddress ip = pair.first;

                    if (clientCircles.find(ip) == clientCircles.end()) {
                        clientCircles[ip] = sf::CircleShape(PLAYER_SIZE / 2.0f);
                        clientCircles[ip].setFillColor(sf::Color::Blue);
                        clientCircles[ip].setOutlineColor(sf::Color(0, 0, 100));
                        clientCircles[ip].setOutlineThickness(2.0f);
                    }

                    // Use interpolated position for smooth movement
                    sf::Vector2f renderClientPos(clientPos.x, clientPos.y);
                    
                    // Calculate distance from server player to client player
                    float dx = renderClientPos.x - renderPos.x;
                    float dy = renderClientPos.y - renderPos.y;
                    float distance = std::sqrt(dx * dx + dy * dy);
                    
                    // Calculate fog alpha
                    sf::Uint8 alpha = calculateFogAlpha(distance);
                    
                    // Only draw if visible
                    if (alpha > 0) {
                        // Apply fog to client player color
                        sf::Color foggedColor = sf::Color::Blue;
                        foggedColor.a = alpha;
                        clientCircles[ip].setFillColor(foggedColor);
                        
                        sf::Color foggedOutline = sf::Color(0, 0, 100);
                        foggedOutline.a = alpha;
                        clientCircles[ip].setOutlineColor(foggedOutline);
                        
                        // Position client circle (centered on position)
                        clientCircles[ip].setPosition(
                            renderClientPos.x - PLAYER_SIZE / 2.0f,
                            renderClientPos.y - PLAYER_SIZE / 2.0f
                        );
                        
                        // Draw client player
                        window.draw(clientCircles[ip]);
                    }
                }
            }
            
            // Draw server player as green circle - always visible
            serverCircle.setRadius(PLAYER_SIZE / 2.0f);
            serverCircle.setPosition(renderPos.x - PLAYER_SIZE / 2.0f, renderPos.y - PLAYER_SIZE / 2.0f);
            window.draw(serverCircle);
            
            // Render fog overlay on top of everything (creates vignette effect)
            renderFogOverlay(window, renderPos);
            
            // Reset view to default for UI rendering
            // UI elements (score, health) need to be drawn in screen coordinates, not world coordinates
            sf::View uiView;
            uiView.setSize(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y));
            uiView.setCenter(static_cast<float>(window.getSize().x) / 2.0f, static_cast<float>(window.getSize().y) / 2.0f);
            window.setView(uiView);
            
            // Draw score in top-left corner
            sf::Text scoreText;
            scoreText.setFont(font);
            scoreText.setString("Score: " + std::to_string(serverScore));
            scoreText.setCharacterSize(28);
            scoreText.setFillColor(sf::Color::White);
            scoreText.setPosition(20.0f, 20.0f);
            window.draw(scoreText);
            
            // Draw health next to score
            sf::Text healthText;
            healthText.setFont(font);
            healthText.setString("Health: " + std::to_string(static_cast<int>(serverHealth)) + "/100");
            healthText.setCharacterSize(28);
            healthText.setFillColor(sf::Color::Green);
            healthText.setPosition(20.0f, 60.0f);
            window.draw(healthText);
            
            // Update inventory animation
            const float ANIMATION_DURATION = 0.3f; // 300ms animation
            if (inventoryOpen && inventoryAnimationProgress < 1.0f) {
                // Opening animation
                inventoryAnimationProgress = std::min(1.0f, inventoryAnimationClock.getElapsedTime().asSeconds() / ANIMATION_DURATION);
            } else if (!inventoryOpen && inventoryAnimationProgress > 0.0f) {
                // Closing animation
                inventoryAnimationProgress = std::max(0.0f, 1.0f - (inventoryAnimationClock.getElapsedTime().asSeconds() / ANIMATION_DURATION));
            }
            
            // Draw inventory with animation
            if (inventoryAnimationProgress > 0.0f) {
                // Smooth easing function (ease-out cubic)
                float easedProgress = 1.0f - std::pow(1.0f - inventoryAnimationProgress, 3.0f);
                
                // Calculate inventory position (centered at bottom)
                float inventoryWidth = INVENTORY_SLOTS * INVENTORY_SLOT_SIZE + (INVENTORY_SLOTS - 1) * INVENTORY_PADDING;
                float inventoryX = (windowSize.x - inventoryWidth) / 2.0f; // Center horizontally
                float inventoryY = windowSize.y - INVENTORY_SLOT_SIZE - 20.0f; // 20px margin from bottom
                
                // Ensure inventory doesn't go off-screen
                if (inventoryX < 20.0f) {
                    inventoryX = 20.0f; // Minimum 20px margin from left
                }
                
                // Slide up animation
                float slideOffset = (1.0f - easedProgress) * (INVENTORY_SLOT_SIZE + 40.0f);
                inventoryY += slideOffset;
                
                // Calculate alpha for fade-in effect
                sf::Uint8 alpha = static_cast<sf::Uint8>(easedProgress * 255);
                
                // Draw inventory slots
                #ifdef _DEBUG
                static bool debugPrinted = false;
                if (!debugPrinted && inventoryOpen) {
                    std::cout << "[DEBUG] Inventory rendering:" << std::endl;
                    std::cout << "  Window size: " << windowSize.x << "x" << windowSize.y << std::endl;
                    std::cout << "  Inventory width: " << inventoryWidth << std::endl;
                    std::cout << "  Inventory X: " << inventoryX << std::endl;
                    std::cout << "  Inventory Y: " << inventoryY << std::endl;
                    std::cout << "  Slots: " << INVENTORY_SLOTS << std::endl;
                    debugPrinted = true;
                }
                #endif
                
                for (int i = 0; i < INVENTORY_SLOTS; i++) {
                    float slotX = inventoryX + i * (INVENTORY_SLOT_SIZE + INVENTORY_PADDING);
                    
                    // Staggered animation - each slot appears slightly after the previous one
                    float slotDelay = i * 0.05f; // 50ms delay per slot
                    float slotProgress = std::max(0.0f, std::min(1.0f, (inventoryAnimationProgress - slotDelay) / (1.0f - slotDelay * INVENTORY_SLOTS)));
                    float slotEasedProgress = 1.0f - std::pow(1.0f - slotProgress, 3.0f);
                    sf::Uint8 slotAlpha = static_cast<sf::Uint8>(slotEasedProgress * 200);
                    
                    // Scale animation for each slot
                    float scale = 0.5f + slotEasedProgress * 0.5f; // Scale from 50% to 100%
                    float scaledSize = INVENTORY_SLOT_SIZE * scale;
                    float scaleOffset = (INVENTORY_SLOT_SIZE - scaledSize) / 2.0f;
                    
                    // Create slot rectangle
                    sf::RectangleShape slot(sf::Vector2f(scaledSize, scaledSize));
                    slot.setPosition(slotX + scaleOffset, inventoryY + scaleOffset);
                    slot.setFillColor(sf::Color(50, 50, 50, slotAlpha)); // Dark semi-transparent
                    slot.setOutlineColor(sf::Color(150, 150, 150, static_cast<sf::Uint8>(slotEasedProgress * 255))); // Light gray border
                    slot.setOutlineThickness(2.0f);
                    
                    window.draw(slot);
                    
                    // Draw slot number
                    if (slotEasedProgress > 0.3f) { // Show text after slot is 30% visible
                        sf::Text slotNumber;
                        slotNumber.setFont(font);
                        slotNumber.setString(std::to_string(i + 1));
                        slotNumber.setCharacterSize(static_cast<unsigned int>(32 * scale)); // Scale text with slot
                        slotNumber.setFillColor(sf::Color(255, 255, 255, static_cast<sf::Uint8>(slotEasedProgress * 255)));
                        
                        // Center the number in the slot
                        sf::FloatRect textBounds = slotNumber.getLocalBounds();
                        slotNumber.setPosition(
                            slotX + scaleOffset + (scaledSize - textBounds.width) / 2.0f - textBounds.left,
                            inventoryY + scaleOffset + (scaledSize - textBounds.height) / 2.0f - textBounds.top
                        );
                        
                        window.draw(slotNumber);
                    }
                }
            }
            
            // Apply screen darkening effect if server player is dead
            if (!serverIsAlive) {
                sf::RectangleShape deathOverlay(sf::Vector2f(static_cast<float>(windowSize.x), static_cast<float>(windowSize.y)));
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