# Implementation Plan

- [ ] 1. Define core weapon and shop data structures
  - Add Weapon struct with all 10 weapon types and their properties to both server and client files
  - Add Shop struct with grid position and world coordinates
  - Add Bullet struct with position, velocity, damage, and range tracking
  - Extend Player struct with inventory array (4 slots), activeSlot, and money fields
  - _Requirements: 1.1, 11.1, 11.2_

- [ ] 1.1 Write property test for weapon initialization
  - **Property 33: Weapon property completeness**
  - **Validates: Requirements 11.2**

- [ ] 2. Implement shop generation algorithm on server
  - Write function to generate 26 random non-overlapping shop positions on 51×51 grid
  - Implement spawn point distance validation (minimum 5 cells)
  - Implement BFS connectivity verification
  - Add fallback shop placement pattern if generation fails after 100 attempts
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5_

- [ ] 2.1 Write property test for shop generation
  - **Property 2: Shop count invariant**
  - **Property 3: Shop position uniqueness**
  - **Property 4: Shop spawn distance constraint**
  - **Validates: Requirements 2.1, 2.2, 2.3**

- [ ] 3. Implement player spawn initialization
  - Modify player spawn logic to equip USP in slot 0
  - Initialize slots 1-3 as empty (nullptr)
  - Set initial money balance to 50,000
  - _Requirements: 1.1, 1.2, 1.3_

- [ ] 3.1 Write property test for player spawn state
  - **Property 1: Player spawn initialization**
  - **Validates: Requirements 1.1, 1.2, 1.3**

- [ ] 4. Implement shop rendering on client
  - Render each shop as 20×20 red square at grid cell center (x+40, y+40)
  - Implement shop visibility with fog of war integration
  - Display "Нажмите B для покупки" prompt when player within 60 pixels
  - _Requirements: 2.6, 3.1, 10.5_

- [ ] 4.1 Write property test for shop interaction range
  - **Property 5: Shop interaction range**
  - **Property 32: Fog of war consistency for shops**
  - **Validates: Requirements 3.1, 10.5**

- [ ] 5. Implement shop UI interface on client
  - Create shop menu with three columns: "Пистолеты", "Автоматы", "Снайперки"
  - Display weapon name, price, damage, and magazine size for each weapon
  - Show purchase status (purchasable/insufficient funds/inventory full)
  - Handle B key press to open/close shop when in range
  - _Requirements: 3.2, 3.3, 3.4, 3.5_

- [ ] 5.1 Write property test for purchase status calculation
  - **Property 6: Purchase status calculation**
  - **Validates: Requirements 3.5**

- [ ] 6. Implement purchase validation and transaction logic on server
  - Validate player has sufficient money for weapon price
  - Validate player has empty inventory slot
  - Deduct weapon price from player money on successful purchase
  - Add weapon to first empty inventory slot
  - Initialize weapon with full magazine and reserve ammo
  - Send inventory update packet to all clients
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_

- [ ] 6.1 Write property tests for purchase mechanics
  - **Property 7: Insufficient funds prevents purchase**
  - **Property 8: Full inventory prevents purchase**
  - **Property 9: Purchase money deduction**
  - **Property 10: Weapon placement in first empty slot**
  - **Property 11: Purchased weapon initialization**
  - **Validates: Requirements 4.1, 4.2, 4.3, 4.4, 4.5**

- [ ] 7. Implement inventory management and weapon switching
  - Handle keys 1-4 to activate corresponding inventory slots (0-3)
  - Set active weapon when non-empty slot selected
  - Clear active weapon and restore speed to 3.0 when empty slot selected
  - Apply weapon movement speed modifier when weapon is active
  - _Requirements: 5.1, 5.2, 5.3, 5.4_

- [ ] 7.1 Write property tests for inventory system
  - **Property 12: Inventory slot activation**
  - **Property 13: Non-empty slot sets active weapon**
  - **Property 14: Empty slot clears weapon and restores speed**
  - **Property 15: Weapon speed modification**
  - **Validates: Requirements 5.1, 5.2, 5.3, 5.4**

- [ ] 8. Implement HUD updates for weapon system
  - Display money balance below health in top-left corner
  - Display weapon name and ammo count "[Name]: [current]/[reserve]" in top-right when weapon active
  - Display "Без оружия" in top-right when no weapon active
  - _Requirements: 1.5, 5.5, 5.6_

- [ ] 9. Implement shooting mechanics on client
  - Handle left mouse button click to fire weapon
  - Calculate bullet trajectory from player position toward cursor
  - Check magazine ammo > 0 before firing
  - Trigger automatic reload when magazine empty
  - Handle R key for manual reload
  - Prevent firing during reload
  - Send shot packet to server with bullet data
  - _Requirements: 6.1, 6.2, 6.3, 6.4_

- [ ] 9.1 Write property tests for shooting mechanics
  - **Property 16: Bullet creation on valid shot**
  - **Property 17: Empty magazine triggers reload**
  - **Property 18: Manual reload initiation**
  - **Property 19: Reload prevents firing**
  - **Validates: Requirements 6.1, 6.2, 6.3, 6.4**

- [ ] 10. Implement reload mechanics
  - Track reload state and timer for each weapon
  - Transfer ammo from reserve to magazine on reload completion
  - Handle case where reserve < magazine capacity
  - Update HUD ammo display during reload
  - _Requirements: 6.5_

- [ ] 10.1 Write property test for reload ammo transfer
  - **Property 20: Reload ammo transfer**
  - **Validates: Requirements 6.5**

- [ ] 11. Implement bullet physics and rendering on client
  - Create bullet entity on fire with position, velocity, and damage
  - Update bullet position each frame based on velocity and deltaTime
  - Render bullets as white lines (5px length, 2px width)
  - Remove bullets that exceed weapon effective range
  - Remove bullets outside screen area + 20% buffer
  - Remove bullets outside map boundaries (0-5100px)
  - Enforce maximum 20 active bullets per player
  - _Requirements: 7.1, 7.2, 7.5, 10.1, 10.2, 10.3_

- [ ] 11.1 Write property tests for bullet behavior
  - **Property 21: Bullet velocity matches weapon**
  - **Property 24: Range limit removes bullet**
  - **Property 29: Bullet count limit**
  - **Property 30: Screen culling removes bullets**
  - **Property 31: Map boundary removes bullets**
  - **Validates: Requirements 7.2, 7.5, 10.1, 10.2, 10.3**

- [ ] 12. Implement bullet collision detection on server
  - Implement quadtree spatial structure with 100×100px cells
  - Check bullet-wall collisions and remove bullets on hit
  - Check bullet-player collisions and remove bullets on hit
  - Send hit packet to all clients when bullet hits player
  - _Requirements: 7.3, 7.4, 10.4_

- [ ] 12.1 Write property tests for bullet collision
  - **Property 22: Wall collision removes bullet**
  - **Property 23: Player collision removes bullet and applies damage**
  - **Validates: Requirements 7.3, 7.4**

- [ ] 13. Implement damage system on server
  - Apply weapon damage to player health on bullet hit
  - Check for player death (health <= 0)
  - Award $5000 to eliminating player on kill
  - Schedule respawn after 5 seconds
  - Restore health to 100 HP on respawn
  - Broadcast damage and death events to all clients
  - _Requirements: 8.1, 8.3, 8.4, 8.5_

- [ ] 13.1 Write property tests for damage system
  - **Property 25: Damage reduces health**
  - **Property 26: Death triggers respawn**
  - **Property 27: Kill reward**
  - **Property 28: Respawn health restoration**
  - **Validates: Requirements 8.1, 8.3, 8.4, 8.5**

- [ ] 14. Implement damage visualization on client
  - Display floating red text "-[damage]" above hit player
  - Animate text moving upward for 1 second
  - Use font size 24px in red color
  - _Requirements: 8.2_

- [ ] 15. Implement network synchronization
  - Define ShotPacket structure with player ID, position, direction, weapon type
  - Define HitPacket structure with shooter ID, victim ID, damage, hit position
  - Define PurchasePacket structure with player ID and weapon type
  - Define InventoryPacket structure with player ID, slot, weapon type, money
  - Send shop positions to clients on map generation
  - Broadcast shot events to all clients
  - Broadcast hit events to all clients
  - Synchronize inventory changes to all clients
  - _Requirements: 7.6, 9.1, 9.2, 9.3, 9.4, 9.5_

- [ ] 16. Implement server-side shot validation
  - Validate player has active weapon
  - Validate weapon has ammo > 0
  - Validate shot cooldown has elapsed
  - Perform authoritative raycast for hit detection
  - Ignore invalid shots without broadcasting
  - _Requirements: 9.2, 9.4_

- [ ] 17. Checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 18. Write integration tests for client-server interaction
  - Test purchase flow: client request → server validation → inventory update
  - Test combat flow: client fires → server validates → damage applied → clients notified
  - Test shop synchronization: server generates → clients receive positions

- [ ] 19. Write performance tests
  - Test bullet stress: 10 players firing continuously, measure frame time with 200 bullets
  - Test shop rendering: 26 shops visible, measure rendering time
  - Test collision detection: 100 bullets + 50 players, measure collision time
  - Target: maintain 60 FPS (< 16.67ms per frame)
