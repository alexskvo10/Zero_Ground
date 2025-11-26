# Замена пуль на текстуру

## Дата: 26 ноября 2025

## Описание

Заменена отрисовка пуль с белых линий (5px длина, 2px ширина) на текстуру `Bullet.png`.

## Изменения в коде

### Файл: Zero_Ground/Zero_Ground.cpp (Сервер)

#### 1. Загрузка текстуры пули (строка ~4095)

**Добавлено:**
```cpp
// Load bullet texture
sf::Texture bulletTexture;
if (!bulletTexture.loadFromFile("Bullet.png")) {
    std::cerr << "Failed to load bullet texture Bullet.png!" << std::endl;
    return -1;
}
```

#### 2. Отрисовка пуль (строка ~4990)

**Было:**
```cpp
// Requirement 7.1: Render bullets as white lines (5px length, 2px width)
{
    std::lock_guard<std::mutex> lock(bulletsMutex);
    
    for (const auto& bullet : activeBullets) {
        // ... fog calculation
        
        // Create line from bullet position extending 5px in direction of travel
        sf::Vertex line[] = {
            sf::Vertex(sf::Vector2f(bullet.x, bullet.y), sf::Color(255, 255, 255, alpha)),
            sf::Vertex(sf::Vector2f(bullet.x + dirX * 5.0f, bullet.y + dirY * 5.0f), sf::Color(255, 255, 255, alpha))
        };
        
        window.draw(line, 2, sf::Lines);
    }
}
```

**Стало:**
```cpp
// Requirement 7.1: Render bullets as sprites with texture
{
    std::lock_guard<std::mutex> lock(bulletsMutex);
    
    // Create bullet sprite (reused for all bullets)
    sf::Sprite bulletSprite;
    bulletSprite.setTexture(bulletTexture);
    
    // Get texture size and set origin to center
    sf::Vector2u bulletTexSize = bulletTexture.getSize();
    bulletSprite.setOrigin(bulletTexSize.x / 2.0f, bulletTexSize.y / 2.0f);
    
    for (const auto& bullet : activeBullets) {
        // ... fog calculation
        
        // Calculate rotation angle (bullet points in direction of travel)
        float angle = std::atan2(dirY, dirX) * 180.0f / 3.14159f;
        
        // Apply fog, position and rotation to bullet sprite
        bulletSprite.setColor(sf::Color(255, 255, 255, alpha));
        bulletSprite.setPosition(bullet.x, bullet.y);
        bulletSprite.setRotation(angle);
        
        window.draw(bulletSprite);
    }
}
```

---

### Файл: Zero_Ground_client/Zero_Ground_client.cpp (Клиент)

#### 1. Загрузка текстуры пули (строка ~3522)

**Было:**
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
        // ... setup sprites
        textureLoaded = true;
    }
}
```

**Стало:**
```cpp
// Load textures (once)
static sf::Texture playerTexture;
static sf::Texture bulletTexture;
static sf::Sprite serverSprite;
static sf::Sprite clientSprite;
static bool textureLoaded = false;

if (!textureLoaded) {
    if (!playerTexture.loadFromFile("Nothing_1.png")) {
        std::cerr << "Failed to load player texture Nothing_1.png!" << std::endl;
    } else if (!bulletTexture.loadFromFile("Bullet.png")) {
        std::cerr << "Failed to load bullet texture Bullet.png!" << std::endl;
    } else {
        // ... setup sprites
        textureLoaded = true;
    }
}
```

#### 2. Отрисовка пуль (строка ~3560)

Аналогично серверу - заменены линии на спрайты с текстурой.

---

## Технические детали

### Алгоритм отрисовки пули

1. **Создание спрайта:**
   ```cpp
   sf::Sprite bulletSprite;
   bulletSprite.setTexture(bulletTexture);
   ```

2. **Центрирование:**
   ```cpp
   sf::Vector2u bulletTexSize = bulletTexture.getSize();
   bulletSprite.setOrigin(bulletTexSize.x / 2.0f, bulletTexSize.y / 2.0f);
   ```
   Устанавливаем центр спрайта в центр текстуры для правильного вращения.

3. **Вычисление направления:**
   ```cpp
   float speed = std::sqrt(bullet.vx * bullet.vx + bullet.vy * bullet.vy);
   float dirX = (speed > 0.001f) ? bullet.vx / speed : 1.0f;
   float dirY = (speed > 0.001f) ? bullet.vy / speed : 0.0f;
   ```

4. **Вычисление угла поворота:**
   ```cpp
   float angle = std::atan2(dirY, dirX) * 180.0f / 3.14159f;
   ```
   Используем `atan2` для вычисления угла направления пули.

5. **Применение параметров:**
   ```cpp
   bulletSprite.setColor(sf::Color(255, 255, 255, alpha)); // Туман войны
   bulletSprite.setPosition(bullet.x, bullet.y);           // Позиция
   bulletSprite.setRotation(angle);                        // Поворот
   ```

6. **Отрисовка:**
   ```cpp
   window.draw(bulletSprite);
   ```

### Особенности реализации

1. **Переиспользование спрайта:**
   - Создается один спрайт для всех пуль
   - Для каждой пули меняются только параметры (позиция, поворот, цвет)
   - Экономит память и улучшает производительность

2. **Туман войны:**
   - Применяется через альфа-канал `setColor()`
   - Пули затухают с расстоянием как и другие объекты

3. **Поворот:**
   - Пуля автоматически поворачивается в направлении полета
   - Использует вектор скорости (vx, vy) для определения направления

4. **Центрирование:**
   - Спрайт вращается вокруг своего центра
   - Позиция пули соответствует центру спрайта

### Преимущества новой реализации

1. **Визуальное улучшение:**
   - Вместо простых линий используется текстура
   - Более детализированное представление пуль
   - Можно использовать разные текстуры для разных типов оружия

2. **Гибкость:**
   - Легко заменить текстуру без изменения кода
   - Можно добавить анимацию пули
   - Можно масштабировать текстуру

3. **Консистентность:**
   - Все объекты (игроки, пули) теперь используют текстуры
   - Единый стиль визуализации

4. **Производительность:**
   - Один спрайт переиспользуется для всех пуль
   - Эффективное применение тумана войны

---

## Файлы текстур

### Расположение:
- `Zero_Ground/Bullet.png` ✅
- `Zero_Ground_client/Bullet.png` ✅

### Требования к текстуре:
- Формат: PNG (с прозрачностью)
- Рекомендуемый размер: 10-20 пикселей
- Ориентация: Текстура должна "смотреть" вправо (→)
  - 0° в SFML = направление вправо
  - Текстура будет автоматически поворачиваться в направлении полета

---

## Компиляция

**Сервер:** ✅ 0 ошибок  
**Клиент:** ✅ 0 ошибок  

---

## Тестирование

### Чек-лист:

- [ ] Файл `Bullet.png` в папке сервера
- [ ] Файл `Bullet.png` в папке клиента
- [ ] Пули отображаются с текстурой вместо линий
- [ ] Пули поворачиваются в направлении полета
- [ ] Туман войны применяется к пулям
- [ ] Пули видны на всех дистанциях (в пределах тумана)
- [ ] Нет визуальных артефактов
- [ ] Производительность не пострадала

### Тестовые сценарии:

1. **Базовая стрельба:**
   - Стреляйте в разных направлениях
   - Проверьте что пули отображаются с текстурой
   - Проверьте что пули поворачиваются правильно

2. **Туман войны:**
   - Стреляйте на большие расстояния
   - Проверьте что пули затухают с расстоянием
   - Проверьте что пули исчезают за пределами видимости

3. **Разные типы оружия:**
   - Стреляйте из разных видов оружия
   - Проверьте что все пули отображаются корректно
   - Проверьте скорость и направление пуль

4. **Производительность:**
   - Создайте много пуль одновременно (автоматическое оружие)
   - Проверьте что FPS не падает
   - Проверьте что нет задержек

---

## Возможные улучшения (будущее)

### 1. Разные текстуры для разных оружий
```cpp
// В структуре Bullet добавить:
sf::Texture* texture;

// При создании пули:
bullet.texture = &pistolBulletTexture; // или rifleTexture, sniperTexture и т.д.

// При отрисовке:
bulletSprite.setTexture(*bullet.texture);
```

### 2. Анимация пули
- Использовать спрайт-лист с несколькими кадрами
- Анимировать вращение или свечение пули

### 3. След от пули
- Добавить эффект следа (trail effect)
- Использовать частицы или дополнительные спрайты

### 4. Масштабирование
- Масштабировать пули в зависимости от типа оружия
- Снайперские пули больше, пистолетные меньше

---

## Известные ограничения

1. **Одна текстура для всех пуль:**
   - Все пули выглядят одинаково
   - Нет различия между типами оружия

2. **Статическая текстура:**
   - Нет анимации
   - Нет эффектов (свечение, вращение)

3. **Размер текстуры:**
   - Размер фиксирован
   - Нет масштабирования в зависимости от оружия

---

## Заключение

Успешно заменена отрисовка пуль с белых линий на текстуру `Bullet.png`. Пули теперь отображаются с текстурой, автоматически поворачиваются в направлении полета и корректно работают с туманом войны. Код скомпилирован без ошибок и готов к тестированию.
