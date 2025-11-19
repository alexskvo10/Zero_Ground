# Implementation Plan

- [ ] 1. Set up project structure and dependencies







  - Create separate directories for server and client code
  - Configure Visual Studio 2022 projects with SFML 2.6.1
  - Add Google Test framework for unit testing
  - Add RapidCheck library for property-based testing
  - Set up common shared code directory for network protocols and data structures
  - _Requirements: All_

- [ ] 2. Implement core data structures and network protocol
  - Define Position, Player, Wall, GameMap structures
  - Implement network packet structures (ConnectPacket, ReadyPacket, StartPacket, MapDataPacket, PositionPacket)
  - Create PacketValidator class with validation methods
  - _Requirements: 4.6, 5.5, 10.3, 10.4_

- [ ] 2.1 Write property test for packet validation
  - **Property 20: Position Validation Bounds**
  - **Validates: Requirements 5.5, 10.3, 10.4**

- [ ] 2.2 Write property test for invalid coordinate rejection
  - **Property 27: Invalid Coordinate Rejection**
  - **Validates: Requirements 10.5**

- [ ] 3. Implement map generation system
  - Create MapGenerator class with wall generation logic
  - Implement random wall placement with size constraints (2-8 units)
  - Calculate and enforce 25-35% coverage constraint
  - Implement BFS pathfinding validation between spawn points
  - Add retry logic (maximum 10 attempts)
  - Handle generation failure with error message
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6_

- [ ] 3.1 Write property test for wall dimensions
  - **Property 1: Wall Dimensions Validity**
  - **Validates: Requirements 1.2**

- [ ] 3.2 Write property test for map coverage
  - **Property 2: Map Coverage Constraint**
  - **Validates: Requirements 1.3**

- [ ] 3.3 Write property test for spawn connectivity
  - **Property 3: Spawn Point Connectivity**
  - **Validates: Requirements 1.4**

- [ ] 3.4 Write unit tests for map generation edge cases
  - Test generation failure after 10 attempts
  - Test spawn point overlap prevention
  - _Requirements: 1.5, 1.6_

- [ ] 4. Implement quadtree spatial partitioning
  - Create Quadtree class with insert and query methods
  - Implement recursive subdivision (max depth 5, max 10 walls per node)
  - Build quadtree from generated map walls
  - _Requirements: 11.3_

- [ ] 4.1 Write unit test for quadtree construction
  - Test that quadtree correctly indexes all walls
  - _Requirements: 11.3_

- [ ] 5. Implement collision detection system
  - Create CollisionSystem class with circle-rectangle collision detection
  - Implement collision resolution with 1-unit pushback
  - Use quadtree for efficient nearby wall queries
  - Add boundary clamping to keep players within [0, 1000] range
  - _Requirements: 2.4, 2.5, 2.6_

- [ ] 5.1 Write property test for collision stops movement
  - **Property 7: Collision Stops Movement**
  - **Validates: Requirements 2.4**

- [ ] 5.2 Write property test for collision pushback
  - **Property 8: Collision Pushback Distance**
  - **Validates: Requirements 2.5**

- [ ] 5.3 Write property test for boundary clamping
  - **Property 9: Position Boundary Clamping**
  - **Validates: Requirements 2.6**

- [ ] 5.4 Write unit tests for collision edge cases
  - Test multiple simultaneous collisions
  - Test corner collisions
  - _Requirements: 2.4, 2.5_

- [ ] 6. Implement thread-safe game state manager
  - Create GameState class with mutex-protected player map
  - Implement updatePlayerPosition with mutex lock
  - Implement getPlayer and getPlayersInRadius with mutex lock
  - Add setPlayerReady and allPlayersReady methods
  - _Requirements: 10.1, 10.2_

- [ ] 6.1 Write property test for thread-safe position access
  - **Property 24: Thread-Safe Position Access**
  - **Validates: Requirements 10.1**

- [ ] 6.2 Write property test for thread-safe map access
  - **Property 25: Thread-Safe Map Access**
  - **Validates: Requirements 10.2**

- [ ] 7. Implement player movement and interpolation
  - Add movement speed constant (5 units/second)
  - Implement WASD input handling with delta time
  - Store previous position for interpolation
  - Implement lerp function for smooth position interpolation
  - _Requirements: 2.1, 11.4_

- [ ] 7.1 Write property test for movement speed
  - **Property 4: Movement Speed Consistency**
  - **Validates: Requirements 2.1**

- [ ] 7.2 Write property test for linear interpolation
  - **Property 28: Linear Interpolation Bounds**
  - **Validates: Requirements 11.4**

- [ ] 8. Implement server TCP listener and connection handling
  - Create TCP listener on port 53000
  - Handle client connection requests
  - Receive and validate ConnectPacket
  - Send map data (MapDataPacket) to connected clients
  - Send initial player positions to connected clients
  - Update server UI to show "Player connected but not ready"
  - _Requirements: 4.1, 4.3, 4.4, 6.6, 8.1, 8.2_

- [ ] 8.1 Write property test for map data transmission
  - **Property 13: Map Data Transmission Completeness**
  - **Validates: Requirements 4.3**

- [ ] 8.2 Write property test for player position transmission
  - **Property 14: Player Position Transmission**
  - **Validates: Requirements 4.4**

- [ ] 8.3 Write property test for connection confirmation protocol
  - **Property 21: Connection Confirmation Protocol**
  - **Validates: Requirements 8.1**

- [ ] 9. Implement ready protocol on server
  - Receive ReadyPacket from clients via TCP
  - Update player ready status in GameState
  - Update server UI to show "Player connected and ready to play"
  - Display green PLAY button when player is ready
  - Send StartPacket to all clients when PLAY is clicked
  - Transition server to game screen
  - _Requirements: 6.7, 6.8, 6.9, 8.4, 8.5, 8.7_

- [ ] 9.1 Write property test for ready status protocol
  - **Property 22: Ready Status Protocol**
  - **Validates: Requirements 8.3**

- [ ] 9.2 Write property test for game start broadcast
  - **Property 23: Game Start Broadcast**
  - **Validates: Requirements 8.5**

- [ ] 10. Implement server UDP position synchronization
  - Create UDP socket on port 53001
  - Run UDP listener in separate thread
  - Receive position updates from clients at 20Hz
  - Validate received positions using PacketValidator
  - Update GameState with validated positions
  - Send server position to clients
  - Implement network culling (only send players within 50 units)
  - _Requirements: 4.2, 4.5, 4.7, 4.8, 5.4_

- [ ] 10.1 Write property test for UDP packet rate
  - **Property 15: UDP Packet Rate**
  - **Validates: Requirements 4.5**

- [ ] 10.2 Write property test for network packet structure
  - **Property 16: Network Packet Structure**
  - **Validates: Requirements 4.6**

- [ ] 10.3 Write property test for network culling inclusion
  - **Property 17: Network Culling Inclusion**
  - **Validates: Requirements 4.7**

- [ ] 10.4 Write property test for network culling exclusion
  - **Property 18: Network Culling Exclusion**
  - **Validates: Requirements 4.8**

- [ ] 10.5 Write property test for invalid packet discarding
  - **Property 19: Invalid Packet Discarding**
  - **Validates: Requirements 5.4**

- [ ] 11. Implement server UI start screen
  - Create fullscreen window with SFML
  - Display "SERVER STARTED" text in green (64pt)
  - Get and display local IP address (handle 127.0.0.1/0.0.0.0 cases)
  - Display "Waiting for player..." text initially
  - Implement Escape key to toggle fullscreen/windowed mode
  - Center all UI elements on window resize
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.10_

- [ ] 12. Implement server game rendering
  - Render server player as green circle (30px radius)
  - Render connected clients as blue circles (20px radius)
  - Render all walls as gray rectangles
  - Use interpolated positions for smooth movement
  - Implement input isolation (server only controls green circle)
  - _Requirements: 2.2_

- [ ] 12.1 Write property test for server input isolation
  - **Property 5: Server Input Isolation**
  - **Validates: Requirements 2.2**

- [ ] 13. Checkpoint - Ensure server tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 14. Implement client TCP connection and handshake
  - Create TCP socket for connection to server
  - Send ConnectPacket on successful connection
  - Receive and parse MapDataPacket
  - Receive initial player positions
  - Display "Connection established" on success
  - Display error message on failure (3 second timeout)
  - _Requirements: 7.9, 7.10, 7.11, 8.1, 8.6_

- [ ] 15. Implement client ready system
  - Display green READY button (200x60px) after connection
  - Send ReadyPacket when button is clicked
  - Display "Waiting for game start..." after sending ready
  - Receive StartPacket from server
  - Transition to game screen on start signal
  - _Requirements: 7.12, 7.13, 7.14, 7.15, 8.3, 8.6_

- [ ] 16. Implement client UDP position synchronization
  - Create UDP socket on port 53002
  - Run UDP sender in separate thread
  - Send local player position at 20Hz
  - Receive other player positions from server
  - Update local state with received positions
  - Handle connection loss (2 second timeout)
  - _Requirements: 4.5, 5.1_

- [ ] 17. Implement client UI connection screen
  - Create fullscreen window with SFML
  - Display "SERVER IP ADDRESS:" label (32pt)
  - Create IP input field (400x50px, dark gray background)
  - Implement input validation (only digits and dots, max 15 chars)
  - Handle Backspace key correctly
  - Highlight input field when focused (green outline)
  - Display "CONNECT TO SERVER" button (300x70px)
  - Handle Enter key to trigger connection
  - Display error message in red on connection failure
  - Implement Escape key to toggle fullscreen/windowed mode
  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.8, 7.9, 7.10, 7.16_

- [ ] 18. Implement fog of war rendering system
  - Create FogOfWarRenderer class
  - Calculate visibility radius (25 units) around player
  - Render all walls (dimmed outside radius, normal inside)
  - Render only players within 25 units
  - Apply black semi-transparent overlay (alpha 200) outside radius
  - Use shader for circular fog cutout (optional enhancement)
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6_

- [ ] 18.1 Write property test for visibility radius calculation
  - **Property 10: Visibility Radius Calculation**
  - **Validates: Requirements 3.1, 3.3, 3.4**

- [ ] 18.2 Write property test for wall rendering completeness
  - **Property 11: Wall Rendering Completeness**
  - **Validates: Requirements 3.5**

- [ ] 18.3 Write property test for wall darkening
  - **Property 12: Wall Darkening by Distance**
  - **Validates: Requirements 3.6**

- [ ] 19. Implement client game rendering
  - Render local player as blue circle (30px radius)
  - Render server player as green circle (20px radius) if visible
  - Render walls with fog of war effects
  - Use interpolated positions for smooth movement
  - Implement input isolation (client only controls blue circle)
  - _Requirements: 2.3_

- [ ] 19.1 Write property test for client input isolation
  - **Property 6: Client Input Isolation**
  - **Validates: Requirements 2.3**

- [ ] 20. Implement HUD and visual feedback
  - Display health bar above player (green rectangle, 100 HP)
  - Update health bar width based on current health
  - Display "Score: [number]" in top-left corner
  - Apply full-screen darkening on player death
  - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5_

- [ ] 21. Implement reconnection system
  - Detect connection loss (no packets for 2 seconds)
  - Display "Connection lost. Press R to reconnect to [IP]" screen
  - Implement R key handler to retry connection
  - Restore game state on successful reconnection
  - _Requirements: 5.1, 5.2_

- [ ] 22. Implement performance monitoring
  - Create PerformanceMonitor class
  - Track frame rate over 1-second windows
  - Log warning when FPS drops below 55
  - Log additional metrics (player count, wall count, CPU usage)
  - _Requirements: 11.5_

- [ ] 23. Implement error handling and logging
  - Create ErrorHandler class
  - Handle invalid packet errors (log and discard)
  - Handle connection lost errors (show reconnect screen)
  - Handle map generation failure (display error and exit)
  - Add logging for all network errors
  - _Requirements: 5.3, 5.4, 1.6_

- [ ] 24. Final checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 25. Integration testing
  - Test full server-client handshake sequence
  - Test ready protocol flow (connect → ready → start)
  - Test position synchronization during gameplay
  - Test collision detection with multiple players
  - Test fog of war visibility with moving players
  - Test reconnection after connection loss
  - Test fullscreen toggle on both server and client

- [ ] 26. Performance testing and optimization
  - Benchmark map generation time (target < 100ms)
  - Benchmark collision detection (target < 1ms per frame)
  - Profile CPU usage with 2 players (target < 40%)
  - Measure network bandwidth usage
  - Verify 55+ FPS on target hardware (Intel i3-8100, 8GB RAM)
  - Optimize quadtree queries if needed
  - Optimize rendering if needed

- [ ] 27. Code cleanup and documentation
  - Add code comments explaining complex algorithms (BFS, quadtree)
  - Document network protocol in README.md
  - Add build instructions for Visual Studio 2022
  - Document SFML 2.6.1 setup requirements
  - Add usage instructions (how to run server and client)
  - Document known limitations and future improvements
