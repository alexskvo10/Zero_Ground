# Requirements Document

## Introduction

This document specifies the requirements for a comprehensive weapon, shop, and combat system for Zero Ground, a multiplayer top-down shooter. The system includes weapon mechanics, shop placement and trading, inventory management, shooting mechanics, damage calculation, and network synchronization across a 51×51 grid-based map.

The implementation maintains the existing two-file architecture: Zero_Ground.cpp (server) and Zero_Ground_client.cpp (client), without introducing additional dependencies or external libraries beyond those already in use (SFML).

## Glossary

- **Game System**: The Zero Ground multiplayer shooter application
- **Server Application**: The Zero_Ground.cpp file containing server-side game logic
- **Client Application**: The Zero_Ground_client.cpp file containing client-side rendering and input
- **Player Entity**: A controllable character in the game with health, inventory, and money
- **Shop Entity**: A stationary trading point on the map where weapons can be purchased
- **Weapon Instance**: An item with specific characteristics (damage, ammo, speed) that can be equipped
- **Inventory Slot**: One of 4 storage positions for weapons in a player's inventory
- **Active Weapon**: The currently equipped weapon that the player is holding
- **Bullet Entity**: A projectile fired from a weapon that travels in a straight line
- **Grid Cell**: One unit in the 51×51 map grid, representing 100×100 pixels
- **Spawn Point**: A location where players appear when joining or respawning
- **HUD**: Heads-Up Display showing player information (health, money, ammo)
- **Fog of War**: Visibility system that obscures areas outside player's view range
- **UDP Packet**: Network message sent via User Datagram Protocol on port 53001
- **Quadtree**: Spatial partitioning data structure for collision detection optimization

## Requirements

### Requirement 1: Player Initial State

**User Story:** As a player, I want to spawn with a starting weapon and money, so that I can immediately participate in combat and purchase upgrades.

#### Acceptance Criteria

1. WHEN a Player Entity spawns THEN the Game System SHALL equip the Player Entity with one USP weapon in inventory slot 0
2. WHEN a Player Entity spawns THEN the Game System SHALL set the Player Entity money balance to 50,000 dollars
3. WHEN a Player Entity spawns THEN the Game System SHALL initialize inventory slots 1, 2, and 3 as empty
4. WHEN a Player Entity with a USP weapon is active THEN the Game System SHALL set the Player Entity movement speed to 2.5 pixels per second
5. WHEN the HUD renders THEN the Game System SHALL display the Player Entity money balance below the health indicator in the top-left corner

### Requirement 2: Shop Generation and Placement

**User Story:** As a game designer, I want shops to be randomly distributed across the map, so that players have strategic locations to acquire weapons.

#### Acceptance Criteria

1. WHEN the map generates THEN the Game System SHALL create exactly 26 Shop Entities at random non-overlapping Grid Cells
2. WHEN selecting a Grid Cell for a Shop Entity THEN the Game System SHALL ensure no other Shop Entity occupies that Grid Cell
3. WHEN placing Shop Entities THEN the Game System SHALL ensure no Shop Entity is within 5 Grid Cells of any Spawn Point
4. WHEN Shop Entity placement completes THEN the Game System SHALL verify map connectivity using breadth-first search
5. IF map connectivity verification fails THEN the Game System SHALL regenerate all Shop Entity positions
6. WHEN rendering a Shop Entity THEN the Game System SHALL display a red square of 20×20 pixels at the Grid Cell center coordinates (x + 40, y + 40)

### Requirement 3: Shop Interaction

**User Story:** As a player, I want to interact with shops to purchase weapons, so that I can upgrade my arsenal.

#### Acceptance Criteria

1. WHEN a Player Entity is within 60 pixels of a Shop Entity THEN the Game System SHALL display the prompt "Нажмите B для покупки"
2. WHEN a Player Entity presses the B key within 60 pixels of a Shop Entity THEN the Game System SHALL open the shop purchase interface
3. WHEN the shop interface renders THEN the Game System SHALL display three categories: "Пистолеты", "Автоматы", "Снайперки" in separate columns
4. WHEN displaying a Weapon Instance in the shop THEN the Game System SHALL show the weapon name, price, damage value, and magazine size
5. WHEN displaying a Weapon Instance purchase status THEN the Game System SHALL indicate whether the weapon is purchasable, money is insufficient, or inventory is full

### Requirement 4: Weapon Purchase Mechanics

**User Story:** As a player, I want to purchase weapons when I have sufficient funds and inventory space, so that I can expand my combat options.

#### Acceptance Criteria

1. WHEN a Player Entity attempts to purchase a Weapon Instance AND the Player Entity money balance is less than the weapon price THEN the Game System SHALL prevent the purchase and display "Недостаточно денег. Требуется: [сумма]"
2. WHEN a Player Entity attempts to purchase a Weapon Instance AND all Inventory Slots are occupied THEN the Game System SHALL prevent the purchase and display "Инвентарь заполнен. Освободите ячейку для покупки."
3. WHEN a Player Entity successfully purchases a Weapon Instance THEN the Game System SHALL deduct the weapon price from the Player Entity money balance
4. WHEN a Player Entity successfully purchases a Weapon Instance THEN the Game System SHALL add the Weapon Instance to the first empty Inventory Slot
5. WHEN a Weapon Instance is added to inventory THEN the Game System SHALL initialize the weapon with full magazine and reserve ammunition

### Requirement 5: Weapon Inventory System

**User Story:** As a player, I want to select weapons from my inventory, so that I can switch between different combat tools.

#### Acceptance Criteria

1. WHEN a Player Entity presses keys 1, 2, 3, or 4 THEN the Game System SHALL activate the Weapon Instance in the corresponding Inventory Slot
2. WHEN a Player Entity activates a non-empty Inventory Slot THEN the Game System SHALL set that Weapon Instance as the Active Weapon
3. WHEN a Player Entity activates an empty Inventory Slot THEN the Game System SHALL set the Active Weapon to null and restore movement speed to 3.0 pixels per second
4. WHEN an Active Weapon is set THEN the Game System SHALL apply the weapon's movement speed modifier to the Player Entity
5. WHEN the HUD renders with an Active Weapon THEN the Game System SHALL display the weapon name and ammunition count in format "[Name]: [current]/[reserve]" in the top-right corner
6. WHEN the HUD renders without an Active Weapon THEN the Game System SHALL display "Без оружия" in the top-right corner

### Requirement 6: Shooting Mechanics

**User Story:** As a player, I want to fire my weapon at enemies, so that I can engage in combat.

#### Acceptance Criteria

1. WHEN a Player Entity presses the left mouse button AND an Active Weapon exists AND the magazine is not empty THEN the Game System SHALL create a Bullet Entity traveling toward the cursor position
2. WHEN a Player Entity presses the left mouse button AND the Active Weapon magazine is empty THEN the Game System SHALL initiate automatic reload
3. WHEN a Player Entity presses the R key AND an Active Weapon exists AND reserve ammunition is greater than zero THEN the Game System SHALL initiate manual reload
4. WHEN a reload initiates THEN the Game System SHALL prevent firing for the duration specified by the weapon's reload time
5. WHEN a reload completes THEN the Game System SHALL transfer ammunition from reserve to magazine up to the magazine capacity

### Requirement 7: Bullet Behavior and Visualization

**User Story:** As a player, I want to see bullets traveling across the map, so that I can track my shots and understand combat feedback.

#### Acceptance Criteria

1. WHEN a Bullet Entity is created THEN the Game System SHALL render it as a white line with length 5 pixels and width 2 pixels
2. WHEN a Bullet Entity updates THEN the Game System SHALL move it at the speed specified by the firing weapon
3. WHEN a Bullet Entity collides with any wall type THEN the Game System SHALL remove the Bullet Entity
4. WHEN a Bullet Entity collides with a Player Entity THEN the Game System SHALL remove the Bullet Entity and apply damage
5. WHEN a Bullet Entity travels beyond its weapon's effective range THEN the Game System SHALL remove the Bullet Entity
6. WHEN a Bullet Entity is created THEN the Game System SHALL synchronize it to all connected clients via UDP Packet

### Requirement 8: Damage System

**User Story:** As a player, I want to deal and receive damage from weapons, so that combat has meaningful consequences.

#### Acceptance Criteria

1. WHEN a Bullet Entity hits a Player Entity THEN the Game System SHALL reduce the target Player Entity health by the weapon damage value
2. WHEN a Player Entity receives damage THEN the Game System SHALL display a floating red text showing "-[damage]" above the Player Entity for 1 second
3. WHEN a Player Entity health reaches zero THEN the Game System SHALL respawn the Player Entity at a random Spawn Point after 5 seconds
4. WHEN a Player Entity eliminates another Player Entity THEN the Game System SHALL add 5,000 dollars to the eliminating Player Entity money balance
5. WHEN a Player Entity respawns THEN the Game System SHALL restore the Player Entity health to 100 HP

### Requirement 9: Network Synchronization

**User Story:** As a multiplayer game system, I want to synchronize shooting events across all clients, so that all players see consistent game state.

#### Acceptance Criteria

1. WHEN a Player Entity fires a weapon THEN the Game System SHALL send a UDP Packet containing player ID, bullet start position, bullet end position, weapon type, hit status, hit player ID, and damage value
2. WHEN the server receives a shot UDP Packet THEN the Game System SHALL validate the shot and broadcast the result to all connected clients
3. WHEN a client receives a shot UDP Packet THEN the Game System SHALL create a visual Bullet Entity based on the packet data
4. WHEN the server detects a bullet hit THEN the Game System SHALL authoritatively apply damage to the target Player Entity
5. WHEN a Player Entity purchases a weapon THEN the Game System SHALL synchronize the inventory change to all clients

### Requirement 10: Performance Optimization

**User Story:** As a game system, I want to optimize bullet and collision calculations, so that the game maintains smooth performance.

#### Acceptance Criteria

1. WHEN counting active Bullet Entities for a Player Entity THEN the Game System SHALL enforce a maximum limit of 20 simultaneous bullets
2. WHEN a Bullet Entity moves outside the visible screen area plus 20 percent buffer THEN the Game System SHALL remove the Bullet Entity
3. WHEN a Bullet Entity position exceeds map boundaries (0-5100 pixels) THEN the Game System SHALL remove the Bullet Entity
4. WHEN performing bullet collision detection THEN the Game System SHALL use a quadtree spatial structure with 100×100 pixel cells
5. WHEN the Fog of War system renders Shop Entities THEN the Game System SHALL apply the same visibility rules as other map entities

### Requirement 11: Weapon Catalog

**User Story:** As a game designer, I want a diverse weapon catalog with distinct characteristics, so that players have meaningful tactical choices.

#### Acceptance Criteria

1. WHEN the Game System initializes THEN the Game System SHALL define 10 weapon types: USP, Glock-18, Five-SeveN, R8 Revolver, Galil AR, M4, AK-47, M10, AWP, M40
2. WHEN a Weapon Instance is created THEN the Game System SHALL assign properties including magazine size, damage, range, bullet speed, reload time, and movement speed modifier
3. WHEN displaying pistol category weapons THEN the Game System SHALL include USP, Glock-18, Five-SeveN, and R8 Revolver with prices 0, 1000, 2500, and 4250 dollars respectively
4. WHEN displaying rifle category weapons THEN the Game System SHALL include Galil AR, M4, and AK-47 with prices 10000, 15000, and 17500 dollars respectively
5. WHEN displaying sniper category weapons THEN the Game System SHALL include M10, AWP, and M40 with prices 20000, 25000, and 22000 dollars respectively

### Requirement 12: System Architecture

**User Story:** As a developer, I want to maintain the existing two-file architecture, so that the codebase remains simple and manageable.

#### Acceptance Criteria

1. WHEN implementing weapon system features THEN the Game System SHALL add all server-side logic to the existing Zero_Ground.cpp file
2. WHEN implementing weapon system features THEN the Game System SHALL add all client-side logic to the existing Zero_Ground_client.cpp file
3. WHEN implementing weapon system features THEN the Game System SHALL NOT introduce additional source files or external dependencies beyond SFML
4. WHEN implementing data structures THEN the Game System SHALL define them inline within the respective cpp files
5. WHEN implementing network protocols THEN the Game System SHALL use the existing UDP socket infrastructure on port 53001
