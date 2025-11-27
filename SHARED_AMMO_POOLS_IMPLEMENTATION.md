# Реализация общих пулов патронов

## Описание
Переход от индивидуальных запасов патронов для каждого оружия к общим пулам патронов по типам оружия.

## Текущая система
- Каждое оружие имеет свой `reserveAmmo` (резервный запас)
- При покупке патронов они добавляются к каждому оружию отдельно
- Переменные в Player: `ammo9x18`, `ammo545x39`, `ammo762x54` (старые) и `pistolAmmo`, `rifleAmmo`, `sniperAmmo` (новые, но не используются)

## Новая система
- Оружие имеет только `currentAmmo` (патроны в магазине)
- Резервные патроны хранятся в Player в общих пулах:
  - `pistolAmmo` - для всех пистолетов (USP, Glock, Five-SeveN, R8)
  - `rifleAmmo` - для всех автоматов (Galil, M4, AK-47)
  - `sniperAmmo` - для всех снайперских винтовок (M10, AWP, M40)
- При покупке патронов пополняется соответствующий общий пул
- При перезарядке патроны берутся из общего пула

## Изменения в коде

### 1. Структура Weapon
- ❌ Удалить: `int reserveAmmo`
- ✅ Оставить: `int currentAmmo` (патроны в магазине)
- ✅ Обновить: метод `startReload()` - брать патроны из Player
- ✅ Обновить: метод `updateReload()` - использовать общий пул
- ✅ Обновить: метод `create()` - не инициализировать reserveAmmo

### 2. Структура Player
- ❌ Удалить: `ammo9x18`, `ammo545x39`, `ammo762x54` (старые переменные)
- ✅ Использовать: `pistolAmmo`, `rifleAmmo`, `sniperAmmo`
- ✅ Добавить: методы для работы с общими пулами

### 3. Логика перезарядки
Изменить `startReload()` и `updateReload()` для работы с Player:
```cpp
void startReload(Player* player) {
    int* ammoPool = getAmmoPool(player);
    if (ammoPool && *ammoPool > 0 && currentAmmo < magazineSize) {
        isReloading = true;
        reloadClock.restart();
    }
}

void updateReload(Player* player) {
    if (isReloading && reloadClock.getElapsedTime().asSeconds() >= reloadTime) {
        int* ammoPool = getAmmoPool(player);
        if (ammoPool) {
            int ammoNeeded = magazineSize - currentAmmo;
            int ammoToTransfer = std::min(ammoNeeded, *ammoPool);
            currentAmmo += ammoToTransfer;
            *ammoPool -= ammoToTransfer;
        }
        isReloading = false;
    }
}

int* getAmmoPool(Player* player) {
    AmmoType ammoType = getAmmoType();
    switch (ammoType) {
        case AmmoType::AMMO_9x18: return &player->pistolAmmo;
        case AmmoType::AMMO_5_45x39: return &player->rifleAmmo;
        case AmmoType::AMMO_7_62x54: return &player->sniperAmmo;
    }
    return nullptr;
}
```

### 4. Покупка патронов
Обновить логику покупки патронов - добавлять в общий пул:
```cpp
// Вместо добавления к каждому оружию:
// weapon->reserveAmmo += ammo->quantity;

// Добавлять в общий пул:
switch (ammo->type) {
    case AmmoType::AMMO_9x18:
        player.pistolAmmo += ammo->quantity;
        break;
    case AmmoType::AMMO_5_45x39:
        player.rifleAmmo += ammo->quantity;
        break;
    case AmmoType::AMMO_7_62x54:
        player.sniperAmmo += ammo->quantity;
        break;
}
```

### 5. Инициализация игрока
Обновить `initializePlayer()`:
```cpp
void initializePlayer(Player& player) {
    player.inventory[0] = Weapon::create(Weapon::USP);
    player.inventory[1] = nullptr;
    player.inventory[2] = nullptr;
    player.inventory[3] = nullptr;
    player.money = 50000;
    player.activeSlot = 0;
    
    // Начальные запасы патронов
    player.pistolAmmo = 100;  // Для USP
    player.rifleAmmo = 0;
    player.sniperAmmo = 0;
}
```

## Преимущества новой системы
1. ✅ Реалистичность - патроны одного калибра используются всеми оружиями этого типа
2. ✅ Упрощение - не нужно отслеживать патроны для каждого оружия отдельно
3. ✅ Удобство - купил патроны один раз, используешь для всех оружий типа
4. ✅ Баланс - игрок должен выбирать, какой тип патронов покупать

## Файлы для изменения
1. `Zero_Ground/Zero_Ground.cpp` (сервер)
2. `Zero_Ground_client/Zero_Ground_client.cpp` (клиент)

## Тестирование
1. Купить пистолет, проверить начальный запас патронов
2. Купить второй пистолет, проверить что используется общий пул
3. Купить патроны, проверить что пополняется общий пул
4. Стрелять и перезаряжаться, проверить расход из общего пула
5. Купить автомат, проверить что у него свой пул патронов
