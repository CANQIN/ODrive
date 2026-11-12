const fs = require('fs');
const path = require('path');


const enumsPath = process.env.ODRIVE_ENUMS_PATH || path.resolve(__dirname, '..', '..', 'tools', 'odrive', 'enums.py');
fs.readFile(enumsPath, 'utf8', function(err, data) {
    if (err) throw err;
    let enumsString = data;
    let lines = enumsString.split(/\r?\n/);
    let enums = {};
    for (const line of lines) {
        const constant = line.match(/^([A-Z][A-Z0-9_]*)\s*=\s*(0x[0-9a-fA-F]+|-?\d+)\s*(?:#.*)?$/);
        if (constant) {
            enums[constant[1]] = Number(constant[2]);
        }
    }
    fs.writeFile(path.resolve(__dirname, '../src/assets/odriveEnums.json'), JSON.stringify(enums, null, 4), function(err) {
        if (err) throw err;
    });
    console.log('Wrote ODrive enums to GUI/src/assets/odriveEnums.json');
});
