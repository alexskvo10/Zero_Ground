#include <SFML/Network.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <thread>
#include <mutex>
#include <sstream>
#include <memory>

enum class ClientState { ConnectScreen, MainScreen, ErrorScreen };

struct Position {
    float x = 0.0f;
    float y = 0.0f;
};

std::mutex mutex;
Position clientPos = { 200.0f, 200.0f };
Position serverPos = { 400.0f, 300.0f };
bool serverConnected = false;
std::string serverIP = "127.0.0.1";

void udpThread(std::unique_ptr<sf::UdpSocket> socket, const std::string& ip) {
    if (socket->bind(53002) != sf::Socket::Done) {
        std::cerr << "Failed to bind UDP socket" << std::endl;
        return;
    }

    char buffer[sizeof(Position)];

    while (true) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            memcpy(buffer, &clientPos, sizeof(Position));
        }
        socket->send(buffer, sizeof(Position), sf::IpAddress(ip), 53001);

        std::size_t received;
        sf::IpAddress sender;
        unsigned short port;

        if (socket->receive(buffer, sizeof(buffer), received, sender, port) == sf::Socket::Done) {
            if (received == sizeof(Position) && sender == sf::IpAddress(ip)) {
                std::lock_guard<std::mutex> lock(mutex);
                memcpy(&serverPos, buffer, sizeof(Position));
                serverConnected = true;
            }
        }

        sf::sleep(sf::milliseconds(50));
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

    window.create(mode, isFullscreen ? "Client" : "Client (Windowed)", style);
    window.setFramerateLimit(60);
}

int main() {
    sf::VideoMode desktopMode = sf::VideoMode::getDesktopMode();
    sf::RenderWindow window(desktopMode, "Client", sf::Style::Fullscreen);
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.loadFromFile("arial.ttf")) {
        std::cerr << "Failed to load font!" << std::endl;
        return -1;
    }

    bool isFullscreen = true;

    ClientState state = ClientState::ConnectScreen;
    std::string inputIP = "127.0.0.1";
    bool inputActive = false;
    bool udpThreadStarted = false;
    std::unique_ptr<sf::UdpSocket> udpSocket = std::make_unique<sf::UdpSocket>();
    std::thread udpWorker;
    sf::Clock errorTimer;

    // Элементы экрана подключения
    sf::Text ipLabel;
    ipLabel.setFont(font);
    ipLabel.setString("SERVER IP ADDRESS:");
    ipLabel.setCharacterSize(32);
    ipLabel.setFillColor(sf::Color::White);

    sf::RectangleShape ipField(sf::Vector2f(400, 50));
    ipField.setFillColor(sf::Color(50, 50, 50));
    ipField.setOutlineColor(sf::Color(100, 100, 100));
    ipField.setOutlineThickness(2.0f);

    sf::Text ipText;
    ipText.setFont(font);
    ipText.setString(inputIP);
    ipText.setCharacterSize(28);
    ipText.setFillColor(sf::Color::White);

    sf::RectangleShape connectButton(sf::Vector2f(300, 70));
    connectButton.setFillColor(sf::Color(0, 150, 0));

    sf::Text connectText;
    connectText.setFont(font);
    connectText.setString("CONNECT TO SERVER");
    connectText.setCharacterSize(32);
    connectText.setFillColor(sf::Color::White);

    sf::Text errorText;
    errorText.setFont(font);
    errorText.setString("SERVER UNAVAILABLE OR INVALID IP");
    errorText.setCharacterSize(28);
    errorText.setFillColor(sf::Color::Red);


    // Элементы основного экрана
    sf::CircleShape clientCircle(30.0f);
    clientCircle.setFillColor(sf::Color::Blue);
    clientCircle.setOutlineColor(sf::Color(0, 0, 100));
    clientCircle.setOutlineThickness(3.0f);
    clientCircle.setPosition(clientPos.x - 30.0f, clientPos.y - 30.0f);

    sf::CircleShape serverCircle(20.0f);
    serverCircle.setFillColor(sf::Color(0, 200, 0, 200));
    serverCircle.setOutlineColor(sf::Color(0, 100, 0));
    serverCircle.setOutlineThickness(2.0f);

    // Функция для центрирования элементов
    auto centerElements = [&]() {
        sf::Vector2u windowSize = window.getSize();

        ipLabel.setPosition(windowSize.x / 2 - ipLabel.getLocalBounds().width / 2,
            windowSize.y / 2 - 150);

        ipField.setPosition(windowSize.x / 2 - 200, windowSize.y / 2 - 50);
        ipText.setPosition(windowSize.x / 2 - 190, windowSize.y / 2 - 45);

        connectButton.setPosition(windowSize.x / 2 - 150, windowSize.y / 2 + 50);
        connectText.setPosition(windowSize.x / 2 - connectText.getLocalBounds().width / 2,
            windowSize.y / 2 + 50 + 10);

        errorText.setPosition(windowSize.x / 2 - errorText.getLocalBounds().width / 2,
            windowSize.y - 100);
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

            if (state == ClientState::ConnectScreen) {
                // Обработка клика на поле ввода
                if (event.type == sf::Event::MouseButtonPressed) {
                    sf::Vector2i mousePos = sf::Mouse::getPosition(window);
                    inputActive = ipField.getGlobalBounds().contains(
                        static_cast<float>(mousePos.x),
                        static_cast<float>(mousePos.y)
                    );
                }

                // Обработка ввода с клавиатуры
                if (inputActive) {
                    if (event.type == sf::Event::KeyPressed) {
                        if (event.key.code == sf::Keyboard::BackSpace && !inputIP.empty()) {
                            inputIP.pop_back();
                            ipText.setString(inputIP);
                        }
                    }
                    else if (event.type == sf::Event::TextEntered) {
                        if (event.text.unicode < 128) {
                            char c = static_cast<char>(event.text.unicode);
                            if (c == '\b') { // Backspace в текстовом событии
                                if (!inputIP.empty()) {
                                    inputIP.pop_back();
                                    ipText.setString(inputIP);
                                }
                            }
                            else if (c == '.' || (c >= '0' && c <= '9')) {
                                if (inputIP.size() < 15) {
                                    inputIP += c;
                                    ipText.setString(inputIP);
                                }
                            }
                            else if (c == '\r' || c == '\n') { // Enter
                                // Автоматическая попытка подключения при нажатии Enter
                                sf::TcpSocket testSocket;
                                testSocket.setBlocking(true);
                                sf::Socket::Status status = testSocket.connect(inputIP, 53000, sf::seconds(3.0f));

                                if (status == sf::Socket::Done) {
                                    serverIP = inputIP;


                                    if (!udpThreadStarted) {
                                        udpWorker = std::thread(udpThread, std::move(udpSocket), serverIP);
                                        udpThreadStarted = true;
                                    }

                                    state = ClientState::MainScreen;
                                }
                                else {
                                    state = ClientState::ErrorScreen;
                                    errorTimer.restart();
                                }
                            }
                        }
                    }
                }

                // Обработка нажатия кнопки подключения
                if (isButtonClicked(connectButton, event, window)) {
                    sf::TcpSocket testSocket;
                    testSocket.setBlocking(true);
                    sf::Socket::Status status = testSocket.connect(inputIP, 53000, sf::seconds(3.0f));

                    if (status == sf::Socket::Done) {
                        serverIP = inputIP;

                        if (!udpThreadStarted) {
                            udpWorker = std::thread(udpThread, std::move(udpSocket), serverIP);
                            udpThreadStarted = true;
                        }

                        state = ClientState::MainScreen;
                    }
                    else {
                        state = ClientState::ErrorScreen;
                        errorTimer.restart();
                    }
                }
            }
        }

        window.clear(sf::Color::Black);

        if (state == ClientState::ConnectScreen) {
            window.draw(ipLabel);
            window.draw(ipField);
            window.draw(ipText);
            window.draw(connectButton);
            window.draw(connectText);

            // Визуальная индикация активного поля ввода
            if (inputActive) {
                ipField.setOutlineColor(sf::Color::Green);
                ipField.setOutlineThickness(3.0f);
            }
            else {
                ipField.setOutlineColor(sf::Color(100, 100, 100));
                ipField.setOutlineThickness(2.0f);
            }
        }
        else if (state == ClientState::ErrorScreen) {
            window.draw(ipLabel);
            window.draw(ipField);
            window.draw(ipText);
            window.draw(connectButton);
            window.draw(connectText);
            window.draw(errorText);

            if (errorTimer.getElapsedTime().asSeconds() > 3.0f) {
                state = ClientState::ConnectScreen;
            }
        }
        else if (state == ClientState::MainScreen) {
            // Управление клиентским кругом
            if (window.hasFocus()) {
                float speed = 300.0f * 0.016f;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) clientPos.y -= speed;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) clientPos.y += speed;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) clientPos.x -= speed;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) clientPos.x += speed;

                sf::Vector2u windowSize = window.getSize();
                clientPos.x = std::max(30.0f, std::min(static_cast<float>(windowSize.x) - 30.0f, clientPos.x));
                clientPos.y = std::max(30.0f, std::min(static_cast<float>(windowSize.y) - 30.0f, clientPos.y));
                clientCircle.setPosition(clientPos.x - 30.0f, clientPos.y - 30.0f);
            }

            // Обновление позиции серверного круга
            {
                std::lock_guard<std::mutex> lock(mutex);
                sf::Vector2u windowSize = window.getSize();
                serverCircle.setPosition(
                    std::max(20.0f, std::min(static_cast<float>(windowSize.x) - 20.0f, serverPos.x - 20.0f)),
                    std::max(20.0f, std::min(static_cast<float>(windowSize.y) - 20.0f, serverPos.y - 20.0f))
                );
            }


            // Отрисовка
            if (serverConnected) {
                window.draw(serverCircle);
            }
            window.draw(clientCircle);
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
