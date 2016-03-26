'use strict';

const fs = require('fs');
const path = require('path');
const electron = require('electron');
const app = electron.app;
const BrowserWindow = electron.BrowserWindow;

let mainWindow;

function usage(){
	console.log("usage: sitedl SITE FILE\n"+
		"Load and render the page at SITE, and download\n"+
		"the HTML contents to FILE.\n");
	app.exit(1);
}

function main () {
	if(process.argv.length != 4)
		usage();
	var MODE = 'HTMLComplete';
	var site = process.argv[2];
	var file = process.argv[3];
	if(file == null || file == undefined)
		usage();
	file = path.dirname(file)+path.sep+path.basename(file);
	try{
		fs.accessSync(file, fs.W_OK);
	}catch(e){
		console.log("sitedl: insufficient permissions on "+file);
		app.exit(1);
	}
	mainWindow = new BrowserWindow({
		width: 0,
		height: 0,
		"node-integration": false
	});

	/* from:
	 * http://electron.atom.io/docs/v0.37.2/api/web-contents/#webcontentssavepagefull-savetype-callback
	 */
	mainWindow.loadURL(site);
	mainWindow.webContents.on('did-finish-load', function(e){
		fs.closeSync(fs.openSync(file, 'w')); /* ensure file exists */
		var ok = mainWindow.webContents.savePage(fs.realpathSync(file), MODE, function(err){
			if(err){
				console.log(err);
				app.exit(1);
			}
			app.exit(0);
		});
		if(!ok)
			app.exit(1); /* not sure we need this */
	});
	// Emitted when the window is closed.
	mainWindow.on('closed', function(){
		mainWindow = null;
	});
}

app.on('ready', main);
app.on('window-all-closed', function(){
	app.quit();
});
