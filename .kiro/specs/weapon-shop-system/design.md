# Design Document: Weapon and Shop System

## Overview

The weapon and shop system extends Zero Ground with comprehensive combat mechanics including weapon purchasing, inventory management, shooting, and damage calculation. The design maintains the existing two-file client-server architecture (Zero_Ground.cpp and Zero_Ground_client.cpp) while adding weapon trading, ballistics, and synchronized combat across the network.

**IMPORTANT: Both Zero_Ground.cpp and Zero_Ground_client.cpp contain identical client-side rendering code.** The host player (running Zero_Ground.cpp) sees the same UI, graphics, and effects as remote players (running Zero_Ground_client.cpp). All client-side features must be implemented in BOTH files to ensure consistent experience.

Key design principles:
- Server-authoritative combat: All damage calculations and hit detection occur on the server
- Client-side prediction: Bullet visualization happens immediately on the client for responsiveness
- Identical client experience: Both host and remote players see the same UI, HUD, bullets, and effects
- Minimal network overhead: Efficient packet structures for shot synchronization
- Grid-based optimization: Leverage existing 51×51 grid for shop placement and spatial queries
- Performance-conscious: Bullet limits and spatial partitioning to maintain 60 FPS

## Architecture

### System Components

The weapon system integrates into the existing client-server architecture:

```
┌─────────────────────────────────────────────────────────────┐
│                    Client (Zero_Ground_client.cpp)           │
├─────────────────────────────────────────────────────────────┤
│  Input Handler                                               │
│  ├─ Mouse click → Fire weapon                                │
│  ├─ Key 1-4 → Switch weapon                                  │
│  ├─ Key B → Open shop (if near)                              │
│  └─ Key R → Reload                                           │
│                                                               │
│  Rendering System                                            │
│  ├─ Shop visualization (red squares)                         │
│  ├─ Bullet trails (white lines)                              │
│  ├─ HUD (money, ammo, weapon name)                           │
│  └─ Damage numbers (floating text)                           │
│                                                               │
│  Local State                                                 │
│  ├─ Player inventory (4 slots)                               │
│  ├─ Active weapon                                            │
│  ├─ Money balance                                            │
│  └─ Bullet entities (visual only)                            │
└─────────────────────────────────────────────────────────────┘
                            │
                            │ UDP (port 53001)
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                    Server (Zero_Ground.cpp)                  │
├─────────────────────────────────────────────────────────────┤
│  Game State Authority                                        │
│  ├─ Player health and positions                              │
│  ├─ Shop locations (26 shops)                                │
│  ├─ Weapon ownership per player                              │
│  └─ Money balances                                           │
│                                                               │
│  Combat System                                               │
│  ├─ Shot validation                                          │
│  ├─ Raycast collision detection                              │
│  ├─ Damage application                                       │
│  └─ Kill rewards ($5000)                                     │
│                                                               │
│  Shop System                                                 │
│  ├─ Purchase validation                                      │
│  ├─ Inventory management                                     │
│  └─ Money transactions                                       │
│                                                               │
│  Spatial Optimization                                        │
│  └─ Quadtree (100×100px cells)                               │
└─────────────────────────────────────────────────────────────┘
```

### Data Flow

1. **Weapon Purchase Flow**:
   - Client: Player presses B near shop → Send purchase request
   - Server: Validate money + inventory space → Deduct money → Add weapon
   - Server: Broadcast inventory update to all clients
   - Client: Update local inventory display

2. **Shooting Flow**:
   - Client: Player clicks mouse → Calculate trajectory → Send shot packet
   - Client: Immediately render bullet locally (prediction)
   - Server: Receive shot → Validate (ammo, cooldown) → Raycast for hits
   - Server: Apply damage if hit → Broadcast shot result to all clients
   - Clients: Render bullet based on server data

3. **Damage Flow**:
   - Server: Bullet hits player → Reduce health → Check for death
   - Server: If death → Award $5000 to killer → Schedule respawn (5s)
   - Server: Broadcast damage event to all clients
   - Clients: Display damage number on hit player

## Components and Interfaces

### Weapon Structure

```cpp
struct Weapon {
    enum Type {
        USP, GLOCK, FIVESEVEN, R8,           // Pistols
        GALIL, M4, AK47,                      // Rifles
        M10, AWP, M40                         // Snipers
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
    
    // Factory method
    static Weapon* create(Type type);
    
    // Methods
    bool canFire() const;
    void startReload();
    void updateReload(float deltaTime);
    void fire();
};
```

### Shop Structure

```cpp
struct Shop {
    int gridX, gridY;              // Position in 51×51 grid
    float worldX, worldY;          // World coordinates (center of cell)
    sf::RectangleShape visual;     // 20×20 red square
    
    // Check if player is in interaction range
    bool isPlayerNear(float px, float py) const {
        float dx = worldX - px;
        float dy = worldY - py;
        return (dx*dx + dy*dy) <= (60*60);
    }
};
```

### Bullet Structure

```cpp
struct Bullet {
    uint8_t ownerId;           // Player who fired
    float x, y;                // Current position
    float vx, vy;              // Velocity vector
    float damage;
    float range;               // Remaining range
    float maxRange;            // Initial range
    Weapon::Type weaponType;
    sf::Clock lifetime;
    
    // Update position
    void update(float deltaTime);
    
    // Check if bullet should be removed
    bool shouldRemove() const;
};
```

### Player Extensions

```cpp
class Player {
    // Existing fields...
    
    // New weapon system fields
    std::array<Weapon*, 4> inventory;  // 4 inventory slots
    int activeSlot;                     // 0-3, or -1 for no weapon
    int money;                          // Starting at 50,000
    
    // Methods
    Weapon* getActiveWeapon();
    bool hasInventorySpace() const;
    int getFirstEmptySlot() const;
    void addWeapon(Weapon* weapon);
    void switchWeapon(int slot);
    float getMovementSpeed() const;     // Base speed or weapon-modified
};
```

### Network Packet Structures

```cpp
// Shot packet (client → server → all clients)
struct ShotPacket {
    uint8_t playerId;
    float startX, startY;
    float dirX, dirY;          // Normalized direction vector
    uint8_t weaponType;
    uint32_t timestamp;        // For lag compensation
};

// Hit confirmation packet (server → all clients)
struct HitPacket {
    uint8_t shooterId;
    uint8_t victimId;
    float damage;
    float hitX, hitY;          // Hit location for effects
    bool wasKill;              // True if victim died
};

// Purchase packet (client → server)
struct PurchasePacket {
    uint8_t playerId;
    uint8_t weaponType;
};

// Inventory update packet (server → all clients)
struct InventoryPacket {
    uint8_t playerId;
    uint8_t slot;              // Which slot changed
    uint8_t weaponType;        // 255 = empty slot
    int newMoneyBalance;
};
```

## Data Models

### Weapon Catalog

All weapon statistics defined as constants:

```cpp
// Pistols
USP:        mag=12,  dmg=15,  range=250,  speed=600,  reload=2.0s, movespeed=2.5
Glock-18:   mag=20,  dmg=10,  range=300,  speed=600,  reload=2.0s, movespeed=2.5
Five-SeveN: mag=20,  dmg=10,  range=400,  speed=800,  reload=2.0s, movespeed=2.5
R8:         mag=8,   dmg=50,  range=200,  speed=700,  reload=5.0s, movespeed=2.5

// Rifles
Galil AR:   mag=35,  dmg=25,  range=450,  speed=900,  reload=3.0s, movespeed=2.0
M4:         mag=30,  dmg=30,  range=425,  speed=850,  reload=3.0s, movespeed=1.8
AK-47:      mag=25,  dmg=35,  range=450,  speed=900,  reload=3.0s, movespeed=1.6

// Snipers
M10:        mag=5,   dmg=50,  range=1000, speed=2000, reload=4.0s, movespeed=1.1
AWP:        mag=1,   dmg=100, range=1000, speed=2000, reload=1.5s, movespeed=1.0
M40:        mag=1,   dmg=99,  range=2000, speed=4000, reload=1.5s, movespeed=1.2
```

### Shop Generation Algorithm

```
1. Initialize empty list of shop positions
2. For i = 0 to 25:
   a. Generate random gridX in [0, 50]
   b. Generate random gridY in [0, 50]
   c. Check if (gridX, gridY) already occupied → retry if yes
   d. Check if within 5 cells of any spawn point → retry if yes
   e. Add (gridX, gridY) to shop list
3. Verify map connectivity using BFS from spawn points
4. If connectivity broken → restart from step 1
5. Convert grid positions to world coordinates:
   worldX = gridX * 100 + 40
   worldY = gridY * 100 + 40
```

### Collision Detection System

Using quadtree for efficient bullet-player and bullet-wall collision:

```
Quadtree Structure:
- Root covers entire 5100×5100 map
- Subdivides into 100×100 pixel cells
- Each cell contains references to:
  - Players in that cell
  - Walls in that cell
  
Bullet Update:
1. Calculate new position: pos += velocity * deltaTime
2. Query quadtree for cell at new position
3. Check collision with all walls in cell
4. Check collision with all players in cell
5. If collision → handle hit, remove bullet
6. If no collision → continue
```

## Corr
ectness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system-essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property 1: Player spawn initialization
*For any* player spawn event, the player's inventory slot 0 should contain a USP weapon, slots 1-3 should be empty, and money balance should equal 50,000 dollars.
**Validates: Requirements 1.1, 1.2, 1.3**

### Property 2: Shop count invariant
*For any* generated map, the number of shop entities should equal exactly 26.
**Validates: Requirements 2.1**

### Property 3: Shop position uniqueness
*For any* generated map, no two shops should occupy the same grid cell coordinates.
**Validates: Requirements 2.2**

### Property 4: Shop spawn distance constraint
*For any* shop on any generated map, the distance from that shop to the nearest spawn point should be greater than 5 grid cells.
**Validates: Requirements 2.3**

### Property 5: Shop interaction range
*For any* player and shop, when the Euclidean distance between them is less than or equal to 60 pixels, the interaction prompt should be available.
**Validates: Requirements 3.1**

### Property 6: Purchase status calculation
*For any* player and weapon, the purchase status should be "insufficient funds" when player money < weapon price, "inventory full" when all 4 slots are occupied, and "purchasable" otherwise.
**Validates: Requirements 3.5**

### Property 7: Insufficient funds prevents purchase
*For any* purchase attempt where player money is less than weapon price, the purchase should fail and money balance should remain unchanged.
**Validates: Requirements 4.1**

### Property 8: Full inventory prevents purchase
*For any* purchase attempt where all 4 inventory slots are occupied, the purchase should fail and inventory should remain unchanged.
**Validates: Requirements 4.2**

### Property 9: Purchase money deduction
*For any* successful weapon purchase, the player's money balance should decrease by exactly the weapon's price.
**Validates: Requirements 4.3**

### Property 10: Weapon placement in first empty slot
*For any* successful weapon purchase, the weapon should be added to the lowest-numbered empty inventory slot.
**Validates: Requirements 4.4**

### Property 11: Purchased weapon initialization
*For any* weapon added to inventory, the current ammo should equal magazine size and reserve ammo should equal the weapon's maximum reserve capacity.
**Validates: Requirements 4.5**

### Property 12: Inventory slot activation
*For any* key press (1-4), the active slot should be set to the corresponding index (0-3).
**Validates: Requirements 5.1**

### Property 13: Non-empty slot sets active weapon
*For any* inventory slot that contains a weapon, activating that slot should set the active weapon to that weapon instance.
**Validates: Requirements 5.2**

### Property 14: Empty slot clears weapon and restores speed
*For any* inventory slot that is empty, activating that slot should set active weapon to null and player movement speed to 3.0.
**Validates: Requirements 5.3**

### Property 15: Weapon speed modification
*For any* active weapon, the player's movement speed should equal the weapon's movement speed property.
**Validates: Requirements 5.4**

### Property 16: Bullet creation on valid shot
*For any* fire action when active weapon exists and magazine ammo > 0, a bullet entity should be created with trajectory toward the cursor position.
**Validates: Requirements 6.1**

### Property 17: Empty magazine triggers reload
*For any* fire action when active weapon magazine ammo equals 0, the weapon should enter reloading state.
**Validates: Requirements 6.2**

### Property 18: Manual reload initiation
*For any* reload key press when active weapon exists and reserve ammo > 0, the weapon should enter reloading state.
**Validates: Requirements 6.3**

### Property 19: Reload prevents firing
*For any* weapon in reloading state, fire actions should not create bullet entities.
**Validates: Requirements 6.4**

### Property 20: Reload ammo transfer
*For any* completed reload, magazine ammo should equal min(magazine capacity, previous magazine ammo + previous reserve ammo), and reserve ammo should decrease accordingly.
**Validates: Requirements 6.5**

### Property 21: Bullet velocity matches weapon
*For any* bullet entity, its velocity magnitude should equal the bullet speed property of the weapon that fired it.
**Validates: Requirements 7.2**

### Property 22: Wall collision removes bullet
*For any* bullet entity that intersects with a wall, the bullet should be removed from the active bullets list.
**Validates: Requirements 7.3**

### Property 23: Player collision removes bullet and applies damage
*For any* bullet entity that intersects with a player entity, the bullet should be removed and the player's health should decrease by the bullet's damage value.
**Validates: Requirements 7.4**

### Property 24: Range limit removes bullet
*For any* bullet entity, when the distance traveled exceeds the weapon's effective range, the bullet should be removed.
**Validates: Requirements 7.5**

### Property 25: Damage reduces health
*For any* bullet hit on a player, the player's health should decrease by exactly the weapon's damage value.
**Validates: Requirements 8.1**

### Property 26: Death triggers respawn
*For any* player whose health reaches zero or below, the player should be marked for respawn after 5 seconds.
**Validates: Requirements 8.3**

### Property 27: Kill reward
*For any* player elimination, the eliminating player's money balance should increase by 5,000 dollars.
**Validates: Requirements 8.4**

### Property 28: Respawn health restoration
*For any* player respawn event, the player's health should be set to 100 HP.
**Validates: Requirements 8.5**

### Property 29: Bullet count limit
*For any* player at any time, the number of active bullets owned by that player should not exceed 20.
**Validates: Requirements 10.1**

### Property 30: Screen culling removes bullets
*For any* bullet entity whose position is outside the visible screen area plus 20% buffer, the bullet should be removed.
**Validates: Requirements 10.2**

### Property 31: Map boundary removes bullets
*For any* bullet entity whose x or y coordinate is less than 0 or greater than 5100, the bullet should be removed.
**Validates: Requirements 10.3**

### Property 32: Fog of war consistency for shops
*For any* shop entity, its visibility should be determined by the same fog of war calculation used for other map entities.
**Validates: Requirements 10.5**

### Property 33: Weapon property completeness
*For any* weapon instance, all required properties (magazine size, damage, range, bullet speed, reload time, movement speed) should have defined non-zero values.
**Validates: Requirements 11.2**

## Error Handling

### Client-Side Error Handling

1. **Invalid Purchase Attempts**:
   - Display localized error message in UI
   - Play error sound effect
   - Do not send purchase packet to server
   - Keep shop interface open for retry

2. **Network Disconnection**:
   - Display "Connection Lost" message
   - Freeze game state
   - Attempt reconnection every 2 seconds
   - Return to main menu after 10 failed attempts

3. **Invalid Weapon Switch**:
   - Ignore key press if slot number > 4
   - Maintain current active weapon
   - No error message (silent fail)

### Server-Side Error Handling

1. **Invalid Purchase Validation**:
   - Check player money >= weapon price
   - Check inventory has empty slot
   - Check weapon type is valid (0-9)
   - If any check fails: send rejection packet, log warning

2. **Invalid Shot Validation**:
   - Check player has active weapon
   - Check weapon has ammo > 0
   - Check shot cooldown elapsed
   - If any check fails: ignore shot packet, do not broadcast

3. **Malformed Packets**:
   - Validate packet size matches expected structure
   - Validate player ID exists in game
   - If invalid: log error, drop packet, do not crash

4. **Shop Generation Failure**:
   - If connectivity check fails after 100 attempts: use fallback shop positions
   - Fallback: place shops in predetermined grid pattern
   - Log warning about fallback usage

## Testing Strategy

### Unit Testing Approach

The weapon and shop system will use unit tests to verify specific examples and edge cases:

1. **Weapon Initialization Tests**:
   - Test each weapon type creates with correct stats
   - Test USP has magazine=12, damage=15, range=250
   - Test AWP has magazine=1, damage=100, range=1000

2. **Purchase Logic Tests**:
   - Test purchase fails when money = price - 1
   - Test purchase succeeds when money = price
   - Test purchase fails when inventory slots = 4/4
   - Test purchase succeeds when inventory slots = 3/4

3. **Reload Logic Tests**:
   - Test reload with reserve > magazine capacity
   - Test reload with reserve < magazine capacity
   - Test reload with reserve = 0 (should not reload)

4. **Collision Detection Tests**:
   - Test bullet-wall collision at perpendicular angle
   - Test bullet-wall collision at 45-degree angle
   - Test bullet-player collision at center
   - Test bullet-player collision at edge

5. **Shop Generation Tests**:
   - Test 26 shops are generated
   - Test no shops overlap
   - Test no shops within 5 cells of spawn

### Property-Based Testing Approach

Property-based testing will verify universal properties across randomized inputs using a C++ property testing library (RapidCheck or similar).

**Library Selection**: RapidCheck (https://github.com/emil-e/rapidcheck)
- Header-only library, easy integration
- Supports custom generators
- Configurable iteration count
- Good error reporting

**Configuration**: Each property test will run a minimum of 100 iterations with randomized inputs.

**Property Test Tagging**: Each property-based test will include a comment tag in this format:
```cpp
// **Feature: weapon-shop-system, Property 1: Player spawn initialization**
RC_GTEST_PROP(WeaponSystem, PlayerSpawnInitialization, ()) {
    // Test implementation
}
```

**Key Property Tests**:

1. **Property 1: Player spawn initialization**
   - Generator: Random player IDs
   - Test: Spawn player, verify slot 0 = USP, slots 1-3 = null, money = 50000

2. **Property 9: Purchase money deduction**
   - Generator: Random weapon types, random initial money (>= weapon price)
   - Test: Purchase weapon, verify money decreased by weapon price

3. **Property 20: Reload ammo transfer**
   - Generator: Random weapons, random current ammo, random reserve ammo
   - Test: Complete reload, verify magazine = min(capacity, prev_mag + prev_reserve)

4. **Property 25: Damage reduces health**
   - Generator: Random weapons, random player health (> 0)
   - Test: Apply bullet hit, verify health decreased by weapon damage

5. **Property 29: Bullet count limit**
   - Generator: Random rapid fire sequences
   - Test: Fire many shots quickly, verify active bullets <= 20

6. **Property 31: Map boundary removes bullets**
   - Generator: Random bullet positions and velocities
   - Test: Update bullets, verify any with x/y outside [0, 5100] are removed

**Custom Generators**:

```cpp
// Generate random weapon types
auto genWeaponType() {
    return rc::gen::inRange(0, 10); // 0-9 for 10 weapon types
}

// Generate random player with inventory state
auto genPlayer() {
    return rc::gen::build<Player>(
        rc::gen::set(&Player::money, rc::gen::inRange(0, 100000)),
        rc::gen::set(&Player::health, rc::gen::inRange(1, 100)),
        rc::gen::set(&Player::x, rc::gen::inRange(0.0f, 5100.0f)),
        rc::gen::set(&Player::y, rc::gen::inRange(0.0f, 5100.0f))
    );
}

// Generate random bullet
auto genBullet() {
    return rc::gen::build<Bullet>(
        rc::gen::set(&Bullet::x, rc::gen::inRange(-100.0f, 5200.0f)),
        rc::gen::set(&Bullet::y, rc::gen::inRange(-100.0f, 5200.0f)),
        rc::gen::set(&Bullet::vx, rc::gen::inRange(-2000.0f, 2000.0f)),
        rc::gen::set(&Bullet::vy, rc::gen::inRange(-2000.0f, 2000.0f))
    );
}
```

### Integration Testing

Integration tests will verify the interaction between client and server:

1. **Client-Server Purchase Flow**:
   - Start server and client
   - Client sends purchase packet
   - Verify server updates inventory
   - Verify client receives inventory update

2. **Client-Server Combat Flow**:
   - Start server and 2 clients
   - Client 1 fires at Client 2
   - Verify server detects hit
   - Verify both clients receive hit notification
   - Verify Client 2 health decreases

3. **Shop Generation Consistency**:
   - Generate map on server
   - Send shop positions to clients
   - Verify all clients render shops at same positions

### Performance Testing

Performance tests will verify the system maintains 60 FPS:

1. **Bullet Stress Test**:
   - Spawn 10 players
   - Each player fires continuously
   - Measure frame time with 200 active bullets
   - Target: < 16.67ms per frame

2. **Shop Rendering Test**:
   - Generate map with 26 shops
   - Render all shops visible on screen
   - Measure frame time
   - Target: < 1ms for shop rendering

3. **Collision Detection Test**:
   - Spawn 100 bullets
   - Spawn 50 players
   - Measure collision detection time
   - Target: < 5ms per frame

## Implementation Notes

### File Organization

All code will be added to existing files:

**Zero_Ground.cpp (Server + Host Client)**:
- **Server-side logic**:
  - Shop generation algorithm
  - Purchase validation logic
  - Shot validation and hit detection
  - Damage application
  - Respawn logic
- **Client-side rendering (for host player)**:
  - Weapon struct definition
  - Shop struct definition
  - Weapon rendering
  - Shop rendering
  - Bullet rendering
  - HUD updates (money, ammo)
  - Shop UI interface
  - Input handling (B key, R key, mouse click)
  - Local bullet prediction

**Zero_Ground_client.cpp (Remote Client)**:
- **Client-side rendering (for remote players)**:
  - Weapon struct definition
  - Shop struct definition
  - Weapon rendering
  - Shop rendering
  - Bullet rendering
  - HUD updates (money, ammo)
  - Shop UI interface
  - Input handling (B key, R key, mouse click)
  - Local bullet prediction

**Note**: All client-side rendering code must be identical in both files to ensure consistent experience for all players.

### Network Protocol Extensions

Extend existing UDP protocol with new packet types:

```cpp
enum PacketType {
    // Existing types...
    PACKET_SHOT = 10,
    PACKET_HIT = 11,
    PACKET_PURCHASE = 12,
    PACKET_INVENTORY_UPDATE = 13,
    PACKET_SHOP_POSITIONS = 14
};
```

### Performance Considerations

1. **Quadtree Implementation**:
   - Build once per frame
   - Only rebuild if players/walls change
   - Use fixed 100×100 cell size (51×51 grid)

2. **Bullet Pooling**:
   - Pre-allocate array of 200 bullet objects
   - Reuse inactive bullets instead of new/delete
   - Reduces memory allocation overhead

3. **Network Optimization**:
   - Batch multiple shots into single packet if fired same frame
   - Use delta compression for position updates
   - Only send shop positions once at map generation

4. **Rendering Optimization**:
   - Cull bullets outside view frustum
   - Use vertex arrays for batch rendering bullets
   - Cache shop visual rectangles

### Compatibility with Existing Systems

The weapon system integrates with existing Zero Ground systems:

1. **Grid System**: Shops use existing 51×51 grid for placement
2. **Fog of War**: Shops respect existing visibility calculations
3. **Player Movement**: Weapon speed modifiers adjust existing movement code
4. **Network Layer**: Uses existing UDP socket on port 53001
5. **Collision System**: Extends existing wall collision for bullets

No breaking changes to existing functionality.
