// Конфиг твоих 4 камер
const AVAILABLE_CAMERAS = [
    { id: 'cam0', name: 'Камера 0 (Основная)' },
    { id: 'cam2', name: 'Камера 2 (Боковая)' },
    { id: 'cam4', name: 'Камера 4 (Запасная)' },
    { id: 'cam6', name: 'Камера 6 (Ультимативная)' },
];

const MEDIAMTX_URL = `http://${window.location.hostname}:8889`;

const activeReaders = {};
const grid = document.getElementById('video-grid');
const togglesContainer = document.getElementById('camera-toggles');
const modal = document.getElementById('fullscreen-modal');
const modalContent = document.getElementById('fullscreen-container');

// ========================================================
// УМНЫЙ WHEP КЛИЕНТ (Без внешних зависимостей)
// ========================================================
class NativeWHEPClient {
    constructor(url, videoElement) {
        this.pc = new RTCPeerConnection();
        this.resourceURL = null; // Ссылка на удаление сессии
        this.videoElement = videoElement;

        this.pc.addTransceiver('video', { direction: 'recvonly' });

        this.pc.ontrack = (event) => {
            console.log(`[WHEP] 🟢 Поток получен для ${url}!`);
            this.videoElement.srcObject = event.streams[0];

            // ХАК №1: Пробуждаем плеер
            this.videoElement.onloadedmetadata = () => {
                this.videoElement.play().catch(e => console.warn("Автоплей заблокирован:", e));
            };
        };

        this.pc.createOffer()
            .then(offer => this.pc.setLocalDescription(offer))
            .then(() => fetch(url, {
                method: 'POST',
                headers: { 'Content-Type': 'application/sdp' },
                body: this.pc.localDescription.sdp
            }))
            .then(res => {
                if (!res.ok) throw new Error(`HTTP Ошибка: ${res.status}`);

                // Сохраняем ссылку для закрытия WebRTC сессии
                const location = res.headers.get('location');
                if (location) {
                    this.resourceURL = location.startsWith('http') ? location : new URL(location, url).toString();
                }
                return res.text();
            })
            .then(sdp => this.pc.setRemoteDescription({ type: 'answer', sdp: sdp }))
            .catch(err => {
                console.error(`[WHEP] ❌ Ошибка соединения (${url}):`, err);
                this.videoElement.style.border = "2px solid red";
            });
    }

    close() {
        this.pc.close();
        if (this.resourceURL) {
            fetch(this.resourceURL, { method: 'DELETE' }).catch(() => {});
        }
    }
}
// ========================================================

function init() {
    AVAILABLE_CAMERAS.forEach(cam => {
        const label = document.createElement('label');
        label.className = 'toggle-label';

        const checkbox = document.createElement('input');
        checkbox.type = 'checkbox';
        checkbox.value = cam.id;
        checkbox.checked = true;

        checkbox.addEventListener('change', (e) => handleToggle(cam, e.target.checked));

        label.appendChild(checkbox);
        label.appendChild(document.createTextNode(cam.name));
        togglesContainer.appendChild(label);

        startCamera(cam);
    });
}

function handleToggle(cam, isChecked) {
    if (isChecked) {
        startCamera(cam);
    } else {
        stopCamera(cam.id);
    }
}

function startCamera(cam) {
    if (activeReaders[cam.id]) return; // Защита от дублей

    console.log(`[${cam.id}] Запуск камеры...`);

    const wrapper = document.createElement('div');
    wrapper.className = 'cam-wrapper';
    wrapper.id = `wrapper-${cam.id}`;

    const video = document.createElement('video');
    // ХАК №2: Обход политик браузера
    video.setAttribute('autoplay', '');
    video.setAttribute('muted', '');
    video.setAttribute('playsinline', '');
    video.autoplay = true;
    video.muted = true;
    video.controls = false;

    // ХАК №3: Принудительный рендер
    video.addEventListener('loadeddata', () => {
        video.play().catch(() => {});
    });

    video.addEventListener('dblclick', () => openFullscreen(video, cam.name));

    const nameTag = document.createElement('div');
    nameTag.className = 'cam-name';
    nameTag.innerText = cam.name;

    wrapper.appendChild(video);
    wrapper.appendChild(nameTag);
    grid.appendChild(wrapper);

    const whepUrl = `${MEDIAMTX_URL}/${cam.id}/whep`;
    const reader = new NativeWHEPClient(whepUrl, video);

    activeReaders[cam.id] = { reader, wrapper };
}

function stopCamera(camId) {
    const session = activeReaders[camId];
    if (session) {
        console.log(`[${camId}] Отключение...`);
        if (session.reader) session.reader.close();
        if (session.wrapper) session.wrapper.remove();
        delete activeReaders[camId];
    }
}

// ---- Полноэкранный режим ----
function openFullscreen(sourceVideo, camName) {
    modalContent.innerHTML = '';
    const fsVideo = document.createElement('video');
    fsVideo.srcObject = sourceVideo.srcObject;
    fsVideo.autoplay = true;
    fsVideo.muted = true;
    fsVideo.controls = true;

    const nameTag = document.createElement('div');
    nameTag.className = 'cam-name';
    nameTag.innerText = `${camName} (Фокус)`;

    modalContent.appendChild(fsVideo);
    modalContent.appendChild(nameTag);
    modal.classList.remove('hidden');
}

document.getElementById('close-modal').addEventListener('click', () => {
    modal.classList.add('hidden');
    modalContent.innerHTML = '';
});

// ---- Управление скрытием боковой панели ----
const sidebar = document.getElementById('sidebar');
const toggleBtn = document.getElementById('toggle-sidebar');

toggleBtn.addEventListener('click', () => {
    sidebar.classList.toggle('collapsed');
});

// Старт
window.addEventListener('load', init);