# TRACEABILITY DB

## COVERAGE ANALYSIS

Total requirements: 64
Coverage: 0

## TRACEABILITY

## DATA

### ACCEPTANCE CRITERIA (64 total)
- 1.1: WHEN a Player Entity spawns THEN the Game System SHALL equip the Player Entity with one USP weapon in inventory slot 0 (not covered)
- 1.2: WHEN a Player Entity spawns THEN the Game System SHALL set the Player Entity money balance to 50,000 dollars (not covered)
- 1.3: WHEN a Player Entity spawns THEN the Game System SHALL initialize inventory slots 1, 2, and 3 as empty (not covered)
- 1.4: WHEN a Player Entity with a USP weapon is active THEN the Game System SHALL set the Player Entity movement speed to 2.5 pixels per second (not covered)
- 1.5: WHEN the HUD renders THEN the Game System SHALL display the Player Entity money balance below the health indicator in the top-left corner (both host and remote clients) (not covered)
- 2.1: WHEN the map generates THEN the Game System SHALL create exactly 26 Shop Entities at random non-overlapping Grid Cells (not covered)
- 2.2: WHEN selecting a Grid Cell for a Shop Entity THEN the Game System SHALL ensure no other Shop Entity occupies that Grid Cell (not covered)
- 2.3: WHEN placing Shop Entities THEN the Game System SHALL ensure no Shop Entity is within 5 Grid Cells of any Spawn Point (not covered)
- 2.4: WHEN Shop Entity placement completes THEN the Game System SHALL verify map connectivity using breadth-first search (not covered)
- 2.5: IF map connectivity verification fails THEN the Game System SHALL regenerate all Shop Entity positions (not covered)
- 2.6: WHEN rendering a Shop Entity THEN the Game System SHALL display a red square of 20×20 pixels at the Grid Cell center coordinates (x + 40, y + 40) (not covered)
- 3.1: WHEN a Player Entity is within 60 pixels of a Shop Entity THEN the Game System SHALL display the prompt "Нажмите B для покупки" (not covered)
- 3.2: WHEN a Player Entity presses the B key within 60 pixels of a Shop Entity THEN the Game System SHALL open the shop purchase interface (not covered)
- 3.3: WHEN the shop interface renders THEN the Game System SHALL display three categories: "Пистолеты", "Автоматы", "Снайперки" in separate columns (not covered)
- 3.4: WHEN displaying a Weapon Instance in the shop THEN the Game System SHALL show the weapon name, price, damage value, and magazine size (not covered)
- 3.5: WHEN displaying a Weapon Instance purchase status THEN the Game System SHALL indicate whether the weapon is purchasable, money is insufficient, or inventory is full (not covered)
- 4.1: WHEN a Player Entity attempts to purchase a Weapon Instance AND the Player Entity money balance is less than the weapon price THEN the Game System SHALL prevent the purchase and display "Недостаточно денег. Требуется: [сумма]" (not covered)
- 4.2: WHEN a Player Entity attempts to purchase a Weapon Instance AND all Inventory Slots are occupied THEN the Game System SHALL prevent the purchase and display "Инвентарь заполнен. Освободите ячейку для покупки." (not covered)
- 4.3: WHEN a Player Entity successfully purchases a Weapon Instance THEN the Game System SHALL deduct the weapon price from the Player Entity money balance (not covered)
- 4.4: WHEN a Player Entity successfully purchases a Weapon Instance THEN the Game System SHALL add the Weapon Instance to the first empty Inventory Slot (not covered)
- 4.5: WHEN a Weapon Instance is added to inventory THEN the Game System SHALL initialize the weapon with full magazine and reserve ammunition (not covered)
- 5.1: WHEN a Player Entity presses keys 1, 2, 3, or 4 THEN the Game System SHALL activate the Weapon Instance in the corresponding Inventory Slot (not covered)
- 5.2: WHEN a Player Entity activates a non-empty Inventory Slot THEN the Game System SHALL set that Weapon Instance as the Active Weapon (not covered)
- 5.3: WHEN a Player Entity activates an empty Inventory Slot THEN the Game System SHALL set the Active Weapon to null and restore movement speed to 3.0 pixels per second (not covered)
- 5.4: WHEN an Active Weapon is set THEN the Game System SHALL apply the weapon's movement speed modifier to the Player Entity (not covered)
- 5.5: WHEN the HUD renders with an Active Weapon THEN the Game System SHALL display the weapon name and ammunition count in format "[Name]: [current]/[reserve]" in the top-right corner (not covered)
- 5.6: WHEN the HUD renders without an Active Weapon THEN the Game System SHALL display "Без оружия" in the top-right corner (not covered)
- 6.1: WHEN a Player Entity presses the left mouse button AND an Active Weapon exists AND the magazine is not empty THEN the Game System SHALL create a Bullet Entity traveling toward the cursor position (not covered)
- 6.2: WHEN a Player Entity presses the left mouse button AND the Active Weapon magazine is empty THEN the Game System SHALL initiate automatic reload (not covered)
- 6.3: WHEN a Player Entity presses the R key AND an Active Weapon exists AND reserve ammunition is greater than zero THEN the Game System SHALL initiate manual reload (not covered)
- 6.4: WHEN a reload initiates THEN the Game System SHALL prevent firing for the duration specified by the weapon's reload time (not covered)
- 6.5: WHEN a reload completes THEN the Game System SHALL transfer ammunition from reserve to magazine up to the magazine capacity (not covered)
- 7.1: WHEN a Bullet Entity is created THEN the Game System SHALL render it as a white line with length 5 pixels and width 2 pixels (not covered)
- 7.2: WHEN a Bullet Entity updates THEN the Game System SHALL move it at the speed specified by the firing weapon (not covered)
- 7.3: WHEN a Bullet Entity collides with any wall type THEN the Game System SHALL remove the Bullet Entity (not covered)
- 7.4: WHEN a Bullet Entity collides with a Player Entity THEN the Game System SHALL remove the Bullet Entity and apply damage (not covered)
- 7.5: WHEN a Bullet Entity travels beyond its weapon's effective range THEN the Game System SHALL remove the Bullet Entity (not covered)
- 7.6: WHEN a Bullet Entity is created THEN the Game System SHALL synchronize it to all connected clients via UDP Packet (not covered)
- 8.1: WHEN a Bullet Entity hits a Player Entity THEN the Game System SHALL reduce the target Player Entity health by the weapon damage value (not covered)
- 8.2: WHEN a Player Entity receives damage THEN the Game System SHALL display a floating red text showing "-[damage]" above the Player Entity for 1 second (not covered)
- 8.3: WHEN a Player Entity health reaches zero THEN the Game System SHALL respawn the Player Entity at a random Spawn Point after 5 seconds (not covered)
- 8.4: WHEN a Player Entity eliminates another Player Entity THEN the Game System SHALL add 5,000 dollars to the eliminating Player Entity money balance (not covered)
- 8.5: WHEN a Player Entity respawns THEN the Game System SHALL restore the Player Entity health to 100 HP (not covered)
- 9.1: WHEN a Player Entity fires a weapon THEN the Game System SHALL send a UDP Packet containing player ID, bullet start position, bullet end position, weapon type, hit status, hit player ID, and damage value (not covered)
- 9.2: WHEN the server receives a shot UDP Packet THEN the Game System SHALL validate the shot and broadcast the result to all connected clients (not covered)
- 9.3: WHEN a client receives a shot UDP Packet THEN the Game System SHALL create a visual Bullet Entity based on the packet data (not covered)
- 9.4: WHEN the server detects a bullet hit THEN the Game System SHALL authoritatively apply damage to the target Player Entity (not covered)
- 9.5: WHEN a Player Entity purchases a weapon THEN the Game System SHALL synchronize the inventory change to all clients (not covered)
- 10.1: WHEN counting active Bullet Entities for a Player Entity THEN the Game System SHALL enforce a maximum limit of 20 simultaneous bullets (not covered)
- 10.2: WHEN a Bullet Entity moves outside the visible screen area plus 20 percent buffer THEN the Game System SHALL remove the Bullet Entity (not covered)
- 10.3: WHEN a Bullet Entity position exceeds map boundaries (0-5100 pixels) THEN the Game System SHALL remove the Bullet Entity (not covered)
- 10.4: WHEN performing bullet collision detection THEN the Game System SHALL use a quadtree spatial structure with 100×100 pixel cells (not covered)
- 10.5: WHEN the Fog of War system renders Shop Entities THEN the Game System SHALL apply the same visibility rules as other map entities (not covered)
- 11.1: WHEN the Game System initializes THEN the Game System SHALL define 10 weapon types: USP, Glock-18, Five-SeveN, R8 Revolver, Galil AR, M4, AK-47, M10, AWP, M40 (not covered)
- 11.2: WHEN a Weapon Instance is created THEN the Game System SHALL assign properties including magazine size, damage, range, bullet speed, reload time, and movement speed modifier (not covered)
- 11.3: WHEN displaying pistol category weapons THEN the Game System SHALL include USP, Glock-18, Five-SeveN, and R8 Revolver with prices 0, 1000, 2500, and 4250 dollars respectively (not covered)
- 11.4: WHEN displaying rifle category weapons THEN the Game System SHALL include Galil AR, M4, and AK-47 with prices 10000, 15000, and 17500 dollars respectively (not covered)
- 11.5: WHEN displaying sniper category weapons THEN the Game System SHALL include M10, AWP, and M40 with prices 20000, 25000, and 22000 dollars respectively (not covered)
- 12.1: WHEN implementing weapon system features THEN the Game System SHALL add all server-side logic to the existing Zero_Ground.cpp file (not covered)
- 12.2: WHEN implementing weapon system features THEN the Game System SHALL add all client-side rendering and UI code to BOTH Zero_Ground.cpp and Zero_Ground_client.cpp files identically (not covered)
- 12.3: WHEN implementing weapon system features THEN the Game System SHALL NOT introduce additional source files or external dependencies beyond SFML (not covered)
- 12.4: WHEN implementing data structures THEN the Game System SHALL define them inline within the respective cpp files (not covered)
- 12.5: WHEN implementing network protocols THEN the Game System SHALL use the existing UDP socket infrastructure on port 53001 (not covered)
- 12.6: WHEN a host player runs Zero_Ground.cpp THEN the Game System SHALL provide the same visual experience (UI, HUD, bullets, effects) as remote players running Zero_Ground_client.cpp (not covered)

### IMPORTANT ACCEPTANCE CRITERIA (0 total)

### CORRECTNESS PROPERTIES (0 total)

### IMPLEMENTATION TASKS (31 total)
1. Определить основные структуры данных для оружия и магазина
1.1 Написать тест свойств для инициализации оружия
2. Реализовать алгоритм генерации магазинов на сервере
2.1 Написать тест свойств для генерации магазинов
3. Реализовать инициализацию спавна игрока
3.1 Написать тест свойств для состояния спавна игрока
4. Реализовать рендеринг магазинов в обоих клиентских файлах
4.1 Написать тест свойств для дальности взаимодействия с магазином
5. Реализовать интерфейс магазина в обоих клиентских файлах
5.1 Написать тест свойств для расчета статуса покупки
6. Реализовать проверку покупки и логику транзакций на сервере
6.1 Написать тесты свойств для механики покупки
7. Реализовать управление инвентарем и переключение оружия в обоих клиентских файлах
7.1 Написать тесты свойств для системы инвентаря
8. Реализовать обновления HUD для системы оружия в обоих клиентских файлах
9. Реализовать механику стрельбы в обоих клиентских файлах
9.1 Написать тесты свойств для механики стрельбы
10. Реализовать механику перезарядки в обоих клиентских файлах
10.1 Написать тест свойств для переноса патронов при перезарядке
11. Реализовать физику и рендеринг пуль в обоих клиентских файлах
11.1 Написать тесты свойств для поведения пуль
12. Реализовать обнаружение столкновений пуль на сервере
12.1 Написать тесты свойств для столкновений пуль
13. Реализовать систему урона на сервере
13.1 Написать тесты свойств для системы урона
14. Реализовать визуализацию урона в обоих клиентских файлах
15. Реализовать сетевую синхронизацию
16. Реализовать серверную проверку выстрелов
17. Контрольная точка - Убедиться, что все тесты проходят
18. Написать интеграционные тесты для взаимодействия клиент-сервер
19. Написать тесты производительности

### IMPLEMENTED PBTS (0 total)