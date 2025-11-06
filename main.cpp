#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>

// MySQL Connector/C++
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>

using namespace std;
using namespace sf;

// ---------- Config ----------
const int ROWS = 6;
const int COLS = 7;
const float CELL_SIZE = 90.f;
const int WINDOW_W = (int)(COLS * CELL_SIZE);
const int WINDOW_H = (int)(ROWS * CELL_SIZE + 240);

// ---------- Database helper ----------
struct Database {
    sql::mysql::MySQL_Driver* driver = nullptr;
    unique_ptr<sql::Connection> con;
    bool ok = false;

    Database(const string &host, const string &user, const string &pass, const string &schema = "connect4_db") {
        try {
            driver = sql::mysql::get_mysql_driver_instance();
            con.reset(driver->connect(host, user, pass));
            unique_ptr<sql::Statement> st(con->createStatement());
            st->execute("CREATE DATABASE IF NOT EXISTS " + schema);
            con->setSchema(schema);

            st->execute("CREATE TABLE IF NOT EXISTS players ("
                        "id INT AUTO_INCREMENT PRIMARY KEY, "
                        "name VARCHAR(100) UNIQUE, "
                        "wins INT DEFAULT 0, losses INT DEFAULT 0)");

            st->execute("CREATE TABLE IF NOT EXISTS game_results ("
                        "id INT AUTO_INCREMENT PRIMARY KEY, "
                        "player1 VARCHAR(100), "
                        "player2 VARCHAR(100), "
                        "winner VARCHAR(100), "
                        "played_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)");

            ok = true;
        } catch (const sql::SQLException &e) {
            cerr << "DB init failed: " << e.what() << endl;
            ok = false;
        }
    }

    void ensurePlayer(const string &name) {
        if (!ok) return;
        try {
            auto ps = unique_ptr<sql::PreparedStatement>(con->prepareStatement(
                "INSERT IGNORE INTO players (name, wins, losses) VALUES (?, 0, 0)"
            ));
            ps->setString(1, name);
            ps->execute();
        } catch (...) {}
    }

    void addWin(const string &name) {
        if (!ok) return;
        try {
            auto ps = unique_ptr<sql::PreparedStatement>(con->prepareStatement(
                "UPDATE players SET wins = wins + 1 WHERE name = ?"
            ));
            ps->setString(1, name);
            ps->execute();
        } catch (...) {}
    }

    void addLoss(const string &name) {
        if (!ok) return;
        try {
            auto ps = unique_ptr<sql::PreparedStatement>(con->prepareStatement(
                "UPDATE players SET losses = losses + 1 WHERE name = ?"
            ));
            ps->setString(1, name);
            ps->execute();
        } catch (...) {}
    }

    void recordMatch(const string &p1, const string &p2, const string &winner) {
        if (!ok) return;
        try {
            auto ps = unique_ptr<sql::PreparedStatement>(con->prepareStatement(
                "INSERT INTO game_results (player1, player2, winner) VALUES (?, ?, ?)"
            ));
            ps->setString(1, p1);
            ps->setString(2, p2);
            ps->setString(3, winner);
            ps->execute();
        } catch (...) {}
    }
};

// ---------- Game logic ----------
bool checkWin(const vector<vector<int>>& b, int p) {
    for (int r=0; r<ROWS; ++r)
        for (int c=0; c<COLS-3; ++c)
            if (b[r][c]==p && b[r][c+1]==p && b[r][c+2]==p && b[r][c+3]==p) return true;
    for (int c=0; c<COLS; ++c)
        for (int r=0; r<ROWS-3; ++r)
            if (b[r][c]==p && b[r+1][c]==p && b[r+2][c]==p && b[r+3][c]==p) return true;
    for (int r=0; r<ROWS-3; ++r)
        for (int c=0; c<COLS-3; ++c)
            if (b[r][c]==p && b[r+1][c+1]==p && b[r+2][c+2]==p && b[r+3][c+3]==p) return true;
    for (int r=3; r<ROWS; ++r)
        for (int c=0; c<COLS-3; ++c)
            if (b[r][c]==p && b[r-1][c+1]==p && b[r-2][c+2]==p && b[r-3][c+3]==p) return true;
    return false;
}

int dropDisc(vector<vector<int>>& b, int col, int player) {
    if (col < 0 || col >= COLS) return -1;
    for (int r = ROWS-1; r>=0; --r)
        if (b[r][col] == 0) { b[r][col] = player; return r; }
    return -1;
}
bool boardFull(const vector<vector<int>>& b) {
    for (int c=0;c<COLS;++c) if (b[0][c]==0) return false;
    return true;
}

// ---------- Popup function ----------
pair<string,string> getNames(Font &font) {
    RenderWindow popup(VideoMode(600, 400), "Connect 4 Game", Style::Titlebar | Style::Close);
    popup.setFramerateLimit(60);

    RectangleShape bg(Vector2f(600.f, 400.f));
    bg.setFillColor(Color(190, 220, 255));

    Text title("Connect 4 Game", font, 36);
    title.setFillColor(Color::Black);
    FloatRect tb = title.getLocalBounds();
    title.setOrigin(tb.width / 2, tb.height / 2);
    title.setPosition(300, 60);

    Text lbl1("Player 1", font, 22); lbl1.setFillColor(Color::Black); lbl1.setPosition(130, 150);
    Text lbl2("Player 2", font, 22); lbl2.setFillColor(Color::Black); lbl2.setPosition(130, 210);

    RectangleShape box1(Vector2f(280, 36)); box1.setPosition(250, 145);
    box1.setFillColor(Color::White); box1.setOutlineColor(Color::Black); box1.setOutlineThickness(1.5f);

    RectangleShape box2(Vector2f(280, 36)); box2.setPosition(250, 205);
    box2.setFillColor(Color::White); box2.setOutlineColor(Color::Black); box2.setOutlineThickness(1.5f);

    RectangleShape startBtn(Vector2f(150, 45));
    startBtn.setFillColor(Color(30, 144, 255));
    startBtn.setPosition(225, 290);

    Text btnText("Start Game", font, 22);
    btnText.setFillColor(Color::White);
    btnText.setPosition(245, 298);

    string name1, name2;
    Text input1("", font, 20); input1.setFillColor(Color::Black); input1.setPosition(260, 150);
    Text input2("", font, 20); input2.setFillColor(Color::Black); input2.setPosition(260, 210);
    Text ph1("Enter your name", font, 20); ph1.setFillColor(Color(150,150,150)); ph1.setPosition(260,150);
    Text ph2("Enter your name", font, 20); ph2.setFillColor(Color(150,150,150)); ph2.setPosition(260,210);

    bool active1=false, active2=false;
    while (popup.isOpen()) {
        Event e;
        while (popup.pollEvent(e)) {
            if (e.type == Event::Closed) popup.close();

            if (e.type == Event::MouseButtonPressed && e.mouseButton.button == Mouse::Left) {
                Vector2f m = popup.mapPixelToCoords(Mouse::getPosition(popup));
                active1 = box1.getGlobalBounds().contains(m);
                active2 = box2.getGlobalBounds().contains(m);
                if (startBtn.getGlobalBounds().contains(m)) {
                    if (!name1.empty() && !name2.empty()) popup.close();
                }
            }
            if (e.type == Event::TextEntered) {
                if (e.text.unicode == 8) {
                    if (active1 && !name1.empty()) name1.pop_back();
                    if (active2 && !name2.empty()) name2.pop_back();
                } else if (e.text.unicode >= 32 && e.text.unicode < 128) {
                    if (active1 && name1.size()<15) name1 += static_cast<char>(e.text.unicode);
                    if (active2 && name2.size()<15) name2 += static_cast<char>(e.text.unicode);
                }
                input1.setString(name1);
                input2.setString(name2);
            }
        }
        popup.clear(Color::White);
        popup.draw(bg);
        popup.draw(title);
        popup.draw(lbl1); popup.draw(lbl2);
        popup.draw(box1); popup.draw(box2);
        if (name1.empty() && !active1) popup.draw(ph1); else popup.draw(input1);
        if (name2.empty() && !active2) popup.draw(ph2); else popup.draw(input2);
        popup.draw(startBtn); popup.draw(btnText);
        popup.display();
    }
    return {name1, name2};
}

// ---------- UI ----------
int main() {
    const string mysql_host = "tcp://127.0.0.1:3306";
    const string mysql_user = "root";
    const string mysql_pass = "Harshi@123";
    const string mysql_schema = "connect4_db";

    unique_ptr<Database> db;
    try { db = make_unique<Database>(mysql_host, mysql_user, mysql_pass, mysql_schema); }
    catch (...) { db.reset(); }
    bool dbOk = db && db->ok;

    Font font;
    font.loadFromFile("C:/Windows/Fonts/arial.ttf");

RestartGame:; // label for restarting the entire flow

    auto [name1, name2] = getNames(font);
    if (dbOk && db) { db->ensurePlayer(name1); db->ensurePlayer(name2); }

    RenderWindow window(VideoMode(WINDOW_W, WINDOW_H), "Connect 4 Game", Style::Titlebar | Style::Close);
    window.setFramerateLimit(60);

    vector<vector<int>> board(ROWS, vector<int>(COLS,0));
    int current = 1;
    bool gameOver = false;
    string winnerStr;

    Text status("", font, 24); status.setFillColor(Color::Black); status.setPosition(16, 16);

    // Restart and Quit buttons
    RectangleShape restartBtn(Vector2f(180.f, 56.f));
    RectangleShape quitBtn(Vector2f(180.f, 56.f));
    float totalW = restartBtn.getSize().x + quitBtn.getSize().x + 40.f;
    float startX = (WINDOW_W - totalW)/2.f;
    restartBtn.setPosition(startX, WINDOW_H - 120.f);
    quitBtn.setPosition(startX + 220.f, WINDOW_H - 120.f);
    restartBtn.setFillColor(Color(100,220,120));
    restartBtn.setOutlineColor(Color::Black);
    restartBtn.setOutlineThickness(2.f);
    quitBtn.setFillColor(Color(220,100,100));
    quitBtn.setOutlineColor(Color::Black);
    quitBtn.setOutlineThickness(2.f);

    Text restartLabel("Restart", font, 20);
    restartLabel.setFillColor(Color::Black);
    restartLabel.setPosition(restartBtn.getPosition().x + 50.f, restartBtn.getPosition().y + 14.f);

    Text quitLabel("Quit Game", font, 20);
    quitLabel.setFillColor(Color::Black);
    quitLabel.setPosition(quitBtn.getPosition().x + 38.f, quitBtn.getPosition().y + 14.f);

    while (window.isOpen()) {
        Event e;
        while (window.pollEvent(e)) {
            if (e.type == Event::Closed) window.close();

            if (!gameOver && e.type == Event::MouseButtonPressed && e.mouseButton.button == Mouse::Left) {
                int col = e.mouseButton.x / CELL_SIZE;
                int row = dropDisc(board, col, current);
                if (row != -1) {
                    if (checkWin(board, current)) {
                        gameOver = true;
                        string winName = (current==1?name1:name2);
                        winnerStr = winName + " Wins!";
                        if (dbOk && db) {
                            db->addWin(winName);
                            db->addLoss((current==1?name2:name1));
                            db->recordMatch(name1, name2, winName);
                        }
                    } else if (boardFull(board)) {
                        gameOver = true;
                        winnerStr = "It's a Draw!";
                        if (dbOk && db) db->recordMatch(name1, name2, "Draw");
                    } else current = 3-current;
                }
            }

            if (gameOver && e.type == Event::MouseButtonPressed && e.mouseButton.button == Mouse::Left) {
                Vector2f m = window.mapPixelToCoords(Mouse::getPosition(window));
                if (restartBtn.getGlobalBounds().contains(m)) {
                    window.close();
                    goto RestartGame;
                } else if (quitBtn.getGlobalBounds().contains(m)) {
                    window.close();
                }
            }
        }

        window.clear(Color(220,230,255));
        RectangleShape cell(Vector2f(CELL_SIZE - 6, CELL_SIZE - 6));
        CircleShape disc(CELL_SIZE/2 - 12);
        for (int r=0;r<ROWS;++r)
            for (int c=0;c<COLS;++c) {
                float x = c*CELL_SIZE+12, y=r*CELL_SIZE+80;
                cell.setPosition(x-6,y-6); cell.setFillColor(Color(30,144,255)); window.draw(cell);
                disc.setPosition(x,y);
                if (board[r][c]==1) disc.setFillColor(Color::Red);
                else if (board[r][c]==2) disc.setFillColor(Color::Yellow);
                else disc.setFillColor(Color(230,230,230));
                window.draw(disc);
            }

        if (!gameOver) status.setString((current==1?name1:name2) + "'s Turn");
        else status.setString(winnerStr);
        window.draw(status);

        if (gameOver) {
            window.draw(restartBtn);
            window.draw(quitBtn);
            window.draw(restartLabel);
            window.draw(quitLabel);
        }

        window.display();
    }

    return 0;
}
