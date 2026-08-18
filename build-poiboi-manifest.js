const fs = require('fs');
const path = require('path');

const poiboiDir = path.join(__dirname, 'poiboi');
const files = fs.readdirSync(poiboiDir).filter(f => f.toLowerCase().endsWith('.bmp'));

console.log(`Found ${files.length} BMP files in poiboi directory.`);

const manifest = [];

files.forEach((file, index) => {
    const filePath = path.join(poiboiDir, file);
    const buf = fs.readFileSync(filePath);

    if (buf.length < 54 || buf.toString('ascii', 0, 2) !== 'BM') {
        console.warn(`Skipping invalid BMP: ${file}`);
        return;
    }

    const dataOffset = buf.readUInt32LE(10);
    const width = buf.readInt32LE(18);
    let height = buf.readInt32LE(22);
    const isTopDown = height < 0;
    height = Math.abs(height);
    const bpp = buf.readUInt16LE(28);

    const base64 = buf.toString('base64');
    const dataUri = `data:image/bmp;base64,${base64}`;

    // Clean name: e.g. "01 birdie.bmp" -> "Birdie", "14 eyeball.bmp" -> "Eyeball", "07 Triangle overlord.bmp" -> "Triangle Overlord"
    let cleanName = file.replace(/\.bmp$/i, '').replace(/^\d+\s*/, '').trim();
    if (!cleanName) cleanName = file.replace(/\.bmp$/i, '');
    cleanName = cleanName.charAt(0).toUpperCase() + cleanName.slice(1);

    manifest.push({
        id: `poiboi_${index + 1}`,
        rawFile: file,
        name: cleanName,
        width,
        height,
        bpp,
        isTopDown,
        dataUri
    });
});

console.log(`Successfully parsed ${manifest.length} PoiBoi BMP patterns.`);

const jsContent = `/**
 * POIBOI EMBEDDED BMP MASTER DATASET (81 Original POV Patterns)
 * Pre-decoded and embedded as binary base64 DataURIs for 100% offline & zero-CORS loading.
 */
window.POIBOI_VAULT = ${JSON.stringify(manifest, null, 2)};
`;

fs.writeFileSync(path.join(__dirname, 'poiboi-manifest.js'), jsContent, 'utf8');
console.log('Saved poiboi-manifest.js successfully!');
