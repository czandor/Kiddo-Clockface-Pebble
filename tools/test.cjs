'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const root = path.resolve(__dirname, '..');
const pkg = JSON.parse(fs.readFileSync(path.join(root, 'pebble/package.json'), 'utf8'));
const config = JSON.parse(fs.readFileSync(path.join(root, 'pebble/src/pkjs/config.json'), 'utf8'));
const mainC = fs.readFileSync(path.join(root, 'pebble/src/c/main.c'), 'utf8');
const glyphsH = fs.readFileSync(path.join(root, 'pebble/src/c/glyphs.h'), 'utf8');

// 1. Manifest
assert.equal(pkg.pebble.displayName, 'Kiddo');
assert.deepEqual(pkg.pebble.targetPlatforms, ['basalt', 'chalk', 'emery', 'gabbro']);
assert.equal(pkg.pebble.watchapp.watchface, true);
assert.notEqual(pkg.pebble.uuid, '3fb5fda5-0820-4c60-8adf-c07541a3870d', 'Must not reuse the Blokk UUID');
assert.notEqual(pkg.pebble.uuid, '2a7685b1-bfef-4ef0-a5fb-3038a672eac5', 'Must not reuse the Fitbit UUID');
for (const platform of pkg.pebble.targetPlatforms) {
  const res = pkg.pebble.resources.media.find((m) => m.name === `KIDDO_SHEET_${platform.toUpperCase()}`);
  assert.ok(res, `${platform}: glyph sheet resource`);
  assert.deepEqual(res.targetPlatforms, [platform]);
  assert.equal(res.memoryFormat, '2BitPalette');
  assert.ok(fs.existsSync(path.join(root, 'pebble/resources', res.file)), `${platform}: ${res.file} exists`);
  assert.ok(glyphsH.includes(`RESOURCE_ID_KIDDO_SHEET_${platform.toUpperCase()}`), `${platform}: glyph table`);
}

// 2. Every Clay messageKey exists, and every AppMessage key read by the C code is in the manifest
const keySet = new Set(pkg.pebble.messageKeys);
const configKeys = [];
for (const entry of config) {
  if (entry.messageKey) configKeys.push(entry.messageKey);
  for (const item of entry.items || []) if (item.messageKey) configKeys.push(item.messageKey);
}
assert.ok(configKeys.length >= 21, 'Expected at least 21 configurable keys');
for (const k of configKeys) assert.ok(keySet.has(k), `Config key ${k} missing from package.json messageKeys`);
for (const m of mainC.matchAll(/MESSAGE_KEY_([A-Z0-9_]+)/g)) {
  assert.ok(keySet.has(m[1]), `C reads MESSAGE_KEY_${m[1]} that is not in the manifest`);
}
for (const k of pkg.pebble.messageKeys) {
  assert.ok(mainC.includes(`MESSAGE_KEY_${k}`), `Manifest key ${k} is never read by the watch`);
}

// 3. All characters the watch draws exist in the small glyph set
const smallChars = /#define SMALL_CHARS "([^"]+)"/.exec(glyphsH)[1];
const words = ['sun', 'mon', 'tue', 'wed', 'thu', 'fri', 'sat', 'am', 'pm', 'on', 'off', '...', '0123456789.', '100+',
  'january', 'february', 'march', 'april', 'may', 'june', 'july', 'august', 'september', 'october', 'november', 'december'];
for (const w of words) {
  for (const c of w) assert.ok(smallChars.includes(c), `Glyph for "${c}" (${w}) missing`);
}

// 4. companion.js in a mocked Pebble environment
const sentMessages = [];
const listeners = {};
const store = {};
let clayConfig = null;

global.localStorage = {
  getItem: (k) => (Object.prototype.hasOwnProperty.call(store, k) ? store[k] : null),
  setItem: (k, v) => { store[k] = String(v); }
};
global.Pebble = {
  addEventListener: (ev, fn) => { listeners[ev] = fn; },
  sendAppMessage: (data, ok) => { sentMessages.push(data); if (ok) ok(); },
  openURL: () => {}
};

const Module = require('node:module');
const origLoad = Module._load;
const mockKeys = {};
pkg.pebble.messageKeys.forEach((k, idx) => { mockKeys[k] = 10000 + idx; });

Module._load = function (request, parent, isMain) {
  if (request === 'message_keys') return mockKeys;
  if (request === '@rebble/clay') {
    return function MockClay(cfg) {
      clayConfig = cfg;
      this.generateUrl = () => 'data:text/html,mock';
      this.getSettings = (resp) => JSON.parse(resp);
      this.setSettings = () => {};
    };
  }
  return origLoad.apply(this, arguments);
};

const companionPath = path.join(root, 'pebble/src/pkjs/companion.js');
function freshCompanion(platform) {
  for (const k of Object.keys(store)) delete store[k];
  sentMessages.length = 0;
  if (platform) global.Pebble.getActiveWatchInfo = () => ({ platform });
  else delete global.Pebble.getActiveWatchInfo;
  delete require.cache[require.resolve(companionPath)];
  return require(companionPath);
}

function slotOptionValues(cfg, key) {
  for (const section of cfg) {
    for (const item of section.items || []) if (item.messageKey === key) return item.options.map((o) => o.value);
  }
  return null;
}

function hasItem(cfg, id) {
  return cfg.some((section) => (section.items || []).some((item) => item.id === id));
}

try {
  // Pebble Time 2 (heart-rate sensor)
  let comp = freshCompanion('emery');
  listeners.ready();
  assert.equal(sentMessages.length, 1);
  const first = sentMessages[0];
  assert.equal(first[mockKeys.BG_OPTION], 1, 'Background: random every minute');
  assert.equal(first[mockKeys.SLOT1], 1, 'emery: heart rate on the left');
  assert.equal(first[mockKeys.SLOT2], 2, 'Steps in the middle');
  assert.equal(first[mockKeys.SLOT3], 7, 'Quiet Time bell on the right by default');
  assert.equal(first[mockKeys.SLOT6], 6, 'Sleep replaces the Fitbit floors slot');
  assert.equal(first[mockKeys.SECOND_BOARD], 1);
  assert.equal(first[mockKeys.HR_SHAKE], 0);
  assert.equal(first[mockKeys.SHOW_AMPM], 1);
  assert.equal(first[mockKeys.BG_COLOR], 0xDB, '0x55AAFF -> GColorPictonBlue');
  assert.equal(first[mockKeys.SAVED], undefined);
  listeners.showConfiguration();
  assert.ok(slotOptionValues(clayConfig, 'SLOT1').includes('1'), 'emery: heart rate option offered');
  assert.ok(hasItem(clayConfig, 'hr-shake'), 'emery: flick measurement offered');

  // Saving settings
  listeners.webviewclosed({
    response: JSON.stringify({
      BG_OPTION: { value: '0' },
      BG_COLOR: { value: 0xFF5500 },
      TEXT_MODE: { value: '2' },
      TEXT_COLOR: { value: 16777215 },
      BOARD_MODE: { value: '1' },
      BOARD_COLOR: { value: '#AAAAAA' },
      SLOT3: { value: '6' },
      HR_SHAKE: { value: true },
      SHOW_AMPM: { value: false },
      HOUR_MODE: { value: '2' }
    })
  });
  const saved = sentMessages[1];
  assert.equal(saved[mockKeys.BG_OPTION], 0);
  assert.equal(saved[mockKeys.BG_COLOR], 0xF4, '0xFF5500 -> GColorOrange');
  assert.equal(saved[mockKeys.TEXT_MODE], 2);
  assert.equal(saved[mockKeys.TEXT_COLOR], 0xFF, 'white');
  assert.equal(saved[mockKeys.BOARD_COLOR], 0xEA, '#AAAAAA -> GColorLightGray');
  assert.equal(saved[mockKeys.SLOT3], 6);
  assert.equal(saved[mockKeys.HR_SHAKE], 1);
  assert.equal(saved[mockKeys.SHOW_AMPM], 0);
  assert.equal(saved[mockKeys.HOUR_MODE], 2);
  assert.equal(saved[mockKeys.SLOT1], 1, 'Missing keys fall back to the defaults');
  assert.equal(saved[mockKeys.SAVED], 1);
  assert.equal(JSON.parse(store['kiddo.settings']).BG_COLOR, 0xFF5500);
  assert.equal(comp.rgbToGColor8(0x000000), 0xC0);
  assert.equal(comp.rgbToGColor8(0x0000AA), 0xC2);
  assert.equal(comp.toBoolInt('true'), 1);
  assert.equal(comp.toBoolInt(false), 0);

  // Watches without a heart-rate sensor
  for (const platform of ['basalt', 'chalk', 'gabbro']) {
    comp = freshCompanion(platform);
    listeners.ready();
    assert.equal(sentMessages[0][mockKeys.SLOT1], 3, `${platform}: active minutes replace heart rate`);
    assert.equal(sentMessages[0][mockKeys.SLOT3], 7, `${platform}: Quiet Time bell by default`);
    listeners.showConfiguration();
    for (const k of ['SLOT1', 'SLOT2', 'SLOT3', 'SLOT4', 'SLOT5', 'SLOT6']) {
      assert.ok(!slotOptionValues(clayConfig, k).includes('1'), `${platform}: no heart rate option in ${k}`);
    }
    assert.ok(!hasItem(clayConfig, 'hr-shake') && !hasItem(clayConfig, 'hr-shake-text'), `${platform}: no flick HR`);
  }

  // A heart-rate slot carried over to a watch without the sensor
  comp = freshCompanion('basalt');
  store['kiddo.settings'] = JSON.stringify({ SLOT4: 1 });
  listeners.ready();
  assert.equal(sentMessages[0][mockKeys.SLOT4], 3);
} finally {
  Module._load = origLoad;
}

console.log('All Kiddo manifest, glyph and companion tests passed.');
