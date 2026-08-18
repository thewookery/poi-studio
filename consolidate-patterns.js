const fs = require('fs');
const path = require('path');

const rootDir = __dirname;
const targetDir = path.join(rootDir, 'patterns');

if (!fs.existsSync(targetDir)) {
    fs.mkdirSync(targetDir, { recursive: true });
}

// Rotate a 24-bit/32-bit BMP buffer 90 degrees clockwise
function rotateBmp90Clockwise(buf) {
    if (buf.length < 54 || buf.toString('ascii', 0, 2) !== 'BM') return buf;

    const dataOffset = buf.readUInt32LE(10);
    const width = buf.readInt32LE(18);
    let height = buf.readInt32LE(22);
    const isTopDown = height < 0;
    height = Math.abs(height);
    const bpp = buf.readUInt16LE(28);

    if (bpp !== 24 && bpp !== 32) return buf;

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

    for (let oldY = 0; oldY < height; oldY++) {
        const srcRowOffset = dataOffset + (isTopDown ? oldY : (height - 1 - oldY)) * oldRowSize;
        for (let oldX = 0; oldX < width; oldX++) {
            const srcPixelOffset = srcRowOffset + oldX * bytesPerPixel;
            
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

const allFound = [];

function scanDir(dir) {
    if (!fs.existsSync(dir)) return;
    const entries = fs.readdirSync(dir, { withFileTypes: true });
    entries.forEach(e => {
        const fullPath = path.join(dir, e.name);
        if (e.isDirectory()) {
            if (e.name !== 'patterns' && e.name !== '.git' && e.name !== 'node_modules') {
                scanDir(fullPath);
            }
        } else if (e.isFile() && e.name.toLowerCase().endsWith('.bmp')) {
            allFound.push({ fullPath, filename: e.name });
        }
    });
}

scanDir(rootDir);
console.log(`Found ${allFound.length} raw BMP files across workspace.`);

const seenNames = new Set();
let copiedCount = 0;
const masterManifest = [];
let idCounter = 1;

allFound.forEach(({ fullPath, filename }) => {
    let baseName = filename.replace(/\.bmp$/i, '').trim();
    let cleanFileName = `${baseName}.bmp`;
    
    // De-duplicate
    let lower = cleanFileName.toLowerCase();
    if (seenNames.has(lower)) return;
    seenNames.add(lower);

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

    // Auto-rotate tall portrait images (Height > Width)
    if (height > width) {
        try {
            buf = rotateBmp90Clockwise(buf);
            const tempW = width;
            width = height;
            height = tempW;
            wasRotated = true;
        } catch(e) {
            console.warn(`Rotation failed for ${filename}:`, e.message);
        }
    }

    const destFile = path.join(targetDir, cleanFileName);
    fs.writeFileSync(destFile, buf);
    copiedCount++;

    const base64 = buf.toString('base64');
    const dataUri = `data:image/bmp;base64,${base64}`;

    let cleanName = baseName.replace(/^\d+\s*[-_]?\s*/, '').trim();
    if (!cleanName) cleanName = baseName;
    cleanName = cleanName.charAt(0).toUpperCase() + cleanName.slice(1);

    masterManifest.push({
        id: `bmp_${idCounter++}`,
        rawFile: cleanFileName,
        name: cleanName,
        width,
        height,
        wasRotated,
        dataUri
    });
});

console.log(`Successfully unified ${copiedCount} pristine, rotated BMP files into "patterns/" folder!`);

// Save updated master-vault-manifest.js
const jsContent = `/**
 * UNIFIED MASTER POV PATTERN DATABASE (${masterManifest.length} BMPs in single 'patterns/' folder)
 * All tall portrait files are pre-rotated 90° for horizontal timeline playback.
 */
window.MASTER_BMP_VAULT = ${JSON.stringify(masterManifest, null, 2)};
`;

fs.writeFileSync(path.join(rootDir, 'master-vault-manifest.js'), jsContent, 'utf8');
console.log('Saved master-vault-manifest.js successfully!');
