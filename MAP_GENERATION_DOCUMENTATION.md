# Документация по генерации карты Zero Ground

## Обзор системы

Игра использует **клеточную систему генерации карты** (Cell-Based Map System) размером 5100×5100 пикселей, разделенную на сетку 51×51 ячеек по 100 пикселей каждая.

## Параметры карты

### Константы
```cpp
const float MAP_SIZE = 5100.0f;      // Размер карты в пикселях
const float CELL_SIZE = 100.0f;      // Размер одной ячейки
const int GRID_SIZE = 51;            // Количество ячеек (5100 / 100 = 51)
const float PLAYER_SIZE = 20.0f;     // Диаметр игрока (радиус = 10px)
const float WALL_WIDTH = 12.0f;      // Толщина стены
const float WALL_LENGTH = 100.0f;    // Длина стены (равна размеру ячейки)
```

### Точки спавна
- **Сервер (зеленый игрок)**: (250, 4850) - нижний левый угол карты
- **Клиент (синий игрок)**: (4850, 250) - верхний правый угол карты

## Типы стен

Игра поддерживает два типа стен с различными визуальными характеристиками:

```cpp
enum class WallType : uint8_t {
    None = 0,      // Нет стены
    Concrete = 1,  // Бетонная стена (серая)
    Wood = 2       // Деревянная стена (коричневая)
};
```

### Визуальные характеристики типов стен

| Тип | Цвет RGB | Описание | Вероятность |
|-----|----------|----------|-------------|
| **Concrete** (Бетон) | (150, 150, 150) | Серая стена | 70% |
| **Wood** (Дерево) | (139, 90, 43) | Коричневая стена | 30% |

## Структура ячейки

Каждая ячейка может иметь стены на четырех сторонах, каждая стена имеет свой тип:

```cpp
struct Cell {
    WallType topWall = WallType::None;      // Стена сверху
    WallType rightWall = WallType::None;    // Стена справа
    WallType bottomWall = WallType::None;   // Стена снизу
    WallType leftWall = WallType::None;     // Стена слева
};
```

### Позиционирование стен

Стены центрированы на границах ячеек:
- **topWall**: y - WALL_WIDTH/2 (6px выше и 6px ниже границы)
- **rightWall**: x + CELL_SIZE - WALL_WIDTH/2 (6px левее и 6px правее границы)
- **bottomWall**: y + CELL_SIZE - WALL_WIDTH/2 (6px выше и 6px ниже границы)
- **leftWall**: x - WALL_WIDTH/2 (6px левее и 6px правее границы)

## Алгоритм генерации

### 1. Вероятностная генерация

Стены генерируются только в ячейках, где `(i + j) % 2 == 1` (шахматный паттерн).

**Вероятности количества стен:**
- **60%** - одна стена на случайной стороне
- **25%** - две стены на разных сторонах
- **15%** - нет стен

**Вероятности типа стены:**
- **70%** - Бетонная стена (Concrete) - серая
- **30%** - Деревянная стена (Wood) - коричневая

Каждая стена получает свой тип независимо от других стен в той же ячейке.

```cpp
void generateMap(std::vector<std::vector<Cell>>& grid) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> probDist(0, 99);
    std::uniform_int_distribution<int> sideDist(0, 3);
    std::uniform_int_distribution<int> typeDist(0, 99);
    
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            // Только ячейки где (i+j)%2==1 могут иметь стены
            if ((i + j) % 2 == 1) {
                int probability = probDist(gen);
                
                if (probability < 60) {
                    // 60% - одна стена
                    int side = sideDist(gen);
                    // Определяем тип: 70% бетон, 30% дерево
                    WallType type = (typeDist(gen) < 70) ? WallType::Concrete : WallType::Wood;
                    setWall(grid[i][j], side, type);
                }
                else if (probability < 85) {
                    // 25% - две стены
                    int side1 = sideDist(gen);
                    int side2 = sideDist(gen);
                    while (side2 == side1) {
                        side2 = sideDist(gen);
                    }
                    
                    // Каждая стена получает свой тип независимо
                    WallType type1 = (typeDist(gen) < 70) ? WallType::Concrete : WallType::Wood;
                    WallType type2 = (typeDist(gen) < 70) ? WallType::Concrete : WallType::Wood;
                    setWall(grid[i][j], side1, type1);
                    setWall(grid[i][j], side2, type2);
                }
                // 15% - нет стен
            }
        }
    }
}

// Вспомогательная функция для установки стены
void setWall(Cell& cell, int side, WallType type) {
    switch (side) {
        case 0: cell.topWall = type; break;
        case 1: cell.rightWall = type; break;
        case 2: cell.bottomWall = type; break;
        case 3: cell.leftWall = type; break;
    }
}
```

### 2. Валидация связности (BFS)

После генерации проверяется, что оба игрока могут достичь друг друга.

**Алгоритм BFS (Breadth-First Search):**
1. Начинаем с точки спавна сервера
2. Исследуем все соседние ячейки (вверх, вниз, влево, вправо)
3. Проверяем, нет ли стены, блокирующей переход
4. Помечаем посещенные ячейки
5. Продолжаем, пока не достигнем точки спавна клиента

```cpp
bool canMove(sf::Vector2i from, sf::Vector2i to, const grid) {
    int dx = to.x - from.x;
    int dy = to.y - from.y;
    
    // Проверяем наличие стены (любого типа) в направлении движения
    if (dx == 1)  return grid[from.x][from.y].rightWall == WallType::None;
    if (dx == -1) return grid[from.x][from.y].leftWall == WallType::None;
    if (dy == 1)  return grid[from.x][from.y].bottomWall == WallType::None;
    if (dy == -1) return grid[from.x][from.y].topWall == WallType::None;
    
    return false;
}
```

### 3. Система повторных попыток

Если путь между игроками не существует, карта генерируется заново.

**Параметры:**
- Максимум попыток: **10**
- При неудаче всех попыток: программа завершается с ошибкой

```cpp
bool generateValidMap(std::vector<std::vector<Cell>>& grid) {
    for (int attempt = 0; attempt < 10; ++attempt) {
        // Очистить сетку
        clearGrid(grid);
        
        // Сгенерировать стены
        generateMap(grid);
        
        // Проверить связность
        if (isPathExists(serverSpawn, clientSpawn, grid)) {
            return true; // Успех!
        }
    }
    
    // Все попытки провалились
    handleMapGenerationFailure();
    return false;
}
```

## Геометрия стен

### Горизонтальные стены (topWall, bottomWall)
- **Размер**: 100×12 пикселей
- **Ориентация**: горизонтальная
- **Позиция**: центрирована на горизонтальной границе ячейки

### Вертикальные стены (leftWall, rightWall)
- **Размер**: 12×100 пикселей
- **Ориентация**: вертикальная
- **Позиция**: центрирована на вертикальной границе ячейки

### Визуальные характеристики
- **Скругление углов**: радиус 3 пикселя
- **Форма**: ConvexShape с 32 точками (8 точек на угол)
- **Цвета**:
  - Бетон (Concrete): RGB(150, 150, 150) - серый
  - Дерево (Wood): RGB(139, 90, 43) - коричневый

### Рендеринг с учетом типа стены

```cpp
// Цвета стен
sf::Color concreteColor(150, 150, 150);  // Серый для бетона
sf::Color woodColor(139, 90, 43);        // Коричневый для дерева

// При рендеринге выбираем цвет в зависимости от типа
if (grid[i][j].topWall != WallType::None) {
    sf::Color baseColor = (grid[i][j].topWall == WallType::Concrete) 
        ? concreteColor 
        : woodColor;
    
    // Применяем туман войны
    horizontalWall.setFillColor(sf::Color(baseColor.r, baseColor.g, baseColor.b, alpha));
    window.draw(horizontalWall);
}
```

## Система столкновений

### Обнаружение столкновений

Игрок представлен как квадрат 20×20 пикселей (для упрощения расчетов).

```cpp
bool checkCollision(sf::Vector2f pos, const grid) {
    sf::FloatRect playerRect(
        pos.x - PLAYER_SIZE / 2.0f,
        pos.y - PLAYER_SIZE / 2.0f,
        PLAYER_SIZE,
        PLAYER_SIZE
    );
    
    // Проверяем ячейки вокруг игрока (3×3)
    int playerCellX = static_cast<int>(pos.x / CELL_SIZE);
    int playerCellY = static_cast<int>(pos.y / CELL_SIZE);
    
    for (int i = playerCellX - 1; i <= playerCellX + 1; i++) {
        for (int j = playerCellY - 1; j <= playerCellY + 1; j++) {
            // Проверяем все 4 стены ячейки
            if (grid[i][j].topWall && intersects(playerRect, topWallRect))
                return true;
            // ... аналогично для других стен
        }
    }
    
    return false;
}
```

### Разрешение столкновений (скольжение по стенам)

При столкновении игрок пытается скользить вдоль стены:

1. **Проверка новой позиции** - есть ли столкновение?
2. **Скольжение по X** - попытка двигаться только по горизонтали
3. **Скольжение по Y** - попытка двигаться только по вертикали
4. **Застревание** - если оба варианта не работают, остаться на месте

```cpp
sf::Vector2f resolveCollisionCellBased(sf::Vector2f oldPos, sf::Vector2f newPos, const grid) {
    if (!checkCollision(newPos, grid)) {
        return clampToMapBounds(newPos);
    }
    
    // Попытка скольжения по X
    sf::Vector2f slideX(newPos.x, oldPos.y);
    if (!checkCollision(slideX, grid)) {
        return clampToMapBounds(slideX);
    }
    
    // Попытка скольжения по Y
    sf::Vector2f slideY(oldPos.x, newPos.y);
    if (!checkCollision(slideY, grid)) {
        return clampToMapBounds(slideY);
    }
    
    // Застряли в углу
    return oldPos;
}
```

## Оптимизация рендеринга

### Отсечение невидимых стен

Рендерятся только стены в видимой области камеры + 2 ячейки отступа.

```cpp
void renderVisibleWalls(sf::RenderWindow& window, sf::Vector2f playerPosition, const grid) {
    sf::View currentView = window.getView();
    sf::Vector2f viewCenter = currentView.getCenter();
    sf::Vector2f viewSize = currentView.getSize();
    
    // Вычисляем видимую область
    float padding = CELL_SIZE * 2.0f;
    float minX = viewCenter.x - viewSize.x / 2.0f - padding;
    float maxX = viewCenter.x + viewSize.x / 2.0f + padding;
    float minY = viewCenter.y - viewSize.y / 2.0f - padding;
    float maxY = viewCenter.y + viewSize.y / 2.0f + padding;
    
    // Конвертируем в координаты ячеек
    int startX = max(0, (int)(minX / CELL_SIZE));
    int endX = min(GRID_SIZE - 1, (int)(maxX / CELL_SIZE));
    int startY = max(0, (int)(minY / CELL_SIZE));
    int endY = min(GRID_SIZE - 1, (int)(maxY / CELL_SIZE));
    
    // Рендерим только видимые стены
    for (int i = startX; i <= endX; i++) {
        for (int j = startY; j <= endY; j++) {
            renderCellWalls(grid[i][j], i, j);
        }
    }
}
```

### Кэширование форм стен

Формы стен создаются один раз и переиспользуются:

```cpp
static sf::ConvexShape horizontalWall = createRoundedRectangle(
    sf::Vector2f(WALL_LENGTH, WALL_WIDTH), 3.0f
);
static sf::ConvexShape verticalWall = createRoundedRectangle(
    sf::Vector2f(WALL_WIDTH, WALL_LENGTH), 3.0f
);
```

## Туман войны

Стены затемняются в зависимости от расстояния до игрока:

| Расстояние | Видимость | Alpha |
|------------|-----------|-------|
| 0-210px    | 100%      | 255   |
| 210-510px  | 60%       | 153   |
| 510-930px  | 40%       | 102   |
| 930-1020px | 20%       | 51    |
| >1020px    | 0%        | 0     |

```cpp
sf::Uint8 calculateFogAlpha(float distance) {
    if (distance <= 210.0f)  return 255;
    if (distance <= 510.0f)  return 153;
    if (distance <= 930.0f)  return 102;
    if (distance <= 1020.0f) return 51;
    return 0;
}
```

## Производительность

### Целевые показатели
- **FPS**: 55+ (целевой frame time: 16.67ms)
- **Обнаружение столкновений**: <0.3ms на кадр
- **Рендеринг стен**: проверяется только ~100-200 стен вместо всех ~5000

### Оптимизации
1. **Отсечение по видимости** - рендеринг только видимых стен
2. **Локальная проверка столкновений** - проверка только 3×3 ячеек вокруг игрока
3. **Кэширование форм** - переиспользование созданных ConvexShape
4. **Туман войны** - пропуск рендеринга полностью невидимых объектов

## Сериализация карты

Карта передается от сервера к клиенту через TCP при подключении.

### Формат данных
```cpp
// 1. Размер данных (4 байта)
uint32_t dataSize = GRID_SIZE * GRID_SIZE * sizeof(Cell);

// 2. Данные сетки (построчно)
for (int i = 0; i < GRID_SIZE; i++) {
    memcpy(buffer + offset, grid[i].data(), GRID_SIZE * sizeof(Cell));
    offset += GRID_SIZE * sizeof(Cell);
}
```

### Размер передаваемых данных
- **Размер одной ячейки**: 4 байта (4 × WallType, где WallType = uint8_t)
- **Всего ячеек**: 51 × 51 = 2,601
- **Общий размер**: 2,601 × 4 = **10,404 байта (~10 КБ)**

**Примечание**: Каждая стена хранится как `WallType` (uint8_t), что позволяет различать типы стен:
- 0 = None (нет стены)
- 1 = Concrete (бетон)
- 2 = Wood (дерево)

## Примеры использования

### Создание новой карты
```cpp
std::vector<std::vector<Cell>> grid(GRID_SIZE, std::vector<Cell>(GRID_SIZE));

if (generateValidMap(grid)) {
    std::cout << "Карта успешно сгенерирована!" << std::endl;
} else {
    std::cerr << "Не удалось сгенерировать карту" << std::endl;
}
```

### Проверка возможности движения
```cpp
sf::Vector2i currentCell(10, 10);
sf::Vector2i nextCell(11, 10); // Движение вправо

if (canMove(currentCell, nextCell, grid)) {
    std::cout << "Можно двигаться вправо" << std::endl;
} else {
    std::cout << "Стена блокирует движение" << std::endl;
}
```

### Добавление стены вручную
```cpp
// Добавить бетонную верхнюю стену в ячейке (5, 5)
grid[5][5].topWall = WallType::Concrete;

// Добавить деревянную правую стену в ячейке (10, 15)
grid[10][15].rightWall = WallType::Wood;

// Удалить стену
grid[5][5].topWall = WallType::None;
```

## Отладка

### Подсчет стен
```cpp
struct WallStats {
    int totalWalls = 0;
    int concreteWalls = 0;
    int woodWalls = 0;
};

WallStats countWalls(const std::vector<std::vector<Cell>>& grid) {
    WallStats stats;
    
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            // Проверяем каждую стену
            if (grid[i][j].topWall != WallType::None) {
                stats.totalWalls++;
                if (grid[i][j].topWall == WallType::Concrete) 
                    stats.concreteWalls++;
                else 
                    stats.woodWalls++;
            }
            
            if (grid[i][j].rightWall != WallType::None) {
                stats.totalWalls++;
                if (grid[i][j].rightWall == WallType::Concrete) 
                    stats.concreteWalls++;
                else 
                    stats.woodWalls++;
            }
            
            if (grid[i][j].bottomWall != WallType::None) {
                stats.totalWalls++;
                if (grid[i][j].bottomWall == WallType::Concrete) 
                    stats.concreteWalls++;
                else 
                    stats.woodWalls++;
            }
            
            if (grid[i][j].leftWall != WallType::None) {
                stats.totalWalls++;
                if (grid[i][j].leftWall == WallType::Concrete) 
                    stats.concreteWalls++;
                else 
                    stats.woodWalls++;
            }
        }
    }
    
    return stats;
}

// Использование:
WallStats stats = countWalls(grid);
std::cout << "Всего стен: " << stats.totalWalls << std::endl;
std::cout << "Бетонных: " << stats.concreteWalls << " (" 
          << (stats.concreteWalls * 100.0 / stats.totalWalls) << "%)" << std::endl;
std::cout << "Деревянных: " << stats.woodWalls << " (" 
          << (stats.woodWalls * 100.0 / stats.totalWalls) << "%)" << std::endl;
```

### Визуализация пути BFS
```cpp
#ifdef _DEBUG
std::cout << "BFS: Проверка ячейки (" << current.x << ", " << current.y << ")" << std::endl;
#endif
```

## Известные ограничения

1. **Максимум 10 попыток генерации** - если все провалились, игра завершается
2. **Фиксированный размер карты** - 5100×5100 пикселей
3. **Шахматный паттерн** - стены только в ячейках где (i+j)%2==1
4. **Простая форма игрока** - квадрат вместо круга для столкновений

## Статистика генерации

При успешной генерации карты выводится подробная статистика:

```
=== Attempt 1 of 10 ===
Clearing grid...
Generating walls...
Generated 1234 walls (567 concrete, 667 wood)
Validating connectivity...
Server spawn: (250, 4850)
Client spawn: (4850, 250)

✓ SUCCESS! Map generated successfully on attempt 1
Path exists between spawn points
Total walls: 1234
  Concrete: 567 (45.9%)
  Wood: 667 (54.1%)
================================
```

### Ожидаемое распределение стен

При правильной работе генератора:
- **Общее количество стен**: ~1200-1400 (зависит от случайности)
- **Бетонные стены**: ~70% от общего количества
- **Деревянные стены**: ~30% от общего количества

## Будущие улучшения

- [ ] Настраиваемые вероятности генерации стен и типов
- [ ] Дополнительные типы стен (металл, стекло, разрушаемые)
- [ ] Сохранение/загрузка карт в файл
- [ ] Редактор карт с GUI
- [ ] Процедурная генерация с seed для воспроизводимости
- [ ] Динамические препятствия и разрушаемые стены
- [ ] Различные свойства стен (прочность, прозрачность)
