# Синхронизация поворотов игроков

## Описание
Реализована синхронизация поворотов игроков между клиентом и сервером. Теперь каждый игрок может видеть, куда смотрит (целится) его противник.

## Изменения

### 1. Структура Player
Добавлено новое поле `rotation` для хранения угла поворота игрока:
```cpp
struct Player {
    // ... существующие поля ...
    float rotation = 0.0f;  // Player rotation angle in degrees (0-360)
    // ... остальные поля ...
};
```

**Файлы:**
- `Zero_Ground/Zero_Ground.cpp` (сервер)
- `Zero_Ground_client/Zero_Ground_client.cpp` (клиент)

### 2. Структура PositionPacket
Добавлено поле `rotation` для передачи угла поворота по сети:
```cpp
struct PositionPacket {
    float x = 0.0f;
    float y = 0.0f;
    float rotation = 0.0f;  // Player rotation angle in degrees
    float health = 100.0f;
    bool isAlive = true;
    uint32_t frameID = 0;
    uint8_t playerId = 0;
};
```

**Файлы:**
- `Zero_Ground/Zero_Ground.cpp` (сервер)
- `Zero_Ground_client/Zero_Ground_client.cpp` (клиент)

### 3. Клиент - Обновление rotation

#### 3.1. Вычисление угла к мыши
Клиент вычисляет угол поворота на основе позиции курсора мыши:
```cpp
// Calculate rotation angle to mouse cursor
sf::Vector2i mousePixelPos = sf::Mouse::getPosition(window);
sf::Vector2f mouseWorldPos = window.mapPixelToCoords(mousePixelPos);

// Calculate angle from player to mouse (in degrees)
float dx = mouseWorldPos.x - renderPos.x;
float dy = mouseWorldPos.y - renderPos.y;
float angleToMouse = std::atan2(dy, dx) * 180.0f / 3.14159f;

// Update player rotation for synchronization
{
    std::lock_guard<std::mutex> lock(mutex);
    clientPlayer.rotation = angleToMouse;
}
```

**Местоположение:** `Zero_Ground_client/Zero_Ground_client.cpp`, функция отрисовки игрока

#### 3.2. Отправка rotation на сервер
Клиент отправляет свой угол поворота в UDP пакете:
```cpp
PositionPacket outPacket;
outPacket.x = clientPos.x;
outPacket.y = clientPos.y;
outPacket.rotation = clientPlayer.rotation;  // Send player rotation
outPacket.isAlive = true;
outPacket.frameID = currentFrameID++;
outPacket.playerId = 1;
```

**Местоположение:** `Zero_Ground_client/Zero_Ground_client.cpp`, функция `udpThread`

#### 3.3. Получение rotation противника
Клиент получает угол поворота серверного игрока:
```cpp
if (inPacket->playerId == 0) { // Server is player 0
    // ... обновление позиции ...
    
    // Update server player rotation
    serverPlayer.rotation = inPacket->rotation;
    
    // ... остальная обработка ...
}
```

**Местоположение:** `Zero_Ground_client/Zero_Ground_client.cpp`, функция `udpThread`

#### 3.4. Отрисовка противника с rotation
Клиент рисует серверного игрока с учетом его поворота:
```cpp
// Apply fog to server player sprite
serverSprite.setColor(sf::Color(255, 255, 255, alpha));
serverSprite.setPosition(currentServerPos.x, currentServerPos.y);
// Apply server player rotation (subtract 90 degrees because sprite initially faces up)
serverSprite.setRotation(serverPlayer.rotation - 90.0f);
window.draw(serverSprite);
```

**Местоположение:** `Zero_Ground_client/Zero_Ground_client.cpp`, основной игровой цикл

### 4. Сервер - Обработка rotation

#### 4.1. Вычисление угла к мыши
Сервер вычисляет угол поворота на основе позиции курсора мыши:
```cpp
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
```

**Местоположение:** `Zero_Ground/Zero_Ground.cpp`, функция отрисовки игрока

#### 4.2. Получение rotation от клиента
Сервер получает угол поворота клиента:
```cpp
if (validatePosition(*receivedPacket)) {
    // ... обновление позиции ...
    
    if (clientIsAlive && !clientWaitingRespawn) {
        std::lock_guard<std::mutex> lock(mutex);
        
        // ... обновление позиции ...
        
        // Update client player rotation
        clientPlayer.rotation = receivedPacket->rotation;
        
        // ... остальная обработка ...
    }
}
```

**Местоположение:** `Zero_Ground/Zero_Ground.cpp`, UDP обработчик

#### 4.3. Отправка rotation клиентам
Сервер отправляет углы поворота обоих игроков:
```cpp
// Prepare server position packet
PositionPacket serverPacket;
serverPacket.x = serverPos.x;
serverPacket.y = serverPos.y;
serverPacket.rotation = serverPlayer.rotation;  // Send server player rotation
serverPacket.health = serverHealth;
serverPacket.isAlive = serverIsAlive;
serverPacket.frameID = static_cast<uint32_t>(std::time(nullptr));
serverPacket.playerId = 0;

// ... отправка ...

// Send client's own position and health back to them
PositionPacket clientPacket;
clientPacket.x = clientPos.x;
clientPacket.y = clientPos.y;
clientPacket.rotation = clientPlayer.rotation;  // Send client player rotation
clientPacket.health = clientHealth;
```

**Местоположение:** `Zero_Ground/Zero_Ground.cpp`, UDP обработчик

#### 4.4. Отрисовка клиента с rotation
Сервер рисует клиента с учетом его поворота:
```cpp
// Apply fog to client player sprite
clientSprites[ip].setColor(sf::Color(255, 255, 255, alpha));

// Position client sprite (centered on position)
clientSprites[ip].setPosition(renderClientPos.x, renderClientPos.y);

// Apply client player rotation (subtract 90 degrees because sprite initially faces up)
clientSprites[ip].setRotation(clientPlayer.rotation - 90.0f);

// Draw client player
window.draw(clientSprites[ip]);
```

**Местоположение:** `Zero_Ground/Zero_Ground.cpp`, основной игровой цикл

### 5. Новые глобальные переменные

#### Клиент
```cpp
// Server player (for rendering opponent)
Player serverPlayer;
```

#### Сервер
```cpp
// Client player (for tracking opponent state)
Player clientPlayer;
```

## Технические детали

### Угол поворота
- Угол измеряется в градусах (0-360°)
- 0° соответствует направлению вправо (восток)
- Угол увеличивается по часовой стрелке
- Вычисляется с помощью `std::atan2(dy, dx) * 180.0f / 3.14159f`

### Отрисовка спрайта
- Спрайт изначально направлен вверх (север)
- При отрисовке вычитается 90° для корректной ориентации: `sprite.setRotation(rotation - 90.0f)`

### Синхронизация
- Rotation отправляется вместе с позицией в каждом UDP пакете (20 раз в секунду)
- Обновление происходит в реальном времени
- Используется thread-safe доступ через mutex

## Компиляция

Для применения изменений необходимо перекомпилировать проект:

### Сервер
```bash
cd Zero_Ground
msbuild Zero_Ground.vcxproj /p:Configuration=Debug /p:Platform=Win32
```

### Клиент
```bash
cd Zero_Ground_client
msbuild Zero_Ground_client.vcxproj /p:Configuration=Debug /p:Platform=Win32
```

Или используйте Visual Studio для компиляции обоих проектов.

## Тестирование

1. Запустите сервер (`Zero_Ground.exe`)
2. Запустите клиент (`Zero_Ground_client.exe`)
3. Подключитесь к серверу
4. Двигайте мышью - ваш персонаж будет поворачиваться
5. На экране противника вы увидите, что его персонаж тоже поворачивается в направлении его мыши

## Результат

Теперь игроки могут видеть направление взгляда друг друга в реальном времени, что делает игру более тактической и реалистичной.
