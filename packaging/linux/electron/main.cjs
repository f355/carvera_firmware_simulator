// This file is part of the Carvera Firmware Simulator.
//
// Copyright (c) 2026 Konstantin Tcepliaev <f355@f355.org>.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

const { spawn } = require('node:child_process');
const http = require('node:http');
const net = require('node:net');
const path = require('node:path');
const { app, BrowserWindow, dialog, Menu, shell } = require('electron');

app.commandLine.appendSwitch('enable-unsafe-swiftshader');

let backendProcess = null;
let mainWindow = null;
let shuttingDown = false;

function takeOption(args, name) {
  const remaining = [];
  let value = null;
  for (let index = 0; index < args.length; index += 1) {
    const argument = args[index];
    if (argument === name && index + 1 < args.length) {
      value = args[index + 1];
      index += 1;
    } else if (argument.startsWith(`${name}=`)) {
      value = argument.slice(name.length + 1);
    } else {
      remaining.push(argument);
    }
  }
  return { remaining, value };
}

function reservePort(host) {
  return new Promise((resolve, reject) => {
    const server = net.createServer();
    server.once('error', reject);
    server.listen(0, host, () => {
      const address = server.address();
      const port = typeof address === 'object' && address !== null ? address.port : null;
      server.close((error) => {
        if (error || port === null) {
          reject(error || new Error('Could not reserve a local port'));
        } else {
          resolve(port);
        }
      });
    });
  });
}

function healthCheck(url) {
  return new Promise((resolve) => {
    const request = http.get(url, (response) => {
      response.resume();
      resolve(response.statusCode === 200);
    });
    request.setTimeout(500, () => request.destroy());
    request.on('error', () => resolve(false));
  });
}

async function waitForBackend(url) {
  for (let attempt = 0; attempt < 120; attempt += 1) {
    if (backendProcess === null || backendProcess.exitCode !== null) {
      throw new Error('The simulator backend exited before it became ready');
    }
    if (await healthCheck(`${url}/healthz`)) {
      return;
    }
    await new Promise((resolve) => setTimeout(resolve, 250));
  }
  throw new Error('The simulator backend did not become ready within 30 seconds');
}

async function webglRenderer(window) {
  return window.webContents.executeJavaScript(`(() => {
    const canvas = document.createElement('canvas');
    const context = canvas.getContext('webgl2');
    if (!context) return null;
    const extension = context.getExtension('WEBGL_debug_renderer_info');
    return extension
      ? context.getParameter(extension.UNMASKED_RENDERER_WEBGL)
      : context.getParameter(context.RENDERER);
  })()`);
}

function stopBackend() {
  if (backendProcess !== null && backendProcess.exitCode === null && backendProcess.signalCode === null) {
    backendProcess.kill('SIGTERM');
  }
}

async function shutdown() {
  if (shuttingDown) {
    return;
  }
  shuttingDown = true;
  console.log('Shutting down Carvera Simulator...');
  stopBackend();
  if (backendProcess !== null && backendProcess.exitCode === null && backendProcess.signalCode === null) {
    await Promise.race([
      new Promise((resolve) => backendProcess.once('exit', resolve)),
      new Promise((resolve) => setTimeout(resolve, 3000)),
    ]);
    if (backendProcess.exitCode === null && backendProcess.signalCode === null) {
      backendProcess.kill('SIGKILL');
    }
  }
  app.quit();
}

async function start() {
  const userArgs = (app.isPackaged ? process.argv.slice(1) : process.argv.slice(2)).filter(
    (argument) => argument !== '--no-sandbox',
  );
  const hostOption = takeOption(userArgs, '--host');
  const portOption = takeOption(hostOption.remaining, '--port');
  const host = hostOption.value || '127.0.0.1';
  const port = portOption.value === null ? await reservePort(host) : Number.parseInt(portOption.value, 10);
  if (!Number.isInteger(port) || port < 1 || port > 65535) {
    throw new Error(`Invalid port: ${portOption.value}`);
  }

  const backendExecutable = path.join(
    process.resourcesPath,
    'backend',
    'Carvera Backend',
    'Carvera Backend',
  );
  backendProcess = spawn(
    backendExecutable,
    [...portOption.remaining, '--host', host, '--port', String(port)],
    { stdio: 'inherit' },
  );
  backendProcess.once('error', (error) => {
    console.error(`Could not start simulator backend: ${error.message}`);
  });
  backendProcess.once('exit', (code, signal) => {
    if (!shuttingDown) {
      console.error(`Simulator backend exited unexpectedly (${signal || code})`);
      app.exit(code || 1);
    }
  });

  const browserHost = host === '0.0.0.0' || host === '::' ? '127.0.0.1' : host;
  const url = `http://${browserHost}:${port}`;
  await waitForBackend(url);

  Menu.setApplicationMenu(null);
  mainWindow = new BrowserWindow({
    width: 1440,
    height: 900,
    minWidth: 960,
    minHeight: 640,
    show: false,
    backgroundColor: '#f5f5f5',
    title: 'Carvera Simulator',
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    },
  });
  mainWindow.webContents.setWindowOpenHandler(({ url: externalUrl }) => {
    void shell.openExternal(externalUrl);
    return { action: 'deny' };
  });
  mainWindow.webContents.on('will-navigate', (event, targetUrl) => {
    if (new URL(targetUrl).origin !== new URL(url).origin) {
      event.preventDefault();
      void shell.openExternal(targetUrl);
    }
  });
  mainWindow.once('closed', () => {
    mainWindow = null;
    void shutdown();
  });

  await mainWindow.loadURL(url);
  const renderer = await webglRenderer(mainWindow);
  if (renderer === null) {
    throw new Error('The native window could not create a WebGL2 rendering context');
  }
  console.log(`WebGL2 renderer: ${renderer}`);
  mainWindow.show();
}

app.whenReady().then(start).catch((error) => {
  console.error(error.stack || error.message);
  dialog.showErrorBox('Carvera Simulator could not start', error.message);
  void shutdown();
});

app.on('window-all-closed', () => {
  void shutdown();
});
app.on('before-quit', stopBackend);
process.on('SIGINT', () => void shutdown());
process.on('SIGTERM', () => void shutdown());
