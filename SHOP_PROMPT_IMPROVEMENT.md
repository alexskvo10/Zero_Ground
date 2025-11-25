# Улучшение подсказки магазина

## Описание
Улучшена система отображения подсказок для взаимодействия с магазином. Теперь подсказка динамически меняется в зависимости от состояния магазина:
- **Магазин закрыт** и игрок рядом → "Press B to purchase"
- **Магазин открыт** → "Press B to close"

## Проблема до изменения
Ранее было две отдельные подсказки:
1. "Press B to purchase" - отображалась когда игрок рядом с магазином
2. "Press B to close" - отображалась внутри Shop UI внизу окна

Это создавало визуальный шум и было неоптимально с точки зрения UX.

## Решение

### 1. Изменена функция `renderShopInteractionPrompt`
Добавлен параметр `shopUIOpen` для определения текущего состояния магазина:

```cpp
void renderShopInteractionPrompt(sf::RenderWindow& window, sf::Vector2f playerPosition, 
                                 const std::vector<Shop>& shops, const sf::Font& font, bool shopUIOpen)
```

### 2. Динамическое изменение текста
Текст подсказки теперь зависит от состояния магазина:

```cpp
// Display prompt based on shop state
if (nearShop || shopUIOpen) {
    // ...
    // Show different text based on shop state
    promptText.setString(shopUIOpen ? "Press B to close" : "Press B to purchase");
    // ...
}
```

### 3. Удалена дублирующая подсказка
Удален код отрисовки "Press B to close" из функции `renderShopUI`, так как теперь эта подсказка отображается через `renderShopInteractionPrompt`.

**До:**
```cpp
// Draw close instruction
sf::Text closeText;
closeText.setFont(font);
closeText.setString("Press B to close");
closeText.setCharacterSize(static_cast<unsigned int>(22 * scale));
closeText.setFillColor(sf::Color(200, 200, 200, static_cast<sf::Uint8>(easedProgress * 255)));
sf::FloatRect closeBounds = closeText.getLocalBounds();
closeText.setPosition(
    scaledX + (scaledWidth - closeBounds.width) / 2.0f - closeBounds.left,
    scaledY + scaledHeight - 40.0f * scale
);
window.draw(closeText);
```

**После:**
```cpp
// Код удален - подсказка теперь отображается через renderShopInteractionPrompt
```

### 4. Обновлены вызовы функции
Добавлен параметр `shopUIOpen` при вызове функции:

**Сервер:**
```cpp
renderShopInteractionPrompt(window, sf::Vector2f(serverPos.x, serverPos.y), shops, font, shopUIOpen);
```

**Клиент:**
```cpp
renderShopInteractionPrompt(window, renderPos, shops, font, shopUIOpen);
```

## Характеристики подсказки

### Позиционирование
- **Расположение**: Центр экрана по горизонтали, 120 пикселей от нижнего края
- **Выравнивание**: По центру текста

### Визуальные параметры
- **Размер шрифта**: 28 пикселей (одинаковый для обоих состояний)
- **Цвет текста**: Белый (255, 255, 255)
- **Обводка**: Черная, толщина 2 пикселя
- **Стиль**: Обычный (не жирный)

### Условия отображения
1. **"Press B to purchase"**:
   - Игрок находится в радиусе 60 пикселей от магазина
   - Магазин закрыт (`shopUIOpen == false`)

2. **"Press B to close"**:
   - Магазин открыт (`shopUIOpen == true`)
   - Отображается независимо от расстояния до магазина

## Преимущества

### 1. Улучшенный UX
- Одна подсказка вместо двух - меньше визуального шума
- Подсказка всегда в одном месте - легче найти
- Динамическое изменение текста - интуитивно понятно

### 2. Консистентность
- Одинаковый размер и стиль шрифта для обоих состояний
- Одинаковое позиционирование
- Единая система отображения подсказок

### 3. Код
- Меньше дублирования кода
- Централизованная логика отображения подсказок
- Легче поддерживать и изменять

### 4. Производительность
- Меньше операций отрисовки текста
- Один текстовый элемент вместо двух

## Применение

Изменения применены к обоим компонентам:
- **Сервер**: `Zero_Ground/Zero_Ground.cpp`
- **Клиент**: `Zero_Ground_client/Zero_Ground_client.cpp`

## Тестирование

Для проверки функциональности:
1. Скомпилируйте сервер и клиент
2. Запустите игру
3. Подойдите к магазину (красный квадрат)
4. Проверьте, что появляется подсказка "Press B to purchase"
5. Нажмите B для открытия магазина
6. Проверьте, что подсказка изменилась на "Press B to close"
7. Проверьте, что подсказка находится в том же месте (120px от низа экрана)
8. Проверьте, что размер и стиль шрифта одинаковые
9. Нажмите B для закрытия магазина
10. Проверьте, что подсказка вернулась к "Press B to purchase"

## Технические детали

### Сигнатура функции
```cpp
void renderShopInteractionPrompt(
    sf::RenderWindow& window,      // Окно для отрисовки
    sf::Vector2f playerPosition,   // Позиция игрока
    const std::vector<Shop>& shops, // Список магазинов
    const sf::Font& font,          // Шрифт для текста
    bool shopUIOpen                // Состояние магазина (открыт/закрыт)
)
```

### Логика отображения
```cpp
// Проверяем, рядом ли игрок с магазином
bool nearShop = false;
for (const auto& shop : shops) {
    if (shop.isPlayerNear(playerPosition.x, playerPosition.y)) {
        nearShop = true;
        break;
    }
}

// Отображаем подсказку если:
// - Игрок рядом с магазином ИЛИ
// - Магазин открыт
if (nearShop || shopUIOpen) {
    // Выбираем текст в зависимости от состояния
    promptText.setString(shopUIOpen ? "Press B to close" : "Press B to purchase");
    // ... отрисовка
}
```

## Совместимость

Изменения полностью обратно совместимы и не влияют на:
- Логику открытия/закрытия магазина
- Систему покупок
- Другие UI элементы
- Производительность игры

## Будущие улучшения

Возможные дальнейшие улучшения:
1. Анимация появления/исчезновения подсказки
2. Разные цвета для разных состояний (например, зеленый для "purchase", красный для "close")
3. Иконка клавиши B рядом с текстом
4. Локализация текста на разные языки
5. Звуковая обратная связь при изменении подсказки
