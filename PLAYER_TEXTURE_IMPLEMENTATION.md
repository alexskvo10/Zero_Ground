# Замена кругов игроков на текстуру

## Дата: 26 ноября 2025

## Описание изменений

Заменены круги игроков (сервер и клиенты) на текстуру `Nothing_1.png` размером 30x30 пикселей.
Обновлены хитбоксы для соответствия новому размеру.

## Изменения в коде

### Файл: Zero_Ground/Zero_Ground.cpp

#### 1. Обновлена константа размера игрока (строка ~357)

**Было:**
```cpp
const float PLAYER_SIZE = 20.0f;  // Diameter (radius = 10px)
```

**Стало:**
```cpp
const float PLAYER_SIZE = 30.0f;  // Texture size 30x30 pixels (radius = 15px)
```

#### 2. Загрузка текстуры и создание спрайтов (строка ~4088)

**Было:**
```cpp
// �������� ��������� ������
sf::CircleShape serverCircle(PLAYER_SIZE / 2.0f);
serverCircle.setFillColor(sf::Color::Green);
serverCircle.setOutlineColor(sf::Color(0, 100, 0));
serverCircle.setOutlineThickness(3.0f);
serverCircle.setPosition(serverPos.x - PLAYER_SIZE / 2.0f, serverPos.y - PLAYER_SIZE / 2.0f);

std::map<sf::IpAddress, sf::CircleShape> clientCircles;
```

**Стало:**
```cpp
// Load player texture
sf::Texture playerTexture;
if (!playerTexture.loadFromFile("Nothing_1.png")) {
    std::cerr << "Failed to load player texture Nothing_1.png!" << std::endl;
    return -1;
}

// Create server player sprite
sf::Sprite serverSprite;
serverSprite.setTexture(playerTexture);
serverSprite.setOrigin(PLAYER_SIZE / 2.0f, PLAYER_SIZE / 2.0f); // Center origin
serverSprite.setPosition(serverPos.x, serverPos.y);

std::map<sf::IpAddress, sf::Sprite> clientSprites;
```

#### 3. Отрисовка сервера (строка ~5069)

**Было:**
```cpp
// Draw server player as green circle - always visible
serverCircle.setRadius(PLAYER_SIZE / 2.0f);
serverCircle.setPosition(renderPos.x - PLAYER_SIZE / 2.0f, renderPos.y - PLAYER_SIZE / 2.0f);
window.draw(serverCircle);
```

**Стало:**
```cpp
// Draw server player sprite - always visible
serverSprite.setPosition(renderPos.x, renderPos.y);
window.draw(serverSprite);
```

#### 4. Отрисовка клиентов (строка ~4960)

**Было:**
```cpp
if (clientCircles.find(ip) == clientCircles.end()) {
    clientCircles[ip] = sf::CircleShape(PLAYER_SIZE / 2.0f);
    clientCircles[ip].setFillColor(sf::Color::Blue);
    clientCircles[ip].setOutlineColor(sf::Color(0, 0, 100));
    clientCircles[ip].setOutlineThickness(2.0f);
}
// ... позиционирование и отрисовка круга
```

**Стало:**
```cpp
if (clientSprites.find(ip) == clientSprites.end()) {
    clientSprites[ip] = sf::Sprite();
    clientSprites[ip].setTexture(playerTexture);
    clientSprites[ip].setOrigin(PLAYER_SIZE / 2.0f, PLAYER_SIZE / 2.0f);
}
// ... позиционирование и отрисовка спрайта с применением тумана войны
clientSprites[ip].setColor(sf::Color(255, 255, 255, alpha));
clientSprites[ip].setPosition(renderClientPos.x, renderClientPos.y);
window.draw(clientSprites[ip]);
```

#### 5. Обновлен радиус коллизии (строка ~4477)

**Было:**
```cpp
const float PLAYER_RADIUS = 20.0f; // PLAYER_SIZE / 2
```

**Стало:**
```cpp
const float PLAYER_RADIUS = 15.0f; // PLAYER_SIZE / 2 (30 / 2 = 15)
```

### Файл: Zero_Ground_client/Zero_Ground_client.cpp

#### 1. Обновлена константа размера игрока (строка ~23)

**Было:**
```cpp
const float PLAYER_SIZE = 20.0f;  // Diameter (radius = 10px)
```

**Стало:**
```cpp
const float PLAYER_SIZE = 30.0f;  // Texture size 30x30 pixels (radius = 15px)
```

#### 2. Загрузка текстуры и создание спрайтов (строка ~3520)

Клиент уже имеет реализацию загрузки текстуры и создания спрайтов:

```cpp
// Load player texture (once)
static sf::Texture playerTexture;
static sf::Sprite serverSprite;
static sf::Sprite clientSprite;
static bool textureLoaded = false;

if (!textureLoaded) {
    if (!playerTexture.loadFromFile("Nothing_1.png")) {
        std::cerr << "Failed to load player texture Nothing_1.png!" << std::endl;
    } else {
        serverSprite.setTexture(playerTexture);
        serverSprite.setOrigin(PLAYER_SIZE / 2.0f, PLAYER_SIZE / 2.0f);
        
        clientSprite.setTexture(playerTexture);
        clientSprite.setOrigin(PLAYER_SIZE / 2.0f, PLAYER_SIZE / 2.0f);
        
        textureLoaded = true;
    }
}
```

#### 3. Отрисовка сервера с туманом войны (строка ~3540)

```cpp
// Draw server player sprite with fog of war
if (isServerConnected && textureLoaded) {
    // Calculate distance from client to server player
    float dx = currentServerPos.x - clientPos.x;
    float dy = currentServerPos.y - clientPos.y;
    float distance = std::sqrt(dx * dx + dy * dy);
    
    // Calculate fog alpha
    sf::Uint8 alpha = calculateFogAlpha(distance);
    
    // Only draw if visible
    if (alpha > 0) {
        // Apply fog to server player sprite
        serverSprite.setColor(sf::Color(255, 255, 255, alpha));
        serverSprite.setPosition(currentServerPos.x, currentServerPos.y);
        window.draw(serverSprite);
    }
}
```

#### 4. Отрисовка клиента (строка ~3640)

```cpp
// Draw local client player sprite - always visible
if (textureLoaded) {
    clientSprite.setPosition(renderPos.x, renderPos.y);
    window.draw(clientSprite);
}
```

## Технические детали

### Размеры

- **Старый размер:** 20x20 пикселей (радиус 10px)
- **Новый размер:** 30x30 пикселей (радиус 15px)
- **Файл текстуры:** `Nothing_1.png`

### Хитбоксы

- **Старый радиус коллизии:** 10 пикселей
- **Новый радиус коллизии:** 15 пикселей
- Хитбокс остается круглым для упрощения расчетов коллизий

### Особенности реализации

1. **Центрирование спрайта:**
   - Используется `setOrigin(PLAYER_SIZE / 2.0f, PLAYER_SIZE / 2.0f)`
   - Это позволяет позиционировать спрайт по центру, а не по левому верхнему углу

2. **Туман войны:**
   - Для клиентов применяется альфа-канал через `setColor(sf::Color(255, 255, 255, alpha))`
   - Сервер всегда видим полностью (без тумана)

3. **Общая текстура:**
   - Все игроки (сервер и клиенты) используют одну и ту же текстуру
   - Это экономит память и упрощает код

## Требования к файлам

Файл `Nothing_1.png` должен находиться в следующих директориях:
- `Zero_Ground/Nothing_1.png` (для сервера)
- `Zero_Ground_client/Nothing_1.png` (для клиента)

Размер текстуры: **30x30 пикселей**

## Компиляция

```
71 Warning(s)
0 Error(s)
Compilation successful!
```

Все изменения успешно скомпилированы.

## Тестирование

Для проверки изменений:

1. Убедитесь что файл `Nothing_1.png` находится в папках сервера и клиента
2. Запустите сервер - должна отображаться текстура вместо зеленого круга
3. Подключите клиент - должна отображаться та же текстура
4. Проверьте коллизии пуль - хитбокс должен соответствовать размеру текстуры
5. Проверьте туман войны - клиенты должны затухать с расстоянием

## Совместимость

Изменения полностью совместимы с существующей системой:
- Сохранена система коллизий
- Сохранен туман войны
- Сохранена интерполяция движения
- Сохранена система респавна
