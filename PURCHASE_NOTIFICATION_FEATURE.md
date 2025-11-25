# Визуальная обратная связь при покупке оружия

## Описание
Добавлена визуальная обратная связь при успешной покупке оружия в магазине. При покупке над слотом купленного оружия появляется надпись "Purchased" зеленого цвета, которая плавно перемещается вверх и постепенно исчезает, аналогично системе отображения урона.

## Реализация

### 1. Структура PurchaseText
Создана новая структура для хранения информации о тексте покупки:

```cpp
struct PurchaseText {
    float x = 0.0f;
    float y = 0.0f;
    std::string weaponName;
    sf::Clock lifetime;
    
    // Check if purchase text should be removed (after 1.5 seconds)
    bool shouldRemove() const {
        return lifetime.getElapsedTime().asSeconds() >= 1.5f;
    }
    
    // Get current Y position with upward animation
    float getAnimatedY() const {
        float elapsed = lifetime.getElapsedTime().asSeconds();
        // Move upward 60 pixels over 1.5 seconds
        return y - (elapsed * 40.0f);
    }
    
    // Get alpha for fade-out effect
    sf::Uint8 getAlpha() const {
        float elapsed = lifetime.getElapsedTime().asSeconds();
        // Fade out in last 0.5 seconds
        if (elapsed > 1.0f) {
            float fadeProgress = (elapsed - 1.0f) / 0.5f;
            return static_cast<sf::Uint8>(255 * (1.0f - fadeProgress));
        }
        return 255;
    }
};
```

### 2. Глобальные переменные
Добавлены глобальные переменные для хранения активных текстов покупок:

```cpp
// Store active purchase notification texts
std::vector<PurchaseText> purchaseTexts;
std::mutex purchaseTextsMutex;
```

### 3. Создание текста при покупке
При успешной покупке создается новый объект PurchaseText:

```cpp
if (success) {
    ErrorHandler::logInfo("Player purchased " + weapon->name);
    
    // Create purchase notification text
    {
        std::lock_guard<std::mutex> lock(purchaseTextsMutex);
        PurchaseText purchaseText;
        purchaseText.x = columnX + COLUMN_WIDTH / 2.0f;
        purchaseText.y = weaponY + WEAPON_HEIGHT / 2.0f;
        purchaseText.weaponName = weapon->name;
        purchaseTexts.push_back(purchaseText);
    }
}
```

### 4. Обновление и удаление текстов
Тексты автоматически удаляются после 1.5 секунд:

```cpp
// Update and remove expired purchase notification texts
{
    std::lock_guard<std::mutex> lock(purchaseTextsMutex);
    purchaseTexts.erase(
        std::remove_if(purchaseTexts.begin(), purchaseTexts.end(),
            [](const PurchaseText& pt) {
                return pt.shouldRemove();
            }),
        purchaseTexts.end()
    );
}
```

### 5. Отрисовка текстов
Тексты отрисовываются поверх Shop UI с анимацией движения вверх и затухания:

```cpp
// Render purchase notification texts on top of shop UI
{
    std::lock_guard<std::mutex> lock(purchaseTextsMutex);
    
    for (const auto& purchaseText : purchaseTexts) {
        // Get animated position and alpha
        float animatedY = purchaseText.getAnimatedY();
        sf::Uint8 textAlpha = purchaseText.getAlpha();
        
        // Create "Purchased" text
        sf::Text text;
        text.setFont(font);
        text.setString("Purchased");
        text.setCharacterSize(28);
        text.setFillColor(sf::Color(0, 255, 0, textAlpha)); // Green with alpha
        text.setStyle(sf::Text::Bold);
        
        // Center text at purchase location
        sf::FloatRect textBounds = text.getLocalBounds();
        text.setOrigin(textBounds.width / 2.0f, textBounds.height / 2.0f);
        text.setPosition(purchaseText.x, animatedY);
        
        window.draw(text);
    }
}
```

## Характеристики анимации

### Длительность
- **Общая длительность**: 1.5 секунды
- **Фаза полной видимости**: 1.0 секунда
- **Фаза затухания**: 0.5 секунды (с 1.0 до 1.5 секунды)

### Движение
- **Скорость**: 40 пикселей в секунду
- **Общее расстояние**: 60 пикселей вверх за 1.5 секунды
- **Начальная позиция**: центр слота купленного оружия

### Визуальные эффекты
- **Цвет**: Зеленый (0, 255, 0) - символизирует успешную покупку
- **Стиль**: Жирный шрифт (Bold)
- **Размер шрифта**: 28 пикселей
- **Прозрачность**: Плавное затухание от 255 (полностью видимый) до 0 (невидимый)

## Сравнение с системой отображения урона

| Характеристика | Урон (DamageText) | Покупка (PurchaseText) |
|----------------|-------------------|------------------------|
| Длительность | 1.0 секунда | 1.5 секунды |
| Скорость движения | 50 пикселей/сек | 40 пикселей/сек |
| Общее расстояние | 50 пикселей | 60 пикселей |
| Фаза затухания | 0.3 секунды | 0.5 секунды |
| Цвет | Красный | Зеленый |
| Текст | "-[урон]" | "Purchased" |

## Применение

Изменения применены к обоим компонентам:
- **Сервер**: `Zero_Ground/Zero_Ground.cpp`
- **Клиент**: `Zero_Ground_client/Zero_Ground_client.cpp`

## Тестирование

Для проверки функциональности:
1. Скомпилируйте сервер и клиент
2. Запустите игру
3. Откройте магазин (клавиша B рядом с красным квадратом)
4. Купите любое оружие
5. Проверьте, что появляется зеленая надпись "Purchased"
6. Убедитесь, что надпись плавно движется вверх
7. Проверьте, что надпись постепенно исчезает через ~1.5 секунды

## Преимущества

1. **Мгновенная обратная связь**: Игрок сразу видит, что покупка прошла успешно
2. **Визуальная привлекательность**: Анимация делает интерфейс более живым и отзывчивым
3. **Консистентность**: Использует тот же подход, что и система отображения урона
4. **Неинвазивность**: Текст не блокирует интерфейс и быстро исчезает
5. **Потокобезопасность**: Использует mutex для защиты от гонки данных

## Технические детали

### Потокобезопасность
Все операции с `purchaseTexts` защищены мьютексом `purchaseTextsMutex` для предотвращения гонки данных между потоками отрисовки и обновления.

### Производительность
- Минимальное влияние на производительность
- Тексты автоматически удаляются после истечения времени жизни
- Использует эффективный алгоритм `std::remove_if` для удаления

### Масштабируемость
Система может обрабатывать множественные покупки одновременно - каждая покупка создает свой независимый текст с собственной анимацией.
