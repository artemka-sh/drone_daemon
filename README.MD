# Drone Vision Daemon

[Russian version below](#russian-version)

A high-performance C++ daemon for real-time multi-camera video streaming and AI object detection, optimized for the Orange Pi AIpro 20T (Ascend 310B4 NPU).

This software captures video from multiple cameras via GStreamer, streams it over RTP/UDP to a MediaMTX server for WebRTC broadcasting, and processes frames concurrently using a hardware-accelerated YOLO12 model. A built-in HTTP server provides a web dashboard for visualization.

Parts of the AscendCL integration code are based on the [Huawei Ascend Samples](https://github.com/Huawei-Ascend/samples) repository (Apache 2.0 License).

## System Requirements
* **Hardware:** Orange Pi AIpro 20T
* **OS:** openEuler 22.03 (opiaipro_20t_openEuler22.03_desktop_aarch64_20250909)
* **AI Framework:** CANN Toolkit (Ascend Toolkit)
* **Model:** Custom YOLO12 (7 classes), exported to ONNX with static shapes (imgsz=640 opset=12 dynamic=False).

## Dependencies
Install the required system packages using dnf or yum:

    sudo dnf install pkgconfig gstreamer1-devel gstreamer1-plugins-base-devel opencv-devel

*Note: Ensure MediaMTX (v1.20.1+) is downloaded and placed in your working environment.*

## Installation & Setup

1. **Model Conversion:**
   Place `yolo12n.onnx` in the project root and compile it for the Ascend NPU:

        bash convert_yolo12.sh
        mkdir -p model
        mv yolo12n.om model/

2. **MediaMTX Configuration:**
   Append the following to `mediamtx.yml` to route GStreamer UDP traffic:

        paths:
          cam0:
            source: udp+rtp://127.0.0.1:5000
            rtpSDP: |
              v=0
              o=- 0 0 IN IP4 127.0.0.1
              s=cam0
              c=IN IP4 127.0.0.1
              t=0 0
              m=video 5000 RTP/AVP 96
              a=rtpmap:96 H264/90000

   *Replicate for cam2 (port 5002), cam4 (port 5004), etc.*
   Start the server: `./mediamtx mediamtx.yml`

3. **Build the Daemon:**

        mkdir build && cd build
        cmake ..
        cmake --build . -j 3

4. **Execution:**

        ./drone_daemon

   Access the dashboard at `http://<BOARD_IP>:8080`

---

<a name="russian-version"></a>
# Drone Vision Daemon

Высокопроизводительный C++ демон для многокамерного стриминга и ИИ-обнаружения объектов в реальном времени. Оптимизирован для платы Orange Pi AIpro 20T (NPU Ascend 310B4).

Программное обеспечение осуществляет захват видео с нескольких камер через GStreamer, передает потоки по RTP/UDP на сервер MediaMTX для трансляции по протоколу WebRTC и параллельно обрабатывает кадры с помощью аппаратно-ускоренной модели YOLO12. Встроенный HTTP-сервер предоставляет веб-дашборд для визуализации результатов.

Часть кода для работы с AscendCL основана на репозитории [Huawei Ascend Samples](https://github.com/Huawei-Ascend/samples) (Лицензия Apache 2.0).

## Системные требования
* **Аппаратная часть:** Orange Pi AIpro 20T
* **ОС:** openEuler 22.03 (opiaipro_20t_openEuler22.03_desktop_aarch64_20250909)
* **ИИ-фреймворк:** CANN Toolkit (Ascend Toolkit)
* **Модель:** Кастомная YOLO12 (7 классов), экспортированная в ONNX со статичными размерностями (imgsz=640 opset=12 dynamic=False).

## Зависимости
Установка системных пакетов выполняется через dnf или yum:

    sudo dnf install pkgconfig gstreamer1-devel gstreamer1-plugins-base-devel opencv-devel

*Примечание: Сервер MediaMTX (v1.20.1+) скачивается и запускается отдельно.*

## Установка и запуск

1. **Компиляция модели:**
   Поместите файл `yolo12n.onnx` в корень проекта и скомпилируйте его для NPU:

        bash convert_yolo12.sh
        mkdir -p model
        mv yolo12n.om model/

2. **Настройка MediaMTX:**
   Добавьте параметры маршрутизации UDP-трафика от GStreamer в конец файла `mediamtx.yml`:

        paths:
          cam0:
            source: udp+rtp://127.0.0.1:5000
            rtpSDP: |
              v=0
              o=- 0 0 IN IP4 127.0.0.1
              s=cam0
              c=IN IP4 127.0.0.1
              t=0 0
              m=video 5000 RTP/AVP 96
              a=rtpmap:96 H264/90000

   *(Продублируйте для остальных камер, меняя порты)*. Запустите сервер: `./mediamtx mediamtx.yml`

3. **Сборка демона:**

        mkdir build && cd build
        cmake ..
        cmake --build . -j 3

4. **Запуск:**

        ./drone_daemon

   Веб-интерфейс доступен по адресу `http://<IP_ПЛАТЫ>:8080`

![pic](https://raw.githubusercontent.com/artemka-sh/drone_daemon/refs/heads/master/result.jpg)
