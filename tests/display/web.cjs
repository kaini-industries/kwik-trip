// SPDX-License-Identifier: GPL-3.0-only
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync('web/app.js', 'utf8');
const html = fs.readFileSync('web/index.html', 'utf8');
const speedControl = { value: 'reliable' };
const browserConsole = {log: console.log, error: console.error, warn() {}};
const context = vm.createContext({ document: { getElementById: () => speedControl }, Uint8Array, Uint8ClampedArray,
  TextEncoder, TextDecoder, setTimeout, clearTimeout, console: browserConsole,
  updateStatusIndicator() {}, updateTxHud() {}, alert() {}, state: {},
});
vm.runInContext(source.slice(source.indexOf('  const KNOWN_PROFILES ='), source.indexOf('  const state =')), context);
vm.runInContext(source.slice(source.indexOf('  function parseBarcode('), source.indexOf('  function getActiveTag(')), context);
assert.equal(context.parseBarcode('G4591371776312423').plid, 'E7014563');
for (const code of ['DEADBEEF', 'G4591371776312424', 'prefixG4591371776312423', 'G4x91371776312423']) {
  assert.equal(context.parseBarcode(code).ok, false);
}
const withChecksum = body => body + ([...body].reduce((n, c) => n + c.charCodeAt(0), 0) % 10);
assert.equal(context.parseBarcode(withChecksum('G464000177631242')).ok, false);
assert.equal(context.parseBarcode(withChecksum('G459154177631242')).ok, false);
const codecStart = source.indexOf('  function runCodeBits(');
const codecEnd = source.indexOf('  function parseBarcode(');
assert(codecStart > 0 && codecEnd > codecStart);
vm.runInContext(source.slice(codecStart, codecEnd), context);
const encode = context.encodeEslArtwork;
const getBit = (bytes, i) => (bytes[i >> 3] >> (7 - (i & 7))) & 1;
function check(raw, bits) {
  const result = encode(raw, bits);
  assert.equal(result.bytes.length % 20, 0);
  if (!result.compression) {
    for (let i = 0; i < bits; i++) assert.equal(getBit(result.bytes, i), getBit(raw, i));
    return;
  }
  let cursor = 1, output = 0, color = getBit(result.bytes, 0);
  while (output < bits) {
    let width = 1;
    while (!getBit(result.bytes, cursor++)) {
      assert(cursor < result.bytes.length * 8);
      width++;
    }
    let run = 1;
    while (--width) run = run * 2 + getBit(result.bytes, cursor++);
    assert(output + run <= bits);
    while (run--) assert.equal(getBit(raw, output++), color);
    color ^= 1;
  }
}
for (const [w, h] of [[152,152], [208,112], [400,300]]) {
  const bits = w * h * 2;
  for (const value of [0, 255, 170, 85]) {
    const raw = new Uint8Array(bits / 8).fill(value);
    raw[raw.length - 1] ^= 1;
    check(raw, bits);
  }
}
for (let n = 1; n <= 10; n++) {
  for (let pattern = 0; pattern < 2 ** n; pattern++) {
    const raw = new Uint8Array(Math.ceil(n / 8));
    for (let i = 0; i < n; i++) if ((pattern >> i) & 1) raw[i >> 3] |= 128 >> (i & 7);
    check(raw, n);
  }
}
assert.throws(() => encode(new Uint8Array(96000).fill(0xaa), 768000), /too detailed/);

const transportStart = source.indexOf('  const Transport = {');
const transportEnd = source.indexOf('\n  };', transportStart) + 5;
vm.runInContext(source.slice(transportStart, transportEnd) + '\nglobalThis.transport = Transport;', context);
const validationStart = source.indexOf('  const TRANSFER_STATUSES');
const validationEnd = source.indexOf('  function ditherFloydSteinberg(');
assert(validationStart > transportEnd && validationEnd > validationStart);
vm.runInContext(source.slice(validationStart, validationEnd), context);

const validTag = {
  id: 'e7014563', name: 'Kitchen', barcode: 'G4591371776312423', model: 'Continuum E4 HCN FZ',
  width: 208, height: 112, rotate: false, color: 2, graphic: true, page: 1,
  transport: 'ir_pp16', firmwareRead: false, firmwareWrite: false, acknowledged: false
};
const validState = {
  device: 'etag Cardputer Advance', status: 'idle', progress: 0, speed: 'reliable', tags: [validTag]
};
assert.equal(context.normalizeDeviceState(validState).tags[0].id, 'E7014563');
const refreshedUpload = context.normalizeDeviceState({...validState, status: 'uploading', progress: 37});
assert.equal(refreshedUpload.status, 'uploading');
assert.equal(refreshedUpload.progress, 37);
context.state.status = 'uploading';
context.state.progress = 12;
context.applyDeviceTransferStatus(refreshedUpload);
assert.equal(context.state.status, 'uploading');
assert.equal(context.state.progress, 37);
assert.deepEqual(JSON.parse(JSON.stringify(context.normalizeDeviceState({
  ...validState, status: 'loaded', id: 'e7014563', page: 1, stage: 12345
}).stage)), {id: 'E7014563', page: 1, stage: 12345});
assert.equal(context.normalizeDeviceState({...validState, tags: [{...validTag, id: '<img onerror=alert(1)>'}]}), null);
assert.equal(context.normalizeDeviceState({...validState, tags: [{...validTag, width: 1000000}]}), null);
assert.equal(context.normalizeDeviceState({...validState, tags: [{...validTag, color: 9}]}), null);
assert.equal(context.normalizeDeviceState({...validState, tags: Array(10).fill(validTag)}), null);
assert.equal(context.normalizeDeviceState({...validState, tags: [validTag, validTag]}), null);
context.state.tags = [];
context.transport.handleMessage({
  event: 'state',
  data: {...validState, tags: [{...validTag, id: '<img onerror=alert(1)>'}]}
});
assert.deepEqual(context.state.tags, []);
assert.equal(context.state.status, 'error');
const savedTagsSource = source.slice(source.indexOf('  function renderSavedTags('), source.indexOf('  function updateCanvasDimensions('));
assert(!savedTagsSource.includes('item.innerHTML'));
assert(!savedTagsSource.includes('data-id="${'));
assert.match(html, /Content-Security-Policy/);
assert.match(html, /script-src 'self'/);

const ditherStart = source.indexOf('  function ditherFloydSteinberg(');
const ditherEnd = source.indexOf('  function runCodeBits(');
vm.runInContext(source.slice(ditherStart, ditherEnd), context);

function ditherPixel(rgb, colorMode, algo, invert = false) {
  const data = new Uint8ClampedArray([...rgb, 255]);
  const ctx = {
    getImageData: () => ({ data }),
    putImageData: image => { assert.equal(image.data, data); }
  };
  context.ditherFloydSteinberg(ctx, 1, 1, {
    colorMode, colorPlane: true, algo, threshold: 128, contrast: 0, invert
  });
  return [...data.subarray(0, 3)];
}

for (const algo of ['threshold', 'floyd']) {
  assert.deepEqual(ditherPixel([255, 255, 0], 2, algo), [245, 197, 24]);
  assert.deepEqual(ditherPixel([255, 255, 0], 3, algo), [245, 197, 24]);
  assert.deepEqual(ditherPixel([255, 0, 0], 1, algo), [239, 68, 68]);
  assert.deepEqual(ditherPixel([255, 0, 0], 3, algo, true), [239, 68, 68]);
  assert.equal(context.classifyPixel(...ditherPixel([255, 0, 0], 2, algo), 2) < 2, true);
}
assert.equal(context.classifyPixel(255, 255, 0, 2), 2);
assert.equal(context.classifyPixel(255, 255, 0, 3), 3);
assert.equal(context.classifyPixel(255, 255, 0, 1) < 2, true);
assert.equal(context.classifyPixel(255, 0, 0, 1), 2);
assert.equal(context.classifyPixel(255, 0, 0, 2) < 2, true);

function exportPixel(rgb, color) {
  const data = new Uint8ClampedArray([...rgb, 255]);
  const canvas = {
    width: 1, height: 1,
    getContext: () => ({ getImageData: () => ({ data }) })
  };
  return context.exportEslBitstream(canvas, {width: 1, height: 1, rotate: false, color});
}
assert.deepEqual([getBit(exportPixel([255, 255, 0], 2), 0), getBit(exportPixel([255, 255, 0], 2), 1)], [1, 0]);
assert.deepEqual([getBit(exportPixel([255, 0, 0], 3), 0), getBit(exportPixel([255, 0, 0], 3), 1)], [1, 0]);
assert.deepEqual([getBit(exportPixel([255, 255, 0], 3), 0), getBit(exportPixel([255, 255, 0], 3), 1)], [0, 0]);

(async () => {
  const transport = context.transport;
  const originalSend = transport.sendLine;
  const order = [];
  transport.session = 1;
  transport.connected = true;
  transport.stopping = false;
  transport.writeLine = async value => {
    order.push(value + '-start');
    await new Promise(resolve => setTimeout(resolve, 1));
    order.push(value + '-end');
  };
  await Promise.all([originalSend.call(transport, 'one'), originalSend.call(transport, 'two')]);
  assert.deepEqual(order, ['one-start', 'one-end', 'two-start', 'two-end']);

  transport.handleMessage({event: 'speedChanged', data: {speed: 'fast'}});
  assert.equal(speedControl.value, 'fast');
  transport.handleMessage({event: 'speedChanged', data: {speed: 'reliable'}});
  assert.equal(speedControl.value, 'reliable');
  transport.sendLine = async () => {
    transport.handleMessage({event: 'chunkAck', data: {received: 240}});
  };
  assert.equal((await transport.request({cmd: 'artChunk'}, 'chunkAck')).received, 240);
  assert.equal(transport.pendingReply, null);
  transport.sendLine = async () => {
    transport.handleMessage({event: 'error', message: 'bad offset'});
  };
  await assert.rejects(transport.request({}, 'chunkAck'), /bad offset/);
  assert.equal(transport.pendingReply, null);
  transport.sendLine = async () => { throw new Error('disconnected'); };
  await assert.rejects(transport.request({}, 'artReady'), /disconnected/);
  assert.equal(transport.pendingReply, null);

  transport.sendLine = originalSend;
  transport.session = 7;
  transport.connected = true;
  transport.stopping = false;
  const bits = new Uint8Array([0xaa, 0x55]);
  context.state.contentRevision = 3;
  context.state.stagedArtwork = {
    session: 7, id: 'E7014563', page: 1, revision: 3, stage: 12345, bitstream: bits.slice()
  };
  assert(context.stagedArtworkMatches({...validTag, id: 'E7014563'}, 1, bits));
  assert(!context.stagedArtworkMatches({...validTag, id: '01020304'}, 1, bits));
  assert(!context.stagedArtworkMatches({...validTag, id: 'E7014563'}, 2, bits));
  assert(!context.stagedArtworkMatches({...validTag, id: 'E7014563'}, 1, new Uint8Array([0xaa, 0x54])));
  assert(context.stageReferenceMatches({id: 'E7014563', page: 1, stage: 12345}));
  context.state.status = 'loaded';
  context.applyDeviceTransferStatus({status: 'sent', progress: 100,
    stage: {id: 'E7014563', page: 1, stage: 12345}});
  assert.equal(context.state.status, 'sent');
  context.applyDeviceTransferStatus({status: 'sent', progress: 100,
    stage: {id: '01020304', page: 1, stage: 999}});
  assert.equal(context.state.status, 'idle');
  assert.equal(JSON.stringify(context.buildTransmitCommand(context.state.stagedArtwork)),
    '{"cmd":"transmit","id":"E7014563","page":1,"stage":12345}');
  assert.throws(() => context.buildTransmitCommand({id: 'E7014563', page: 1, stage: 0}), /No valid staged/);
  context.invalidateArtwork();
  assert.equal(context.state.stagedArtwork, null);
  assert.equal(context.state.contentRevision, 4);
  context.state.stagedArtwork = {
    session: 7, id: 'E7014563', page: 1, revision: 4, stage: 12345, bitstream: bits.slice()
  };
  transport.session++;
  assert(!context.stageReferenceMatches({id: 'E7014563', page: 1, stage: 12345}));

  const activeReadable = new ReadableStream();
  let activeCloseCount = 0;
  const activePort = {
    readable: activeReadable,
    writable: null,
    async close() {
      assert.equal(activeReadable.locked, false, 'serial stream must be unlocked before close');
      activeCloseCount++;
    }
  };
  const serialTransport = Object.assign({}, transport, {
    type: 'serial', session: 20, connected: true, stopping: false,
    serialPort: activePort, serialReader: null, serialWriter: null,
    serialReadTask: null, sendQueue: Promise.resolve(), pendingReply: null,
    updateUi() {}, handleIncoming() {}
  });
  const activeReadTask = serialTransport.readSerialLoop(activePort, 20);
  serialTransport.serialReadTask = activeReadTask;
  await new Promise(resolve => setTimeout(resolve, 0));
  assert(activeReadable.locked);
  await serialTransport.disconnect();
  assert.equal(activeCloseCount, 1);
  assert.equal(activeReadable.locked, false);
  assert.equal(serialTransport.serialPort, null);

  const textBytes = new TextEncoder().encode('{"name":"Café"}\n');
  let streamController;
  const splitReadable = new ReadableStream({start(controller) { streamController = controller; }});
  const splitPort = {readable: splitReadable, async close() { assert.equal(splitReadable.locked, false); }};
  const decoded = [];
  const splitTransport = Object.assign({}, transport, {
    session: 30, connected: true, stopping: false, serialPort: splitPort,
    serialReader: null, serialReadTask: null, updateUi() {},
    handleIncoming(chunk) { decoded.push(chunk); }
  });
  const splitTask = splitTransport.readSerialLoop(splitPort, 30);
  streamController.enqueue(textBytes.subarray(0, 13));
  streamController.enqueue(textBytes.subarray(13, 14));
  streamController.enqueue(textBytes.subarray(14));
  streamController.close();
  await splitTask;
  assert.equal(decoded.join(''), '{"name":"Café"}\n');

  let staleController;
  const staleReadable = new ReadableStream({start(controller) { staleController = controller; }});
  const staleChunks = [];
  const staleTransport = Object.assign({}, transport, {
    session: 40, connected: true, stopping: false,
    serialReader: null, serialReadTask: null,
    updateUi() {}, handleIncoming(chunk) { staleChunks.push(chunk); }
  });
  const staleTask = staleTransport.readSerialLoop({readable: staleReadable}, 40);
  await new Promise(resolve => setTimeout(resolve, 0));
  staleTransport.session = 41;
  staleController.enqueue(new TextEncoder().encode('{"event":"state"}\n'));
  staleController.close();
  await staleTask;
  assert.deepEqual(staleChunks, [], 'old serial sessions must not process late bytes');
  assert.equal(staleReadable.locked, false);

  console.log('Browser codec, palette, validation, identity, and transport checks passed');
})().catch(error => { console.error(error); process.exitCode = 1; });
