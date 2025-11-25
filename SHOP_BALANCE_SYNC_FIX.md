# Исправление синхронизации баланса в магазине

## Проблема
Баланс игрока (и сервера, и клиента) в магазине не синхронизировался с реальным балансом. При открытии магазина всегда отображался баланс $50,000 вместо текущего баланса игрока.

## Причина
В коде отрисовки Shop UI (как на сервере, так и на клиенте) использовался временный объект `currentPlayer` с захардкоженным балансом 50000:

### Серверный код (Zero_Ground/Zero_Ground.cpp, строка ~4900):
```cpp
// Draw shop UI with animation
// Requirements: 3.2, 3.3, 3.4, 3.5
if (shopAnimationProgress > 0.0f) {
    // Get current player for purchase status calculation
    Player currentPlayer;
    {
        std::lock_guard<std::mutex> lock(mutex);
        // Create a temporary player with current state
        currentPlayer.money = 50000;  // TODO: Get actual player money from server
        currentPlayer.inventory[0] = nullptr;  // TODO: Get actual inventory from server
        currentPlayer.inventory[1] = nullptr;
        currentPlayer.inventory[2] = nullptr;
        currentPlayer.inventory[3] = nullptr;
    }
    
    renderShopUI(window, currentPlayer, font, shopAnimationProgress);
}
```

### Клиентский код (Zero_Ground_client/Zero_Ground_client.cpp, строка ~3555):
```cpp
// Requirements: 3.2, 3.3, 3.4, 3.5
if (shopAnimationProgress > 0.0f) {
    // Get current player for purchase status calculation
    Player currentPlayer;
    {
        std::lock_guard<std::mutex> lock(mutex);
        // Create a temporary player with current state
        currentPlayer.money = 50000;  // TODO: Get actual player money from server
        currentPlayer.inventory[0] = nullptr;  // TODO: Get actual inventory from server
        currentPlayer.inventory[1] = nullptr;
        currentPlayer.inventory[2] = nullptr;
        currentPlayer.inventory[3] = nullptr;
    }
    
    renderShopUI(window, currentPlayer, font, shopAnimationProgress);
}
```

## Решение
Заменить временный объект `currentPlayer` на реальный объект игрока (`serverPlayer` на сервере и `clientPlayer` на клиенте).

### Исправленный серверный код:
```cpp
// Draw shop UI with animation
// Requirements: 3.2, 3.3, 3.4, 3.5
if (shopAnimationProgress > 0.0f) {
    // Use actual server player with real balance and inventory
    renderShopUI(window, serverPlayer, font, shopAnimationProgress);
}
```

### Исправленный клиентский код:
```cpp
// Requirements: 3.2, 3.3, 3.4, 3.5
if (shopAnimationProgress > 0.0f) {
    // Use actual client player with real balance and inventory
    renderShopUI(window, clientPlayer, font, shopAnimationProgress);
}
```

## Результат
После исправления:
1. В магазине отображается реальный баланс игрока
2. Баланс обновляется после каждой покупки
3. Статус покупки (можно купить / недостаточно средств / инвентарь полон) рассчитывается корректно на основе реального баланса
4. Инвентарь игрока также отображается корректно в магазине

## Тестирование
Для проверки исправления:
1. Скомпилируйте сервер и клиент
2. Запустите сервер и клиент
3. Откройте магазин (клавиша B рядом с красным квадратом магазина)
4. Проверьте, что отображается начальный баланс $50,000
5. Купите оружие
6. Проверьте, что баланс уменьшился на стоимость оружия
7. Проверьте, что статус покупки обновляется корректно (например, "Insufficient funds" если денег не хватает)

## Дополнительные улучшения
Баланс игрока также корректно отображается в HUD (левый верхний угол экрана) благодаря использованию `serverPlayer.money` и `clientPlayer.money` в соответствующих местах кода.

## Файлы изменены
- `Zero_Ground/Zero_Ground.cpp` - исправлена отрисовка Shop UI на сервере
- `Zero_Ground_client/Zero_Ground_client.cpp` - исправлена отрисовка Shop UI на клиенте
