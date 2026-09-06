/**
 * ============================================================================
 * OPEN POI STUDIO - VIRTUAL WIIMOTE & MOTION CONTROLLER (poi-wiimote.js)
 * ============================================================================
 * Transforms any smartphone (iOS / Android) into an authoritative Wii Nunchuk
 * motion controller using DeviceMotion and DeviceOrientation Web APIs!
 * 
 * 🎮 FEATURES:
 * - ⚡ Giant Ergonomic Z-Trigger: Hold to engage 24 Motion Flows, release for 0ms stealth blackout!
 * - 🌀 Barrel Roll Tilt Speed Throttle: Real-time gyro roll maps smoothly from 2x (chill drift) to 10x (torrent)
 * - 💥 Wrist Flick Acceleration Snapping: Instant jerk detection triggers reactive excitation bursts
 * - 🌟 C-Button: Tap to cycle 48 Pro Palettes, hold 1.0s to reset True RGB
 * - 🕹️ Virtual D-Pad / Thumbpad: Quick slot, bank, and effect navigation
 * - 🥷 Authentic Bridge Simulation: Sends Central Identity 0x01 for native hardware stealth
 * - 📳 Haptic Vibrations & 🔊 Web Audio Arcade Synthesizer
 * - 🖥️ Desktop Testing Simulator for non-mobile testing
 */

(function(window) {
    'use strict';

    // 24 Motion Flow Names
    const MOTION_NAMES = [
        "⚪ No Motion (Steady Pattern)",
        "⬆️ Flow UP (Geyser)",
        "⬇️ Flow DOWN",
        "🌧️ Matrix Rain",
        "🌊 Tidal Wave",
        "⚡ Plasma Blast",
        "💫 Stardust Fall",
        "💓 Chroma Pulse",
        "🔄 POV Spin CW",
        "🌀 3D Spiral Helix",
        "⚡ Strobe Blade",
        "🌌 Warp Pulse",
        "⚡ Glitch Spark",
        "🌈 Aurora Wave",
        "🌋 Lava Flare",
        "🪩 Disco Strobe",
        "🪐 Black Hole",
        "💡 Fireflies Swarm",
        "🌀 Plasma Vortex",
        "✨ Cosmic Spiral",
        "📡 Sonar Radar Ping",
        "🧊 Tesseract Rhythm",
        "🪼 Jellyfish Pulse",
        "⛲ Rainbow Fountain",
        "⚡ Tachyon Stream"
    ];

    // 48 Pro Palette Names
    const PALETTE_NAMES = [
        "⚪ True RGB (Original)",
        "🌈 Rainbow", "🌡️ FLIR Heat", "🪩 Prismatic Opal", "🧊 Cyberpunk 2077",
        "🌆 Synthwave", "🌋 Molten Magma", "💻 Matrix Phosphor", "❄️ Glacial Ice",
        "🧪 Toxic Acid", "🍬 Cotton Candy", "👑 Royal Gold", "🌸 Sakura Drift",
        "🩸 Blood Moon", "🌿 Emerald Crystal", "☀️ Solar Flare", "🩵 Turquoise",
        "💜 Mystic Amethyst", "🫐 Blueberry Indigo", "🍊 Tangerine Dream", "💎 Diamond Strobe",
        "🦚 Peacock Feather", "⚡ Hyper Neon Triad", "🏜️ Desert Mirage", "🖤 Blacklight UV",
        "🍉 Watermelon Wave", "🌌 Deep Nebula", "🍋 Lemon-Lime", "🏮 Tokyo Neon",
        "🌊 Biolum Abyss", "🎃 Witch Fire", "🎆 Fourth of July", "🪙 Bronze & Copper",
        "🌆 Vaporwave Dream", "🌲 Electric Forest", "🪐 Saturn Rings", "🧪 Acid Cyber Rain",
        "🪼 Biolum Jellyfish", "🌺 Sunset Hibiscus", "🌌 Glacial Aurora", "🔮 Arcane Quartz",
        "🍧 Pastel Sorbet", "🖤 Obsidian Ember", "🏝️ Caribbean Lagoon", "🪩 Cyber Disco Glitz",
        "🍒 Black Cherry", "🌕 Harvest Moon", "💎 Opal Prism", "⚡ High-Voltage Arc"
    ];

    // Internal State
    const state = {
        isZHeld: false,
        activeMotion: 4,      // Default: Tidal Wave
        activePalette: 1,     // Default: Rainbow
        activeSlot: 0,        // 0-9
        activeBank: 0,        // 0-4
        activeSpeed: 3,       // 1-10
        preZPalette: 1,

        // Motion & Sensors
        sensorsActive: false,
        hasOrientationEvent: false,
        tareRoll: 0,
        currentRoll: 0,
        currentSpeed: 3,
        currentJerk: 0,
        flickThreshold: 18.0, // m/s^2 total jerk
        lastFlickTime: 0,
        lastSpeedSent: 3,
        lastSpeedTime: 0,
        prevAccel: { x: 0, y: 0, z: 0 },

        // User Preferences
        stealthMode: true,     // Simulates Master Bridge 0x01
        hapticsEnabled: true,
        soundEnabled: true,

        // Audio & Timers
        audioCtx: null,
        cTimer: null,
        heartbeatTimer: null,
        touchStartX: 0,
        touchStartY: 0
    };

    // --- 🔊 SYNTHESIZED ARCADE WEB AUDIO FX ---
    function playSound(type) {
        if (!state.soundEnabled) return;
        try {
            if (!state.audioCtx) {
                state.audioCtx = new (window.AudioContext || window.webkitAudioContext)();
            }
            if (state.audioCtx.state === 'suspended') {
                state.audioCtx.resume();
            }
            const now = state.audioCtx.currentTime;
            const osc = state.audioCtx.createOscillator();
            const gain = state.audioCtx.createGain();
            osc.connect(gain);
            gain.connect(state.audioCtx.destination);

            if (type === 'z-engage') {
                osc.type = 'sawtooth';
                osc.frequency.setValueAtTime(140, now);
                osc.frequency.exponentialRampToValueAtTime(360, now + 0.09);
                gain.gain.setValueAtTime(0.2, now);
                gain.gain.exponentialRampToValueAtTime(0.01, now + 0.09);
                osc.start(now);
                osc.stop(now + 0.09);
            } else if (type === 'z-release') {
                osc.type = 'sine';
                osc.frequency.setValueAtTime(300, now);
                osc.frequency.exponentialRampToValueAtTime(120, now + 0.08);
                gain.gain.setValueAtTime(0.18, now);
                gain.gain.exponentialRampToValueAtTime(0.01, now + 0.08);
                osc.start(now);
                osc.stop(now + 0.08);
            } else if (type === 'flick') {
                osc.type = 'triangle';
                osc.frequency.setValueAtTime(420, now);
                osc.frequency.exponentialRampToValueAtTime(1400, now + 0.05);
                osc.frequency.exponentialRampToValueAtTime(180, now + 0.22);
                gain.gain.setValueAtTime(0.35, now);
                gain.gain.exponentialRampToValueAtTime(0.01, now + 0.22);
                osc.start(now);
                osc.stop(now + 0.22);
            } else if (type === 'c-tap') {
                osc.type = 'sine';
                osc.frequency.setValueAtTime(540, now);
                osc.frequency.exponentialRampToValueAtTime(780, now + 0.04);
                gain.gain.setValueAtTime(0.12, now);
                gain.gain.exponentialRampToValueAtTime(0.01, now + 0.04);
                osc.start(now);
                osc.stop(now + 0.04);
            } else if (type === 'c-reset') {
                osc.type = 'sawtooth';
                osc.frequency.setValueAtTime(800, now);
                osc.frequency.exponentialRampToValueAtTime(220, now + 0.16);
                gain.gain.setValueAtTime(0.25, now);
                gain.gain.exponentialRampToValueAtTime(0.01, now + 0.16);
                osc.start(now);
                osc.stop(now + 0.16);
            } else if (type === 'step') {
                osc.type = 'sine';
                osc.frequency.setValueAtTime(400, now);
                gain.gain.setValueAtTime(0.08, now);
                gain.gain.exponentialRampToValueAtTime(0.01, now + 0.03);
                osc.start(now);
                osc.stop(now + 0.03);
            }
        } catch (e) {
            // Audio blocked until user gesture
        }
    }

    // --- 📳 HAPTIC FEEDBACK ---
    function vibrate(pattern) {
        if (state.hapticsEnabled && typeof navigator.vibrate === 'function') {
            try { navigator.vibrate(pattern); } catch (e) {}
        }
    }

    // --- 📡 SENSOR PERMISSION & LIFECYCLE ---
    async function requestSensorPermission() {
        if (typeof DeviceMotionEvent !== 'undefined' && typeof DeviceMotionEvent.requestPermission === 'function') {
            try {
                const res = await DeviceMotionEvent.requestPermission();
                if (res === 'granted') {
                    startMotionListeners();
                    updateSensorBadge(true, "iOS Motion Sensors Active (60 Hz)");
                    return true;
                } else {
                    updateSensorBadge(false, "Motion Permission Denied");
                    return false;
                }
            } catch (err) {
                console.warn('[Wiimote] Sensor permission prompt failed:', err);
                updateSensorBadge(false, "Permission Error: " + err.message);
                return false;
            }
        } else {
            // Android, Desktop, or Standard Browser
            startMotionListeners();
            updateSensorBadge(true, "Sensors Connected (60 Hz)");
            return true;
        }
    }

    function startMotionListeners() {
        if (state.sensorsActive) return;
        state.sensorsActive = true;
        window.addEventListener('devicemotion', onDeviceMotion, { passive: true });
        window.addEventListener('deviceorientation', onDeviceOrientation, { passive: true });
        updateSensorBadge(true, "Sensors Active & Calibrated");
    }

    function stopMotionListeners() {
        if (!state.sensorsActive) return;
        state.sensorsActive = false;
        window.removeEventListener('devicemotion', onDeviceMotion);
        window.removeEventListener('deviceorientation', onDeviceOrientation);
        updateSensorBadge(false, "Sensors Standby");
    }

    // --- 📐 MOTION & ORIENTATION PROCESSING ---
    function onDeviceMotion(e) {
        const acc = e.accelerationIncludingGravity || e.acceleration;
        if (!acc) return;

        const ax = acc.x || 0;
        const ay = acc.y || 0;
        const az = acc.z || 0;

        // Calculate instantaneous Jerk (rate of change of acceleration)
        const dx = Math.abs(ax - state.prevAccel.x);
        const dy = Math.abs(ay - state.prevAccel.y);
        const dz = Math.abs(az - state.prevAccel.z);
        state.currentJerk = dx + dy + dz;
        state.prevAccel = { x: ax, y: ay, z: az };

        updateJerkUI(state.currentJerk);

        // 💥 WRIST FLICK EXCITATION: Sharp snap > threshold while Z is held
        const now = Date.now();
        if (state.isZHeld && state.currentJerk > state.flickThreshold && (now - state.lastFlickTime > 380)) {
            state.lastFlickTime = now;
            triggerWristFlick();
        }

        // If deviceorientation is not supported, derive roll angle from gravity on X axis
        if (!state.hasOrientationEvent) {
            const derivedRoll = Math.max(-90, Math.min(90, (ax / 9.8) * 90));
            processRoll(derivedRoll);
        }
    }

    function onDeviceOrientation(e) {
        if (e.gamma !== null && e.gamma !== undefined) {
            state.hasOrientationEvent = true;
            processRoll(e.gamma);
        }
    }

    function processRoll(rawRoll) {
        state.currentRoll = rawRoll;
        const effectiveRoll = rawRoll - state.tareRoll;
        const absDev = Math.abs(effectiveRoll);

        // Map tilt deviation: Deadzone 0-8° = speed 2; >= 45° = speed 10
        let targetSpeed = 2;
        if (absDev > 8) {
            const factor = Math.min(1.0, (absDev - 8) / 37);
            targetSpeed = 2 + Math.round(factor * 8); // 2 to 10
        }
        state.currentSpeed = targetSpeed;

        updateRollUI(effectiveRoll, targetSpeed);

        // Transmit throttled speed to prop when Z is held
        const now = Date.now();
        if (state.isZHeld && targetSpeed !== state.lastSpeedSent && (now - state.lastSpeedTime > 55)) {
            state.lastSpeedSent = targetSpeed;
            state.lastSpeedTime = now;
            if (window.OpenPixelPoiBLE && window.OpenPixelPoiBLE.isConnected && typeof window.OpenPixelPoiBLE.setPaletteSpeed === 'function') {
                window.OpenPixelPoiBLE.setPaletteSpeed(targetSpeed);
            }
        }
    }

    // --- 💥 TRIGGER WRIST FLICK ---
    async function triggerWristFlick() {
        console.log(`💥 [Wiimote] WRIST FLICK DETECTED! Jerk: ${state.currentJerk.toFixed(1)} m/s²`);
        vibrate([50, 40, 90]);
        playSound('flick');
        flashFlickUI();

        if (window.OpenPixelPoiBLE && window.OpenPixelPoiBLE.isConnected && typeof window.OpenPixelPoiBLE.triggerFlick === 'function') {
            await window.OpenPixelPoiBLE.triggerFlick();
        }
    }

    // --- ⚡ Z-TRIGGER GESTURE ENGINE ---
    async function engageZ() {
        if (state.isZHeld) return;
        state.isZHeld = true;
        state.preZPalette = state.activePalette;

        vibrate(35);
        playSound('z-engage');
        updateZButtonUI(true);

        if (window.OpenPixelPoiBLE && window.OpenPixelPoiBLE.isConnected) {
            // Activate Motion Flow
            if (typeof window.OpenPixelPoiBLE.setMotionFX === 'function') {
                await window.OpenPixelPoiBLE.setMotionFX(state.activeMotion);
            }
            // Apply current tilt speed
            if (typeof window.OpenPixelPoiBLE.setPaletteSpeed === 'function') {
                await window.OpenPixelPoiBLE.setPaletteSpeed(state.currentSpeed);
            }
        }
    }

    async function releaseZ() {
        if (!state.isZHeld) return;
        state.isZHeld = false;

        vibrate(25);
        playSound('z-release');
        updateZButtonUI(false);

        if (window.OpenPixelPoiBLE && window.OpenPixelPoiBLE.isConnected) {
            // Motion FX 0: In Bridge Mode, this hardware-blanks the prop to 0mA pitch black!
            // In Web App Mode, this cleanly restores steady pattern playback!
            if (typeof window.OpenPixelPoiBLE.setMotionFX === 'function') {
                await window.OpenPixelPoiBLE.setMotionFX(0);
            }
            if (typeof window.OpenPixelPoiBLE.setPaletteSpeed === 'function') {
                await window.OpenPixelPoiBLE.setPaletteSpeed(5);
            }
        }
    }

    // --- 🌟 C-BUTTON ENGINE ---
    function onCDown() {
        vibrate(15);
        state.cTimer = setTimeout(async () => {
            // Long Press (1.0s) -> Reset True RGB
            state.activePalette = 0;
            vibrate([40, 40, 60]);
            playSound('c-reset');
            updatePaletteUI();
            if (window.OpenPixelPoiBLE && window.OpenPixelPoiBLE.isConnected && typeof window.OpenPixelPoiBLE.setPaletteFX === 'function') {
                await window.OpenPixelPoiBLE.setPaletteFX(0);
            }
            if (typeof window.showToast === 'function') {
                window.showToast("🌟 Reset Palette to True RGB!", "var(--neon-cyan)");
            }
            state.cTimer = null;
        }, 900);
    }

    async function onCUp() {
        if (state.cTimer) {
            clearTimeout(state.cTimer);
            state.cTimer = null;
            // Short Tap -> Advance to next of 48 Pro Palettes
            state.activePalette = (state.activePalette % 48) + 1;
            vibrate(20);
            playSound('c-tap');
            updatePaletteUI();
            if (window.OpenPixelPoiBLE && window.OpenPixelPoiBLE.isConnected && typeof window.OpenPixelPoiBLE.setPaletteFX === 'function') {
                await window.OpenPixelPoiBLE.setPaletteFX(state.activePalette);
            }
        }
    }

    // --- 🕹️ DIRECTIONAL D-PAD / THUMBPAD ---
    async function stepDirection(dir) {
        playSound('step');
        vibrate(20);

        if (dir === 'left') {
            if (state.isZHeld) {
                // Previous Motion Flow (1 to 24)
                state.activeMotion = (state.activeMotion > 1) ? (state.activeMotion - 1) : 24;
                updateMotionUI();
                if (window.OpenPixelPoiBLE && window.OpenPixelPoiBLE.isConnected) {
                    await window.OpenPixelPoiBLE.setMotionFX(state.activeMotion);
                }
            } else {
                // Previous Pattern Slot (0 to 9)
                state.activeSlot = (state.activeSlot > 0) ? (state.activeSlot - 1) : 9;
                updateSlotUI();
                if (window.OpenPixelPoiBLE && window.OpenPixelPoiBLE.isConnected) {
                    await window.OpenPixelPoiBLE.setPatternSlot(state.activeSlot);
                }
            }
        } else if (dir === 'right') {
            if (state.isZHeld) {
                // Next Motion Flow (1 to 24)
                state.activeMotion = (state.activeMotion % 24) + 1;
                updateMotionUI();
                if (window.OpenPixelPoiBLE && window.OpenPixelPoiBLE.isConnected) {
                    await window.OpenPixelPoiBLE.setMotionFX(state.activeMotion);
                }
            } else {
                // Next Pattern Slot (0 to 9)
                state.activeSlot = (state.activeSlot + 1) % 10;
                updateSlotUI();
                if (window.OpenPixelPoiBLE && window.OpenPixelPoiBLE.isConnected) {
                    await window.OpenPixelPoiBLE.setPatternSlot(state.activeSlot);
                }
            }
        } else if (dir === 'up') {
            if (state.isZHeld) {
                // Speed Step Up (1 to 10)
                state.activeSpeed = Math.min(10, state.activeSpeed + 1);
                updateSpeedUI();
                if (window.OpenPixelPoiBLE && window.OpenPixelPoiBLE.isConnected) {
                    await window.OpenPixelPoiBLE.setPaletteSpeed(state.activeSpeed);
                }
            } else {
                // Next Bank (0 to 4)
                state.activeBank = (state.activeBank + 1) % 5;
                updateBankUI();
                if (window.OpenPixelPoiBLE && window.OpenPixelPoiBLE.isConnected) {
                    await window.OpenPixelPoiBLE.setBank(state.activeBank);
                }
            }
        } else if (dir === 'down') {
            if (state.isZHeld) {
                // Speed Step Down (1 to 10)
                state.activeSpeed = Math.max(1, state.activeSpeed - 1);
                updateSpeedUI();
                if (window.OpenPixelPoiBLE && window.OpenPixelPoiBLE.isConnected) {
                    await window.OpenPixelPoiBLE.setPaletteSpeed(state.activeSpeed);
                }
            } else {
                // Previous Bank (0 to 4)
                state.activeBank = (state.activeBank > 0) ? (state.activeBank - 1) : 4;
                updateBankUI();
                if (window.OpenPixelPoiBLE && window.OpenPixelPoiBLE.isConnected) {
                    await window.OpenPixelPoiBLE.setBank(state.activeBank);
                }
            }
        }
    }

    // --- 🖥️ UI TELEMETRY UPDATERS ---
    function updateSensorBadge(active, text) {
        const badge = document.getElementById('wm-sensor-badge');
        if (badge) {
            badge.innerText = `● ${text}`;
            badge.style.color = active ? 'var(--neon-green)' : 'var(--text-muted)';
            badge.style.borderColor = active ? 'rgba(0, 255, 136, 0.4)' : 'rgba(255, 255, 255, 0.1)';
        }
        const btnReq = document.getElementById('btn-wm-req-sensors');
        if (btnReq) {
            btnReq.style.display = active ? 'none' : 'inline-flex';
        }
    }

    function updateRollUI(roll, speed) {
        const degEl = document.getElementById('wm-roll-degrees');
        if (degEl) degEl.innerText = `${roll > 0 ? '+' : ''}${Math.round(roll)}°`;

        const spdEl = document.getElementById('wm-roll-speed');
        if (spdEl) spdEl.innerText = `${speed}x (${speed <= 3 ? 'Chill Drift' : (speed <= 7 ? 'Flowing River' : 'Rushing Torrent')})`;

        const bubble = document.getElementById('wm-spirit-bubble');
        if (bubble) {
            // Clamp roll visually between -45 and +45 for smooth UI movement
            const pct = Math.max(-45, Math.min(45, roll)) / 45; // -1 to 1
            const offsetPx = pct * 90; // Move up to 90px left or right
            bubble.style.transform = `translateX(${offsetPx}px)`;
        }

        const bar = document.getElementById('wm-speed-bar-fill');
        if (bar) {
            const fillPct = ((speed - 1) / 9) * 100;
            bar.style.width = `${fillPct}%`;
        }
    }

    function updateJerkUI(jerk) {
        const valEl = document.getElementById('wm-jerk-val');
        if (valEl) valEl.innerText = `${jerk.toFixed(1)} m/s²`;

        const bar = document.getElementById('wm-jerk-bar-fill');
        if (bar) {
            const pct = Math.min(100, (jerk / 35.0) * 100);
            bar.style.width = `${pct}%`;
            bar.style.background = (jerk >= state.flickThreshold) 
                ? 'linear-gradient(90deg, #ff0055, #ff00ea)' 
                : 'linear-gradient(90deg, var(--neon-cyan), var(--neon-green))';
        }
    }

    function flashFlickUI() {
        const wrap = document.getElementById('workspace-wiimote');
        if (wrap) {
            wrap.classList.add('wiimote-flick-flash');
            setTimeout(() => wrap.classList.remove('wiimote-flick-flash'), 280);
        }
        const burst = document.getElementById('wm-flick-alert');
        if (burst) {
            burst.style.opacity = '1';
            burst.style.transform = 'scale(1.08)';
            setTimeout(() => {
                burst.style.opacity = '0';
                burst.style.transform = 'scale(1)';
            }, 320);
        }
    }

    function updateZButtonUI(isHeld) {
        const btnZ = document.getElementById('btn-wm-z-trigger');
        const statePill = document.getElementById('wm-status-pill');
        if (btnZ) {
            btnZ.classList.toggle('z-trigger-active', isHeld);
        }
        if (statePill) {
            if (isHeld) {
                statePill.innerText = "⚡ Z ENGAGED • FLOW & FLICK ACTIVE";
                statePill.style.color = "var(--neon-green)";
                statePill.style.borderColor = "var(--neon-green)";
                statePill.style.background = "rgba(0, 255, 136, 0.15)";
            } else {
                statePill.innerText = state.stealthMode ? "🥷 STEALTH STANDBY (PITCH BLACK)" : "⚪ STEADY PATTERN PLAYBACK";
                statePill.style.color = state.stealthMode ? "var(--neon-cyan)" : "var(--text-muted)";
                statePill.style.borderColor = "rgba(255, 255, 255, 0.12)";
                statePill.style.background = "rgba(0, 0, 0, 0.4)";
            }
        }
    }

    function updateMotionUI() {
        const elName = document.getElementById('wm-motion-name');
        if (elName) elName.innerText = MOTION_NAMES[state.activeMotion] || `Effect ${state.activeMotion}`;
    }

    function updatePaletteUI() {
        const elName = document.getElementById('wm-palette-name');
        if (elName) elName.innerText = PALETTE_NAMES[state.activePalette] || `Palette ${state.activePalette}`;
    }

    function updateSlotUI() {
        const elSlot = document.getElementById('wm-slot-val');
        if (elSlot) elSlot.innerText = `Slot ${state.activeSlot + 1}`;
    }

    function updateBankUI() {
        const elBank = document.getElementById('wm-bank-val');
        if (elBank) elBank.innerText = `Bank ${state.activeBank + 1}`;
    }

    function updateSpeedUI() {
        const elSpeed = document.getElementById('wm-roll-speed');
        if (elSpeed) elSpeed.innerText = `${state.activeSpeed}x`;
    }

    // --- 🔄 WORKSPACE LIFECYCLE HOOKS ---
    function initWiimoteMode() {
        console.log('[Wiimote] Entering Virtual Wiimote Workspace');
        startMotionListeners();

        // If stealth mode is active, announce Bridge Source Identity 0x01
        if (state.stealthMode && window.OpenPixelPoiBLE && window.OpenPixelPoiBLE.isConnected) {
            if (typeof window.OpenPixelPoiBLE.setSourceIdentity === 'function') {
                window.OpenPixelPoiBLE.setSourceIdentity(1);
            }
            // Start 4-second heartbeat to keep Bridge mode alive
            if (!state.heartbeatTimer) {
                state.heartbeatTimer = setInterval(() => {
                    if (window.OpenPixelPoiBLE && window.OpenPixelPoiBLE.isConnected && typeof window.OpenPixelPoiBLE.setSourceIdentity === 'function') {
                        window.OpenPixelPoiBLE.setSourceIdentity(1);
                    }
                }, 4000);
            }
        }

        updateMotionUI();
        updatePaletteUI();
        updateSlotUI();
        updateBankUI();
        updateZButtonUI(false);
    }

    function exitWiimoteMode() {
        console.log('[Wiimote] Exiting Virtual Wiimote Workspace');
        if (state.isZHeld) releaseZ();

        if (state.heartbeatTimer) {
            clearInterval(state.heartbeatTimer);
            state.heartbeatTimer = null;
        }

        // Restore normal Web App studio identity (0x02) so prop illuminates normally in other tabs
        if (window.OpenPixelPoiBLE && window.OpenPixelPoiBLE.isConnected && typeof window.OpenPixelPoiBLE.setSourceIdentity === 'function') {
            window.OpenPixelPoiBLE.setSourceIdentity(2);
        }
    }

    // --- 🎮 DOM EVENT ATTACHMENT ---
    function setupDOMBindings() {
        // Sensor Request Button
        const btnReq = document.getElementById('btn-wm-req-sensors');
        if (btnReq) btnReq.onclick = () => requestSensorPermission();

        // Tare / Zero Horizon Button
        const btnTare = document.getElementById('btn-wm-tare');
        if (btnTare) {
            btnTare.onclick = () => {
                state.tareRoll = state.currentRoll;
                vibrate(25);
                playSound('step');
                if (typeof window.showToast === 'function') {
                    window.showToast("🎯 Spirit Level Zeroed to Current Hold Angle!", "var(--neon-green)");
                }
            };
        }

        // Stealth Mode Checkbox
        const chkStealth = document.getElementById('chk-wm-stealth');
        if (chkStealth) {
            chkStealth.checked = state.stealthMode;
            chkStealth.onchange = (e) => {
                state.stealthMode = e.target.checked;
                if (window.OpenPixelPoiBLE && window.OpenPixelPoiBLE.isConnected && typeof window.OpenPixelPoiBLE.setSourceIdentity === 'function') {
                    window.OpenPixelPoiBLE.setSourceIdentity(state.stealthMode ? 1 : 2);
                }
                updateZButtonUI(state.isZHeld);
            };
        }

        // Haptics & Audio Toggles
        const chkHaptics = document.getElementById('chk-wm-haptics');
        if (chkHaptics) {
            chkHaptics.checked = state.hapticsEnabled;
            chkHaptics.onchange = (e) => state.hapticsEnabled = e.target.checked;
        }
        const chkSound = document.getElementById('chk-wm-sound');
        if (chkSound) {
            chkSound.checked = state.soundEnabled;
            chkSound.onchange = (e) => state.soundEnabled = e.target.checked;
        }

        // Z-TRIGGER BUTTON
        const btnZ = document.getElementById('btn-wm-z-trigger');
        if (btnZ) {
            btnZ.addEventListener('pointerdown', (e) => {
                e.preventDefault();
                try { btnZ.setPointerCapture(e.pointerId); } catch(err) {}
                engageZ();
            });
            btnZ.addEventListener('pointerup', (e) => {
                e.preventDefault();
                try { btnZ.releasePointerCapture(e.pointerId); } catch(err) {}
                releaseZ();
            });
            btnZ.addEventListener('pointercancel', (e) => {
                e.preventDefault();
                releaseZ();
            });
        }

        // C-BUTTON
        const btnC = document.getElementById('btn-wm-c-btn');
        if (btnC) {
            btnC.addEventListener('pointerdown', (e) => {
                e.preventDefault();
                try { btnC.setPointerCapture(e.pointerId); } catch(err) {}
                onCDown();
            });
            btnC.addEventListener('pointerup', (e) => {
                e.preventDefault();
                try { btnC.releasePointerCapture(e.pointerId); } catch(err) {}
                onCUp();
            });
            btnC.addEventListener('pointercancel', (e) => {
                e.preventDefault();
                if (state.cTimer) clearTimeout(state.cTimer);
            });
        }

        // D-PAD BUTTONS
        const btnUp = document.getElementById('btn-wm-dpad-up');
        if (btnUp) btnUp.onclick = () => stepDirection('up');
        const btnDown = document.getElementById('btn-wm-dpad-down');
        if (btnDown) btnDown.onclick = () => stepDirection('down');
        const btnLeft = document.getElementById('btn-wm-dpad-left');
        if (btnLeft) btnLeft.onclick = () => stepDirection('left');
        const btnRight = document.getElementById('btn-wm-dpad-right');
        if (btnRight) btnRight.onclick = () => stepDirection('right');

        // D-PAD TOUCHPAD SWIPE LISTENER
        const dpadArea = document.getElementById('wm-dpad-surface');
        if (dpadArea) {
            dpadArea.addEventListener('touchstart', (e) => {
                if (e.touches.length > 0) {
                    state.touchStartX = e.touches[0].clientX;
                    state.touchStartY = e.touches[0].clientY;
                }
            }, { passive: true });

            dpadArea.addEventListener('touchend', (e) => {
                if (e.changedTouches.length > 0) {
                    const dx = e.changedTouches[0].clientX - state.touchStartX;
                    const dy = e.changedTouches[0].clientY - state.touchStartY;
                    if (Math.abs(dx) > Math.abs(dy)) {
                        if (dx > 30) stepDirection('right');
                        else if (dx < -30) stepDirection('left');
                    } else {
                        if (dy > 30) stepDirection('down');
                        else if (dy < -30) stepDirection('up');
                    }
                }
            }, { passive: true });
        }

        // DESKTOP SIMULATOR CONTROLS
        const simRoll = document.getElementById('sim-wm-roll');
        if (simRoll) {
            simRoll.oninput = (e) => {
                const val = parseFloat(e.target.value) || 0;
                processRoll(val);
            };
        }
        const btnSimFlick = document.getElementById('btn-sim-wm-flick');
        if (btnSimFlick) {
            btnSimFlick.onclick = () => triggerWristFlick();
        }

        // KEYBOARD SHORTCUTS FOR ACCESSIBILITY / DESKTOP TESTING
        window.addEventListener('keydown', (e) => {
            const w = document.getElementById('workspace-wiimote');
            if (!w || w.style.display === 'none') return;

            if (e.code === 'Space' || e.key === 'z' || e.key === 'Z') {
                if (!e.repeat) engageZ();
            } else if (e.key === 'c' || e.key === 'C') {
                if (!e.repeat) onCDown();
            } else if (e.key === 'ArrowLeft') {
                stepDirection('left');
            } else if (e.key === 'ArrowRight') {
                stepDirection('right');
            } else if (e.key === 'ArrowUp') {
                stepDirection('up');
            } else if (e.key === 'ArrowDown') {
                stepDirection('down');
            }
        });

        window.addEventListener('keyup', (e) => {
            const w = document.getElementById('workspace-wiimote');
            if (!w || w.style.display === 'none') return;

            if (e.code === 'Space' || e.key === 'z' || e.key === 'Z') {
                releaseZ();
            } else if (e.key === 'c' || e.key === 'C') {
                onCUp();
            }
        });
    }

    // Auto-initialize when DOM is ready
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', setupDOMBindings);
    } else {
        setupDOMBindings();
    }

    // Public API
    window.OpenPixelPoiWiimote = {
        state: state,
        init: initWiimoteMode,
        exit: exitWiimoteMode,
        requestSensorPermission: requestSensorPermission,
        engageZ: engageZ,
        releaseZ: releaseZ,
        triggerFlick: triggerWristFlick,
        stepDirection: stepDirection
    };

    window.initWiimoteMode = initWiimoteMode;
    window.exitWiimoteMode = exitWiimoteMode;

})(window);
