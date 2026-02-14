const {promises: fs} = require('fs');
const {join, dirname} = require('path');

const $dirname = dirname(__filename);
const root = join($dirname, 'node_modules', 'registry-js');
const src_dir = join(root, 'src');

// fs.rm(join(src_dir), {recursive: true, force: true})
// 	.then(() => fs.symlink(join($dirname,'src'), src_dir));

const binding_gyp = join(root, 'binding.gyp');
fs.readFile(binding_gyp, 'utf-8')
	.then(text=>new Function(`return ${text.replaceAll('#','//')}`)())
	.then(/** @param {{targets:[{conditions:[]}]}} cnf */ cnf=>{
		for(const {conditions} of cnf.targets){
			for(const items of conditions){
				for(const item of items){
					if(!item.sources)continue;
					item.sources=Array.from(new Set(item.sources).add('src/writable.cc'));
				}
			}
		}
		return fs.writeFile(binding_gyp, JSON.stringify(cnf, null, 4));
	});
