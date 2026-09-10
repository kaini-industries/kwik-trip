// SPDX-License-Identifier: GPL-3.0-only

(function () {
  'use strict';

  const KNOWN_PROFILES = [
    { typeCode: 1206, width: 0, height: 0, kind: 'segment', color: 0, name: 'Continuum E2 HCS', page: 0 },
    { typeCode: 1207, width: 0, height: 0, kind: 'segment', color: 0, name: 'Continuum E2 HCN', page: 0 },
    { typeCode: 1217, width: 0, height: 0, kind: 'segment', color: 0, name: 'Continuum E5 HCS', page: 0 },
    { typeCode: 1219, width: 0, height: 0, kind: 'segment', color: 0, name: 'Continuum E5 HCN', page: 0 },
    { typeCode: 1240, width: 0, height: 0, kind: 'segment', color: 0, name: 'Continuum E4 HCS', page: 0 },
    { typeCode: 1241, width: 0, height: 0, kind: 'segment', color: 0, name: 'Continuum E4 HCN', page: 0 },
    { typeCode: 1242, width: 0, height: 0, kind: 'segment', color: 0, name: 'Continuum E4 HCN FZ', page: 0 },
    { typeCode: 1243, width: 0, height: 0, kind: 'segment', color: 0, name: 'Continuum E4 HCW', page: 0 },
    { typeCode: 1265, width: 0, height: 0, kind: 'segment', color: 0, name: 'Continuum E5 HCS', page: 0 },
    { typeCode: 1275, width: 320, height: 192, kind: 'graphic', color: 0, name: 'DM110', page: 0 },
    { typeCode: 1276, width: 320, height: 140, kind: 'graphic', color: 0, name: 'DM90', page: 0 },
    { typeCode: 1291, width: 0, height: 0, kind: 'segment', color: 0, name: 'FVL Promoline 3-16', page: 0 },
    { typeCode: 1300, width: 172, height: 72, kind: 'graphic', color: 0, name: 'DM3370', page: 0 },
    { typeCode: 1314, width: 400, height: 300, kind: 'graphic', color: 0, name: 'SmartTag HD110', page: 0 },
    { typeCode: 1315, width: 296, height: 128, kind: 'graphic', color: 0, name: 'SmartTag HD L', page: 0 },
    { typeCode: 1317, width: 152, height: 152, kind: 'graphic', color: 0, name: 'SmartTag HD S', page: 0 },
    { typeCode: 1318, width: 208, height: 112, kind: 'graphic', color: 0, name: 'SmartTag HD M', page: 0 },
    { typeCode: 1319, width: 800, height: 480, kind: 'graphic', color: 0, name: 'SmartTag HD200', page: 0 },
    { typeCode: 1322, width: 152, height: 152, kind: 'graphic', color: 0, name: 'SmartTag HD S', page: 0 },
    { typeCode: 1324, width: 208, height: 112, kind: 'graphic', color: 0, name: 'SmartTag HD M FZ', page: 0 },
    { typeCode: 1327, width: 208, height: 112, kind: 'graphic', color: 1, name: 'SmartTag HD M Red', page: 0 },
    { typeCode: 1328, width: 296, height: 128, kind: 'graphic', color: 1, name: 'SmartTag HD L Red', page: 0 },
    { typeCode: 1336, width: 400, height: 300, kind: 'graphic', color: 1, name: 'SmartTag HD110 Red', page: 0 },
    { typeCode: 1339, width: 152, height: 152, kind: 'graphic', color: 1, name: 'SmartTag HD S Red', page: 0 },
    { typeCode: 1340, width: 800, height: 480, kind: 'graphic', color: 1, name: 'SmartTag HD200 Red', page: 0 },
    { typeCode: 1344, width: 296, height: 128, kind: 'graphic', color: 2, name: 'SmartTag HD L Yellow', page: 0 },
    { typeCode: 1346, width: 800, height: 480, kind: 'graphic', color: 2, name: 'SmartTag HD200 Yellow', page: 0 },
    { typeCode: 1348, width: 264, height: 176, kind: 'graphic', color: 1, name: 'SmartTag HD T Red', page: 0 },
    { typeCode: 1349, width: 264, height: 176, kind: 'graphic', color: 2, name: 'SmartTag HD T Yellow', page: 0 },
    { typeCode: 1351, width: 648, height: 480, kind: 'graphic', color: 0, name: 'SmartTag HD150', page: 0 },
    { typeCode: 1353, width: 648, height: 480, kind: 'graphic', color: 1, name: 'SmartTag HD150 Red', page: 0 },
    { typeCode: 1354, width: 648, height: 480, kind: 'graphic', color: 1, name: 'SmartTag HD150 Red', page: 0 },
    { typeCode: 1370, width: 296, height: 128, kind: 'graphic', color: 1, name: 'SmartTag HD L Red', page: 0 },
    { typeCode: 1371, width: 648, height: 480, kind: 'graphic', color: 1, name: 'SmartTag HD150 Red', page: 0 },
    { typeCode: 1510, width: 0, height: 0, kind: 'segment', color: 0, name: 'SmartTag E5 M', page: 0 },
    { typeCode: 1626, width: 296, height: 152, kind: 'graphic', color: 3, name: 'SmartTag Color 2.6', page: 1 },
    { typeCode: 1627, width: 296, height: 128, kind: 'graphic', color: 1, name: 'SmartTag HD L Red', page: 0 },
    { typeCode: 1628, width: 296, height: 128, kind: 'graphic', color: 1, name: 'SmartTag HD L Red', page: 0 },
    { typeCode: 1639, width: 152, height: 152, kind: 'graphic', color: 1, name: 'SmartTag HD S Red', page: 0 },
    { typeCode: 3145, width: 400, height: 300, kind: 'graphic', color: 0, name: 'SmartTag HD110', page: 0 },
    { typeCode: 3547, width: 648, height: 480, kind: 'graphic', color: 1, name: 'SmartTag HD150 Red', page: 0 },
    { typeCode: 6275, width: 296, height: 128, kind: 'graphic', color: 0, name: 'SmartTag HD L', page: 0 },
    { typeCode: 3220, width: 152, height: 152, kind: 'graphic', color: 0, name: 'SmartTag HD S', page: 0 },
    { typeCode: 3227, width: 152, height: 152, kind: 'graphic', color: 0, name: 'SmartTag HD S', page: 0 },
    { typeCode: 3229, width: 152, height: 152, kind: 'graphic', color: 0, name: 'SmartTag HD S', page: 0 }
  ];

  function getProfile(typeCode) {
    return KNOWN_PROFILES.find(p => p.typeCode === typeCode) || null;
  }

  function colorLabel(code) {
    switch (code) {
      case 0: return 'Mono';
      case 1: return 'BWR Red';
      case 2: return 'BWY Yellow';
      case 3: return '4-Color';
      default: return 'Unknown';
    }
  }

  const state = {
    transportType: 'serial',
    status: 'idle',
    progress: 0,
    tags: [],
    activeTagId: null,
    viewMode: 'view-image',

    studioImage: null,
    studioFit: 'cover',
    studioDither: 'floyd',
    studioContrast: 0,
    studioThreshold: 128,
    studioInvert: false,
    studioColorPlane: true,
    studioPage: 1,
    speed: 'reliable',
    contentRevision: 0,
    stagedArtwork: null,

    activePlugin: 'github',

    scannerActive: false,
    scannerStream: null,
    scannerTimer: null,
    scannerDetector: null,
    scannerZxing: null,
    pendingScannedTag: null
  };

  const Transport = {
    type: 'serial',
    bleDevice: null,
    bleServer: null,
    bleRx: null,
    bleTx: null,
    serialPort: null,
    serialReader: null,
    serialWriter: null,
    connected: false,
    deviceName: '',
    buffer: '',
    pendingReply: null,
    sendQueue: Promise.resolve(),
    session: 0,
    stopping: false,
    serialReadTask: null,
    bleDecoder: null,
    bleNotificationHandler: null,

    NUS_SERVICE: '6e400001-b5a3-f393-e0a9-e50e24dcca9e',
    NUS_RX: '6e400002-b5a3-f393-e0a9-e50e24dcca9e',
    NUS_TX: '6e400003-b5a3-f393-e0a9-e50e24dcca9e',

    isConnected() {
      return this.connected;
    },

    beginSession() {
      this.session += 1;
      this.stopping = false;
      this.buffer = '';
      this.sendQueue = Promise.resolve();
      resetTransferSession();
      return this.session;
    },

    markDisconnected(session) {
      if (session !== this.session) return;
      if (this.bleTx && this.bleNotificationHandler) {
        this.bleTx.removeEventListener('characteristicvaluechanged', this.bleNotificationHandler);
      }
      this.session += 1;
      this.connected = false;
      this.stopping = true;
      this.buffer = '';
      this.deviceName = '';
      this.bleDevice = null;
      this.bleServer = null;
      this.bleRx = null;
      this.bleTx = null;
      this.bleDecoder = null;
      this.bleNotificationHandler = null;
      this.serialPort = null;
      this.serialReader = null;
      this.serialWriter = null;
      this.serialReadTask = null;
      const pending = this.pendingReply;
      this.pendingReply = null;
      if (pending) pending.reject(new Error('Device disconnected.'));
      resetTransferSession();
      this.updateUi();
    },

    async connectBle() {
      if (!navigator.bluetooth) {
        alert('Web Bluetooth is not supported on this browser. Try Chrome, Edge, or Bluefy on iOS.');
        return;
      }

      const session = this.beginSession();
      let device = null;
      try {
        device = await navigator.bluetooth.requestDevice({
          filters: [{ namePrefix: 'etag Cardputer' }],
          optionalServices: [this.NUS_SERVICE]
        });

        const server = await device.gatt.connect();
        const service = await server.getPrimaryService(this.NUS_SERVICE);
        const rx = await service.getCharacteristic(this.NUS_RX);
        const tx = await service.getCharacteristic(this.NUS_TX);
        if (session !== this.session) {
          if (device.gatt.connected) device.gatt.disconnect();
          return;
        }

        this.bleDecoder = new TextDecoder();
        this.bleNotificationHandler = (e) => {
          if (session !== this.session || device !== this.bleDevice) return;
          const chunk = this.bleDecoder.decode(e.target.value, { stream: true });
          this.handleIncoming(chunk);
        };
        await tx.startNotifications();
        tx.addEventListener('characteristicvaluechanged', this.bleNotificationHandler);

        device.addEventListener('gattserverdisconnected', () => {
          this.markDisconnected(session);
        });

        this.bleDevice = device;
        this.bleServer = server;
        this.bleRx = rx;
        this.bleTx = tx;
        this.connected = true;
        this.type = 'ble';
        this.deviceName = device.name || 'etag Cardputer';
        this.updateUi();

        await this.sendLine('{"cmd":"getState"}');
      } catch (err) {
        if (device && device.gatt.connected) device.gatt.disconnect();
        this.markDisconnected(session);
        console.error('BLE connection failed:', err);
        alert('Bluetooth error: ' + err.message);
      }
    },

    async connectSerial() {
      if (!navigator.serial) {
        alert('Web Serial is not supported on this browser. Try desktop Chrome or Edge.');
        return;
      }

      const session = this.beginSession();
      let port = null;
      try {
        port = await navigator.serial.requestPort();
        await port.open({ baudRate: 115200 });
        if (session !== this.session) {
          await port.close();
          return;
        }

        this.serialPort = port;
        this.connected = true;
        this.type = 'serial';
        this.deviceName = 'USB Serial';
        this.updateUi();

        const readTask = this.readSerialLoop(port, session);
        this.serialReadTask = readTask;
        await this.sendLine('{"cmd":"getState"}');
      } catch (err) {
        if (session === this.session && port && port === this.serialPort) {
          try { await this.disconnect(); } catch (_) {}
        } else if (port && session === this.session) {
          try { await port.close(); } catch (_) {}
          this.markDisconnected(session);
        }
        console.error('Serial connection error:', err);
        alert('Serial error: ' + err.message);
      }
    },

    async readSerialLoop(port, session) {
      const dec = new TextDecoder();
      let reader = null;
      try {
        if (session !== this.session || this.stopping || !port.readable) return;
        reader = port.readable.getReader();
        this.serialReader = reader;
        while (session === this.session && !this.stopping) {
          const { value, done } = await reader.read();
          if (session !== this.session || this.stopping) break;
          if (done) break;
          if (value) {
            this.handleIncoming(dec.decode(value, { stream: true }));
          }
        }
        if (session === this.session && !this.stopping) this.handleIncoming(dec.decode());
      } catch (err) {
        if (session === this.session && !this.stopping) console.warn('Serial read error:', err);
      } finally {
        if (reader) reader.releaseLock();
        if (this.serialReader === reader) this.serialReader = null;
      }

      if (session === this.session && !this.stopping) {
        try { await port.close(); } catch (_) {}
        this.markDisconnected(session);
      }
    },

    async disconnect() {
      const closingSession = this.session;
      const serialPort = this.serialPort;
      const serialReader = this.serialReader;
      const serialWriter = this.serialWriter;
      const serialReadTask = this.serialReadTask;
      const bleDevice = this.bleDevice;
      const bleTx = this.bleTx;
      const bleNotificationHandler = this.bleNotificationHandler;
      const writes = this.sendQueue;
      this.session += 1;
      this.stopping = true;
      this.connected = false;
      this.buffer = '';
      this.sendQueue = Promise.resolve();
      const pending = this.pendingReply;
      this.pendingReply = null;
      if (pending) pending.reject(new Error('Device disconnected.'));
      resetTransferSession();
      this.updateUi();

      if (bleTx && bleNotificationHandler) {
        bleTx.removeEventListener('characteristicvaluechanged', bleNotificationHandler);
      }
      if (bleDevice && bleDevice.gatt.connected) bleDevice.gatt.disconnect();

      if (serialWriter) {
        try { await serialWriter.abort(new Error('Device disconnected.')); } catch (_) {}
      }
      try { await writes; } catch (_) {}
      if (serialReader) {
        try { await serialReader.cancel(); } catch (_) {}
      }
      if (serialReadTask) {
        try { await serialReadTask; } catch (_) {}
      }
      let closeError = null;
      if (serialPort) {
        try { await serialPort.close(); } catch (error) { closeError = error; }
      }

      if (closingSession + 1 === this.session) {
        this.bleDevice = null;
        this.bleServer = null;
        this.bleRx = null;
        this.bleTx = null;
        this.bleDecoder = null;
        this.bleNotificationHandler = null;
        this.serialPort = null;
        this.serialReader = null;
        this.serialWriter = null;
        this.serialReadTask = null;
      }
      if (closeError) throw closeError;
    },

    sendLine(str) {
      // Whole JSON lines must not interleave across asynchronous BLE writes.
      const session = this.session;
      const next = this.sendQueue.catch(() => {}).then(() => {
        if (session !== this.session || this.stopping || !this.connected) {
          throw new Error('Device disconnected.');
        }
        return this.writeLine(str, session);
      });
      this.sendQueue = next;
      return next;
    },

    async writeLine(str, session = this.session) {
      if (!str.endsWith('\n')) str += '\n';
      const enc = new TextEncoder();
      const bytes = enc.encode(str);

      if (this.type === 'ble' && this.bleRx) {
        const MTU = 20;
        for (let i = 0; i < bytes.length; i += MTU) {
          if (session !== this.session || this.stopping) throw new Error('Device disconnected.');
          const slice = bytes.slice(i, i + MTU);
          await this.bleRx.writeValueWithResponse(slice);
        }
      } else if (this.type === 'serial' && this.serialPort && this.serialPort.writable) {
        const writer = this.serialPort.writable.getWriter();
        this.serialWriter = writer;
        try { await writer.write(bytes); }
        finally {
          if (this.serialWriter === writer) this.serialWriter = null;
          writer.releaseLock();
        }
      } else {
        throw new Error('Device disconnected.');
      }
    },

    async request(command, event) {
      if (this.pendingReply) throw new Error('Another upload request is pending.');
      let timer;
      const reply = new Promise((resolve, reject) => {
        timer = setTimeout(() => reject(new Error('Device did not acknowledge the upload.')), 10000);
        this.pendingReply = { event, resolve, reject, session: this.session };
      });
      const pending = this.pendingReply;
      try {
        const [, data] = await Promise.all([this.sendLine(JSON.stringify(command)), reply]);
        return data;
      } finally {
        clearTimeout(timer);
        if (this.pendingReply === pending) this.pendingReply = null;
      }
    },

    handleIncoming(chunk) {
      if (this.buffer.length + chunk.length > 16384) {
        this.buffer = '';
        if (this.pendingReply) this.pendingReply.reject(new Error('Device response too large. Reconnect.'));
        return;
      }
      this.buffer += chunk;
      let newline;
      while ((newline = this.buffer.indexOf('\n')) !== -1) {
        const line = this.buffer.substring(0, newline).trim();
        this.buffer = this.buffer.substring(newline + 1);
        if (line) {
          try {
            const msg = JSON.parse(line);
            this.handleMessage(msg);
          } catch (e) {
            // Non-JSON debug output
          }
        }
      }
    },

    handleMessage(msg) {
      if (!msg || typeof msg !== 'object' || Array.isArray(msg) ||
          typeof msg.event !== 'string' || !/^[A-Za-z][A-Za-z0-9]{0,31}$/.test(msg.event)) {
        rejectDeviceMessage('Invalid event envelope received from the device.');
        return;
      }
      if (msg.data && (msg.data.speed === 'fast' || msg.data.speed === 'reliable')) {
        state.speed = msg.data.speed;
        document.getElementById('transfer-speed').value = state.speed;
      }
      if (this.pendingReply && this.pendingReply.session === this.session) {
        if (msg.event === this.pendingReply.event) this.pendingReply.resolve(msg.data);
        if (msg.event === 'error') this.pendingReply.reject(new Error(msg.message || 'Upload rejected.'));
      }
      if (msg.event === 'state' && msg.data) {
        const next = normalizeDeviceState(msg.data);
        if (!next) {
          rejectDeviceMessage('Invalid state response received from the device.');
          return;
        }
        const tagsChanged = tagListSignature(state.tags) !== tagListSignature(next.tags);
        state.tags = next.tags;
        state.speed = next.speed;
        document.getElementById('transfer-speed').value = state.speed;
        if (tagsChanged) invalidateArtwork();
        renderTagSelector();
        renderSavedTags();
        applyDeviceTransferStatus(next);
        updateStatusIndicator();
        updateTxHud();
      } else if (msg.event === 'progress' || msg.event === 'status') {
        const d = msg.data || msg;
        const transfer = normalizeTransferStatus(d);
        if (!transfer) {
          rejectDeviceMessage('Invalid transfer status received from the device.');
          return;
        }
        if (transfer.speed) {
          state.speed = transfer.speed;
          document.getElementById('transfer-speed').value = state.speed;
        }
        applyDeviceTransferStatus(transfer);
        updateStatusIndicator();
        updateTxHud();
      } else if (msg.event === 'artLoaded') {
        // sendArtwork validates and adopts this acknowledgement after the upload
        // promise resumes. An unsolicited artLoaded event never enables transmit.
      } else if (msg.event === 'artStarted') {
        const stage = normalizeStageReference(msg.data);
        if (stage && stageReferenceMatches(stage)) {
          state.status = 'sending';
          state.progress = 0;
          updateStatusIndicator();
          updateTxHud();
        }
      } else if (msg.event === 'cancelled') {
        state.status = 'cancelled';
        updateStatusIndicator();
        updateTxHud();
      } else if (msg.event === 'error') {
        state.status = 'error';
        updateStatusIndicator();
        updateTxHud();
        const message = typeof msg.message === 'string' && msg.message.length <= 256 ? msg.message : 'Unknown';
        alert('Device Error: ' + message);
      }
    },

    async sendArtwork(tagId, page, rawBitstream, profile) {
      const isColorTag = profile && profile.color !== 0;
      const totalBits = profile ? (profile.width * profile.height * (isColorTag ? 2 : 1)) : (rawBitstream.length * 8);
      const encoded = encodeEslArtwork(rawBitstream, totalBits);

      const startCmd = {
        cmd: 'startArt',
        id: tagId,
        page: parseInt(page, 10),
        len: encoded.bytes.length,
        comp: encoded.compression
      };
      const ready = await this.request(startCmd, 'artReady');
      if (!ready || !Number.isInteger(ready.len) || ready.len !== encoded.bytes.length) {
        throw new Error('Artwork size was not accepted.');
      }

      const chunkSize = 240;
      const totalBytes = encoded.bytes.length;
      for (let offset = 0; offset < totalBytes; offset += chunkSize) {
        const slice = encoded.bytes.subarray(offset, Math.min(totalBytes, offset + chunkSize));
        let binary = '';
        for (let i = 0; i < slice.length; i++) binary += String.fromCharCode(slice[i]);
        const b64 = btoa(binary);

        const ack = await this.request({ cmd: 'artChunk', offset, data: b64 }, 'chunkAck');
        if (!ack || !Number.isInteger(ack.received) || !Number.isInteger(ack.expected) ||
            ack.received !== offset + slice.length || ack.expected !== totalBytes) {
          throw new Error('Artwork upload lost a chunk. Restart the upload.');
        }

        const currentProg = Math.min(99, Math.round(((offset + slice.length) / totalBytes) * 100));
        state.progress = currentProg;
        updateTxHud();
        updateStatusIndicator();


      }

      const loaded = await this.request({ cmd: 'finishArt' }, 'artLoaded');
      const stage = normalizeStageReference(loaded);
      if (!stage || stage.id !== tagId || stage.page !== parseInt(page, 10)) {
        throw new Error('Cardputer firmware did not identify the staged artwork. Update its firmware and retry.');
      }
      return stage;
    },

    updateUi() {
      const dot = document.getElementById('header-status-dot');
      const text = document.getElementById('header-status-text');
      const connBtn = document.getElementById('btn-connect-active');
      const disconnBtn = document.getElementById('btn-disconnect-active');
      const infoStatus = document.getElementById('info-conn-status');
      const infoDevice = document.getElementById('info-device-name');
      const infoTransport = document.getElementById('info-transport-type');

      if (this.connected) {
        dot.className = 'status-dot online';
        text.textContent = (this.type === 'ble' ? 'BLE' : 'USB') + ' READY';
        connBtn.style.display = 'none';
        disconnBtn.style.display = 'block';
        infoStatus.textContent = 'Connected';
        infoStatus.style.color = 'var(--accent-green)';
        infoDevice.textContent = this.deviceName;
        infoTransport.textContent = this.type.toUpperCase();
      } else {
        dot.className = 'status-dot';
        text.textContent = 'CONNECT';
        connBtn.style.display = 'block';
        disconnBtn.style.display = 'none';
        infoStatus.textContent = 'Disconnected';
        infoStatus.style.color = '#ffffff';
        infoDevice.textContent = '-';
        infoTransport.textContent = this.type.toUpperCase();
      }
      updateStatusIndicator();
    }
  };

  const TRANSFER_STATUSES = new Set(['idle', 'uploading', 'loaded', 'sending', 'sent', 'error', 'cancelled']);

  function boundedString(value, maximum) {
    return typeof value === 'string' && value.length <= maximum ? value : null;
  }

  function normalizeTag(value) {
    if (!value || typeof value !== 'object' || Array.isArray(value)) return null;
    if (typeof value.id !== 'string' || !/^[0-9a-fA-F]{8}$/.test(value.id)) return null;
    const id = value.id.toUpperCase();
    const name = boundedString(value.name, 24);
    const barcode = boundedString(value.barcode, 17);
    const model = boundedString(value.model, 64);
    if (name === null || barcode === null || model === null ||
        !/^[A-Za-z0-9]4[0-9]{15}$/.test(barcode) ||
        typeof value.rotate !== 'boolean' || typeof value.graphic !== 'boolean' ||
        !Number.isInteger(value.width) || !Number.isInteger(value.height) ||
        !Number.isInteger(value.color) || value.color < 0 || value.color > 3 ||
        !Number.isInteger(value.page) || value.page < 0 || value.page > 7 ||
        (value.transport !== 'ir_pp4' && value.transport !== 'ir_pp16')) return null;

    if (value.graphic) {
      const pixels = value.width * value.height;
      if (value.width < 8 || value.width > 2048 || value.height < 8 || value.height > 2048 ||
          pixels > 384000 || pixels % 8 !== 0) return null;
    } else if (value.width !== 0 || value.height !== 0 || value.transport !== 'ir_pp4') {
      return null;
    }

    return {
      id, name, barcode, model,
      width: value.width,
      height: value.height,
      rotate: value.rotate,
      color: value.color,
      graphic: value.graphic,
      page: value.page,
      transport: value.transport,
      firmwareRead: value.firmwareRead === true,
      firmwareWrite: value.firmwareWrite === true,
      acknowledged: value.acknowledged === true
    };
  }

  function normalizeStageReference(value) {
    if (!value || typeof value !== 'object' || Array.isArray(value) ||
        typeof value.id !== 'string' || !/^[0-9a-fA-F]{8}$/.test(value.id) ||
        !Number.isInteger(value.page) || value.page < 0 || value.page > 7 ||
        !Number.isInteger(value.stage) || value.stage < 1 || value.stage > 0xffffffff) return null;
    return { id: value.id.toUpperCase(), page: value.page, stage: value.stage };
  }

  function normalizeTransferStatus(value) {
    if (!value || typeof value !== 'object' || Array.isArray(value) ||
        typeof value.status !== 'string' || !TRANSFER_STATUSES.has(value.status) ||
        !Number.isInteger(value.progress) || value.progress < 0 || value.progress > 100) return null;
    if (value.speed !== undefined && value.speed !== 'fast' && value.speed !== 'reliable') return null;
    const hasStageField = value.stage !== undefined || value.id !== undefined || value.page !== undefined;
    const stage = hasStageField ? normalizeStageReference(value) : null;
    if (hasStageField && !stage) return null;
    return {
      status: value.status,
      progress: value.progress,
      speed: value.speed,
      stage
    };
  }

  function normalizeDeviceState(value) {
    if (!value || typeof value !== 'object' || Array.isArray(value) ||
        boundedString(value.device, 64) === null || !Array.isArray(value.tags) || value.tags.length > 9 ||
        (value.speed !== 'fast' && value.speed !== 'reliable')) return null;
    const transfer = normalizeTransferStatus(value);
    if (!transfer) return null;
    const tags = value.tags.map(normalizeTag);
    if (tags.some(tag => tag === null) || new Set(tags.map(tag => tag.id)).size !== tags.length) return null;
    return { device: value.device, tags, ...transfer };
  }

  function tagListSignature(tags) {
    return JSON.stringify(tags.map(tag => [tag.id, tag.width, tag.height, tag.rotate, tag.color,
      tag.graphic, tag.page, tag.transport]));
  }

  function sameBytes(left, right) {
    if (!(left instanceof Uint8Array) || !(right instanceof Uint8Array) || left.length !== right.length) return false;
    for (let i = 0; i < left.length; i++) if (left[i] !== right[i]) return false;
    return true;
  }

  function stageReferenceMatches(reference) {
    const staged = state.stagedArtwork;
    return Boolean(staged && reference && staged.session === Transport.session &&
      staged.id === reference.id && staged.page === reference.page && staged.stage === reference.stage);
  }

  function stagedArtworkMatches(active, page, bitstream) {
    const staged = state.stagedArtwork;
    return Boolean(staged && Transport.isConnected() && staged.session === Transport.session &&
      active && staged.id === active.id && staged.page === page &&
      staged.revision === state.contentRevision && sameBytes(staged.bitstream, bitstream));
  }

  function buildTransmitCommand(staged) {
    const reference = normalizeStageReference(staged);
    if (!reference) throw new Error('No valid staged artwork is available.');
    return { cmd: 'transmit', id: reference.id, page: reference.page, stage: reference.stage };
  }

  function applyDeviceTransferStatus(transfer) {
    if ((transfer.status === 'loaded' || transfer.status === 'sent') &&
        (!transfer.stage || !stageReferenceMatches(transfer.stage))) {
      if (state.status !== 'uploading' && state.status !== 'sending') {
        state.status = 'idle';
        state.progress = 0;
      }
      return;
    }
    if (transfer.stage && !stageReferenceMatches(transfer.stage) &&
        (transfer.status === 'sending' || transfer.status === 'sent')) return;
    state.status = transfer.status;
    state.progress = transfer.progress;
  }

  function resetTransferSession() {
    state.stagedArtwork = null;
    state.status = 'idle';
    state.progress = 0;
  }

  function invalidateArtwork() {
    state.contentRevision += 1;
    state.stagedArtwork = null;
    if (state.status !== 'uploading' && state.status !== 'sending') {
      state.status = 'idle';
      state.progress = 0;
    }
    updateStatusIndicator();
    updateTxHud();
  }

  function rejectDeviceMessage(reason) {
    console.warn(reason);
    const pending = Transport.pendingReply;
    Transport.pendingReply = null;
    if (pending) pending.reject(new Error(reason));
    state.status = 'error';
    state.progress = 0;
    updateStatusIndicator();
    updateTxHud();
  }

  async function disconnectTransport() {
    try {
      await Transport.disconnect();
    } catch (error) {
      console.error('Disconnect failed:', error);
      alert('Disconnect error: ' + error.message);
    }
  }

  function ditherFloydSteinberg(ctx, width, height, opts = {}) {
    const imgData = ctx.getImageData(0, 0, width, height);
    const d = imgData.data;
    const invert = opts.invert || false;
    const contrast = Number.isFinite(opts.contrast) ? opts.contrast : 0;
    const contrastFactor = (259 * (contrast + 255)) / (255 * (259 - contrast));
    const colorMode = Number.isInteger(opts.colorMode) ? opts.colorMode : 0;

    const lum = new Float32Array(width * height);
    const colorMask = new Uint8Array(width * height); // 2 = red, 3 = yellow

    for (let i = 0; i < lum.length; i++) {
      let r = d[i * 4];
      let g = d[i * 4 + 1];
      let b = d[i * 4 + 2];

      r = Math.min(255, Math.max(0, contrastFactor * (r - 128) + 128));
      g = Math.min(255, Math.max(0, contrastFactor * (g - 128) + 128));
      b = Math.min(255, Math.max(0, contrastFactor * (b - 128) + 128));

      if (colorMode !== 0 && opts.colorPlane) {
        const pixelColor = classifyPixel(r, g, b, colorMode);
        if (pixelColor === 2 || pixelColor === 3) {
          colorMask[i] = colorMode === 2 ? 3 : pixelColor;
        }
      }

      let gray = 0.299 * r + 0.587 * g + 0.114 * b;
      if (invert) gray = 255 - gray;
      lum[i] = gray;
    }

    if (opts.algo === 'threshold') {
      const thresh = Number.isFinite(opts.threshold) ? opts.threshold : 128;
      for (let i = 0; i < lum.length; i++) {
        if (colorMask[i] !== 0) {
          const yellow = colorMask[i] === 3;
          d[i * 4] = yellow ? 245 : 239;
          d[i * 4 + 1] = yellow ? 197 : 68;
          d[i * 4 + 2] = yellow ? 24 : 68;
          d[i * 4 + 3] = 255;
        } else {
          const val = lum[i] >= thresh ? 255 : 0;
          d[i * 4] = val; d[i * 4 + 1] = val; d[i * 4 + 2] = val; d[i * 4 + 3] = 255;
        }
      }
    } else {
      for (let y = 0; y < height; y++) {
        for (let x = 0; x < width; x++) {
          const idx = y * width + x;
          if (colorMask[idx] !== 0) {
            const yellow = colorMask[idx] === 3;
            d[idx * 4] = yellow ? 245 : 239;
            d[idx * 4 + 1] = yellow ? 197 : 68;
            d[idx * 4 + 2] = yellow ? 24 : 68;
            d[idx * 4 + 3] = 255;
            continue;
          }

          const oldVal = lum[idx];
          const newVal = oldVal >= 128 ? 255 : 0;
          lum[idx] = newVal;
          const err = oldVal - newVal;

          d[idx * 4] = newVal;
          d[idx * 4 + 1] = newVal;
          d[idx * 4 + 2] = newVal;
          d[idx * 4 + 3] = 255;

          if (x + 1 < width) lum[idx + 1] += err * (7 / 16);
          if (x - 1 >= 0 && y + 1 < height) lum[(y + 1) * width + (x - 1)] += err * (3 / 16);
          if (y + 1 < height) lum[(y + 1) * width + x] += err * (5 / 16);
          if (x + 1 < width && y + 1 < height) lum[(y + 1) * width + (x + 1)] += err * (1 / 16);
        }
      }
    }

    ctx.putImageData(imgData, 0, 0);
  }

  function exportEslBitstream(canvas, profile) {
    const wireWidth = profile.width;
    const wireHeight = profile.height;
    const isMono = profile.color === 0;
    const isFourColor = profile.color === 3;
    const hasColorPlane = !isMono;

    const dispW = canvas.width;
    const dispH = canvas.height;
    const ctx = canvas.getContext('2d');
    const imgData = ctx.getImageData(0, 0, dispW, dispH);
    const data = imgData.data;

    const totalPixels = wireWidth * wireHeight;
    const totalBits = totalPixels * (hasColorPlane ? 2 : 1);
    const totalBytes = Math.ceil(totalBits / 8);
    const out = new Uint8Array(totalBytes);

    let bitPos = 0;
    function appendBit(val) {
      if (val) {
        out[bitPos >> 3] |= (0x80 >> (bitPos & 7));
      }
      bitPos++;
    }

    for (let y = 0; y < wireHeight; y++) {
      for (let x = 0; x < wireWidth; x++) {
        const sx = profile.rotate ? y : x;
        const sy = profile.rotate ? (wireWidth - 1 - x) : y;
        const pxIdx = (sy * dispW + sx) * 4;
        const r = data[pxIdx];
        const g = data[pxIdx + 1];
        const b = data[pxIdx + 2];

        const pixelColor = classifyPixel(r, g, b, profile.color);
        let firstPlane = (pixelColor !== 0);
        if (isFourColor) {
          firstPlane = (pixelColor === 1 || pixelColor === 2);
        }
        appendBit(firstPlane);
      }
    }

    if (hasColorPlane) {
      for (let y = 0; y < wireHeight; y++) {
        for (let x = 0; x < wireWidth; x++) {
          const sx = profile.rotate ? y : x;
          const sy = profile.rotate ? (wireWidth - 1 - x) : y;
          const pxIdx = (sy * dispW + sx) * 4;
          const r = data[pxIdx];
          const g = data[pxIdx + 1];
          const b = data[pxIdx + 2];

          const pixelColor = classifyPixel(r, g, b, profile.color);
          const secondPlane = isFourColor ? (pixelColor < 2) : (pixelColor !== 2);
          appendBit(secondPlane);
        }
      }
    }

    return out;
  }

  function classifyPixel(r, g, b, colorMode) {
    const lum = 0.299 * r + 0.587 * g + 0.114 * b;
    if (colorMode === 0) return lum > 128 ? 1 : 0;

    const max = Math.max(r, g, b);
    const min = Math.min(r, g, b);
    const sat = max === 0 ? 0 : (max - min) / max;

    if (sat > 0.4) {
      if ((colorMode === 1 || colorMode === 3) && r > 140 && g < 110 && b < 110) return 2;
      if ((colorMode === 2 || colorMode === 3) && r > 140 && g > 120 && b < 80) {
        return colorMode === 3 ? 3 : 2;
      }
    }
    return lum > 128 ? 1 : 0;
  }

  function runCodeBits(runLength) {
    let width = 0;
    for (let v = runLength; v !== 0; v >>>= 1) width++;
    return width * 2 - 1;
  }

  function encodeEslArtwork(rawBytes, totalBits) {
    function bitAt(bytes, idx) {
      return (bytes[idx >> 3] & (0x80 >> (idx & 7))) !== 0;
    }

    if (totalBits === 0) {
      return { bytes: new Uint8Array(0), compression: 0, totalBits: 0 };
    }

    let runPixel = bitAt(rawBytes, 0);
    let runLength = 1;
    let compressedBits = 1;

    for (let i = 1; i < totalBits; i++) {
      const px = bitAt(rawBytes, i);
      if (px === runPixel) {
        runLength++;
      } else {
        compressedBits += runCodeBits(runLength);
        runPixel = px;
        runLength = 1;
      }
    }
    compressedBits += runCodeBits(runLength);

    const useCompression = (compressedBits < totalBits) || (totalBits > 65535 * 8);
    const finalBits = useCompression ? compressedBits : totalBits;
    const paddedBits = Math.ceil(finalBits / 160) * 160;
    const paddedBytes = paddedBits / 8;
    if (paddedBytes > 65520) throw new Error('Image is too detailed for one transfer. Reduce dithering or simplify the artwork.');
    const out = new Uint8Array(paddedBytes);

    let bitPos = 0;
    function appendBit(v) {
      if (v) {
        out[bitPos >> 3] |= (0x80 >> (bitPos & 7));
      }
      bitPos++;
    }

    function appendRun(len) {
      let bits = 0;
      for (let v = len; v !== 0; v >>>= 1) bits++;
      for (let z = 1; z < bits; z++) appendBit(false);
      for (let b = bits; b > 0; b--) {
        appendBit((len & (1 << (b - 1))) !== 0);
      }
    }

    if (useCompression) {
      runPixel = bitAt(rawBytes, 0);
      appendBit(runPixel);
      runLength = 1;
      for (let i = 1; i < totalBits; i++) {
        const px = bitAt(rawBytes, i);
        if (px === runPixel) {
          runLength++;
        } else {
          appendRun(runLength);
          runPixel = px;
          runLength = 1;
        }
      }
      appendRun(runLength);
    } else {
      out.set(rawBytes.subarray(0, Math.ceil(totalBits / 8)));
    }

    return {
      bytes: out,
      compression: useCompression ? 2 : 0,
      totalBits: totalBits
    };
  }

  function parseBarcode(barcode) {
    if (!barcode) return { ok: false, error: 'Empty code' };
    barcode = barcode.trim();

    if (!/^[A-Za-z0-9]4[0-9]{15}$/.test(barcode)) {
      return { ok: false, error: 'A complete 17-character IR barcode is required.' };
    }

    let sum = 0;
    for (let i = 0; i < 16; i++) sum += barcode.charCodeAt(i);
    if ((sum % 10) !== parseInt(barcode[16], 10)) {
      return { ok: false, error: 'Barcode checksum mismatch.' };
    }

    const mfg = parseInt(barcode.substring(2, 7), 10);
    const serial = parseInt(barcode.substring(7, 12), 10);
    const typeCode = parseInt(barcode.substring(12, 16), 10);

    const unit = parseInt(barcode.substring(2, 4), 10);
    const week = parseInt(barcode.substring(5, 7), 10);
    if (unit >= 64 || week >= 54 || mfg > 65535 || serial >= 65535) {
      return { ok: false, error: 'Barcode fields are out of range.' };
    }
    const plidVal = ((mfg << 16) | serial) >>> 0;
    const wirePlid = [
      plidVal & 0xff,
      (plidVal >> 8) & 0xff,
      (plidVal >> 16) & 0xff,
      (plidVal >> 24) & 0xff
    ];
    const hexPlid = wirePlid.slice().reverse().map(b => b.toString(16).padStart(2, '0').toUpperCase()).join('');
    const profile = getProfile(typeCode);

    return {
      ok: true,
      barcode: barcode,
      plid: hexPlid,
      typeCode: typeCode,
      wirePlid: wirePlid,
      profile: profile
    };
  }

  function getActiveTag() {
    if (!state.tags.length) return null;
    return state.tags.find(t => t.id === state.activeTagId) || state.tags[0];
  }

  function updateStatusIndicator() {
    const dot = document.getElementById('header-status-dot');
    const txt = document.getElementById('header-status-text');

    if (state.status === 'uploading') {
      dot.className = 'status-dot busy';
      txt.textContent = `UPLOADING ${state.progress}%`;
    } else if (state.status === 'loaded') {
      dot.className = 'status-dot online';
      txt.textContent = 'READY ON CARDPUTER';
    } else if (state.status === 'sending') {
      dot.className = 'status-dot busy';
      txt.textContent = `SENDING ${state.progress}%`;
    } else if (state.status === 'sent') {
      dot.className = 'status-dot online';
      txt.textContent = 'TRANSMITTED';
    } else if (state.status === 'error') {
      dot.className = 'status-dot error';
      txt.textContent = 'TX ERROR';
    } else if (Transport.isConnected()) {
      dot.className = 'status-dot online';
      txt.textContent = (Transport.type === 'ble' ? 'BLE' : 'USB') + ' READY';
    } else {
      dot.className = 'status-dot';
      txt.textContent = 'CONNECT';
    }
  }

  function updateTxHud() {
    const transferBusy = state.status === 'sending' || state.status === 'uploading';
    document.getElementById('transfer-speed').disabled = transferBusy;
    document.getElementById('btn-quick-blink').disabled =
      transferBusy || !Transport.isConnected();
    document.querySelectorAll('.btn-delete-tag').forEach(button => {
      button.disabled = transferBusy;
    });
    const hud = document.getElementById('transmit-hud');
    const sendBtn = document.getElementById('btn-send-esl');
    const fill = document.getElementById('hud-progress-fill');
    const percent = document.getElementById('hud-percent');
    const statusText = document.getElementById('hud-status');

    if (state.status === 'uploading') {
      hud.classList.add('active');
      statusText.textContent = 'PUSHING TO CARDPUTER…';
      percent.textContent = `${state.progress}%`;
      fill.style.width = `${state.progress}%`;
      sendBtn.disabled = true;
      sendBtn.textContent = `UPLOADING (${state.progress}%)…`;
    } else if (state.status === 'loaded') {
      hud.classList.add('active');
      statusText.textContent = 'READY — PRESS ENTER ON CARDPUTER';
      percent.textContent = 'READY';
      fill.style.width = '100%';
      sendBtn.disabled = false;
      sendBtn.innerHTML = '<span>⚡</span> SEND TO ESL (OR ENTER ON CARDPUTER)';
    } else if (state.status === 'sending') {
      hud.classList.add('active');
      statusText.textContent = 'TRANSMITTING OVER INFRARED…';
      percent.textContent = `${state.progress}%`;
      fill.style.width = `${state.progress}%`;
      sendBtn.disabled = true;
      sendBtn.textContent = `SENDING (${state.progress}%)…`;
    } else if (state.status === 'sent') {
      hud.classList.add('active');
      statusText.textContent = 'TRANSMITTED — CHECK TAG DISPLAY';
      percent.textContent = '100%';
      fill.style.width = '100%';
      sendBtn.disabled = false;
      sendBtn.innerHTML = '<span>🔁</span> REPEAT SEND (OR ENTER ON CARDPUTER)';
    } else {
      hud.classList.remove('active');
      sendBtn.disabled = false;
      sendBtn.innerHTML = '<span>📥</span> PUSH TO CARDPUTER';
    }
  }

  function renderTagSelector() {
    const select = document.getElementById('header-target-select');
    const specsBadge = document.getElementById('studio-tag-specs');
    const countBadge = document.getElementById('tab-tag-count');

    countBadge.textContent = String(state.tags.length);
    select.innerHTML = '';

    if (!state.tags.length) {
      select.innerHTML = '<option value="">(No tags saved)</option>';
      specsBadge.textContent = '--';
      return;
    }

    state.tags.forEach(tag => {
      const opt = document.createElement('option');
      opt.value = tag.id;
      opt.textContent = `${tag.name || 'Tag'} [${tag.id}]`;
      if (tag.id === state.activeTagId) opt.selected = true;
      select.appendChild(opt);
    });

    const active = getActiveTag();
    if (active) {
      state.activeTagId = active.id;
      specsBadge.textContent = `${active.width}×${active.height} • ${colorLabel(active.color)}`;
      updateCanvasDimensions(active);
    }
  }

  function renderSavedTags() {
    const container = document.getElementById('saved-tags-list');
    container.textContent = '';

    if (!state.tags.length) {
      const empty = document.createElement('div');
      empty.style.cssText = 'color: var(--text-muted); font-size: 11px; text-align: center; padding: 20px;';
      empty.append('No tags found on Cardputer NVS.', document.createElement('br'),
        'Scan a barcode above to add one.');
      container.appendChild(empty);
      return;
    }

    state.tags.forEach(tag => {
      const item = document.createElement('div');
      item.className = 'tag-item';
      if (tag.id === state.activeTagId) item.classList.add('active');

      const info = document.createElement('div');
      info.className = 'tag-info';
      const title = document.createElement('div');
      title.className = 'tag-title';
      const icon = document.createElement('span');
      icon.textContent = '🏷️';
      const name = document.createElement('span');
      name.textContent = tag.name || 'Unnamed';
      const id = document.createElement('span');
      id.style.cssText = 'font-family: var(--font-mono); font-size: 10px; color: var(--text-muted);';
      id.textContent = `[${tag.id}]`;
      title.append(icon, name, id);
      const meta = document.createElement('div');
      meta.className = 'tag-meta';
      meta.textContent = `${tag.model} • ${tag.width}×${tag.height} • ${colorLabel(tag.color)}`;
      info.append(title, meta);

      const actions = document.createElement('div');
      actions.className = 'tag-actions';
      const select = document.createElement('button');
      select.type = 'button';
      select.className = 'btn btn-secondary btn-sm btn-select-tag';
      select.dataset.id = tag.id;
      select.textContent = 'SELECT';
      const remove = document.createElement('button');
      remove.type = 'button';
      remove.className = 'btn btn-danger btn-sm btn-delete-tag';
      remove.dataset.id = tag.id;
      remove.textContent = '✕';
      remove.disabled = state.status === 'uploading' || state.status === 'sending';
      actions.append(select, remove);
      item.append(info, actions);

      select.addEventListener('click', () => {
        if (state.activeTagId !== tag.id) {
          state.activeTagId = tag.id;
          invalidateArtwork();
        }
        renderTagSelector();
        renderSavedTags();
        switchTab('view-image');
      });

      remove.addEventListener('click', async () => {
        if (state.status === 'uploading' || state.status === 'sending') return;
        if (!confirm(`Remove tag "${tag.name || tag.id}"?`)) return;
        await Transport.sendLine(JSON.stringify({ cmd: 'removeTag', id: tag.id }));
      });

      container.appendChild(item);
    });
  }

  function updateCanvasDimensions(tag) {
    const canvas = document.getElementById('esl-canvas');
    if (!tag) {
      canvas.width = 296;
      canvas.height = 152;
    } else {
      const w = tag.rotate ? tag.height : tag.width;
      const h = tag.rotate ? tag.width : tag.height;
      if (canvas.width !== w || canvas.height !== h) {
        canvas.width = w;
        canvas.height = h;
      }
    }
    renderActiveCanvas();
  }

  function renderActiveCanvas() {
    if (state.viewMode === 'view-plugins') {
      renderPluginCanvas();
    } else {
      renderStudioImage();
    }
  }

  function renderStudioImage() {
    const canvas = document.getElementById('esl-canvas');
    const ctx = canvas.getContext('2d');
    const active = getActiveTag();
    const w = canvas.width;
    const h = canvas.height;

    ctx.fillStyle = '#ffffff';
    ctx.fillRect(0, 0, w, h);

    if (!state.studioImage) {
      ctx.fillStyle = '#000000';
      ctx.fillRect(0, 0, w, h);

      ctx.fillStyle = '#ffffff';
      ctx.font = 'bold 16px monospace';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText('ETAG DISPLAY EDITOR', w / 2, h / 2 - 12);

      ctx.fillStyle = '#888888';
      ctx.font = '11px monospace';
      ctx.fillText(active ? `${active.width}×${active.height} • ${colorLabel(active.color)}` : 'SELECT AN ESL TAG', w / 2, h / 2 + 10);
      return;
    }

    const img = state.studioImage;
    const offCanvas = document.createElement('canvas');
    offCanvas.width = w;
    offCanvas.height = h;
    const offCtx = offCanvas.getContext('2d');

    offCtx.fillStyle = '#ffffff';
    offCtx.fillRect(0, 0, w, h);

    if (state.studioFit === 'cover') {
      const scale = Math.max(w / img.width, h / img.height);
      const sw = img.width * scale;
      const sh = img.height * scale;
      const sx = (w - sw) / 2;
      const sy = (h - sh) / 2;
      offCtx.drawImage(img, sx, sy, sw, sh);
    } else if (state.studioFit === 'contain') {
      const scale = Math.min(w / img.width, h / img.height);
      const sw = img.width * scale;
      const sh = img.height * scale;
      const sx = (w - sw) / 2;
      const sy = (h - sh) / 2;
      offCtx.drawImage(img, sx, sy, sw, sh);
    } else {
      offCtx.drawImage(img, 0, 0, w, h);
    }

    ctx.drawImage(offCanvas, 0, 0);

    ditherFloydSteinberg(ctx, w, h, {
      invert: state.studioInvert,
      contrast: state.studioContrast,
      threshold: state.studioThreshold,
      algo: state.studioDither,
      colorMode: active ? active.color : 0,
      colorPlane: state.studioColorPlane
    });
  }

  function loadCustomImageFile(file) {
    if (!file || !file.type.startsWith('image/')) return;
    const reader = new FileReader();
    reader.onload = (e) => {
      const img = new Image();
      img.onload = () => {
        state.studioImage = img;
        invalidateArtwork();
        renderStudioImage();
      };
      img.src = e.target.result;
    };
    reader.readAsDataURL(file);
  }

  function renderPluginCanvas() {
    const canvas = document.getElementById('esl-canvas');
    const ctx = canvas.getContext('2d');
    const active = getActiveTag();
    const w = canvas.width;
    const h = canvas.height;

    ctx.fillStyle = '#ffffff';
    ctx.fillRect(0, 0, w, h);

    if (state.activePlugin === 'github') {
      drawGithubPlugin(ctx, w, h, active);
    } else if (state.activePlugin === 'crypto') {
      drawCryptoPlugin(ctx, w, h, active);
    } else if (state.activePlugin === 'clock') {
      drawClockPlugin(ctx, w, h, active);
    }
  }

  function drawGithubPlugin(ctx, w, h, active) {
    const username = document.getElementById('gh-username')?.value.trim() || 'torvalds';
    const data = state._ghData || {
      login: username,
      name: 'Developer',
      public_repos: 42,
      followers: 1337
    };

    ctx.fillStyle = '#000000';
    ctx.fillRect(0, 0, w, h);

    ctx.fillStyle = '#ffffff';
    ctx.font = 'bold 16px sans-serif';
    ctx.textAlign = 'left';
    ctx.fillText(data.login, 14, 26);

    ctx.font = '11px monospace';
    ctx.fillStyle = '#888888';
    ctx.fillText(`REPOS: ${data.public_repos}   FOLLOWERS: ${data.followers}`, 14, 46);

    if (active && active.color !== 0) {
      ctx.fillStyle = active.color === 2 ? '#f5c518' : '#ef4444';
      ctx.fillRect(14, 54, w - 28, 2);
    }

    ctx.fillStyle = '#ffffff';
    ctx.font = 'bold 12px monospace';
    ctx.fillText('STATUS: CODING', 14, 76);
  }

  function drawCryptoPlugin(ctx, w, h, active) {
    const data = state._cryptoData || { btc: 65430, eth: 3450, sol: 145 };

    ctx.fillStyle = '#ffffff';
    ctx.fillRect(0, 0, w, h);

    ctx.fillStyle = '#000000';
    ctx.font = 'bold 14px monospace';
    ctx.fillText('BTC: $' + data.btc.toLocaleString(), 12, 28);
    ctx.fillText('ETH: $' + data.eth.toLocaleString(), 12, 54);
    ctx.fillText('SOL: $' + data.sol.toLocaleString(), 12, 80);

    if (active && active.color !== 0) {
      ctx.fillStyle = active.color === 2 ? '#f5c518' : '#ef4444';
      ctx.fillRect(12, 92, w - 24, 3);
    }
  }

  function drawClockPlugin(ctx, w, h, active) {
    const title = document.getElementById('clock-title')?.value.trim() || 'NOTICE';
    const now = new Date();
    const timeStr = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });

    ctx.fillStyle = '#000000';
    ctx.fillRect(0, 0, w, h);

    ctx.fillStyle = '#888888';
    ctx.font = 'bold 11px monospace';
    ctx.fillText(title.toUpperCase(), 14, 22);

    ctx.fillStyle = '#ffffff';
    ctx.font = 'bold 36px monospace';
    ctx.fillText(timeStr, 14, 68);

    if (active && active.color !== 0) {
      ctx.fillStyle = active.color === 2 ? '#f5c518' : '#ef4444';
      ctx.fillRect(14, 78, w - 28, 3);
    }
  }

  async function startCamera() {
    if (state.scannerActive) return;
    const video = document.getElementById('scanner-video');
    const placeholder = document.getElementById('scanner-placeholder');
    const reticle = document.getElementById('scanner-reticle');
    const toggleBtn = document.getElementById('btn-toggle-cam');

    if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
      alert('Camera requires HTTPS origin (GitHub Pages).');
      return;
    }

    try {
      const constraints = {
        video: {
          facingMode: { ideal: 'environment' },
          width: { ideal: 1920 },
          height: { ideal: 1080 }
        }
      };

      const stream = await navigator.mediaDevices.getUserMedia(constraints);
      state.scannerStream = stream;
      video.srcObject = stream;
      await video.play();

      video.style.display = 'block';
      placeholder.style.display = 'none';
      reticle.style.display = 'flex';
      toggleBtn.innerHTML = '<span>⏹️</span> STOP CAMERA';
      state.scannerActive = true;

      if ('BarcodeDetector' in window) {
        try {
          state.scannerDetector = new BarcodeDetector({
            formats: ['code_128', 'code_39', 'ean_13', 'qr_code', 'data_matrix']
          });
        } catch (e) {
          state.scannerDetector = null;
        }
      }

      if (!state.scannerDetector && typeof ZXing !== 'undefined') {
        state.scannerZxing = new ZXing.BrowserMultiFormatReader();
      }

      // Scan loop throttled to 10 fps
      state.scannerTimer = setInterval(async () => {
        if (!state.scannerActive || video.readyState < 2) return;

        if (state.scannerDetector) {
          try {
            const barcodes = await state.scannerDetector.detect(video);
            if (barcodes && barcodes.length > 0) {
              onBarcodeScanned(barcodes[0].rawValue);
            }
          } catch (e) {}
        } else if (state.scannerZxing) {
          try {
            const res = await state.scannerZxing.decodeFromVideoElement(video);
            if (res && res.getText()) {
              onBarcodeScanned(res.getText());
            }
          } catch (e) {}
        }
      }, 100);

    } catch (err) {
      console.error('Camera open failed:', err);
      alert('Camera error: ' + err.message);
      stopCamera();
    }
  }

  function stopCamera() {
    if (state.scannerTimer) {
      clearInterval(state.scannerTimer);
      state.scannerTimer = null;
    }
    if (state.scannerStream) {
      state.scannerStream.getTracks().forEach(t => t.stop());
      state.scannerStream = null;
    }
    const video = document.getElementById('scanner-video');
    const placeholder = document.getElementById('scanner-placeholder');
    const reticle = document.getElementById('scanner-reticle');
    const toggleBtn = document.getElementById('btn-toggle-cam');

    video.style.display = 'none';
    placeholder.style.display = 'block';
    reticle.style.display = 'none';
    toggleBtn.innerHTML = '<span>📸</span> OPEN CAMERA';
    state.scannerActive = false;
  }

  async function scanBarcodeFromFile(file) {
    if (!file) return;
    const img = new Image();
    img.src = URL.createObjectURL(file);
    await img.decode();

    if ('BarcodeDetector' in window) {
      try {
        const detector = new BarcodeDetector({
          formats: ['code_128', 'code_39', 'ean_13', 'qr_code', 'data_matrix']
        });
        const barcodes = await detector.detect(img);
        if (barcodes && barcodes.length > 0) {
          onBarcodeScanned(barcodes[0].rawValue);
          return;
        }
      } catch (e) {}
    }

    if (typeof ZXing !== 'undefined') {
      try {
        const reader = new ZXing.BrowserMultiFormatReader();
        const res = await reader.decodeFromImageElement(img);
        if (res && res.getText()) {
          onBarcodeScanned(res.getText());
          return;
        }
      } catch (e) {}
    }

    alert('No barcode detected in this image. Ensure clear lighting and focus.');
  }

  function onBarcodeScanned(rawCode) {
    rawCode = (rawCode || '').trim();
    const parsed = parseBarcode(rawCode);

    if (!parsed.ok) {
      return;
    }

    try {
      if (navigator.vibrate) navigator.vibrate(60);
      const audioCtx = new (window.AudioContext || window.webkitAudioContext)();
      const osc = audioCtx.createOscillator();
      osc.type = 'sine';
      osc.frequency.setValueAtTime(880, audioCtx.currentTime);
      osc.connect(audioCtx.destination);
      osc.start();
      osc.stop(audioCtx.currentTime + 0.08);
    } catch (e) {}

    state.pendingScannedTag = parsed;

    const resultBox = document.getElementById('scan-result-box');
    resultBox.style.display = 'flex';
    document.getElementById('res-barcode').textContent = parsed.barcode;
    document.getElementById('res-model').textContent = parsed.profile ? parsed.profile.name : 'Standard Graphic';
    document.getElementById('res-plid').textContent = parsed.plid;
    document.getElementById('input-scanned-name').value = parsed.profile ? parsed.profile.name.split(' ')[0] : 'Tag';

    stopCamera();
  }

  async function savePendingTag() {
    if (!state.pendingScannedTag) return;
    const name = document.getElementById('input-scanned-name').value.trim() || 'Tag';
    const tag = state.pendingScannedTag;

    const cmd = {
      cmd: 'saveTag',
      barcode: tag.barcode,
      name: name
    };

    if (!tag.profile) { alert('Unknown model: use manual entry with a display preset, or configure it on the Cardputer.'); return; }
    try { await Transport.request(cmd, 'tagSaved'); }
    catch (error) { alert(error.message); return; }
    document.getElementById('scan-result-box').style.display = 'none';
    state.pendingScannedTag = null;
    alert(`Tag "${name}" saved to Cardputer!`);
  }

  function switchTab(viewId) {
    const previousContentView = state.viewMode === 'view-plugins' ? 'plugins' : 'image';
    const nextContentView = viewId === 'view-plugins' ? 'plugins' : 'image';
    state.viewMode = viewId;
    if (previousContentView !== nextContentView) invalidateArtwork();
    document.querySelectorAll('.view-panel').forEach(p => p.classList.remove('active'));
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));

    const panel = document.getElementById(viewId);
    if (panel) panel.classList.add('active');

    const tabBtn = document.querySelector(`.tab-btn[data-view="${viewId}"]`);
    if (tabBtn) tabBtn.classList.add('active');

    if (viewId !== 'view-tags') {
      stopCamera();
    }
    renderActiveCanvas();
  }

  function setupEvents() {
    document.getElementById('transfer-speed').addEventListener('change', async (event) => {
      const select = event.target;
      const requested = select.value;
      select.disabled = true;
      try {
        if (!Transport.isConnected()) throw new Error('Connect the Cardputer first.');
        await Transport.request({ cmd: 'setSpeed', speed: requested }, 'speedChanged');
      } catch (error) {
        select.value = state.speed;
        alert(error.message);
      } finally {
        select.disabled = state.status === 'sending' || state.status === 'uploading';
      }
    });
    document.getElementById('btn-header-connect').addEventListener('click', async () => {
      if (Transport.isConnected()) {
        await disconnectTransport();
      } else {
        Transport.type === 'ble' ? Transport.connectBle() : Transport.connectSerial();
      }
    });

    document.getElementById('btn-send-esl').addEventListener('click', async () => {
      const active = getActiveTag();
      if (!active || !active.graphic) {
        alert('Please connect to Cardputer and select a saved graphic tag first.');
        return;
      }
      if (!Transport.isConnected()) {
        alert('Connect the Cardputer before uploading artwork.');
        return;
      }
      if (state.status === 'uploading' || state.status === 'sending') return;

      const canvas = document.getElementById('esl-canvas');
      const bitstream = exportEslBitstream(canvas, active);
      const page = state.studioPage;

      if (stagedArtworkMatches(active, page, bitstream)) {
        const staged = state.stagedArtwork;
        const transmissionSession = Transport.session;
        state.status = 'sending';
        state.progress = 0;
        updateStatusIndicator();
        updateTxHud();
        try {
          await Transport.request(buildTransmitCommand(staged), 'artStarted');
        } catch (err) {
          if (Transport.session !== transmissionSession || !Transport.isConnected()) return;
          state.status = 'error';
          updateStatusIndicator();
          updateTxHud();
          alert('Transmission trigger failed: ' + err.message);
        }
        return;
      }

      const uploadIdentity = {
        session: Transport.session,
        id: active.id,
        page,
        revision: state.contentRevision
      };
      state.stagedArtwork = null;
      state.status = 'uploading';
      state.progress = 0;
      updateStatusIndicator();
      updateTxHud();

      try {
        const stage = await Transport.sendArtwork(active.id, page, bitstream, active);
        const current = getActiveTag();
        const currentBits = current && current.graphic
          ? exportEslBitstream(document.getElementById('esl-canvas'), current)
          : null;
        if (Transport.session !== uploadIdentity.session || !Transport.isConnected() ||
            state.contentRevision !== uploadIdentity.revision || !current || current.id !== uploadIdentity.id ||
            state.studioPage !== uploadIdentity.page || !sameBytes(bitstream, currentBits)) {
          state.status = 'idle';
          state.progress = 0;
        } else {
          state.stagedArtwork = {
            ...uploadIdentity,
            stage: stage.stage,
            bitstream: bitstream.slice()
          };
          state.status = 'loaded';
          state.progress = 0;
        }
        updateStatusIndicator();
        updateTxHud();
      } catch (err) {
        if (Transport.session !== uploadIdentity.session || !Transport.isConnected()) return;
        state.status = 'error';
        updateStatusIndicator();
        updateTxHud();
        alert('Upload failed: ' + err.message);
      }
    });

    document.getElementById('header-target-select').addEventListener('change', (e) => {
      if (state.activeTagId !== e.target.value) {
        state.activeTagId = e.target.value;
        invalidateArtwork();
      }
      renderTagSelector();
      renderSavedTags();
    });

    document.getElementById('btn-quick-blink').addEventListener('click', async () => {
      if (state.status === 'uploading' || state.status === 'sending') return;
      const active = getActiveTag();
      if (!active) return;
      await Transport.sendLine(JSON.stringify({ cmd: 'blink', id: active.id }));
    });

    document.getElementById('btn-refresh-tags').addEventListener('click', async () => {
      await Transport.sendLine('{"cmd":"getState"}');
    });

    document.querySelectorAll('.tab-btn').forEach(btn => {
      btn.addEventListener('click', () => switchTab(btn.dataset.view));
    });

    const dropzone = document.getElementById('studio-dropzone');
    const fileInput = document.getElementById('file-studio-input');

    dropzone.addEventListener('click', () => fileInput.click());
    fileInput.addEventListener('change', (e) => {
      if (e.target.files.length) loadCustomImageFile(e.target.files[0]);
    });

    dropzone.addEventListener('dragover', (e) => {
      e.preventDefault();
      dropzone.classList.add('drag-over');
    });
    dropzone.addEventListener('dragleave', () => dropzone.classList.remove('drag-over'));
    dropzone.addEventListener('drop', (e) => {
      e.preventDefault();
      dropzone.classList.remove('drag-over');
      if (e.dataTransfer.files.length) loadCustomImageFile(e.dataTransfer.files[0]);
    });

    window.addEventListener('paste', (e) => {
      const items = e.clipboardData?.items;
      if (!items) return;
      for (const item of items) {
        if (item.type.startsWith('image/')) {
          loadCustomImageFile(item.getAsFile());
          switchTab('view-image');
          break;
        }
      }
    });

    document.querySelectorAll('#group-fit-mode .pill-opt').forEach(btn => {
      btn.addEventListener('click', () => {
        document.querySelectorAll('#group-fit-mode .pill-opt').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        if (state.studioFit !== btn.dataset.fit) {
          state.studioFit = btn.dataset.fit;
          invalidateArtwork();
        }
        renderStudioImage();
      });
    });

    document.querySelectorAll('#group-dither-algo .pill-opt').forEach(btn => {
      btn.addEventListener('click', () => {
        document.querySelectorAll('#group-dither-algo .pill-opt').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        if (state.studioDither !== btn.dataset.algo) {
          state.studioDither = btn.dataset.algo;
          invalidateArtwork();
        }
        renderStudioImage();
      });
    });

    const sliderContrast = document.getElementById('slider-contrast');
    sliderContrast.addEventListener('input', (e) => {
      const contrast = parseInt(e.target.value, 10);
      if (state.studioContrast !== contrast) {
        state.studioContrast = contrast;
        invalidateArtwork();
      }
      document.getElementById('lbl-contrast').textContent = e.target.value;
      renderStudioImage();
    });

    const sliderThreshold = document.getElementById('slider-threshold');
    sliderThreshold.addEventListener('input', (e) => {
      const threshold = parseInt(e.target.value, 10);
      if (state.studioThreshold !== threshold) {
        state.studioThreshold = threshold;
        invalidateArtwork();
      }
      document.getElementById('lbl-threshold').textContent = e.target.value;
      renderStudioImage();
    });

    document.getElementById('chk-invert').addEventListener('change', (e) => {
      if (state.studioInvert !== e.target.checked) {
        state.studioInvert = e.target.checked;
        invalidateArtwork();
      }
      renderStudioImage();
    });
    document.getElementById('chk-color-plane').addEventListener('change', (e) => {
      if (state.studioColorPlane !== e.target.checked) {
        state.studioColorPlane = e.target.checked;
        invalidateArtwork();
      }
      renderStudioImage();
    });

    document.querySelectorAll('#group-target-page .pill-opt').forEach(btn => {
      btn.addEventListener('click', () => {
        document.querySelectorAll('#group-target-page .pill-opt').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        const page = parseInt(btn.dataset.page, 10);
        if (state.studioPage !== page) {
          state.studioPage = page;
          invalidateArtwork();
        }
      });
    });

    document.querySelectorAll('#group-plugin-select .pill-opt').forEach(btn => {
      btn.addEventListener('click', () => {
        document.querySelectorAll('#group-plugin-select .pill-opt').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        if (state.activePlugin !== btn.dataset.plugin) {
          state.activePlugin = btn.dataset.plugin;
          invalidateArtwork();
        }
        renderPluginForm();
        renderPluginCanvas();
      });
    });

    document.getElementById('btn-plugin-render').addEventListener('click', () => {
      invalidateArtwork();
      renderPluginCanvas();
    });

    document.getElementById('btn-toggle-cam').addEventListener('click', () => {
      if (state.scannerActive) stopCamera();
      else startCamera();
    });

    const fileBarcode = document.getElementById('file-barcode-snap');
    document.getElementById('btn-snap-barcode').addEventListener('click', () => fileBarcode.click());
    fileBarcode.addEventListener('change', (e) => {
      if (e.target.files.length) scanBarcodeFromFile(e.target.files[0]);
    });

    document.getElementById('btn-save-scanned').addEventListener('click', savePendingTag);

    document.getElementById('btn-manual-save').addEventListener('click', async () => {
      const code = document.getElementById('manual-code-input').value.trim();
      const name = document.getElementById('manual-name-input').value.trim() || 'Custom Tag';
      const model = parseInt(document.getElementById('manual-model-select').value, 10);

      const parsed = parseBarcode(code);
      if (!parsed.ok) {
        alert('Enter a valid 17-character IR tag barcode, including its checksum.');
        return;
      }

      const cmd = {
        cmd: 'saveTag',
        barcode: parsed.barcode,
        name: name
      };
      if (!parsed.profile) {
        const preset = getProfile(model);
        Object.assign(cmd, { width: preset.width, height: preset.height, color: preset.color });
      }
      try {
        await Transport.request(cmd, 'tagSaved');
        alert(`Tag "${name}" added!`);
      } catch (error) { alert(error.message); }
    });

    document.querySelectorAll('#group-transport .pill-opt').forEach(btn => {
      btn.addEventListener('click', () => {
        document.querySelectorAll('#group-transport .pill-opt').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        if (Transport.isConnected()) {
          alert('Disconnect before changing connection type.');
          document.querySelectorAll('#group-transport .pill-opt').forEach(b =>
            b.classList.toggle('active', b.dataset.transport === Transport.type));
          return;
        }
        Transport.type = btn.dataset.transport;
        Transport.updateUi();
        document.getElementById('btn-connect-active').textContent = `CONNECT VIA ${Transport.type.toUpperCase()}`;
      });
    });

    document.getElementById('btn-connect-active').addEventListener('click', () => {
      if (Transport.type === 'ble') Transport.connectBle();
      else if (Transport.type === 'serial') Transport.connectSerial();
    });

    document.getElementById('btn-disconnect-active').addEventListener('click', async () => {
      await disconnectTransport();
    });
  }

  function renderPluginForm() {
    const container = document.getElementById('plugin-form-slot');
    container.innerHTML = '';

    if (state.activePlugin === 'github') {
      container.innerHTML = `
        <div class="form-group">
          <div class="form-label">GITHUB USERNAME</div>
          <div class="input-row">
            <input type="text" class="input-text" id="gh-username" placeholder="e.g. torvalds" value="torvalds">
            <button class="btn btn-secondary btn-sm" id="btn-gh-fetch">FETCH API</button>
          </div>
        </div>
      `;
      document.getElementById('btn-gh-fetch').addEventListener('click', async () => {
        const u = document.getElementById('gh-username').value.trim();
        if (!u) return;
        try {
          const res = await fetch(`https://api.github.com/users/${encodeURIComponent(u)}`);
          if (!res.ok) throw new Error('User not found');
          state._ghData = await res.json();
          invalidateArtwork();
          renderPluginCanvas();
        } catch (e) {
          alert('GitHub fetch failed: ' + e.message);
        }
      });
    } else if (state.activePlugin === 'crypto') {
      container.innerHTML = `
        <div style="font-size: 11px; color: var(--text-muted);">
          Fetches live CoinGecko spot rates for BTC, ETH, and SOL.
        </div>
        <button class="btn btn-secondary btn-sm" id="btn-crypto-fetch">FETCH MARKET PRICES</button>
      `;
      document.getElementById('btn-crypto-fetch').addEventListener('click', async () => {
        try {
          const res = await fetch('https://api.coingecko.com/api/v3/simple/price?ids=bitcoin,ethereum,solana&vs_currencies=usd');
          const json = await res.json();
          state._cryptoData = {
            btc: json.bitcoin?.usd || 0,
            eth: json.ethereum?.usd || 0,
            sol: json.solana?.usd || 0
          };
          invalidateArtwork();
          renderPluginCanvas();
        } catch (e) {
          alert('Crypto fetch failed: ' + e.message);
        }
      });
    } else if (state.activePlugin === 'clock') {
      container.innerHTML = `
        <div class="form-group">
          <div class="form-label">HEADER TITLE</div>
          <input type="text" class="input-text" id="clock-title" value="WORK NOTICE" maxlength="20">
        </div>
      `;
      document.getElementById('clock-title').addEventListener('input', () => {
        invalidateArtwork();
        renderPluginCanvas();
      });
    }
  }

  function boot() {
    setupEvents();
    renderPluginForm();
    renderTagSelector();
    renderStudioImage();
    Transport.updateUi();

    if (navigator.bluetooth) {
      console.log('Web Bluetooth is supported.');
    }
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', boot);
  } else {
    boot();
  }
})();
