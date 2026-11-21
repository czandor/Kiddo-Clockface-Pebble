'use strict';

var Clay = require('@rebble/clay');
var keys = require('message_keys');
var baseConfig = require('./config.json');

// Runs inside the configuration page (serialized by Clay): no outer variables.
function clayCustomFn() {
  this.on(this.EVENTS.AFTER_BUILD, function () {
    var self = this;

    function bindColor(selectKey, colorKey, fixedValue) {
      var select = self.getItemByMessageKey(selectKey);
      var color = self.getItemByMessageKey(colorKey);
      if (!select || !color) return;
      function sync() {
        if (String(select.get()) === fixedValue) color.show();
        else color.hide();
      }
      select.on('change', sync);
      sync();
    }

    bindColor('BG_OPTION', 'BG_COLOR', '0');
    bindColor('TEXT_MODE', 'TEXT_COLOR', '2');
    bindColor('BOARD_MODE', 'BOARD_COLOR', '1');
    bindColor('VALUE_MODE', 'VALUE_COLOR', '1');

    var second = self.getItemByMessageKey('SECOND_BOARD');
    var secondSlots = ['SLOT4', 'SLOT5', 'SLOT6'].map(function (k) {
      return self.getItemByMessageKey(k);
    });
    function syncSecond() {
      var enabled = !!second.get();
      secondSlots.forEach(function (item) {
        if (!item) return;
        if (enabled) item.show();
        else item.hide();
      });
    }
    if (second) {
      second.on('change', syncSecond);
      syncSecond();
    }
  });
}

var BOOL_KEYS = ['SECOND_BOARD', 'HR_SHAKE', 'TAP_VIBE', 'VIBE_IN_QUIET', 'SHOW_AMPM'];
var COLOR_KEYS = ['BG_COLOR', 'TEXT_COLOR', 'BOARD_COLOR', 'VALUE_COLOR'];

// Colors are kept as 0xRRGGBB (as Clay stores them) and sent as GColor8 argb.
var defaults = {
  BG_OPTION: 1,        // random every minute
  BG_COLOR: 0x55AAFF,
  TEXT_MODE: 0,        // rainbow letters
  TEXT_COLOR: 0xFFFF55,
  BOARD_MODE: 0,
  BOARD_COLOR: 0xFFAA55,
  VALUE_MODE: 0,
  VALUE_COLOR: 0x0000AA,
  SLOT1: 1,            // heart rate
  SLOT2: 2,            // steps
  SLOT3: 7,            // vibration / Quiet Time bell
  SLOT4: 4,            // distance
  SLOT5: 5,            // calories
  SLOT6: 6,            // sleep
  SECOND_BOARD: 1,
  HR_SHAKE: 0,
  TAP_VIBE: 1,
  VIBE_IN_QUIET: 0,
  HOUR_MODE: 0,
  SHOW_AMPM: 1,
  DIST_UNIT: 0
};

// Pebble Time (basalt), Time Round (chalk) and Round 2 (gabbro) have no heart-rate sensor.
var NO_HR_PLATFORMS = ['basalt', 'chalk', 'gabbro'];
var SLOT_HR = 1;
var NO_HR_SLOT1 = 3; // active minutes take the heart-rate slot

function activePlatform() {
  try {
    if (typeof Pebble !== 'undefined' && typeof Pebble.getActiveWatchInfo === 'function') {
      var info = Pebble.getActiveWatchInfo();
      if (info && info.platform) return info.platform;
    }
  } catch (e) {}
  return '';
}

function lacksHr(platform) {
  return NO_HR_PLATFORMS.indexOf(platform) !== -1;
}

function defaultFor(key, noHr) {
  if (noHr && key === 'SLOT1') return NO_HR_SLOT1;
  return defaults[key];
}

// Settings page for the active watch: without a heart-rate sensor the heart
// rate option and the flick measurement are left out.
function configFor(platform) {
  var config = JSON.parse(JSON.stringify(baseConfig));
  if (!lacksHr(platform)) return config;
  var slotKeys = ['SLOT1', 'SLOT2', 'SLOT3', 'SLOT4', 'SLOT5', 'SLOT6'];
  config.forEach(function (section) {
    if (!Array.isArray(section.items)) return;
    section.items = section.items.filter(function (item) {
      return item.id !== 'hr-shake' && item.id !== 'hr-shake-text';
    });
    section.items.forEach(function (item) {
      if (slotKeys.indexOf(item.messageKey) !== -1) {
        item.options = item.options.filter(function (o) { return o.value !== String(SLOT_HR); });
        if (item.defaultValue === String(SLOT_HR)) item.defaultValue = String(NO_HR_SLOT1);
      }
    });
  });
  return config;
}

var settings = {};
var queue = [];
var sending = false;
var clay = null;

function stored(key, fallback) {
  try {
    return JSON.parse(localStorage.getItem(key)) || fallback;
  } catch (e) {
    return fallback;
  }
}

function loadSettings() {
  var saved = stored('kiddo.settings', null);
  var noHr = lacksHr(activePlatform());
  var src = saved || {};
  Object.keys(defaults).forEach(function (k) {
    settings[k] = src[k] !== undefined ? src[k] : defaultFor(k, noHr);
  });
  // A heart-rate slot carried over to a watch without the sensor shows active minutes.
  if (noHr) {
    ['SLOT1', 'SLOT2', 'SLOT3', 'SLOT4', 'SLOT5', 'SLOT6'].forEach(function (k) {
      if (toInt(settings[k], 0) === SLOT_HR) settings[k] = NO_HR_SLOT1;
    });
  }
}

function send(message) {
  queue.push({ data: message, attempts: 0 });
  drain();
}

function drain() {
  if (sending || !queue.length) return;
  sending = true;
  var item = queue[0];
  Pebble.sendAppMessage(
    item.data,
    function () {
      queue.shift();
      sending = false;
      drain();
    },
    function () {
      sending = false;
      if (++item.attempts >= 3) queue.shift();
      setTimeout(drain, 1500);
    }
  );
}

function toInt(val, fallback) {
  var n = typeof val === 'number' ? val : parseInt(val, 10);
  return isNaN(n) ? fallback : n;
}

function toBoolInt(val) {
  if (val === true || val === 1 || val === '1' || val === 'true') return 1;
  return 0;
}

// Clay color values are numbers (0xRRGGBB); strings like "FF5500", "#FF5500" or "0xFF5500" are accepted too.
function toRgb(val, fallback) {
  if (typeof val === 'number' && isFinite(val)) return val & 0xFFFFFF;
  if (typeof val === 'string') {
    var s = val.replace(/^#|^0x/i, '');
    if (/^[0-9a-f]{1,6}$/i.test(s)) return parseInt(s, 16);
  }
  return fallback;
}

// 0xRRGGBB -> Pebble GColor8 argb (opaque, 2 bits per channel)
function rgbToGColor8(rgb) {
  var r = Math.round(((rgb >> 16) & 0xFF) / 85);
  var g = Math.round(((rgb >> 8) & 0xFF) / 85);
  var b = Math.round((rgb & 0xFF) / 85);
  return 0xC0 | (r << 4) | (g << 2) | b;
}

function buildPayload(savedFlag) {
  var msg = {};
  Object.keys(defaults).forEach(function (k) {
    if (BOOL_KEYS.indexOf(k) !== -1) msg[keys[k]] = toBoolInt(settings[k]);
    else if (COLOR_KEYS.indexOf(k) !== -1) msg[keys[k]] = rgbToGColor8(toRgb(settings[k], defaults[k]));
    else msg[keys[k]] = toInt(settings[k], defaults[k]);
  });
  if (savedFlag) msg[keys.SAVED] = 1;
  return msg;
}

function getClay() {
  var platform = activePlatform();
  clay = new Clay(configFor(platform), clayCustomFn, { autoHandleEvents: false });
  return clay;
}

loadSettings();

Pebble.addEventListener('ready', function () {
  loadSettings();
  send(buildPayload(false));
});

Pebble.addEventListener('showConfiguration', function () {
  var c = getClay();
  // Fresh install without a heart-rate sensor: show active minutes in the left slot.
  if (!stored('kiddo.settings', null) && lacksHr(activePlatform()) && typeof c.setSettings === 'function') {
    try {
      c.setSettings({ SLOT1: String(NO_HR_SLOT1) });
    } catch (e) {}
  }
  Pebble.openURL(c.generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;
  var raw = (clay || getClay()).getSettings(e.response, false);
  var noHr = lacksHr(activePlatform());
  var parsed = {};
  Object.keys(defaults).forEach(function (k) {
    var val = raw[k] !== undefined && raw[k] !== null && raw[k].value !== undefined ? raw[k].value : raw[k];
    if (val === undefined || val === null) parsed[k] = defaultFor(k, noHr);
    else if (BOOL_KEYS.indexOf(k) !== -1) parsed[k] = toBoolInt(val);
    else if (COLOR_KEYS.indexOf(k) !== -1) parsed[k] = toRgb(val, defaults[k]);
    else parsed[k] = toInt(val, defaultFor(k, noHr));
  });
  settings = parsed;
  try {
    localStorage.setItem('kiddo.settings', JSON.stringify(settings));
  } catch (err) {
    console.log('Storage error: ' + err.message);
  }
  send(buildPayload(true));
});

if (typeof module !== 'undefined' && module.exports) {
  module.exports = {
    defaults: defaults,
    defaultFor: defaultFor,
    configFor: configFor,
    buildPayload: buildPayload,
    rgbToGColor8: rgbToGColor8,
    toRgb: toRgb,
    toInt: toInt,
    toBoolInt: toBoolInt
  };
}
