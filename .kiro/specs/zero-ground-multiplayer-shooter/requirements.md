# Requirements Document

## Introduction

Zero Ground is an online 2D top-down shooter with dynamic procedurally generated maps and fog of war mechanics. The game supports multiplayer gameplay over local network using client-server architecture with SFML 2.6.1 library. The server hosts the game session and controls one player (green circle), while clients connect to play as additional players (blue circles). The game features collision detection with randomly generated walls, visibility system based on distance, and real-time network synchronization.

## Glossary

- **Server**: The host application that manages game state, generates the map, and controls the green player character
- **Client**: The player application that connects to the Server and controls a blue player character
- **Map**: The 1000x1000 unit game world containing walls and player spawn points, scaled to screen resolution
- **Wall**: Rectangular obstacles that block player movement and are always visible but dimmed outside visibility radius
- **Fog of War**: Visual obscuration system that limits player visibility to a 25-unit radius around their character
- **Visibility Radius**: The 25-unit circular area around a player where other players and full details are visible
- **Network Packet**: UDP data structure containing player position, alive status, and frame ID for synchronization
- **BFS**: Breadth-First Search algorithm used to validate map connectivity between spawn points
- **Spawn Point**: Initial player position on the map (bottom-left for server, top-right for client)
- **HUD**: Heads-Up Display showing player health bar and kill score
- **Collision**: Physical interaction when a player contacts a wall, resulting in immediate stop and 1-unit pushback

## Requirements

### Requirement 1: Map Generation and Validation

**User Story:** As a server operator, I want the system to generate a valid playable map with walls and verified connectivity, so that all players can navigate the entire game world.

#### Acceptance Criteria

1. WHEN the Server starts THEN the System SHALL generate a map of 1000x1000 units scaled to the current screen resolution
2. WHEN generating walls THEN the System SHALL create random rectangular walls with width between 2-8 units and height between 2-8 units
3. WHEN calculating total wall coverage THEN the System SHALL ensure walls occupy between 25% and 35% of the total map area
4. WHEN validating map connectivity THEN the System SHALL use BFS algorithm to verify a path exists between server spawn point (bottom-left) and client spawn point (top-right)
5. IF no valid path exists between spawn points THEN the System SHALL regenerate the map with a maximum of 10 attempts
6. WHEN all generation attempts fail THEN the System SHALL display an error message and terminate gracefully

### Requirement 2: Player Movement and Collision

**User Story:** As a player, I want smooth character movement with proper collision detection, so that I can navigate the map without passing through walls.

#### Acceptance Criteria

1. WHEN a player presses movement keys (WASD) THEN the System SHALL update player position using float coordinates at 5 units per second
2. WHEN the Server player presses WASD keys THEN the System SHALL move only the green circle
3. WHEN a Client player presses WASD keys THEN the System SHALL move only that client's blue circle
4. WHEN a player collides with a wall THEN the System SHALL immediately stop the player movement
5. WHEN a collision occurs THEN the System SHALL push the player back by 1 unit to prevent wall clipping
6. WHEN a player reaches map boundaries THEN the System SHALL constrain the player position within valid map coordinates

### Requirement 3: Fog of War Visibility System

**User Story:** As a player, I want limited visibility around my character with fog of war, so that the game has strategic depth and exploration elements.

#### Acceptance Criteria

1. WHEN rendering the game view THEN the System SHALL display a 25-unit visibility radius around the player's character
2. WHEN rendering areas outside visibility radius THEN the System SHALL overlay a black semi-transparent rectangle with alpha value 200
3. WHEN another player is within 25 units THEN the System SHALL render that player fully visible
4. WHEN another player is beyond 25 units THEN the System SHALL not render that player
5. WHEN rendering walls THEN the System SHALL always display all walls on the map regardless of distance
6. WHEN rendering walls outside visibility radius THEN the System SHALL apply darkening effect to those walls

### Requirement 4: Network Architecture and Synchronization

**User Story:** As a system architect, I want reliable client-server network communication with proper state synchronization, so that all players see consistent game state.

#### Acceptance Criteria

1. WHEN the Server starts THEN the System SHALL bind TCP socket to port 53000 for initial connections
2. WHEN the Server starts THEN the System SHALL bind UDP socket to port 53001 for position updates
3. WHEN a Client connects THEN the System SHALL send complete map state including all wall coordinates via TCP
4. WHEN a Client connects THEN the System SHALL send initial positions of all players via TCP
5. WHEN synchronizing positions THEN the System SHALL send UDP packets at 20 packets per second
6. WHEN creating network packets THEN the System SHALL include player x coordinate, y coordinate, alive status, and frame ID
7. WHEN the Server detects a player within 50 units THEN the System SHALL include that player's data in network updates
8. WHEN the Server detects a player beyond 50 units THEN the System SHALL exclude that player's data from network updates to optimize bandwidth

### Requirement 5: Connection Management and Error Handling

**User Story:** As a player, I want clear feedback about connection status and ability to reconnect, so that I can recover from network issues.

#### Acceptance Criteria

1. WHEN a Client loses connection for more than 2 seconds THEN the System SHALL display "Connection lost. Press R to reconnect to [IP]" message
2. WHEN a Client presses R on the reconnection screen THEN the System SHALL attempt to reconnect to the last known server IP address
3. WHEN a Client attempts connection to an invalid IP THEN the System SHALL display "Server unavailable. Check IP and try again" in red text for 3 seconds
4. WHEN network packets fail validation THEN the System SHALL discard invalid packets and log the error
5. WHEN validating received position data THEN the System SHALL verify x and y coordinates are within map bounds (0-1000)

### Requirement 6: Server User Interface and Player Readiness

**User Story:** As a server operator, I want a clear interface showing server status, connection information, and player readiness state, so that I can coordinate game start with connected players.

#### Acceptance Criteria

1. WHEN the Server application starts THEN the System SHALL display a start screen in fullscreen mode
2. WHEN displaying the start screen THEN the System SHALL show "SERVER STARTED" text in green at 64pt font size
3. WHEN displaying the start screen THEN the System SHALL show "Server IP: [actual local IP]" text in white at 32pt font size
4. IF the local IP is 127.0.0.1 or 0.0.0.0 THEN the System SHALL display "IP not available" instead of the IP address
5. WHEN displaying the start screen THEN the System SHALL show "Waiting for player..." text in white at 28pt font size
6. WHEN a Client successfully connects THEN the System SHALL replace "Waiting for player..." with "Player connected but not ready" text in yellow at 28pt font size
7. WHEN the connected Client clicks the Ready button THEN the System SHALL replace the status text with "Player connected and ready to play" in green at 28pt font size
8. WHEN the player status shows ready THEN the System SHALL display a green "PLAY" button sized 200x60 pixels
9. WHEN the operator clicks the PLAY button THEN the System SHALL send game start signal to all clients and transition to the main game screen
10. WHEN the operator presses Escape key THEN the System SHALL toggle between fullscreen and windowed mode

### Requirement 7: Client User Interface and Ready System

**User Story:** As a client player, I want an intuitive connection interface with ready confirmation, so that I can coordinate game start with the server.

#### Acceptance Criteria

1. WHEN the Client application starts THEN the System SHALL display a connection screen in fullscreen mode
2. WHEN displaying the connection screen THEN the System SHALL show "SERVER IP ADDRESS:" label in white at 32pt font size
3. WHEN displaying the connection screen THEN the System SHALL show an input field sized 400x50 pixels with dark gray background
4. WHEN the input field is focused THEN the System SHALL change the outline color to green with 3-pixel thickness
5. WHEN the user types in the input field THEN the System SHALL accept only digits (0-9) and dots (.)
6. WHEN the user presses Backspace in the input field THEN the System SHALL remove the last character
7. WHEN the input field contains text THEN the System SHALL limit input to maximum 15 characters
8. WHEN displaying the connection screen THEN the System SHALL show a green "CONNECT TO SERVER" button sized 300x70 pixels
9. WHEN the user clicks the connect button or presses Enter THEN the System SHALL attempt TCP connection to the entered IP address on port 53000 with 3-second timeout
10. WHEN connection fails THEN the System SHALL display "SERVER UNAVAILABLE OR INVALID IP" in red text for 3 seconds
11. WHEN connection succeeds THEN the System SHALL display "Connection established" text in green at 32pt font size
12. WHEN connection succeeds THEN the System SHALL display a green "READY" button sized 200x60 pixels
13. WHEN the user clicks the READY button THEN the System SHALL send ready status to the Server
14. WHEN the ready status is sent THEN the System SHALL replace the READY button with "Waiting for game start..." text in yellow at 28pt font size
15. WHEN the Server sends game start signal THEN the System SHALL transition to the main game screen
16. WHEN the user presses Escape key THEN the System SHALL toggle between fullscreen and windowed mode

### Requirement 8: Player Ready Protocol

**User Story:** As a system architect, I want a reliable ready-check protocol between server and clients, so that the game starts only when all players are prepared.

#### Acceptance Criteria

1. WHEN a Client connects via TCP THEN the System SHALL send a connection confirmation packet to the Server
2. WHEN the Server receives connection confirmation THEN the System SHALL update the server UI to show player connected status
3. WHEN a Client clicks the READY button THEN the System SHALL send a ready status packet to the Server via TCP
4. WHEN the Server receives ready status THEN the System SHALL update the server UI to show player ready status
5. WHEN the Server operator clicks PLAY button THEN the System SHALL send game start signal to all connected clients via TCP
6. WHEN a Client receives game start signal THEN the System SHALL load the map data and transition to game screen
7. WHEN the Server sends game start signal THEN the System SHALL transition to game screen simultaneously with clients

### Requirement 9: Game HUD and Visual Feedback

**User Story:** As a player, I want to see my health status and game score, so that I can track my performance during gameplay.

#### Acceptance Criteria

1. WHEN rendering the game screen THEN the System SHALL display a health bar above the player character
2. WHEN the player has 100 HP THEN the System SHALL render the health bar as a green rectangle
3. WHEN the player health decreases THEN the System SHALL reduce the health bar width proportionally
4. WHEN rendering the game screen THEN the System SHALL display "Score: [number]" text in the top-left corner
5. WHEN a player dies THEN the System SHALL apply full-screen darkening effect

### Requirement 10: Thread Safety and Data Validation

**User Story:** As a system architect, I want thread-safe access to shared game state and validated network data, so that the application remains stable under concurrent operations.

#### Acceptance Criteria

1. WHEN accessing shared player positions THEN the System SHALL use std::mutex to protect concurrent read and write operations
2. WHEN accessing shared map data THEN the System SHALL use std::mutex to protect concurrent read and write operations
3. WHEN receiving network position data THEN the System SHALL validate x coordinate is between 0 and 1000
4. WHEN receiving network position data THEN the System SHALL validate y coordinate is between 0 and 1000
5. IF received coordinates are outside valid range THEN the System SHALL discard the packet and continue processing

### Requirement 11: Performance and Optimization

**User Story:** As a player, I want smooth gameplay with consistent frame rate, so that I have a responsive gaming experience.

#### Acceptance Criteria

1. WHEN running on a system with Intel i3-8100 and 8GB RAM THEN the System SHALL maintain minimum 55 FPS
2. WHEN two players are connected THEN the System SHALL use less than 40% CPU
3. WHEN detecting collisions THEN the System SHALL use spatial partitioning (quadtree) for optimization
4. WHEN rendering player movement THEN the System SHALL use linear interpolation between previous and target positions for smooth animation
5. WHEN the frame rate drops below 55 FPS THEN the System SHALL log performance metrics for debugging
