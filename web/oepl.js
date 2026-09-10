// SPDX-License-Identifier: GPL-3.0-only
// OpenEPaperLink HTTP integration. This is independent of the PP4/PP16 transport.
(function (root) {
  'use strict';
  const MAX_JPEG_BYTES = 2 * 1024 * 1024;
  const MAX_SD_JPEG_BYTES = 512 * 1024;
  const MAX_PIXELS = 2048 * 2048;

  function normalizeAp(value) {
    let url;
    try { url = new URL(value); } catch (_) { throw new Error('Enter an AP address such as http://192.168.1.50.'); }
    if (!['http:', 'https:'].includes(url.protocol) || !url.hostname || url.username || url.password ||
        url.search || url.hash || url.pathname !== '/') {
      throw new Error('Use an http:// or https:// AP address without a path, credentials, or query.');
    }
    return url.origin;
  }

  function normalizeTag(value) {
    if (!value || typeof value !== 'object' || typeof value.mac !== 'string' ||
        !/^[0-9a-f]{16}$/i.test(value.mac) ||
        (value.alias != null && (typeof value.alias !== 'string' || value.alias.length > 512))) {
      throw new Error('The bridge returned an invalid tag record.');
    }
    const tag = {mac: value.mac.toUpperCase(), alias: value.alias || ''};
    for (const key of ['hwType', 'width', 'height', 'bpp', 'batteryMv', 'rssi', 'lastseen', 'pending']) {
      if (value[key] != null && !Number.isSafeInteger(value[key])) throw new Error('The bridge returned invalid tag metadata.');
      tag[key] = value[key] == null ? null : value[key];
    }
    return tag;
  }

  function hasDimensions(tag) {
    return !!tag && Number.isInteger(tag.width) && Number.isInteger(tag.height) &&
      tag.width > 0 && tag.height > 0 && tag.width <= 2048 && tag.height <= 2048 &&
      tag.width * tag.height <= MAX_PIXELS;
  }

  function validateResponseAp(data, ap) {
    if (!data || typeof data !== 'object' || typeof data.ap !== 'string' || normalizeAp(data.ap) !== ap) {
      throw new Error('The bridge response does not match the requested access point.');
    }
  }

  function normalizeTags(data, ap) {
    validateResponseAp(data, ap);
    if (!Array.isArray(data.tags) || data.tags.length > 10000) throw new Error('The bridge returned an invalid tag list.');
    const tags = data.tags.map(normalizeTag);
    if (new Set(tags.map(tag => tag.mac)).size !== tags.length) throw new Error('The bridge returned duplicate tag IDs.');
    return tags;
  }

  // Inspect the browser-generated JPEG header before exporting for the Cardputer.
  // Pixel decoding remains the responsibility of the display/AP JPEG decoder.
  function sdJpegBytes(image, width, height) {
    if (typeof image !== 'string' || image.length > Math.ceil(MAX_SD_JPEG_BYTES / 3) * 4) {
      throw new Error('This JPEG exceeds the Cardputer limit of 512 KiB. Use simpler artwork or Send image from Studio.');
    }
    let bytes;
    try { bytes = Uint8Array.from(root.atob(image), char => char.charCodeAt(0)); }
    catch (_) { throw new Error('Could not read the prepared JPEG.'); }
    if (bytes.length > MAX_SD_JPEG_BYTES) throw new Error('This JPEG exceeds the Cardputer limit of 512 KiB. Use simpler artwork or Send image from Studio.');
    if (bytes[0] !== 255 || bytes[1] !== 216 || bytes[bytes.length - 2] !== 255 || bytes[bytes.length - 1] !== 217) {
      throw new Error('The browser did not produce a complete JPEG.');
    }
    let offset = 2, baseline = false;
    while (offset + 4 < bytes.length) {
      if (bytes[offset++] !== 255) break;
      while (bytes[offset] === 255) offset++;
      const marker = bytes[offset++];
      const length = bytes[offset] * 256 + bytes[offset + 1];
      if (length < 2 || offset + length > bytes.length) break;
      if (marker === 0xC0) {
        if (baseline || length < 11 || bytes[offset + 2] !== 8) break;
        const h = bytes[offset + 3] * 256 + bytes[offset + 4], w = bytes[offset + 5] * 256 + bytes[offset + 6];
        if (w !== width || h !== height) throw new Error('JPEG dimensions do not match the selected tag. Review the preview again.');
        baseline = true;
      } else if (marker >= 0xC1 && marker <= 0xCF && ![0xC4, 0xC8, 0xCC].includes(marker)) {
        throw new Error('The Cardputer requires baseline JPEG. This browser produced another JPEG format.');
      } else if (marker === 0xDA) {
        if (baseline && offset + length < bytes.length - 2) return bytes;
        break;
      }
      offset += length;
    }
    throw new Error('The browser did not produce a supported baseline JPEG.');
  }

  class Client {
    constructor(fetcher, onChange = () => {}) {
      this.fetcher = fetcher;
      this.onChange = onChange;
      this.ap = '';
      this.tags = [];
      this.selected = '';
      this.session = 0;
      this.revision = 0;
      this.busy = '';
      this.pending = null;
      this.message = 'Load tags from your OpenEPaperLink access point to begin.';
      this.error = false;
    }

    changed() { this.onChange(this); }
    tag() { return this.tags.find(tag => tag.mac === this.selected) || null; }
    note(message, error = false) { this.message = message; this.error = error; this.changed(); }
    requireEditable() {
      if (this.busy === 'uploading') throw new Error('Wait for the current upload before changing its destination or artwork.');
    }
    setAp(value) {
      this.requireEditable();
      if (value === this.ap) return;
      this.session++;
      this.revision++;
      if (this.pending) this.pending.abort();
      this.pending = null;
      this.busy = '';
      this.ap = value;
      this.tags = [];
      this.selected = '';
      this.note('Address changed. Load tags from this access point.');
    }
    select(mac) {
      this.requireEditable();
      if (mac && !this.tags.some(tag => tag.mac === mac)) throw new Error('Choose a tag reported by this access point.');
      if (mac === this.selected) return;
      this.selected = mac;
      this.revision++;
      this.note(mac ? 'Destination selected. Prepare and review the artwork before sending.' : 'Choose a destination tag.');
    }
    artworkChanged() {
      this.requireEditable();
      this.revision++;
      this.changed();
    }
    begin(kind, message) {
      if (this.busy) throw new Error('Wait for the current request to finish.');
      const controller = new AbortController();
      this.pending = controller;
      this.busy = kind;
      this.note(message);
      return controller;
    }
    finish(controller) {
      if (this.pending !== controller) return;
      this.pending = null;
      this.busy = '';
      this.changed();
    }
    async request(action, body, controller) {
      let response;
      try {
        response = await this.fetcher('/api/oepl/' + action, {
          method: 'POST', headers: {'Content-Type': 'application/json'},
          credentials: 'same-origin', body: JSON.stringify(body), signal: controller.signal
        });
      } catch (error) {
        if (error.name === 'AbortError') throw error;
        throw new Error(action === 'upload'
          ? 'Connection lost during upload; its outcome is unknown. Refresh this tag before deciding whether to send again.'
          : 'Cannot reach the local Studio bridge. Start the editor with make studio.');
      }
      let data;
      try { data = await response.json(); }
      catch (_) { throw new Error('Expected JSON from the local bridge. Start this editor with make studio.'); }
      if (!response.ok) throw new Error(typeof data?.error === 'string' ? data.error : 'The bridge rejected the request.');
      return data;
    }
    async load(value) {
      this.requireEditable();
      if (this.busy) throw new Error('Wait for the current request to finish.');
      const ap = normalizeAp(value);
      this.setAp(ap);
      // A re-load deliberately clears selection; the user must choose the destination again.
      this.tags = [];
      this.selected = '';
      this.session++;
      this.revision++;
      const session = this.session;
      const controller = this.begin('loading', 'Reading tags from the access point…');
      try {
        const data = await this.request('tags', {ap}, controller);
        if (session !== this.session) return false;
        this.tags = normalizeTags(data, ap);
        this.note(this.tags.length ? `${this.tags.length} tag${this.tags.length === 1 ? '' : 's'} found. Choose a destination.`
          : 'This AP reports no tags. Pair a compatible tag with OpenEPaperLink, then load tags again.');
        if (typeof data.metadataError === 'string' && data.metadataError) {
          this.note(this.message + '\nDisplay metadata could not be loaded. Tags with unknown dimensions cannot receive images here.');
        }
        return true;
      } catch (error) {
        if (session === this.session) this.note(error.message, true);
        return false;
      } finally { this.finish(controller); }
    }
    async refresh() {
      const tag = this.tag();
      if (!tag) throw new Error('Choose a destination tag first.');
      const ap = normalizeAp(this.ap), mac = tag.mac, session = this.session;
      const controller = this.begin('refreshing', 'Reading the latest AP-reported tag status…');
      try {
        const data = await this.request('status', {ap, mac}, controller);
        if (session !== this.session || this.selected !== mac) return false;
        validateResponseAp(data, ap);
        const latest = normalizeTag(data.tag);
        if (latest.mac !== mac) throw new Error('The bridge returned status for a different tag.');
        if (latest.width !== tag.width || latest.height !== tag.height || latest.bpp !== tag.bpp) this.revision++;
        this.tags = this.tags.map(item => item.mac === mac ? latest : item);
        this.note('Status refreshed from the AP. Last seen and pending work do not prove that this artwork is on the display.');
        return true;
      } catch (error) {
        if (session === this.session && this.selected === mac) this.note(error.message, true);
        return false;
      } finally { this.finish(controller); }
    }
    identity() {
      const tag = this.tag();
      return {ap: normalizeAp(this.ap), mac: tag?.mac, width: tag?.width, height: tag?.height,
        session: this.session, revision: this.revision};
    }
    matches(snapshot) {
      const tag = this.tag();
      return this.ap === snapshot.ap && this.session === snapshot.session && this.revision === snapshot.revision &&
        tag?.mac === snapshot.mac && tag?.width === snapshot.width && tag?.height === snapshot.height;
    }
    async send(encodeImage, dither) {
      if (!hasDimensions(this.tag())) throw new Error('This tag needs known, supported display dimensions before sending.');
      if (dither !== 0 && dither !== 1) throw new Error('Choose a valid dithering setting.');
      const snapshot = this.identity();
      const controller = this.begin('preparing', 'Preparing the previewed image…');
      try {
        const image = await encodeImage(snapshot);
        if (!this.matches(snapshot)) throw new Error('Destination or artwork changed. Review the preview before sending.');
        if (typeof image !== 'string' || !/^\/9j\/[A-Za-z0-9+/]*={0,2}$/.test(image) || image.length > Math.ceil(MAX_JPEG_BYTES / 3) * 4) {
          throw new Error('The prepared image must be a JPEG no larger than 2 MiB.');
        }
        this.busy = 'uploading';
        this.note(`Uploading to ${snapshot.mac}…`);
        const data = await this.request('upload', {ap: snapshot.ap, mac: snapshot.mac, image, dither}, controller);
        if (!this.matches(snapshot)) return false;
        validateResponseAp(data, snapshot.ap);
        if (data.mac !== snapshot.mac || data.status !== 'submitted' || data.displayConfirmed !== false) {
          throw new Error('Unexpected upload response. The display update is unconfirmed; refresh tag status before sending again.');
        }
        this.note(`Submitted to the AP for ${snapshot.mac}. Display refresh is not confirmed. Use Refresh tag status to inspect pending work.`);
        return true;
      } catch (error) {
        if (snapshot.session === this.session) this.note(error.message, true);
        return false;
      } finally { this.finish(controller); }
    }
    async save(encodeImage, download) {
      if (!hasDimensions(this.tag())) throw new Error('This tag needs known, supported display dimensions before saving.');
      const snapshot = this.identity();
      const controller = this.begin('exporting', 'Preparing the previewed JPEG for Cardputer SD…');
      try {
        const image = await encodeImage(snapshot);
        if (!this.matches(snapshot)) throw new Error('Destination or artwork changed. Review the preview before saving.');
        const bytes = sdJpegBytes(image, snapshot.width, snapshot.height);
        const filename = `etag-${snapshot.mac}.jpg`;
        download(bytes, filename);
        this.note(`JPEG download started: ${filename}. Copy it to /etag/images on your Cardputer SD card. No image was submitted to the AP.`);
        return true;
      } catch (error) {
        if (snapshot.session === this.session) this.note(error.message, true);
        return false;
      } finally { this.finish(controller); }
    }
  }

  function canvasJpeg(canvas) {
    return new Promise((resolve, reject) => {
      canvas.toBlob(blob => {
        if (!blob || blob.type !== 'image/jpeg') return reject(new Error('This browser could not encode a JPEG image.'));
        if (blob.size > MAX_JPEG_BYTES) return reject(new Error('The image exceeds the 2 MiB upload limit.'));
        const reader = new FileReader();
        reader.onerror = () => reject(new Error('Could not read the prepared image.'));
        reader.onload = () => resolve(String(reader.result).split(',')[1]);
        reader.readAsDataURL(blob);
      }, 'image/jpeg', 0.9);
    });
  }

  function boot(document) {
    const $ = id => document.getElementById(id);
    if (!$('view-oepl')) return;
    let artwork = null, artworkId = 0, loadingImage = false, imageRequest = 0, previewKey = '';
    const canvas = $('oepl-canvas');
    const client = new Client(root.fetch.bind(root), render);

    function run(operation) {
      Promise.resolve().then(operation).catch(error => client.note(error.message, true));
    }
    function renderPreview(tag) {
      const key = [tag?.mac, tag?.width, tag?.height, artworkId, $('oepl-fit').value].join(':');
      if (key === previewKey) return;
      previewKey = key;
      canvas.width = hasDimensions(tag) ? tag.width : 296;
      canvas.height = hasDimensions(tag) ? tag.height : 128;
      const ctx = canvas.getContext('2d'), w = canvas.width, h = canvas.height;
      ctx.fillStyle = '#ffffff';
      ctx.fillRect(0, 0, w, h);
      if (!artwork || !hasDimensions(tag)) {
        ctx.fillStyle = '#444444';
        ctx.font = '14px sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText(hasDimensions(tag) ? 'Choose artwork or write a message' : 'Choose a tag with known dimensions', w / 2, h / 2, w - 16);
      } else if (artwork.text !== undefined) {
        const lines = artwork.text.split('\n');
        let size = Math.max(8, Math.min(64, Math.floor(h / (lines.length * 1.3 + 1))));
        ctx.font = `bold ${size}px sans-serif`;
        while (size > 5 && lines.some(line => ctx.measureText(line).width > w - 16)) {
          ctx.font = `bold ${--size}px sans-serif`;
        }
        ctx.fillStyle = '#000000';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        lines.forEach((line, i) => ctx.fillText(line, w / 2, h / 2 + (i - (lines.length - 1) / 2) * size * 1.3, Math.max(1, w - 16)));
      } else {
        const image = artwork.image, fit = $('oepl-fit').value;
        const scale = fit === 'cover' ? Math.max(w / image.width, h / image.height) : Math.min(w / image.width, h / image.height);
        const dw = fit === 'stretch' ? w : image.width * scale, dh = fit === 'stretch' ? h : image.height * scale;
        ctx.drawImage(image, (w - dw) / 2, (h - dh) / 2, dw, dh);
      }
      $('oepl-preview-label').textContent = hasDimensions(tag)
        ? `${w} × ${h} pixels · ${artwork ? 'JPEG preview ready for ' + tag.mac : 'Choose artwork to continue'}`
        : 'Display dimensions are unknown or unsupported. Image upload is disabled.';
    }

    function render() {
      const tag = client.tag(), locked = !!client.busy || loadingImage;
      const select = $('oepl-tag');
      const selection = client.selected;
      select.replaceChildren();
      const empty = document.createElement('option');
      empty.value = '';
      empty.textContent = client.tags.length ? 'Choose a destination tag' : 'Load tags to choose a destination';
      select.appendChild(empty);
      client.tags.forEach(item => {
        const option = document.createElement('option');
        option.value = item.mac;
        option.textContent = (item.alias ? item.alias + ' · ' : '') + item.mac;
        select.appendChild(option);
      });
      select.value = selection;
      select.disabled = locked || !client.tags.length;
      $('oepl-ap').disabled = client.busy === 'uploading';
      $('oepl-connect').disabled = locked;
      $('oepl-refresh').disabled = locked || !tag;
      for (const id of ['oepl-copy', 'oepl-choose', 'oepl-file', 'oepl-text', 'oepl-use-text', 'oepl-fit', 'oepl-dither']) {
        $(id).disabled = locked;
      }
      $('oepl-send').disabled = locked || !artwork || !hasDimensions(tag);
      $('oepl-save').disabled = locked || !artwork || !hasDimensions(tag);
      $('oepl-progress').hidden = !locked;
      $('oepl-status').textContent = client.message;
      $('oepl-status').dataset.error = String(client.error);
      const details = $('oepl-details');
      details.replaceChildren();
      if (tag) {
        const seen = tag.lastseen && tag.lastseen > 0 ? new Date(tag.lastseen * 1000) : null;
        const rows = [
          ['Display', hasDimensions(tag) ? `${tag.width} × ${tag.height} pixels` : 'Unknown / unsupported'],
          ['Hardware type', tag.hwType == null ? 'Unknown' : String(tag.hwType)],
          ['Battery', tag.batteryMv == null ? 'Unknown' : `${tag.batteryMv} mV`],
          ['Signal', tag.rssi == null ? 'Unknown' : `${tag.rssi} dBm`],
          ['Last seen by AP', seen && Number.isFinite(seen.getTime()) ? seen.toLocaleString() : 'Unknown'],
          ['Pending work', tag.pending == null ? 'Unknown' : String(tag.pending)]
        ];
        rows.forEach(([label, value]) => {
          const item = document.createElement('div'), dt = document.createElement('dt'), dd = document.createElement('dd');
          dt.textContent = label;
          dd.textContent = value;
          item.append(dt, dd);
          details.appendChild(item);
        });
      }
      renderPreview(tag);
    }

    function setArtwork(value) {
      client.requireEditable();
      artwork = value;
      artworkId++;
      client.artworkChanged();
      client.note('Artwork ready. Check the destination and preview, then Send image.');
    }
    $('oepl-ap').addEventListener('input', () => run(() => client.setAp($('oepl-ap').value.trim())));
    $('oepl-connect').addEventListener('click', () => run(() => client.load($('oepl-ap').value.trim())));
    $('oepl-tag').addEventListener('change', () => run(() => client.select($('oepl-tag').value)));
    $('oepl-refresh').addEventListener('click', () => run(() => client.refresh()));
    $('oepl-copy').addEventListener('click', () => run(() => {
      const source = $('esl-canvas'), copy = document.createElement('canvas');
      copy.width = source.width;
      copy.height = source.height;
      copy.getContext('2d').drawImage(source, 0, 0);
      setArtwork({image: copy});
    }));
    $('oepl-use-text').addEventListener('click', () => run(() => {
      const text = $('oepl-text').value.trim();
      if (!text) throw new Error('Write a message first.');
      setArtwork({text});
    }));
    $('oepl-fit').addEventListener('change', () => run(() => client.artworkChanged()));
    $('oepl-dither').addEventListener('change', () => run(() => client.artworkChanged()));
    $('oepl-choose').addEventListener('click', () => $('oepl-file').click());
    $('oepl-file').addEventListener('change', () => run(async () => {
      const file = $('oepl-file').files[0];
      if (!file) return;
      if (!/^image\/(png|jpeg|webp|bmp)$/.test(file.type) || file.size > 20 * 1024 * 1024) {
        throw new Error('Choose a PNG, JPEG, WebP, or BMP image no larger than 20 MiB.');
      }
      const request = ++imageRequest, session = client.session;
      loadingImage = true;
      client.artworkChanged();
      client.note('Reading the image…');
      const url = URL.createObjectURL(file);
      try {
        const image = new Image();
        image.src = url;
        await image.decode();
        if (request !== imageRequest || session !== client.session) return;
        if (!image.width || !image.height || image.width * image.height > 40000000) throw new Error('Choose an image with at most 40 million pixels.');
        setArtwork({image});
      } catch (error) {
        if (session === client.session) client.note('Could not load the image: ' + error.message, true);
      } finally {
        URL.revokeObjectURL(url);
        if (request === imageRequest) { loadingImage = false; render(); }
        $('oepl-file').value = '';
      }
    }));
    function encodePreview(snapshot) {
      if (canvas.width !== snapshot.width || canvas.height !== snapshot.height) throw new Error('Preview dimensions changed. Review the artwork again.');
      return canvasJpeg(canvas);
    }
    $('oepl-send').addEventListener('click', () => run(() => {
      if (!artwork) throw new Error('Prepare artwork first.');
      return client.send(encodePreview, $('oepl-dither').checked ? 1 : 0);
    }));
    $('oepl-save').addEventListener('click', () => run(() => {
      if (!artwork) throw new Error('Prepare artwork first.');
      return client.save(encodePreview, (bytes, filename) => {
        const url = URL.createObjectURL(new Blob([bytes], {type: 'image/jpeg'}));
        const link = document.createElement('a');
        link.href = url;
        link.download = filename;
        document.body.appendChild(link);
        try { link.click(); }
        finally { link.remove(); setTimeout(() => URL.revokeObjectURL(url), 1000); }
      });
    }));
    render();
    return client;
  }

  const api = {Client, normalizeAp, normalizeTag, normalizeTags, hasDimensions, sdJpegBytes, canvasJpeg, boot};
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else if (root.document) {
    if (root.document.readyState === 'loading') root.document.addEventListener('DOMContentLoaded', () => boot(root.document));
    else boot(root.document);
  }
})(typeof globalThis !== 'undefined' ? globalThis : this);
