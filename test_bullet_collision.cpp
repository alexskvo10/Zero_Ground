// Test file to verify bullet collision logic compiles correctly
#include <vector>
#include <algorithm>
#include <cmath>

const float CELL_SIZE = 100.0f;
const int GRID_SIZE = 51;
const float WALL_WIDTH = 12.0f;
const float WALL_LENGTH = 100.0f;

enum class WallType : uint8_t {
    None = 0,
    Concrete = 1,
    Wood = 2
};

struct Cell {
    WallType topWall = WallType::None;
    WallType rightWall = WallType::None;
    WallType bottomWall = WallType::None;
    WallType leftWall = WallType::None;
};

struct Bullet {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float range = 0.0f;
    
    // Check collision with cell-based walls (returns wall type if collision, None otherwise)
    WallType checkCellWallCollision(const std::vector<std::vector<Cell>>& grid) const {
        // Calculate which cell the bullet is in
        int cellX = static_cast<int>(x / CELL_SIZE);
        int cellY = static_cast<int>(y / CELL_SIZE);
        
        // Check bounds
        if (cellX < 0 || cellX >= GRID_SIZE || cellY < 0 || cellY >= GRID_SIZE) {
            return WallType::None;
        }
        
        // Check nearby cells for wall collisions
        for (int i = std::max(0, cellX - 1); i <= std::min(GRID_SIZE - 1, cellX + 1); i++) {
            for (int j = std::max(0, cellY - 1); j <= std::min(GRID_SIZE - 1, cellY + 1); j++) {
                float cellWorldX = i * CELL_SIZE;
                float cellWorldY = j * CELL_SIZE;
                
                // Check top wall
                if (grid[i][j].topWall != WallType::None) {
                    float wallX = cellWorldX;
                    float wallY = cellWorldY - WALL_WIDTH / 2.0f;
                    if (x >= wallX && x <= wallX + WALL_LENGTH &&
                        y >= wallY && y <= wallY + WALL_WIDTH) {
                        return grid[i][j].topWall;
                    }
                }
                
                // Check right wall
                if (grid[i][j].rightWall != WallType::None) {
                    float wallX = cellWorldX + CELL_SIZE - WALL_WIDTH / 2.0f;
                    float wallY = cellWorldY;
                    if (x >= wallX && x <= wallX + WALL_WIDTH &&
                        y >= wallY && y <= wallY + WALL_LENGTH) {
                        return grid[i][j].rightWall;
                    }
                }
                
                // Check bottom wall
                if (grid[i][j].bottomWall != WallType::None) {
                    float wallX = cellWorldX;
                    float wallY = cellWorldY + CELL_SIZE - WALL_WIDTH / 2.0f;
                    if (x >= wallX && x <= wallX + WALL_LENGTH &&
                        y >= wallY && y <= wallY + WALL_WIDTH) {
                        return grid[i][j].bottomWall;
                    }
                }
                
                // Check left wall
                if (grid[i][j].leftWall != WallType::None) {
                    float wallX = cellWorldX - WALL_WIDTH / 2.0f;
                    float wallY = cellWorldY;
                    if (x >= wallX && x <= wallX + WALL_WIDTH &&
                        y >= wallY && y <= wallY + WALL_LENGTH) {
                        return grid[i][j].leftWall;
                    }
                }
            }
        }
        
        return WallType::None;
    }
};

int main() {
    // Test the logic
    std::vector<std::vector<Cell>> grid(GRID_SIZE, std::vector<Cell>(GRID_SIZE));
    
    // Create a concrete wall
    grid[10][10].topWall = WallType::Concrete;
    
    // Create a wooden wall
    grid[20][20].rightWall = WallType::Wood;
    
    // Test bullet hitting concrete wall
    Bullet bullet1;
    bullet1.x = 1000.0f;
    bullet1.y = 994.0f; // Just above the top wall
    WallType hit1 = bullet1.checkCellWallCollision(grid);
    
    // Test bullet hitting wooden wall
    Bullet bullet2;
    bullet2.x = 2106.0f; // Just at the right wall
    bullet2.y = 2000.0f;
    WallType hit2 = bullet2.checkCellWallCollision(grid);
    
    return 0;
}
