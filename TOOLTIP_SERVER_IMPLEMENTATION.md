# Применение tooltip к серверу

## Обзор
Все изменения tooltip, сделанные для клиента, теперь применены и к серверу. Игрок на сервере также может видеть всплывающие подсказки с характеристиками оружия при наведении курсора в магазине.

## Изменения в серверном коде

### Файл: `Zero_Ground/Zero_Ground.cpp`

#### 1. Добавлен include
```cpp
#include <iomanip>  // Для форматирования float чисел
```

#### 2. Добавлена функция `renderWeaponTooltip` (строка ~2188)
Полная копия функции из клиента с теми же параметрами:
- **Размер:** 320×290 пикселей
- **Позиционирование:** Адаптивное с резервом 150px снизу
- **Форматирование:** Float числа с точностью 1 знак после запятой
- **Дизайн:** Золотая рамка, структурированная информация

```cpp
void renderWeaponTooltip(sf::RenderWindow& window, const Weapon* weapon, 
                         float mouseX, float mouseY, const sf::Font& font) {
    // Tooltip dimensions
    const float TOOLTIP_WIDTH = 320.0f;
    const float TOOLTIP_HEIGHT = 290.0f;
    const float PADDING = 15.0f;
    const float BOTTOM_UI_RESERVE = 150.0f;
    
    // ... (полная реализация)
}
```

#### 3. Модифицирована функция `renderShopUI` (строка ~2300)

**Добавлены переменные для хранения наведённого оружия:**
```cpp
Weapon* hoveredWeapon = nullptr;
float hoveredMouseX = 0.0f;
float hoveredMouseY = 0.0f;
```

**Изменена логика в цикле отрисовки оружия:**
```cpp
// Проверка наведения
sf::Vector2i mousePixelPos = sf::Mouse::getPosition(window);
sf::FloatRect weaponBounds(
    columnX + 10.0f * scale,
    weaponY,
    COLUMN_WIDTH - 20.0f * scale,
    WEAPON_HEIGHT
);

if (weaponBounds.contains(mousePos)) {
    if (hoveredWeapon != nullptr) {
        delete hoveredWeapon;
    }
    hoveredWeapon = weapon;
    hoveredMouseX = static_cast<float>(mousePixelPos.x);
    hoveredMouseY = static_cast<float>(mousePixelPos.y);
} else {
    delete weapon;
}
```

**Добавлена отрисовка tooltip в конце:**
```cpp
// Render tooltip at the very end
if (hoveredWeapon != nullptr) {
    renderWeaponTooltip(window, hoveredWeapon, hoveredMouseX, hoveredMouseY, font);
    delete hoveredWeapon;
}
```

## Идентичность с клиентом

### Одинаковые параметры
| Параметр | Значение |
|----------|----------|
| TOOLTIP_WIDTH | 320px |
| TOOLTIP_HEIGHT | 290px |
| PADDING | 15px |
| BOTTOM_UI_RESERVE | 150px |

### Одинаковое поведение
- ✅ Адаптивное позиционирование
- ✅ Резерв снизу для UI элементов
- ✅ Форматирование float с 1 знаком
- ✅ Z-order (tooltip поверх всех элементов)
- ✅ Золотая рамка и цветовая схема

### Одинаковые характеристики
Tooltip показывает те же 8 характеристик:
1. Damage (урон)
2. Magazine (размер магазина)
3. Reserve Ammo (запас патронов)
4. Range (дальность)
5. Bullet Speed (скорость пули)
6. Reload Time (время перезарядки)
7. Movement Speed (скорость передвижения)
8. Fire Mode (режим огня)

## Тестирование

### Для сервера
1. Скомпилируйте сервер: `compile_server.bat`
2. Запустите сервер: `Zero_Ground\Debug\Zero_Ground.exe`
3. Нажмите PLAY для начала игры
4. Подойдите к магазину (красный квадрат)
5. Нажмите **B** для открытия магазина
6. Наведите курсор на любое оружие
7. Проверьте, что tooltip отображается корректно

### Проверить
- [ ] Tooltip появляется при наведении
- [ ] Tooltip не перекрывает "Press B to close"
- [ ] Tooltip не перекрывает "E - inventory"
- [ ] Числа форматируются правильно (3.0, а не 3.000000)
- [ ] Текст "Automatic (10 rps)" полностью влезает
- [ ] Tooltip позиционируется корректно во всех углах экрана

## Синхронизация клиента и сервера

### Общий код
Функции `renderWeaponTooltip` и логика в `renderShopUI` идентичны в обоих файлах:
- `Zero_Ground_client/Zero_Ground_client.cpp`
- `Zero_Ground/Zero_Ground.cpp`

### Преимущества
- Одинаковый пользовательский опыт для игрока на сервере и клиенте
- Легче поддерживать и обновлять (изменения применяются к обоим)
- Консистентность UI во всей игре

## Статус

### Клиент
- [x] Код написан
- [x] Синтаксис проверен
- [x] Документация создана
- [ ] Компиляция
- [ ] Тестирование

### Сервер
- [x] Код написан
- [x] Синтаксис проверен
- [x] Документация создана
- [ ] Компиляция
- [ ] Тестирование

## Следующие шаги

1. Скомпилировать клиент и сервер
2. Протестировать tooltip на обоих
3. Убедиться, что поведение идентично
4. Проверить производительность

---

**Дата:** 25 ноября 2025  
**Версия:** 1.2 (с высотой 290px)  
**Статус:** ✅ Готово к компиляции
