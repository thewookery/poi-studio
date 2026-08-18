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
            this.device = null;
            this.server = null;
            this.service = null;
            this.rxCharacteristic = null;
            this.txCharacteristic = null;
            this.isConnected = false;
            this.isUploading = false;
            this.deviceName = 'Open Pixel Poi';
            this.stateListeners = [];
            this.maxPacketSize = 500; // Safe chunk size below 509 MTU
        }

        isSupported() {
            return typeof navigator !== 'undefined' && !!navigator.bluetooth;
        }

        onStateChange(listener) {
            if (typeof listener === 'function') {
                this.stateListeners.push(listener);
            }
        }

        _notifyState(state, data = {}) {
            this.stateListeners.forEach(fn => {
                try {
                    fn(state, {
                        isConnected: this.isConnected,
                        deviceName: this.deviceName,
                        ...data
                    });
                } catch (e) {
                    console.error('[BLE Callback Error]', e);
                }
            });
        }

        async connect() {
            if (!this.isSupported()) {
                throw new Error('Web Bluetooth is not supported in this browser. Please use Google Chrome, Microsoft Edge, or Android Chromium.');
            }

            try {
                this._notifyState('connecting');

                // Request BLE Device
                this.device = await navigator.bluetooth.requestDevice({
                    filters: [
                        { namePrefix: 'Pixel Poi' },
                        { services: [NORDIC_UART_SERVICE] }
                    ],
                    optionalServices: [NORDIC_UART_SERVICE]
                });

                this.deviceName = this.device.name || 'Pixel Poi';

                this.device.addEventListener('gattserverdisconnected', () => {
                    this.isConnected = false;
                    this.rxCharacteristic = null;
                    this.txCharacteristic = null;
                    this._notifyState('disconnected');
                });

                // Connect GATT Server
                this.server = await this.device.gatt.connect();
                this.service = await this.server.getPrimaryService(NORDIC_UART_SERVICE);
                this.rxCharacteristic = await this.service.getCharacteristic(NORDIC_UART_RX_CHAR);

                try {
                    this.txCharacteristic = await this.service.getCharacteristic(NORDIC_UART_TX_CHAR);
                } catch (e) {
                    console.warn('[BLE] TX characteristic not available (write-only mode)');
                }

                this.isConnected = true;
                this._notifyState('connected', { deviceName: this.deviceName });
                return true;
            } catch (err) {
                this.isConnected = false;
                this._notifyState('error', { error: err.message });
                throw err;
            }
        }

        async disconnect() {
            if (this.device && this.device.gatt && this.device.gatt.connected) {
                await this.device.gatt.disconnect();
            }
            this.isConnected = false;
            this._notifyState('disconnected');
        }

        async _writeChunk(bytes) {
            if (!this.isConnected || !this.rxCharacteristic) {
                throw new Error('No Open Pixel Poi connected via Bluetooth.');
            }

            const buffer = new Uint8Array(bytes);
            if (this.rxCharacteristic.writeValueWithoutResponse) {
                await this.rxCharacteristic.writeValueWithoutResponse(buffer);
            } else {
                await this.rxCharacteristic.writeValue(buffer);
            }
        }

        async _sendMessage(messageBytes) {
            const packet = [START_BYTE, ...messageBytes, END_BYTE];
            await this._writeChunk(packet);
        }

        /**
         * Set LED Brightness on Poi
         * @param {number} brightness 0 (off) to 255 (max)
         */
        async setBrightness(brightness) {
            const b = Math.max(0, Math.min(255, Math.round(brightness)));
            await this._sendMessage([COMM_CODES.CC_SET_BRIGHTNESS, b]);
        }

        /**
         * Set Animation Speed (Frames per second / Hz)
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
         * Change Active Pattern Slot (0-15)
         */
        async setPatternSlot(slotIndex) {
            const slot = Math.max(0, Math.min(15, parseInt(slotIndex) || 0));
            await this._sendMessage([COMM_CODES.CC_SET_PATTERN_SLOT, slot]);
        }

        /**
         * Auto-Loop All Patterns in current bank
         */
        async loopAllPatterns() {
            await this._sendMessage([COMM_CODES.CC_SET_PATTERN_ALL]);
        }

        /**
         * Change Active Bank (0-15)
         */
        async setBank(bankIndex) {
            const bank = Math.max(0, Math.min(15, parseInt(bankIndex) || 0));
            await this._sendMessage([COMM_CODES.CC_SET_BANK, bank]);
        }

        /**
         * Auto-Loop All Banks
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
                width,
                height,
                rgbData
            };
        }

        /**
         * Upload a Pattern Canvas directly to Open Pixel Poi over BLE
         * @param {HTMLCanvasElement} canvas The canvas to upload
         * @param {Function} progressCallback Optional callback (progress: 0.0 - 1.0, details)
         */
        async uploadPattern(canvas, progressCallback = null) {
            if (!this.isConnected) {
                throw new Error('Please connect your Open Pixel Poi via Bluetooth first.');
            }
            if (this.isUploading) {
                throw new Error('Another pattern upload is already in progress.');
            }

            const { width, height, rgbData } = this.canvasToPatternBytes(canvas);
            this.isUploading = true;

            try {
                // Construct complete packet
                // [0xD0, 0x04, height, width_high, width_low, ...rgbData, 0xD1]
                const totalLength = 1 + 1 + 1 + 2 + rgbData.length + 1;
                const fullPacket = new Uint8Array(totalLength);
                
                fullPacket[0] = START_BYTE;
                fullPacket[1] = COMM_CODES.CC_SET_PATTERN;
                fullPacket[2] = height & 0xFF;
                fullPacket[3] = (width >> 8) & 0xFF;
                fullPacket[4] = width & 0xFF;
                fullPacket.set(rgbData, 5);
                fullPacket[totalLength - 1] = END_BYTE;

                const chunkSize = this.maxPacketSize;
                const totalChunks = Math.ceil(fullPacket.length / chunkSize);

                console.log('[BLE Upload] Starting upload of ' + width + 'x' + height + ' pattern (' + fullPacket.length + ' bytes in ' + totalChunks + ' packets)...');

                for (let i = 0; i < totalChunks; i++) {
                    const start = i * chunkSize;
                    const end = Math.min(start + chunkSize, fullPacket.length);
                    const chunk = fullPacket.subarray(start, end);

                    await this._writeChunk(chunk);

                    const progress = (i + 1) / totalChunks;
                    if (progressCallback) {
                        progressCallback(progress, {
                            chunk: i + 1,
                            totalChunks: totalChunks,
                            sentBytes: end,
                            totalBytes: fullPacket.length
                        });
                    }

                    // Pacing delay (15ms) to give ESP32 Bluetooth stack and flash time to process
                    await new Promise(r => setTimeout(r, 15));
                }

                console.log('[BLE Upload] Pattern successfully uploaded and saved to Open Pixel Poi!');

                return true;
            } finally {
                this.isUploading = false;
            }
        }
    }

    // Export singleton instance
    global.OpenPixelPoiBLE = new OpenPixelPoiBLEClient();

})(typeof window !== 'undefined' ? window : this);
