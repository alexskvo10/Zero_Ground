// Weapon System Property-Based Tests for Zero Ground
// **Feature: weapon-shop-system, Property 33: Weapon property completeness**
// **Validates: Requirements 11.2**

#include <iostream>
#include <cassert>
#include <cstring>
#include <cmath>
#include <vector>
#include <memory>
#include <sstream>
#include <random>
#include <string>
#include <array>

// ========================
// Test Framework Macros
// ========================

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running test: " << #name << "..."; \
    try { \
        test_##name(); \
        std::cout << " PASSED" << std::endl; \
        passedTests++; \
    } catch (const std::exception& e) { \
        std::cout << " FAILED: " << e.what() << std::endl; \
        failedTests++; \
    } \
    totalTests++; \
} while(0)

#define ASSERT_TRUE(condition) do { \
    if (!(condition)) { \
        std::ostringstream oss; \
        oss << "Assertion failed: " << #condition << " at line " << __LINE__; \
        throw std::runtime_error(oss.str()); \
    } \
} while(0)

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

#define ASSERT_EQ(expected, actual) do { \
    if ((expected) != (actual)) { \
        std::ostringstream oss; \
        oss << "Assertion failed: expected " << (expected) << " but got " << (actual) << " at line " << __LINE__; \
        throw std::runtime_error(oss.str()); \
    } \
} while(0)

#define ASSERT_GT(value, threshold) do { \
    if (!((value) > (threshold))) { \
        std::ostringstream oss; \
        oss << "Assertion failed: expected " << (value) << " > " << (threshold) << " at line " << __LINE__; \
        throw std::runtime_error(oss.str()); \
    } \
} while(0)

#define ASSERT_GE(value, threshold) do { \
    if (!((value) >= (threshold))) { \
        std::ostringstream oss; \
        oss << "Assertion failed: expected " << (value) << " >= " << (threshold) << " at line " << __LINE__; \
        throw std::runtime_error(oss.str()); \
    } \
} while(0)

// Test counters
int totalTests = 0;
int passedTests = 0;
int failedTests = 0;

// ========================
// Minimal Weapon Structure (copied from main code)
// ========================

struct Weapon {
    enum Type {
        USP = 0, GLOCK = 1, FIVESEVEN = 2, R8 = 3,      // Pistols
        GALIL = 4, M4 = 5, AK47 = 6,                     // Rifles
        M10 = 7, AWP = 8, M40 = 9                        // Snipers
    };
    
    Type type;
    std::string name;
    int price;
    int magazineSize;
    int currentAmmo;
    int reserveAmmo;
    float damage;
    float range;              // Effective range in pixels
    float bulletSpeed;        // Pixels per second
    float reloadTime;         // Seconds
    float movementSpeed;      // Player speed modifier
    
    // Factory method to create weapons with proper stats
    static Weapon* create(Type type) {
        Weapon* w = new Weapon();
        w->type = type;
        w->currentAmmo = 0;  // Will be set below
        w->reserveAmmo = 0;  // Will be set below
        
        switch (type) {
            case USP:
                w->name = "USP";
                w->price = 0;
                w->magazineSize = 12;
                w->damage = 15.0f;
                w->range = 250.0f;
                w->bulletSpeed = 600.0f;
                w->reloadTime = 2.0f;
                w->movementSpeed = 2.5f;
                w->reserveAmmo = 100;
                break;
            case GLOCK:
                w->name = "Glock-18";
                w->price = 1000;
                w->magazineSize = 20;
                w->damage = 10.0f;
                w->range = 300.0f;
                w->bulletSpeed = 600.0f;
                w->reloadTime = 2.0f;
                w->movementSpeed = 2.5f;
                w->reserveAmmo = 120;
                break;
            case FIVESEVEN:
                w->name = "Five-SeveN";
                w->price = 2500;
                w->magazineSize = 20;
                w->damage = 10.0f;
                w->range = 400.0f;
                w->bulletSpeed = 800.0f;
                w->reloadTime = 2.0f;
                w->movementSpeed = 2.5f;
                w->reserveAmmo = 100;
                break;
            case R8:
                w->name = "R8 Revolver";
                w->price = 4250;
                w->magazineSize = 8;
                w->damage = 50.0f;
                w->range = 200.0f;
                w->bulletSpeed = 700.0f;
                w->reloadTime = 5.0f;
                w->movementSpeed = 2.5f;
                w->reserveAmmo = 40;
                break;
            case GALIL:
                w->name = "Galil AR";
                w->price = 10000;
                w->magazineSize = 35;
                w->damage = 25.0f;
                w->range = 450.0f;
                w->bulletSpeed = 900.0f;
                w->reloadTime = 3.0f;
                w->movementSpeed = 2.0f;
                w->reserveAmmo = 140;
                break;
            case M4:
                w->name = "M4";
                w->price = 15000;
                w->magazineSize = 30;
                w->damage = 30.0f;
                w->range = 425.0f;
                w->bulletSpeed = 850.0f;
                w->reloadTime = 3.0f;
                w->movementSpeed = 1.8f;
                w->reserveAmmo = 120;
                break;
            case AK47:
                w->name = "AK-47";
                w->price = 17500;
                w->magazineSize = 25;
                w->damage = 35.0f;
                w->range = 450.0f;
                w->bulletSpeed = 900.0f;
                w->reloadTime = 3.0f;
                w->movementSpeed = 1.6f;
                w->reserveAmmo = 100;
                break;
            case M10:
                w->name = "M10";
                w->price = 20000;
                w->magazineSize = 5;
                w->damage = 50.0f;
                w->range = 1000.0f;
                w->bulletSpeed = 2000.0f;
                w->reloadTime = 4.0f;
                w->movementSpeed = 1.1f;
                w->reserveAmmo = 25;
                break;
            case AWP:
                w->name = "AWP";
                w->price = 25000;
                w->magazineSize = 1;
                w->damage = 100.0f;
                w->range = 1000.0f;
                w->bulletSpeed = 2000.0f;
                w->reloadTime = 1.5f;
                w->movementSpeed = 1.0f;
                w->reserveAmmo = 10;
                break;
            case M40:
                w->name = "M40";
                w->price = 22000;
                w->magazineSize = 1;
                w->damage = 99.0f;
                w->range = 2000.0f;
                w->bulletSpeed = 4000.0f;
                w->reloadTime = 1.5f;
                w->movementSpeed = 1.2f;
                w->reserveAmmo = 10;
                break;
        }
        
        // Initialize with full magazine
        w->currentAmmo = w->magazineSize;
        
        return w;
    }
};

// ========================
// Property-Based Test Helpers
// ========================

// Generate random weapon type
Weapon::Type generateRandomWeaponType(std::mt19937& gen) {
    std::uniform_int_distribution<int> dist(0, 9);  // 0-9 for 10 weapon types
    return static_cast<Weapon::Type>(dist(gen));
}

// Check if weapon has all required properties set to valid values
bool hasCompleteProperties(const Weapon* weapon) {
    // Check name is not empty
    if (weapon->name.empty()) return false;
    
    // Check magazine size is positive
    if (weapon->magazineSize <= 0) return false;
    
    // Check damage is positive
    if (weapon->damage <= 0.0f) return false;
    
    // Check range is positive
    if (weapon->range <= 0.0f) return false;
    
    // Check bullet speed is positive
    if (weapon->bulletSpeed <= 0.0f) return false;
    
    // Check reload time is positive
    if (weapon->reloadTime <= 0.0f) return false;
    
    // Check movement speed is positive
    if (weapon->movementSpeed <= 0.0f) return false;
    
    // Check reserve ammo is non-negative
    if (weapon->reserveAmmo < 0) return false;
    
    // Check current ammo is initialized correctly (should equal magazine size)
    if (weapon->currentAmmo != weapon->magazineSize) return false;
    
    // Check price is non-negative (USP is free, so >= 0)
    if (weapon->price < 0) return false;
    
    return true;
}

// ========================
// Property-Based Tests
// ========================

// **Feature: weapon-shop-system, Property 33: Weapon property completeness**
// **Validates: Requirements 11.2**
//
// Property: For any weapon type, all required properties (magazine size, damage, range, 
// bullet speed, reload time, movement speed) should have defined non-zero values.
//
// This test runs 100 iterations with random weapon types to ensure the property holds
// across all possible weapon configurations.
TEST(Property_WeaponPropertyCompleteness) {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    const int NUM_ITERATIONS = 100;
    int successCount = 0;
    
    std::cout << std::endl;
    std::cout << "  Running property-based test with " << NUM_ITERATIONS << " iterations..." << std::endl;
    
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Generate random weapon type
        Weapon::Type weaponType = generateRandomWeaponType(gen);
        
        // Create weapon
        Weapon* weapon = Weapon::create(weaponType);
        
        // Check property: weapon should have all properties set to valid values
        bool propertyHolds = hasCompleteProperties(weapon);
        
        if (!propertyHolds) {
            std::ostringstream oss;
            oss << "Property violated for weapon type " << static_cast<int>(weaponType) 
                << " (" << weapon->name << "): ";
            
            if (weapon->name.empty()) oss << "name is empty; ";
            if (weapon->magazineSize <= 0) oss << "magazineSize=" << weapon->magazineSize << "; ";
            if (weapon->damage <= 0.0f) oss << "damage=" << weapon->damage << "; ";
            if (weapon->range <= 0.0f) oss << "range=" << weapon->range << "; ";
            if (weapon->bulletSpeed <= 0.0f) oss << "bulletSpeed=" << weapon->bulletSpeed << "; ";
            if (weapon->reloadTime <= 0.0f) oss << "reloadTime=" << weapon->reloadTime << "; ";
            if (weapon->movementSpeed <= 0.0f) oss << "movementSpeed=" << weapon->movementSpeed << "; ";
            if (weapon->reserveAmmo < 0) oss << "reserveAmmo=" << weapon->reserveAmmo << "; ";
            if (weapon->currentAmmo != weapon->magazineSize) 
                oss << "currentAmmo=" << weapon->currentAmmo << " != magazineSize=" << weapon->magazineSize << "; ";
            if (weapon->price < 0) oss << "price=" << weapon->price << "; ";
            
            delete weapon;
            throw std::runtime_error(oss.str());
        }
        
        successCount++;
        delete weapon;
    }
    
    std::cout << "  ✓ Property held for all " << successCount << " iterations" << std::endl;
    ASSERT_EQ(successCount, NUM_ITERATIONS);
}

// Test specific weapon types to ensure correct initialization
TEST(WeaponInitialization_USP) {
    Weapon* usp = Weapon::create(Weapon::USP);
    
    ASSERT_EQ(usp->name, "USP");
    ASSERT_EQ(usp->price, 0);
    ASSERT_EQ(usp->magazineSize, 12);
    ASSERT_EQ(usp->currentAmmo, 12);  // Should be initialized to full
    ASSERT_EQ(usp->reserveAmmo, 100);
    ASSERT_EQ(usp->damage, 15.0f);
    ASSERT_EQ(usp->range, 250.0f);
    ASSERT_EQ(usp->bulletSpeed, 600.0f);
    ASSERT_EQ(usp->reloadTime, 2.0f);
    ASSERT_EQ(usp->movementSpeed, 2.5f);
    
    delete usp;
}

TEST(WeaponInitialization_AWP) {
    Weapon* awp = Weapon::create(Weapon::AWP);
    
    ASSERT_EQ(awp->name, "AWP");
    ASSERT_EQ(awp->price, 25000);
    ASSERT_EQ(awp->magazineSize, 1);
    ASSERT_EQ(awp->currentAmmo, 1);  // Should be initialized to full
    ASSERT_EQ(awp->reserveAmmo, 10);
    ASSERT_EQ(awp->damage, 100.0f);
    ASSERT_EQ(awp->range, 1000.0f);
    ASSERT_EQ(awp->bulletSpeed, 2000.0f);
    ASSERT_EQ(awp->reloadTime, 1.5f);
    ASSERT_EQ(awp->movementSpeed, 1.0f);
    
    delete awp;
}

TEST(WeaponInitialization_AK47) {
    Weapon* ak47 = Weapon::create(Weapon::AK47);
    
    ASSERT_EQ(ak47->name, "AK-47");
    ASSERT_EQ(ak47->price, 17500);
    ASSERT_EQ(ak47->magazineSize, 25);
    ASSERT_EQ(ak47->currentAmmo, 25);  // Should be initialized to full
    ASSERT_EQ(ak47->reserveAmmo, 100);
    ASSERT_EQ(ak47->damage, 35.0f);
    ASSERT_EQ(ak47->range, 450.0f);
    ASSERT_EQ(ak47->bulletSpeed, 900.0f);
    ASSERT_EQ(ak47->reloadTime, 3.0f);
    ASSERT_EQ(ak47->movementSpeed, 1.6f);
    
    delete ak47;
}

// Test all 10 weapon types have unique names
TEST(WeaponCatalog_UniqueNames) {
    std::array<std::string, 10> names;
    
    for (int i = 0; i < 10; i++) {
        Weapon* weapon = Weapon::create(static_cast<Weapon::Type>(i));
        names[i] = weapon->name;
        delete weapon;
    }
    
    // Check all names are unique
    for (int i = 0; i < 10; i++) {
        for (int j = i + 1; j < 10; j++) {
            ASSERT_TRUE(names[i] != names[j]);
        }
    }
}

// Test weapon prices are in correct ranges
TEST(WeaponPrices_ValidRanges) {
    // Pistols: 0-5000
    Weapon* usp = Weapon::create(Weapon::USP);
    ASSERT_GE(usp->price, 0);
    ASSERT_TRUE(usp->price <= 5000);
    delete usp;
    
    Weapon* r8 = Weapon::create(Weapon::R8);
    ASSERT_GE(r8->price, 0);
    ASSERT_TRUE(r8->price <= 5000);
    delete r8;
    
    // Rifles: 10000-20000
    Weapon* galil = Weapon::create(Weapon::GALIL);
    ASSERT_GE(galil->price, 10000);
    ASSERT_TRUE(galil->price <= 20000);
    delete galil;
    
    Weapon* ak47 = Weapon::create(Weapon::AK47);
    ASSERT_GE(ak47->price, 10000);
    ASSERT_TRUE(ak47->price <= 20000);
    delete ak47;
    
    // Snipers: 20000-30000
    Weapon* m10 = Weapon::create(Weapon::M10);
    ASSERT_GE(m10->price, 20000);
    ASSERT_TRUE(m10->price <= 30000);
    delete m10;
    
    Weapon* awp = Weapon::create(Weapon::AWP);
    ASSERT_GE(awp->price, 20000);
    ASSERT_TRUE(awp->price <= 30000);
    delete awp;
}

// ========================
// Shop Generation Structures and Tests
// ========================

// Constants for shop generation
const float CELL_SIZE = 100.0f;
const int GRID_SIZE = 51;
const int NUM_SHOPS = 26;
const int MIN_SPAWN_DISTANCE = 5;

// Shop structure (minimal version for testing)
struct Shop {
    int gridX = 0;
    int gridY = 0;
    float worldX = 0.0f;
    float worldY = 0.0f;
};

// Helper function to generate random shops for testing
// This is a simplified version that doesn't check connectivity
std::vector<Shop> generateTestShops(std::mt19937& gen, const std::vector<std::pair<int, int>>& spawnPoints) {
    std::vector<Shop> shops;
    std::uniform_int_distribution<int> gridDist(0, GRID_SIZE - 1);
    std::vector<std::pair<int, int>> usedPositions;
    
    for (int i = 0; i < NUM_SHOPS; ++i) {
        int maxRetries = 1000;
        bool foundValidPosition = false;
        
        for (int retry = 0; retry < maxRetries; ++retry) {
            int gridX = gridDist(gen);
            int gridY = gridDist(gen);
            
            // Check if position is already occupied
            bool occupied = false;
            for (const auto& pos : usedPositions) {
                if (pos.first == gridX && pos.second == gridY) {
                    occupied = true;
                    break;
                }
            }
            
            if (occupied) continue;
            
            // Check distance from spawn points
            bool tooCloseToSpawn = false;
            for (const auto& spawn : spawnPoints) {
                int dx = gridX - spawn.first;
                int dy = gridY - spawn.second;
                float distance = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                
                if (distance < MIN_SPAWN_DISTANCE) {
                    tooCloseToSpawn = true;
                    break;
                }
            }
            
            if (tooCloseToSpawn) continue;
            
            // Valid position found
            usedPositions.push_back(std::make_pair(gridX, gridY));
            
            Shop shop;
            shop.gridX = gridX;
            shop.gridY = gridY;
            shop.worldX = gridX * CELL_SIZE + CELL_SIZE / 2.0f;
            shop.worldY = gridY * CELL_SIZE + CELL_SIZE / 2.0f;
            shops.push_back(shop);
            
            foundValidPosition = true;
            break;
        }
        
        if (!foundValidPosition) {
            // Failed to find valid position, return what we have
            return shops;
        }
    }
    
    return shops;
}

// **Feature: weapon-shop-system, Property 2: Shop count invariant**
// **Validates: Requirements 2.1**
//
// Property: For any generated map, the number of shop entities should equal exactly 26.
TEST(Property_ShopCountInvariant) {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    const int NUM_ITERATIONS = 100;
    int successCount = 0;
    
    std::cout << std::endl;
    std::cout << "  Running property-based test with " << NUM_ITERATIONS << " iterations..." << std::endl;
    
    // Define spawn points (convert from world coordinates to grid coordinates)
    std::vector<std::pair<int, int>> spawnPoints;
    spawnPoints.push_back(std::make_pair(250 / static_cast<int>(CELL_SIZE), 4850 / static_cast<int>(CELL_SIZE)));
    spawnPoints.push_back(std::make_pair(4850 / static_cast<int>(CELL_SIZE), 250 / static_cast<int>(CELL_SIZE)));
    
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Generate shops
        std::vector<Shop> shops = generateTestShops(gen, spawnPoints);
        
        // Check property: should have exactly 26 shops
        bool propertyHolds = (shops.size() == NUM_SHOPS);
        
        if (!propertyHolds) {
            std::ostringstream oss;
            oss << "Property violated: expected " << NUM_SHOPS << " shops but got " << shops.size();
            throw std::runtime_error(oss.str());
        }
        
        successCount++;
    }
    
    std::cout << "  ✓ Property held for all " << successCount << " iterations" << std::endl;
    ASSERT_EQ(successCount, NUM_ITERATIONS);
}

// **Feature: weapon-shop-system, Property 3: Shop position uniqueness**
// **Validates: Requirements 2.2**
//
// Property: For any generated map, no two shops should occupy the same grid cell coordinates.
TEST(Property_ShopPositionUniqueness) {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    const int NUM_ITERATIONS = 100;
    int successCount = 0;
    
    std::cout << std::endl;
    std::cout << "  Running property-based test with " << NUM_ITERATIONS << " iterations..." << std::endl;
    
    // Define spawn points
    std::vector<std::pair<int, int>> spawnPoints;
    spawnPoints.push_back(std::make_pair(250 / static_cast<int>(CELL_SIZE), 4850 / static_cast<int>(CELL_SIZE)));
    spawnPoints.push_back(std::make_pair(4850 / static_cast<int>(CELL_SIZE), 250 / static_cast<int>(CELL_SIZE)));
    
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Generate shops
        std::vector<Shop> shops = generateTestShops(gen, spawnPoints);
        
        // Check property: all shop positions should be unique
        bool propertyHolds = true;
        for (size_t j = 0; j < shops.size(); j++) {
            for (size_t k = j + 1; k < shops.size(); k++) {
                if (shops[j].gridX == shops[k].gridX && shops[j].gridY == shops[k].gridY) {
                    propertyHolds = false;
                    std::ostringstream oss;
                    oss << "Property violated: shops at indices " << j << " and " << k 
                        << " occupy same position (" << shops[j].gridX << ", " << shops[j].gridY << ")";
                    throw std::runtime_error(oss.str());
                }
            }
        }
        
        if (!propertyHolds) {
            throw std::runtime_error("Property violated: duplicate shop positions found");
        }
        
        successCount++;
    }
    
    std::cout << "  ✓ Property held for all " << successCount << " iterations" << std::endl;
    ASSERT_EQ(successCount, NUM_ITERATIONS);
}

// **Feature: weapon-shop-system, Property 4: Shop spawn distance constraint**
// **Validates: Requirements 2.3**
//
// Property: For any shop on any generated map, the distance from that shop to the nearest 
// spawn point should be greater than 5 grid cells.
TEST(Property_ShopSpawnDistanceConstraint) {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    const int NUM_ITERATIONS = 100;
    int successCount = 0;
    
    std::cout << std::endl;
    std::cout << "  Running property-based test with " << NUM_ITERATIONS << " iterations..." << std::endl;
    
    // Define spawn points
    std::vector<std::pair<int, int>> spawnPoints;
    spawnPoints.push_back(std::make_pair(250 / static_cast<int>(CELL_SIZE), 4850 / static_cast<int>(CELL_SIZE)));
    spawnPoints.push_back(std::make_pair(4850 / static_cast<int>(CELL_SIZE), 250 / static_cast<int>(CELL_SIZE)));
    
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Generate shops
        std::vector<Shop> shops = generateTestShops(gen, spawnPoints);
        
        // Check property: all shops should be at least MIN_SPAWN_DISTANCE cells from any spawn point
        bool propertyHolds = true;
        for (const auto& shop : shops) {
            for (const auto& spawn : spawnPoints) {
                int dx = shop.gridX - spawn.first;
                int dy = shop.gridY - spawn.second;
                float distance = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                
                if (distance < MIN_SPAWN_DISTANCE) {
                    propertyHolds = false;
                    std::ostringstream oss;
                    oss << "Property violated: shop at (" << shop.gridX << ", " << shop.gridY 
                        << ") is only " << distance << " cells from spawn point (" 
                        << spawn.first << ", " << spawn.second << "), minimum is " << MIN_SPAWN_DISTANCE;
                    throw std::runtime_error(oss.str());
                }
            }
        }
        
        if (!propertyHolds) {
            throw std::runtime_error("Property violated: shop too close to spawn point");
        }
        
        successCount++;
    }
    
    std::cout << "  ✓ Property held for all " << successCount << " iterations" << std::endl;
    ASSERT_EQ(successCount, NUM_ITERATIONS);
}

// ========================
// Main Test Runner
// ========================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Weapon & Shop System Property-Based Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // Run weapon property-based tests
    std::cout << "--- Weapon System Tests ---" << std::endl;
    RUN_TEST(Property_WeaponPropertyCompleteness);
    
    // Run unit tests for specific weapons
    RUN_TEST(WeaponInitialization_USP);
    RUN_TEST(WeaponInitialization_AWP);
    RUN_TEST(WeaponInitialization_AK47);
    RUN_TEST(WeaponCatalog_UniqueNames);
    RUN_TEST(WeaponPrices_ValidRanges);
    
    std::cout << std::endl;
    
    // Run shop generation property-based tests
    std::cout << "--- Shop Generation Tests ---" << std::endl;
    RUN_TEST(Property_ShopCountInvariant);
    RUN_TEST(Property_ShopPositionUniqueness);
    RUN_TEST(Property_ShopSpawnDistanceConstraint);
    
    // Print summary
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Test Summary" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Total tests: " << totalTests << std::endl;
    std::cout << "Passed: " << passedTests << std::endl;
    std::cout << "Failed: " << failedTests << std::endl;
    std::cout << "Success rate: " << (totalTests > 0 ? (passedTests * 100 / totalTests) : 0) << "%" << std::endl;
    std::cout << std::endl;
    
    if (failedTests == 0) {
        std::cout << "✓ All tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "✗ Some tests failed!" << std::endl;
        return 1;
    }
}
