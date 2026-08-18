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
const allFiles = [];

// 1. Standard patterns folder
const patternsDir = path.join(rootDir, 'patterns');
if (fs.existsSync(patternsDir)) {
    const list = fs.readdirSync(patternsDir);
    list.forEach(f => {
        if (f.toLowerCase().endsWith('.bmp')) {
            allFiles.push({
                fullPath: path.join(patternsDir, f),
                relName: f,
                folder: 'patterns',
                folderLabel: 'Standard Patterns'
            });
        }
    });
}

// 2. Highres folder
const highresDir = path.join(patternsDir, 'highres');
if (fs.existsSync(highresDir)) {
    const list = fs.readdirSync(highresDir);
    list.forEach(f => {
        if (f.toLowerCase().endsWith('.bmp')) {
            allFiles.push({
                fullPath: path.join(highresDir, f),
                relName: f,
                folder: 'highres',
                folderLabel: 'High-Res Patterns'
            });
        }
    });
}

console.log(`Discovered ${allFiles.length} total BMP files across patterns and patterns/highres.`);

const masterManifest = [];
let idCounter = 1;

allFiles.forEach(({ fullPath, relName, folder, folderLabel }) => {
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

    if (height > width) {
        try {
            buf = rotateBmp90Clockwise(buf);
            const tempW = width;
            width = height;
            height = tempW;
            wasRotated = true;
        } catch(e) {}
    }

    const base64 = buf.toString('base64');
    const dataUri = `data:image/bmp;base64,${base64}`;

    let cleanName = relName.replace(/\.bmp$/i, '').replace(/^\d+\s*[-_]?\s*/, '').trim();
    if (!cleanName) cleanName = relName.replace(/\.bmp$/i, '');
    cleanName = cleanName.charAt(0).toUpperCase() + cleanName.slice(1);

    masterManifest.push({
        id: `bmp_${idCounter++}`,
        rawFile: relName,
        name: cleanName,
        folder,
        folderLabel,
        width,
        height,
        wasRotated,
        dataUri
    });
});

const pCount = masterManifest.filter(m => m.folder === 'patterns').length;
const hCount = masterManifest.filter(m => m.folder === 'highres').length;
console.log(`Successfully compiled ${masterManifest.length} master patterns (${pCount} Standard, ${hCount} HighRes).`);

const jsContent = `/**
 * MASTER POV PATTERN DATABASE (${masterManifest.length} BMPs: ${pCount} Standard, ${hCount} HighRes)
 * Pre-rotated for horizontal timeline playback.
 */
window.MASTER_BMP_VAULT = ${JSON.stringify(masterManifest, null, 2)};
`;

fs.writeFileSync(path.join(rootDir, 'master-vault-manifest.js'), jsContent, 'utf8');
console.log('Saved master-vault-manifest.js successfully!');


