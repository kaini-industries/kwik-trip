// SPDX-License-Identifier: GPL-3.0-only
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync('web/app.js', 'utf8');
const speedControl = { value: 'reliable' };
const context = vm.createContext({ document: { getElementById: () => speedControl }, Uint8Array, TextEncoder, setTimeout, clearTimeout,
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
(async () => {
  const transport = context.transport;
  const originalSend = transport.sendLine;
  const order = [];
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
  console.log('Browser codec and upload acknowledgement checks passed');
})().catch(error => { console.error(error); process.exitCode = 1; });
