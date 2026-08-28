#ifndef DRONE_DAEMON_WEBSERVER_H
#define DRONE_DAEMON_WEBSERVER_H

#include <thread>
#include <mutex>
#include <map>
#include <vector>
#include <sstream>
#include "cpp-httplib/httplib.h"
#include "dataTypes.h"

class FrontendServer
{
private:
    httplib::Server server_;
    std::thread serverThread_;
    int port = 8080;

    std::mutex data_mtx_;
    std::map<int, std::vector<DetectionResult>> latest_boxes_;

    std::string generate_json() {
        std::lock_guard<std::mutex> lock(data_mtx_);
        std::stringstream ss;
        ss << "{";
        bool first_cam = true;
        for (const auto& [cam_id, bboxes] : latest_boxes_) {
            if (!first_cam) ss << ",";
            ss << "\"" << cam_id << "\":[";
            bool first_box = true;
            for (const auto& box : bboxes) {
                if (!first_box) ss << ",";
                ss << "{\"x1\":" << box.lt.x << ",\"y1\":" << box.lt.y
                   << ",\"x2\":" << box.rb.x << ",\"y2\":" << box.rb.y
                   << ",\"label\":\"" << box.result_text << "\"}";
                first_box = false;
            }
            ss << "]";
            first_cam = false;
        }
        ss << "}";
        return ss.str();
    }

public:
    FrontendServer() = default;

    void update_boxes(const ResultTask& task) {
        std::lock_guard<std::mutex> lock(data_mtx_);
        latest_boxes_[task.cam_id] = task.bboxes;
    }

    void start()
    {
        auto ret = server_.set_mount_point("/", "./res");
        if (!ret) {
            std::cerr << "Ошибка: Папка ./res не найдена! (Убедись, что запускаешь бинарник из правильного места)" << std::endl;
        }

        server_.Get("/", [](const httplib::Request&, httplib::Response& res) {
            res.set_redirect("/index.html");
        });

        server_.Get("/api/boxes", [this](const httplib::Request&, httplib::Response& res) {
            res.set_content(generate_json(), "application/json");
        });

        serverThread_ = std::thread([this]()
        {
            std::cout <<  "Web-сервер запущен на http://0.0.0.0:" << port << std::endl;
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