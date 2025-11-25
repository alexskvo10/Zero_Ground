#include <SFML/Network.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <thread>
#include <mutex>
#include <sstream>
#include <memory>
#include <vector>
#include <array>
#include <cstring>
#include <queue>

// Global icon image (needs to persist for window lifetime)
sf::Image g_windowIcon;

// ========================
// Constants for the new cell-based map system
// ========================
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

// ========================
// Wall types
enum class WallType : uint8_t {
    None = 0,      // No wall
    Concrete = 1,  // Concrete wall (gray)
    Wood = 2       // Wooden wall (brown)
};

// Cell structure for grid-based map
// ========================
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

// Forward declaration for Weapon
struct Weapon;

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
                break;
        }
        
        // Initialize with full magazine
        w->currentAmmo = w->magazineSize;
        
        return w;
    }
    
    bool canFire() const {
        return !isReloading && currentAmmo > 0;
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
    float health = 100.0f;
    bool isAlive = true;
    uint32_t frameID = 0;
    uint8_t playerId = 0;
};

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

// ========================
// NEW: Optimized Fog of War Background Rendering
// ========================

// Render background with smooth fog of war gradient effect (optimized)
// The background gets darker the further it is from the player
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
// NEW: Optimized Visible Wall Rendering
// ========================

// Render only visible walls within the camera view
// This function implements:
// 1. Camera-based visibility culling (render walls visible in current view)
// 2. Caching of visible bounds (recalculate only when camera moves significantly)
// 3. Debug output for rendered wall count
//
// PERFORMANCE OPTIMIZATION:
// - Static variables cache the last camera center and visible bounds
// - Bounds are recalculated only when camera moves significantly
// - This reduces computation from every frame to only when needed
//
// WALL POSITIONING:
// - Walls are centered on cell boundaries
// - topWall: centered on top edge (y - WALL_WIDTH/2)
// - rightWall: centered on right edge (x + CELL_SIZE - WALL_WIDTH/2)
// - bottomWall: centered on bottom edge (y + CELL_SIZE - WALL_WIDTH/2)
// - leftWall: centered on left edge (x - WALL_WIDTH/2)
void renderVisibleWalls(sf::RenderWindow& window, sf::Vector2f playerPosition, 
                       const std::vector<std::vector<Cell>>& grid) {
    // Get current view to determine visible area
    sf::View currentView = window.getView();
    sf::Vector2f viewCenter = currentView.getCenter();
    sf::Vector2f viewSize = currentView.getSize();
    
    // Calculate visible world bounds with some padding
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
            // Calculate cell position in world coordinates
            float x = i * CELL_SIZE;
            float y = j * CELL_SIZE;
            
            // Render topWall (horizontal wall on top edge of cell)
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
            
            // Render rightWall (vertical wall on right edge of cell)
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
            
            // Render bottomWall (horizontal wall on bottom edge of cell)
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
            
            // Render leftWall (vertical wall on left edge of cell)
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
    
    // Debug output: log wall count in debug builds only
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
    
    // Step 6: No collision detected, clamp new position to map boundaries and return
    newPos.x = std::max(PLAYER_SIZE / 2.0f, std::min(newPos.x, MAP_SIZE - PLAYER_SIZE / 2.0f));
    newPos.y = std::max(PLAYER_SIZE / 2.0f, std::min(newPos.y, MAP_SIZE - PLAYER_SIZE / 2.0f));
    
    return newPos;
}

std::mutex mutex;
Position clientPos = { 4850.0f, 250.0f }; // Client spawn position (top-right corner of 5100×5100 map)
Position clientPosPrevious = { 4850.0f, 250.0f }; // Previous position for interpolation
Position serverPos = { 250.0f, 4850.0f }; // Server spawn position (bottom-left corner of 5100×5100 map)
Position serverPosPrevious = { 250.0f, 4850.0f }; // Previous server position for interpolation
Position serverPosTarget = { 250.0f, 4850.0f }; // Target server position (latest received)
std::string serverIP = "127.0.0.1";
GameMap clientGameMap; // Store map data received from server
std::unique_ptr<sf::TcpSocket> tcpSocket; // TCP socket for connection
std::string connectionMessage = "";
sf::Color connectionMessageColor = sf::Color::White;
bool udpRunning = true; // Flag to control UDP thread
bool serverConnected = false; // Track server connection status
sf::Clock lastPacketReceived; // Track last received packet for connection loss detection
uint32_t currentFrameID = 0; // Frame counter for position packets

// Grid for cell-based map system (global for easy access from handshake)
std::vector<std::vector<Cell>> grid(GRID_SIZE, std::vector<Cell>(GRID_SIZE));

// Shop system
std::vector<Shop> shops;  // Shops received from server
bool shopUIOpen = false;  // Shop UI state
sf::Clock shopAnimationClock;
float shopAnimationProgress = 0.0f;  // 0.0 = closed, 1.0 = fully open

// Requirement 7.1: Store active bullets
std::vector<Bullet> activeBullets;
std::mutex bulletsMutex;

// Requirement 8.2: Store active damage texts
std::vector<DamageText> damageTexts;
std::mutex damageTextsMutex;

// Client player with inventory and weapons
Player clientPlayer;

// Inventory system
const int INVENTORY_SLOTS = 6;
const float INVENTORY_SLOT_SIZE = 100.0f;
const float INVENTORY_PADDING = 10.0f;
bool inventoryOpen = false;
float inventoryAnimationProgress = 0.0f; // 0.0 = closed, 1.0 = fully open
sf::Clock inventoryAnimationClock;

// HUD variables
float clientHealth = 100.0f; // Client player health (0-100)
float serverHealth = 100.0f; // Server player health (0-100)
bool serverWasAlive = true; // Track if server was alive in previous frame
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

// Receive shop positions from server via TCP
// Parameters:
//   serverSocket - TCP socket connected to the server
//   shops - Vector to populate with received shops
// Returns: true if successful, false if any error occurred
//
// PROTOCOL:
// 1. Receive shop count as uint8_t (1 byte)
// 2. For each shop, receive gridX and gridY as int32_t (8 bytes per shop)
// 3. Calculate world coordinates from grid coordinates
bool receiveShopsFromServer(sf::TcpSocket& serverSocket, std::vector<Shop>& shops) {
    std::cout << "[INFO] Waiting to receive shops from server..." << std::endl;
    
    // Step 1: Receive shop count
    uint8_t shopCount = 0;
    std::size_t received = 0;
    
    sf::Socket::Status countStatus = serverSocket.receive(&shopCount, sizeof(shopCount), received);
    if (countStatus != sf::Socket::Done) {
        ErrorHandler::logTCPError("Receive shop count", countStatus, 
                                 serverSocket.getRemoteAddress().toString());
        return false;
    }
    
    std::cout << "[INFO] Receiving " << static_cast<int>(shopCount) << " shops..." << std::endl;
    
    // Step 2: Receive each shop's data
    shops.clear();
    shops.reserve(shopCount);
    
    for (int i = 0; i < shopCount; ++i) {
        struct ShopData {
            int32_t gridX;
            int32_t gridY;
        } shopData;
        
        sf::Socket::Status shopStatus = serverSocket.receive(&shopData, sizeof(shopData), received);
        if (shopStatus != sf::Socket::Done) {
            ErrorHandler::logTCPError("Receive shop data", shopStatus, 
                                     serverSocket.getRemoteAddress().toString());
            return false;
        }
        
        // Create shop and calculate world coordinates
        Shop shop;
        shop.gridX = shopData.gridX;
        shop.gridY = shopData.gridY;
        shop.worldX = shopData.gridX * CELL_SIZE + CELL_SIZE / 2.0f;
        shop.worldY = shopData.gridY * CELL_SIZE + CELL_SIZE / 2.0f;
        
        shops.push_back(shop);
    }
    
    std::cout << "[INFO] Shops received successfully: " << static_cast<int>(shopCount) << " shops" << std::endl;
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
    
    // Receive shop positions from server
    ErrorHandler::logInfo("Waiting to receive shop positions from server...");
    
    if (!receiveShopsFromServer(*tcpSocket, shops)) {
        connectionMessage = "Failed to receive shop data";
        connectionMessageColor = sf::Color::Red;
        tcpSocket.reset();
        return false;
    }
    
    ErrorHandler::logInfo("Shop positions received successfully from server");
    
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
        char buffer[256];  // Buffer large enough for any packet type
        std::size_t received;
        sf::IpAddress sender;
        unsigned short port;
        
        sf::Socket::Status status = socket->receive(buffer, sizeof(buffer), received, sender, port);
        
        if (status == sf::Socket::Done) {
            if (sender == sf::IpAddress(ip)) {
                // Check packet type by size
                if (received == sizeof(PositionPacket)) {
                    PositionPacket* inPacket = reinterpret_cast<PositionPacket*>(buffer);
                    
                    if (validatePosition(*inPacket)) {
                        std::lock_guard<std::mutex> lock(mutex);
                        
                        // Update server position with interpolation support
                        if (inPacket->playerId == 0) { // Server is player 0
                            // Store previous position for interpolation
                            serverPosPrevious.x = serverPosTarget.x;
                            serverPosPrevious.y = serverPosTarget.y;
                            
                            // Update target position (latest received)
                            serverPosTarget.x = inPacket->x;
                            serverPosTarget.y = inPacket->y;
                            
                            // Update server health
                            float previousServerHealth = serverHealth;
                            serverHealth = inPacket->health;
                            
                            // Check if server just died (was alive, now dead)
                            if (serverWasAlive && serverHealth <= 0.0f) {
                                serverWasAlive = false;
                                clientPlayer.money += 5000;
                                clientScore += 1;
                                ErrorHandler::logInfo("!!! SERVER PLAYER DIED !!! Client gets $5000 reward and +1 score. Client money: $" + std::to_string(clientPlayer.money) + ", Score: " + std::to_string(clientScore));
                            } else if (serverHealth > 0.0f) {
                                serverWasAlive = true;
                            }
                            
                            serverConnected = true;
                            lastPacketReceived.restart(); // Reset timeout timer
                        }
                        else if (inPacket->playerId == 1) { // Client's own position and health from server
                            // Update client health (calculated on server when hit by bullets)
                            clientHealth = inPacket->health;
                            clientIsAlive = inPacket->isAlive;
                            
                            serverConnected = true;
                            lastPacketReceived.restart(); // Reset timeout timer
                        }
                    }
                }
                else if (received == sizeof(ShotPacket)) {
                    // Handle shot packet from server
                    ShotPacket* shotPacket = reinterpret_cast<ShotPacket*>(buffer);
                    
                    ErrorHandler::logInfo("Received shot packet! Owner: " + std::to_string(shotPacket->playerId));
                    
                    // Create bullet on client
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
                        ErrorHandler::logInfo("Bullet added from server! Total bullets: " + std::to_string(activeBullets.size()));
                    }
                }
                else if (received == sizeof(HitPacket)) {
                    // Handle hit packet from server
                    HitPacket* hitPacket = reinterpret_cast<HitPacket*>(buffer);
                    
                    ErrorHandler::logInfo("Received hit packet! Shooter: " + std::to_string(hitPacket->shooterId) + 
                                         ", Victim: " + std::to_string(hitPacket->victimId) + 
                                         ", Damage: " + std::to_string(hitPacket->damage));
                    
                    // Create damage text at hit location
                    {
                        std::lock_guard<std::mutex> lock(damageTextsMutex);
                        DamageText damageText;
                        damageText.x = hitPacket->hitX;
                        damageText.y = hitPacket->hitY - 30.0f; // Start above hit position
                        damageText.damage = hitPacket->damage;
                        damageTexts.push_back(damageText);
                    }
                    
                    // Mark bullet for removal at hit location
                    {
                        std::lock_guard<std::mutex> lock(bulletsMutex);
                        for (auto& bullet : activeBullets) {
                            // Find bullet near hit location from the shooter
                            if (bullet.ownerId == hitPacket->shooterId) {
                                float dx = bullet.x - hitPacket->hitX;
                                float dy = bullet.y - hitPacket->hitY;
                                float distSq = dx * dx + dy * dy;
                                if (distSq < 100.0f) { // Within 10 pixels
                                    bullet.range = 0.0f; // Mark for removal
                                    break;
                                }
                            }
                        }
                    }
                }
                else {
                    std::ostringstream oss;
                    oss << "Unknown packet size - received " << received << " bytes";
                    ErrorHandler::handleInvalidPacket(oss.str(), ip);
                }
            } else {
                ErrorHandler::handleInvalidPacket("Packet from unexpected sender: " + sender.toString(), ip);
            }
        } else if (status != sf::Socket::NotReady) {
            ErrorHandler::logUDPError("Receive packet", "Socket error occurred");
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

// ========================
// Shop Rendering System
// ========================

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

// Process weapon purchase request
// Requirements: 4.1, 4.2, 4.3, 4.4, 4.5
// Returns: true if purchase successful, false otherwise
bool processPurchase(Player& player, Weapon::Type weaponType) {
    // Create weapon to get price
    Weapon* weapon = Weapon::create(weaponType);
    
    // Requirement 4.1: Validate player has sufficient money
    if (player.money < weapon->price) {
        std::cout << "[PURCHASE] Player has insufficient funds: " 
                  << player.money << " < " << weapon->price << std::endl;
        delete weapon;
        return false;
    }
    
    // Requirement 4.2: Validate player has empty inventory slot
    if (!player.hasInventorySpace()) {
        std::cout << "[PURCHASE] Player has full inventory" << std::endl;
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
    
    std::cout << "[PURCHASE] Player purchased " << weapon->name 
              << " for $" << weapon->price << " in slot " << emptySlot 
              << ". New balance: $" << player.money << std::endl;
    
    return true;
}

// Check if player is near any shop and render interaction prompt
// Requirement 3.1: Display prompt when player within 60 pixels
void renderShopInteractionPrompt(sf::RenderWindow& window, sf::Vector2f playerPosition, 
                                 const std::vector<Shop>& shops, const sf::Font& font) {
    const float INTERACTION_RANGE = 60.0f;  // 60 pixels interaction range
    
    // Check if player is near any shop
    bool nearShop = false;
    for (const auto& shop : shops) {
        if (shop.isPlayerNear(playerPosition.x, playerPosition.y)) {
            nearShop = true;
            break;
        }
    }
    
    // Display prompt if near a shop
    if (nearShop) {
        // Get window size for UI positioning
        sf::Vector2u windowSize = window.getSize();
        
        // Create prompt text
        sf::Text promptText;
        promptText.setFont(font);
        promptText.setString("Press B to purchase");
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

// ========================
// Shop UI Rendering System
// ========================

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
        const float WEAPON_HEIGHT = 120.0f * scale;
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
            
            weaponY += WEAPON_HEIGHT + WEAPON_PADDING;
            
            delete weapon;
        }
    }
    
    // Draw close instruction
    sf::Text closeText;
    closeText.setFont(font);
    closeText.setString("Press B to close");
    closeText.setCharacterSize(static_cast<unsigned int>(22 * scale));
    closeText.setFillColor(sf::Color(200, 200, 200, static_cast<sf::Uint8>(easedProgress * 255)));
    sf::FloatRect closeBounds = closeText.getLocalBounds();
    closeText.setPosition(
        scaledX + (scaledWidth - closeBounds.width) / 2.0f - closeBounds.left,
        scaledY + scaledHeight - 40.0f * scale
    );
    window.draw(closeText);
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
    
    // Restore window icon after recreation
    if (g_windowIcon.getSize().x > 0) {
        window.setIcon(g_windowIcon.getSize().x, g_windowIcon.getSize().y, g_windowIcon.getPixelsPtr());
    }
}

int main() {
    sf::VideoMode desktopMode = sf::VideoMode::getDesktopMode();
    sf::RenderWindow window(desktopMode, "Client", sf::Style::Fullscreen);
    window.setFramerateLimit(60);
    
    // Load and set window icon
    if (g_windowIcon.loadFromFile("Icon.png")) {
        window.setIcon(g_windowIcon.getSize().x, g_windowIcon.getSize().y, g_windowIcon.getPixelsPtr());
    } else {
        std::cerr << "Warning: Failed to load icon!" << std::endl;
    }

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
    sf::CircleShape clientCircle(PLAYER_SIZE / 2.0f);
    clientCircle.setFillColor(sf::Color::Blue);
    clientCircle.setOutlineColor(sf::Color(0, 0, 100));
    clientCircle.setOutlineThickness(3.0f);
    clientCircle.setPosition(clientPos.x - PLAYER_SIZE / 2.0f, clientPos.y - PLAYER_SIZE / 2.0f);

    sf::CircleShape serverCircle(PLAYER_SIZE / 2.0f);
    serverCircle.setFillColor(sf::Color(0, 200, 0, 200));
    serverCircle.setOutlineColor(sf::Color(0, 100, 0));
    serverCircle.setOutlineThickness(2.0f);
    
    // Initialize client player with starting equipment
    // Requirements: 1.1, 1.2, 1.3
    initializePlayer(clientPlayer);
    clientPlayer.x = clientPos.x;
    clientPlayer.y = clientPos.y;
    std::cout << "Client player initialized with USP and $50,000\n" << std::endl;
    
    // Generate shops for standalone client testing
    // In multiplayer, shops will be received from server
    if (shops.empty()) {
        std::cout << "Generating shops for client..." << std::endl;
        // Create 26 shops in a grid pattern for testing
        for (int i = 0; i < 26; i++) {
            Shop shop;
            // Distribute shops across the map
            int row = i / 6;
            int col = i % 6;
            shop.gridX = 5 + col * 8;  // Spread across X
            shop.gridY = 5 + row * 8;  // Spread across Y
            shop.worldX = shop.gridX * CELL_SIZE + CELL_SIZE / 2.0f;
            shop.worldY = shop.gridY * CELL_SIZE + CELL_SIZE / 2.0f;
            shops.push_back(shop);
        }
        std::cout << "Generated " << shops.size() << " shops for client\n" << std::endl;
    }
    
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
        // Set camera view BEFORE processing events so mouse coordinates are correct
        if (state == ClientState::MainScreen) {
            sf::Vector2f renderPos = sf::Vector2f(clientPos.x, clientPos.y);
            updateCamera(window, renderPos);
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
            if (event.type == sf::Event::KeyPressed && state == ClientState::MainScreen) {
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
                            if (shop.isPlayerNear(clientPos.x, clientPos.y)) {
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
                    clientPlayer.switchWeapon(0);
                    ErrorHandler::logInfo("Switched to weapon slot 1");
                }
                else if (event.key.code == sf::Keyboard::Num2) {
                    clientPlayer.switchWeapon(1);
                    ErrorHandler::logInfo("Switched to weapon slot 2");
                }
                else if (event.key.code == sf::Keyboard::Num3) {
                    clientPlayer.switchWeapon(2);
                    ErrorHandler::logInfo("Switched to weapon slot 3");
                }
                else if (event.key.code == sf::Keyboard::Num4) {
                    clientPlayer.switchWeapon(3);
                    ErrorHandler::logInfo("Switched to weapon slot 4");
                }
                
                // Manual reload with R key
                // Requirement 6.3: Handle R key for manual reload
                if (event.key.code == sf::Keyboard::R) {
                    Weapon* activeWeapon = clientPlayer.getActiveWeapon();
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
                const float WEAPON_HEIGHT = 120.0f * scale;
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
                            PurchaseStatus status = calculatePurchaseStatus(clientPlayer, weapon);
                            
                            if (status == PurchaseStatus::Purchasable) {
                                // Process purchase locally
                                bool success = processPurchase(clientPlayer, weaponType);
                                
                                if (success) {
                                    ErrorHandler::logInfo("Client player purchased " + weapon->name);
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
                Weapon* activeWeapon = clientPlayer.getActiveWeapon();
                if (activeWeapon == nullptr) {
                    ErrorHandler::logInfo("Cannot fire: No active weapon. Active slot: " + std::to_string(clientPlayer.activeSlot));
                }
                if (activeWeapon != nullptr && !shopUIOpen && !inventoryOpen) {
                    // Check if weapon can fire
                    if (activeWeapon->canFire()) {
                        // Get mouse position in world coordinates
                        sf::Vector2i mousePixelPos = sf::Mouse::getPosition(window);
                        sf::Vector2f mouseWorldPos = window.mapPixelToCoords(mousePixelPos);
                        
                        // Calculate bullet trajectory
                        float dx = mouseWorldPos.x - clientPos.x;
                        float dy = mouseWorldPos.y - clientPos.y;
                        float distance = std::sqrt(dx * dx + dy * dy);
                        
                        if (distance > 0.001f) {  // Avoid division by zero
                            // Normalize direction
                            dx /= distance;
                            dy /= distance;
                            
                            // Create bullet
                            Bullet bullet;
                            bullet.ownerId = 0;  // Client player ID
                            bullet.x = clientPos.x;
                            bullet.y = clientPos.y;
                            bullet.prevX = clientPos.x;  // Initialize previous position
                            bullet.prevY = clientPos.y;
                            bullet.vx = dx * activeWeapon->bulletSpeed;
                            bullet.vy = dy * activeWeapon->bulletSpeed;
                            bullet.damage = activeWeapon->damage;
                            bullet.range = activeWeapon->range;
                            bullet.maxRange = activeWeapon->range;
                            bullet.weaponType = activeWeapon->type;
                            
                            // Fire weapon (consumes ammo)
                            activeWeapon->fire();
                            
                            // NOTE: Don't create bullet locally - wait for server to send it back
                            // This prevents duplicate bullets (local + server response)
                            
                            // Send shot packet to server
                            ShotPacket shotPacket;
                            shotPacket.playerId = 0; // Client is player 0
                            shotPacket.x = clientPos.x;
                            shotPacket.y = clientPos.y;
                            shotPacket.dirX = dx;
                            shotPacket.dirY = dy;
                            shotPacket.weaponType = static_cast<uint8_t>(activeWeapon->type);
                            shotPacket.bulletSpeed = activeWeapon->bulletSpeed;
                            shotPacket.damage = activeWeapon->damage;
                            shotPacket.range = activeWeapon->range;
                            
                            // Create temporary UDP socket for sending shot
                            sf::UdpSocket shotSocket;
                            sf::Socket::Status shotStatus = shotSocket.send(&shotPacket, sizeof(ShotPacket), sf::IpAddress(serverIP), 53001);
                            if (shotStatus == sf::Socket::Done) {
                                ErrorHandler::logInfo("Shot packet sent to server");
                            } else {
                                ErrorHandler::logUDPError("Send shot packet", "Failed to send to server");
                            }
                            
                            ErrorHandler::logInfo("Fired " + activeWeapon->name + " - Ammo: " + 
                                                 std::to_string(activeWeapon->currentAmmo) + "/" + 
                                                 std::to_string(activeWeapon->reserveAmmo));
                        }
                    }
                    // Requirement 6.2: Trigger automatic reload when magazine empty
                    else if (activeWeapon->currentAmmo == 0 && activeWeapon->reserveAmmo > 0) {
                        activeWeapon->startReload();
                        ErrorHandler::logInfo("Automatic reload triggered for " + activeWeapon->name);
                    }
                }
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

        // Clear window with black for menu screens, fogged background will be drawn in MainScreen
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
                    if (grid[i][j].topWall != WallType::None) wallCount++;
                    if (grid[i][j].rightWall != WallType::None) wallCount++;
                    if (grid[i][j].bottomWall != WallType::None) wallCount++;
                    if (grid[i][j].leftWall != WallType::None) wallCount++;
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
                // Requirements: 5.3, 5.4 - Use weapon-modified movement speed
                float currentSpeed = clientPlayer.getMovementSpeed();
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) newPos.y -= currentSpeed * deltaTime * 60.0f;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) newPos.y += currentSpeed * deltaTime * 60.0f;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) newPos.x -= currentSpeed * deltaTime * 60.0f;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) newPos.x += currentSpeed * deltaTime * 60.0f;
                
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
            
            // Update weapon reload state
            // Requirement 6.4: Update reload progress
            {
                Weapon* activeWeapon = clientPlayer.getActiveWeapon();
                if (activeWeapon != nullptr) {
                    activeWeapon->updateReload(deltaTime);
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
                const float PLAYER_RADIUS = 20.0f; // PLAYER_SIZE / 2
                
                // Debug: Log bullet count
                static sf::Clock bulletLogClock;
                if (bulletLogClock.getElapsedTime().asSeconds() > 2.0f && !activeBullets.empty()) {
                    ErrorHandler::logInfo("Active bullets: " + std::to_string(activeBullets.size()));
                    bulletLogClock.restart();
                }
                
                // NOTE: Collision detection is now handled by server only
                // Server will send HitPacket when bullet hits a player
                // This prevents duplicate damage text (local detection + server notification)
                // Client only renders bullets, server handles all game logic
                
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
            
            // Render fogged background (must be after camera update)
            renderFoggedBackground(window, renderPos);
            
            // Get window size for UI rendering later
            sf::Vector2u windowSize = window.getSize();
            
            // NOTE: Scaling is no longer needed with the new camera system
            // The camera view handles coordinate transformation automatically
            // All rendering now uses world coordinates directly (0-5000 range)
            
            // Get server position for rendering with interpolation
            sf::Vector2f currentServerPos;
            bool isServerConnected = false;
            {
                std::lock_guard<std::mutex> lock(mutex);
                
                // Interpolate server position for smooth movement
                // This prevents jerky movement when receiving position updates at 20Hz
                // We interpolate between the previous and target positions
                const float serverInterpolationSpeed = 15.0f; // Higher = faster catch-up
                float serverAlpha = std::min(1.0f, deltaTime * serverInterpolationSpeed);
                
                serverPos.x = lerp(serverPos.x, serverPosTarget.x, serverAlpha);
                serverPos.y = lerp(serverPos.y, serverPosTarget.y, serverAlpha);
                
                currentServerPos = sf::Vector2f(serverPos.x, serverPos.y);
                isServerConnected = serverConnected;
            }
            
            // Render visible walls using cell-based system
            renderVisibleWalls(window, sf::Vector2f(clientPos.x, clientPos.y), grid);
            
            // Render shops with fog of war integration
            // Requirements: 2.6, 3.1, 10.5
            renderShops(window, renderPos, shops);
            
            // Draw server player (green circle) with fog of war
            if (isServerConnected) {
                // Calculate distance from client to server player
                float dx = currentServerPos.x - clientPos.x;
                float dy = currentServerPos.y - clientPos.y;
                float distance = std::sqrt(dx * dx + dy * dy);
                
                // Calculate fog alpha
                sf::Uint8 alpha = calculateFogAlpha(distance);
                
                // Only draw if visible
                if (alpha > 0) {
                    sf::CircleShape serverCircle(PLAYER_SIZE / 2.0f);
                    
                    // Apply fog to server player color
                    sf::Color foggedColor(0, 200, 0, alpha);
                    serverCircle.setFillColor(foggedColor);
                    
                    sf::Color foggedOutline(0, 100, 0, alpha);
                    serverCircle.setOutlineColor(foggedOutline);
                    serverCircle.setOutlineThickness(2.0f);
                    
                    serverCircle.setPosition(
                        currentServerPos.x - PLAYER_SIZE / 2.0f,
                        currentServerPos.y - PLAYER_SIZE / 2.0f
                    );
                    window.draw(serverCircle);
                }
            }
            
            // Requirement 7.1: Render bullets as white lines (5px length, 2px width)
            {
                std::lock_guard<std::mutex> lock(bulletsMutex);
                
                for (const auto& bullet : activeBullets) {
                    // Calculate distance for fog
                    float dx = bullet.x - renderPos.x;
                    float dy = bullet.y - renderPos.y;
                    float distance = std::sqrt(dx * dx + dy * dy);
                    
                    // Calculate fog alpha
                    sf::Uint8 alpha = calculateFogAlpha(distance);
                    
                    // Only draw if visible
                    if (alpha > 0) {
                        // Calculate bullet direction for line rendering
                        float speed = std::sqrt(bullet.vx * bullet.vx + bullet.vy * bullet.vy);
                        float dirX = (speed > 0.001f) ? bullet.vx / speed : 1.0f;
                        float dirY = (speed > 0.001f) ? bullet.vy / speed : 0.0f;
                        
                        // Create line from bullet position extending 5px in direction of travel
                        sf::Vertex line[] = {
                            sf::Vertex(sf::Vector2f(bullet.x, bullet.y), sf::Color(255, 255, 255, alpha)),
                            sf::Vertex(sf::Vector2f(bullet.x + dirX * 5.0f, bullet.y + dirY * 5.0f), sf::Color(255, 255, 255, alpha))
                        };
                        
                        window.draw(line, 2, sf::Lines);
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
            
            // Draw local client player as blue circle - always visible
            clientCircle.setRadius(PLAYER_SIZE / 2.0f);
            clientCircle.setFillColor(sf::Color::Blue);
            clientCircle.setOutlineColor(sf::Color(0, 0, 100));
            clientCircle.setOutlineThickness(3.0f);
            clientCircle.setPosition(renderPos.x - PLAYER_SIZE / 2.0f, renderPos.y - PLAYER_SIZE / 2.0f);
            window.draw(clientCircle);
            
            // Render fog overlay on top of everything (creates vignette effect)
            renderFogOverlay(window, renderPos);

            // Reset to default view for UI rendering (HUD should be fixed on screen)
            sf::View uiView;
            uiView.setSize(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y));
            uiView.setCenter(static_cast<float>(window.getSize().x) / 2.0f, static_cast<float>(window.getSize().y) / 2.0f);
            window.setView(uiView);
            
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
            
            // Draw money balance below health
            // Requirement 1.5: Display money balance below health in top-left corner
            sf::Text moneyText;
            moneyText.setFont(font);
            moneyText.setString("Money: $" + std::to_string(clientPlayer.money));
            moneyText.setCharacterSize(28);
            moneyText.setFillColor(sf::Color(255, 215, 0));  // Gold color
            moneyText.setPosition(20.0f, 100.0f);
            window.draw(moneyText);
            
            // Draw weapon info in top-right corner
            // Requirements: 5.5, 5.6
            sf::Text weaponText;
            weaponText.setFont(font);
            weaponText.setCharacterSize(28);
            
            // Get active weapon from client player
            Weapon* activeWeapon = clientPlayer.getActiveWeapon();
            if (activeWeapon != nullptr) {
                // Requirement 5.5: Display weapon name and ammo count "[Name]: [current]/[reserve]"
                std::string weaponInfo = activeWeapon->name + ": " + 
                                        std::to_string(activeWeapon->currentAmmo) + "/" + 
                                        std::to_string(activeWeapon->reserveAmmo);
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
            if (activeWeapon != nullptr && activeWeapon->isReloading) {
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
                // Get current player for purchase status calculation
                Player currentPlayer;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    // Create a temporary player with current state
                    currentPlayer.money = 50000;  // TODO: Get actual player money from server
                    currentPlayer.inventory[0] = nullptr;  // TODO: Get actual inventory from server
                    currentPlayer.inventory[1] = nullptr;
                    currentPlayer.inventory[2] = nullptr;
                    currentPlayer.inventory[3] = nullptr;
                }
                
                renderShopUI(window, currentPlayer, font, shopAnimationProgress);
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
                        slotWeapon = clientPlayer.inventory[i];
                    }
                    
                    // Highlight active weapon slot
                    bool isActiveSlot = (i == clientPlayer.activeSlot);
                    
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
            renderShopInteractionPrompt(window, renderPos, shops, font);
            
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
            connectionLostText.setString("Connection lost. Press J to reconnect to " + serverIP);
            connectionLostText.setCharacterSize(32);
            connectionLostText.setFillColor(sf::Color::Red);
            
            sf::Vector2u windowSize = window.getSize();
            connectionLostText.setPosition(
                windowSize.x / 2 - connectionLostText.getLocalBounds().width / 2,
                windowSize.y / 2
            );
            
            window.draw(connectionLostText);
            
            // Handle R key for reconnection
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::J)) {
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