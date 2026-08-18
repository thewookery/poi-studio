/**
 * ============================================================================
 * OPEN PIXEL POI - WEB BLUETOOTH (BLE) HARDWARE UPLINK ENGINE
 * ============================================================================
 * Fully compatible with official Open Pixel Poi ESP32 Firmware & Nordic UART Service.
 * Allows direct browser-to-poi pattern uploading, show flashing, brightness,
 * speed, pattern slot, and bank control over Bluetooth Low Energy.
 */

(function (global) {
    'use strict';

    // Nordic UART Service (NUS) UUIDs
    const NORDIC_UART_SERVICE = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
    const NORDIC_UART_RX_CHAR = '6e400002-b5a3-f393-e0a9-e50e24dcca9e'; // Write to Poi
    const NORDIC_UART_TX_CHAR = '6e400003-b5a3-f393-e0a9-e50e24dcca9e'; // Read from Poi
    const NORDIC_UART_NOTIFY_CHAR = '6e400004-b5a3-f393-e0a9-e50e24dcca9e';

    // Framing protocol bytes
    const START_BYTE = 0xD0;
    const END_BYTE = 0xD1;

    // Command Codes (from OpenPixelPoiBLE firmware)
    const COMM_CODES = {
        CC_SUCCESS: 0x00,
        CC_ERROR: 0x01,
        CC_SET_BRIGHTNESS: 0x02,
        CC_SET_SPEED: 0x03,
        CC_SET_PATTERN: 0x04,
        CC_SET_PATTERN_SLOT: 0x05,
        CC_SET_PATTERN_ALL: 0x06,
        CC_SET_BANK: 0x07,
        CC_SET_BANK_ALL: 0x08,
        CC_GET_FW_VERSION: 0x09,
        CC_SET_HARDWARE_VERSION: 0x0A,
        CC_SET_LED_TYPE: 0x0B,
        CC_SET_LED_COUNT: 0x0C,
        CC_SET_DEVICE_NAME: 0x0D,
        CC_SET_SEQUENCER: 0x0E,
        CC_START_SEQUENCER: 0x0F,
        CC_SET_BRIGHTNESS_OPTION: 0x10,
        CC_SET_BRIGHTNESS_OPTIONS: 0x11,
        CC_SET_SPEED_OPTION: 0x12,
        CC_SET_SPEED_OPTIONS: 0x13,
        CC_SET_PATTERN_SHUFFLE_DURATION: 0x14
    };

    class OpenPixelPoiBLEClient {
        constructor() {
            this.devices = []; // Array of { id, device, server, service, rxCharacteristic, txCharacteristic, name }
            this.isUploading = false;
            this.stateListeners = [];
            this.maxPacketSize = 500; // Safe chunk size below 509 MTU
        }

        get isConnected() {
            return this.devices.length > 0;
        }

        get count() {
            return this.devices.length;
        }

        get deviceNames() {
            return this.devices.map(function(d) { return d.name; });
        }

        isSupported() {
            return typeof navigator !== 'undefined' && !!navigator.bluetooth;
        }

        onStateChange(listener) {
            if (typeof listener === 'function') {
                this.stateListeners.push(listener);
            }
        }

        _notifyState(state, data) {
            data = data || {};
            this.stateListeners.forEach(function(fn) {
                try {
                    fn(state, Object.assign({
                        isConnected: this.isConnected,
                        count: this.count,
                        devices: this.devices.map(function(d) { return { id: d.id, name: d.name }; })
                    }, data));
                } catch (e) {
                    console.error('[BLE Callback Error]', e);
                }
            }.bind(this));
        }

        async connectDevice(options) {
            options = options || {};
            if (!this.isSupported()) {
                throw new Error('Web Bluetooth is not supported in this browser. Please use Google Chrome, Microsoft Edge, or Android Chromium (or Bluefy on iOS).');
            }

            try {
                // Strict filters: only show devices matching Open or Poi
                const scanConfig = (options.scanAll === true) ? {
                    acceptAllDevices: true,
                    optionalServices: [NORDIC_UART_SERVICE]
                } : {
                    filters: [
                        { namePrefix: 'Open' },
                        { namePrefix: 'open' },
                        { namePrefix: 'Poi' },
                        { namePrefix: 'poi' },
                        { namePrefix: 'POI' },
                        { namePrefix: 'Pixel' },
                        { namePrefix: 'pixel' }
                    ],
                    optionalServices: [NORDIC_UART_SERVICE]
                };

                const device = await navigator.bluetooth.requestDevice(scanConfig);

                // Check if already in list
                const existing = this.devices.find(function(d) { return d.device.id === device.id; });
                if (existing) {
                    this._notifyState('connected', { device: existing });
                    return existing;
                }

                const server = await device.gatt.connect();
                const service = await server.getPrimaryService(NORDIC_UART_SERVICE);
                const rxChar = await service.getCharacteristic(NORDIC_UART_RX_CHAR);
                let txChar = null;
                try {
                    txChar = await service.getCharacteristic(NORDIC_UART_TX_CHAR);
                } catch (e) {
                    console.warn('[BLE] TX characteristic not available (write-only mode)');
                }

                const deviceEntry = {
                    id: device.id,
                    device: device,
                    server: server,
                    service: service,
                    rxCharacteristic: rxChar,
                    txCharacteristic: txChar,
                    name: device.name || ('Open Pixel Poi #' + (this.devices.length + 1))
                };

                device.addEventListener('gattserverdisconnected', function() {
                    this.devices = this.devices.filter(function(d) { return d.id !== device.id; });
                    this._notifyState('disconnected', { disconnectedId: device.id });
                }.bind(this));

                this.devices.push(deviceEntry);
                this._notifyState('connected', { addedDevice: deviceEntry });
                return deviceEntry;
            } catch (err) {
                this._notifyState('error', { error: err.message });
                throw err;
            }
        }

        async getPairedDevices() {
            if (!this.isSupported() || !navigator.bluetooth.getDevices) return [];
            try {
                return await navigator.bluetooth.getDevices();
            } catch (e) {
                console.warn('[BLE] getDevices error:', e);
                return [];
            }
        }

        async reconnectPaired() {
            const paired = await this.getPairedDevices();
            if (!paired || paired.length === 0) return 0;
            let connectedCount = 0;
            for (let device of paired) {
                if (!this.devices.some(function(d) { return d.id === device.id; })) {
                    try {
                        const server = await device.gatt.connect();
                        const service = await server.getPrimaryService(NORDIC_UART_SERVICE);
                        const rxChar = await service.getCharacteristic(NORDIC_UART_RX_CHAR);
                        let txChar = null;
                        try {
                            txChar = await service.getCharacteristic(NORDIC_UART_TX_CHAR);
                        } catch (e) {}

                        const deviceEntry = {
                            id: device.id,
                            device: device,
                            server: server,
                            service: service,
                            rxCharacteristic: rxChar,
                            txCharacteristic: txChar,
                            name: device.name || ('Open Pixel Poi #' + (this.devices.length + 1))
                        };

                        device.addEventListener('gattserverdisconnected', function() {
                            this.devices = this.devices.filter(function(d) { return d.id !== device.id; });
                            this._notifyState('disconnected', { disconnectedId: device.id });
                        }.bind(this));

                        this.devices.push(deviceEntry);
                        this._notifyState('connected', { addedDevice: deviceEntry });
                        connectedCount++;
                    } catch (err) {
                        console.warn('[BLE] Could not auto-reconnect to:', device.name, err.message);
                    }
                }
            }
            return connectedCount;
        }


        async disconnectDevice(id) {
            const entry = this.devices.find(function(d) { return d.id === id; });
            if (entry && entry.device.gatt && entry.device.gatt.connected) {
                await entry.device.gatt.disconnect();
            }
            this.devices = this.devices.filter(function(d) { return d.id !== id; });
            this._notifyState('disconnected');
        }

        async disconnectAll() {
            for (let i = 0; i < this.devices.length; i++) {
                try {
                    if (this.devices[i].device.gatt && this.devices[i].device.gatt.connected) {
                        await this.devices[i].device.gatt.disconnect();
                    }
                } catch (e) {}
            }
            this.devices = [];
            this._notifyState('disconnected');
        }

        // Backward compatibility wrappers
        async connect(options) {
            return this.connectDevice(options);
        }

        async disconnect() {
            return this.disconnectAll();
        }

        async _writeChunkToDevice(deviceEntry, bytes) {
            if (!deviceEntry || !deviceEntry.rxCharacteristic) {
                throw new Error('Device is not ready for transmission.');
            }
            const buffer = new Uint8Array(bytes);
            if (deviceEntry.rxCharacteristic.writeValueWithoutResponse) {
                await deviceEntry.rxCharacteristic.writeValueWithoutResponse(buffer);
            } else {
                await deviceEntry.rxCharacteristic.writeValue(buffer);
            }
        }

        async _writeChunkToAll(bytes) {
            if (this.devices.length === 0) {
                throw new Error('No Open Pixel Poi connected via Bluetooth.');
            }

            const buffer = new Uint8Array(bytes);
            const promises = this.devices.map(function(d) {
                if (d.rxCharacteristic.writeValueWithoutResponse) {
                    return d.rxCharacteristic.writeValueWithoutResponse(buffer).catch(function(err) {
                        console.warn('[BLE Write Error on ' + d.name + ']', err);
                    });
                } else {
                    return d.rxCharacteristic.writeValue(buffer).catch(function(err) {
                        console.warn('[BLE Write Error on ' + d.name + ']', err);
                    });
                }
            });

            await Promise.all(promises);
        }

        async _sendMessage(messageBytes) {
            const packet = [START_BYTE].concat(messageBytes).concat([END_BYTE]);
            await this._writeChunkToAll(packet);
        }

        /**
         * Set LED Brightness on all connected Poi
         * @param {number} brightness 0 (off) to 255 (max)
         */
        async setBrightness(brightness) {
            const b = Math.max(0, Math.min(255, Math.round(brightness)));
            await this._sendMessage([COMM_CODES.CC_SET_BRIGHTNESS, b]);
        }

        /**
         * Set Animation Speed (Frames per second / Hz) on all connected Poi
         * @param {number} speedHz 1 to 65535
         */
        async setSpeed(speedHz) {
            const s = Math.max(1, Math.min(65535, Math.round(speedHz)));
            await this._sendMessage([
                COMM_CODES.CC_SET_SPEED,
                (s >> 8) & 0xFF,
                s & 0xFF
            ]);
        }

        /**
         * Change Active Pattern Slot (0-15) on all connected Poi
         */
        async setPatternSlot(slotIndex) {
            const slot = Math.max(0, Math.min(15, parseInt(slotIndex) || 0));
            await this._sendMessage([COMM_CODES.CC_SET_PATTERN_SLOT, slot]);
        }

        /**
         * Auto-Loop All Patterns in current bank on all connected Poi
         */
        async loopAllPatterns() {
            await this._sendMessage([COMM_CODES.CC_SET_PATTERN_ALL]);
        }

        /**
         * Change Active Bank (0-15) on all connected Poi
         */
        async setBank(bankIndex) {
            const bank = Math.max(0, Math.min(15, parseInt(bankIndex) || 0));
            await this._sendMessage([COMM_CODES.CC_SET_BANK, bank]);
        }

        /**
         * Auto-Loop All Banks on all connected Poi
         */
        async loopAllBanks() {
            await this._sendMessage([COMM_CODES.CC_SET_BANK_ALL]);
        }

        /**
         * Convert a Canvas into Open Pixel Poi Column-Major RGB Byte Array
         */
        canvasToPatternBytes(canvas) {
            const width = canvas.width;
            const height = canvas.height;
            const totalPixels = width * height;

            if (totalPixels > 40000) {
                throw new Error('Pattern size (' + width + 'x' + height + ' = ' + totalPixels + ' pixels) exceeds Open Pixel Poi hardware buffer limit (40,000 pixels max).');
            }
            if (height > 255) {
                throw new Error('Pattern height (' + height + 'px) exceeds Open Pixel Poi LED limit (255px max).');
            }

            const ctx = canvas.getContext('2d');
            const imgData = ctx.getImageData(0, 0, width, height).data;
            const rgbData = new Uint8Array(totalPixels * 3);

            let ptr = 0;
            // Column-Major order as required by ESP32 firmware LED engine
            for (let x = 0; x < width; x++) {
                for (let y = 0; y < height; y++) {
                    const srcIdx = (y * width + x) * 4;
                    rgbData[ptr++] = imgData[srcIdx];     // Red
                    rgbData[ptr++] = imgData[srcIdx + 1]; // Green
                    rgbData[ptr++] = imgData[srcIdx + 2]; // Blue
                }
            }

            return {
                width: width,
                height: height,
                rgbData: rgbData
            };
        }

        /**
         * Build properly formatted BLE packets matching ESP32 open_pixel_poi_ble.cpp firmware:
         * - Single packet if <= 509 bytes.
         * - For multipart (> 509 bytes): every middle chunk is EXACTLY 509 bytes.
         * - The final chunk is strictly < 509 bytes and ends with 0xD1.
         */
        buildPatternPackets(canvas) {
            const data = this.canvasToPatternBytes(canvas);
            const width = data.width;
            const height = data.height;
            const rgbData = data.rgbData;

            const totalLength = 1 + 1 + 1 + 2 + rgbData.length + 1;
            const fullPacket = new Uint8Array(totalLength);

            fullPacket[0] = START_BYTE;
            fullPacket[1] = COMM_CODES.CC_SET_PATTERN;
            fullPacket[2] = height & 0xFF;
            fullPacket[3] = (width >> 8) & 0xFF;
            fullPacket[4] = width & 0xFF;
            fullPacket.set(rgbData, 5);
            fullPacket[totalLength - 1] = END_BYTE;

            const CHUNK_SIZE = 509; // Required by ESP32 firmware
            const packets = [];
            let offset = 0;

            while (offset < fullPacket.length) {
                const remaining = fullPacket.length - offset;
                if (remaining > CHUNK_SIZE) {
                    packets.push(fullPacket.subarray(offset, offset + CHUNK_SIZE));
                    offset += CHUNK_SIZE;
                } else if (remaining === CHUNK_SIZE) {
                    // Send 509 bytes, then a 1-byte EOF packet so the final packet is strictly < 509
                    packets.push(fullPacket.subarray(offset, offset + CHUNK_SIZE));
                    packets.push(new Uint8Array([END_BYTE]));
                    offset += CHUNK_SIZE;
                } else {
                    // remaining < 509 (standard final packet)
                    packets.push(fullPacket.subarray(offset, offset + remaining));
                    offset += remaining;
                }
            }

            return {
                width: width,
                height: height,
                totalBytes: fullPacket.length,
                packets: packets
            };
        }

        /**
         * Upload a Pattern Canvas sequentially to each connected Open Pixel Poi over BLE
         * @param {HTMLCanvasElement} canvas The canvas to upload
         * @param {Function} progressCallback Optional callback (progress: 0.0 - 1.0, details)
         * @param {number} pacingDelayMs Optional delay between packets in ms (default 45ms)
         */
        async uploadPattern(canvas, progressCallback, pacingDelayMs) {
            if (this.devices.length === 0) {
                throw new Error('Please connect your Open Pixel Poi via Bluetooth first.');
            }
            if (this.isUploading) {
                throw new Error('Another pattern upload is already in progress.');
            }

            const pacing = (typeof pacingDelayMs === 'number' && pacingDelayMs >= 10) ? pacingDelayMs : 45;
            const built = this.buildPatternPackets(canvas);
            const packets = built.packets;
            const totalPackets = packets.length;
            const totalDevices = this.devices.length;
            const totalOverallSteps = totalPackets * totalDevices;

            this.isUploading = true;

            try {
                console.log('[BLE Upload] Starting sequential transfer to ' + totalDevices + ' Poi (' + built.width + 'x' + built.height + ', ' + built.totalBytes + ' bytes, ' + totalPackets + ' packets per poi, ' + pacing + 'ms pacing)...');

                for (let dIdx = 0; dIdx < totalDevices; dIdx++) {
                    const currentDev = this.devices[dIdx];
                    console.log('[BLE Upload] Transferring to Poi ' + (dIdx + 1) + '/' + totalDevices + ' (' + currentDev.name + ')...');

                    for (let pIdx = 0; pIdx < totalPackets; pIdx++) {
                        const chunk = packets[pIdx];
                        await this._writeChunkToDevice(currentDev, chunk);

                        const currentStep = (dIdx * totalPackets) + (pIdx + 1);
                        const overallProgress = currentStep / totalOverallSteps;
                        const deviceProgress = (pIdx + 1) / totalPackets;

                        if (progressCallback) {
                            progressCallback(overallProgress, {
                                deviceIndex: dIdx + 1,
                                totalDevices: totalDevices,
                                deviceName: currentDev.name,
                                deviceProgress: deviceProgress,
                                chunk: pIdx + 1,
                                totalChunks: totalPackets,
                                sentBytes: Math.min((pIdx + 1) * 509, built.totalBytes),
                                totalBytes: built.totalBytes
                            });
                        }

                        // Smooth hardware pacing delay (45ms) to ensure ESP32 LittleFS flash writes cleanly
                        await new Promise(function(r) { setTimeout(r, pacing); });
                    }

                    // Inter-device buffer settling pause (250ms)
                    if (dIdx < totalDevices - 1) {
                        await new Promise(function(r) { setTimeout(r, 250); });
                    }
                }

                console.log('[BLE Upload] Pattern successfully delivered cleanly to all ' + totalDevices + ' Open Pixel Poi!');
                return true;
            } finally {
                this.isUploading = false;
            }
        }
    }

    // Export singleton instance
    global.OpenPixelPoiBLE = new OpenPixelPoiBLEClient();

})(typeof window !== 'undefined' ? window : this);

