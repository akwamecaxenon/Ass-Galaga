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
        
        CONSOLE_CURSOR_INFO cursorInfo;
        GetConsoleCursorInfo(consoleHandle, &cursorInfo);
        cursorInfo.bVisible = false;
        SetConsoleCursorInfo(consoleHandle, &cursorInfo);
        
        system("mode con: cols=80 lines=40");
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
        SetConsoleCursorPosition(consoleHandle, {0, 0});
        
        cout << string(WIDTH, '=') << endl;
        
        for (int y = 0; y < HEIGHT; y++) {
            cout << buffer[y] << endl;
        }
        
        cout << string(WIDTH, '=') << endl;
        
        cout.flush();
    }
};

struct Bullet {
    int x, y;
    bool active;
    bool playerBullet;
    int moveCounter;
    bool bossSpreadBullet;
    
    Bullet(int x, int y, bool isPlayer, bool spread = false) : 
        x(x), y(y), active(true), playerBullet(isPlayer), moveCounter(0), bossSpreadBullet(spread) {}
    
    void update() {
        if (playerBullet) {
            y -= 2;
        } else {
            if (bossSpreadBullet) {
                moveCounter++;
                if (moveCounter % 3 == 0) {
                    y += 1;
                    x += (moveCounter % 6 < 3) ? 1 : -1;
                }
            } else {
                moveCounter++;
                if (moveCounter % 5 == 0) {
                    y += 1;
                }
            }
        }
        
        if (y < 0 || y >= HEIGHT || x < 0 || x >= WIDTH) active = false;
    }
};

struct Enemy {
    int x, y;
    bool alive;
    bool isBoss;
    bool isGiantBoss;
    int health;
    int direction;
    int moveCounter;
    int shootCooldown;
    int patternCounter;
    
    Enemy(int x, int y, bool boss = false, bool giant = false) : 
        x(x), y(y), alive(true), isBoss(boss), isGiantBoss(giant), 
        direction(1), moveCounter(0), shootCooldown(0), patternCounter(0) {
        if (isGiantBoss) {
            health = 15;
        } else if (isBoss) {
            health = 3;
        } else {
            health = 1;
        }
    }
    
    void update() {
        moveCounter++;
        patternCounter++;
        
        if (isGiantBoss) {
            if (moveCounter % 4 == 0) {
                x += direction;
                
                if (patternCounter > 60) {
                    if (rand() % 100 < 5) {
                        direction = -direction;
                    }
                    if (patternCounter > 120) {
                        patternCounter = 0;
                        y += 1;
                    }
                }
                
                if (x <= 5) x = 5;
                if (x >= WIDTH - 6) x = WIDTH - 6;
                if (y >= HEIGHT - 8) y = HEIGHT - 8;
            }
        } else if (isBoss) {
            if (moveCounter % 3 == 0) {
                x += direction;
                if (x <= 1 || x >= WIDTH - 2) {
                    direction = -direction;
                    if (rand() % 2 == 0) {
                        y++;
                    }
                }
            }
        } else {
            if (moveCounter % 3 == 0) {
                x += direction;
                if (x <= 1 || x >= WIDTH - 2) {
                    direction = -direction;
                }
            }
        }
        
        if (shootCooldown > 0) shootCooldown--;
    }
    
    vector<Bullet> shoot() {
        vector<Bullet> newBullets;
        
        if (shootCooldown > 0) return newBullets;
        
        if (isGiantBoss) {
            if (patternCounter % 30 == 0) {
                for (int i = -2; i <= 2; i++) {
                    newBullets.emplace_back(x + i, y + 3, false, true);
                }
                shootCooldown = 45;
            }
            
            if (patternCounter % 60 == 0) {
                newBullets.emplace_back(x - 4, y + 3, false, false);
                newBullets.emplace_back(x + 4, y + 3, false, false);
                newBullets.emplace_back(x, y + 3, false, false);
                shootCooldown = 30;
            }
        } else if (isBoss) {
            if (rand() % 150 < 2) {
                newBullets.emplace_back(x, y + 1, false);
                shootCooldown = 25;
            }
        } else {
            if (rand() % 200 < 2) {
                newBullets.emplace_back(x, y + 1, false);
                shootCooldown = 30;
            }
        }
        
        return newBullets;
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
    bool showTitleScreen;
    bool showMechanics;
    int titleSelection;
    
    vector<Bullet> bullets;
    vector<Enemy> enemies;
    
    bool leftPressed;
    bool rightPressed;
    bool spacePressed;
    bool upPressed;
    bool downPressed;
    bool enterPressed;
    
    chrono::steady_clock::time_point lastFrame;
    int frameCount;
    int shootCooldown;
    int mechanicsDisplayTime;
    bool giantBossSpawnedThisWave;
    
public:
    ResponsiveGame() : playerX(WIDTH / 2), score(0), lives(10), wave(1), gameOver(false),
                      showTitleScreen(true), showMechanics(false), titleSelection(0),
                      leftPressed(false), rightPressed(false), spacePressed(false),
                      upPressed(false), downPressed(false), enterPressed(false),
                      frameCount(0), shootCooldown(0), mechanicsDisplayTime(0),
                      giantBossSpawnedThisWave(false) {
        createEnemyWave();
        lastFrame = chrono::steady_clock::now();
    }
    
    void createEnemyWave() {
        enemies.clear();
        giantBossSpawnedThisWave = false;
        
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 3; j++) {
                Enemy enemy(10 + i * 8, 3 + j * 2);
                enemy.direction = (j % 2 == 0) ? 1 : -1;
                enemies.push_back(enemy);
            }
        }
    }
    
    void spawnRegularBoss() {
        Enemy boss(WIDTH / 2, 2, true, false);
        enemies.push_back(boss);
    }
    
    void spawnGiantBoss() {
        Enemy giantBoss(WIDTH / 2, 3, true, true);
        enemies.push_back(giantBoss);
        giantBossSpawnedThisWave = true;
        cout << endl << " WARNING: GIANT BOSS INCOMING! " << endl;
    }
    
    void drawGiantBoss(int x, int y, int health) {
        renderer.setChar(x, y, '^');
        
        renderer.setChar(x - 1, y + 1, '/');
        renderer.setChar(x, y + 1, '|');
        renderer.setChar(x + 1, y + 1, '\\');
        
        renderer.setChar(x - 2, y + 2, '[');
        renderer.setChar(x - 1, y + 2, '=');
        renderer.setChar(x, y + 2, 'O');
        renderer.setChar(x + 1, y + 2, '=');
        renderer.setChar(x + 2, y + 2, ']');
        
        renderer.setChar(x - 3, y + 3, '<');
        renderer.setChar(x - 2, y + 3, '=');
        renderer.setChar(x - 1, y + 3, '=');
        renderer.setChar(x, y + 3, 'V');
        renderer.setChar(x + 1, y + 3, '=');
        renderer.setChar(x + 2, y + 3, '=');
        renderer.setChar(x + 3, y + 3, '>');
        
        renderer.setChar(x - 2, y + 4, '|');
        renderer.setChar(x - 1, y + 4, '|');
        renderer.setChar(x, y + 4, '|');
        renderer.setChar(x + 1, y + 4, '|');
        renderer.setChar(x + 2, y + 4, '|');
        
        int healthBarWidth = 15;
        int healthSegment = (healthBarWidth * health) / 15;
        if (healthSegment < 1) healthSegment = 1;
        
        for (int i = 0; i < healthBarWidth; i++) {
            char healthChar;
            if (i < healthSegment) {
                healthChar = '=';
            } else {
                healthChar = ' ';
            }
            renderer.setChar(x - healthBarWidth/2 + i, y - 1, healthChar);
        }
        
        string bossLabel = "BOSS";
        for (int i = 0; i < bossLabel.length(); i++) {
            renderer.setChar(x - 2 + i, y - 2, bossLabel[i]);
        }
    }
    
    void updateInput() {
        leftPressed = rightPressed = spacePressed = false;
        upPressed = downPressed = enterPressed = false;
        
        while (_kbhit()) {
            char key = _getch();
            
            if (key == 0 || key == -32) {
                key = _getch();
                switch (key) {
                    case 72: upPressed = true; break;
                    case 80: downPressed = true; break;
                    case 75: leftPressed = true; break;
                    case 77: rightPressed = true; break;
                }
            } else {
                switch (key) {
                    case 'a': case 'A': leftPressed = true; break;
                    case 'd': case 'D': rightPressed = true; break;
                    case 'w': case 'W': upPressed = true; break;
                    case 's': case 'S': downPressed = true; break;
                    case ' ': spacePressed = true; break;
                    case '\r': enterPressed = true; break;
                    case 'q': case 'Q': gameOver = true; break;
                    case 'r': case 'R': resetGame(); break;
                    case 'm': case 'M': toggleMechanics(); break;
                }
            }
        }
        
        if (showTitleScreen) {
            if (upPressed) titleSelection = (titleSelection - 1 + 2) % 2;
            if (downPressed) titleSelection = (titleSelection + 1) % 2;
            if (enterPressed) {
                if (titleSelection == 0) {
                    showTitleScreen = false;
                } else {
                    gameOver = true;
                }
            }
            return;
        }
        
        if (rightPressed && leftPressed) {
            if (playerX < WIDTH / 2) {
                leftPressed = false;
            } else {
                rightPressed = false;
            }
        }
        
        if (rightPressed && !leftPressed) {
            if (playerX < WIDTH - 2) playerX += 2;
        } else if (leftPressed && !rightPressed) {
            if (playerX > 1) playerX -= 2;
        }
        
        if (spacePressed && shootCooldown <= 0) {
            bullets.emplace_back(playerX, PLAYER_POS - 1, true);
            shootCooldown = 8;
        }
        
        if (shootCooldown > 0) shootCooldown--;
        
        if (mechanicsDisplayTime > 0) {
            mechanicsDisplayTime--;
            if (mechanicsDisplayTime == 0) {
                showMechanics = false;
            }
        }
    }
    
    void toggleMechanics() {
        showMechanics = !showMechanics;
        if (showMechanics) {
            mechanicsDisplayTime = 180;
        }
    }
    
    void updateGame() {
        if (gameOver || showTitleScreen) return;
        
        auto currentTime = chrono::steady_clock::now();
        float deltaTime = chrono::duration<float>(currentTime - lastFrame).count();
        lastFrame = currentTime;
        frameCount++;
        
        for (auto& bullet : bullets) {
            bullet.update();
        }
        
        bullets.erase(remove_if(bullets.begin(), bullets.end(),
            [](const Bullet& b) { return !b.active; }), bullets.end());
        
        for (auto& enemy : enemies) {
            if (enemy.alive) {
                enemy.update();
                
                vector<Bullet> newBullets = enemy.shoot();
                for (const auto& bullet : newBullets) {
                    bullets.push_back(bullet);
                }
            }
        }
        
        checkCollisions();
        
        if (frameCount % 900 == 0) {
            spawnRegularBoss();
        }
        
        bool allRegularEnemiesDead = true;
        for (const auto& enemy : enemies) {
            if (enemy.alive && !enemy.isBoss) {
                allRegularEnemiesDead = false;
                break;
            }
        }
        
        if (allRegularEnemiesDead && enemies.size() > 0) {
            bool hasBoss = false;
            for (const auto& enemy : enemies) {
                if (enemy.alive && enemy.isBoss) {
                    hasBoss = true;
                    break;
                }
            }
            
            if (!hasBoss) {
                if (wave % 3 == 0 && !giantBossSpawnedThisWave) {
                    spawnGiantBoss();
                } else {
                    wave++;
                    createEnemyWave();
                }
            }
        }
        
        if (giantBossSpawnedThisWave) {
            bool giantBossAlive = false;
            for (const auto& enemy : enemies) {
                if (enemy.alive && enemy.isGiantBoss) {
                    giantBossAlive = true;
                    break;
                }
            }
            
            if (!giantBossAlive && giantBossSpawnedThisWave) {
                static int giantBossDefeatTimer = 0;
                giantBossDefeatTimer++;
                
                if (giantBossDefeatTimer > 180) {
                    wave++;
                    createEnemyWave();
                    giantBossDefeatTimer = 0;
                }
            }
        }
        
        if (lives <= 0) gameOver = true;
    }
    
    void checkCollisions() {
        for (auto& bullet : bullets) {
            if (!bullet.playerBullet || !bullet.active) continue;
            
            for (auto& enemy : enemies) {
                if (!enemy.alive) continue;
                
                if (enemy.isGiantBoss) {
                    int distanceX = abs(bullet.x - enemy.x);
                    int distanceY = abs(bullet.y - enemy.y);
                    
                    if (distanceX <= 4 && distanceY <= 2) {
                        enemy.health--;
                        bullet.active = false;
                        
                        if (enemy.health <= 0) {
                            enemy.alive = false;
                            score += 300;
                        }
                        break;
                    }
                } else {
                    int distanceX = abs(bullet.x - enemy.x);
                    int distanceY = abs(bullet.y - enemy.y);
                    
                    if (distanceX <= 1 && distanceY <= 1) {
                        enemy.health--;
                        bullet.active = false;
                        
                        if (enemy.health <= 0) {
                            enemy.alive = false;
                            if (enemy.isBoss) {
                                score += 100;
                            } else {
                                score += 10;
                            }
                        }
                        break;
                    }
                }
            }
        }
        
        for (auto& bullet : bullets) {
            if (bullet.playerBullet || !bullet.active) continue;
            
            if (bullet.y == PLAYER_POS && abs(bullet.x - playerX) <= 1) {
                bullet.active = false;
                lives--;
                if (lives <= 0) gameOver = true;
            }
        }
    }
    
    void renderTitleScreen() {
        renderer.clear();
        
        string title = "RESPONSIVE GALAGA";
        int titleX = (WIDTH - title.length()) / 2;
        
        for (int i = 0; i < title.length(); i++) {
            renderer.setChar(titleX + i, 5, title[i]);
        }
        
        string startText = "> START GAME <";
        string exitText = "> EXIT GAME <";
        
        if (titleSelection == 0) {
            startText = "> START GAME <";
            exitText = "  EXIT GAME  ";
        } else {
            startText = "  START GAME  ";
            exitText = "> EXIT GAME <";
        }
    
        int startX = (WIDTH - startText.length()) / 2;
        int exitX = (WIDTH - exitText.length()) / 2;
        
        for (int i = 0; i < startText.length(); i++) {
            renderer.setChar(startX + i, 10, startText[i]);
        }
        
        for (int i = 0; i < exitText.length(); i++) {
            renderer.setChar(exitX + i, 12, exitText[i]);
        }
        
        string controls = "CONTROLS: ARROWS/ENTER/M";
        int controlsX = (WIDTH - controls.length()) / 2;
        for (int i = 0; i < controls.length(); i++) {
            renderer.setChar(controlsX + i, 16, controls[i]);
        }
        
        renderer.draw();
        
        cout << "\r\n";
        cout << "  FEATURES:                                         \r\n";
        cout << "  - Classic space shooter                           \r\n";
        cout << "  - Regular & GIANT BOSS battles!                   \r\n";
        cout << "  - Responsive controls                             \r\n";
        cout << "  - Multiple enemy waves                            \r\n";
        cout << "  - Mechanics display (M key)                       \r\n";
        cout << "\r\n";
        cout << "  MECHANICS:                                        \r\n";
        cout << "  - Enemies: 10 pts, Bosses: 100 pts               \r\n";
        cout << "  - GIANT BOSS: 300 pts, 15 HP!                    \r\n";
        cout << "  - Giant boss appears every 3 waves               \r\n";
        cout << "  - Start with 10 lives                            \r\n";
        cout << "\r\n";
    }
    
    void renderMechanics() {
        int boxWidth = 50;
        int boxHeight = 16;
        int boxX = (WIDTH - boxWidth) / 2;
        int boxY = (HEIGHT - boxHeight) / 2;
        
        for (int x = boxX; x < boxX + boxWidth; x++) {
            renderer.setChar(x, boxY, '=');
            renderer.setChar(x, boxY + boxHeight - 1, '=');
        }
        for (int y = boxY; y < boxY + boxHeight; y++) {
            renderer.setChar(boxX, y, '|');
            renderer.setChar(boxX + boxWidth - 1, y, '|');
        }
        
        string title = "GAME MECHANICS";
        int titleX = boxX + (boxWidth - title.length()) / 2;
        for (int i = 0; i < title.length(); i++) {
            renderer.setChar(titleX + i, boxY + 1, title[i]);
        }
        
        vector<string> mechanics = {
            "CONTROLS:",
            "  A/D or ARROWS: Move",
            "  SPACE: Shoot",
            "  M: Show/Hide Mechanics",
            "  R: Restart",
            "  Q: Quit",
            "",
            "SCORING:",
            "  Regular Enemy: 10 pts",
            "  Boss Enemy: 100 pts",
            "  GIANT BOSS: 300 pts",
            "  Boss HP: 3, Giant: 15",
            "",
            "GIANT BOSS SPAWN:",
            "  - Appears every 3 waves",
            "  - Only after clearing all",
            "    regular enemies",
            "",
            "Press M to close"
        };
        
        for (int i = 0; i < mechanics.size(); i++) {
            int textX = boxX + 2;
            string line = mechanics[i];
            for (int j = 0; j < line.length(); j++) {
                if (textX + j < boxX + boxWidth - 2) {
                    renderer.setChar(textX + j, boxY + 3 + i, line[j]);
                }
            }
        }
    }
    
    void render() {
        if (showTitleScreen) {
            renderTitleScreen();
            return;
        }
        
        renderer.clear();
        
        renderer.setChar(playerX, PLAYER_POS, 'A');
        renderer.setChar(playerX - 1, PLAYER_POS, '<');
        renderer.setChar(playerX + 1, PLAYER_POS, '>');
        
        for (const auto& bullet : bullets) {
            if (bullet.active) {
                if (bullet.bossSpreadBullet) {
                    renderer.setChar(bullet.x, bullet.y, '*');
                } else {
                    renderer.setChar(bullet.x, bullet.y, bullet.playerBullet ? '|' : '!');
                }
            }
        }
        
        for (const auto& enemy : enemies) {
            if (enemy.alive) {
                if (enemy.isGiantBoss) {
                    drawGiantBoss(enemy.x, enemy.y, enemy.health);
                } else if (enemy.isBoss) {
                    renderer.setChar(enemy.x, enemy.y, 'B');
                    renderer.setChar(enemy.x - 1, enemy.y, '[');
                    renderer.setChar(enemy.x + 1, enemy.y, ']');
                    
                    int healthWidth = enemy.health;
                    for (int i = 0; i < healthWidth; i++) {
                        renderer.setChar(enemy.x - 1 + i, enemy.y - 1, '=');
                    }
                } else {
                    renderer.setChar(enemy.x, enemy.y, 'E');
                    renderer.setChar(enemy.x - 1, enemy.y, '-');
                    renderer.setChar(enemy.x + 1, enemy.y, '-');
                }
            }
        }
        
        if (showMechanics) {
            renderMechanics();
        }
        
        renderer.draw();
        
        cout << " Score: " << score << " | Lives: " << lives << " | Wave: " << wave;
        
        for (const auto& enemy : enemies) {
            if (enemy.alive && enemy.isGiantBoss) {
                cout << " | GIANT BOSS: " << enemy.health << " HP";
                break;
            }
        }
        
        if (wave % 3 == 0 && !giantBossSpawnedThisWave) {
            cout << " | NEXT: GIANT BOSS WAVE!";
        }
        
        cout << " | Enemies: " << count_if(enemies.begin(), enemies.end(), 
                                           [](const Enemy& e) { return e.alive; });
        
        cout << endl;
        
        cout << " [A/D] Move | [SPACE] Shoot | [R] Restart | [Q] Quit";
        cout << " | [M] Mechanics";
        
        if (gameOver) {
            cout << endl << " GAME OVER! Press R to restart or Q to quit";
        }
        
        cout << endl;
        
        if (!showMechanics) {
            vector<string> hints = {
                "TIP: GIANT BOSS appears every 3 waves after clearing enemies!",
                "TIP: Giant boss has 15 HP and fires spread patterns!",
                "TIP: Regular bosses (100 pts) appear every 15 seconds",
                "TIP: Regular bosses are slower and only have 3 HP!",
                "TIP: Clear all regular enemies to advance to next wave!",
                "TIP: Defeat the giant boss to earn 300 points!"
            };
            
            int hintIndex = (frameCount / 300) % hints.size();
            cout << " " << hints[hintIndex];
        }
        
        cout << endl;
    }
    
    void resetGame() {
        playerX = WIDTH / 2;
        score = 0;
        lives = 10;
        wave = 1;
        gameOver = false;
        showTitleScreen = true;
        showMechanics = false;
        titleSelection = 0;
        leftPressed = rightPressed = spacePressed = false;
        upPressed = downPressed = enterPressed = false;
        bullets.clear();
        enemies.clear();
        createEnemyWave();
        frameCount = 0;
        shootCooldown = 0;
        mechanicsDisplayTime = 0;
        giantBossSpawnedThisWave = false;
    }
    
    bool isGameOver() const { return gameOver; }
    bool isShowingTitleScreen() const { return showTitleScreen; }
};

int main() {
    system("title Responsive Galaga - Giant Boss Every 3 Waves");
    system("cls");
    
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(console, &csbi);
    
    COORD newSize;
    newSize.X = csbi.dwSize.X;
    newSize.Y = 1000;
    SetConsoleScreenBufferSize(console, newSize);
    
    ResponsiveGame game;
    
    auto lastTime = chrono::steady_clock::now();
    const double MS_PER_UPDATE = 1.0 / 60.0;
    
    while (!game.isGameOver()) {
        auto currentTime = chrono::steady_clock::now();
        double deltaTime = chrono::duration<double>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        game.updateInput();
        game.updateGame();
        game.render();
        
        this_thread::sleep_for(chrono::milliseconds(1));
    }
    
    cout << endl << "Thanks for playing!" << endl;
    system("pause");
    return 0;
}
