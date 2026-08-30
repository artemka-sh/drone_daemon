const AVAILABLE_CAMERAS = [
    { id: 'cam0', name: 'Камера 0' },
    { id: 'cam2', name: 'Камера 2' },
    { id: 'cam4', name: 'Камера 4' },
    { id: 'cam6', name: 'Камера 6' },
];

const MEDIAMTX_URL = `http://${window.location.hostname}:8889`;

const activeReaders = {};
const grid = document.getElementById('video-grid');
const togglesContainer = document.getElementById('camera-toggles');
const modal = document.getElementById('fullscreen-modal');
const modalContent = document.getElementById('fullscreen-container');

class NativeWHEPClient {
    constructor(url, videoElement) {
        this.url = url;
        this.videoElement = videoElement;
        this.pc = null;
        this.lastFrames = -1;
        this.isReconnecting = false;
        this.watchdogTimer = setInterval(() => this.checkStream(), 3000);
        this.connect();
    }

    connect() {
        this.isReconnecting = true;
        this.videoElement.style.border = "2px solid red";
        this.lastFrames = -1;

        if (this.pc) {
            this.pc.close();
        }
        this.videoElement.srcObject = null;

        this.pc = new RTCPeerConnection();
        this.pc.addTransceiver('video', { direction: 'recvonly' });

        this.pc.ontrack = (event) => {
            this.videoElement.srcObject = event.streams[0];
            this.videoElement.play().catch(() => {});
            this.videoElement.style.border = "none";
            this.isReconnecting = false;
        };

        this.pc.onconnectionstatechange = () => {
            if (this.pc.connectionState === 'failed' || this.pc.connectionState === 'disconnected') {
                if (!this.isReconnecting) {
                    this.connect();
                }
            }
        };

        this.pc.createOffer()
            .then(offer => this.pc.setLocalDescription(offer))
            .then(() => fetch(this.url, {
                method: 'POST',
                headers: { 'Content-Type': 'application/sdp' },
                body: this.pc.localDescription.sdp
            }))
            .then(res => {
                if (!res.ok) throw new Error();
                return res.text();
            })
            .then(sdp => this.pc.setRemoteDescription({ type: 'answer', sdp: sdp }))
            .catch(() => {
                this.isReconnecting = false;
            });
    }

    checkStream() {
        if (this.isReconnecting || !this.pc) return;

        this.pc.getStats(null).then(stats => {
            let currentFrames = 0;
            stats.forEach(report => {
                if (report.type === 'inbound-rtp' && report.kind === 'video') {
                    currentFrames = report.framesDecoded || report.packetsReceived || 0;
                }
            });

            if (this.lastFrames !== -1 && currentFrames === this.lastFrames) {
                this.connect();
            }

            this.lastFrames = currentFrames;
        }).catch(() => {
            this.connect();
        });
    }
}

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

    setInterval(fetchBoxes, 100);
}

function handleToggle(cam, isChecked) {
    if (isChecked) {
        startCamera(cam);
    } else {
        stopCamera(cam.id);
    }
}

function startCamera(cam) {
    if (activeReaders[cam.id]) return;

    const wrapper = document.createElement('div');
    wrapper.className = 'cam-wrapper';
    wrapper.id = `wrapper-${cam.id}`;

    const video = document.createElement('video');
    video.setAttribute('autoplay', '');
    video.setAttribute('muted', '');
    video.setAttribute('playsinline', '');
    video.autoplay = true;
    video.muted = true;
    video.controls = false;

    const canvas = document.createElement('canvas');
    canvas.className = 'ai-canvas';

    video.addEventListener('loadedmetadata', () => {
        canvas.width = video.videoWidth;
        canvas.height = video.videoHeight;
    });

    video.addEventListener('loadeddata', () => {
        video.play().catch(() => {});
    });

    video.addEventListener('dblclick', () => openFullscreen(video, canvas, cam.name));

    const nameTag = document.createElement('div');
    nameTag.className = 'cam-name';
    nameTag.innerText = cam.name;

    wrapper.appendChild(video);
    wrapper.appendChild(canvas);
    wrapper.appendChild(nameTag);
    grid.appendChild(wrapper);

    const whepUrl = `${MEDIAMTX_URL}/${cam.id}/whep`;
    const reader = new NativeWHEPClient(whepUrl, video);

    activeReaders[cam.id] = { reader, wrapper, canvas, video };
}

function stopCamera(camId) {
    const session = activeReaders[camId];
    if (session) {
        if (session.reader) session.reader.close();
        if (session.wrapper) session.wrapper.remove();
        delete activeReaders[camId];
    }
}

async function fetchBoxes() {
    try {
        const response = await fetch('/api/boxes');
        const data = await response.json();

        for (const [camIntId, boxes] of Object.entries(data)) {
            const camIdStr = `cam${camIntId}`;
            const session = activeReaders[camIdStr];

            if (session && session.canvas && session.canvas.width > 0) {
                const ctx = session.canvas.getContext('2d');
                const w = session.canvas.width;
                const h = session.canvas.height;

                ctx.clearRect(0, 0, w, h);

                boxes.forEach(box => {
                    const bw = box.x2 - box.x1;
                    const bh = box.y2 - box.y1;

                    ctx.strokeStyle = '#00ff00';
                    ctx.lineWidth = Math.max(2, w / 300);
                    ctx.strokeRect(box.x1, box.y1, bw, bh);

                    ctx.fillStyle = '#00ff00';
                    ctx.font = '16px Arial';
                    const textWidth = ctx.measureText(box.label).width;
                    ctx.fillRect(box.x1, box.y1 - 22, textWidth + 10, 22);

                    ctx.fillStyle = '#000000';
                    ctx.fillText(box.label, box.x1 + 5, box.y1 - 6);
                });
            }
        }
    } catch (e) {}
}

function openFullscreen(sourceVideo, sourceCanvas, camName) {
    modalContent.innerHTML = '';

    const fsVideo = document.createElement('video');
    fsVideo.srcObject = sourceVideo.srcObject;
    fsVideo.autoplay = true;
    fsVideo.muted = true;
    fsVideo.controls = false;

    const fsCanvas = document.createElement('canvas');
    fsCanvas.className = 'ai-canvas';
    fsCanvas.width = sourceCanvas.width;
    fsCanvas.height = sourceCanvas.height;

    const nameTag = document.createElement('div');
    nameTag.className = 'cam-name';
    nameTag.innerText = `${camName} (Фокус)`;

    modalContent.appendChild(fsVideo);
    modalContent.appendChild(fsCanvas);
    modalContent.appendChild(nameTag);
    modal.classList.remove('hidden');

    const syncInterval = setInterval(() => {
        if(modal.classList.contains('hidden')) {
            clearInterval(syncInterval);
            return;
        }
        const ctx = fsCanvas.getContext('2d');
        ctx.clearRect(0, 0, fsCanvas.width, fsCanvas.height);
        ctx.drawImage(sourceCanvas, 0, 0);
    }, 50);
}

document.getElementById('close-modal').addEventListener('click', () => {
    modal.classList.add('hidden');
    modalContent.innerHTML = '';
});

const sidebar = document.getElementById('sidebar');
const toggleBtn = document.getElementById('toggle-sidebar');

toggleBtn.addEventListener('click', () => {
    sidebar.classList.toggle('collapsed');
});

window.addEventListener('load', init);