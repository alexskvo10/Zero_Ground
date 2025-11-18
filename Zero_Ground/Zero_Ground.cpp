#include <SFML/Network.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <thread>
#include <mutex>
#include <map>
#include <memory>
#include <string>

enum class ServerState { StartScreen, MainScreen };

struct Position {
    float x = 0.0f;
    float y = 0.0f;
};

std::mutex mutex;
Position serverPos = { 400.0f, 300.0f };
std::map<sf::IpAddress, Position> clients;

void udpThread(std::unique_ptr<sf::UdpSocket> socket) {
    if (socket->bind(53001) != sf::Socket::Done) {
        std::cerr << "Failed to bind UDP socket" << std::endl;
        return;
    }

    char buffer[sizeof(Position)];
    std::size_t received;
    sf::IpAddress sender;
    unsigned short port;

    while (true) {
        if (socket->receive(buffer, sizeof(buffer), received, sender, port) == sf::Socket::Done) {
            if (received == sizeof(Position)) {
                Position pos;
                memcpy(&pos, buffer, sizeof(Position));

                {
                    std::lock_guard<std::mutex> lock(mutex);
                    clients[sender] = pos;
                }

                memcpy(buffer, &serverPos, sizeof(Position));
                socket->send(buffer, sizeof(Position), sender, port);
            }
        }
        sf::sleep(sf::milliseconds(10));
    }
}

bool isButtonClicked(const sf::RectangleShape& button, const sf::Event& event, const sf::RenderWindow& window) {
    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2i mousePos = sf::Mouse::getPosition(window);
        if (button.getGlobalBounds().contains(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y))) {
            return true;
        }
    }
    return false;
}

void toggleFullscreen(sf::RenderWindow& window, bool& isFullscreen, const sf::VideoMode& desktopMode) {
    isFullscreen = !isFullscreen;
    sf::Uint32 style = isFullscreen ? sf::Style::Fullscreen : sf::Style::Resize | sf::Style::Close;
    sf::VideoMode mode = isFullscreen ? desktopMode : sf::VideoMode(800, 600);

    window.create(mode, isFullscreen ? "Server" : "Server (Windowed)", style);
    window.setFramerateLimit(60);
}

int main() {
    sf::TcpListener tcpListener;
    if (tcpListener.listen(53000) != sf::Socket::Done) {
        std::cerr << "Failed to start TCP server" << std::endl;
        return -1;
    }
    tcpListener.setBlocking(false);

    sf::VideoMode desktopMode = sf::VideoMode::getDesktopMode();
    sf::RenderWindow window(desktopMode, "Server", sf::Style::Fullscreen);
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.loadFromFile("arial.ttf")) {
        std::cerr << "Failed to load font!" << std::endl;
        return -1;
    }

    bool isFullscreen = true;
    ServerState state = ServerState::StartScreen;
    std::unique_ptr<sf::UdpSocket> udpSocket = std::make_unique<sf::UdpSocket>();
    std::thread udpWorker;
    bool udpThreadStarted = false;

    // Получаем локальный IP-адрес
    sf::IpAddress localIP = sf::IpAddress::getLocalAddress();
    std::string ipString = localIP.toString();
    if (ipString == "0.0.0.0" || ipString == "127.0.0.1") {
        ipString = "IP not available";
    }

    // Элементы стартового экрана
    sf::Text startText;
    startText.setFont(font);
    startText.setString("SERVER STARTED");
    startText.setCharacterSize(64);
    startText.setFillColor(sf::Color::Green);

    sf::Text ipText;
    ipText.setFont(font);
    ipText.setString("Server IP: " + ipString);
    ipText.setCharacterSize(32);
    ipText.setFillColor(sf::Color::White);

    sf::RectangleShape nextButton(sf::Vector2f(200, 60));
    nextButton.setFillColor(sf::Color(0, 150, 0));

    sf::Text buttonText;
    buttonText.setFont(font);
    buttonText.setString("NEXT");
    buttonText.setCharacterSize(32);
    buttonText.setFillColor(sf::Color::White);

    // Элементы основного экрана
    sf::CircleShape serverCircle(30.0f);
    serverCircle.setFillColor(sf::Color::Green);
    serverCircle.setOutlineColor(sf::Color(0, 100, 0));
    serverCircle.setOutlineThickness(3.0f);
    serverCircle.setPosition(serverPos.x - 30.0f, serverPos.y - 30.0f);

    std::map<sf::IpAddress, sf::CircleShape> clientCircles;

    // Функция для центрирования элементов
    auto centerElements = [&]() {
        sf::Vector2u windowSize = window.getSize();

        startText.setPosition(windowSize.x / 2 - startText.getLocalBounds().width / 2,
            windowSize.y / 2 - 150);

        ipText.setPosition(windowSize.x / 2 - ipText.getLocalBounds().width / 2,
            windowSize.y / 2 - 50);

        nextButton.setPosition(windowSize.x / 2 - 100, windowSize.y / 2 + 50);
        buttonText.setPosition(windowSize.x / 2 - buttonText.getLocalBounds().width / 2,
            windowSize.y / 2 + 50 + 10);
        };

    centerElements();

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();

            // Обработка Esc для переключения режима
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                toggleFullscreen(window, isFullscreen, desktopMode);
                centerElements();
            }

            if (state == ServerState::StartScreen) {
                if (isButtonClicked(nextButton, event, window)) {
                    state = ServerState::MainScreen;

                    if (!udpThreadStarted) {
                        udpWorker = std::thread(udpThread, std::move(udpSocket));
                        udpThreadStarted = true;
                    }
                }
            }
        }

        window.clear(sf::Color::Black);

        if (state == ServerState::StartScreen) {
            window.draw(startText);
            window.draw(ipText);
            window.draw(nextButton);
            window.draw(buttonText);
        }
        else if (state == ServerState::MainScreen) {
            // Управление серверным кругом
            if (window.hasFocus()) {
                float speed = 300.0f * 0.016f;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) serverPos.y -= speed;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) serverPos.y += speed;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) serverPos.x -= speed;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) serverPos.x += speed;

                sf::Vector2u windowSize = window.getSize();
                serverPos.x = std::max(30.0f, std::min(static_cast<float>(windowSize.x) - 30.0f, serverPos.x));
                serverPos.y = std::max(30.0f, std::min(static_cast<float>(windowSize.y) - 30.0f, serverPos.y));
                serverCircle.setPosition(serverPos.x - 30.0f, serverPos.y - 30.0f);
            }

            // Принятие подключений
            sf::TcpSocket* client = new sf::TcpSocket;
            if (tcpListener.accept(*client) == sf::Socket::Done) {
                delete client;
            }
            else {
                delete client;
            }

            // Обновление клиентских кругов
            {
                std::lock_guard<std::mutex> lock(mutex);
                for (auto& pair : clients) {
                    sf::IpAddress ip = pair.first;
                    Position pos = pair.second;

                    if (clientCircles.find(ip) == clientCircles.end()) {
                        clientCircles[ip] = sf::CircleShape(20.0f);
                        clientCircles[ip].setFillColor(sf::Color::Blue);
                        clientCircles[ip].setOutlineColor(sf::Color(0, 0, 100));
                        clientCircles[ip].setOutlineThickness(2.0f);
                    }

                    sf::Vector2u windowSize = window.getSize();
                    clientCircles[ip].setPosition(
                        std::max(20.0f, std::min(static_cast<float>(windowSize.x) - 20.0f, pos.x - 20.0f)),
                        std::max(20.0f, std::min(static_cast<float>(windowSize.y) - 20.0f, pos.y - 20.0f))
                    );
                }
            }

            // Отрисовка
            for (auto& pair : clientCircles) {
                window.draw(pair.second);
            }
            window.draw(serverCircle);
        }

        window.display();
        sf::sleep(sf::milliseconds(16));
    }

    // Остановка потока UDP
    if (udpThreadStarted) {
        udpWorker.join();
    }

    return 0;
}