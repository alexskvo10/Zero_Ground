#include <SFML/Network.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <array>
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

// Forward declaration for Weapon
struct Weapon;

struct Player {
    uint32_t id = 0;
    sf::IpAddress ipAddress;
    float x = 0.0f;
    float y = 0.0f;
    float previousX = 0.0f;
    float previousY = 0.0f;
    float rotation = 0.0f;  // Player rotation angle in degrees (0-360)
    float health = 100.0f;
    int score = 0;
    bool isAlive = true;
    bool isReady = false;
    sf::Color color = sf::Color::Blue;
    
    // Weapon system fields
    std::array<Weapon*, 4> inventory = {nullptr, nullptr, nullptr, nullptr};
    int activeSlot = -1;  // -1 means no weapon active
    int money = 50000;    // Starting money
    
    float getInterpolatedX(float alpha) const {
        return previousX + (x - previousX) * alpha;
    }
    
    float getInterpolatedY(float alpha) const {
        return previousY + (y - previousY) * alpha;
    }
    
    // Weapon system methods
    Weapon* getActiveWeapon() {
        if (activeSlot >= 0 && activeSlot < 4 && inventory[activeSlot] != nullptr) {
            return inventory[activeSlot];
        }
        return nullptr;
    }
    
    bool hasInventorySpace() const {
        for (int i = 0; i < 4; i++) {
            if (inventory[i] == nullptr) return true;
        }
        return false;
    }
    
    int getFirstEmptySlot() const {
        for (int i = 0; i < 4; i++) {
            if (inventory[i] == nullptr) return i;
        }
        return -1;
    }
    
    void addWeapon(Weapon* weapon) {
        int slot = getFirstEmptySlot();
        if (slot >= 0) {
            inventory[slot] = weapon;
        }
    }
    
    void switchWeapon(int slot) {
        if (slot >= 0 && slot < 4) {
            activeSlot = slot;
        }
    }
    
    float getMovementSpeed() const;  // Defined after Weapon struct
};

// ========================
// Weapon System Data Structures
// ========================

struct Weapon {
    enum Type {
        USP = 0, GLOCK = 1, FIVESEVEN = 2, R8 = 3,      // Pistols
        GALIL = 4, M4 = 5, AK47 = 6,                     // Rifles
        M10 = 7, AWP = 8, M40 = 9                        // Snipers
    };
    
    Type type;
    std::string name;
    int price;
    int magazineSize;
    int currentAmmo;
    int reserveAmmo;
    float damage;
    float range;              // Effective range in pixels
    float bulletSpeed;        // Pixels per second
    float reloadTime;         // Seconds
    float movementSpeed;      // Player speed modifier
    float fireRate;           // Shots per second (for automatic weapons)
    sf::Clock lastShotTime;   // For fire rate limiting
    bool isReloading;
    sf::Clock reloadClock;
    
    // Factory method to create weapons with proper stats
    static Weapon* create(Type type) {
        Weapon* w = new Weapon();
        w->type = type;
        w->isReloading = false;
        w->currentAmmo = 0;  // Will be set below
        w->reserveAmmo = 0;  // Will be set below
        
        switch (type) {
            case USP:
                w->name = "USP";
                w->price = 0;
                w->magazineSize = 12;
                w->damage = 15.0f;
                w->range = 250.0f;
                w->bulletSpeed = 600.0f;
                w->reloadTime = 2.0f;
                w->movementSpeed = 2.5f;
                w->reserveAmmo = 100;
                w->fireRate = 0.0f;  // Не автоматическое
                break;
            case GLOCK:
                w->name = "Glock-18";
                w->price = 1000;
                w->magazineSize = 20;
                w->damage = 10.0f;
                w->range = 300.0f;
                w->bulletSpeed = 600.0f;
                w->reloadTime = 2.0f;
                w->movementSpeed = 2.5f;
                w->reserveAmmo = 120;
                w->fireRate = 0.0f;  // Не автоматическое
                break;
            case FIVESEVEN:
                w->name = "Five-SeveN";
                w->price = 2500;
                w->magazineSize = 20;
                w->damage = 10.0f;
                w->range = 400.0f;
                w->bulletSpeed = 800.0f;
                w->reloadTime = 2.0f;
                w->movementSpeed = 2.5f;
                w->reserveAmmo = 100;
                w->fireRate = 0.0f;  // Не автоматическое
                break;
            case R8:
                w->name = "R8 Revolver";
                w->price = 4250;
                w->magazineSize = 8;
                w->damage = 50.0f;
                w->range = 200.0f;
                w->bulletSpeed = 700.0f;
                w->reloadTime = 5.0f;
                w->movementSpeed = 2.5f;
                w->reserveAmmo = 40;
                w->fireRate = 0.0f;  // Не автоматическое
                break;
            case GALIL:
                w->name = "Galil AR";
                w->price = 10000;
                w->magazineSize = 35;
                w->damage = 25.0f;
                w->range = 450.0f;
                w->bulletSpeed = 900.0f;
                w->reloadTime = 3.0f;
                w->movementSpeed = 2.0f;
                w->reserveAmmo = 140;
                w->fireRate = 10.0f;  // 10 выстрелов в секунду
                break;
            case M4:
                w->name = "M4";
                w->price = 15000;
                w->magazineSize = 30;
                w->damage = 30.0f;
                w->range = 425.0f;
                w->bulletSpeed = 850.0f;
                w->reloadTime = 3.0f;
                w->movementSpeed = 1.8f;
                w->reserveAmmo = 120;
                w->fireRate = 10.0f;  // 10 выстрелов в секунду
                break;
            case AK47:
                w->name = "AK-47";
                w->price = 17500;
                w->magazineSize = 25;
                w->damage = 35.0f;
                w->range = 450.0f;
                w->bulletSpeed = 900.0f;
                w->reloadTime = 3.0f;
                w->movementSpeed = 1.6f;
                w->reserveAmmo = 100;
                w->fireRate = 10.0f;  // 10 выстрелов в секунду
                break;
            case M10:
                w->name = "M10";
                w->price = 20000;
                w->magazineSize = 5;
                w->damage = 50.0f;
                w->range = 1000.0f;
                w->bulletSpeed = 2000.0f;
                w->reloadTime = 4.0f;
                w->movementSpeed = 1.1f;
                w->reserveAmmo = 25;
                w->fireRate = 0.0f;  // Не автоматическое
                break;
            case AWP:
                w->name = "AWP";
                w->price = 25000;
                w->magazineSize = 1;
                w->damage = 100.0f;
                w->range = 1000.0f;
                w->bulletSpeed = 2000.0f;
                w->reloadTime = 1.5f;
                w->movementSpeed = 1.0f;
                w->reserveAmmo = 10;
                w->fireRate = 0.0f;  // Не автоматическое
                break;
            case M40:
                w->name = "M40";
                w->price = 22000;
                w->magazineSize = 1;
                w->damage = 99.0f;
                w->range = 2000.0f;
                w->bulletSpeed = 4000.0f;
                w->reloadTime = 1.5f;
                w->movementSpeed = 1.2f;
                w->reserveAmmo = 10;
                w->fireRate = 0.0f;  // Не автоматическое
                break;
        }
        
        // Initialize with full magazine
        w->currentAmmo = w->magazineSize;
        
        return w;
    }
    
    bool canFire() const {
        return !isReloading && currentAmmo > 0;
    }
    
    bool isAutomatic() const {
        return fireRate > 0.0f;
    }
    
    bool canFireAutomatic() const {
        if (!canFire() || !isAutomatic()) return false;
        float timeSinceLastShot = lastShotTime.getElapsedTime().asSeconds();
        float fireInterval = 1.0f / fireRate;
        return timeSinceLastShot >= fireInterval;
    }
    
    void startReload() {
        if (reserveAmmo > 0 && currentAmmo < magazineSize) {
            isReloading = true;
            reloadClock.restart();
        }
    }
    
    void updateReload(float deltaTime) {
        if (isReloading && reloadClock.getElapsedTime().asSeconds() >= reloadTime) {
            // Transfer ammo from reserve to magazine
            int ammoNeeded = magazineSize - currentAmmo;
            int ammoToTransfer = std::min(ammoNeeded, reserveAmmo);
            currentAmmo += ammoToTransfer;
            reserveAmmo -= ammoToTransfer;
            isReloading = false;
        }
    }
    
    void fire() {
        if (canFire()) {
            currentAmmo--;
            lastShotTime.restart();
        }
    }
};

// ========================
// Player Methods Implementation
// ========================

// Get player movement speed based on active weapon
// Requirements: 5.3, 5.4
float Player::getMovementSpeed() const {
    Weapon* activeWeapon = const_cast<Player*>(this)->getActiveWeapon();
    if (activeWeapon != nullptr) {
        return activeWeapon->movementSpeed;
    }
    return 3.0f;  // Base speed when no weapon is active
}

// ========================
// Player Initialization
// ========================

// Initialize a player with starting equipment
// Requirements: 1.1, 1.2, 1.3
void initializePlayer(Player& player) {
    // Requirement 1.1: Equip USP in slot 0
    player.inventory[0] = Weapon::create(Weapon::USP);
    
    // Requirement 1.3: Initialize slots 1-3 as empty
    player.inventory[1] = nullptr;
    player.inventory[2] = nullptr;
    player.inventory[3] = nullptr;
    
    // Requirement 1.2: Set initial money to 50,000 (already set in struct default)
    player.money = 50000;
    
    // Set active slot to 0 (USP equipped)
    player.activeSlot = 0;
}

// ========================
// Constants for the cell-based map system
// ========================
const float MAP_SIZE = 5100.0f;
const float CELL_SIZE = 100.0f;
const int GRID_SIZE = 51;  // 5100 / 100 = 51
const float PLAYER_SIZE = 30.0f;  // Texture size 30x30 pixels (radius = 15px)
const float WALL_WIDTH = 12.0f;
const float WALL_LENGTH = 100.0f;

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

struct Shop {
    int gridX = 0;
    int gridY = 0;              // Position in 51×51 grid
    float worldX = 0.0f;
    float worldY = 0.0f;        // World coordinates (center of cell)
    
    // Check if player is in interaction range (60 pixels)
    bool isPlayerNear(float px, float py) const {
        float dx = worldX - px;
        float dy = worldY - py;
        return (dx*dx + dy*dy) <= (60.0f * 60.0f);
    }
};

struct Bullet {
    uint8_t ownerId = 0;           // Player who fired
    float x = 0.0f;
    float y = 0.0f;                // Current position
    float prevX = 0.0f;
    float prevY = 0.0f;            // Previous position (for ray casting)
    float vx = 0.0f;
    float vy = 0.0f;               // Velocity vector
    float damage = 0.0f;
    float range = 0.0f;            // Remaining range
    float maxRange = 0.0f;         // Initial range
    Weapon::Type weaponType;
    sf::Clock lifetime;
    
    // Update position
    void update(float deltaTime) {
        // Store previous position for ray casting
        prevX = x;
        prevY = y;
        
        // Update current position
        x += vx * deltaTime;
        y += vy * deltaTime;
        
        // Update remaining range
        float distanceTraveled = std::sqrt(vx * vx + vy * vy) * deltaTime;
        range -= distanceTraveled;
    }
    
    // Check if bullet should be removed
    bool shouldRemove() const {
        // Remove if exceeded range
        if (range <= 0.0f) return true;
        
        // Remove if outside map boundaries
        if (x < 0.0f || x > 5100.0f || y < 0.0f || y > 5100.0f) return true;
        
        return false;
    }
    
    // Requirement 7.3: Check collision with wall
    bool checkWallCollision(const Wall& wall) const {
        // Simple point-in-rectangle collision
        return (x >= wall.x && x <= wall.x + wall.width &&
                y >= wall.y && y <= wall.y + wall.height);
    }
    
    // Helper function to check if a line segment intersects with a rectangle
    bool lineIntersectsRect(float x1, float y1, float x2, float y2, 
                           float rectX, float rectY, float rectW, float rectH) const {
        // Check if either endpoint is inside the rectangle
        if ((x1 >= rectX && x1 <= rectX + rectW && y1 >= rectY && y1 <= rectY + rectH) ||
            (x2 >= rectX && x2 <= rectX + rectW && y2 >= rectY && y2 <= rectY + rectH)) {
            return true;
        }
        
        // Check if line intersects any of the four edges of the rectangle
        // Using line-line intersection algorithm
        auto lineIntersectsLine = [](float x1, float y1, float x2, float y2,
                                     float x3, float y3, float x4, float y4) -> bool {
            float denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
            if (std::abs(denom) < 0.0001f) return false; // Parallel lines
            
            float t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
            float u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / denom;
            
            return (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f);
        };
        
        // Check intersection with all four edges
        // Top edge
        if (lineIntersectsLine(x1, y1, x2, y2, rectX, rectY, rectX + rectW, rectY)) return true;
        // Right edge
        if (lineIntersectsLine(x1, y1, x2, y2, rectX + rectW, rectY, rectX + rectW, rectY + rectH)) return true;
        // Bottom edge
        if (lineIntersectsLine(x1, y1, x2, y2, rectX, rectY + rectH, rectX + rectW, rectY + rectH)) return true;
        // Left edge
        if (lineIntersectsLine(x1, y1, x2, y2, rectX, rectY, rectX, rectY + rectH)) return true;
        
        return false;
    }
    
    // Check collision with cell-based walls using ray casting (returns wall type if collision, None otherwise)
    // This method checks the trajectory from previous position to current position
    WallType checkCellWallCollision(const std::vector<std::vector<Cell>>& grid, 
                                    float prevX, float prevY) const {
        // Calculate which cells the bullet trajectory passes through
        int cellX1 = static_cast<int>(prevX / CELL_SIZE);
        int cellY1 = static_cast<int>(prevY / CELL_SIZE);
        int cellX2 = static_cast<int>(x / CELL_SIZE);
        int cellY2 = static_cast<int>(y / CELL_SIZE);
        
        // Determine the range of cells to check
        int minCellX = std::max(0, std::min(cellX1, cellX2) - 1);
        int maxCellX = std::min(GRID_SIZE - 1, std::max(cellX1, cellX2) + 1);
        int minCellY = std::max(0, std::min(cellY1, cellY2) - 1);
        int maxCellY = std::min(GRID_SIZE - 1, std::max(cellY1, cellY2) + 1);
        
        // Check all cells along the trajectory
        for (int i = minCellX; i <= maxCellX; i++) {
            for (int j = minCellY; j <= maxCellY; j++) {
                float cellWorldX = i * CELL_SIZE;
                float cellWorldY = j * CELL_SIZE;
                
                // Check top wall
                if (grid[i][j].topWall != WallType::None) {
                    float wallX = cellWorldX;
                    float wallY = cellWorldY - WALL_WIDTH / 2.0f;
                    if (lineIntersectsRect(prevX, prevY, x, y, wallX, wallY, WALL_LENGTH, WALL_WIDTH)) {
                        return grid[i][j].topWall;
                    }
                }
                
                // Check right wall
                if (grid[i][j].rightWall != WallType::None) {
                    float wallX = cellWorldX + CELL_SIZE - WALL_WIDTH / 2.0f;
                    float wallY = cellWorldY;
                    if (lineIntersectsRect(prevX, prevY, x, y, wallX, wallY, WALL_WIDTH, WALL_LENGTH)) {
                        return grid[i][j].rightWall;
                    }
                }
                
                // Check bottom wall
                if (grid[i][j].bottomWall != WallType::None) {
                    float wallX = cellWorldX;
                    float wallY = cellWorldY + CELL_SIZE - WALL_WIDTH / 2.0f;
                    if (lineIntersectsRect(prevX, prevY, x, y, wallX, wallY, WALL_LENGTH, WALL_WIDTH)) {
                        return grid[i][j].bottomWall;
                    }
                }
                
                // Check left wall
                if (grid[i][j].leftWall != WallType::None) {
                    float wallX = cellWorldX - WALL_WIDTH / 2.0f;
                    float wallY = cellWorldY;
                    if (lineIntersectsRect(prevX, prevY, x, y, wallX, wallY, WALL_WIDTH, WALL_LENGTH)) {
                        return grid[i][j].leftWall;
                    }
                }
            }
        }
        
        return WallType::None;
    }
    
    // Requirement 7.4: Check collision with player (circle collision)
    bool checkPlayerCollision(float playerX, float playerY, float playerRadius) const {
        float dx = x - playerX;
        float dy = y - playerY;
        float distanceSquared = dx * dx + dy * dy;
        return distanceSquared <= (playerRadius * playerRadius);
    }
};

// Requirement 8.2: Damage text visualization
struct DamageText {
    float x = 0.0f;
    float y = 0.0f;
    float damage = 0.0f;
    sf::Clock lifetime;
    
    // Check if damage text should be removed (after 1 second)
    bool shouldRemove() const {
        return lifetime.getElapsedTime().asSeconds() >= 1.0f;
    }
    
    // Get current Y position with upward animation
    float getAnimatedY() const {
        float elapsed = lifetime.getElapsedTime().asSeconds();
        // Move upward 50 pixels over 1 second
        return y - (elapsed * 50.0f);
    }
    
    // Get alpha for fade-out effect
    sf::Uint8 getAlpha() const {
        float elapsed = lifetime.getElapsedTime().asSeconds();
        // Fade out in last 0.3 seconds
        if (elapsed > 0.7f) {
            float fadeProgress = (elapsed - 0.7f) / 0.3f;
            return static_cast<sf::Uint8>(255 * (1.0f - fadeProgress));
        }
        return 255;
    }
};

// Purchase notification text visualization
struct PurchaseText {
    float x = 0.0f;
    float y = 0.0f;
    std::string weaponName;
    sf::Clock lifetime;
    
    // Check if purchase text should be removed (after 1.5 seconds)
    bool shouldRemove() const {
        return lifetime.getElapsedTime().asSeconds() >= 1.5f;
    }
    
    // Get current Y position with upward animation
    float getAnimatedY() const {
        float elapsed = lifetime.getElapsedTime().asSeconds();
        // Move upward 60 pixels over 1.5 seconds
        return y - (elapsed * 40.0f);
    }
    
    // Get alpha for fade-out effect
    sf::Uint8 getAlpha() const {
        float elapsed = lifetime.getElapsedTime().asSeconds();
        // Fade out in last 0.5 seconds
        if (elapsed > 1.0f) {
            float fadeProgress = (elapsed - 1.0f) / 0.5f;
            return static_cast<sf::Uint8>(255 * (1.0f - fadeProgress));
        }
        return 255;
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

// Fog of War visibility ranges
const float FOG_RANGE_1 = 210.0f;   // 100% visibility
const float FOG_RANGE_2 = 510.0f;   // 60% visibility
const float FOG_RANGE_3 = 930.0f;   // 40% visibility
const float FOG_RANGE_4 = 1020.0f;  // 20% visibility

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
// Shop Generation System
// ========================

// Generate 26 random non-overlapping shop positions on the 51×51 grid
// Parameters:
//   shops - Output vector to store generated shop positions
//   spawnPoints - Vector of spawn point positions to check distance from
//   grid - The cell grid to verify connectivity
// Returns: true if successful, false if failed after max attempts
//
// ALGORITHM:
// 1. Generate 26 random grid positions (0-50, 0-50)
// 2. Ensure no two shops occupy the same grid cell
// 3. Ensure each shop is at least 5 cells away from any spawn point
// 4. Verify map connectivity using BFS (shops shouldn't block paths)
// 5. If any constraint fails, retry (max 100 attempts)
// 6. If all attempts fail, use fallback pattern
//
// SPAWN DISTANCE CONSTRAINT:
// Shops must be at least 5 grid cells away from spawn points to ensure
// players have safe space to spawn without immediately being in a shop.
// Distance is calculated using Euclidean distance in grid coordinates.
//
// FALLBACK PATTERN:
// If random generation fails after 100 attempts, shops are placed in a
// predetermined grid pattern that guarantees valid placement.
bool generateShops(std::vector<Shop>& shops, const std::vector<sf::Vector2i>& spawnPoints, const std::vector<std::vector<Cell>>& grid) {
    const int NUM_SHOPS = 26;
    const int MAX_ATTEMPTS = 100;
    const int MIN_SPAWN_DISTANCE = 5;  // Minimum distance from spawn points in grid cells
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> gridDist(0, GRID_SIZE - 1);  // 0-50 for 51x51 grid
    
    std::cout << "\n=== Starting Shop Generation ===" << std::endl;
    std::cout << "Target shops: " << NUM_SHOPS << std::endl;
    std::cout << "Grid size: " << GRID_SIZE << "x" << GRID_SIZE << std::endl;
    std::cout << "Min spawn distance: " << MIN_SPAWN_DISTANCE << " cells" << std::endl;
    std::cout << "Maximum attempts: " << MAX_ATTEMPTS << std::endl;
    
    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        shops.clear();
        std::vector<std::pair<int, int>> usedPositions;
        bool attemptFailed = false;
        
        // Generate 26 shops
        for (int i = 0; i < NUM_SHOPS; ++i) {
            int maxRetries = 1000;  // Max retries for finding a valid position for this shop
            bool foundValidPosition = false;
            
            for (int retry = 0; retry < maxRetries; ++retry) {
                // Generate random grid position
                int gridX = gridDist(gen);
                int gridY = gridDist(gen);
                
                // Check if position is already occupied
                bool occupied = false;
                for (const auto& pos : usedPositions) {
                    if (pos.first == gridX && pos.second == gridY) {
                        occupied = true;
                        break;
                    }
                }
                
                if (occupied) {
                    continue;  // Try another position
                }
                
                // Check distance from spawn points
                bool tooCloseToSpawn = false;
                for (const auto& spawn : spawnPoints) {
                    // Convert spawn world coordinates to grid coordinates
                    int spawnGridX = static_cast<int>(spawn.x / CELL_SIZE);
                    int spawnGridY = static_cast<int>(spawn.y / CELL_SIZE);
                    
                    // Calculate distance in grid cells
                    int dx = gridX - spawnGridX;
                    int dy = gridY - spawnGridY;
                    float distance = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                    
                    if (distance < MIN_SPAWN_DISTANCE) {
                        tooCloseToSpawn = true;
                        break;
                    }
                }
                
                if (tooCloseToSpawn) {
                    continue;  // Try another position
                }
                
                // Valid position found!
                usedPositions.push_back(std::make_pair(gridX, gridY));
                
                // Create shop at this position
                Shop shop;
                shop.gridX = gridX;
                shop.gridY = gridY;
                // World coordinates: center of the cell (grid * CELL_SIZE + CELL_SIZE/2)
                shop.worldX = gridX * CELL_SIZE + CELL_SIZE / 2.0f;
                shop.worldY = gridY * CELL_SIZE + CELL_SIZE / 2.0f;
                shops.push_back(shop);
                
                foundValidPosition = true;
                break;
            }
            
            if (!foundValidPosition) {
                attemptFailed = true;
                break;
            }
        }
        
        if (attemptFailed) {
            continue;  // Try entire generation again
        }
        
        // Verify we generated exactly 26 shops
        if (shops.size() != NUM_SHOPS) {
            continue;
        }
        
        // Verify connectivity: check that shops don't block paths between spawn points
        // We already know the map has valid connectivity from generateValidMap,
        // so we just need to ensure shops are placed in accessible areas
        bool allShopsAccessible = true;
        for (const auto& shop : shops) {
            sf::Vector2i shopPos(static_cast<int>(shop.worldX), static_cast<int>(shop.worldY));
            
            // Check if shop position is accessible from at least one spawn point
            bool accessibleFromAnySpawn = false;
            for (const auto& spawn : spawnPoints) {
                if (isPathExists(spawn, shopPos, grid)) {
                    accessibleFromAnySpawn = true;
                    break;
                }
            }
            
            if (!accessibleFromAnySpawn) {
                allShopsAccessible = false;
                break;
            }
        }
        
        if (!allShopsAccessible) {
            continue;  // Try again
        }
        
        // Success! All constraints satisfied
        std::cout << "\n✓ SUCCESS! Shops generated successfully on attempt " << (attempt + 1) << std::endl;
        std::cout << "Total shops: " << shops.size() << std::endl;
        
        // Log some shop positions for verification
        std::cout << "Sample shop positions:" << std::endl;
        for (int i = 0; i < std::min(5, static_cast<int>(shops.size())); ++i) {
            std::cout << "  Shop " << (i + 1) << ": grid(" << shops[i].gridX << ", " << shops[i].gridY 
                      << ") world(" << shops[i].worldX << ", " << shops[i].worldY << ")" << std::endl;
        }
        std::cout << "================================\n" << std::endl;
        
        return true;
    }
    
    // All attempts failed, use fallback pattern
    std::cerr << "\n✗ WARNING: Random shop generation failed after " << MAX_ATTEMPTS << " attempts" << std::endl;
    std::cerr << "Using fallback shop placement pattern..." << std::endl;
    
    shops.clear();
    
    // Fallback: Place shops in a predetermined pattern
    // Use a grid pattern with spacing to ensure valid placement
    const int spacing = 10;  // 10 cells between shops
    int shopsPlaced = 0;
    
    for (int x = 5; x < GRID_SIZE && shopsPlaced < NUM_SHOPS; x += spacing) {
        for (int y = 5; y < GRID_SIZE && shopsPlaced < NUM_SHOPS; y += spacing) {
            // Check distance from spawn points
            bool tooCloseToSpawn = false;
            for (const auto& spawn : spawnPoints) {
                int spawnGridX = static_cast<int>(spawn.x / CELL_SIZE);
                int spawnGridY = static_cast<int>(spawn.y / CELL_SIZE);
                
                int dx = x - spawnGridX;
                int dy = y - spawnGridY;
                float distance = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                
                if (distance < MIN_SPAWN_DISTANCE) {
                    tooCloseToSpawn = true;
                    break;
                }
            }
            
            if (tooCloseToSpawn) {
                continue;
            }
            
            Shop shop;
            shop.gridX = x;
            shop.gridY = y;
            shop.worldX = x * CELL_SIZE + CELL_SIZE / 2.0f;
            shop.worldY = y * CELL_SIZE + CELL_SIZE / 2.0f;
            shops.push_back(shop);
            shopsPlaced++;
        }
    }
    
    std::cout << "✓ Fallback pattern placed " << shops.size() << " shops" << std::endl;
    std::cout << "================================\n" << std::endl;
    
    return true;
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
    float rotation = 0.0f;  // Player rotation angle in degrees
    float health = 100.0f;
    bool isAlive = true;
    uint32_t frameID = 0;
    uint8_t playerId = 0;
};

// ========================
// Weapon Shop Network Packets
// ========================

// Purchase packet (client → server)
// Requirement 4.1-4.5: Purchase validation and transaction
struct PurchasePacket {
    uint8_t playerId;
    uint8_t weaponType;  // Weapon::Type enum value
};

// Inventory update packet (server → all clients)
// Sent after successful purchase to synchronize inventory
struct InventoryPacket {
    uint8_t playerId;
    uint8_t slot;        // Which slot changed (0-3)
    uint8_t weaponType;  // Weapon::Type enum value, 255 = empty slot
    int newMoneyBalance;
};

// Requirement 7.6: Shot packet (client → server → all clients)
// Sent when player fires weapon
struct ShotPacket {
    uint8_t playerId;
    float x, y;          // Shot origin position
    float dirX, dirY;    // Normalized direction vector
    uint8_t weaponType;  // Weapon::Type enum value
    float bulletSpeed;   // Bullet speed for this weapon
    float damage;        // Damage for this weapon
    float range;         // Range for this weapon
};

// Requirement 10.4: Hit packet (server → all clients)
// Sent when bullet hits a player
struct HitPacket {
    uint8_t shooterId;   // Player who fired the bullet
    uint8_t victimId;    // Player who was hit
    float damage;        // Damage dealt
    float hitX, hitY;    // Position where hit occurred
    bool wasKill;        // True if this hit killed the victim
};

// Shop positions packet (server → clients)
// Sent after map generation to synchronize shop locations
struct ShopPositionsPacket {
    uint8_t shopCount;
    // Followed by shopCount * (gridX, gridY) pairs
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

// ========================
// Purchase Status System
// ========================

// Enum for purchase status
// Requirement 3.5: Show purchase status
enum class PurchaseStatus {
    Purchasable,
    InsufficientFunds,
    InventoryFull
};

// Calculate purchase status for a player and weapon
// Requirement 3.5: Purchase status calculation
PurchaseStatus calculatePurchaseStatus(const Player& player, const Weapon* weapon) {
    // Check if inventory is full
    if (!player.hasInventorySpace()) {
        return PurchaseStatus::InventoryFull;
    }
    
    // Check if player has sufficient funds
    if (player.money < weapon->price) {
        return PurchaseStatus::InsufficientFunds;
    }
    
    // Player can purchase
    return PurchaseStatus::Purchasable;
}

// Get purchase status text
std::string getPurchaseStatusText(PurchaseStatus status, int weaponPrice) {
    switch (status) {
        case PurchaseStatus::Purchasable:
            return "Can purchase";
        case PurchaseStatus::InsufficientFunds:
            return "Insufficient funds. Required: $" + std::to_string(weaponPrice);
        case PurchaseStatus::InventoryFull:
            return "Inventory full. Free a slot to purchase.";
        default:
            return "";
    }
}

// Get purchase status color
sf::Color getPurchaseStatusColor(PurchaseStatus status) {
    switch (status) {
        case PurchaseStatus::Purchasable:
            return sf::Color::Green;
        case PurchaseStatus::InsufficientFunds:
            return sf::Color::Red;
        case PurchaseStatus::InventoryFull:
            return sf::Color::Yellow;
        default:
            return sf::Color::White;
    }
}

// ========================
// Purchase Validation and Transaction Logic
// ========================

// Process weapon purchase request from client
// Requirements: 4.1, 4.2, 4.3, 4.4, 4.5
// Returns: true if purchase successful, false otherwise
bool processPurchase(Player& player, Weapon::Type weaponType) {
    // Create weapon to get price
    Weapon* weapon = Weapon::create(weaponType);
    
    // Requirement 4.1: Validate player has sufficient money
    if (player.money < weapon->price) {
        std::cout << "[PURCHASE] Player " << player.id << " has insufficient funds: " 
                  << player.money << " < " << weapon->price << std::endl;
        delete weapon;
        return false;
    }
    
    // Requirement 4.2: Validate player has empty inventory slot
    if (!player.hasInventorySpace()) {
        std::cout << "[PURCHASE] Player " << player.id << " has full inventory" << std::endl;
        delete weapon;
        return false;
    }
    
    // Get first empty slot
    int emptySlot = player.getFirstEmptySlot();
    if (emptySlot < 0) {
        // This shouldn't happen if hasInventorySpace() returned true, but check anyway
        std::cerr << "[ERROR] hasInventorySpace() returned true but getFirstEmptySlot() returned -1" << std::endl;
        delete weapon;
        return false;
    }
    
    // Requirement 4.3: Deduct weapon price from player money
    player.money -= weapon->price;
    
    // Requirement 4.4: Add weapon to first empty inventory slot
    player.inventory[emptySlot] = weapon;
    
    // Requirement 4.5: Weapon is already initialized with full magazine and reserve ammo
    // (This is done in Weapon::create())
    
    std::cout << "[PURCHASE] Player " << player.id << " purchased " << weapon->name 
              << " for $" << weapon->price << " in slot " << emptySlot 
              << ". New balance: $" << player.money << std::endl;
    
    return true;
}

// ========================
// Shop UI Rendering System
// ========================

// Render weapon tooltip with full stats
void renderWeaponTooltip(sf::RenderWindow& window, const Weapon* weapon, float mouseX, float mouseY, const sf::Font& font) {
    sf::Vector2u windowSize = window.getSize();
    
    // Tooltip dimensions
    const float TOOLTIP_WIDTH = 320.0f;  // Увеличено с 300 до 320 для лучшей читаемости
    const float TOOLTIP_HEIGHT = 290.0f;  // Высота для всех характеристик
    const float PADDING = 15.0f;
    
    // Position tooltip near mouse, but keep it on screen
    // Reserve space for UI elements at bottom (150px for "Press B" and "E - inventory")
    const float BOTTOM_UI_RESERVE = 150.0f;
    
    float tooltipX = mouseX + 20.0f;
    float tooltipY = mouseY + 20.0f;
    
    // Adjust if tooltip goes off screen
    if (tooltipX + TOOLTIP_WIDTH > windowSize.x - 10.0f) {
        tooltipX = mouseX - TOOLTIP_WIDTH - 20.0f;
    }
    if (tooltipY + TOOLTIP_HEIGHT > windowSize.y - BOTTOM_UI_RESERVE) {
        // Position above cursor if would overlap bottom UI
        tooltipY = mouseY - TOOLTIP_HEIGHT - 20.0f;
    }
    if (tooltipX < 10.0f) tooltipX = 10.0f;
    if (tooltipY < 10.0f) tooltipY = 10.0f;
    
    // Draw tooltip background
    sf::RectangleShape tooltipBg(sf::Vector2f(TOOLTIP_WIDTH, TOOLTIP_HEIGHT));
    tooltipBg.setPosition(tooltipX, tooltipY);
    tooltipBg.setFillColor(sf::Color(20, 20, 20, 240));
    tooltipBg.setOutlineColor(sf::Color(255, 215, 0));  // Gold border
    tooltipBg.setOutlineThickness(2.0f);
    window.draw(tooltipBg);
    
    float textY = tooltipY + PADDING;
    
    // Weapon name (title)
    sf::Text nameText;
    nameText.setFont(font);
    nameText.setString(weapon->name);
    nameText.setCharacterSize(24);
    nameText.setFillColor(sf::Color(255, 215, 0));  // Gold
    nameText.setStyle(sf::Text::Bold);
    nameText.setPosition(tooltipX + PADDING, textY);
    window.draw(nameText);
    textY += 35.0f;
    
    // Price
    sf::Text priceText;
    priceText.setFont(font);
    priceText.setString("Price: $" + std::to_string(weapon->price));
    priceText.setCharacterSize(20);
    priceText.setFillColor(sf::Color(100, 255, 100));  // Light green
    priceText.setPosition(tooltipX + PADDING, textY);
    window.draw(priceText);
    textY += 30.0f;
    
    // Separator line
    sf::RectangleShape separator(sf::Vector2f(TOOLTIP_WIDTH - 2 * PADDING, 1.0f));
    separator.setPosition(tooltipX + PADDING, textY);
    separator.setFillColor(sf::Color(100, 100, 100));
    window.draw(separator);
    textY += 10.0f;
    
    // Stats - format floats properly
    std::ostringstream reloadStream, moveStream;
    reloadStream << std::fixed << std::setprecision(1) << weapon->reloadTime;
    moveStream << std::fixed << std::setprecision(1) << weapon->movementSpeed;
    
    std::vector<std::pair<std::string, std::string>> stats = {
        {"Damage:", std::to_string(static_cast<int>(weapon->damage))},
        {"Magazine:", std::to_string(weapon->magazineSize)},
        {"Reserve Ammo:", std::to_string(weapon->reserveAmmo)},
        {"Range:", std::to_string(static_cast<int>(weapon->range)) + " px"},
        {"Bullet Speed:", std::to_string(static_cast<int>(weapon->bulletSpeed)) + " px/s"},
        {"Reload Time:", reloadStream.str() + " s"},
        {"Movement Speed:", moveStream.str()},
        {"Fire Mode:", weapon->isAutomatic() ? "Automatic (" + std::to_string(static_cast<int>(weapon->fireRate)) + " rps)" : "Semi-Auto"}
    };
    
    for (const auto& stat : stats) {
        sf::Text statLabel;
        statLabel.setFont(font);
        statLabel.setString(stat.first);
        statLabel.setCharacterSize(16);
        statLabel.setFillColor(sf::Color(200, 200, 200));
        statLabel.setPosition(tooltipX + PADDING, textY);
        window.draw(statLabel);
        
        sf::Text statValue;
        statValue.setFont(font);
        statValue.setString(stat.second);
        statValue.setCharacterSize(16);
        statValue.setFillColor(sf::Color::White);
        statValue.setStyle(sf::Text::Bold);
        statValue.setPosition(tooltipX + PADDING + 150.0f, textY);
        window.draw(statValue);
        
        textY += 22.0f;
    }
}

// Render shop UI with three columns
// Requirements: 3.2, 3.3, 3.4, 3.5
void renderShopUI(sf::RenderWindow& window, const Player& player, const sf::Font& font, float animationProgress) {
    sf::Vector2u windowSize = window.getSize();
    
    // Shop UI dimensions
    const float UI_WIDTH = 1000.0f;
    const float UI_HEIGHT = 700.0f;
    const float UI_X = (windowSize.x - UI_WIDTH) / 2.0f;
    const float UI_Y = (windowSize.y - UI_HEIGHT) / 2.0f;
    
    // Smooth easing function (ease-out cubic)
    float easedProgress = 1.0f - std::pow(1.0f - animationProgress, 3.0f);
    
    // Scale animation
    float scale = 0.7f + easedProgress * 0.3f;  // Scale from 70% to 100%
    float scaledWidth = UI_WIDTH * scale;
    float scaledHeight = UI_HEIGHT * scale;
    float scaledX = UI_X + (UI_WIDTH - scaledWidth) / 2.0f;
    float scaledY = UI_Y + (UI_HEIGHT - scaledHeight) / 2.0f;
    
    // Alpha for fade-in
    sf::Uint8 alpha = static_cast<sf::Uint8>(easedProgress * 230);
    
    // Draw semi-transparent background overlay
    sf::RectangleShape overlay(sf::Vector2f(windowSize.x, windowSize.y));
    overlay.setFillColor(sf::Color(0, 0, 0, static_cast<sf::Uint8>(easedProgress * 180)));
    window.draw(overlay);
    
    // Draw main shop panel
    sf::RectangleShape shopPanel(sf::Vector2f(scaledWidth, scaledHeight));
    shopPanel.setPosition(scaledX, scaledY);
    shopPanel.setFillColor(sf::Color(40, 40, 40, alpha));
    shopPanel.setOutlineColor(sf::Color(100, 100, 100, static_cast<sf::Uint8>(easedProgress * 255)));
    shopPanel.setOutlineThickness(3.0f);
    window.draw(shopPanel);
    
    // Draw title
    sf::Text titleText;
    titleText.setFont(font);
    titleText.setString("WEAPON SHOP");
    titleText.setCharacterSize(static_cast<unsigned int>(40 * scale));
    titleText.setFillColor(sf::Color(255, 255, 255, static_cast<sf::Uint8>(easedProgress * 255)));
    sf::FloatRect titleBounds = titleText.getLocalBounds();
    titleText.setPosition(
        scaledX + (scaledWidth - titleBounds.width) / 2.0f - titleBounds.left,
        scaledY + 20.0f * scale
    );
    window.draw(titleText);
    
    // Draw player money
    sf::Text moneyText;
    moneyText.setFont(font);
    moneyText.setString("Money: $" + std::to_string(player.money));
    moneyText.setCharacterSize(static_cast<unsigned int>(28 * scale));
    moneyText.setFillColor(sf::Color(100, 255, 100, static_cast<sf::Uint8>(easedProgress * 255)));
    moneyText.setPosition(scaledX + 20.0f * scale, scaledY + 70.0f * scale);
    window.draw(moneyText);
    
    // Column dimensions
    const float COLUMN_WIDTH = (scaledWidth - 80.0f * scale) / 3.0f;
    const float COLUMN_HEIGHT = scaledHeight - 150.0f * scale;
    const float COLUMN_Y = scaledY + 120.0f * scale;
    const float COLUMN_PADDING = 20.0f * scale;
    
    // Define weapon categories
    // Requirement 3.3: Three categories
    struct WeaponCategory {
        std::string name;
        std::vector<Weapon::Type> weapons;
    };
    
    std::vector<WeaponCategory> categories = {
        {"Pistols", {Weapon::USP, Weapon::GLOCK, Weapon::FIVESEVEN, Weapon::R8}},
        {"Rifles", {Weapon::GALIL, Weapon::M4, Weapon::AK47}},
        {"Snipers", {Weapon::M10, Weapon::AWP, Weapon::M40}}
    };
    
    // Variables to store hovered weapon for tooltip rendering at the end
    Weapon* hoveredWeapon = nullptr;
    float hoveredMouseX = 0.0f;
    float hoveredMouseY = 0.0f;
    
    // Draw each column
    for (size_t col = 0; col < categories.size(); col++) {
        float columnX = scaledX + 20.0f * scale + col * (COLUMN_WIDTH + COLUMN_PADDING);
        
        // Draw column background
        sf::RectangleShape columnBg(sf::Vector2f(COLUMN_WIDTH, COLUMN_HEIGHT));
        columnBg.setPosition(columnX, COLUMN_Y);
        columnBg.setFillColor(sf::Color(30, 30, 30, alpha));
        columnBg.setOutlineColor(sf::Color(80, 80, 80, static_cast<sf::Uint8>(easedProgress * 255)));
        columnBg.setOutlineThickness(2.0f);
        window.draw(columnBg);
        
        // Draw column title
        sf::Text columnTitle;
        columnTitle.setFont(font);
        columnTitle.setString(categories[col].name);
        columnTitle.setCharacterSize(static_cast<unsigned int>(26 * scale));
        columnTitle.setFillColor(sf::Color(255, 200, 100, static_cast<sf::Uint8>(easedProgress * 255)));
        sf::FloatRect columnTitleBounds = columnTitle.getLocalBounds();
        columnTitle.setPosition(
            columnX + (COLUMN_WIDTH - columnTitleBounds.width) / 2.0f - columnTitleBounds.left,
            COLUMN_Y + 10.0f * scale
        );
        window.draw(columnTitle);
        
        // Draw weapons in this column
        float weaponY = COLUMN_Y + 50.0f * scale;
        const float WEAPON_HEIGHT = 110.0f * scale;
        const float WEAPON_PADDING = 10.0f * scale;
        
        for (Weapon::Type weaponType : categories[col].weapons) {
            // Create weapon to get stats
            Weapon* weapon = Weapon::create(weaponType);
            
            // Calculate purchase status
            // Requirement 3.5: Show purchase status
            PurchaseStatus status = calculatePurchaseStatus(player, weapon);
            
            // Draw weapon panel
            sf::RectangleShape weaponPanel(sf::Vector2f(COLUMN_WIDTH - 20.0f * scale, WEAPON_HEIGHT));
            weaponPanel.setPosition(columnX + 10.0f * scale, weaponY);
            
            // Color based on purchase status
            if (status == PurchaseStatus::Purchasable) {
                weaponPanel.setFillColor(sf::Color(50, 70, 50, alpha));  // Green tint
                weaponPanel.setOutlineColor(sf::Color(100, 200, 100, static_cast<sf::Uint8>(easedProgress * 255)));
            } else {
                weaponPanel.setFillColor(sf::Color(50, 50, 50, alpha));  // Gray
                weaponPanel.setOutlineColor(sf::Color(100, 100, 100, static_cast<sf::Uint8>(easedProgress * 255)));
            }
            weaponPanel.setOutlineThickness(1.0f);
            window.draw(weaponPanel);
            
            // Draw weapon name
            // Requirement 3.4: Display weapon name
            sf::Text weaponName;
            weaponName.setFont(font);
            weaponName.setString(weapon->name);
            weaponName.setCharacterSize(static_cast<unsigned int>(20 * scale));
            weaponName.setFillColor(sf::Color(255, 255, 255, static_cast<sf::Uint8>(easedProgress * 255)));
            weaponName.setPosition(columnX + 15.0f * scale, weaponY + 5.0f * scale);
            window.draw(weaponName);
            
            // Draw weapon price
            // Requirement 3.4: Display price
            sf::Text weaponPrice;
            weaponPrice.setFont(font);
            weaponPrice.setString("$" + std::to_string(weapon->price));
            weaponPrice.setCharacterSize(static_cast<unsigned int>(18 * scale));
            weaponPrice.setFillColor(sf::Color(255, 215, 0, static_cast<sf::Uint8>(easedProgress * 255)));  // Gold color
            weaponPrice.setPosition(columnX + 15.0f * scale, weaponY + 28.0f * scale);
            window.draw(weaponPrice);
            
            // Draw weapon stats
            // Requirement 3.4: Display damage and magazine size
            sf::Text weaponStats;
            weaponStats.setFont(font);
            std::ostringstream statsStream;
            statsStream << "Damage: " << static_cast<int>(weapon->damage) << "\n";
            statsStream << "Magazine: " << weapon->magazineSize;
            weaponStats.setString(statsStream.str());
            weaponStats.setCharacterSize(static_cast<unsigned int>(16 * scale));
            weaponStats.setFillColor(sf::Color(200, 200, 200, static_cast<sf::Uint8>(easedProgress * 255)));
            weaponStats.setPosition(columnX + 15.0f * scale, weaponY + 50.0f * scale);
            window.draw(weaponStats);
            
            // Draw purchase status
            // Requirement 3.5: Show purchase status
            sf::Text statusText;
            statusText.setFont(font);
            statusText.setString(getPurchaseStatusText(status, weapon->price));
            statusText.setCharacterSize(static_cast<unsigned int>(14 * scale));
            sf::Color statusColor = getPurchaseStatusColor(status);
            statusText.setFillColor(sf::Color(statusColor.r, statusColor.g, statusColor.b, static_cast<sf::Uint8>(easedProgress * 255)));
            statusText.setPosition(columnX + 15.0f * scale, weaponY + 90.0f * scale);
            window.draw(statusText);
            
            // Check if mouse is hovering over this weapon slot
            sf::Vector2i mousePixelPos = sf::Mouse::getPosition(window);
            sf::FloatRect weaponBounds(
                columnX + 10.0f * scale,
                weaponY,
                COLUMN_WIDTH - 20.0f * scale,
                WEAPON_HEIGHT
            );
            
            // If hovering, store weapon for tooltip rendering at the end
            if (weaponBounds.contains(static_cast<float>(mousePixelPos.x), static_cast<float>(mousePixelPos.y))) {
                // Delete previous hovered weapon if exists
                if (hoveredWeapon != nullptr) {
                    delete hoveredWeapon;
                }
                // Store this weapon for tooltip (don't delete it yet)
                hoveredWeapon = weapon;
                hoveredMouseX = static_cast<float>(mousePixelPos.x);
                hoveredMouseY = static_cast<float>(mousePixelPos.y);
            } else {
                // Not hovering, delete weapon as usual
                delete weapon;
            }
            
            weaponY += WEAPON_HEIGHT + WEAPON_PADDING;
        }
    }
    
    // Render tooltip at the very end to ensure it's on top of all other UI elements
    if (hoveredWeapon != nullptr) {
        renderWeaponTooltip(window, hoveredWeapon, hoveredMouseX, hoveredMouseY, font);
        delete hoveredWeapon;  // Clean up after rendering
    }
}

// Check if player is near any shop and render interaction prompt
// Requirement 3.1: Display prompt when player within 60 pixels
void renderShopInteractionPrompt(sf::RenderWindow& window, sf::Vector2f playerPosition, 
                                 const std::vector<Shop>& shops, const sf::Font& font, bool shopUIOpen) {
    const float INTERACTION_RANGE = 60.0f;  // 60 pixels interaction range
    
    // Check if player is near any shop
    bool nearShop = false;
    for (const auto& shop : shops) {
        if (shop.isPlayerNear(playerPosition.x, playerPosition.y)) {
            nearShop = true;
            break;
        }
    }
    
    // Display prompt based on shop state
    if (nearShop || shopUIOpen) {
        // Get window size for UI positioning
        sf::Vector2u windowSize = window.getSize();
        
        // Create prompt text
        sf::Text promptText;
        promptText.setFont(font);
        // Show different text based on shop state
        promptText.setString(shopUIOpen ? "Press B to close" : "Press B to purchase");
        promptText.setCharacterSize(28);
        promptText.setFillColor(sf::Color::White);
        promptText.setOutlineColor(sf::Color::Black);
        promptText.setOutlineThickness(2.0f);
        
        // Position at bottom center of screen
        sf::FloatRect textBounds = promptText.getLocalBounds();
        promptText.setPosition(
            windowSize.x / 2.0f - textBounds.width / 2.0f - textBounds.left,
            windowSize.y - 120.0f  // 120px from bottom (above inventory hint)
        );
        
        window.draw(promptText);
    }
}

// Render shops with fog of war integration
// Requirements: 2.6, 3.1, 10.5
void renderShops(sf::RenderWindow& window, sf::Vector2f playerPosition, const std::vector<Shop>& shops) {
    const float SHOP_SIZE = 20.0f;  // 20×20 pixel red square
    const sf::Color shopColor(255, 0, 0);  // Red color for shops
    
    for (const auto& shop : shops) {
        // Calculate distance from player to shop center
        float dx = shop.worldX - playerPosition.x;
        float dy = shop.worldY - playerPosition.y;
        float distance = std::sqrt(dx * dx + dy * dy);
        
        // Calculate fog alpha (same as other entities for consistency)
        // Requirement 10.5: Fog of war consistency for shops
        sf::Uint8 alpha = calculateFogAlpha(distance);
        
        // Only render if visible (alpha > 0)
        if (alpha > 0) {
            // Create shop visual as 20×20 red square
            // Requirement 2.6: Render each shop as 20×20 red square at grid cell center
            sf::RectangleShape shopRect(sf::Vector2f(SHOP_SIZE, SHOP_SIZE));
            
            // Position at grid cell center (worldX and worldY are already at center)
            // Offset by half the shop size to center the square
            shopRect.setPosition(shop.worldX - SHOP_SIZE / 2.0f, shop.worldY - SHOP_SIZE / 2.0f);
            
            // Apply fog to shop color
            sf::Color foggedColor(shopColor.r, shopColor.g, shopColor.b, alpha);
            shopRect.setFillColor(foggedColor);
            
            window.draw(shopRect);
        }
    }
}

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

// Send shop positions to a connected client via TCP
// Parameters:
//   clientSocket - TCP socket connected to the client
//   shops - Vector of shops to send
// Returns: true if successful, false if any error occurred
//
// PROTOCOL:
// 1. Send shop count as uint8_t (1 byte)
// 2. For each shop, send gridX and gridY as int32_t (8 bytes per shop)
//
// ERROR HANDLING:
// - Logs errors using ErrorHandler
// - Returns false on any transmission error
bool sendShopsToClient(sf::TcpSocket& clientSocket, const std::vector<Shop>& shops) {
    std::cout << "[INFO] Preparing to send shops to client..." << std::endl;
    
    // Step 1: Send shop count
    uint8_t shopCount = static_cast<uint8_t>(shops.size());
    std::cout << "[INFO] Sending shop count: " << static_cast<int>(shopCount) << std::endl;
    
    sf::Socket::Status countStatus = clientSocket.send(&shopCount, sizeof(shopCount));
    if (countStatus != sf::Socket::Done) {
        ErrorHandler::logTCPError("Send shop count", countStatus, 
                                 clientSocket.getRemoteAddress().toString());
        return false;
    }
    
    // Step 2: Send each shop's grid position
    for (const auto& shop : shops) {
        struct ShopData {
            int32_t gridX;
            int32_t gridY;
        } shopData;
        
        shopData.gridX = shop.gridX;
        shopData.gridY = shop.gridY;
        
        sf::Socket::Status shopStatus = clientSocket.send(&shopData, sizeof(shopData));
        if (shopStatus != sf::Socket::Done) {
            ErrorHandler::logTCPError("Send shop data", shopStatus, 
                                     clientSocket.getRemoteAddress().toString());
            return false;
        }
    }
    
    std::cout << "[INFO] Shops sent successfully: " << static_cast<int>(shopCount) << " shops" << std::endl;
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
    
    // Requirement 8.1: Thread-safe apply damage to player
    void applyDamage(uint32_t playerId, float damage) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (players_.count(playerId)) {
            players_[playerId].health -= damage;
            if (players_[playerId].health < 0.0f) {
                players_[playerId].health = 0.0f;
            }
        }
    }
    
    // Requirement 8.3: Thread-safe check if player is dead
    bool isPlayerDead(uint32_t playerId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (players_.count(playerId)) {
            return players_.at(playerId).health <= 0.0f;
        }
        return false;
    }
    
    // Requirement 8.4: Thread-safe award money to player
    void awardMoney(uint32_t playerId, int amount) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (players_.count(playerId)) {
            players_[playerId].money += amount;
            ErrorHandler::logInfo("Player " + std::to_string(playerId) + 
                                 " awarded $" + std::to_string(amount) + 
                                 ". New balance: $" + std::to_string(players_[playerId].money));
        }
    }
    
    // Requirement 8.5: Thread-safe respawn player
    void respawnPlayer(uint32_t playerId, float x, float y) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (players_.count(playerId)) {
            players_[playerId].health = 100.0f;
            players_[playerId].isAlive = true;
            players_[playerId].x = x;
            players_[playerId].y = y;
            players_[playerId].previousX = x;
            players_[playerId].previousY = y;
        }
    }
    
    // Thread-safe set player alive status
    void setPlayerAlive(uint32_t playerId, bool alive) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (players_.count(playerId)) {
            players_[playerId].isAlive = alive;
        }
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
// Random Spawn Generation
// ========================

// Generate random spawn positions with minimum distance constraint
// Parameters:
//   grid - The cell grid to check for wall collisions
//   minDistance - Minimum distance between spawn points (in pixels)
// Returns: pair of positions (server spawn, client spawn)
//
// ALGORITHM:
// 1. Generate random position for server player
// 2. Check if position is valid (no wall collision)
// 3. Generate random position for client player
// 4. Check if position is valid and distance >= minDistance
// 5. Retry if constraints not met (max 100 attempts)
std::pair<Position, Position> generateRandomSpawns(const std::vector<std::vector<Cell>>& grid, float minDistance = 2100.0f) {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // Generate positions with margin from edges (at least 1 cell = 100 pixels)
    const float MARGIN = CELL_SIZE;
    std::uniform_real_distribution<float> xDist(MARGIN, MAP_SIZE - MARGIN);
    std::uniform_real_distribution<float> yDist(MARGIN, MAP_SIZE - MARGIN);
    
    const int MAX_ATTEMPTS = 100;
    
    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        // Generate server spawn position
        Position serverSpawn;
        serverSpawn.x = xDist(gen);
        serverSpawn.y = yDist(gen);
        
        // Check if server spawn is valid (no wall collision)
        if (checkCollision(sf::Vector2f(serverSpawn.x, serverSpawn.y), grid)) {
            continue; // Try again
        }
        
        // Generate client spawn position
        Position clientSpawn;
        clientSpawn.x = xDist(gen);
        clientSpawn.y = yDist(gen);
        
        // Check if client spawn is valid (no wall collision)
        if (checkCollision(sf::Vector2f(clientSpawn.x, clientSpawn.y), grid)) {
            continue; // Try again
        }
        
        // Check distance between spawns
        float dx = serverSpawn.x - clientSpawn.x;
        float dy = serverSpawn.y - clientSpawn.y;
        float distance = std::sqrt(dx * dx + dy * dy);
        
        if (distance >= minDistance) {
            // Success! Found valid spawn positions
            std::cout << "[INFO] Generated random spawns:" << std::endl;
            std::cout << "  Server spawn: (" << serverSpawn.x << ", " << serverSpawn.y << ")" << std::endl;
            std::cout << "  Client spawn: (" << clientSpawn.x << ", " << clientSpawn.y << ")" << std::endl;
            std::cout << "  Distance: " << distance << " pixels (" << (distance / CELL_SIZE) << " cells)" << std::endl;
            std::cout << "  Attempts: " << (attempt + 1) << std::endl;
            
            return std::make_pair(serverSpawn, clientSpawn);
        }
    }
    
    // Failed to find valid spawns, use fallback positions
    std::cerr << "[WARNING] Failed to generate random spawns after " << MAX_ATTEMPTS << " attempts" << std::endl;
    std::cerr << "  Using fallback spawn positions" << std::endl;
    
    Position serverSpawn = { 250.0f, 4850.0f };
    Position clientSpawn = { 4850.0f, 250.0f };
    
    return std::make_pair(serverSpawn, clientSpawn);
}

// ========================
// Global State
// ========================

std::mutex mutex;
Position serverPos = { 250.0f, 4850.0f }; // Server spawn position (will be randomized)
Position serverPosPrevious = { 250.0f, 4850.0f }; // Previous position for interpolation
float serverHealth = 100.0f; // Server player health (0-100)
int serverScore = 0; // Server player score
bool serverIsAlive = true; // Server player alive status
sf::Clock serverRespawnTimer; // Requirement 8.4: Timer for respawn
bool serverWaitingRespawn = false; // Requirement 8.3: Waiting for respawn flag
std::map<sf::IpAddress, Position> clients;
Position clientPos = { 4850.0f, 250.0f }; // Client position (will be randomized)
Position clientPosPrevious = { 4850.0f, 250.0f }; // Previous client position
Position clientPosTarget = { 4850.0f, 250.0f }; // Target client position (latest received)
float clientHealth = 100.0f; // Client player health (0-100)
int clientScore = 0; // Client player score
bool clientIsAlive = true; // Client player alive status
sf::Clock clientRespawnTimer; // Timer for client respawn
bool clientWaitingRespawn = false; // Waiting for client respawn flag
GameMap gameMap;
GameState gameState; // Thread-safe game state manager

// Server player with inventory and weapons
Player serverPlayer;

// Client player (for tracking opponent state)
Player clientPlayer;

// Shop system
std::vector<Shop> shops;

// Requirement 7.1: Store active bullets
std::vector<Bullet> activeBullets;
std::mutex bulletsMutex;

// Requirement 8.2: Store active damage texts
std::vector<DamageText> damageTexts;
std::mutex damageTextsMutex;

// Store active purchase notification texts
std::vector<PurchaseText> purchaseTexts;
std::mutex purchaseTextsMutex;

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

// ========================
// Weapon Firing System (Server)
// ========================

// Fire weapon and send shot packet to all clients
void fireWeaponServer(Player& player, const sf::RenderWindow& window, sf::UdpSocket& udpSocket, 
                      std::vector<Bullet>& activeBullets, std::mutex& bulletsMutex) {
    Weapon* activeWeapon = player.getActiveWeapon();
    if (activeWeapon == nullptr || !activeWeapon->canFire()) {
        return;
    }
    
    // Get mouse position in world coordinates
    sf::Vector2i mousePixelPos = sf::Mouse::getPosition(window);
    sf::Vector2f mouseWorldPos = window.mapPixelToCoords(mousePixelPos);
    
    // Calculate bullet trajectory
    float dx = mouseWorldPos.x - player.x;
    float dy = mouseWorldPos.y - player.y;
    float distance = std::sqrt(dx * dx + dy * dy);
    
    if (distance > 0.001f) {  // Avoid division by zero
        // Normalize direction
        dx /= distance;
        dy /= distance;
        
        // Create bullet
        Bullet bullet;
        bullet.ownerId = 1;  // Server player ID (1 for server, 0 for clients)
        bullet.x = player.x;
        bullet.y = player.y;
        bullet.prevX = player.x;  // Initialize previous position
        bullet.prevY = player.y;
        bullet.vx = dx * activeWeapon->bulletSpeed;
        bullet.vy = dy * activeWeapon->bulletSpeed;
        bullet.damage = activeWeapon->damage;
        bullet.range = activeWeapon->range;
        bullet.maxRange = activeWeapon->range;
        bullet.weaponType = activeWeapon->type;
        
        // Fire weapon (consumes ammo)
        activeWeapon->fire();
        
        // Add bullet to active bullets list
        {
            std::lock_guard<std::mutex> lock(bulletsMutex);
            
            // Count bullets owned by this player
            int playerBulletCount = 0;
            for (const auto& b : activeBullets) {
                if (b.ownerId == 1) playerBulletCount++;
            }
            
            // Only add if under limit
            if (playerBulletCount < 20) {
                activeBullets.push_back(bullet);
                ErrorHandler::logInfo("Bullet created! Total bullets: " + std::to_string(activeBullets.size()));
            } else {
                ErrorHandler::logInfo("Bullet limit reached (20)");
            }
        }
        
        // Send shot packet to all clients
        ShotPacket shotPacket;
        shotPacket.playerId = 1;  // Server player ID
        shotPacket.x = player.x;
        shotPacket.y = player.y;
        shotPacket.dirX = dx;
        shotPacket.dirY = dy;
        shotPacket.weaponType = static_cast<uint8_t>(activeWeapon->type);
        shotPacket.bulletSpeed = activeWeapon->bulletSpeed;
        shotPacket.damage = activeWeapon->damage;
        shotPacket.range = activeWeapon->range;
        
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            for (const auto& client : connectedClients) {
                if (client.socket && client.isReady) {
                    udpSocket.send(&shotPacket, sizeof(ShotPacket), client.address, 53002);
                }
            }
        }
        
        ErrorHandler::logInfo("Fired " + activeWeapon->name + " - Ammo: " + 
                             std::to_string(activeWeapon->currentAmmo) + "/" + 
                             std::to_string(activeWeapon->reserveAmmo));
    }
}

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
                        
                        // Send shop positions to client
                        if (!sendShopsToClient(*clientSocket, shops)) {
                            ErrorHandler::logTCPError("Send shop positions", sf::Socket::Error, clientIP);
                            return;
                        }
                        ErrorHandler::logInfo("Successfully sent shop positions to client");
                        
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
                        
                        // Send client's spawn position (randomly generated)
                        PositionPacket clientPosPacket;
                        clientPosPacket.x = clientPos.x;
                        clientPosPacket.y = clientPos.y;
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
        // Receive packets from clients (position or shot)
        char buffer[256]; // Buffer large enough for any packet type
        std::size_t received = 0;
        sf::IpAddress sender;
        unsigned short senderPort;
        
        sf::Socket::Status status = socket->receive(buffer, sizeof(buffer), received, sender, senderPort);
        
        if (status == sf::Socket::Done) {
            // Track network bandwidth
            if (perfMonitor) {
                perfMonitor->recordNetworkReceived(received);
            }
            
            // Determine packet type by size
            if (received == sizeof(PositionPacket)) {
                // Handle position packet
                PositionPacket* receivedPacket = reinterpret_cast<PositionPacket*>(buffer);
                
                // Validate received position
                if (validatePosition(*receivedPacket)) {
                    // Update GameState with validated position
                    gameState.updatePlayerPosition(receivedPacket->playerId, receivedPacket->x, receivedPacket->y);
                    
                    // Update client position with interpolation support
                    // IMPORTANT: Only update if client is alive or not waiting for respawn
                    // This prevents client from overwriting server-assigned respawn position
                    if (clientIsAlive && !clientWaitingRespawn) {
                        std::lock_guard<std::mutex> lock(mutex);
                        
                        // Store previous position for interpolation
                        clientPosPrevious.x = clientPosTarget.x;
                        clientPosPrevious.y = clientPosTarget.y;
                        
                        // Update target position (latest received)
                        clientPosTarget.x = receivedPacket->x;
                        clientPosTarget.y = receivedPacket->y;
                        
                        // Update client player rotation
                        clientPlayer.rotation = receivedPacket->rotation;
                        
                        // Also update legacy clients map for backward compatibility
                        Position pos;
                        pos.x = receivedPacket->x;
                        pos.y = receivedPacket->y;
                        clients[sender] = pos;
                    }
                }
            }
            else if (received == sizeof(ShotPacket)) {
                // Handle shot packet
                ShotPacket* shotPacket = reinterpret_cast<ShotPacket*>(buffer);
                
                ErrorHandler::logInfo("Received shot packet from client! Owner: " + std::to_string(shotPacket->playerId));
                
                // Create bullet on server
                Bullet bullet;
                bullet.ownerId = shotPacket->playerId;
                bullet.x = shotPacket->x;
                bullet.y = shotPacket->y;
                bullet.prevX = shotPacket->x;  // Initialize previous position
                bullet.prevY = shotPacket->y;
                bullet.vx = shotPacket->dirX * shotPacket->bulletSpeed;
                bullet.vy = shotPacket->dirY * shotPacket->bulletSpeed;
                bullet.damage = shotPacket->damage;
                bullet.range = shotPacket->range;
                bullet.maxRange = shotPacket->range;
                bullet.weaponType = static_cast<Weapon::Type>(shotPacket->weaponType);
                
                // Add bullet to active bullets list
                {
                    std::lock_guard<std::mutex> lock(bulletsMutex);
                    activeBullets.push_back(bullet);
                    ErrorHandler::logInfo("Client bullet added! Total bullets: " + std::to_string(activeBullets.size()));
                }
                
                // Broadcast shot packet to all clients
                {
                    std::lock_guard<std::mutex> lock(clientsMutex);
                    for (const auto& client : connectedClients) {
                        if (client.socket && client.isReady) {
                            socket->send(shotPacket, sizeof(ShotPacket), client.address, 53002);
                        }
                    }
                }
            }
            else {
                std::ostringstream oss;
                oss << "Unknown packet size from " << sender.toString() 
                    << " - received " << received << " bytes";
                ErrorHandler::handleInvalidPacket(oss.str());
            }
        } else if (status != sf::Socket::NotReady && status != sf::Socket::Done) {
            ErrorHandler::logUDPError("Receive packet", "Socket error occurred");
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
                serverPacket.rotation = serverPlayer.rotation;  // Send server player rotation
                serverPacket.health = serverHealth;
                serverPacket.isAlive = serverIsAlive;
                serverPacket.frameID = static_cast<uint32_t>(std::time(nullptr));
                serverPacket.playerId = 0; // Server is player 0
                
                // Send server position
                socket->send(&serverPacket, sizeof(PositionPacket), client.address, 53002);
                
                // Track network bandwidth
                if (perfMonitor) {
                    perfMonitor->recordNetworkSent(sizeof(PositionPacket));
                }
                
                // Send client's own position and health back to them
                // This is important so client knows their health (calculated on server)
                PositionPacket clientPacket;
                clientPacket.x = clientPos.x;
                clientPacket.y = clientPos.y;
                clientPacket.rotation = clientPlayer.rotation;  // Send client player rotation
                clientPacket.health = clientHealth;
                clientPacket.isAlive = clientIsAlive;
                clientPacket.frameID = static_cast<uint32_t>(std::time(nullptr));
                clientPacket.playerId = 1; // Client is player 1
                
                socket->send(&clientPacket, sizeof(PositionPacket), client.address, 53002);
                
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
    
    // Generate random spawn positions with minimum distance of 2100 pixels (21 cells)
    std::cout << "\n=== Generating Random Spawn Positions ===" << std::endl;
    auto spawns = generateRandomSpawns(grid, 2100.0f);
    serverPos = spawns.first;
    serverPosPrevious = spawns.first;
    clientPos = spawns.second;
    clientPosPrevious = spawns.second;
    clientPosTarget = spawns.second;
    std::cout << "Spawn generation complete\n" << std::endl;
    
    // Generate shops
    std::vector<sf::Vector2i> spawnPoints;
    spawnPoints.push_back(sf::Vector2i(static_cast<int>(serverPos.x), static_cast<int>(serverPos.y)));
    spawnPoints.push_back(sf::Vector2i(static_cast<int>(clientPos.x), static_cast<int>(clientPos.y)));
    
    if (!generateShops(shops, spawnPoints, grid)) {
        std::cerr << "[CRITICAL] Shop generation failed, exiting..." << std::endl;
        return -1;
    }
    std::cout << "Shop generation complete - Generated " << shops.size() << " shops\n" << std::endl;
    
    // Initialize server player with starting equipment
    // Requirements: 1.1, 1.2, 1.3
    initializePlayer(serverPlayer);
    serverPlayer.x = serverPos.x;
    serverPlayer.y = serverPos.y;
    
    // Debug: Check weapon initialization
    Weapon* usp = serverPlayer.inventory[0];
    if (usp != nullptr) {
        std::cout << "Server player initialized with:" << std::endl;
        std::cout << "  Weapon: " << usp->name << std::endl;
        std::cout << "  Ammo: " << usp->currentAmmo << "/" << usp->reserveAmmo << std::endl;
        std::cout << "  Active slot: " << serverPlayer.activeSlot << std::endl;
        std::cout << "  Money: $" << serverPlayer.money << "\n" << std::endl;
    } else {
        std::cout << "ERROR: Server player weapon is NULL!\n" << std::endl;
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
    
    // Shop system
    bool shopUIOpen = false;  // Shop UI state
    sf::Clock shopAnimationClock;
    float shopAnimationProgress = 0.0f;  // 0.0 = closed, 1.0 = fully open
    
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

    // Load player texture
    sf::Texture playerTexture;
    if (!playerTexture.loadFromFile("Nothing_1.png")) {
        std::cerr << "Failed to load player texture Nothing_1.png!" << std::endl;
        return -1;
    }
    
    // Load bullet texture
    sf::Texture bulletTexture;
    if (!bulletTexture.loadFromFile("Bullet.png")) {
        std::cerr << "Failed to load bullet texture Bullet.png!" << std::endl;
        return -1;
    }
    
    // Create server player sprite
    sf::Sprite serverSprite;
    serverSprite.setTexture(playerTexture);
    serverSprite.setOrigin(PLAYER_SIZE / 2.0f, PLAYER_SIZE / 2.0f); // Center origin
    serverSprite.setPosition(serverPos.x, serverPos.y);

    std::map<sf::IpAddress, sf::Sprite> clientSprites;
    
    // Clock for delta time calculation
    sf::Clock deltaClock;
    float interpolationAlpha = 0.0f;
    
    // Performance monitoring
    PerformanceMonitor perfMonitor;

    // Function to center UI elements on window resize
    auto centerElements = [&]() {
        sf::Vector2u winSize = window.getSize();

        // Center "SERVER RUNNING" text
        startText.setPosition(static_cast<float>(winSize.x) / 2.0f - startText.getLocalBounds().width / 2.0f,
            static_cast<float>(winSize.y) / 2.0f - 150.0f);

        // Center "Server IP:" text
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
        // Set camera view BEFORE processing events so mouse coordinates are correct
        if (serverState.load() == ServerState::MainScreen) {
            updateCamera(window, sf::Vector2f(serverPos.x, serverPos.y));
        }
        
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();

            // Handle Esc for fullscreen toggle
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                toggleFullscreen(window, isFullscreen, desktopMode);
                centerElements();
            }
            
            // Toggle inventory with E key (works in both English and Russian layouts)
            if (event.type == sf::Event::KeyPressed && serverState.load() == ServerState::MainScreen) {
                // E key for inventory
                if (event.key.code == sf::Keyboard::E) {
                    inventoryOpen = !inventoryOpen;
                    inventoryAnimationClock.restart(); // Start animation
                    ErrorHandler::logInfo(inventoryOpen ? "Inventory opened" : "Inventory closed");
                }
                
                // Toggle shop UI with B key when near a shop
                // Requirement 3.2: Handle B key press to open/close shop
                if (event.key.code == sf::Keyboard::B) {
                    // Check if player is near any shop
                    bool nearShop = false;
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        for (const auto& shop : shops) {
                            if (shop.isPlayerNear(serverPos.x, serverPos.y)) {
                                nearShop = true;
                                break;
                            }
                        }
                    }
                    
                    // Only toggle if near a shop or closing
                    if (nearShop || shopUIOpen) {
                        shopUIOpen = !shopUIOpen;
                        shopAnimationClock.restart();  // Start animation
                        ErrorHandler::logInfo(shopUIOpen ? "Shop UI opened" : "Shop UI closed");
                    }
                }
                
                // Weapon switching with keys 1-4
                // Requirements: 5.1, 5.2, 5.3, 5.4
                if (event.key.code == sf::Keyboard::Num1) {
                    serverPlayer.switchWeapon(0);
                    ErrorHandler::logInfo("Switched to weapon slot 1");
                }
                else if (event.key.code == sf::Keyboard::Num2) {
                    serverPlayer.switchWeapon(1);
                    ErrorHandler::logInfo("Switched to weapon slot 2");
                }
                else if (event.key.code == sf::Keyboard::Num3) {
                    serverPlayer.switchWeapon(2);
                    ErrorHandler::logInfo("Switched to weapon slot 3");
                }
                else if (event.key.code == sf::Keyboard::Num4) {
                    serverPlayer.switchWeapon(3);
                    ErrorHandler::logInfo("Switched to weapon slot 4");
                }
                
                // Manual reload with R key
                // Requirement 6.3: Handle R key for manual reload
                if (event.key.code == sf::Keyboard::R) {
                    Weapon* activeWeapon = serverPlayer.getActiveWeapon();
                    if (activeWeapon != nullptr) {
                        activeWeapon->startReload();
                        ErrorHandler::logInfo("Manual reload initiated for " + activeWeapon->name);
                    }
                }
            }
            
            // Handle mouse button press for shop purchases
            // Requirement 4: Handle weapon purchase in shop UI
            if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left && shopUIOpen) {
                sf::Vector2i mousePixelPos = sf::Mouse::getPosition(window);
                sf::Vector2u windowSize = window.getSize();
                
                // Shop UI dimensions (same as in renderShopUI)
                const float UI_WIDTH = 1000.0f;
                const float UI_HEIGHT = 700.0f;
                const float UI_X = (windowSize.x - UI_WIDTH) / 2.0f;
                const float UI_Y = (windowSize.y - UI_HEIGHT) / 2.0f;
                
                // Use current animation progress
                float easedProgress = 1.0f - std::pow(1.0f - shopAnimationProgress, 3.0f);
                float scale = 0.7f + easedProgress * 0.3f;
                float scaledWidth = UI_WIDTH * scale;
                float scaledHeight = UI_HEIGHT * scale;
                float scaledX = UI_X + (UI_WIDTH - scaledWidth) / 2.0f;
                float scaledY = UI_Y + (UI_HEIGHT - scaledHeight) / 2.0f;
                
                // Column dimensions
                const float COLUMN_WIDTH = (scaledWidth - 80.0f * scale) / 3.0f;
                const float COLUMN_HEIGHT = scaledHeight - 150.0f * scale;
                const float COLUMN_Y = scaledY + 120.0f * scale;
                const float COLUMN_PADDING = 20.0f * scale;
                const float WEAPON_HEIGHT = 110.0f * scale;
                const float WEAPON_PADDING = 10.0f * scale;
                
                // Define weapon categories (same as in renderShopUI)
                struct WeaponCategory {
                    std::string name;
                    std::vector<Weapon::Type> weapons;
                };
                
                std::vector<WeaponCategory> categories = {
                    {"Pistols", {Weapon::USP, Weapon::GLOCK, Weapon::FIVESEVEN, Weapon::R8}},
                    {"Rifles", {Weapon::GALIL, Weapon::M4, Weapon::AK47}},
                    {"Snipers", {Weapon::M10, Weapon::AWP, Weapon::M40}}
                };
                
                // Check each weapon panel for click
                for (size_t col = 0; col < categories.size(); col++) {
                    float columnX = scaledX + 20.0f * scale + col * (COLUMN_WIDTH + COLUMN_PADDING);
                    float weaponY = COLUMN_Y + 50.0f * scale;
                    
                    for (Weapon::Type weaponType : categories[col].weapons) {
                        // Check if click is within weapon panel bounds
                        sf::FloatRect weaponBounds(
                            columnX + 10.0f * scale,
                            weaponY,
                            COLUMN_WIDTH - 20.0f * scale,
                            WEAPON_HEIGHT
                        );
                        
                        if (weaponBounds.contains(static_cast<float>(mousePixelPos.x), static_cast<float>(mousePixelPos.y))) {
                            // Weapon clicked! Attempt purchase
                            Weapon* weapon = Weapon::create(weaponType);
                            PurchaseStatus status = calculatePurchaseStatus(serverPlayer, weapon);
                            
                            if (status == PurchaseStatus::Purchasable) {
                                // Process purchase directly on server
                                bool success = processPurchase(serverPlayer, weaponType);
                                
                                if (success) {
                                    ErrorHandler::logInfo("Server player purchased " + weapon->name);
                                    
                                    // Create purchase notification text
                                    {
                                        std::lock_guard<std::mutex> lock(purchaseTextsMutex);
                                        PurchaseText purchaseText;
                                        purchaseText.x = columnX + COLUMN_WIDTH / 2.0f;
                                        purchaseText.y = weaponY + WEAPON_HEIGHT / 2.0f;
                                        purchaseText.weaponName = weapon->name;
                                        purchaseTexts.push_back(purchaseText);
                                    }
                                }
                            } else if (status == PurchaseStatus::InsufficientFunds) {
                                ErrorHandler::logInfo("Cannot purchase " + weapon->name + ": Insufficient funds (need $" + std::to_string(weapon->price) + ")");
                            } else if (status == PurchaseStatus::InventoryFull) {
                                ErrorHandler::logInfo("Cannot purchase " + weapon->name + ": Inventory full");
                            }
                            
                            delete weapon;
                            break;
                        }
                        
                        weaponY += WEAPON_HEIGHT + WEAPON_PADDING;
                    }
                }
            }
            
            // Handle mouse button press for shooting
            // Requirement 6.1: Handle left mouse button click to fire weapon
            if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                Weapon* activeWeapon = serverPlayer.getActiveWeapon();
                if (activeWeapon == nullptr) {
                    ErrorHandler::logInfo("Cannot fire: No active weapon. Active slot: " + std::to_string(serverPlayer.activeSlot));
                }
                if (activeWeapon != nullptr && !shopUIOpen && !inventoryOpen) {
                    // Fire weapon (works for both automatic and semi-automatic)
                    fireWeaponServer(serverPlayer, window, udpSocket, activeBullets, bulletsMutex);
                    
                    // Requirement 6.2: Trigger automatic reload when magazine empty
                    if (activeWeapon->currentAmmo == 0 && activeWeapon->reserveAmmo > 0) {
                        activeWeapon->startReload();
                        ErrorHandler::logInfo("Automatic reload triggered for " + activeWeapon->name);
                    }
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
            // Get window size for UI rendering later
            sf::Vector2u windowSize = window.getSize();
            
            // Calculate delta time for frame-independent movement
            float deltaTime = deltaClock.restart().asSeconds();
            
            // Update weapon reload state
            // Requirement 6.4, 6.5: Update reload progress
            Weapon* activeWeapon = serverPlayer.getActiveWeapon();
            if (activeWeapon != nullptr) {
                activeWeapon->updateReload(deltaTime);
            }
            
            // Handle automatic fire when LMB is held down
            // Only for automatic weapons (Galil, M4, AK47)
            if (window.hasFocus() && !shopUIOpen && !inventoryOpen) {
                Weapon* activeWeapon = serverPlayer.getActiveWeapon();
                if (activeWeapon != nullptr && activeWeapon->isAutomatic()) {
                    // Check if left mouse button is held down
                    if (sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
                        // Check if enough time has passed since last shot (fire rate control)
                        if (activeWeapon->canFireAutomatic()) {
                            fireWeaponServer(serverPlayer, window, udpSocket, activeBullets, bulletsMutex);
                            
                            // Trigger automatic reload when magazine empty
                            if (activeWeapon->currentAmmo == 0 && activeWeapon->reserveAmmo > 0) {
                                activeWeapon->startReload();
                                ErrorHandler::logInfo("Automatic reload triggered for " + activeWeapon->name);
                            }
                        }
                    }
                }
            }
            
            // Requirement 7.2: Update bullet positions
            // Requirement 7.3, 7.4: Check bullet collisions
            // Requirement 7.5, 10.1, 10.2, 10.3: Remove bullets based on conditions
            {
                std::lock_guard<std::mutex> lock(bulletsMutex);
                
                // Update all bullets
                for (auto& bullet : activeBullets) {
                    bullet.update(deltaTime);
                }
                
                // Requirement 7.3: Check bullet-wall collisions with cell-based grid
                // Bullets pass through wooden walls but stop at concrete walls
                for (auto& bullet : activeBullets) {
                    WallType hitWallType = bullet.checkCellWallCollision(grid, bullet.prevX, bullet.prevY);
                    
                    if (hitWallType == WallType::Concrete) {
                        // Concrete walls stop bullets completely
                        bullet.range = 0.0f;
                    }
                    else if (hitWallType == WallType::Wood) {
                        // Wooden walls reduce bullet speed by 50%
                        bullet.vx *= 0.5f;
                        bullet.vy *= 0.5f;
                        
                        // Also reduce remaining range proportionally
                        bullet.range *= 0.5f;
                    }
                }
                
                // Requirement 7.4: Check bullet-player collisions
                const float PLAYER_RADIUS = 15.0f; // PLAYER_SIZE / 2 (30 / 2 = 15)
                
                // Debug: Log bullet count
                static sf::Clock bulletLogClock;
                if (bulletLogClock.getElapsedTime().asSeconds() > 2.0f && !activeBullets.empty()) {
                    ErrorHandler::logInfo("Active bullets: " + std::to_string(activeBullets.size()));
                    bulletLogClock.restart();
                }
                
                // Check collision with server player
                for (auto& bullet : activeBullets) {
                    // Don't check collision with own bullets (server is ID=1) and skip if already dead
                    if (bullet.ownerId != 1 && serverIsAlive) {
                        // Debug: Check distance to player
                        float dx = bullet.x - serverPos.x;
                        float dy = bullet.y - serverPos.y;
                        float distance = std::sqrt(dx * dx + dy * dy);
                        if (distance < 50.0f) {  // Close to player
                            ErrorHandler::logInfo("Bullet near server player! Distance: " + std::to_string(distance) + 
                                                 ", Owner: " + std::to_string(bullet.ownerId));
                        }
                        
                        if (bullet.checkPlayerCollision(serverPos.x, serverPos.y, PLAYER_RADIUS)) {
                            // Requirement 8.1: Apply damage to server player
                            float oldHealth = serverHealth;
                            ErrorHandler::logInfo("BEFORE damage: serverHealth = " + std::to_string(serverHealth));
                            serverHealth -= bullet.damage;
                            ErrorHandler::logInfo("AFTER subtraction: serverHealth = " + std::to_string(serverHealth));
                            if (serverHealth < 0.0f) serverHealth = 0.0f;
                            ErrorHandler::logInfo("AFTER clamp: serverHealth = " + std::to_string(serverHealth));
                            
                            // Mark bullet for removal
                            bullet.range = 0.0f;
                            
                            ErrorHandler::logInfo("Server player hit! Damage: " + std::to_string(bullet.damage) + 
                                                 ", Health: " + std::to_string(oldHealth) + " -> " + std::to_string(serverHealth));
                            
                            // Requirement 8.2: Create damage text visualization
                            {
                                std::lock_guard<std::mutex> lock(damageTextsMutex);
                                DamageText damageText;
                                damageText.x = serverPos.x;
                                damageText.y = serverPos.y - 30.0f; // Start above player
                                damageText.damage = bullet.damage;
                                damageTexts.push_back(damageText);
                                ErrorHandler::logInfo("Damage text created at (" + std::to_string(damageText.x) + 
                                                     ", " + std::to_string(damageText.y) + ")");
                            }
                            
                            // NEW DEATH SYSTEM: Check for player death
                            bool wasKill = false;
                            if (serverHealth <= 0.0f) {
                                ErrorHandler::logInfo("!!! SERVER PLAYER DEATH TRIGGERED !!! Health: " + std::to_string(serverHealth));
                                serverIsAlive = false;
                                serverWaitingRespawn = true;
                                serverRespawnTimer.restart(); // Start 5 second respawn timer
                                wasKill = true;
                                
                                // Requirement 8.4: Award $5000 to eliminating player
                                uint8_t killerId = bullet.ownerId;
                                
                                // Award money to killer
                                if (killerId == 0) {
                                    // Client killed server - client will award itself when it sees serverHealth <= 0
                                    ErrorHandler::logInfo("!!! SERVER PLAYER DIED !!! Killed by client (player 0)");
                                } else if (gameState.hasPlayer(killerId)) {
                                    // Other player killed server
                                    gameState.awardMoney(killerId, 5000);
                                    ErrorHandler::logInfo("!!! SERVER PLAYER DIED !!! Player " + std::to_string(killerId) + " gets $5000 reward");
                                }
                                
                                ErrorHandler::logInfo("Server player eliminated by player " + 
                                                     std::to_string(killerId) + "! Respawn in 5 seconds...");
                                
                                // TODO: Requirement 9.5: Broadcast death event to all clients
                            }
                            
                            // Requirement 10.4: Send hit packet to all clients
                            HitPacket hitPacket;
                            hitPacket.shooterId = bullet.ownerId;
                            hitPacket.victimId = 1; // Server is player 1
                            hitPacket.damage = bullet.damage;
                            hitPacket.hitX = serverPos.x;
                            hitPacket.hitY = serverPos.y;
                            hitPacket.wasKill = wasKill;
                            
                            // Broadcast to all connected clients
                            for (const auto& client : connectedClients) {
                                if (client.socket && client.isReady) {
                                    udpSocket.send(&hitPacket, sizeof(HitPacket), client.address, 53002);
                                }
                            }
                            
                            ErrorHandler::logInfo("Hit packet sent to all clients");
                            
                            // TODO: Requirement 10.4: Send hit packet to all clients
                        }
                    }
                }
                
                // Requirement 7.4: Check collision with all other players from GameState
                std::vector<Player> allPlayers = gameState.getPlayersInRadius(
                    sf::Vector2f(serverPos.x, serverPos.y), 10000.0f); // Large radius to get all players
                
                for (auto& bullet : activeBullets) {
                    if (bullet.range <= 0.0f) continue; // Skip already hit bullets
                    
                    // Check collision with each player
                    for (const auto& player : allPlayers) {
                        // Don't check collision with bullet owner or dead players
                        if (bullet.ownerId == player.id || !player.isAlive) continue;
                        
                        if (bullet.checkPlayerCollision(player.x, player.y, PLAYER_RADIUS)) {
                            // Requirement 8.1: Apply damage to player
                            gameState.applyDamage(player.id, bullet.damage);
                            
                            // Mark bullet for removal
                            bullet.range = 0.0f;
                            
                            ErrorHandler::logInfo("Player " + std::to_string(player.id) + 
                                                " hit! Damage: " + std::to_string(bullet.damage));
                            
                            // Requirement 8.2: Create damage text visualization
                            {
                                std::lock_guard<std::mutex> lock(damageTextsMutex);
                                DamageText damageText;
                                damageText.x = player.x;
                                damageText.y = player.y - 30.0f; // Start above player
                                damageText.damage = bullet.damage;
                                damageTexts.push_back(damageText);
                            }
                            
                            // Requirement 8.3: Check for player death
                            bool wasKill = false;
                            if (gameState.isPlayerDead(player.id)) {
                                gameState.setPlayerAlive(player.id, false);
                                wasKill = true;
                                
                                // Requirement 8.4: Award $5000 to eliminating player
                                if (gameState.hasPlayer(bullet.ownerId)) {
                                    gameState.awardMoney(bullet.ownerId, 5000);
                                }
                                
                                ErrorHandler::logInfo("Player " + std::to_string(player.id) + 
                                                     " eliminated by player " + std::to_string(bullet.ownerId) + "!");
                                
                                // TODO: Requirement 8.4: Schedule respawn after 5 seconds
                                // TODO: Requirement 9.5: Broadcast death event to all clients
                            }
                            
                            // Requirement 10.4: Send hit packet to all clients
                            HitPacket hitPacket;
                            hitPacket.shooterId = bullet.ownerId;
                            hitPacket.victimId = player.id;
                            hitPacket.damage = bullet.damage;
                            hitPacket.hitX = player.x;
                            hitPacket.hitY = player.y;
                            hitPacket.wasKill = wasKill;
                            
                            // Broadcast to all connected clients
                            for (const auto& client : connectedClients) {
                                if (client.socket && client.isReady) {
                                    udpSocket.send(&hitPacket, sizeof(HitPacket), client.address, 53002);
                                }
                            }
                            
                            ErrorHandler::logInfo("Hit packet sent to all clients");
                            
                            break; // Bullet can only hit one player
                        }
                    }
                }
                
                // Also check collision with simple client position (for basic 2-player mode)
                for (auto& bullet : activeBullets) {
                    if (bullet.range <= 0.0f) continue; // Skip already hit bullets
                    
                    // Don't check collision with own bullets (server bullets hit client)
                    if (bullet.ownerId == 1 && clientIsAlive) {
                        if (bullet.checkPlayerCollision(clientPos.x, clientPos.y, PLAYER_RADIUS)) {
                            // Mark bullet for removal
                            bullet.range = 0.0f;
                            
                            // Apply damage to client
                            float oldHealth = clientHealth;
                            clientHealth -= bullet.damage;
                            if (clientHealth < 0.0f) clientHealth = 0.0f;
                            
                            ErrorHandler::logInfo("Client player hit! Damage: " + std::to_string(bullet.damage) + 
                                                 ", Health: " + std::to_string(oldHealth) + " -> " + std::to_string(clientHealth));
                            
                            // Requirement 8.2: Create damage text visualization on server
                            {
                                std::lock_guard<std::mutex> lock(damageTextsMutex);
                                DamageText damageText;
                                damageText.x = clientPos.x;
                                damageText.y = clientPos.y - 30.0f; // Start above player
                                damageText.damage = bullet.damage;
                                damageTexts.push_back(damageText);
                            }
                            
                            // NEW DEATH SYSTEM: Check if client died
                            bool wasKill = false;
                            if (clientHealth <= 0.0f && clientIsAlive) {
                                clientIsAlive = false;
                                clientWaitingRespawn = true;
                                clientRespawnTimer.restart(); // Start 5 second respawn timer
                                wasKill = true;
                                
                                // Server gets kill reward
                                serverPlayer.money += 5000;
                                serverScore += 1;
                                
                                ErrorHandler::logInfo("!!! CLIENT PLAYER DIED !!! Server gets $5000 reward and +1 score. Server money: $" + std::to_string(serverPlayer.money) + ", Score: " + std::to_string(serverScore));
                            }
                            
                            // Requirement 10.4: Send hit packet to client
                            HitPacket hitPacket;
                            hitPacket.shooterId = bullet.ownerId;
                            hitPacket.victimId = 0; // Client is player 0
                            hitPacket.damage = bullet.damage;
                            hitPacket.hitX = clientPos.x;
                            hitPacket.hitY = clientPos.y;
                            hitPacket.wasKill = wasKill;
                            
                            // Send to client
                            for (const auto& client : connectedClients) {
                                if (client.socket && client.isReady) {
                                    udpSocket.send(&hitPacket, sizeof(HitPacket), client.address, 53002);
                                }
                            }
                            
                            ErrorHandler::logInfo("Hit packet sent to client");
                        }
                    }
                }
                
                // Get screen bounds with 20% buffer for culling
                sf::Vector2f viewCenter = window.getView().getCenter();
                sf::Vector2f viewSize = window.getView().getSize();
                float bufferMultiplier = 1.2f;
                float screenLeft = viewCenter.x - (viewSize.x * bufferMultiplier) / 2.0f;
                float screenRight = viewCenter.x + (viewSize.x * bufferMultiplier) / 2.0f;
                float screenTop = viewCenter.y - (viewSize.y * bufferMultiplier) / 2.0f;
                float screenBottom = viewCenter.y + (viewSize.y * bufferMultiplier) / 2.0f;
                
                // Remove bullets that should be removed
                activeBullets.erase(
                    std::remove_if(activeBullets.begin(), activeBullets.end(),
                        [screenLeft, screenRight, screenTop, screenBottom](const Bullet& b) {
                            // Requirement 7.5: Remove if exceeded range
                            if (b.shouldRemove()) return true;
                            
                            // Requirement 10.2: Remove if outside screen + 20% buffer
                            if (b.x < screenLeft || b.x > screenRight || 
                                b.y < screenTop || b.y > screenBottom) {
                                return true;
                            }
                            
                            return false;
                        }),
                    activeBullets.end()
                );
            }
            
            // Requirement 8.2: Update and remove expired damage texts
            {
                std::lock_guard<std::mutex> lock(damageTextsMutex);
                damageTexts.erase(
                    std::remove_if(damageTexts.begin(), damageTexts.end(),
                        [](const DamageText& dt) {
                            return dt.shouldRemove();
                        }),
                    damageTexts.end()
                );
            }
            
            // Update and remove expired purchase notification texts
            {
                std::lock_guard<std::mutex> lock(purchaseTextsMutex);
                purchaseTexts.erase(
                    std::remove_if(purchaseTexts.begin(), purchaseTexts.end(),
                        [](const PurchaseText& pt) {
                            return pt.shouldRemove();
                        }),
                    purchaseTexts.end()
                );
            }
            
            // NEW DEATH SYSTEM: Handle respawn with 5 second delay
            if (serverWaitingRespawn) {
                // Check if 5 seconds have passed since death
                if (serverRespawnTimer.getElapsedTime().asSeconds() >= 5.0f) {
                    // Respawn after 5 second delay
                    ErrorHandler::logInfo("!!! SERVER PLAYER RESPAWNING !!!");
                    serverHealth = 100.0f;
                    serverIsAlive = true;
                    serverWaitingRespawn = false;
                
                // Generate random respawn position at least 1000 pixels away from client
                std::random_device rd;
                std::mt19937 gen(rd());
                const float MARGIN = CELL_SIZE;
                std::uniform_real_distribution<float> xDist(MARGIN, MAP_SIZE - MARGIN);
                std::uniform_real_distribution<float> yDist(MARGIN, MAP_SIZE - MARGIN);
                
                const int MAX_ATTEMPTS = 100;
                bool foundValidSpawn = false;
                
                for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
                    Position newSpawn;
                    newSpawn.x = xDist(gen);
                    newSpawn.y = yDist(gen);
                    
                    // Check if position is valid (no wall collision)
                    if (checkCollision(sf::Vector2f(newSpawn.x, newSpawn.y), grid)) {
                        continue;
                    }
                    
                    // Check distance from client (must be >= 1000 pixels)
                    float dx = newSpawn.x - clientPos.x;
                    float dy = newSpawn.y - clientPos.y;
                    float distance = std::sqrt(dx * dx + dy * dy);
                    
                    if (distance >= 1000.0f) {
                        serverPos = newSpawn;
                        serverPosPrevious = newSpawn;
                        foundValidSpawn = true;
                        ErrorHandler::logInfo("Server player respawned at (" + std::to_string(serverPos.x) + 
                                            ", " + std::to_string(serverPos.y) + "), distance from client: " + 
                                            std::to_string(distance) + " pixels");
                        break;
                    }
                }
                
                    if (!foundValidSpawn) {
                        // Fallback: use original spawn position
                        serverPos.x = 250.0f;
                        serverPos.y = 4850.0f;
                        serverPosPrevious = serverPos;
                        ErrorHandler::logWarning("Failed to find valid respawn position, using fallback");
                    }
                }
            }
            
            // NEW DEATH SYSTEM: Handle client respawn with 5 second delay
            if (clientWaitingRespawn) {
                // Check if 5 seconds have passed since death
                if (clientRespawnTimer.getElapsedTime().asSeconds() >= 5.0f) {
                    // Respawn after 5 second delay
                    ErrorHandler::logInfo("!!! CLIENT PLAYER RESPAWNING !!!");
                    clientHealth = 100.0f;
                    clientIsAlive = true;
                    clientWaitingRespawn = false;
                
                // Generate random respawn position at least 1000 pixels away from server
                std::random_device rd;
                std::mt19937 gen(rd());
                const float MARGIN = CELL_SIZE;
                std::uniform_real_distribution<float> xDist(MARGIN, MAP_SIZE - MARGIN);
                std::uniform_real_distribution<float> yDist(MARGIN, MAP_SIZE - MARGIN);
                
                const int MAX_ATTEMPTS = 100;
                bool foundValidSpawn = false;
                
                for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
                    Position newSpawn;
                    newSpawn.x = xDist(gen);
                    newSpawn.y = yDist(gen);
                    
                    // Check if position is valid (no wall collision)
                    if (checkCollision(sf::Vector2f(newSpawn.x, newSpawn.y), grid)) {
                        continue;
                    }
                    
                    // Check distance from server (must be >= 1000 pixels)
                    float dx = newSpawn.x - serverPos.x;
                    float dy = newSpawn.y - serverPos.y;
                    float distance = std::sqrt(dx * dx + dy * dy);
                    
                    if (distance >= 1000.0f) {
                        clientPos = newSpawn;
                        clientPosPrevious = newSpawn;
                        clientPosTarget = newSpawn;
                        foundValidSpawn = true;
                        ErrorHandler::logInfo("Client player respawned at (" + std::to_string(clientPos.x) + 
                                            ", " + std::to_string(clientPos.y) + "), distance from server: " + 
                                            std::to_string(distance) + " pixels");
                        break;
                    }
                }
                
                    if (!foundValidSpawn) {
                        // Fallback: use original spawn position
                        clientPos.x = 4850.0f;
                        clientPos.y = 250.0f;
                        clientPosPrevious = clientPos;
                        clientPosTarget = clientPos;
                        ErrorHandler::logWarning("Failed to find valid respawn position, using fallback");
                    }
                }
            }
            
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
                // Requirements: 5.3, 5.4 - Use weapon-modified movement speed
                float currentSpeed = serverPlayer.getMovementSpeed();
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) newPos.y -= currentSpeed * deltaTime * 60.0f;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) newPos.y += currentSpeed * deltaTime * 60.0f;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) newPos.x -= currentSpeed * deltaTime * 60.0f;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) newPos.x += currentSpeed * deltaTime * 60.0f;
                
                // NEW: Apply cell-based collision detection
                newPos = resolveCollisionCellBased(oldPos, newPos, grid);
                
                serverPos.x = newPos.x;
                serverPos.y = newPos.y;
                // Update player position for weapon firing
                serverPlayer.x = newPos.x;
                serverPlayer.y = newPos.y;
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

                    if (clientSprites.find(ip) == clientSprites.end()) {
                        clientSprites[ip] = sf::Sprite();
                        clientSprites[ip].setTexture(playerTexture);
                        clientSprites[ip].setOrigin(PLAYER_SIZE / 2.0f, PLAYER_SIZE / 2.0f);
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
                        // Apply fog to client player sprite
                        clientSprites[ip].setColor(sf::Color(255, 255, 255, alpha));
                        
                        // Position client sprite (centered on position)
                        clientSprites[ip].setPosition(renderClientPos.x, renderClientPos.y);
                        
                        // Apply client player rotation (subtract 90 degrees because sprite initially faces up)
                        clientSprites[ip].setRotation(clientPlayer.rotation - 90.0f);
                        
                        // Draw client player
                        window.draw(clientSprites[ip]);
                    }
                }
            }
            
            // Requirement 7.1: Render bullets as sprites with texture
            {
                std::lock_guard<std::mutex> lock(bulletsMutex);
                
                // Create bullet sprite (reused for all bullets)
                sf::Sprite bulletSprite;
                bulletSprite.setTexture(bulletTexture);
                
                // Get texture size and set origin to center
                sf::Vector2u bulletTexSize = bulletTexture.getSize();
                bulletSprite.setOrigin(bulletTexSize.x / 2.0f, bulletTexSize.y / 2.0f);
                
                for (const auto& bullet : activeBullets) {
                    // Calculate distance for fog
                    float dx = bullet.x - renderPos.x;
                    float dy = bullet.y - renderPos.y;
                    float distance = std::sqrt(dx * dx + dy * dy);
                    
                    // Calculate fog alpha
                    sf::Uint8 alpha = calculateFogAlpha(distance);
                    
                    // Only draw if visible
                    if (alpha > 0) {
                        // Calculate bullet direction and rotation
                        float speed = std::sqrt(bullet.vx * bullet.vx + bullet.vy * bullet.vy);
                        float dirX = (speed > 0.001f) ? bullet.vx / speed : 1.0f;
                        float dirY = (speed > 0.001f) ? bullet.vy / speed : 0.0f;
                        
                        // Calculate rotation angle (bullet points in direction of travel)
                        float angle = std::atan2(dirY, dirX) * 180.0f / 3.14159f;
                        
                        // Apply fog, position and rotation to bullet sprite
                        bulletSprite.setColor(sf::Color(255, 255, 255, alpha));
                        bulletSprite.setPosition(bullet.x, bullet.y);
                        bulletSprite.setRotation(angle);
                        
                        window.draw(bulletSprite);
                    }
                }
            }
            
            // Requirement 8.2: Render damage texts
            {
                std::lock_guard<std::mutex> lock(damageTextsMutex);
                
                for (const auto& damageText : damageTexts) {
                    // Calculate distance for fog
                    float dx = damageText.x - renderPos.x;
                    float dy = damageText.y - renderPos.y;
                    float distance = std::sqrt(dx * dx + dy * dy);
                    
                    // Calculate fog alpha
                    sf::Uint8 fogAlpha = calculateFogAlpha(distance);
                    
                    // Only draw if visible
                    if (fogAlpha > 0) {
                        // Get animated position and alpha
                        float animatedY = damageText.getAnimatedY();
                        sf::Uint8 textAlpha = damageText.getAlpha();
                        
                        // Combine fog and fade-out alpha
                        sf::Uint8 finalAlpha = static_cast<sf::Uint8>(
                            (fogAlpha / 255.0f) * (textAlpha / 255.0f) * 255.0f
                        );
                        
                        // Create damage text
                        sf::Text text;
                        text.setFont(font);
                        text.setString("-" + std::to_string(static_cast<int>(damageText.damage)));
                        text.setCharacterSize(24);
                        text.setFillColor(sf::Color(255, 0, 0, finalAlpha)); // Red with alpha
                        text.setStyle(sf::Text::Bold);
                        
                        // Center text above player
                        sf::FloatRect textBounds = text.getLocalBounds();
                        text.setOrigin(textBounds.width / 2.0f, textBounds.height / 2.0f);
                        text.setPosition(damageText.x, animatedY);
                        
                        window.draw(text);
                    }
                }
            }
            
            // Draw server player sprite - always visible
            // Calculate rotation angle to mouse cursor
            sf::Vector2i mousePixelPos = sf::Mouse::getPosition(window);
            sf::Vector2f mouseWorldPos = window.mapPixelToCoords(mousePixelPos);
            
            // Calculate angle from player to mouse (in degrees)
            float dx = mouseWorldPos.x - renderPos.x;
            float dy = mouseWorldPos.y - renderPos.y;
            float angleToMouse = std::atan2(dy, dx) * 180.0f / 3.14159f;
            
            // Update server player rotation for synchronization
            {
                std::lock_guard<std::mutex> lock(mutex);
                serverPlayer.rotation = angleToMouse;
            }
            
            // Rotate sprite to face mouse (subtract 90 degrees because sprite initially faces up)
            serverSprite.setRotation(angleToMouse - 90.0f);
            serverSprite.setPosition(renderPos.x, renderPos.y);
            window.draw(serverSprite);
            
            // Render fog overlay on top of everything (creates vignette effect)
            renderFogOverlay(window, renderPos);
            
            // Render shops AFTER fog overlay so they are visible
            // Requirements: 2.6, 3.1, 10.5
            renderShops(window, sf::Vector2f(serverPos.x, serverPos.y), shops);
            
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
            
            // Debug: Log health periodically
            static sf::Clock healthLogClock;
            static float lastLoggedHealth = serverHealth;
            static float lastLoggedClientHealth = clientHealth;
            if (healthLogClock.getElapsedTime().asSeconds() > 3.0f || 
                std::abs(serverHealth - lastLoggedHealth) > 0.1f ||
                std::abs(clientHealth - lastLoggedClientHealth) > 0.1f) {
                ErrorHandler::logInfo("Current server health: " + std::to_string(serverHealth) + 
                                     ", Client health: " + std::to_string(clientHealth));
                lastLoggedHealth = serverHealth;
                lastLoggedClientHealth = clientHealth;
                healthLogClock.restart();
            }
            
            // Draw money balance below health
            // Requirement 1.5: Display money balance in HUD
            sf::Text moneyText;
            moneyText.setFont(font);
            moneyText.setString("Money: $" + std::to_string(serverPlayer.money));
            moneyText.setCharacterSize(28);
            moneyText.setFillColor(sf::Color(255, 215, 0));  // Gold color
            moneyText.setPosition(20.0f, 100.0f);
            window.draw(moneyText);
            
            // Draw weapon info in top-right corner
            // Requirements: 5.5, 5.6
            sf::Text weaponText;
            weaponText.setFont(font);
            weaponText.setCharacterSize(28);
            
            Weapon* currentWeapon = serverPlayer.getActiveWeapon();
            if (currentWeapon != nullptr) {
                // Requirement 5.5: Display weapon name and ammo count
                std::string weaponInfo = currentWeapon->name + ": " + 
                                        std::to_string(currentWeapon->currentAmmo) + "/" + 
                                        std::to_string(currentWeapon->reserveAmmo);
                weaponText.setString(weaponInfo);
                weaponText.setFillColor(sf::Color::White);
            } else {
                // Requirement 5.6: Display "No weapon" when no weapon active
                weaponText.setString("No weapon");
                weaponText.setFillColor(sf::Color(150, 150, 150));  // Gray color
            }
            
            // Position in top-right corner
            sf::FloatRect weaponBounds = weaponText.getLocalBounds();
            weaponText.setPosition(
                windowSize.x - weaponBounds.width - 20.0f - weaponBounds.left,
                20.0f
            );
            window.draw(weaponText);
            
            // Draw "Reloading..." text if weapon is reloading
            if (currentWeapon != nullptr && currentWeapon->isReloading) {
                sf::Text reloadingText;
                reloadingText.setFont(font);
                reloadingText.setString("Reloading...");
                reloadingText.setCharacterSize(24);
                reloadingText.setFillColor(sf::Color::Yellow);
                
                // Position below weapon info
                sf::FloatRect reloadingBounds = reloadingText.getLocalBounds();
                reloadingText.setPosition(
                    windowSize.x - reloadingBounds.width - 20.0f - reloadingBounds.left,
                    60.0f  // Below weapon text (20 + 28 + 12 spacing)
                );
                window.draw(reloadingText);
            }
            
            // Update shop UI animation
            const float SHOP_ANIMATION_DURATION = 0.3f; // 300ms animation
            if (shopUIOpen && shopAnimationProgress < 1.0f) {
                // Opening animation
                shopAnimationProgress = std::min(1.0f, shopAnimationClock.getElapsedTime().asSeconds() / SHOP_ANIMATION_DURATION);
            } else if (!shopUIOpen && shopAnimationProgress > 0.0f) {
                // Closing animation
                shopAnimationProgress = std::max(0.0f, 1.0f - (shopAnimationClock.getElapsedTime().asSeconds() / SHOP_ANIMATION_DURATION));
            }
            
            // Update inventory animation
            const float ANIMATION_DURATION = 0.3f; // 300ms animation
            if (inventoryOpen && inventoryAnimationProgress < 1.0f) {
                // Opening animation
                inventoryAnimationProgress = std::min(1.0f, inventoryAnimationClock.getElapsedTime().asSeconds() / ANIMATION_DURATION);
            } else if (!inventoryOpen && inventoryAnimationProgress > 0.0f) {
                // Closing animation
                inventoryAnimationProgress = std::max(0.0f, 1.0f - (inventoryAnimationClock.getElapsedTime().asSeconds() / ANIMATION_DURATION));
            }
            
            // Draw shop UI with animation
            // Requirements: 3.2, 3.3, 3.4, 3.5
            if (shopAnimationProgress > 0.0f) {
                // Use actual server player with real balance and inventory
                renderShopUI(window, serverPlayer, font, shopAnimationProgress);
                
                // Render purchase notification texts on top of shop UI
                {
                    std::lock_guard<std::mutex> lock(purchaseTextsMutex);
                    
                    for (const auto& purchaseText : purchaseTexts) {
                        // Get animated position and alpha
                        float animatedY = purchaseText.getAnimatedY();
                        sf::Uint8 textAlpha = purchaseText.getAlpha();
                        
                        // Create "Purchased" text
                        sf::Text text;
                        text.setFont(font);
                        text.setString("Purchased");
                        text.setCharacterSize(28);
                        text.setFillColor(sf::Color(0, 255, 0, textAlpha)); // Green with alpha
                        text.setStyle(sf::Text::Bold);
                        
                        // Center text at purchase location
                        sf::FloatRect textBounds = text.getLocalBounds();
                        text.setOrigin(textBounds.width / 2.0f, textBounds.height / 2.0f);
                        text.setPosition(purchaseText.x, animatedY);
                        
                        window.draw(text);
                    }
                }
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
                    
                    // Check if this slot has a weapon
                    Weapon* slotWeapon = nullptr;
                    if (i < 4) { // Only first 4 slots can have weapons
                        slotWeapon = serverPlayer.inventory[i];
                    }
                    
                    // Highlight active weapon slot
                    bool isActiveSlot = (i == serverPlayer.activeSlot);
                    
                    // Create slot rectangle
                    sf::RectangleShape slot(sf::Vector2f(scaledSize, scaledSize));
                    slot.setPosition(slotX + scaleOffset, inventoryY + scaleOffset);
                    
                    // Different background color for active slot
                    if (isActiveSlot && slotWeapon != nullptr) {
                        slot.setFillColor(sf::Color(70, 70, 30, slotAlpha)); // Yellowish tint for active slot
                        slot.setOutlineColor(sf::Color(255, 215, 0, static_cast<sf::Uint8>(slotEasedProgress * 255))); // Gold border
                    } else {
                        slot.setFillColor(sf::Color(50, 50, 50, slotAlpha)); // Dark semi-transparent
                        slot.setOutlineColor(sf::Color(150, 150, 150, static_cast<sf::Uint8>(slotEasedProgress * 255))); // Light gray border
                    }
                    slot.setOutlineThickness(2.0f);
                    
                    window.draw(slot);
                    
                    // Draw slot content
                    if (slotEasedProgress > 0.3f) { // Show text after slot is 30% visible
                        if (slotWeapon != nullptr) {
                            // Draw weapon name
                            sf::Text weaponName;
                            weaponName.setFont(font);
                            weaponName.setString(slotWeapon->name);
                            weaponName.setCharacterSize(static_cast<unsigned int>(16 * scale)); // Smaller text for weapon name
                            weaponName.setFillColor(sf::Color(255, 255, 255, static_cast<sf::Uint8>(slotEasedProgress * 255)));
                            
                            // Center the weapon name in the slot
                            sf::FloatRect nameBounds = weaponName.getLocalBounds();
                            weaponName.setPosition(
                                slotX + scaleOffset + (scaledSize - nameBounds.width) / 2.0f - nameBounds.left,
                                inventoryY + scaleOffset + (scaledSize - nameBounds.height) / 2.0f - nameBounds.top - 15.0f * scale
                            );
                            window.draw(weaponName);
                            
                            // Draw ammo count below weapon name
                            sf::Text ammoText;
                            ammoText.setFont(font);
                            ammoText.setString(std::to_string(slotWeapon->currentAmmo) + "/" + std::to_string(slotWeapon->reserveAmmo));
                            ammoText.setCharacterSize(static_cast<unsigned int>(14 * scale));
                            ammoText.setFillColor(sf::Color(200, 200, 200, static_cast<sf::Uint8>(slotEasedProgress * 255)));
                            
                            // Center the ammo text below weapon name
                            sf::FloatRect ammoBounds = ammoText.getLocalBounds();
                            ammoText.setPosition(
                                slotX + scaleOffset + (scaledSize - ammoBounds.width) / 2.0f - ammoBounds.left,
                                inventoryY + scaleOffset + (scaledSize - ammoBounds.height) / 2.0f - ammoBounds.top + 15.0f * scale
                            );
                            window.draw(ammoText);
                            
                            // Draw slot number in top-left corner
                            sf::Text slotNumber;
                            slotNumber.setFont(font);
                            slotNumber.setString(std::to_string(i + 1));
                            slotNumber.setCharacterSize(static_cast<unsigned int>(18 * scale));
                            slotNumber.setFillColor(sf::Color(150, 150, 150, static_cast<sf::Uint8>(slotEasedProgress * 200)));
                            slotNumber.setPosition(
                                slotX + scaleOffset + 5.0f * scale,
                                inventoryY + scaleOffset + 5.0f * scale
                            );
                            window.draw(slotNumber);
                        } else {
                            // Empty slot - draw slot number in center
                            sf::Text slotNumber;
                            slotNumber.setFont(font);
                            slotNumber.setString(std::to_string(i + 1));
                            slotNumber.setCharacterSize(static_cast<unsigned int>(32 * scale)); // Scale text with slot
                            slotNumber.setFillColor(sf::Color(100, 100, 100, static_cast<sf::Uint8>(slotEasedProgress * 200))); // Dimmed for empty slots
                            
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
            }
            
            // Render shop interaction prompt if player is near a shop
            // Requirement 3.1: Display prompt when player within 60 pixels
            renderShopInteractionPrompt(window, sf::Vector2f(serverPos.x, serverPos.y), shops, font, shopUIOpen);
            
            // Draw inventory hint at bottom center
            sf::Text inventoryHint;
            inventoryHint.setFont(font);
            inventoryHint.setString("E - inventory");
            inventoryHint.setCharacterSize(24);
            inventoryHint.setFillColor(sf::Color(200, 200, 200, 180)); // Light gray, semi-transparent
            
            // Center the hint at the bottom of the screen
            sf::FloatRect hintBounds = inventoryHint.getLocalBounds();
            inventoryHint.setPosition(
                windowSize.x / 2.0f - hintBounds.width / 2.0f - hintBounds.left,
                windowSize.y - 40.0f // 40px from bottom
            );
            window.draw(inventoryHint);
            
            // NEW DEATH SYSTEM: Black screen with "You dead" text
            if (!serverIsAlive) {
                // Full black screen
                sf::RectangleShape deathOverlay(sf::Vector2f(static_cast<float>(windowSize.x), static_cast<float>(windowSize.y)));
                deathOverlay.setFillColor(sf::Color(0, 0, 0, 255)); // Completely black
                window.draw(deathOverlay);
                
                // Red "You dead" text
                sf::Text deathText;
                deathText.setFont(font);
                deathText.setString("You dead");
                deathText.setCharacterSize(80);
                deathText.setFillColor(sf::Color::Red);
                deathText.setStyle(sf::Text::Bold);
                
                // Center the text
                sf::FloatRect textBounds = deathText.getLocalBounds();
                deathText.setPosition(
                    static_cast<float>(windowSize.x) / 2.0f - textBounds.width / 2.0f - textBounds.left,
                    static_cast<float>(windowSize.y) / 2.0f - textBounds.height / 2.0f - textBounds.top
                );
                window.draw(deathText);
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