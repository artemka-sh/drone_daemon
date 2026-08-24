//
// Created by arm64 on 24.08.2026.
//

#ifndef DRONE_DAEMON_WEBSERVER_H
#define DRONE_DAEMON_WEBSERVER_H

#include <print>
#include <thread>
#include "cpp-httplib/httplib.h"

class FrontendServer
{
private:
    httplib::Server server_;
    std::thread serverThread_;
    int port = 8080;

public:
    FrontendServer() = default;

    void start()
    {
        auto ret = server_.set_mount_point("/", "./res");
        if (!ret) {
            std::println(stderr, "Ошибка: Папка ./res не найдена! (Убедись, что запускаешь бинарник из правильного места)");
        }

        server_.Get("/", [](const httplib::Request&, httplib::Response& res) {
            res.set_redirect("/index.html");
        });

        serverThread_ = std::thread([this]()
        {
            std::println(stdout, "Web-сервер запущен на http://0.0.0.0:{}", port);
            server_.listen("0.0.0.0", port);
        });
    }

    ~FrontendServer()
    {
        server_.stop();

        if (serverThread_.joinable()) {
            serverThread_.join();
        }
    }
};

#endif //DRONE_DAEMON_WEBSERVER_H