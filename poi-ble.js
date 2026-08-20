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
        CC_SET_PATTERN_SHUFFLE_DURATION: 0x14,
        CC_SET_PALETTE_FX: 0x15,
        CC_SET_BLEND_MODE: 0x16,
        CC_SET_PALETTE_SPEED: 0x17,
        CC_SET_MOTION_FX: 0x18
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

        /**
         * Calculate accurate battery status and percentage using Open Pixel Poi standard voltage curve:
         * - Green (Full / Healthy): >= 3.90V (~75% - 100%)
         * - Yellow (Medium / Nominal): 3.50V - 3.90V (~25% - 75%)
         * - Red (Low / Critical): <= 3.45V (< 25%)
         */
        parseBatteryData(rawVal) {
            if (rawVal === null || rawVal === undefined) return null;
            let val = Number(rawVal);
            if (isNaN(val)) return null;

            // If raw voltage in millivolts (e.g. 3300 - 4200)
            if (val > 1000) {
                val = val / 1000.0;
            }
            
            // If voltage in volts (3.25V - 4.20V)
            if (val > 2.0 && val <= 5.0) {
                // LiPo 1S curve mapping based on OPP hardware thresholds
                let pct = Math.round(((val - 3.45) / (4.15 - 3.45)) * 100);
                pct = Math.max(0, Math.min(100, pct));
                return {
                    percent: pct,
                    voltage: val.toFixed(2),
                    status: (val >= 3.90) ? 'green' : (val >= 3.50 ? 'yellow' : 'red'),
                    icon: (val >= 3.90) ? '🟢' : (val >= 3.50 ? '🟡' : '🔴'),
                    label: (val >= 3.90) ? 'Good' : (val >= 3.50 ? 'Medium' : 'Low')
                };
            }


            // If percentage (0 - 100)
            let pct = Math.max(0, Math.min(100, Math.round(val)));
            let status = (pct >= 70) ? 'green' : (pct >= 30 ? 'yellow' : 'red');
            let icon = (pct >= 70) ? '🟢' : (pct >= 30) ? '🟡' : '🔴';
            let label = (pct >= 70) ? 'Good' : (pct >= 30) ? 'Medium' : 'Low';
            return {
                percent: pct,
                voltage: (3.45 + (pct / 100) * 0.70).toFixed(2),
                status: status,
                icon: icon,
                label: label
            };
        }

        _notifyState(state, data) {
            data = data || {};
            this.stateListeners.forEach(function(fn) {
                try {
                    fn(state, Object.assign({
                        isConnected: this.isConnected,
                        count: this.count,
                        devices: this.devices.map(function(d) { 
                            return { 
                                id: d.id, 
                                name: d.name,
                                batteryLevel: d.batteryLevel,
                                batteryInfo: d.batteryInfo
                            }; 
                        })
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
                    optionalServices: [NORDIC_UART_SERVICE, 'battery_service']
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
                    optionalServices: [NORDIC_UART_SERVICE, 'battery_service']
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
                    batteryLevel: null,
                    batteryInfo: null,
                    name: device.name || ('Open Pixel Poi #' + (this.devices.length + 1))
                };

                device.addEventListener('gattserverdisconnected', function() {
                    this.devices = this.devices.filter(function(d) { return d.id !== device.id; });
                    this._notifyState('disconnected', { disconnectedId: device.id });
                }.bind(this));

                this.devices.push(deviceEntry);
                this._saveDeviceCache();
                this._notifyState('connected', { addedDevice: deviceEntry });

                // Deferred non-blocking battery check (waits 2.5s for BLE settle)
                this._queryBatteryDeferred(deviceEntry);

                return deviceEntry;
            } catch (err) {
                this._notifyState('error', { error: err.message });
                throw err;
            }
        }

        _saveDeviceCache() {
            try {
                let existing = [];
                try {
                    const raw = localStorage.getItem('open_poi_cached_devices');
                    if (raw) existing = JSON.parse(raw);
                } catch(e) {}
                if (!Array.isArray(existing)) existing = [];

                this.devices.forEach(function(d) {
                    const found = existing.find(function(item) { return item.id === d.id; });
                    if (found) {
                        found.name = d.name;
                        found.lastSeen = Date.now();
                    } else {
                        existing.push({ id: d.id, name: d.name, lastSeen: Date.now() });
                    }
                });
                localStorage.setItem('open_poi_cached_devices', JSON.stringify(existing));
            } catch (e) {}
        }

        async _queryBatteryDeferred(deviceEntry) {
            setTimeout(async () => {
                try {
                    if (!deviceEntry.server || !deviceEntry.server.connected) return;
                    const batService = await deviceEntry.server.getPrimaryService('battery_service');
                    if (batService) {
                        const batChar = await batService.getCharacteristic('battery_level');
                        const val = await batChar.readValue();
                        const raw = val.getUint8(0);
                        deviceEntry.batteryLevel = raw;
                        deviceEntry.batteryInfo = this.parseBatteryData(raw);
                        console.log(`[BLE] 🔋 Deferred Battery for ${deviceEntry.name}:`, deviceEntry.batteryInfo);
                        this._notifyState('connected');
                    }
                } catch (e) {
                    // Stock firmware does not implement 0x180F Battery Service
                }
            }, 2500);
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
                        } catch (e) {
                            console.warn('[BLE] TX characteristic not available');
                        }

                        const deviceEntry = {
                            id: device.id,
                            device: device,
                            server: server,
                            service: service,
                            rxCharacteristic: rxChar,
                            txCharacteristic: txChar,
                            batteryLevel: null,
                            batteryInfo: null,
                            name: device.name || ('Open Pixel Poi #' + (this.devices.length + 1))
                        };

                        device.addEventListener('gattserverdisconnected', function() {
                            this.devices = this.devices.filter(function(d) { return d.id !== device.id; });
                            this._notifyState('disconnected', { disconnectedId: device.id });
                        }.bind(this));

                        this.devices.push(deviceEntry);
                        this._saveDeviceCache();
                        this._queryBatteryDeferred(deviceEntry);
                        connectedCount++;
                    } catch (e) {
                        console.warn('[BLE] Failed auto-reconnect to:', device.name, e);
                    }
                }
            }
            if (connectedCount > 0) {
                this._notifyState('connected', { count: this.devices.length });
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

        async _sendMessageToDevice(deviceEntry, messageBytes) {
            const packet = [START_BYTE].concat(messageBytes).concat([END_BYTE]);
            await this._writeChunkToDevice(deviceEntry, packet);
        }

        async _sendMessage(messageBytes) {
            const packet = [START_BYTE].concat(messageBytes).concat([END_BYTE]);
            await this._writeChunkToAll(packet);
        }

        /**
         * Set LED Brightness Option Level (0 to 5) on all connected Poi
         * @param {number} optionIndex 0 to 5
         */
        async setBrightnessOption(optionIndex) {
            const opt = Math.max(0, Math.min(5, parseInt(optionIndex) || 0));
            await this._sendMessage([COMM_CODES.CC_SET_BRIGHTNESS_OPTION, opt]);
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
         * Set Animation Speed Option Level (0 to 5) on all connected Poi
         * @param {number} optionIndex 0 to 5
         */
        async setSpeedOption(optionIndex) {
            const opt = Math.max(0, Math.min(5, parseInt(optionIndex) || 0));
            await this._sendMessage([COMM_CODES.CC_SET_SPEED_OPTION, opt]);
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
         * Change Active Pattern Slot on a specific Poi
         */
        async setPatternSlotOnDevice(deviceEntry, slotIndex) {
            const slot = Math.max(0, Math.min(15, parseInt(slotIndex) || 0));
            await this._sendMessageToDevice(deviceEntry, [COMM_CODES.CC_SET_PATTERN_SLOT, slot]);
        }

        /**
         * Set Auto-Loop Pattern Shuffle Interval (in seconds, 1 - 255)
         */
        async setPatternShuffleDuration(seconds) {
            const sec = Math.max(1, Math.min(255, parseInt(seconds) || 10));
            await this._sendMessage([COMM_CODES.CC_SET_PATTERN_SHUFFLE_DURATION, sec]);
        }

        /**
         * Auto-Loop All Patterns in current bank on all connected Poi
         */
        async loopAllPatterns(durationSeconds) {
            if (durationSeconds) {
                await this.setPatternShuffleDuration(durationSeconds);
                await new Promise(r => setTimeout(r, 100));
            }
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
         * Change Active Bank and Pattern Slot simultaneously on all connected Poi
         */
        async setBankAndSlot(bankIndex, slotIndex) {
            const bank = Math.max(0, Math.min(15, parseInt(bankIndex) || 0));
            const slot = Math.max(0, Math.min(15, parseInt(slotIndex) || 0));
            await this._sendMessage([COMM_CODES.CC_SET_BANK, bank]);
            await new Promise(r => setTimeout(r, 40));
            await this._sendMessage([COMM_CODES.CC_SET_PATTERN_SLOT, slot]);
        }

        /**
         * Auto-Loop All Banks on all connected Poi
         */
        async loopAllBanks(durationSeconds) {
            if (durationSeconds) {
                await this.setPatternShuffleDuration(durationSeconds);
                await new Promise(r => setTimeout(r, 100));
            }
            await this._sendMessage([COMM_CODES.CC_SET_BANK_ALL]);
        }

        /**
         * Set Real-Time Color Palette Filter (0=Normal, 1-20 Palettes)
         */
        async setPaletteFX(paletteId) {
            const id = Math.max(0, Math.min(24, parseInt(paletteId) || 0));
            await this._sendMessage([COMM_CODES.CC_SET_PALETTE_FX, id]);
        }

        /**
         * Set Real-Time Motion Flow FX Bank (0=Solid, 1=Flow Up, 2=Flow Down, 3=Rain, 4=Tidal, 5=Plasma, 6=Stardust, 7=Pulse, 8=POV Spin, 9=Spiral, 10=Strobe)
         */
        async setMotionFX(motionId) {
            const id = Math.max(0, Math.min(10, parseInt(motionId) || 0));
            await this._sendMessage([COMM_CODES.CC_SET_MOTION_FX, id]);
        }


        /**
         * Set Pattern Transition Blend Mode (0=Cut, 1=Fade, 2=Flash, 3=Wipe, 4=Glow, 5=Iris, 6=Squeeze, 7=Glitch, 8=Quantum, 9=Comet)
         */
        async setBlendMode(modeId) {
            const mode = Math.max(0, Math.min(9, parseInt(modeId) || 0));
            await this._sendMessage([COMM_CODES.CC_SET_BLEND_MODE, mode]);
        }


        /**
         * Set Palette Color Cycle Speed (1-10)
         */
        async setPaletteSpeed(speed) {
            const spd = Math.max(1, Math.min(10, parseInt(speed) || 3));
            await this._sendMessage([COMM_CODES.CC_SET_PALETTE_SPEED, spd]);
        }






        /**
         * Upload a Pattern Canvas directly into a specific Bank and Slot (0-4)
         * @param {HTMLCanvasElement} canvas The pattern canvas to upload
         * @param {number} bankIndex 0 to 15 (e.g. Bank 1 to 4)
         * @param {number} slotIndex 0 to 4 (Slot 1 to 5)
         * @param {Function} progressCallback Optional callback (progress: 0.0 - 1.0, info)
         * @param {number} pacingDelayMs Optional delay between packets
         */
        /**
         * Dynamically calculate adaptive transmission pacing and flash commit settle delays
         * based on pattern size (prevents data corruption or SPI flash write blocking).
         */
        calculateAdaptiveTimings(canvas, userPacing) {
            const width = canvas.width;
            const height = canvas.height;
            const totalPixels = width * height;
            const totalBytes = totalPixels * 3;

            // 1. Dynamic Packet Pacing:
            let pacing = 25;
            if (typeof userPacing === 'number' && userPacing >= 10) {
                pacing = userPacing;
                if (totalPixels > 15000 && pacing < 25) {
                    pacing = 28;
                }
            } else {
                if (totalPixels > 25000) {
                    pacing = 32; // ~75KB - 120KB patterns
                } else if (totalPixels > 10000) {
                    pacing = 28; // ~30KB - 75KB patterns
                } else {
                    pacing = 25; // Standard rock-solid fast patterns (~0.35s total)
                }
            }

            // 2. Dynamic Flash Commit Settle Delay:
            // LittleFS writes data to flash when the final EOF packet arrives.
            let commitSettleMs = 180;
            if (totalPixels > 25000) {
                commitSettleMs = 500; // Heavy SPI flash sector block write
            } else if (totalPixels > 10000) {
                commitSettleMs = 300;
            } else if (totalPixels > 4000) {
                commitSettleMs = 200;
            }

            return {
                totalPixels,
                totalBytes,
                pacing,
                commitSettleMs
            };
        }

        async uploadPatternToSlot(canvas, bankIndex, slotIndex, progressCallback, pacingDelayMs) {
            if (this.devices.length === 0) {
                throw new Error('Please connect your Open Pixel Poi via Bluetooth first.');
            }

            const bank = Math.max(0, Math.min(15, parseInt(bankIndex) || 0));
            const slot = Math.max(0, Math.min(15, parseInt(slotIndex) || 0));
            const timings = this.calculateAdaptiveTimings(canvas, pacingDelayMs);

            // 1. Select target bank and slot on all connected poi
            await this.setBank(bank);
            await new Promise(r => setTimeout(r, 80));
            await this.setPatternSlot(slot);
            await new Promise(r => setTimeout(r, 80));

            // 2. Upload pattern with adaptive pacing
            const result = await this.uploadPattern(canvas, progressCallback, timings.pacing);

            // 3. Adaptive Post-upload settle delay so ESP32 LittleFS commits cleanly
            await new Promise(r => setTimeout(r, timings.commitSettleMs + 100));

            // 4. Re-trigger slot playback so ESP32 immediately reloads and displays the new pattern on LEDs!
            await this.setPatternSlot(slot);
            await new Promise(r => setTimeout(r, 60));

            return result;
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

            const timings = this.calculateAdaptiveTimings(canvas, pacingDelayMs);
            const pacing = timings.pacing;
            const built = this.buildPatternPackets(canvas);
            const packets = built.packets;
            const totalPackets = packets.length;
            const totalDevices = this.devices.length;
            const totalOverallSteps = totalPackets * totalDevices;

            this.isUploading = true;

            try {
                console.log('[BLE Upload] Starting adaptive transfer to ' + totalDevices + ' Poi (' + built.width + 'x' + built.height + ', ' + built.totalBytes + ' bytes, ' + totalPackets + ' packets per poi, ' + pacing + 'ms pacing, ' + timings.commitSettleMs + 'ms commit)...');

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

                        // High-speed hardware pacing delay
                        await new Promise(function(r) { setTimeout(r, pacing); });
                    }

                    // Adaptive post-upload flash commit settling delay
                    await new Promise(function(r) { setTimeout(r, timings.commitSettleMs); });

                    // Inter-device buffer settling pause (150ms)
                    if (dIdx < totalDevices - 1) {
                        await new Promise(function(r) { setTimeout(r, 150); });
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

