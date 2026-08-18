const fs = require('fs');
const path = require('path');

// Rotate a 24-bit/32-bit BMP buffer 90 degrees clockwise
function rotateBmp90Clockwise(buf) {
    if (buf.length < 54 || buf.toString('ascii', 0, 2) !== 'BM') return buf;

    const dataOffset = buf.readUInt32LE(10);
    const width = buf.readInt32LE(18);
    let height = buf.readInt32LE(22);
    const isTopDown = height < 0;
    height = Math.abs(height);
    const bpp = buf.readUInt16LE(28);

    if (bpp !== 24 && bpp !== 32) return buf; // only rotate 24/32-bit directly

    const bytesPerPixel = bpp / 8;
    const oldRowSize = Math.floor((bpp * width + 31) / 32) * 4;

    const newWidth = height;
    const newHeight = width;
    const newRowSize = Math.floor((bpp * newWidth + 31) / 32) * 4;
    const newPixelArraySize = newRowSize * newHeight;
    const newFileSize = 54 + newPixelArraySize;

    const outBuf = Buffer.alloc(newFileSize);
    buf.copy(outBuf, 0, 0, 54);

    outBuf.writeUInt32LE(newFileSize, 2);
    outBuf.writeUInt32LE(54, 10);
    outBuf.writeInt32LE(newWidth, 18);
    outBuf.writeInt32LE(newHeight, 22);
    outBuf.writeUInt32LE(newPixelArraySize, 34);

    // Read old pixel matrix (bottom-up BMP)
    // Old (x, y) with y=0 bottom, x=0 left
    // After 90 deg CW rotation: newX = y, newY = (width - 1 - x)
    for (let oldY = 0; oldY < height; oldY++) {
        const srcRowOffset = dataOffset + (isTopDown ? oldY : (height - 1 - oldY)) * oldRowSize;
        for (let oldX = 0; oldX < width; oldX++) {
            const srcPixelOffset = srcRowOffset + oldX * bytesPerPixel;
            
            // In rotated image:
            const newX = oldY;
            const newY = width - 1 - oldX;
            const destRowOffset = 54 + (newHeight - 1 - newY) * newRowSize;
            const destPixelOffset = destRowOffset + newX * bytesPerPixel;

            for (let b = 0; b < bytesPerPixel; b++) {
                outBuf[destPixelOffset + b] = buf[srcPixelOffset + b];
            }
        }
    }

    return outBuf;
}

const rootDir = __dirname;
const designCollDir = path.join(rootDir, 'design collection');
const poiboiDir = path.join(rootDir, 'poiboi');

const allFiles = [];

function scanDir(dir, categoryName = "") {
    if (!fs.existsSync(dir)) return;
    const entries = fs.readdirSync(dir, { withFileTypes: true });
    entries.forEach(e => {
        const fullPath = path.join(dir, e.name);
        if (e.isDirectory()) {
            const cat = categoryName ? `${categoryName} / ${e.name}` : e.name;
            scanDir(fullPath, cat);
        } else if (e.isFile() && e.name.toLowerCase().endsWith('.bmp')) {
            allFiles.push({ fullPath, relName: e.name, category: categoryName || "Uncategorized" });
        }
    });
}

scanDir(designCollDir);
scanDir(poiboiDir, "PoiBoi Original");

console.log(`Discovered ${allFiles.length} total BMP files across all folders.`);

const seenFiles = new Set();
const masterManifest = [];
let idCounter = 1;

allFiles.forEach(({ fullPath, relName, category }) => {
    // Deduplicate by clean category + filename
    const uniqueKey = `${category.toLowerCase()}_${relName.toLowerCase()}`;
    if (seenFiles.has(uniqueKey)) return;
    seenFiles.add(uniqueKey);

    let buf;
    try {
        buf = fs.readFileSync(fullPath);
    } catch(err) {
        return;
    }

    if (buf.length < 54 || buf.toString('ascii', 0, 2) !== 'BM') return;

    let width = buf.readInt32LE(18);
    let height = Math.abs(buf.readInt32LE(22));
    let wasRotated = false;

    // AUTO-ROTATE TALL IMAGES (Height > Width)
    // Tall images (e.g. 22x96 or 55x240) are portrait strips that represent a horizontal timeline when spun on poi.
    if (height > width) {
        try {
            buf = rotateBmp90Clockwise(buf);
            const tempW = width;
            width = height;
            height = tempW;
            wasRotated = true;
        } catch(e) {
            console.warn(`Rotation failed for ${relName}:`, e.message);
        }
    }

    const base64 = buf.toString('base64');
    const dataUri = `data:image/bmp;base64,${base64}`;

    let cleanName = relName.replace(/\.bmp$/i, '').replace(/^\d+\s*[-_]?\s*/, '').trim();
    if (!cleanName) cleanName = relName.replace(/\.bmp$/i, '');
    cleanName = cleanName.charAt(0).toUpperCase() + cleanName.slice(1);

    let cleanCategory = category
        .replace(/^\d+\s*/, '')
        .replace(/\//g, '➔')
        .replace(/➔ poiboi/gi, '')
        .trim();
    cleanCategory = cleanCategory.charAt(0).toUpperCase() + cleanCategory.slice(1);
    if (cleanCategory.includes('Poiboi')) cleanCategory = 'PoiBoi Collection';

    masterManifest.push({
        id: `bmp_${idCounter++}`,
        rawFile: relName,
        name: cleanName,
        category: cleanCategory,
        width,
        height,
        wasRotated,
        dataUri
    });
});

console.log(`Successfully compiled ${masterManifest.length} unique master BMP patterns into database.`);

const jsContent = `/**
 * MASTER POV PATTERN DATABASE (${masterManifest.length} Original & Curated BMPs)
 * Auto-rotated for non-squished horizontal POV timeline spinning.
 */
window.MASTER_BMP_VAULT = ${JSON.stringify(masterManifest, null, 2)};
`;

fs.writeFileSync(path.join(rootDir, 'master-vault-manifest.js'), jsContent, 'utf8');
console.log('Saved master-vault-manifest.js successfully!');

