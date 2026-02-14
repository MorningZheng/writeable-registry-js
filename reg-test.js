// const {Object}=require('./bin/v24');
//
// const ws=new Object('WScript.Shell');
// // const clsid=ws.RegRead('HKCR\\Word.Application\\CLSID\\');
// //
// // console.log(ws.RegRead(`HKCR\\CLSID\\${clsid}\\ProgID\\`));
// // console.log(ws.RegRead(`HKCR\\CLSID\\${clsid}\\LocalServer32\\`));
//
// const version=`${(/\.(\d+)$/.exec(ws.RegRead('HKCR\\Word.Application\\CurVer\\'))??[0, 0])[1]}.0`;
// console.log(version);


const Registry = require('registry-js'),native=require('registry-js/build/Release/registry.node')
const {HKEY,enumerateKeys,enumerateValues,setValue,createKey,}=Registry;
const {readValues}=native;

// console.log(enumerateKeys(HKEY.HKEY_CLASSES_ROOT, 'Word.Application'));

const version=`${(/\.(\d+)$/.exec(enumerateValues(HKEY.HKEY_CLASSES_ROOT, 'Word.Application\\CurVer')[0].data)??[0, 0])[1]}.0`;


// console.log(version);

// for(const item of enumerateKeys(HKEY.HKEY_CURRENT_USER, `Software\\Microsoft\\Office\\${version}\\Word\\Resiliency`)){
// 	console.log(item);
// }


console.log(HKEY.HKEY_CLASSES_ROOT);

console.log(native.deleteFromPath(HKEY.HKEY_CURRENT_USER, `Software\\Microsoft\\Office\\${version}\\Word\\Resiliency`));

// console.log(native.deleteFromPath(HKEY.HKEY_CLASSES_ROOT, `Word.Application\\test`));

// console.log(enumerateKeys(HKEY.HKEY_CLASSES_ROOT,`Word.Application\\`))

// console.log(readValues(HKEY.HKEY_CLASSES_ROOT, `Word.Application`));