# Система проникновения пуль через деревянные стены

## Описание
Реализована система, позволяющая пулям проходить через деревянные стены, но останавливаться при столкновении с бетонными стенами.

## Изменения

### 1. Новый метод в структуре Bullet

Добавлен метод `checkCellWallCollision()` в структуру `Bullet` (в обоих файлах: клиент и сервер):

```cpp
WallType checkCellWallCollision(const std::vector<std::vector<Cell>>& grid) const
```

**Функциональность:**
- Определяет, в какой ячейке сетки находится пуля
- Проверяет столкновения со стенами в текущей и соседних ячейках
- Возвращает тип стены при столкновении (`WallType::Concrete`, `WallType::Wood`) или `WallType::None`

**Алгоритм:**
1. Вычисляет координаты ячейки пули: `cellX = x / CELL_SIZE`, `cellY = y / CELL_SIZE`
2. Проверяет границы карты
3. Для каждой из 9 ячеек (текущая + 8 соседних):
   - Проверяет все 4 стены ячейки (верхняя, правая, нижняя, левая)
   - Для каждой стены вычисляет её мировые координаты
   - Проверяет, находится ли пуля внутри прямоугольника стены
   - Если да - возвращает тип этой стены

### 2. Обновленная логика обработки столкновений

**Клиент** (`Zero_Ground_client/Zero_Ground_client.cpp`, строки ~2960-2970):
```cpp
// Requirement 7.3: Check bullet-wall collisions with cell-based grid
// Bullets pass through wooden walls but stop at concrete walls
for (auto& bullet : activeBullets) {
    WallType hitWallType = bullet.checkCellWallCollision(grid);
    
    // Only stop bullet if it hit a concrete wall
    // Wooden walls are penetrable
    if (hitWallType == WallType::Concrete) {
        bullet.range = 0.0f;
    }
}
```

**Сервер** (`Zero_Ground/Zero_Ground.cpp`, строки ~3975-3985):
```cpp
// Requirement 7.3: Check bullet-wall collisions with cell-based grid
// Bullets pass through wooden walls but stop at concrete walls
for (auto& bullet : activeBullets) {
    WallType hitWallType = bullet.checkCellWallCollision(grid);
    
    // Only stop bullet if it hit a concrete wall
    // Wooden walls are penetrable
    if (hitWallType == WallType::Concrete) {
        bullet.range = 0.0f;
    }
}
```

## Поведение системы

### Бетонные стены (Concrete)
- **Цвет:** Серый (150, 150, 150)
- **Поведение:** Пули останавливаются при столкновении
- **Вероятность генерации:** 70%

### Деревянные стены (Wood)
- **Цвет:** Коричневый (139, 90, 43)
- **Поведение:** Пули проходят насквозь без остановки
- **Вероятность генерации:** 30%

## Технические детали

### Константы
- `CELL_SIZE = 100.0f` - размер ячейки сетки
- `WALL_WIDTH = 12.0f` - ширина стены
- `WALL_LENGTH = 100.0f` - длина стены
- `GRID_SIZE = 51` - размер сетки (51×51 ячеек)

### Позиционирование стен
Стены центрированы на границах ячеек:
- **Верхняя стена:** `y = cellY - WALL_WIDTH/2`
- **Правая стена:** `x = cellX + CELL_SIZE - WALL_WIDTH/2`
- **Нижняя стена:** `y = cellY + CELL_SIZE - WALL_WIDTH/2`
- **Левая стена:** `x = cellX - WALL_WIDTH/2`

### Оптимизация
- Проверка только 9 ячеек (текущая + 8 соседних) вместо всей карты
- Ранний выход при обнаружении столкновения
- Проверка границ карты для предотвращения выхода за пределы массива

## Тестирование

Для тестирования функциональности:

1. **Запустите сервер:**
   ```
   compile_server.bat
   Zero_Ground\Debug\Zero_Ground.exe
   ```

2. **Запустите клиент:**
   ```
   compile_client.bat
   Zero_Ground_client\Debug\Zero_Ground_client.exe
   ```

3. **Проверьте поведение:**
   - Стреляйте в серые (бетонные) стены - пули должны останавливаться
   - Стреляйте в коричневые (деревянные) стены - пули должны проходить насквозь

## Совместимость

Изменения полностью совместимы с существующей системой:
- Сохранена старая функция `checkWallCollision()` для обратной совместимости
- Новая система работает параллельно с существующей сеточной системой карты
- Не требуется изменение сетевого протокола

## Файлы изменены

1. `Zero_Ground_client/Zero_Ground_client.cpp`
   - Добавлен метод `Bullet::checkCellWallCollision()`
   - Обновлена логика проверки столкновений пуль

2. `Zero_Ground/Zero_Ground.cpp`
   - Добавлен метод `Bullet::checkCellWallCollision()`
   - Обновлена логика проверки столкновений пуль

## Будущие улучшения

Возможные расширения функциональности:
- Добавление урона при прохождении через деревянные стены (снижение damage)
- Ограничение количества деревянных стен, через которые может пройти пуля
- Визуальные эффекты при прохождении пули через деревянную стену
- Звуковые эффекты для разных типов столкновений
