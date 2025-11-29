
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

        system("mode con: cols=80 lines=25");
    }

    void clear() {
        for (int y = 0; y < HEIGHT; y++)
            for (int x = 0; x < WIDTH; x++)
                buffer[y][x] = ' ';
    }

    void setChar(int x, int y, char c) {
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
            buffer[y][x] = c;
    }

    void draw() {
        SetConsoleCursorPosition(consoleHandle, {0, 0});
        cout << string(WIDTH, '=') << endl;

        for (int y = 0; y < HEIGHT; y++)
            cout << buffer[y] << endl;

        cout << string(WIDTH, '=') << endl;
        cout.flush();
    }
};

struct Bullet {
    int x, y;
    bool active;
    bool playerBullet;
    int moveCounter;

    Bullet(int x, int y, bool isPlayer)
        : x(x), y(y), active(true), playerBullet(isPlayer), moveCounter(0) {}

    void update() {
        if (playerBullet) {
            y -= 2;
        } else {
            moveCounter++;
            if (moveCounter % 5 == 0)
                y += 1;
        }
        if (y < 0 || y >= HEIGHT)
            active = false;
    }
};

struct Enemy {
    int x, y;
    bool alive;
    bool isBoss;
    int health;
    int direction;
    int moveCounter;

    Enemy(int x, int y, bool boss = false)
        : x(x), y(y), alive(true), isBoss(boss), direction(1), moveCounter(0) {
        health = isBoss ? 3 : 1;
    }

    void update() {
        moveCounter++;
        if (isBoss) {
            x += direction;
            if (x <= 1 || x >= WIDTH - 2) {
                direction = -direction;
                y++;
            }
        } else {
            if (moveCounter % 3 == 0) {
                x += direction;
                if (x <= 1 || x >= WIDTH - 2)
                    direction = -direction;
            }
        }
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

    bool leftPressed, rightPressed, spacePressed;

    chrono::steady_clock::time_point lastFrame;
    int frameCount;
    int shootCooldown;

public:
    ResponsiveGame()
        : playerX(WIDTH / 2), score(0), lives(10), wave(1), gameOver(false),
          leftPressed(false), rightPressed(false), spacePressed(false),
          frameCount(0), shootCooldown(0) {
        createEnemyWave();
        lastFrame = chrono::steady_clock::now();
    }

    void createEnemyWave() {
        enemies.clear();
        for (int i = 0; i < 8; i++)
            for (int j = 0; j < 3; j++) {
                Enemy e(10 + i * 8, 3 + j * 2);
                e.direction = (j % 2 == 0) ? 1 : -1;
                enemies.push_back(e);
            }
    }

    void spawnBoss() {
        Enemy boss(WIDTH / 2, 2, true);
        enemies.push_back(boss);
    }

    void updateInput() {
        leftPressed = rightPressed = spacePressed = false;

        while (_kbhit()) {
            char key = _getch();

            if (gameOver) {
                if (key == 'r' || key == 'R') resetGame();
                if (key == 'q' || key == 'Q') exit(0);
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

        if (rightPressed && !leftPressed && playerX < WIDTH - 2)
            playerX += 2;
        else if (leftPressed && !rightPressed && playerX > 1)
            playerX -= 2;

        if (spacePressed && shootCooldown <= 0) {
            bullets.emplace_back(playerX, PLAYER_POS - 1, true);
            shootCooldown = 8;
        }

        if (shootCooldown > 0) shootCooldown--;
    }

    void updateGame() {
        if (gameOver) return;

        for (auto &b : bullets) b.update();
        bullets.erase(
            remove_if(bullets.begin(), bullets.end(), [](auto &b) { return !b.active; }),
            bullets.end()
        );

        for (auto &e : enemies) {
            if (e.alive) {
                e.update();
                if (rand() % 200 < 2)
                    bullets.emplace_back(e.x, e.y + 1, false);
            }
        }

        checkCollisions();

        frameCount++;
        if (frameCount % 900 == 0)
            spawnBoss();

        if (all_of(enemies.begin(), enemies.end(), [](auto &e) { return !e.alive; })) {
            wave++;
            createEnemyWave();
        }

        if (lives <= 0)
            gameOver = true;
    }

    void checkCollisions() {
        for (auto &b : bullets) {
            if (!b.playerBullet || !b.active) continue;

            for (auto &e : enemies) {
                if (!e.alive) continue;
                if (abs(b.x - e.x) <= 1 && abs(b.y - e.y) <= 1) {
                    e.health--;
                    b.active = false;
                    if (e.health <= 0) {
                        e.alive = false;
                        score += e.isBoss ? 100 : 10;
                    }
                    break;
                }
            }
        }

        for (auto &b : bullets) {
            if (b.playerBullet || !b.active) continue;
            if (b.y == PLAYER_POS && abs(b.x - playerX) <= 1) {
                b.active = false;
                lives--;
            }
        }
    }

    void render() {
        renderer.clear();

        renderer.setChar(playerX, PLAYER_POS, 'A');
        renderer.setChar(playerX - 1, PLAYER_POS, '<');
        renderer.setChar(playerX + 1, PLAYER_POS, '>');

        for (auto &b : bullets)
            if (b.active)
                renderer.setChar(b.x, b.y, b.playerBullet ? '|' : '!');

        for (auto &e : enemies) {
            if (!e.alive) continue;

            if (e.isBoss) {
                renderer.setChar(e.x, e.y, 'B');
                renderer.setChar(e.x - 1, e.y, '[');
                renderer.setChar(e.x + 1, e.y, ']');
            } else {
                renderer.setChar(e.x, e.y, 'E');
                renderer.setChar(e.x - 1, e.y, '-');
                renderer.setChar(e.x + 1, e.y, '-');
            }
        }

        renderer.draw();

        cout << "Score: " << score << " | Lives: " << lives << " | Wave: " << wave << endl;

        if (gameOver) {
            cout << "\n\n";
            cout << string(26, ' ') << "======================\n";
            cout << string(26, ' ') << "      GAME  OVER      \n";
            cout << string(26, ' ') << "======================\n\n";
            cout << string(30, ' ') << "FINAL SCORE: " << score << "\n";
            cout << string(30, ' ') << "WAVE REACHED: " << wave << "\n\n";
            cout << string(30, ' ') << "Press R to Restart\n";
            cout << string(30, ' ') << "Press Q to Quit\n";
        }
    }

    void resetGame() {
        playerX = WIDTH / 2;
        score = 0;
        lives = 10;
        wave = 1;
        gameOver = false;
        bullets.clear();
        createEnemyWave();
        frameCount = 0;
        shootCooldown = 0;
    }

    bool isGameOver() const { return gameOver; }
};

int main() {
    system("title Galaga - Enhanced Game Over");
    system("cls");

    ResponsiveGame game;

    const double TARGET_FRAME_TIME = 1000.0 / 60.0;

    while (true) {
        auto frameStart = chrono::steady_clock::now();

        game.updateInput();
        game.updateGame();
        game.render();

        if (game.isGameOver()) {
            while (game.isGameOver()) {
                game.render();
                Sleep(100);
                game.updateInput();
            }
        }

        auto frameEnd = chrono::steady_clock::now();
        double frameTime =
            chrono::duration<double, milli>(frameEnd - frameStart).count();
        double sleepTime = TARGET_FRAME_TIME - frameTime;
        if (sleepTime > 0)
            Sleep((DWORD)sleepTime);
    }

    return 0;
}
