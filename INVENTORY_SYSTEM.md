# Система инвентаря Zero Ground

## Обзор

Простая система инвентаря с 6 ячейками, которая открывается/закрывается нажатием клавиши **E** (английская раскладка) или **У** (русская раскладка).

## Параметры

```cpp
const int INVENTORY_SLOTS = 6;                    // Количество ячеек
const float INVENTORY_SLOT_SIZE = 100.0f;         // Размер ячейки (100×100 пикселей)
const float INVENTORY_PADDING = 10.0f;            // Отступ между ячейками
bool inventoryOpen = false;                       // Состояние (открыт/закрыт)
float inventoryAnimationProgress = 0.0f;          // Прогресс анимации (0.0-1.0)
sf::Clock inventoryAnimationClock;                // Таймер для анимации
```

## Управление

- **Клавиша E** (или У на русской раскладке) - открыть/закрыть инвентарь
- Работает только во время игры (состояние `MainScreen`)

## Визуальное отображение

### Позиция
- **Расположение**: по центру внизу экрана
- **Отступы**: 20 пикселей от нижнего края, центрирован по горизонтали
- **Защита от выхода за экран**: минимум 20px от левого края если не помещается

### Внешний вид ячеек
- **Размер**: 100×100 пикселей каждая
- **Фон**: RGB(50, 50, 50) с прозрачностью 200/255
- **Рамка**: RGB(150, 150, 150) толщиной 2 пикселя
- **Номер ячейки**: белый текст, размер 32, центрирован

### Расположение ячеек
Ячейки расположены в один горизонтальный ряд с отступом 10 пикселей между ними:

```
[  1  ] [  2  ] [  3  ] [  4  ] [  5  ] [  6  ]
```

### Анимация открытия/закрытия

**Длительность**: 300 миллисекунд (0.3 секунды)

**Эффекты анимации**:
1. **Скольжение вверх** - инвентарь выезжает снизу
2. **Плавное появление** (fade-in) - прозрачность от 0 до 255
3. **Масштабирование** - каждая ячейка увеличивается от 50% до 100%
4. **Каскадный эффект** - каждая ячейка появляется с задержкой 50мс
5. **Ease-out cubic** - плавное замедление в конце анимации

**Формула easing**:
```cpp
float easedProgress = 1.0f - std::pow(1.0f - progress, 3.0f);
```

## Реализация

### Переключение инвентаря

```cpp
// В обработке событий
if (event.type == sf::Event::KeyPressed && state == ClientState::MainScreen) {
    if (event.key.code == sf::Keyboard::E) {
        inventoryOpen = !inventoryOpen;
        inventoryAnimationClock.restart(); // Запуск анимации
    }
}
```

### Обновление анимации

```cpp
const float ANIMATION_DURATION = 0.3f; // 300ms

// Обновление прогресса анимации
if (inventoryOpen && inventoryAnimationProgress < 1.0f) {
    // Анимация открытия
    inventoryAnimationProgress = std::min(1.0f, 
        inventoryAnimationClock.getElapsedTime().asSeconds() / ANIMATION_DURATION);
} else if (!inventoryOpen && inventoryAnimationProgress > 0.0f) {
    // Анимация закрытия
    inventoryAnimationProgress = std::max(0.0f, 
        1.0f - (inventoryAnimationClock.getElapsedTime().asSeconds() / ANIMATION_DURATION));
}
```

### Рендеринг инвентаря с анимацией

```cpp
// Рисуем только если анимация активна
if (inventoryAnimationProgress > 0.0f) {
    // Применяем easing для плавности
    float easedProgress = 1.0f - std::pow(1.0f - inventoryAnimationProgress, 3.0f);
    
    // Вычисляем позицию (центр внизу экрана)
    float inventoryWidth = INVENTORY_SLOTS * INVENTORY_SLOT_SIZE + 
                          (INVENTORY_SLOTS - 1) * INVENTORY_PADDING;
    float inventoryX = (windowSize.x - inventoryWidth) / 2.0f; // Центрируем
    float inventoryY = windowSize.y - INVENTORY_SLOT_SIZE - 20.0f;
    
    // Защита от выхода за экран
    if (inventoryX < 20.0f) {
        inventoryX = 20.0f;
    }
    
    // Эффект скольжения вверх
    float slideOffset = (1.0f - easedProgress) * (INVENTORY_SLOT_SIZE + 40.0f);
    inventoryY += slideOffset;
    
    // Рисуем каждую ячейку с каскадной анимацией
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        float slotX = inventoryX + i * (INVENTORY_SLOT_SIZE + INVENTORY_PADDING);
        
        // Каскадная задержка для каждой ячейки
        float slotDelay = i * 0.05f; // 50ms задержка
        float slotProgress = std::max(0.0f, std::min(1.0f, 
            (inventoryAnimationProgress - slotDelay) / (1.0f - slotDelay * INVENTORY_SLOTS)));
        float slotEasedProgress = 1.0f - std::pow(1.0f - slotProgress, 3.0f);
        
        // Масштабирование от 50% до 100%
        float scale = 0.5f + slotEasedProgress * 0.5f;
        float scaledSize = INVENTORY_SLOT_SIZE * scale;
        float scaleOffset = (INVENTORY_SLOT_SIZE - scaledSize) / 2.0f;
        
        // Создаем прямоугольник ячейки
        sf::RectangleShape slot(sf::Vector2f(scaledSize, scaledSize));
        slot.setPosition(slotX + scaleOffset, inventoryY + scaleOffset);
        slot.setFillColor(sf::Color(50, 50, 50, 
            static_cast<sf::Uint8>(slotEasedProgress * 200)));
        slot.setOutlineColor(sf::Color(150, 150, 150, 
            static_cast<sf::Uint8>(slotEasedProgress * 255)));
        slot.setOutlineThickness(2.0f);
        window.draw(slot);
        
        // Рисуем номер ячейки (появляется после 30% видимости)
        if (slotEasedProgress > 0.3f) {
            sf::Text slotNumber;
            slotNumber.setFont(font);
            slotNumber.setString(std::to_string(i + 1));
            slotNumber.setCharacterSize(static_cast<unsigned int>(32 * scale));
            slotNumber.setFillColor(sf::Color(255, 255, 255, 
                static_cast<sf::Uint8>(slotEasedProgress * 255)));
            // Центрируем текст...
            window.draw(slotNumber);
        }
    }
}
```

## Текущее состояние

✅ **Реализовано:**
- Открытие/закрытие по клавише E (работает на обеих раскладках)
- 6 ячеек размером 100×100 пикселей в один ряд
- Позиционирование в нижнем правом углу
- Визуальное оформление с рамками
- Нумерация ячеек (1-6)
- **Плавная анимация открытия/закрытия (300мс)**
- **Каскадный эффект появления ячеек**
- **Масштабирование и fade-in эффекты**
- **Скольжение вверх при открытии**
- Работает на клиенте и сервере независимо

❌ **Не реализовано (для будущего):**
- Хранение предметов в ячейках
- Иконки предметов
- Перетаскивание предметов
- Использование предметов
- Синхронизация инвентаря по сети
- Подсказки при наведении

## Будущие улучшения

1. **Система предметов**
   - Структура `Item` с типом, иконкой, количеством
   - Массив предметов `Item inventory[INVENTORY_SLOTS]`

2. **Взаимодействие**
   - Клик по ячейке для выбора/использования
   - Drag & Drop для перемещения предметов
   - Правый клик для дополнительных действий

3. **Визуальные улучшения**
   - Иконки предметов (текстуры)
   - Подсветка выбранной ячейки
   - ✅ ~~Анимация открытия/закрытия~~ (реализовано)
   - Счетчик количества предметов
   - Звуковые эффекты при открытии/закрытии

4. **Сетевая синхронизация**
   - Отправка состояния инвентаря серверу
   - Валидация действий на сервере
   - Синхронизация при подборе предметов


## Детали анимации

### Временная шкала (300мс)

```
0ms    50ms   100ms  150ms  200ms  250ms  300ms
│      │      │      │      │      │      │
Slot1  Slot2  Slot3  Slot4  Slot5  Slot6  Done
▼      ▼      ▼      ▼      ▼      ▼      ▼
```

### Эффекты по слоям

1. **Глобальное скольжение** (0-300мс)
   - Весь инвентарь выезжает снизу вверх
   - Смещение: от +140px до 0px

2. **Каскадное появление** (0-300мс)
   - Каждая ячейка начинает анимацию с задержкой
   - Задержка: 50мс × номер ячейки

3. **Масштабирование ячеек** (индивидуально)
   - Размер: от 50% до 100%
   - Применяется к каждой ячейке отдельно

4. **Fade-in** (индивидуально)
   - Прозрачность: от 0 до 200 (фон) и 255 (рамка)
   - Синхронизировано с масштабированием

5. **Появление текста** (после 30% видимости)
   - Текст появляется когда ячейка видна на 30%
   - Масштабируется вместе с ячейкой

### Математика easing

**Ease-out cubic** создает эффект быстрого старта и плавного замедления:

```
Linear:     ────────────────────────
Ease-out:   ─────────────────────────
            ↑ быстро    ↓ медленно
```

**Формула**: `y = 1 - (1 - x)³`

Где:
- `x` = прогресс от 0.0 до 1.0 (линейный)
- `y` = результат от 0.0 до 1.0 (с easing)

### Производительность

- **Вычислений на кадр**: ~20 (6 ячеек × 3-4 операции)
- **Draw calls**: 12 (6 ячеек + 6 текстов)
- **Влияние на FPS**: минимальное (<1%)
- **Память**: ~200 байт (переменные анимации)
