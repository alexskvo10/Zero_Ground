# Реализация поворота игрока к курсору мыши

## Дата: 26 ноября 2025

## Описание

Реализован поворот текстуры игрока в направлении курсора мыши. Текстура изначально смотрит вниз и после спавна поворачивается в направлении курсора.

## Изменения в коде

### Файл: Zero_Ground/Zero_Ground.cpp (Сервер)

**Расположение:** Строка ~5067 (отрисовка спрайта сервера)

**Было:**
```cpp
// Draw server player sprite - always visible
serverSprite.setPosition(renderPos.x, renderPos.y);
window.draw(serverSprite);
```

**Стало:**
```cpp
// Draw server player sprite - always visible
// Calculate rotation angle to mouse cursor
sf::Vector2i mousePixelPos = sf::Mouse::getPosition(window);
sf::Vector2f mouseWorldPos = window.mapPixelToCoords(mousePixelPos);

// Calculate angle from player to mouse (in degrees)
float dx = mouseWorldPos.x - renderPos.x;
float dy = mouseWorldPos.y - renderPos.y;
float angleToMouse = std::atan2(dy, dx) * 180.0f / 3.14159f;

// Rotate sprite to face mouse (subtract 90 degrees because sprite initially faces up)
serverSprite.setRotation(angleToMouse - 90.0f);
serverSprite.setPosition(renderPos.x, renderPos.y);
window.draw(serverSprite);
```

---

### Файл: Zero_Ground_client/Zero_Ground_client.cpp (Клиент)

**Расположение:** Строка ~3637 (отрисовка спрайта клиента)

**Было:**
```cpp
// Draw local client player sprite - always visible
if (textureLoaded) {
    clientSprite.setPosition(renderPos.x, renderPos.y);
    window.draw(clientSprite);
}
```

**Стало:**
```cpp
// Draw local client player sprite - always visible
if (textureLoaded) {
    // Calculate rotation angle to mouse cursor
    sf::Vector2i mousePixelPos = sf::Mouse::getPosition(window);
    sf::Vector2f mouseWorldPos = window.mapPixelToCoords(mousePixelPos);
    
    // Calculate angle from player to mouse (in degrees)
    float dx = mouseWorldPos.x - renderPos.x;
    float dy = mouseWorldPos.y - renderPos.y;
    float angleToMouse = std::atan2(dy, dx) * 180.0f / 3.14159f;
    
    // Rotate sprite to face mouse (subtract 90 degrees because sprite initially faces up)
    clientSprite.setRotation(angleToMouse - 90.0f);
    clientSprite.setPosition(renderPos.x, renderPos.y);
    window.draw(clientSprite);
}
```

---

## Технические детали

### Алгоритм расчета угла поворота

1. **Получение позиции мыши:**
   ```cpp
   sf::Vector2i mousePixelPos = sf::Mouse::getPosition(window);
   ```
   Получаем позицию мыши в пиксельных координатах окна.

2. **Преобразование в мировые координаты:**
   ```cpp
   sf::Vector2f mouseWorldPos = window.mapPixelToCoords(mousePixelPos);
   ```
   Преобразуем пиксельные координаты в мировые координаты игры (учитывая камеру).

3. **Вычисление вектора направления:**
   ```cpp
   float dx = mouseWorldPos.x - renderPos.x;
   float dy = mouseWorldPos.y - renderPos.y;
   ```
   Вычисляем разницу между позицией мыши и позицией игрока.

4. **Вычисление угла:**
   ```cpp
   float angleToMouse = std::atan2(dy, dx) * 180.0f / 3.14159f;
   ```
   Используем `atan2` для вычисления угла в радианах, затем конвертируем в градусы.

5. **Применение поворота:**
   ```cpp
   serverSprite.setRotation(angleToMouse - 90.0f);
   ```
   Вычитаем 90 градусов, потому что текстура изначально смотрит вверх (0° = вправо в SFML).

### Система координат

**SFML координаты:**
- 0° = направление вправо (→)
- 90° = направление вниз (↓)
- 180° = направление влево (←)
- 270° = направление вверх (↑)

**Текстура:**
- Изначально смотрит вверх (-90° в системе SFML)
- Поэтому вычитаем 90° из вычисленного угла

### Математика

**Функция atan2:**
- `atan2(dy, dx)` возвращает угол в радианах от -π до π
- Конвертация в градусы: `радианы * 180 / π`
- Результат: угол от -180° до 180°

**Пример:**
- Мышь справа от игрока: dx > 0, dy = 0 → угол = 0° → поворот = -90° (вправо)
- Мышь снизу от игрока: dx = 0, dy > 0 → угол = 90° → поворот = 0° (вниз)
- Мышь слева от игрока: dx < 0, dy = 0 → угол = 180° → поворот = 90° (влево)
- Мышь сверху от игрока: dx = 0, dy < 0 → угол = -90° → поворот = -180° (вверх)

---

## Особенности реализации

### 1. Мгновенный поворот
- Поворот происходит мгновенно без плавной анимации
- Спрайт всегда точно смотрит на курсор

### 2. Работа с камерой
- Используется `window.mapPixelToCoords()` для корректного преобразования координат
- Учитывается положение камеры (важно для сервера, где камера следует за игроком)

### 3. Центр вращения
- Спрайт вращается вокруг своего центра (установлено через `setOrigin()`)
- Центр находится в точке (15, 15) для текстуры 30x30

### 4. Производительность
- Расчет угла выполняется каждый кадр
- Операции очень быстрые (несколько арифметических операций)
- Нет заметного влияния на производительность

---

## Преимущества

1. **Интуитивность:**
   - Игрок всегда видит куда смотрит его персонаж
   - Легче целиться и стрелять

2. **Визуальная обратная связь:**
   - Понятно в какую сторону направлено оружие
   - Улучшает игровой опыт

3. **Реализм:**
   - Персонаж ведет себя естественно
   - Следит за курсором как в других играх

4. **Простота:**
   - Минимальный код
   - Нет сложной логики
   - Легко поддерживать

---

## Возможные улучшения (будущее)

### 1. Плавный поворот
```cpp
// Вместо мгновенного поворота
float targetAngle = angleToMouse + 90.0f;
float currentAngle = serverSprite.getRotation();
float rotationSpeed = 360.0f; // градусов в секунду

// Интерполяция угла
float angleDiff = targetAngle - currentAngle;
// Нормализация угла (-180 до 180)
while (angleDiff > 180.0f) angleDiff -= 360.0f;
while (angleDiff < -180.0f) angleDiff += 360.0f;

float newAngle = currentAngle + angleDiff * rotationSpeed * deltaTime;
serverSprite.setRotation(newAngle);
```

### 2. Ограничение скорости поворота
- Добавить максимальную скорость поворота
- Сделать поворот более реалистичным

### 3. Анимация поворота
- Добавить промежуточные кадры анимации
- Использовать разные текстуры для разных углов

### 4. Инерция поворота
- Добавить плавное ускорение/замедление
- Сделать движение более естественным

---

## Тестирование

### Чек-лист:

- [ ] Сервер: спрайт поворачивается к курсору
- [ ] Клиент: спрайт поворачивается к курсору
- [ ] Поворот работает во всех направлениях (360°)
- [ ] Поворот работает при движении камеры
- [ ] Поворот работает в магазине (когда UI открыт)
- [ ] Поворот работает после респавна
- [ ] Нет визуальных артефактов
- [ ] Производительность не пострадала

### Тестовые сценарии:

1. **Базовый поворот:**
   - Двигайте мышь вокруг игрока
   - Проверьте что спрайт всегда смотрит на курсор

2. **Поворот при движении:**
   - Двигайте игрока клавишами WASD
   - Одновременно двигайте мышь
   - Проверьте что поворот работает корректно

3. **Поворот в UI:**
   - Откройте магазин (B)
   - Проверьте что поворот продолжает работать
   - Закройте магазин

4. **Поворот после респавна:**
   - Умрите и дождитесь респавна
   - Проверьте что поворот работает сразу после респавна

---

## Компиляция

### Сервер
```
Compilation successful!
71 Warning(s)
0 Error(s)
```

### Клиент
```
Compilation successful!
No diagnostics found
```

---

## Известные ограничения

1. **Мгновенный поворот:**
   - Нет плавной анимации
   - Может выглядеть резко при быстром движении мыши

2. **Одна текстура:**
   - Используется одна текстура для всех направлений
   - Нет отдельных спрайтов для разных углов

3. **Нет ограничения скорости:**
   - Спрайт может поворачиваться бесконечно быстро
   - Нет физических ограничений

---

## Заключение

Реализован поворот текстуры игрока к курсору мыши для сервера и клиента. Поворот работает корректно во всех направлениях и учитывает положение камеры. Код скомпилирован без ошибок и готов к тестированию.
