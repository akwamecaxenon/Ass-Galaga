#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <conio.h>
#include <windows.h>
#include <algorithm>

using namespace std;

const int WIDTH = 80;
const int HEIGHT = 24;
const int PLAYER_POS = HEIGHT - 2;

class ConsoleRenderer {
private:
    vector<string> buffer;
    HANDLE consoleHandle;
    
public:
    ConsoleRenderer() {
        buffer.resize(HEIGHT, string(WIDTH, ' '));
        consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        
        // Hide cursor
        CONSOLE_CURSOR_INFO cursorInfo;
        GetConsoleCursorInfo(consoleHandle, &cursorInfo);
        cursorInfo.bVisible = false;
        SetConsoleCursorInfo(consoleHandle, &cursorInfo);
        
        // Set window size
        system("mode con: cols=80 lines=25");
    }
    
    void clear() {
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                buffer[y][x] = ' ';
            }
        }
    }
    
    void setChar(int x, int y, char c) {
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
            buffer[y][x] = c;
        }
    }
    
    void draw() {
        // Move cursor to top-left
        SetConsoleCursorPosition(consoleHandle, {0, 0});
        
        // Draw top border
        cout << string(WIDTH, '=') << endl;
        
        // Draw buffer
        for (int y = 0; y < HEIGHT; y++) {
            cout << buffer[y] << endl;
        }
        
        // Draw bottom border
        cout << string(WIDTH, '=') << endl;
        
        // Force output
        cout.flush();
    }
};

struct Bullet {
    int x, y;
    bool active;
    bool playerBullet;
    int moveCounter;
    
    Bullet(int x, int y, bool isPlayer) : x(x), y(y), active(true), playerBullet(isPlayer), moveCounter(0) {}
    
    void update() {
        if (playerBullet) {
            y -= 2;  // Player bullets: fast
        } else {
            moveCounter++;
            if (moveCounter % 5 == 0) {  // Enemy bullets move every 3rd frame
                y += 1;
            }
        }
        
        if (y < 0 || y >= HEIGHT) active = false;
    }
};

struct Enemy {
    int x, y;
    bool alive;
    bool isBoss;
    int health;
    int direction;
    int moveCounter;
    
    Enemy(int x, int y, bool boss = false) : x(x), y(y), alive(true), isBoss(boss), direction(1), moveCounter(0) {
        health = isBoss ? 3 : 1;
    }
    
    void update() {
        moveCounter++;
        
        if (isBoss) {
            // Boss moves every frame
            x += direction;
            if (x <= 1 || x >= WIDTH - 2) {
                direction = -direction;
                y++;
            }
        } else {
            // Regular enemies move every 3 frames (smoother)
            if (moveCounter % 3 == 0) {
                x += direction;
                if (x <= 1 || x >= WIDTH - 2) {
                    direction = -direction;
                }
            }
        }
        
        // Keep on screen
        if (x < 1) x = 1;
        if (x >= WIDTH - 1) x = WIDTH - 2;
        if (y >= HEIGHT - 3) y = HEIGHT - 3;
    }
};

class ResponsiveGame {
private:
    ConsoleRenderer renderer;
    int playerX;
    int score;
    int lives;
    int wave;
    bool gameOver;
    
    vector<Bullet> bullets;
    vector<Enemy> enemies;
    
    // Input state tracking
    bool leftPressed;
    bool rightPressed;
    bool spacePressed;
    
    chrono::steady_clock::time_point lastFrame;
    int frameCount;
    int shootCooldown;
    
public:
    ResponsiveGame() : playerX(WIDTH / 2), score(0), lives(10), wave(1), gameOver(false), 
                      leftPressed(false), rightPressed(false), spacePressed(false),
                      frameCount(0), shootCooldown(0) {
        createEnemyWave();
        lastFrame = chrono::steady_clock::now();
    }
    
    void createEnemyWave() {
        enemies.clear();
        
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 3; j++) {
                Enemy enemy(10 + i * 8, 3 + j * 2);
                enemy.direction = (j % 2 == 0) ? 1 : -1;
                enemies.push_back(enemy);
            }
        }
    }
    
    void spawnBoss() {
        Enemy boss(WIDTH / 2, 2, true);
        enemies.push_back(boss);
    }
    
    void updateInput() {
        // Reset movement flags
        leftPressed = false;
        rightPressed = false;
        spacePressed = false;
        
        // Check all pending keys
        while (_kbhit()) {
            char key = _getch();
            
            if (gameOver) {
                if (key == 'r' || key == 'R') {
                    resetGame();
                }
                continue;
            }
            
            switch (key) {
                case 'a': case 'A': leftPressed = true; break;
                case 'd': case 'D': rightPressed = true; break;
                case ' ': spacePressed = true; break;
                case 'q': case 'Q': gameOver = true; break;
                case 'r': case 'R': resetGame(); break;
            }
        }
        
        // Handle movement with priority (new keys override old)
        if (rightPressed && leftPressed) {
            // If both pressed, the most recent one wins
            // Since we process all keys, we can't easily determine "most recent"
            // So we'll prioritize the direction that's NOT current
            if (playerX < WIDTH / 2) {
                // If on left side, prioritize right
                leftPressed = false;
            } else {
                // If on right side, prioritize left
                rightPressed = false;
            }
        }
        
        // Apply movement
        if (rightPressed && !leftPressed) {
            if (playerX < WIDTH - 2) playerX += 2;
        } else if (leftPressed && !rightPressed) {
            if (playerX > 1) playerX -= 2;
        }
        // If both are false, player stops (no momentum)
        
        // Handle shooting
        if (spacePressed && shootCooldown <= 0) {
            bullets.emplace_back(playerX, PLAYER_POS - 1, true);
            shootCooldown = 8; // Cooldown frames
        }
        
        if (shootCooldown > 0) shootCooldown--;
    }
    
    void updateGame() {
        if (gameOver) return;
        
        // Calculate delta time
        auto currentTime = chrono::steady_clock::now();
        float deltaTime = chrono::duration<float>(currentTime - lastFrame).count();
        lastFrame = currentTime;
        frameCount++;
        
        // Update bullets
        for (auto& bullet : bullets) {
            bullet.update();
        }
        
        // Remove inactive bullets
        bullets.erase(remove_if(bullets.begin(), bullets.end(),
            [](const Bullet& b) { return !b.active; }), bullets.end());
        
        // Update enemies
        for (auto& enemy : enemies) {
            if (enemy.alive) {
                enemy.update();
                
                // Enemy shooting
                if (rand() % 200 < 2) {
                    bullets.emplace_back(enemy.x, enemy.y + 1, false);
                }
            }
        }
        
        checkCollisions();
        
        // Spawn boss every 15 seconds
        if (frameCount % 900 == 0) {
            spawnBoss();
        }
        
        // Check wave completion
        if (all_of(enemies.begin(), enemies.end(), 
            [](const Enemy& e) { return !e.alive; })) {
            wave++;
            createEnemyWave();
        }
        
        if (lives <= 0) gameOver = true;
    }
    
    void checkCollisions() {
        // Player bullets vs enemies
        for (auto& bullet : bullets) {
            if (!bullet.playerBullet || !bullet.active) continue;
            
            for (auto& enemy : enemies) {
                if (!enemy.alive) continue;
                
                int distanceX = abs(bullet.x - enemy.x);
                int distanceY = abs(bullet.y - enemy.y);
                
                if (distanceX <= 1 && distanceY <= 1) {
                    enemy.health--;
                    bullet.active = false;
                    
                    if (enemy.health <= 0) {
                        enemy.alive = false;
                        score += enemy.isBoss ? 100 : 10;
                    }
                    break;
                }
            }
        }
        
        // Enemy bullets vs player
        for (auto& bullet : bullets) {
            if (bullet.playerBullet || !bullet.active) continue;
            
            if (bullet.y == PLAYER_POS && abs(bullet.x - playerX) <= 1) {
                bullet.active = false;
                lives--;
                if (lives <= 0) gameOver = true;
            }
        }
    }
    
    void render() {
        renderer.clear();
        
        // Draw player with better graphics
        renderer.setChar(playerX, PLAYER_POS, 'A');
        renderer.setChar(playerX - 1, PLAYER_POS, '<');
        renderer.setChar(playerX + 1, PLAYER_POS, '>');
        
        // Draw bullets
        for (const auto& bullet : bullets) {
            if (bullet.active) {
                renderer.setChar(bullet.x, bullet.y, bullet.playerBullet ? '|' : '!');
            }
        }
        
        // Draw enemies
        for (const auto& enemy : enemies) {
            if (enemy.alive) {
                if (enemy.isBoss) {
                    renderer.setChar(enemy.x, enemy.y, 'B');
                    renderer.setChar(enemy.x - 1, enemy.y, '[');
                    renderer.setChar(enemy.x + 1, enemy.y, ']');
                    
                    // Health bar
                    int healthWidth = (enemy.health * 2) / 10;
                    for (int i = 0; i < healthWidth; i++) {
                        renderer.setChar(enemy.x - 2 + i, enemy.y - 1, '=');
                    }
                } else {
                    renderer.setChar(enemy.x, enemy.y, 'E');
                    renderer.setChar(enemy.x - 1, enemy.y, '-');
                    renderer.setChar(enemy.x + 1, enemy.y, '-');
                }
            }
        }
        
        renderer.draw();
        
        // Draw UI with input state
        cout << "Score: " << score << " | Lives: " << lives << " | Wave: " << wave;
        cout << " | Input: ";
        if (leftPressed) cout << "LEFT ";
        if (rightPressed) cout << "RIGHT ";
        if (spacePressed) cout << "SHOOT ";
        if (!leftPressed && !rightPressed && !spacePressed) cout << "NONE";
        
        if (gameOver) cout << " | GAME OVER! Press R to restart";
        cout << endl << "Controls: A/D Move, Space Shoot, R Restart, Q Quit" << endl;
    }
    
    void resetGame() {
        playerX = WIDTH / 2;
        score = 0;
        lives = 10;
        wave = 1;
        gameOver = false;
        leftPressed = rightPressed = spacePressed = false;
        bullets.clear();
        createEnemyWave();
        frameCount = 0;
        shootCooldown = 0;
    }
    
    bool isGameOver() const { return gameOver; }
};

int main() {
    system("title Responsive Galaga with Boss Fights");
    system("cls");
    
    ResponsiveGame game;
    
    // High-precision game loop
    auto lastTime = chrono::steady_clock::now();
    const double MS_PER_UPDATE = 1.0 / 60.0; // 60 FPS
    
    while (!game.isGameOver()) {
        auto currentTime = chrono::steady_clock::now();
        double deltaTime = chrono::duration<double>(currentTime - lastTime).count();
        lastTime = currentTime;
        
       
        game.updateInput();
        
        
        game.updateGame();
        
        
        game.render();
        
        
        this_thread::sleep_for(chrono::milliseconds(1));
    }
    
    cout << "Thanks for playing!" << endl;
    return 0;
}