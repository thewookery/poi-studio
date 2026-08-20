// =========================================================================
// OPEN POI STUDIO - WI-FI HIGH-SPEED CLIENT ENGINE (poi-wifi.js)
// Turbo HTTP/REST & ESP-NOW Prop Sync Controller
// =========================================================================

class PoiWiFiEngine {
    constructor() {
        this.ip = '192.168.4.1';
        this.isConnected = false;
        this.statusInterval = null;
        this.lastStatus = null;
    }

    async checkConnection(targetIp) {
        const ipToTest = targetIp || this.ip;
        try {
            const controller = new AbortController();
            const timeoutId = setTimeout(() => controller.abort(), 2500);
            const res = await fetch(`http://${ipToTest}/api/status`, {
                signal: controller.signal,
                headers: { 'Accept': 'application/json' }
            });
            clearTimeout(timeoutId);
            if (res.ok) {
                const data = await res.json();
                this.ip = ipToTest;
                this.isConnected = true;
                this.lastStatus = data;
                this.onConnected(data);
                this.startPolling();
                return { success: true, data };
            }
        } catch (err) {
            console.warn('[PoiWiFi] Connection check failed:', err.message);
        }
        return { success: false };
    }

    startPolling() {
        if (this.statusInterval) clearInterval(this.statusInterval);
        this.statusInterval = setInterval(async () => {
            if (!this.isConnected) return;
            try {
                const res = await fetch(`http://${this.ip}/api/status`);
                if (res.ok) {
                    const data = await res.json();
                    this.lastStatus = data;
                    this.updateTelemetry(data);
                }
            } catch (e) {
                // Ignore transient polling drops
            }
        }, 3000);
    }

    stopPolling() {
        if (this.statusInterval) {
            clearInterval(this.statusInterval);
            this.statusInterval = null;
        }
    }

    disconnect() {
        this.isConnected = false;
        this.stopPolling();
        this.onDisconnected();
    }

    async setState(params) {
        if (!this.isConnected) return false;
        try {
            const query = new URLSearchParams(params).toString();
            const res = await fetch(`http://${this.ip}/api/state?${query}`, {
                method: 'POST'
            });
            return res.ok;
        } catch (err) {
            console.error('[PoiWiFi] setState error:', err);
            return false;
        }
    }

    async uploadPatternToSlot(canvas, bank, slot, onProgress = null) {
        if (!this.isConnected) {
            throw new Error('Poi is not connected over Wi-Fi! Connect to OpenPixelPoi hotspot first.');
        }

        if (onProgress) onProgress(0.1, 'Encoding pattern...');

        const ctx = canvas.getContext('2d');
        const width = canvas.width;
        const height = canvas.height;
        const imgData = ctx.getImageData(0, 0, width, height).data;

        // Convert to 24-bit RGB BMP buffer
        const rowSize = Math.floor((24 * width + 31) / 32) * 4;
        const pixelArraySize = rowSize * height;
        const fileSize = 54 + pixelArraySize;
        const buffer = new ArrayBuffer(fileSize);
        const view = new DataView(buffer);

        // BMP Header
        view.setUint16(0, 0x4D42, false); // "BM"
        view.setUint32(2, fileSize, true);
        view.setUint32(10, 54, true); // offset to pixel data
        view.setUint32(14, 40, true); // DIB header size
        view.setInt32(18, width, true);
        view.setInt32(22, -height, true); // Top-down
        view.setUint16(26, 1, true); // Planes
        view.setUint16(28, 24, true); // 24-bit BGR
        view.setUint32(34, pixelArraySize, true);

        const bytes = new Uint8Array(buffer);
        let dstOffset = 54;

        for (let y = 0; y < height; y++) {
            for (let x = 0; x < width; x++) {
                const srcIdx = (y * width + x) * 4;
                bytes[dstOffset++] = imgData[srcIdx + 2]; // B
                bytes[dstOffset++] = imgData[srcIdx + 1]; // G
                bytes[dstOffset++] = imgData[srcIdx + 0]; // R
            }
            // Padding to 4-byte boundary
            const pad = rowSize - (width * 3);
            for (let p = 0; p < pad; p++) bytes[dstOffset++] = 0;
        }

        if (onProgress) onProgress(0.5, 'Streaming to LittleFS...');

        const blob = new Blob([buffer], { type: 'image/bmp' });
        const formData = new FormData();
        formData.append('file', blob, `p_${bank}_${slot}.bmp`);

        const url = `http://${this.ip}/api/pattern?bank=${bank}&slot=${slot}&width=${width}&height=${height}`;
        const res = await fetch(url, {
            method: 'POST',
            body: formData
        });

        if (!res.ok) {
            throw new Error(`Upload failed with status: ${res.status}`);
        }

        if (onProgress) onProgress(1.0, 'Upload Complete!');
        return true;
    }

    onConnected(data) {
        const btn = document.getElementById('btn-wifi-connect');
        if (btn) {
            btn.classList.add('connected');
            btn.innerHTML = `🌐 Wi-Fi: Connected (Bank ${data.bank + 1})`;
            btn.style.background = 'rgba(0, 255, 136, 0.2)';
            btn.style.borderColor = 'var(--neon-green)';
            btn.style.color = 'var(--neon-green)';
        }
        this.updateTelemetry(data);
        if (window.showToast) {
            window.showToast(`🌐 Connected to Open Pixel Poi over Wi-Fi! Instant sync active.`, 3000);
        }
    }

    onDisconnected() {
        const btn = document.getElementById('btn-wifi-connect');
        if (btn) {
            btn.classList.remove('connected');
            btn.innerHTML = `🌐 Connect Wi-Fi`;
            btn.style.background = '';
            btn.style.borderColor = '';
            btn.style.color = '';
        }
    }

    updateTelemetry(data) {
        if (!data) return;
        const freeKb = Math.round((data.freeBytes || 0) / 1024);
        const freeMb = ((data.freeBytes || 0) / (1024 * 1024)).toFixed(2);

        const storageEl = document.getElementById('ble-storage-info');
        if (storageEl) {
            storageEl.innerHTML = `💾 Flash: <b>${freeMb} MB free</b> (${freeKb} KB) • Slot <b>${data.slot + 1}</b> • Bank <b>${data.bank + 1}</b>`;
        }
    }
}

window.poiWiFi = new PoiWiFiEngine();
