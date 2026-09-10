// SPDX-License-Identifier: GPL-3.0-only
const assert = require('node:assert/strict');
const {Client, normalizeAp, normalizeTag, normalizeTags, hasDimensions, sdJpegBytes} = require('../../web/oepl.js');

const ap = 'http://192.168.1.50';
const tag = {mac: '0000000001020304', alias: 'Desk', hwType: 1, width: 296, height: 128,
  bpp: 2, batteryMv: 2950, rssi: -61, lastseen: 1770000000, pending: 0};
const other = {...tag, mac: '0000000005060708', alias: 'Other'};
const jpeg = '/9j/AAAA'; // JPEG bytes are validated by the bridge; this test exercises browser state.
const deferred = () => { let resolve, reject; const promise = new Promise((a, b) => { resolve = a; reject = b; }); return {promise, resolve, reject}; };
const response = (data, ok = true) => ({ok, json: async () => data});

function baselineJpeg(width = 296, height = 128) {
  // Original constant-gray baseline JPEG: two zero Huffman codes per 8x8 MCU.
  const segment = (marker, data) => Buffer.from([255, marker, (data.length + 2) >> 8, (data.length + 2) & 255, ...data]);
  const bits = 2 * Math.ceil(width / 8) * Math.ceil(height / 8);
  const scan = Buffer.alloc(Math.ceil(bits / 8));
  if (bits % 8) scan[scan.length - 1] = (1 << (8 - bits % 8)) - 1;
  return Buffer.concat([
    Buffer.from([255, 216]), segment(0xDB, [0, ...Array(64).fill(1)]),
    segment(0xC0, [8, height >> 8, height & 255, width >> 8, width & 255, 1, 1, 0x11, 0]),
    segment(0xC4, [0, 1, ...Array(15).fill(0), 0, 0x10, 1, ...Array(15).fill(0), 0]),
    segment(0xDA, [1, 1, 0, 0, 63, 0]), scan, Buffer.from([255, 217])
  ]).toString('base64');
}

assert.equal(normalizeAp('http://AP.local:80/'), 'http://ap.local');
for (const invalid of ['ap.local', 'ftp://ap.local', 'http://user:pass@ap.local', 'http://ap.local/path', 'http://ap.local/?x=y', 'http://ap.local/#x']) {
  assert.throws(() => normalizeAp(invalid));
}
assert(hasDimensions(tag));
for (const invalid of [null, {...tag, width: null}, {...tag, height: 0}, {...tag, width: -1}, {...tag, width: 2049}]) {
  assert.equal(hasDimensions(invalid), false);
}
assert.equal(normalizeTag({...tag, mac: 'abcdef1234567890'}).mac, 'ABCDEF1234567890');
assert.equal(normalizeTag({...tag, alias: null}).alias, '');
for (const invalid of [null, {...tag, mac: 'bad-id'}, {...tag, width: '296'}, {...tag, pending: {}}, {...tag, alias: 17}]) {
  assert.throws(() => normalizeTag(invalid));
}
for (const invalid of [{ap, tags: {}}, {ap, tags: [tag, tag]}, {ap: 'http://different.local', tags: [tag]}, {ap, tags: [null]}]) {
  assert.throws(() => normalizeTags(invalid, ap));
}
assert.deepEqual(normalizeTags({ap, tags: []}, ap), []);

async function ready(fetcher) {
  const client = new Client(async (url, options) => url.endsWith('/tags') ? response({ap, tags: [tag, other]}) : fetcher(url, options));
  assert.equal(await client.load(ap), true);
  assert.equal(client.selected, '', 'never auto-select a destination');
  client.select(tag.mac);
  return client;
}

(async () => {
  const calls = [];
  const client = await ready(async (url, options) => {
    const body = JSON.parse(options.body);
    calls.push({url, options, body});
    if (url.endsWith('/status')) return response({ap, tag: {...tag, pending: 1}});
    return response({ap, mac: body.mac, status: 'submitted', displayConfirmed: false});
  });
  assert.equal(await client.send(async snapshot => {
    assert.equal(snapshot.width, 296);
    assert.equal(snapshot.height, 128);
    return jpeg;
  }, 1), true);
  assert.equal(calls[0].url, '/api/oepl/upload');
  assert.deepEqual(calls[0].body, {ap, mac: tag.mac, image: jpeg, dither: 1});
  assert.equal(calls[0].options.credentials, 'same-origin');
  assert.match(client.message, /not confirmed/);
  assert.equal(client.busy, '');
  assert.equal(await client.refresh(), true);
  assert.equal(client.tag().pending, 1);
  assert.match(client.message, /do not prove/);

  // Local SD export never sends a request, and uses the same selected dimensions.
  const downloads = [], requestCount = calls.length;
  assert.equal(await client.save(async snapshot => baselineJpeg(snapshot.width, snapshot.height),
    (bytes, filename) => downloads.push({bytes, filename})), true);
  assert.equal(calls.length, requestCount);
  assert.equal(downloads[0].filename, 'etag-' + tag.mac + '.jpg');
  assert(downloads[0].bytes instanceof Uint8Array);
  assert.match(client.message, /No image was submitted/);
  assert.equal(client.busy, '');
  assert.throws(() => sdJpegBytes(baselineJpeg(1, 1), 296, 128), /dimensions/);
  const progressive = Buffer.from(baselineJpeg(), 'base64');
  progressive[progressive.indexOf(Buffer.from([255, 192])) + 1] = 194;
  assert.throws(() => sdJpegBytes(progressive.toString('base64'), 296, 128), /baseline/);
  assert.throws(() => sdJpegBytes(Buffer.alloc(512 * 1024 + 1).toString('base64'), 296, 128), /512 KiB/);
  assert.equal(await client.save(() => baselineJpeg(1, 1), () => { throw new Error('Should not download'); }), false);
  assert.equal(downloads.length, 1);
  assert.equal(calls.length, requestCount);

  for (const change of [c => c.select(other.mac), c => c.artworkChanged(), c => c.setAp('http://other.local')]) {
    const exporting = await ready(() => { throw new Error('SD export must not call fetch'); });
    const encoding = deferred();
    let saved = false;
    const saving = exporting.save(() => encoding.promise, () => { saved = true; });
    change(exporting);
    encoding.resolve(baselineJpeg());
    assert.equal(await saving, false);
    assert.equal(saved, false, 'stale destinations and artwork are not downloaded');
  }

  const beforeFailure = calls.length;
  await assert.rejects(client.send(() => jpeg, 2), /dithering/);
  for (const bad of ['data:image/png;base64,AAAA', null, '/9j/' + 'A'.repeat(3000000)]) {
    assert.equal(await client.send(() => bad, 0), false);
  }
  assert.equal(calls.length, beforeFailure, 'invalid images never reach the AP');

  // Destination and artwork edits during asynchronous encoding must not send stale data.
  for (const change of [c => c.select(other.mac), c => c.artworkChanged(), c => c.setAp('http://other.local')]) {
    let sent = 0;
    const target = await ready(async () => { sent++; return response({}); });
    const encoding = deferred();
    const sending = target.send(() => encoding.promise, 1);
    assert.equal(target.busy, 'preparing');
    change(target);
    encoding.resolve(jpeg);
    assert.equal(await sending, false);
    assert.equal(sent, 0);
  }

  // Once POST starts, editing/repeated sending cannot change the physical destination.
  const pendingUpload = deferred();
  let uploadCalls = 0;
  const locked = await ready(async () => { uploadCalls++; return pendingUpload.promise; });
  const sending = locked.send(() => jpeg, 0);
  await Promise.resolve();
  assert.equal(locked.busy, 'uploading');
  assert.throws(() => locked.select(other.mac), /Wait/);
  assert.throws(() => locked.artworkChanged(), /Wait/);
  assert.throws(() => locked.setAp('http://other.local'), /Wait/);
  await assert.rejects(locked.send(() => jpeg, 0), /Wait/);
  pendingUpload.resolve(response({ap, mac: tag.mac, status: 'submitted', displayConfirmed: false}));
  assert.equal(await sending, true);
  assert.equal(uploadCalls, 1);

  // Old reads cannot restore tags or overwrite status after the AP address changes.
  const oldRead = deferred();
  const readClient = new Client(() => oldRead.promise);
  const loading = readClient.load(ap);
  readClient.setAp('http://other.local');
  oldRead.resolve(response({ap, tags: [tag]}));
  assert.equal(await loading, false);
  assert.equal(readClient.tags.length, 0);
  assert.equal(readClient.ap, 'http://other.local');
  assert.equal(readClient.busy, '');

  const staleRead = deferred();
  const statusClient = await ready(() => staleRead.promise);
  const refreshing = statusClient.refresh();
  statusClient.select(other.mac);
  staleRead.resolve(response({ap, tag: {...tag, pending: 9}}));
  assert.equal(await refreshing, false);
  assert.equal(statusClient.tag().mac, other.mac);
  assert.equal(statusClient.tag().pending, 0);

  // Shape/identity failures, AP errors and dropped connections never claim success or retry.
  for (const fetcher of [
    async () => response({ap, mac: other.mac, status: 'submitted', displayConfirmed: false}),
    async () => response({ap, mac: tag.mac, status: 'sent', displayConfirmed: true}),
    async () => response({ap: 'http://other.local', mac: tag.mac, status: 'submitted', displayConfirmed: false}),
    async () => response({error: 'AP rejected image'}, false),
    async () => ({ok: true, json: async () => { throw new Error('html page'); }}),
    async () => { throw new Error('network down'); }
  ]) {
    let count = 0;
    const bad = await ready((...args) => { count++; return fetcher(...args); });
    assert.equal(await bad.send(() => jpeg, 1), false);
    assert.equal(bad.error, true);
    assert.equal(bad.busy, '');
    assert.equal(count, 1);
  }
  for (const data of [{ap, tags: []}, {ap, tags: [{...tag, width: null, height: null}]}]) {
    const unknown = new Client(async () => response(data));
    assert.equal(await unknown.load(ap), true);
    if (unknown.tags.length) unknown.select(tag.mac);
    await assert.rejects(unknown.send(() => jpeg, 1), /dimensions/);
    await assert.rejects(unknown.save(() => baselineJpeg(), () => {}), /dimensions/);
  }
  const missingMetadata = new Client(async () => response({ap, tags: [{...tag, width: null}], metadataError: 'AP file missing'}));
  await missingMetadata.load(ap);
  assert.match(missingMetadata.message, /metadata could not be loaded/);

  // The DOM rendering path treats AP-supplied aliases as literal text, never markup.
  const {boot} = require('../../web/oepl.js');
  class Element {
    constructor() { this.value = ''; this.children = []; this.listeners = {}; this.dataset = {}; this.width = 296; this.height = 128; }
    appendChild(child) { this.children.push(child); }
    append(...children) { this.children.push(...children); }
    replaceChildren() { this.children = []; }
    addEventListener(type, callback) { this.listeners[type] = callback; }
    getContext() { return {fillRect() {}, fillText() {}, drawImage() {}}; }
    set innerHTML(_) { throw new Error('Untrusted HTML insertion'); }
  }
  const elements = new Map();
  const doc = {getElementById(id) { if (!elements.has(id)) elements.set(id, new Element()); return elements.get(id); }, createElement() { return new Element(); }};
  global.fetch = async () => response({ap, tags: [{...tag, alias: '<img src=x onerror=alert(1)>'}]});
  const ui = boot(doc);
  await ui.load(ap);
  assert.equal(elements.get('oepl-tag').children[1].textContent, '<img src=x onerror=alert(1)> · ' + tag.mac);
  assert.equal(elements.get('oepl-send').disabled, true);
  assert.equal(elements.get('oepl-save').disabled, true);
  ui.select(tag.mac);
  assert.equal(elements.get('oepl-send').disabled, true, 'an empty artwork cannot be sent');
  assert.equal(elements.get('oepl-save').disabled, true, 'an empty artwork cannot be saved');
  assert.equal(elements.get('oepl-canvas').width, 296);
  assert.equal(elements.get('oepl-canvas').height, 128);
  ui.tags = [{...tag, width: null}];
  ui.changed();
  assert.equal(elements.get('oepl-send').disabled, true);
  assert.equal(elements.get('oepl-save').disabled, true);
  assert.match(elements.get('oepl-preview-label').textContent, /unknown/);
  console.log('OEPL browser validation, request failures, destination isolation, and DOM safety tests passed.');
})().catch(error => { console.error(error); process.exitCode = 1; });
