const native = require('./node_modules/registry-js/build/Release/registry.node');


Object.assign(
	module.exports=require('registry-js'),
	{
		deleteFromPath:native['deleteFromPath'],
		readFromKey:native['readFromKey'],
	}
)