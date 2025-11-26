# Реализация задержки респавна 5 секунд

## Описание изменений

Добавлена задержка в 5 секунд перед респавном игрока после смерти.
Исправлена проблема с респавном клиента - теперь клиент респавнится в случайном месте, как и сервер.

## Изменения в коде

### Файл: Zero_Ground/Zero_Ground.cpp

#### 1. Запуск таймера при смерти сервера (строка ~4524)

```cpp
if (serverHealth <= 0.0f) {
    ErrorHandler::logInfo("!!! SERVER PLAYER DEATH TRIGGERED !!! Health: " + std::to_string(serverHealth));
    serverIsAlive = false;
    serverWaitingRespawn = true;
    serverRespawnTimer.restart(); // Start 5 second respawn timer
    wasKill = true;
    // ...
}
```

#### 2. Запуск таймера при смерти клиента (строка ~4675)

```cpp
if (clientHealth <= 0.0f && clientIsAlive) {
    clientIsAlive = false;
    clientWaitingRespawn = true;
    clientRespawnTimer.restart(); // Start 5 second respawn timer
    wasKill = true;
    // ...
}
```

#### 3. Проверка таймера перед респавном сервера (строка ~4760)

```cpp
// NEW DEATH SYSTEM: Handle respawn with 5 second delay
if (serverWaitingRespawn) {
    // Check if 5 seconds have passed since death
    if (serverRespawnTimer.getElapsedTime().asSeconds() >= 5.0f) {
        // Respawn after 5 second delay
        ErrorHandler::logInfo("!!! SERVER PLAYER RESPAWNING !!!");
        serverHealth = 100.0f;
        serverIsAlive = true;
        serverWaitingRespawn = false;
        // ... генерация позиции респавна
    }
}
```

#### 4. Проверка таймера перед респавном клиента (строка ~4815)

```cpp
// NEW DEATH SYSTEM: Handle client respawn with 5 second delay
if (clientWaitingRespawn) {
    // Check if 5 seconds have passed since death
    if (clientRespawnTimer.getElapsedTime().asSeconds() >= 5.0f) {
        // Respawn after 5 second delay
        ErrorHandler::logInfo("!!! CLIENT PLAYER RESPAWNING !!!");
        clientHealth = 100.0f;
        clientIsAlive = true;
        clientWaitingRespawn = false;
        // ... генерация позиции респавна
    }
}
```

## Технические детали

### Используемые переменные

- `serverRespawnTimer` - таймер для отслеживания времени с момента смерти сервера
- `clientRespawnTimer` - таймер для отслеживания времени с момента смерти клиента
- `serverWaitingRespawn` - флаг ожидания респавна сервера
- `clientWaitingRespawn` - флаг ожидания респавна клиента

### Алгоритм работы

1. При смерти игрока (health <= 0):
   - Устанавливается флаг `*WaitingRespawn = true`
   - Запускается таймер через `*RespawnTimer.restart()`
   - Игрок помечается как мертвый (`*IsAlive = false`)

2. В каждом кадре игрового цикла:
   - Проверяется флаг `*WaitingRespawn`
   - Если флаг установлен, проверяется время с момента смерти
   - Если прошло >= 5 секунд, происходит респавн

3. При респавне:
   - Восстанавливается здоровье до 100
   - Игрок помечается как живой
   - Сбрасывается флаг ожидания респавна
   - Генерируется новая позиция вдали от противника (>= 1000 пикселей)

## Компиляция

Код успешно скомпилирован без ошибок:
```
71 Warning(s)
0 Error(s)
Compilation successful!
```

## Тестирование

Для проверки работы задержки респавна:

1. Запустите сервер
2. Подключите клиент
3. Убейте одного из игроков
4. Наблюдайте черный экран с текстом "You dead"
5. Через 5 секунд игрок должен респавниться в новой позиции
6. Проверьте логи сервера для подтверждения времени респавна

## Совместимость

Изменения полностью совместимы с существующей системой:
- Сохранена награда за убийство ($5000)
- Сохранено восстановление здоровья до 100 HP
- Сохранена генерация случайной позиции респавна
- Сохранена проверка расстояния до противника


## Исправление проблемы с респавном клиента

### Проблема

Клиент после смерти респавнился в том же месте, а не в случайном, как сервер.

### Причина

Когда сервер устанавливал новую случайную позицию для клиента при респавне, клиент продолжал отправлять свою старую позицию серверу через UDP. Сервер получал эту старую позицию и перезаписывал ею новую позицию респавна.

### Решение

Добавлена проверка в UDP потоке сервера (строка ~3730):

```cpp
// Update client position with interpolation support
// IMPORTANT: Only update if client is alive or not waiting for respawn
// This prevents client from overwriting server-assigned respawn position
if (clientIsAlive && !clientWaitingRespawn) {
    std::lock_guard<std::mutex> lock(mutex);
    
    // Store previous position for interpolation
    clientPosPrevious.x = clientPosTarget.x;
    clientPosPrevious.y = clientPosTarget.y;
    
    // Update target position (latest received)
    clientPosTarget.x = receivedPacket->x;
    clientPosTarget.y = receivedPacket->y;
    
    // Also update legacy clients map for backward compatibility
    Position pos;
    pos.x = receivedPacket->x;
    pos.y = receivedPacket->y;
    clients[sender] = pos;
}
```

Теперь сервер игнорирует обновления позиции от клиента когда:
- Клиент мертв (`!clientIsAlive`)
- Клиент ожидает респавна (`clientWaitingRespawn`)

Это позволяет серверу установить новую случайную позицию при респавне без риска что клиент перезапишет её своей старой позицией.

### Результат

Теперь клиент корректно респавнится в случайном месте на расстоянии >= 1000 пикселей от сервера, точно так же как и сервер.
