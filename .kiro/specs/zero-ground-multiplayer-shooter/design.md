# Документ дизайна

## Обзор

Zero Ground - это клиент-серверный многопользовательский 2D шутер, построенный на C++ и SFML 2.6.1. Архитектура разделяет задачи на отдельные слои: сетевой транспорт (TCP/UDP), управление состоянием игры, рендеринг и обработка ввода. Сервер выступает в качестве авторитетного источника состояния игры, генерируя карту и координируя готовность игроков перед стартом игры. Клиенты подключаются, синхронизируют состояние через UDP и отрисовывают свой локальный вид с эффектами тумана войны.

Дизайн делает акцент на потокобезопасности через защищённое mutex общее состояние, оптимизацию сети через пространственную отсечку и плавный геймплей через интерполяцию. Система проверки готовности обеспечивает координированный старт игры, в то время как система тумана войны создаёт стратегическую глубину геймплея.

**ВАЖНО: Размер игрового поля - 500x500 единиц (изменено с 1000x1000)**

## Архитектура

### Высокоуровневая архитектура

```
┌─────────────────────────────────────────────────────────────┐
│                         СЕРВЕР                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ TCP Слушатель│  │ UDP Сокет    │  │ Менеджер     │      │
│  │ (Порт 53000) │  │ (Порт 53001) │  │ Состояния    │      │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
│         │                  │                  │              │
│         └──────────────────┴──────────────────┘              │
│                            │                                 │
│         ┌──────────────────┴──────────────────┐             │
│         │                                      │             │
│  ┌──────▼───────┐  ┌──────────────┐  ┌───────▼──────┐      │
│  │ Генератор    │  │ Обнаружение  │  │ Движок       │      │
│  │ Карты (BFS)  │  │ Столкновений │  │ Рендеринга   │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                            ▲
                            │ TCP/UDP
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                         КЛИЕНТ                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ TCP Сокет    │  │ UDP Сокет    │  │ Локальный    │      │
│  │ (Порт 53000) │  │ (Порт 53002) │  │ Менеджер     │      │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
│         │                  │                  │              │
│         └──────────────────┴──────────────────┘              │
│                            │                                 │
│         ┌──────────────────┴──────────────────┐             │
│         │                                      │             │
│  ┌──────▼───────┐  ┌──────────────┐  ┌───────▼──────┐      │
│  │ Обработчик   │  │ Система      │  │ Движок       │      │
│  │ Ввода        │  │ Тумана Войны │  │ Рендеринга   │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
```

### Ответственность компонентов

**Компоненты сервера:**
- **TCP Слушатель**: Обрабатывает начальные подключения клиентов, статус готовности и сигналы старта игры
- **UDP Сокет**: Транслирует позиции игроков с частотой 20Hz подключённым клиентам
- **Менеджер состояния игры**: Поддерживает авторитетные позиции игроков, здоровье и счёт с защитой mutex
- **Генератор карты**: Создаёт процедурные карты с BFS валидацией связности
- **Обнаружение столкновений**: Использует пространственное разбиение quadtree для эффективной проверки столкновений со стенами
- **Движок рендеринга**: Отображает игрока сервера (зелёный круг) и подключённых клиентов (синие круги)

**Компоненты клиента:**
- **TCP Сокет**: Получает данные карты, начальное состояние и сигналы старта игры
- **UDP Сокет**: Отправляет позицию локального игрока и получает позиции других игроков
- **Локальный менеджер состояния**: Поддерживает интерполированные позиции для плавной отрисовки
- **Обработчик ввода**: Обрабатывает ввод WASD и отправляет обновления позиции
- **Система тумана войны**: Рассчитывает радиус видимости и применяет эффекты затемнения
- **Движок рендеринга**: Отображает локального игрока (синий круг), видимых врагов и стены с эффектами тумана

### Диаграммы конечных автоматов

**Поток состояний сервера:**
```
┌─────────────┐
│ Стартовый   │
│ Экран       │
│ (Показ IP)  │
└──────┬──────┘
       │
       │ Клиент подключается
       ▼
┌─────────────┐
│ Ожидание    │
│ Готовности  │
│ (Жёлтое сообщ)│
└──────┬──────┘
       │
       │ Клиент отправляет ГОТОВ
       ▼
┌─────────────┐
│ Игрок Готов │
│ (Зелёное сообщ)│
│ Показ ИГРАТЬ│
└──────┬──────┘
       │
       │ Оператор нажимает ИГРАТЬ
       ▼
┌─────────────┐
│ Игра Активна│
│ (Главный цикл)│
└─────────────┘
```

**Поток состояний клиента:**
```
┌─────────────┐
│ Экран       │
│ Подключения │
│ (Ввод IP)   │
└──────┬──────┘
       │
       │ Нажатие ПОДКЛЮЧИТЬСЯ / Enter
       ▼
┌─────────────┐
│ Подключение │
│ (TCP тест)  │
└──────┬──────┘
       │
       ├─ Неудача ──► Экран ошибки (3с) ──► Экран подключения
       │
       │ Успех
       ▼
┌─────────────┐
│ Подключён   │
│ Показ ГОТОВ │
└──────┬──────┘
       │
       │ Нажатие ГОТОВ
       ▼
┌─────────────┐
│ Ожидание    │
│ Старта      │
│ (Жёлтое сообщ)│
└──────┬──────┘
       │
       │ Получение сигнала СТАРТ
       ▼
┌─────────────┐
│ Игра Активна│
│ (Главный цикл)│
└─────────────┘
```

## Компоненты и интерфейсы

### Сетевой протокол

**TCP Сообщения (Порт 53000):**

```cpp
// Типы сообщений
enum class MessageType : uint8_t {
    CLIENT_CONNECT = 0x01,      // Клиент → Сервер
    SERVER_ACK = 0x02,          // Сервер → Клиент
    CLIENT_READY = 0x03,        // Клиент → Сервер
    SERVER_START = 0x04,        // Сервер → Клиент
    MAP_DATA = 0x05             // Сервер → Клиент
};

// Пакет подключения
struct ConnectPacket {
    MessageType type = MessageType::CLIENT_CONNECT;
    uint32_t protocolVersion = 1;
    char playerName[32] = {0};
};

// Пакет готовности
struct ReadyPacket {
    MessageType type = MessageType::CLIENT_READY;
    bool isReady = true;
};

// Пакет старта игры
struct StartPacket {
    MessageType type = MessageType::SERVER_START;
    uint32_t timestamp;
};

// Пакет данных карты
struct MapDataPacket {
    MessageType type = MessageType::MAP_DATA;
    uint32_t wallCount;
    // Далее следует wallCount * структур Wall
};

struct Wall {
    float x, y;           // Верхний левый угол
    float width, height;
};
```

**UDP Сообщения (Порт 53001):**

```cpp
// Пакет обновления позиции (отправляется 20 раз в секунду)
struct PositionPacket {
    float x, y;
    bool isAlive;
    uint32_t frameID;     // Для компенсации задержки
    uint8_t playerId;
};
```


### Основные структуры данных

```cpp
// Представление игрока
struct Player {
    uint32_t id;
    sf::IpAddress ipAddress;
    float x, y;
    float previousX, previousY;  // Для интерполяции
    float health = 100.0f;
    int score = 0;
    bool isAlive = true;
    bool isReady = false;
    sf::Color color;
    
    // Поддержка интерполяции
    float getInterpolatedX(float alpha) const {
        return previousX + (x - previousX) * alpha;
    }
    
    float getInterpolatedY(float alpha) const {
        return previousY + (y - previousY) * alpha;
    }
};

// Игровая карта (500x500 единиц)
struct GameMap {
    std::vector<Wall> walls;
    const float width = 500.0f;   // Изменено с 1000.0f
    const float height = 500.0f;  // Изменено с 1000.0f
    
    // Quadtree для оптимизации столкновений
    std::unique_ptr<Quadtree> spatialIndex;
    
    bool isValidPath(sf::Vector2f start, sf::Vector2f end) const;
    void generate();
};

// Узел Quadtree для пространственного разбиения
struct Quadtree {
    sf::FloatRect bounds;
    std::vector<Wall*> walls;
    std::unique_ptr<Quadtree> children[4];
    const int MAX_WALLS = 10;
    const int MAX_DEPTH = 5;
    
    void insert(Wall* wall, int depth = 0);
    std::vector<Wall*> query(sf::FloatRect area) const;
};
```

### Алгоритм генерации карты

```cpp
class MapGenerator {
public:
    GameMap generate() {
        for (int attempt = 0; attempt < 10; ++attempt) {
            GameMap map;
            generateWalls(map);
            
            if (validateConnectivity(map)) {
                map.spatialIndex = buildQuadtree(map.walls);
                return map;
            }
        }
        throw std::runtime_error("Не удалось сгенерировать валидную карту после 10 попыток");
    }
    
private:
    void generateWalls(GameMap& map) {
        const float targetCoverage = 0.30f; // 30% в среднем
        const float totalArea = 500.0f * 500.0f; // 250,000 кв. единиц
        float currentCoverage = 0.0f;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> posDist(0.0f, 500.0f);
        std::uniform_real_distribution<float> sizeDist(2.0f, 8.0f);
        
        while (currentCoverage < targetCoverage * totalArea) {
            Wall wall;
            wall.x = posDist(gen);
            wall.y = posDist(gen);
            wall.width = sizeDist(gen);
            wall.height = sizeDist(gen);
            
            // Убедиться, что стена не перекрывает точки спавна
            if (!overlapsSpawnPoint(wall)) {
                map.walls.push_back(wall);
                currentCoverage += wall.width * wall.height;
            }
        }
    }
    
    bool validateConnectivity(const GameMap& map) {
        // BFS от нижнего левого к верхнему правому
        sf::Vector2f start(25.0f, 475.0f);  // Спавн сервера (изменено)
        sf::Vector2f end(475.0f, 25.0f);    // Спавн клиента (изменено)
        
        return bfsPathExists(start, end, map);
    }
    
    bool bfsPathExists(sf::Vector2f start, sf::Vector2f end, const GameMap& map) {
        // BFS на основе сетки с ячейками 10x10 единиц
        const int gridSize = 50; // Изменено с 100 (500/10 = 50)
        std::vector<std::vector<bool>> visited(gridSize, std::vector<bool>(gridSize, false));
        std::queue<sf::Vector2i> queue;
        
        sf::Vector2i startCell(start.x / 10, start.y / 10);
        sf::Vector2i endCell(end.x / 10, end.y / 10);
        
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
                
                if (nx >= 0 && nx < gridSize && ny >= 0 && ny < gridSize &&
                    !visited[nx][ny] && !cellHasWall(nx, ny, map)) {
                    visited[nx][ny] = true;
                    queue.push(sf::Vector2i(nx, ny));
                }
            }
        }
        
        return false;
    }
    
    bool cellHasWall(int cellX, int cellY, const GameMap& map) {
        sf::FloatRect cellBounds(cellX * 10.0f, cellY * 10.0f, 10.0f, 10.0f);
        for (const auto& wall : map.walls) {
            sf::FloatRect wallBounds(wall.x, wall.y, wall.width, wall.height);
            if (cellBounds.intersects(wallBounds)) return true;
        }
        return false;
    }
    
    bool overlapsSpawnPoint(const Wall& wall) {
        sf::FloatRect wallBounds(wall.x, wall.y, wall.width, wall.height);
        sf::FloatRect serverSpawn(0.0f, 450.0f, 50.0f, 50.0f);   // Изменено
        sf::FloatRect clientSpawn(450.0f, 0.0f, 50.0f, 50.0f);   // Изменено
        
        return wallBounds.intersects(serverSpawn) || wallBounds.intersects(clientSpawn);
    }
};
```

### Система обнаружения столкновений

```cpp
class CollisionSystem {
public:
    CollisionSystem(const GameMap& map) : map_(map) {}
    
    // Возвращает скорректированную позицию после разрешения столкновения
    sf::Vector2f resolveCollision(sf::Vector2f oldPos, sf::Vector2f newPos, float radius) {
        // Запрос ближайших стен используя quadtree
        sf::FloatRect queryArea(
            newPos.x - radius - 1.0f,
            newPos.y - radius - 1.0f,
            radius * 2 + 2.0f,
            radius * 2 + 2.0f
        );
        
        auto nearbyWalls = map_.spatialIndex->query(queryArea);
        
        for (const Wall* wall : nearbyWalls) {
            if (circleRectCollision(newPos, radius, *wall)) {
                // Оттолкнуть на 1 единицу в противоположном направлении
                sf::Vector2f direction = oldPos - newPos;
                float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
                
                if (length > 0.001f) {
                    direction /= length;
                    return oldPos + direction * 1.0f;
                }
                
                return oldPos; // Нет движения если уже в столкновении
            }
        }
        
        return newPos; // Нет столкновения
    }
    
private:
    bool circleRectCollision(sf::Vector2f center, float radius, const Wall& wall) {
        // Найти ближайшую точку на прямоугольнике к центру круга
        float closestX = std::max(wall.x, std::min(center.x, wall.x + wall.width));
        float closestY = std::max(wall.y, std::min(center.y, wall.y + wall.height));
        
        float dx = center.x - closestX;
        float dy = center.y - closestY;
        
        return (dx * dx + dy * dy) < (radius * radius);
    }
    
    const GameMap& map_;
};
```

### Рендеринг тумана войны

```cpp
class FogOfWarRenderer {
public:
    void render(sf::RenderWindow& window, sf::Vector2f playerPos, 
                const std::vector<Player>& otherPlayers, const GameMap& map) {
        const float visibilityRadius = 25.0f;
        
        // Отрисовать все стены (затемнённые вне радиуса)
        for (const auto& wall : map.walls) {
            sf::RectangleShape wallShape(sf::Vector2f(wall.width, wall.height));
            wallShape.setPosition(wall.x, wall.y);
            
            float distance = getDistance(playerPos, sf::Vector2f(wall.x, wall.y));
            if (distance > visibilityRadius) {
                wallShape.setFillColor(sf::Color(100, 100, 100)); // Затемнённый
            } else {
                wallShape.setFillColor(sf::Color(150, 150, 150)); // Нормальный
            }
            
            window.draw(wallShape);
        }
        
        // Отрисовать только видимых игроков
        for (const auto& player : otherPlayers) {
            float distance = getDistance(playerPos, sf::Vector2f(player.x, player.y));
            if (distance <= visibilityRadius) {
                sf::CircleShape playerShape(20.0f);
                playerShape.setPosition(player.x - 20.0f, player.y - 20.0f);
                playerShape.setFillColor(player.color);
                window.draw(playerShape);
            }
        }
        
        // Применить оверлей тумана
        sf::RectangleShape fogOverlay(sf::Vector2f(window.getSize()));
        fogOverlay.setFillColor(sf::Color(0, 0, 0, 200));
        
        // Создать круговую вырезку используя шейдер (упрощённый подход)
        // В реальной реализации использовать sf::Shader для правильного кругового тумана
        window.draw(fogOverlay);
    }
    
private:
    float getDistance(sf::Vector2f a, sf::Vector2f b) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }
};
```

## Модели данных

### Потокобезопасное состояние игры

```cpp
class GameState {
public:
    // Потокобезопасный доступ к игрокам
    void updatePlayerPosition(uint32_t playerId, float x, float y) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (players_.count(playerId)) {
            players_[playerId].previousX = players_[playerId].x;
            players_[playerId].previousY = players_[playerId].y;
            players_[playerId].x = x;
            players_[playerId].y = y;
        }
    }
    
    Player getPlayer(uint32_t playerId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return players_.at(playerId);
    }
    
    std::vector<Player> getPlayersInRadius(sf::Vector2f center, float radius) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Player> result;
        
        for (const auto& [id, player] : players_) {
            float dx = player.x - center.x;
            float dy = player.y - center.y;
            float distance = std::sqrt(dx * dx + dy * dy);
            
            if (distance <= radius) {
                result.push_back(player);
            }
        }
        
        return result;
    }
    
    void setPlayerReady(uint32_t playerId, bool ready) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (players_.count(playerId)) {
            players_[playerId].isReady = ready;
        }
    }
    
    bool allPlayersReady() const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [id, player] : players_) {
            if (!player.isReady) return false;
        }
        return !players_.empty();
    }
    
private:
    mutable std::mutex mutex_;
    std::map<uint32_t, Player> players_;
    GameMap map_;
};
```

### Валидация сетевых пакетов

```cpp
class PacketValidator {
public:
    static bool validatePosition(const PositionPacket& packet) {
        return packet.x >= 0.0f && packet.x <= 500.0f &&  // Изменено с 1000.0f
               packet.y >= 0.0f && packet.y <= 500.0f;    // Изменено с 1000.0f
    }
    
    static bool validateMapData(const MapDataPacket& packet) {
        return packet.wallCount > 0 && packet.wallCount < 10000;
    }
    
    static bool validateConnect(const ConnectPacket& packet) {
        return packet.protocolVersion == 1 &&
               std::strlen(packet.playerName) < 32;
    }
};
```

## Обработка ошибок

### Сетевые ошибки

**Ошибки подключения:**
- Таймаут TCP подключения (3 секунды) вызывает экран ошибки с опцией повтора
- Невалидный формат IP адреса отображает сообщение об ошибке на 3 секунды
- Обнаружение потери соединения после 2 секунд без UDP пакетов

**Валидация пакетов:**
```cpp
class ErrorHandler {
public:
    static void handleInvalidPacket(const std::string& reason) {
        std::cerr << "[ОШИБКА] Невалидный пакет: " << reason << std::endl;
        // Пакет отбрасывается, игра продолжается
    }
    
    static void handleConnectionLost(const std::string& serverIP) {
        // Отобразить экран переподключения
        showReconnectScreen(serverIP);
    }
    
    static void handleMapGenerationFailure() {
        std::cerr << "[КРИТИЧНО] Не удалось сгенерировать валидную карту после 10 попыток" << std::endl;
        // Отобразить сообщение об ошибке и корректно выйти
        exit(1);
    }
};
```

### Ошибки состояния игры

**Граничные случаи столкновений:**
- Спавн игрока внутри стены: Оттолкнуть к ближайшей валидной позиции
- Множественные одновременные столкновения: Разрешить в порядке обнаружения
- Ошибки точности с плавающей точкой: Использовать сравнение с эпсилон (0.001f)

**Потокобезопасность:**
- Весь доступ к общему состоянию обёрнут в блокировки mutex
- Нет прямого обмена указателями между потоками
- Копирование данных при передаче между потоками

### Деградация производительности

**Мониторинг частоты кадров:**
```cpp
class PerformanceMonitor {
public:
    void update(float deltaTime) {
        frameCount_++;
        elapsedTime_ += deltaTime;
        
        if (elapsedTime_ >= 1.0f) {
            float fps = frameCount_ / elapsedTime_;
            
            if (fps < 55.0f) {
                logPerformanceWarning(fps);
            }
            
            frameCount_ = 0;
            elapsedTime_ = 0.0f;
        }
    }
    
private:
    void logPerformanceWarning(float fps) {
        std::cerr << "[ПРЕДУПРЕЖДЕНИЕ] FPS упал до " << fps << std::endl;
        // Логировать дополнительные метрики: количество игроков, стен и т.д.
    }
    
    int frameCount_ = 0;
    float elapsedTime_ = 0.0f;
};
```

## Примечания по реализации

### Упрощённая архитектура

**ВАЖНО:** Весь код должен быть в двух файлах:
- `Zero_Ground/Zero_Ground.cpp` - Весь код сервера
- `Zero_Ground_client/Zero_Ground_client.cpp` - Весь код клиента

Все функции, структуры и классы должны быть определены внутри этих файлов. НЕ создавать отдельные заголовочные файлы или модули.

### Адаптация размера поля

Все координаты и расчёты адаптированы для поля 500x500 единиц:
- Границы карты: [0, 500] для x и y
- Общая площадь: 250,000 кв. единиц
- Точки спавна: (25, 475) для сервера, (475, 25) для клиента
- Зоны спавна: 50x50 единиц в углах
- Сетка BFS: 50x50 ячеек (10 единиц на ячейку)

### Константы производительности

- Радиус видимости: 25 единиц (без изменений)
- Радиус сетевой отсечки: 50 единиц (без изменений)
- Скорость движения: 5 единиц/секунду (без изменений)
- Частота UDP: 20 пакетов/секунду (без изменений)
- Целевой FPS: 55+ (без изменений)
