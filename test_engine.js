// Test suite for POI Studio Engine v10.0 (Optical Illusions & Geometry Morphing Engine)
const fs = require('fs');
const path = require('path');

global.window = global;
const code = fs.readFileSync(path.join(__dirname, 'poi-engine.js'), 'utf8');
eval(code);

console.log("=== Testing POI Studio Engine v10.0 (Optical Illusions & Geometry Morphing Engine) ===");

let totalPatterns = 0;
let missing = [];

window.POI_PATTERN_DB.forEach(group => {
    group.items.forEach(item => {
        totalPatterns++;
        if (typeof window.POI_GENERATORS[item.v] !== 'function') {
            missing.push(item.v);
        }
    });
});

console.log(`Total Algorithmic Patterns in DB: ${totalPatterns}`);
if (missing.length > 0) {
    console.error(`Missing generators for: ${missing.join(', ')}`);
    process.exit(1);
} else {
    console.log("✓ All 118+ pure mathematical and optical illusion generator functions exist!");
}

function createMockContext(w, h) {
    return {
        width: w, height: h,
        strokeStyle: '#ffffff', fillStyle: '#ffffff', lineWidth: 1,
        setLineDash: () => {}, beginPath: () => {}, moveTo: () => {}, lineTo: () => {},
        arc: () => {}, ellipse: () => {}, bezierCurveTo: () => {}, quadraticCurveTo: () => {},
        strokeRect: () => {}, fillRect: () => {}, stroke: () => {}, fill: () => {},
        closePath: () => {}, save: () => {}, restore: () => {}, translate: () => {},
        rotate: () => {}, scale: () => {}, transform: () => {}, clearRect: () => {},
        createImageData: (width, height) => ({ width, height, data: new Uint8ClampedArray(width * height * 4) }),
        getImageData: (x, y, width, height) => ({ width, height, data: new Uint8ClampedArray(width * height * 4) }),
        putImageData: () => {}, drawImage: () => {}
    };
}

const w = 96, h = 55;
const mockCtx = createMockContext(w, h);
const sampler = window.createColorSampler('laser_cyan_pink', 'h', 'cyclic');
const params = { chaos: 0.5, scale: 1.0, thickness: 2.5, spacing: 1.8 };

let tested = 0;
for (const [key, genFn] of Object.entries(window.POI_GENERATORS)) {
    try {
        genFn(mockCtx, w, h, params, sampler);
        tested++;
    } catch (e) {
        console.error(`Error in generator ${key}:`, e);
        process.exit(1);
    }
}
console.log(`✓ Tested all ${tested} mathematical & optical illusion generators successfully!`);

console.log("ALL TESTS PASSED! ✨");
