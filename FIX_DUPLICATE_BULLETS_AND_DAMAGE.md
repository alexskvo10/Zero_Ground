# Исправление дублирования пуль и текста урона

## Проблема
Клиент видел дублирование:
1. **2 пули вместо 1** при выстреле
2. **2 текста урона** при попадании

## Причина
1. **Дублирование пуль**: Клиент создавал пулю локально при выстреле, а затем получал ту же пулю от сервера через ShotPacket
2. **Дублирование урона**: Клиент проверял столкновения локально и создавал текст урона, а сервер также должен был отправлять HitPacket (но не отправлял)

## Решение

### Изменения в клиенте (Zero_Ground_client/Zero_Ground_client.cpp)

#### 1. Убрано локальное создание пули при выстреле
**Строки ~2605-2625**: Удален код, который добавлял пулю в `activeBullets` сразу после выстрела.

**Было:**
```cpp
// Fire weapon (consumes ammo)
activeWeapon->fire();

// Add bullet to active bullets list
{
    std::lock_guard<std::mutex> lock(bulletsMutex);
    // ... добавление пули локально
    activeBullets.push_back(bullet);
}

// Send shot packet to server
```

**Стало:**
```cpp
// Fire weapon (consumes ammo)
activeWeapon->fire();

// NOTE: Don't create bullet locally - wait for server to send it back
// This prevents duplicate bullets (local + server response)

// Send shot packet to server
```

Теперь клиент только отправляет ShotPacket на сервер и ждет, когда сервер вернет пулю обратно.

#### 2. Убрана локальная проверка столкновений пуль
**Строки ~2966-3040**: Удален весь код проверки столкновений пуль с игроками на клиенте.

**Было:**
```cpp
// Check collision with client player (local player)
for (auto& bullet : activeBullets) {
    // ... проверка столкновений
    // ... создание текста урона
}

// Check collision with server player
for (auto& bullet : activeBullets) {
    // ... проверка столкновений
    // ... создание текста урона
}
```

**Стало:**
```cpp
// NOTE: Collision detection is now handled by server only
// Server will send HitPacket when bullet hits a player
// This prevents duplicate damage text (local detection + server notification)
// Client only renders bullets, server handles all game logic
```

#### 3. Добавлена обработка HitPacket от сервера
**Строки ~1936-1970**: Добавлен новый обработчик для HitPacket в UDP потоке.

```cpp
else if (received == sizeof(HitPacket)) {
    // Handle hit packet from server
    HitPacket* hitPacket = reinterpret_cast<HitPacket*>(buffer);
    
    ErrorHandler::logInfo("Received hit packet! Shooter: " + std::to_string(hitPacket->shooterId) + 
                         ", Victim: " + std::to_string(hitPacket->victimId) + 
                         ", Damage: " + std::to_string(hitPacket->damage));
    
    // Create damage text at hit location
    {
        std::lock_guard<std::mutex> lock(damageTextsMutex);
        DamageText damageText;
        damageText.x = hitPacket->hitX;
        damageText.y = hitPacket->hitY - 30.0f;
        damageText.damage = hitPacket->damage;
        damageTexts.push_back(damageText);
    }
    
    // Mark bullet for removal at hit location
    {
        std::lock_guard<std::mutex> lock(bulletsMutex);
        for (auto& bullet : activeBullets) {
            if (bullet.ownerId == hitPacket->shooterId) {
                float dx = bullet.x - hitPacket->hitX;
                float dy = bullet.y - hitPacket->hitY;
                float distSq = dx * dx + dy * dy;
                if (distSq < 100.0f) { // Within 10 pixels
                    bullet.range = 0.0f; // Mark for removal
                    break;
                }
            }
        }
    }
}
```

### Изменения на сервере (Zero_Ground/Zero_Ground.cpp)

#### 1. Добавлена отправка HitPacket при попадании в игрока из GameState
**Строки ~4220-4255**: Добавлена отправка HitPacket всем клиентам при попадании.

```cpp
// Requirement 8.3: Check for player death
bool wasKill = false;
if (gameState.isPlayerDead(player.id)) {
    gameState.setPlayerAlive(player.id, false);
    wasKill = true;
    // ... награда за убийство
}

// Requirement 10.4: Send hit packet to all clients
HitPacket hitPacket;
hitPacket.shooterId = bullet.ownerId;
hitPacket.victimId = player.id;
hitPacket.damage = bullet.damage;
hitPacket.hitX = player.x;
hitPacket.hitY = player.y;
hitPacket.wasKill = wasKill;

// Broadcast to all connected clients
for (const auto& client : connectedClients) {
    if (client.socket && client.isReady) {
        udpSocket.send(&hitPacket, sizeof(HitPacket), client.address, 53002);
    }
}
```

#### 2. Добавлена отправка HitPacket при попадании в сервер
**Строки ~4155-4195**: Добавлена отправка HitPacket при попадании в серверного игрока.

```cpp
// Requirement 8.3: Check for player death
bool wasKill = false;
if (serverHealth <= 0.0f) {
    // ... обработка смерти
    wasKill = true;
}

// Requirement 10.4: Send hit packet to all clients
HitPacket hitPacket;
hitPacket.shooterId = bullet.ownerId;
hitPacket.victimId = 1; // Server is player 1
hitPacket.damage = bullet.damage;
hitPacket.hitX = serverPos.x;
hitPacket.hitY = serverPos.y;
hitPacket.wasKill = wasKill;

// Broadcast to all connected clients
for (const auto& client : connectedClients) {
    if (client.socket && client.isReady) {
        udpSocket.send(&hitPacket, sizeof(HitPacket), client.address, 53002);
    }
}
```

#### 3. Добавлена отправка HitPacket при попадании в клиента (режим 2 игроков)
**Строки ~4290-4310**: Добавлена отправка HitPacket при попадании в клиента.

```cpp
ErrorHandler::logInfo("Client player hit! Damage: " + std::to_string(bullet.damage) + 
                     ", Health: " + std::to_string(oldHealth) + " -> " + std::to_string(clientHealth));

// Requirement 8.2: Create damage text visualization on server
{
    std::lock_guard<std::mutex> lock(damageTextsMutex);
    DamageText damageText;
    damageText.x = clientPos.x;
    damageText.y = clientPos.y - 30.0f; // Start above player
    damageText.damage = bullet.damage;
    damageTexts.push_back(damageText);
}

// Check if client died
bool wasKill = false;
if (clientHealth <= 0.0f && clientIsAlive) {
    clientIsAlive = false;
    clientWaitingRespawn = true;
    wasKill = true;
    // ... обработка смерти
}

// Requirement 10.4: Send hit packet to client
HitPacket hitPacket;
hitPacket.shooterId = bullet.ownerId;
hitPacket.victimId = 0; // Client is player 0
hitPacket.damage = bullet.damage;
hitPacket.hitX = clientPos.x;
hitPacket.hitY = clientPos.y;
hitPacket.wasKill = wasKill;

// Send to client
for (const auto& client : connectedClients) {
    if (client.socket && client.isReady) {
        udpSocket.send(&hitPacket, sizeof(HitPacket), client.address, 53002);
    }
}
```

## Архитектура решения

### Поток данных для выстрела:
1. Клиент нажимает ЛКМ
2. Клиент отправляет ShotPacket на сервер
3. Сервер получает ShotPacket и создает пулю
4. Сервер отправляет ShotPacket всем клиентам (включая отправителя)
5. Все клиенты создают визуальную пулю

### Поток данных для попадания:
1. Сервер обновляет позиции пуль
2. Сервер проверяет столкновения пуль с игроками
3. При попадании сервер:
   - Применяет урон
   - Отправляет HitPacket всем клиентам
4. Клиенты получают HitPacket и:
   - Создают текст урона в позиции попадания
   - Помечают пулю для удаления

## Преимущества
- **Единственный источник истины**: Сервер - единственный, кто решает, попала ли пуля
- **Нет дублирования**: Каждая пуля и каждый текст урона создается только один раз
- **Синхронизация**: Все клиенты видят одинаковую картину
- **Защита от читов**: Клиент не может подделать попадания

## Важное примечание
Сервер продолжает создавать локальный текст урона для своего отображения, но клиенты получают информацию о попаданиях только через HitPacket от сервера. Это обеспечивает:
- Сервер видит урон локально (для игрока на сервере)
- Клиенты видят урон через сетевые пакеты (синхронизировано)
- Нет дублирования на клиентах

## Тестирование
После компиляции нужно проверить:
1. ✅ При выстреле появляется только 1 пуля (на клиенте)
2. ✅ При попадании появляется только 1 текст урона "-[дамаг]" (на клиенте)
3. ✅ Урон применяется корректно (проверить здоровье)
4. ✅ Пули исчезают после попадания
5. ✅ Сервер видит текст урона при попадании в клиента
6. ✅ Клиент видит текст урона при попадании в сервера
7. ✅ Работает в режиме 2 игроков (клиент vs сервер)
8. ✅ Работает в режиме множества игроков (через GameState)

## Компиляция
```bash
# Клиент
.\compile_client.bat

# Сервер
.\compile_server.bat
```
