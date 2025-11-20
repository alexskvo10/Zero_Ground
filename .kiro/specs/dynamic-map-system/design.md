# Документ дизайна: Система динамической карты 5000×5000

## Обзор

Система динамической карты заменяет существующую генерацию карты 500×500 единиц на масштабируемую архитектуру на основе ячеек размером 5000×5000 пикселей. Новая система использует вероятностную генерацию стен, динамическую камеру, следующую за игроком, и оптимизированный рендеринг только видимых областей. Архитектура интегрируется в существующие файлы Zero_Ground.cpp и Zero_Ground_client.cpp без создания дополнительных модулей.

Ключевые особенности дизайна:
- **Сетка ячеек 167×167** с размером ячейки 30 пикселей
- **Вероятностная генерация**: 60% - 1 стенка, 25% - 2 стенки, 15% - 0 стенок
- **Динамическая камера**: игрок всегда в центре экрана
- **Оптимизация рендеринга**: только видимые ячейки (радиус ~10 ячеек)
- **Кэширование**: пересчёт границ только при смене ячейки игрока
- **Простые AABB-коллизии**: без Quadtree для упрощения архитектуры

## Архитектура

### Структура данных ячейки

```cpp
// Структура ячейки (внутри main())
struct Cell {
    bool topWall = false;
    bool rightWall = false;
    bool bottomWall = false;
    bool leftWall = false;
};

// Константы системы
const float MAP_SIZE = 5000.0f;
const float CELL_SIZE = 30.0f;
const int GRID_SIZE = 167;  // 5000 / 30 ≈ 167
const float PLAYER_SIZE = 10.0f;
const float WALL_WIDTH = 12.0f;
const float WALL_LENGTH = 30.0f;

// Сетка карты
std::vector<std::vector<Cell>> grid(GRID_SIZE, std::vector<Cell>(GRID_SIZE));
```

**Важно о расположении стен:**
Стены располагаются на границах между ячейками и центрируются на этих границах. Стена шириной 12 пикселей будет заходить на 6 пикселей в одну ячейку и на 6 пикселей в соседнюю. Это означает:
- Ячейка с координатами (i,j) где (i+j)%2==1 может генерировать стены
- Эти стены физически будут частично находиться в соседних ячейках с (i+j)%2==0
- При рендеринге и коллизиях стены центрируются на границах: `position - WALL_WIDTH/2`

### Алгоритм вероятностной генерации

```cpp
void generateMap() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> probDist(0, 99);  // 0-99 для процентов
    std::uniform_int_distribution<int> sideDist(0, 3);   // 0-3 для сторон
    
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            // Проверка условия нечётной суммы
            if ((i + j) % 2 == 1) {
                int probability = probDist(gen);
                
                if (probability < 60) {
                    // 60% - одна стенка
                    int side = sideDist(gen);
                    setWall(grid[i][j], side);
                }
                else if (probability < 85) {
                    // 25% - две стенки
                    int side1 = sideDist(gen);
                    int side2 = sideDist(gen);
                    
                    // Убедиться, что стороны разные
                    while (side2 == side1) {
                        side2 = sideDist(gen);
                    }
                    
                    setWall(grid[i][j], side1);
                    setWall(grid[i][j], side2);
                }
                // 15% - ноль стенок (ничего не делаем)
            }
        }
    }
}

void setWall(Cell& cell, int side) {
    switch (side) {
        case 0: cell.topWall = true; break;
        case 1: cell.rightWall = true; break;
        case 2: cell.bottomWall = true; break;
        case 3: cell.leftWall = true; break;
    }
}
```

### BFS валидация проходимости

```cpp
bool isPathExists(sf::Vector2i start, sf::Vector2i end) {
    // BFS на основе сетки ячеек
    std::vector<std::vector<bool>> visited(GRID_SIZE, std::vector<bool>(GRID_SIZE, false));
    std::queue<sf::Vector2i> queue;
    
    sf::Vector2i startCell(start.x / CELL_SIZE, start.y / CELL_SIZE);
    sf::Vector2i endCell(end.x / CELL_SIZE, end.y / CELL_SIZE);
    
    queue.push(startCell);
    visited[startCell.x][startCell.y] = true;
    
    const int dx[] = {0, 1, 0, -1};
    const int dy[] = {1, 0, -1, 0};
    
    while (!queue.empty()) {
        sf::Vector2i current = queue.front();
        queue.pop();
        
        if (current == endCell) return true;
        
        for (int i = 0; i < 4; ++i) {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];
            
            if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE &&
                !visited[nx][ny] && canMove(current, sf::Vector2i(nx, ny))) {
                visited[nx][ny] = true;
                queue.push(sf::Vector2i(nx, ny));
            }
        }
    }
    
    return false;
}

bool canMove(sf::Vector2i from, sf::Vector2i to) {
    // Проверка, можно ли перейти из ячейки from в ячейку to
    int dx = to.x - from.x;
    int dy = to.y - from.y;
    
    if (dx == 1) return !grid[from.x][from.y].rightWall;  // Движение вправо
    if (dx == -1) return !grid[from.x][from.y].leftWall;  // Движение влево
    if (dy == 1) return !grid[from.x][from.y].bottomWall; // Движение вниз
    if (dy == -1) return !grid[from.x][from.y].topWall;   // Движение вверх
    
    return false;
}
```

### Генерация с повторными попытками

```cpp
bool generateValidMap() {
    const int MAX_ATTEMPTS = 10;
    
    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        // Очистить сетку
        for (int i = 0; i < GRID_SIZE; i++) {
            for (int j = 0; j < GRID_SIZE; j++) {
                grid[i][j] = Cell();
            }
        }
        
        // Сгенерировать карту
        generateMap();
        
        // Проверить проходимость
        sf::Vector2i serverSpawn(250, 4750);  // Нижний левый угол
        sf::Vector2i clientSpawn(4750, 250);  // Верхний правый угол
        
        if (isPathExists(serverSpawn, clientSpawn)) {
            std::cout << "Map generated successfully on attempt " << (attempt + 1) << std::endl;
            return true;
        }
        
        std::cout << "Attempt " << (attempt + 1) << " failed, regenerating..." << std::endl;
    }
    
    std::cerr << "ERROR: Failed to generate valid map after " << MAX_ATTEMPTS << " attempts" << std::endl;
    return false;
}
```

## Компоненты и интерфейсы

### Динамическая камера

```cpp
void updateCamera(sf::RenderWindow& window, sf::Vector2f playerPosition) {
    sf::View view;
    view.setSize(window.getSize().x, window.getSize().y);
    view.setCenter(playerPosition);
    
    // Ограничить камеру границами карты
    float halfWidth = window.getSize().x / 2.0f;
    float halfHeight = window.getSize().y / 2.0f;
    
    sf::Vector2f center = playerPosition;
    center.x = std::max(halfWidth, std::min(center.x, MAP_SIZE - halfWidth));
    center.y = std::max(halfHeight, std::min(center.y, MAP_SIZE - halfHeight));
    
    view.setCenter(center);
    window.setView(view);
}
```

### Оптимизированный рендеринг

```cpp
void renderVisibleWalls(sf::RenderWindow& window, sf::Vector2f playerPosition) {
    // Вычисление текущей ячейки игрока
    int playerCellX = static_cast<int>(playerPosition.x / CELL_SIZE);
    int playerCellY = static_cast<int>(playerPosition.y / CELL_SIZE);
    
    // Кэширование границ видимости
    static int lastPlayerCellX = -1;
    static int lastPlayerCellY = -1;
    static int startX, startY, endX, endY;
    
    if (playerCellX != lastPlayerCellX || playerCellY != lastPlayerCellY) {
        // Пересчитать границы только при смене ячейки
        startX = std::max(0, playerCellX - 10);
        startY = std::max(0, playerCellY - 10);
        endX = std::min(GRID_SIZE - 1, playerCellX + 10);
        endY = std::min(GRID_SIZE - 1, playerCellY + 10);
        
        lastPlayerCellX = playerCellX;
        lastPlayerCellY = playerCellY;
    }
    
    int wallCount = 0;
    
    // Отрисовка видимых стен
    for (int i = startX; i <= endX; i++) {
        for (int j = startY; j <= endY; j++) {
            float x = i * CELL_SIZE;
            float y = j * CELL_SIZE;
            
            // Отрисовка topWall (стена на верхней границе ячейки)
            // Стена центрируется на границе: 6 пикселей выше, 6 пикселей ниже
            if (grid[i][j].topWall) {
                sf::RectangleShape wall(sf::Vector2f(WALL_LENGTH, WALL_WIDTH));
                wall.setPosition(x, y - WALL_WIDTH / 2);  // Центрировать на границе
                wall.setFillColor(sf::Color(150, 150, 150));
                window.draw(wall);
                wallCount++;
            }
            
            // Отрисовка rightWall (стена на правой границе ячейки)
            // Стена центрируется на границе: 6 пикселей левее, 6 пикселей правее
            if (grid[i][j].rightWall) {
                sf::RectangleShape wall(sf::Vector2f(WALL_WIDTH, WALL_LENGTH));
                wall.setPosition(x + CELL_SIZE - WALL_WIDTH / 2, y);  // Центрировать на границе
                wall.setFillColor(sf::Color(150, 150, 150));
                window.draw(wall);
                wallCount++;
            }
            
            // Отрисовка bottomWall (стена на нижней границе ячейки)
            // Стена центрируется на границе: 6 пикселей выше, 6 пикселей ниже
            if (grid[i][j].bottomWall) {
                sf::RectangleShape wall(sf::Vector2f(WALL_LENGTH, WALL_WIDTH));
                wall.setPosition(x, y + CELL_SIZE - WALL_WIDTH / 2);  // Центрировать на границе
                wall.setFillColor(sf::Color(150, 150, 150));
                window.draw(wall);
                wallCount++;
            }
            
            // Отрисовка leftWall (стена на левой границе ячейки)
            // Стена центрируется на границе: 6 пикселей левее, 6 пикселей правее
            if (grid[i][j].leftWall) {
                sf::RectangleShape wall(sf::Vector2f(WALL_WIDTH, WALL_LENGTH));
                wall.setPosition(x - WALL_WIDTH / 2, y);  // Центрировать на границе
                wall.setFillColor(sf::Color(150, 150, 150));
                window.draw(wall);
                wallCount++;
            }
        }
    }
    
    #ifdef _DEBUG
    std::cout << "Walls rendered: " << wallCount << std::endl;
    #endif
}
```

### Система столкновений

```cpp
sf::Vector2f resolveCollision(sf::Vector2f oldPos, sf::Vector2f newPos) {
    // Создать прямоугольник игрока
    sf::FloatRect playerRect(
        newPos.x - PLAYER_SIZE / 2,
        newPos.y - PLAYER_SIZE / 2,
        PLAYER_SIZE,
        PLAYER_SIZE
    );
    
    // Вычислить видимые ячейки
    int playerCellX = static_cast<int>(newPos.x / CELL_SIZE);
    int playerCellY = static_cast<int>(newPos.y / CELL_SIZE);
    
    int startX = std::max(0, playerCellX - 1);
    int startY = std::max(0, playerCellY - 1);
    int endX = std::min(GRID_SIZE - 1, playerCellX + 1);
    int endY = std::min(GRID_SIZE - 1, playerCellY + 1);
    
    bool collision = false;
    
    // Проверка столкновений с видимыми стенами
    for (int i = startX; i <= endX; i++) {
        for (int j = startY; j <= endY; j++) {
            float x = i * CELL_SIZE;
            float y = j * CELL_SIZE;
            
            // Проверка topWall (центрирована на границе)
            if (grid[i][j].topWall) {
                sf::FloatRect wallRect(x, y - WALL_WIDTH / 2, WALL_LENGTH, WALL_WIDTH);
                if (playerRect.intersects(wallRect)) {
                    collision = true;
                    break;
                }
            }
            
            // Проверка rightWall (центрирована на границе)
            if (grid[i][j].rightWall) {
                sf::FloatRect wallRect(x + CELL_SIZE - WALL_WIDTH / 2, y, WALL_WIDTH, WALL_LENGTH);
                if (playerRect.intersects(wallRect)) {
                    collision = true;
                    break;
                }
            }
            
            // Проверка bottomWall (центрирована на границе)
            if (grid[i][j].bottomWall) {
                sf::FloatRect wallRect(x, y + CELL_SIZE - WALL_WIDTH / 2, WALL_LENGTH, WALL_WIDTH);
                if (playerRect.intersects(wallRect)) {
                    collision = true;
                    break;
                }
            }
            
            // Проверка leftWall (центрирована на границе)
            if (grid[i][j].leftWall) {
                sf::FloatRect wallRect(x - WALL_WIDTH / 2, y, WALL_WIDTH, WALL_LENGTH);
                if (playerRect.intersects(wallRect)) {
                    collision = true;
                    break;
                }
            }
        }
        if (collision) break;
    }
    
    if (collision) {
        // Оттолкнуть на 1 пиксель
        sf::Vector2f direction = oldPos - newPos;
        float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
        
        if (length > 0.001f) {
            direction /= length;
            return oldPos + direction * 1.0f;
        }
        
        return oldPos;
    }
    
    // Ограничить границами карты
    newPos.x = std::max(PLAYER_SIZE / 2, std::min(newPos.x, MAP_SIZE - PLAYER_SIZE / 2));
    newPos.y = std::max(PLAYER_SIZE / 2, std::min(newPos.y, MAP_SIZE - PLAYER_SIZE / 2));
    
    return newPos;
}
```

## Модели данных

### Сетевая синхронизация карты

```cpp
// Сериализация карты для отправки клиенту
void serializeMap(std::vector<char>& buffer) {
    buffer.resize(sizeof(grid));
    std::memcpy(buffer.data(), &grid, sizeof(grid));
}

// Десериализация карты на клиенте
void deserializeMap(const std::vector<char>& buffer) {
    std::memcpy(&grid, buffer.data(), sizeof(grid));
}

// Отправка карты через TCP (в функции обработки подключения)
void sendMapToClient(sf::TcpSocket& clientSocket) {
    std::vector<char> mapData;
    serializeMap(mapData);
    
    // Отправить размер данных
    uint32_t dataSize = static_cast<uint32_t>(mapData.size());
    clientSocket.send(&dataSize, sizeof(dataSize));
    
    // Отправить данные карты
    clientSocket.send(mapData.data(), mapData.size());
}

// Получение карты на клиенте
bool receiveMapFromServer(sf::TcpSocket& serverSocket) {
    // Получить размер данных
    uint32_t dataSize;
    std::size_t received;
    if (serverSocket.receive(&dataSize, sizeof(dataSize), received) != sf::Socket::Done) {
        return false;
    }
    
    // Получить данные карты
    std::vector<char> mapData(dataSize);
    if (serverSocket.receive(mapData.data(), dataSize, received) != sf::Socket::Done) {
        return false;
    }
    
    deserializeMap(mapData);
    return true;
}
```

### Валидация позиций

```cpp
bool validatePosition(sf::Vector2f position) {
    return position.x >= 0.0f && position.x <= MAP_SIZE &&
           position.y >= 0.0f && position.y <= MAP_SIZE;
}
```

## Обработка ошибок

### Ошибки генерации карты

```cpp
void handleMapGenerationFailure() {
    std::cerr << "[CRITICAL] Failed to generate valid map after 10 attempts" << std::endl;
    std::cerr << "The map may have too many walls blocking paths." << std::endl;
    std::cerr << "Please restart the server to try again." << std::endl;
    
    // Показать сообщение об ошибке в окне
    sf::RenderWindow errorWindow(sf::VideoMode(800, 200), "Map Generation Error");
    sf::Font font;
    // ... отобразить сообщение об ошибке
    
    // Корректно завершить работу
    exit(1);
}
```

### Ошибки сетевой синхронизации

```cpp
void handleMapSyncError() {
    std::cerr << "[ERROR] Failed to synchronize map data" << std::endl;
    // Попытаться переподключиться или показать сообщение об ошибке
}
```

## Стратегия тестирования

### Unit тесты

1. **Тест генерации ячейки**
   - Проверить, что ячейки с (i+j)%2==1 получают стены согласно вероятностям
   - Проверить, что ячейки с (i+j)%2==0 не генерируют стены (но могут содержать части стен соседних ячеек)

2. **Тест вероятностей**
   - Сгенерировать 1000 карт
   - Проверить, что распределение стен близко к 60%/25%/15%

3. **Тест BFS**
   - Создать карту с известным путём
   - Проверить, что BFS находит путь
   - Создать карту без пути
   - Проверить, что BFS возвращает false

4. **Тест столкновений**
   - Создать игрока рядом со стеной
   - Попытаться пройти сквозь стену
   - Проверить, что игрок отталкивается

5. **Тест кэширования**
   - Проверить, что границы пересчитываются только при смене ячейки
   - Проверить, что границы не пересчитываются при движении внутри ячейки

### Интеграционные тесты

1. **Тест полного цикла генерации**
   - Запустить сервер
   - Проверить, что карта генерируется успешно
   - Проверить, что путь между спавнами существует

2. **Тест сетевой синхронизации**
   - Запустить сервер и клиент
   - Проверить, что клиент получает карту
   - Проверить, что карты идентичны на сервере и клиенте

3. **Тест рендеринга**
   - Переместить игрока по карте
   - Проверить, что отрисовываются только видимые стены
   - Проверить, что количество стен не превышает 500

4. **Тест производительности**
   - Измерить FPS при движении по карте
   - Проверить, что FPS >= 55
   - Измерить использование CPU
   - Проверить, что CPU < 40%

## Примечания по реализации

### Интеграция в существующий код

**Изменения в Zero_Ground.cpp (сервер):**
1. Заменить функцию генерации карты на `generateValidMap()`
2. Заменить функцию рендеринга на `renderVisibleWalls()`
3. Добавить `updateCamera()` в игровой цикл
4. Заменить систему столкновений на `resolveCollision()`
5. Обновить отправку карты клиенту на `sendMapToClient()`

**Изменения в Zero_Ground_client.cpp (клиент):**
1. Заменить получение карты на `receiveMapFromServer()`
2. Заменить функцию рендеринга на `renderVisibleWalls()`
3. Добавить `updateCamera()` в игровой цикл
4. Заменить систему столкновений на `resolveCollision()`

**Сохранить без изменений:**
- TCP/UDP сокеты и сетевые потоки
- Протокол готовности игроков
- UI стартового экрана сервера
- UI экрана подключения клиента
- Систему тумана войны (адаптировать к новым координатам)
- HUD и визуальную обратную связь

### Оптимизации

1. **Кэширование видимых ячеек**: Пересчёт только при смене ячейки игрока
2. **Ограничение области столкновений**: Проверка только ячеек в радиусе 1 от игрока
3. **Ограничение рендеринга**: Максимум 500 стен в кадре (21×21 ячейка × ~4 стены = ~1764 стен, но большинство ячеек пустые)
4. **Простые AABB-коллизии**: Без Quadtree для упрощения

### Константы производительности

- Размер карты: 5000×5000 пикселей
- Размер ячейки: 30 пикселей
- Количество ячеек: 167×167 = 27,889 ячеек
- Видимая область: 21×21 = 441 ячейка
- Максимум стен в кадре: 500
- Целевой FPS: 55+
- Целевое использование CPU: <40%
