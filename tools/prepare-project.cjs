const fs = require('node:fs');
const path = require('node:path');

const root = path.resolve(__dirname, '../pebble');
const version = '1.0.0';

// AppMessage keys: the order gives the ids, so new keys always go to the end.
const keys = [
  'SAVED',
  'BG_OPTION',
  'BG_COLOR',
  'TEXT_MODE',
  'TEXT_COLOR',
  'BOARD_MODE',
  'BOARD_COLOR',
  'VALUE_MODE',
  'VALUE_COLOR',
  'SLOT1',
  'SLOT2',
  'SLOT3',
  'SLOT4',
  'SLOT5',
  'SLOT6',
  'SECOND_BOARD',
  'HR_SHAKE',
  'TAP_VIBE',
  'VIBE_IN_QUIET',
  'HOUR_MODE',
  'SHOW_AMPM',
  'DIST_UNIT'
];

// One glyph sprite sheet per platform (tools/make-glyphs.py), kept as a
// 2-bit palette so the fill color can be changed per glyph at runtime.
const media = ['basalt', 'chalk', 'emery', 'gabbro'].map((platform) => ({
  type: 'bitmap',
  name: `KIDDO_SHEET_${platform.toUpperCase()}`,
  file: `images/kiddo_${platform}.png`,
  memoryFormat: '2BitPalette',
  storageFormat: 'pbi',
  targetPlatforms: [platform]
}));

fs.writeFileSync(
  path.join(root, 'package.json'),
  JSON.stringify(
    {
      name: 'kiddo',
      author: 'Andor Czafik',
      version,
      private: true,
      description: 'Kiddo watchface for Pebble Time, Pebble Time Round, Pebble Time 2 and Pebble Round 2, ported from the original Fitbit clockface (Design & Code: Andor Czafik).',
      // @rebble/clay is the maintained Clay fork; pebble-clay 1.0.4 has no gabbro/flint binaries.
      dependencies: { '@rebble/clay': '1.1.0' },
      pebble: {
        displayName: 'Kiddo',
        uuid: '675d81c0-00ae-4273-92f2-546b04dcbf59',
        sdkVersion: '3',
        enableMultiJS: true,
        targetPlatforms: ['basalt', 'chalk', 'emery', 'gabbro'],
        capabilities: ['configurable', 'health'],
        watchapp: { watchface: true },
        messageKeys: keys,
        resources: { media }
      }
    },
    null,
    2
  ) + '\n'
);

const slotOptions = [
  { label: 'Nothing', value: '0' },
  { label: 'Heart rate', value: '1' },
  { label: 'Steps', value: '2' },
  { label: 'Active minutes', value: '3' },
  { label: 'Distance (km or mi)', value: '4' },
  { label: 'Calories', value: '5' },
  { label: 'Sleep hours', value: '6' },
  { label: 'Vibration (bell, off in Quiet Time)', value: '7' }
];

const config = [
  { type: 'heading', defaultValue: 'Kiddo' },
  {
    type: 'text',
    defaultValue: `Pebble Time / Time Round / Time 2 / Round 2 · ${version}<br>Design & Code: Andor Czafik`
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Colors' },
      {
        type: 'select',
        messageKey: 'BG_OPTION',
        label: 'Background color',
        defaultValue: '1',
        options: [
          { label: 'Choose one color', value: '0' },
          { label: 'Random every day', value: '4' },
          { label: 'Random every hour', value: '2' },
          { label: 'Random every minute', value: '1' },
          { label: 'Random on wrist flick', value: '3' }
        ]
      },
      { type: 'color', messageKey: 'BG_COLOR', label: 'Fixed background color', defaultValue: '55AAFF', allowGray: true },
      {
        type: 'select',
        messageKey: 'TEXT_MODE',
        label: 'Time, date and battery letters',
        defaultValue: '0',
        options: [
          { label: 'Rainbow (random color for every letter)', value: '0' },
          { label: 'One random color per text', value: '1' },
          { label: 'One fixed color', value: '2' }
        ]
      },
      { type: 'color', messageKey: 'TEXT_COLOR', label: 'Letter color', defaultValue: 'FFFF55', allowGray: true },
      {
        type: 'select',
        messageKey: 'BOARD_MODE',
        label: 'Health board',
        defaultValue: '0',
        options: [
          { label: 'Random color', value: '0' },
          { label: 'Fixed color', value: '1' }
        ]
      },
      { type: 'color', messageKey: 'BOARD_COLOR', label: 'Board color', defaultValue: 'FFAA55', allowGray: true },
      {
        type: 'select',
        messageKey: 'VALUE_MODE',
        label: 'Health board numbers',
        defaultValue: '0',
        options: [
          { label: 'Random color', value: '0' },
          { label: 'Fixed color', value: '1' }
        ]
      },
      { type: 'color', messageKey: 'VALUE_COLOR', label: 'Number color', defaultValue: '0000AA', allowGray: true },
      {
        type: 'text',
        defaultValue: 'Random colors change together with the background. With a fixed background color the letters get new colors every minute.'
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Health Board' },
      { type: 'select', messageKey: 'SLOT1', label: 'Left', defaultValue: '1', options: slotOptions },
      { type: 'select', messageKey: 'SLOT2', label: 'Middle', defaultValue: '2', options: slotOptions },
      { type: 'select', messageKey: 'SLOT3', label: 'Right', defaultValue: '7', options: slotOptions },
      {
        type: 'toggle',
        messageKey: 'SECOND_BOARD',
        label: 'Second board on wrist flick',
        defaultValue: true
      },
      { type: 'select', messageKey: 'SLOT4', label: 'Second board: Left', defaultValue: '4', options: slotOptions },
      { type: 'select', messageKey: 'SLOT5', label: 'Second board: Middle', defaultValue: '5', options: slotOptions },
      { type: 'select', messageKey: 'SLOT6', label: 'Second board: Right', defaultValue: '6', options: slotOptions },
      {
        type: 'text',
        defaultValue: 'Flick your wrist to show the second board for 20 seconds (flick again to return immediately). The bell shows whether the watch may vibrate: a crossed bell and OFF mean Quiet Time is active.'
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Wrist Flick' },
      {
        type: 'toggle',
        id: 'hr-shake',
        messageKey: 'HR_SHAKE',
        label: 'Measure heart rate on wrist flick',
        defaultValue: false
      },
      {
        type: 'text',
        id: 'hr-shake-text',
        defaultValue: 'A wrist flick requests up to 45 seconds of faster heart rate sampling (1 Hz) until two consistent readings confirm the BPM. The heart blinks while measuring, and the new value stays for a minute, so it is ready when the main board returns. Frequent use drains the battery faster.'
      },
      {
        type: 'toggle',
        messageKey: 'TAP_VIBE',
        label: 'Vibrate on wrist flick',
        defaultValue: true
      },
      {
        type: 'toggle',
        messageKey: 'VIBE_IN_QUIET',
        label: 'Vibrate on flick during Quiet Time',
        defaultValue: false
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Time & Units' },
      {
        type: 'select',
        messageKey: 'HOUR_MODE',
        label: 'Time format',
        defaultValue: '0',
        options: [
          { label: 'Watch preference', value: '0' },
          { label: '24h', value: '1' },
          { label: '12h', value: '2' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'SHOW_AMPM',
        label: 'Show AM/PM in 12h mode',
        defaultValue: true
      },
      {
        type: 'select',
        messageKey: 'DIST_UNIT',
        label: 'Distance unit',
        defaultValue: '0',
        options: [
          { label: 'Watch setting', value: '0' },
          { label: 'Metric (km)', value: '1' },
          { label: 'US (miles)', value: '2' }
        ]
      }
    ]
  },
  { type: 'submit', defaultValue: 'Save settings' }
];

fs.writeFileSync(path.join(root, 'src/pkjs/config.json'), JSON.stringify(config, null, 2) + '\n');
console.log('Prepared Kiddo manifest and offline Clay settings.');
