// ======================================================================
// SmartMet Server data explorer
//
// Builds queries from the metadata of the APIs listed in the API catalog
// (/api): the parameters, models, times and areas narrow each other, and the
// results are shown as charts, tables and trees. The request is shown as a URL
// and as bash, Python, R and JavaScript code.
//
// Served by spine (Spine::OpenAPI::Catalog) at /api/explorer. Uses uPlot for
// the charts and the SmartMet Swagger UI plugin (SmartMetSwaggerUI) for the
// code snippets and the response tree. The DOM is built from text nodes only,
// never from HTML strings.
//
// The parts of this file, in order:
// - utilities, the autocomplete field and the TimeSeries parameter index
// - map settings, palettes and the basemap, and the map with its drawing
//   tools (createMap)
// - the vertical views: profiles, time-height sections, cross sections and
//   time series along paths, flight levels, the animated time slider, and the
//   thermodynamic diagrams (skew-T, tephigram, Stuve)
// - charts of time series (lines, percentiles and swarms of many locations),
//   values on a map with colour scales, tables and code snippets
// - the modules of the APIs: TimeSeries, EDR and Coverages, each building
//   its form, its request and the views of its results
// - the application: the API catalog, the choice of the API, and the state
//   kept in the address of the page
// ======================================================================

(function(root) {
'use strict';

const S = root.SmartMetSwaggerUI;

// ------------------------------------------------------------------
// Languages
// ------------------------------------------------------------------

// The languages of the page: English, and those of the translation files explorer-<code>.js
// loaded before this file, each {name, strings: {English text: translation}}. A text with
// values has them as {0}, {1}, ... in the order of a template literal.
const TRANSLATIONS = root.SmartMetExplorerTranslations || {};
const LANGUAGE_KEY = 'smartmet-explorer-language';

// The language of the page: the choice of the user, else the default of the server, else
// the first language of the browser that has a translation, else English
const LANGUAGE = (() => {
  const known = (code) => code === 'en' || Object.prototype.hasOwnProperty.call(TRANSLATIONS, code);
  try
  {
    const stored = localStorage.getItem(LANGUAGE_KEY);
    if (stored && known(stored))
      return stored;
  }
  catch (e)
  {
    // Not remembered
  }
  try
  {
    const page = typeof document !== 'undefined' ? document.getElementById('explorer') : null;
    const server = page && page.dataset.defaults ? JSON.parse(page.dataset.defaults).language : '';
    if (server && known(server))
      return server;
  }
  catch (e)
  {
    // No defaults
  }
  const browser = typeof navigator !== 'undefined' ? navigator.languages || [navigator.language] : [];
  for (const tag of browser)
  {
    const code = String(tag || '').toLowerCase().split(/[-_]/)[0];
    if (known(code))
      return code;
  }
  return 'en';
})();
const STRINGS = (TRANSLATIONS[LANGUAGE] && TRANSLATIONS[LANGUAGE].strings) || {};

// A count with the word for one or many: counted(3, L('member'), L('members')), '3 members'
function counted(n, one, many)
{
  return `${n} ${n === 1 ? one : many}`;
}

// A text of the page in its language (L for localized): L("Text"), or as the tag of a template literal,
// L`${n} rows`, whose translation key is '{0} rows'. A text without a translation is English.
function L(text, ...values)
{
  const template = Array.isArray(text) && text.raw;
  const key = template ? text.reduce((out, part, i) => (i === 0 ? part : `${out}{${i - 1}}${part}`), '') : text;
  const found = Object.prototype.hasOwnProperty.call(STRINGS, key);
  // The texts without a translation are listed for the tests of the translations
  if (!found && LANGUAGE !== 'en')
    MISSING_TEXTS.add(key);
  const translated = found ? STRINGS[key] : key;
  return template ? translated.replace(/\{(\d+)\}/g, (m, i) => (i < values.length ? String(values[i]) : m)) :
                    translated;
}
const MISSING_TEXTS = new Set();

// ------------------------------------------------------------------
// Utilities
// ------------------------------------------------------------------

// el("div", {class: "x", onclick: f}, child, "text", ...)
function el(tag, attrs, ...children)
{
  const node = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs || {}))
  {
    if (v === undefined || v === null || v === false)
      continue;
    if (k === 'class')
      node.className = v;
    else if (k.startsWith('on'))
      node.addEventListener(k.slice(2), v);
    else if (k === 'value')
      node.value = v;
    else
      node.setAttribute(k, v === true ? '' : String(v));
  }
  for (const child of children.flat())
  {
    if (child === undefined || child === null || child === false)
      continue;
    node.append(child instanceof Node ? child : document.createTextNode(String(child)));
  }
  return node;
}

// A function calling fn only once the calls have paused for ms milliseconds, e.g. while typing
function debounce(fn, ms)
{
  let timer = null;
  return (...args) => {
    clearTimeout(timer);
    timer = setTimeout(() => fn(...args), ms);
  };
}

// The API key of the user, sent as the fmi-apikey header of the requests of the explorer (never
// in their URLs) and remembered by the browser
const APIKEY_KEY = 'smartmet-explorer-apikey';
let apiKey = (() => {
  try
  {
    return localStorage.getItem(APIKEY_KEY) || '';
  }
  catch (e)
  {
    return '';
  }
})();

// The field of the API key; the module is opened again when it changes
function apiKeyField(onChange)
{
  const input = el('input', {
    type: 'password',
    'aria-label': L('API key'),
    placeholder: L('API key (optional)'),
    autocomplete: 'off',
    spellcheck: 'false',
    value: apiKey
  });
  input.addEventListener('change', () => {
    apiKey = input.value.trim();
    try
    {
      if (apiKey)
        localStorage.setItem(APIKEY_KEY, apiKey);
      else
        localStorage.removeItem(APIKEY_KEY);
    }
    catch (e)
    {
      // Not remembered
    }
    onChange();
  });
  return el('label', {class: 'apikey'}, input);
}

// The headers of a request of the explorer: the given ones and the API key
function apiHeaders(headers = {})
{
  return apiKey ? Object.assign({}, headers, {'fmi-apikey': apiKey}) : headers;
}

// The message of a failed request: the detail of a problem document, the error header of the
// plugin (X-TimeSeriesPlugin-Error, X-EDRPlugin-Error, ...), or the start of the response
function serverError(response, text)
{
  let detail = '';
  try
  {
    const doc = JSON.parse(text);
    detail = (doc && (doc.detail || doc.title || doc.description)) || '';
  }
  catch (e)
  {
    // Not JSON
  }
  if (!detail)
    for (const [name, value] of response.headers)
      if (/^x-.*-error$/i.test(name) && value)
      {
        detail = value;
        break;
      }
  if (!detail)
    detail = String(text || '').replace(/<[^>]*>/g, ' ').replace(/\s+/g, ' ').trim().slice(0, 500);
  return L`The server answered ${response.status} ${response.statusText}: ${detail}`;
}

// Copies text to the clipboard and tells it on the button for a moment. The Clipboard API is
// only available on HTTPS pages (and localhost): elsewhere the text is copied from a hidden
// field.
async function copyText(text, button)
{
  let copied = false;
  try
  {
    if (navigator.clipboard && window.isSecureContext)
    {
      await navigator.clipboard.writeText(text);
      copied = true;
    }
  }
  catch (e)
  {
    // The fallback below
  }
  if (!copied)
  {
    const field = el('textarea', {readonly: '', 'aria-hidden': 'true',
                                  style: 'position: fixed; top: 0; left: -9999px; opacity: 0'});
    field.value = text;
    document.body.append(field);
    field.select();
    try
    {
      copied = document.execCommand('copy');
    }
    catch (e)
    {
      copied = false;
    }
    field.remove();
    button.focus();
  }
  if (!button.dataset.label)
    button.dataset.label = button.textContent;
  button.textContent = copied ? L('Copied') : L('Copying failed');
  clearTimeout(button._copyTimer);
  button._copyTimer = setTimeout(() => {
    button.textContent = button.dataset.label;
  }, 1200);
  return copied;
}

// GET a JSON document; a failed request throws with the message of the server
async function fetchJson(url)
{
  const response = await fetch(url, {headers: apiHeaders({accept: 'application/json'})});
  if (!response.ok)
    throw new Error(serverError(response, await response.text()));
  return response.json();
}

// A note below a request: an error of the server, or why the result is empty
function requestNote()
{
  const note = el('p', {class: 'request-note', role: 'status', hidden: true});
  return {
    element: note,
    clear() {
      note.hidden = true;
      note.replaceChildren();
    },
    show(cls, ...text) {
      note.className = `request-note ${cls}`;
      note.replaceChildren(...text);
      note.hidden = false;
    }
  };
}

// Whether rows have no value of any of the parameters
function rowsWithoutValues(rows, parameters)
{
  return rows.length > 0 && parameters.length > 0 &&
      !rows.some((r) => parameters.some((p) => r[p] !== null && r[p] !== undefined && r[p] !== ''));
}

// Query strings keep commas, colons, slashes and parentheses readable, everything else is percent
// encoded
function encode(value)
{
  return encodeURIComponent(value)
      .replace(/%2C/gi, ',')
      .replace(/%3A/gi, ':')
      .replace(/%28/gi, '(')
      .replace(/%29/gi, ')')
      .replace(/%2F/gi, '/');
}

// A query string of [name, value] pairs, empty values left out
function queryString(params)
{
  return params.filter(([, v]) => v !== undefined && v !== null && v !== '')
      .map(([k, v]) => `${encodeURIComponent(k)}=${encode(v)}`)
      .join('&');
}

const pad = (n) => String(n).padStart(2, '0');

// A Date as the value of a datetime-local input in UTC
function toInput(date)
{
  return `${date.getUTCFullYear()}-${pad(date.getUTCMonth() + 1)}-${pad(date.getUTCDate())}T` +
      `${pad(date.getUTCHours())}:${pad(date.getUTCMinutes())}`;
}

// The value of a datetime-local input in UTC as an ISO time
function fromInput(value)
{
  return value ? `${value}:00Z` : '';
}

// A time for display: 2026-10-16 12:00 UTC
function formatTime(date)
{
  if (clock === '12')
  {
    const h = date.getUTCHours();
    return `${toInput(date).slice(0, 10)} ${(h + 11) % 12 + 1}:${pad(date.getUTCMinutes())} ${h < 12 ? 'AM' : 'PM'} UTC`;
  }
  return `${toInput(date).replace('T', ' ')} UTC`;
}

// The clock of the times shown, '24' or '12' hours: the choice of the user, else the default of
// the server, else that of the language of the browser
const CLOCK_KEY = 'smartmet-explorer-clock';
let clock = '24';
function initialClock(serverClock)
{
  try
  {
    const stored = localStorage.getItem(CLOCK_KEY);
    if (stored === '12' || stored === '24')
      return stored;
  }
  catch (e)
  {
    // Not remembered
  }
  if (serverClock === '12' || serverClock === '24')
    return serverClock;
  try
  {
    const language = typeof navigator !== 'undefined' ? navigator.language : undefined;
    // en-US outside the Americas is taken for English of no region
    const effective = /^en[-_]US$/i.test(language || '') && !browserRegion(language, browserTimeZone()) ? 'en-GB' : language;
    const cycle = new Intl.DateTimeFormat(effective, {hour: 'numeric'}).resolvedOptions().hourCycle;
    return cycle === 'h11' || cycle === 'h12' ? '12' : '24';
  }
  catch (e)
  {
    return '24';
  }
}

// A date template of the charts (uPlot) for the clock: 24 hours with ISO dates, or as uPlot has
// it, 12 hours with month/day
function clockTemplate(template)
{
  if (clock === '12')
    return template;
  let t = template.replace('{M}/{D}/{YY}', '{YYYY}-{MM}-{DD}').replace('{M}/{D}', '{MM}-{DD}');
  if (t.includes('{h}'))
    t = t.replace('{h}', t.includes('{mm}') ? '{HH}' : '{HH}:00');
  return t.replace(/\{(aa|AA|a|A)\}/g, '').trim();
}

// A chart: uPlot with the times of the clock chosen
function makePlot(root, opts, data, target)
{
  const timed = !(opts.scales && opts.scales.x && opts.scales.x.time === false);
  const options = timed ? Object.assign({fmtDate: (t) => root.uPlot.fmtDate(clockTemplate(t))}, opts) : opts;
  return new root.uPlot(options, data, target);
}

// A time 'YYYY-MM-DDTHH:MM' (UTC, as datetime-local inputs have it) for a text field in the
// clock chosen: 2026-10-16 13:00 or 2026-10-16 1:00 PM
function timeText(value)
{
  const m = /^(\d{4}-\d\d-\d\d)T(\d\d):(\d\d)/.exec(value || '');
  if (!m)
    return '';
  if (clock !== '12')
    return `${m[1]} ${m[2]}:${m[3]}`;
  const h = Number(m[2]);
  return `${m[1]} ${(h + 11) % 12 + 1}:${m[3]} ${h < 12 ? 'AM' : 'PM'}`;
}

// The text of a time field as 'YYYY-MM-DDTHH:MM', '' when it is not a time: a date with an
// optional time of the 24 or the 12 hour clock
function parseTimeText(text)
{
  const m = /^\s*(\d{4})-(\d{1,2})-(\d{1,2})(?:[ T]+(\d{1,2})(?::(\d\d))?\s*(am|pm|a\.m\.|p\.m\.)?)?\s*(?:utc|z)?\s*$/i
                .exec(text || '');
  if (!m)
    return '';
  let h = m[4] === undefined ? 0 : Number(m[4]);
  const minutes = m[5] === undefined ? 0 : Number(m[5]);
  const half = (m[6] || '').toLowerCase()[0];
  if (half && (h < 1 || h > 12))
    return '';
  if (half === 'p' && h < 12)
    h += 12;
  else if (half === 'a' && h === 12)
    h = 0;
  const month = Number(m[2]);
  const day = Number(m[3]);
  const date = new Date(Date.UTC(Number(m[1]), month - 1, day, h, minutes));
  if (h > 23 || minutes > 59 || date.getUTCMonth() !== month - 1 || date.getUTCDate() !== day)
    return '';
  return toInput(date);
}

// A small calendar drawn with SVG: emoji are missing from many fonts
function calendarIcon()
{
  const icon = svg('svg', {width: 14, height: 14, viewBox: '0 0 14 14', 'aria-hidden': 'true', class: 'icon'});
  icon.append(svg('rect', {x: 1.5, y: 2.5, width: 11, height: 10, rx: 1.5, fill: 'none', stroke: 'currentColor'}),
              svg('line', {x1: 1.5, y1: 5.5, x2: 12.5, y2: 5.5, stroke: 'currentColor'}),
              svg('line', {x1: 4.5, y1: 1, x2: 4.5, y2: 4, stroke: 'currentColor'}),
              svg('line', {x1: 9.5, y1: 1, x2: 9.5, y2: 4, stroke: 'currentColor'}));
  return icon;
}

// The time fields shown, written again in a new clock
const timeFields = new Set();

// A period {min, max} of ISO times as values of time fields ('' for an open end)
function periodOf(start, end)
{
  const value = (t) => {
    const d = t ? new Date(t) : null;
    return d && !isNaN(d) ? toInput(d) : '';
  };
  return {min: value(start), max: value(end)};
}

// Keeps the period of the request (s.start and s.end) within the period of the data and the
// start before the end, and lets the time fields offer only such times
function limitPeriod(s, startInput, endInput, period)
{
  const limited = (v) => (!v ? v : period.min && v < period.min ? period.min :
                                   period.max && v > period.max ? period.max : v);
  s.start = limited(s.start);
  s.end = limited(s.end);
  if (s.start && s.end && s.end < s.start)
    s.end = s.start;
  startInput.setRange(period.min, s.end || period.max);
  endInput.setRange(s.start || period.min, period.max);
}

// A time field in UTC in the clock chosen, with a button for the date picker of the browser:
// an element whose value is 'YYYY-MM-DDTHH:MM' like that of a datetime-local input, and which
// passes on the input and change events of its text
function timeInput(attrs)
{
  const label = attrs['aria-label'] || 'Time (UTC)';
  const text = el('input', {type: 'text', class: 'time-text', size: 18, 'aria-label': L`${label} text`,
                            spellcheck: 'false', autocomplete: 'off'});
  const picker = el('input', {type: 'datetime-local', class: 'time-picker', tabindex: '-1', 'aria-hidden': 'true'});
  const button = el('button', {type: 'button', class: 'small time-pick', 'aria-label': L`Choose the ${label.toLowerCase()}`,
                               title: L('Choose from a calendar')}, calendarIcon());
  const box = el('span', {class: 'time-input', 'aria-label': label}, text, button, picker);
  let current = '';
  // The data period the value is kept in, 'YYYY-MM-DDTHH:MM' or '' for no limit
  let range = {min: '', max: ''};
  const limited = (v) => (!v ? v : range.min && v < range.min ? range.min : range.max && v > range.max ? range.max : v);
  // A value outside the period is moved to its end, and the field tells it for a moment
  const keepInRange = () => {
    const v = limited(current);
    if (v === current)
      return;
    current = v;
    text.value = timeText(current);
    text.title = L('Moved within the period of the data');
    text.classList.add('time-adjusted');
    setTimeout(() => {
      text.classList.remove('time-adjusted');
      text.title = '';
    }, 2500);
  };
  const placeholder = () => {
    text.placeholder = clock === '12' ? L('YYYY-MM-DD h:mm AM') : L('YYYY-MM-DD HH:MM');
  };
  Object.defineProperty(box, 'value', {
    get: () => current,
    set: (v) => {
      current = /^\d{4}-\d\d-\d\dT\d\d:\d\d/.test(v || '') ? v.slice(0, 16) : '';
      text.value = timeText(current);
      text.removeAttribute('aria-invalid');
    }
  });
  text.addEventListener('input', () => {
    current = parseTimeText(text.value);
    if (current || text.value.trim() === '')
      text.removeAttribute('aria-invalid');
    else
      text.setAttribute('aria-invalid', 'true');
  });
  // The value written in the clock when the field is left, within the period of the data
  text.addEventListener('change', () => {
    keepInRange();
    if (current)
      text.value = timeText(current);
  });
  button.addEventListener('click', () => {
    picker.value = current;
    try
    {
      picker.showPicker();
    }
    catch (e)
    {
      picker.focus();
    }
  });
  picker.addEventListener('change', () => {
    current = picker.value;
    text.value = timeText(current);
    text.removeAttribute('aria-invalid');
    text.dispatchEvent(new Event('input', {bubbles: true}));
    text.dispatchEvent(new Event('change', {bubbles: true}));
  });
  // The events of the picker are its own
  picker.addEventListener('input', (e) => e.stopPropagation());
  placeholder();
  box.refresh = () => {
    placeholder();
    text.value = timeText(current);
  };
  // The period the times can be chosen in: the calendar offers only its days, and other times
  // are moved within it
  box.setRange = (min, max) => {
    range = {min: min || '', max: max || ''};
    if (range.min)
      picker.min = range.min;
    else
      picker.removeAttribute('min');
    if (range.max)
      picker.max = range.max;
    else
      picker.removeAttribute('max');
  };
  timeFields.add(box);
  return box;
}

// The views drawn again when a setting (the clock, the units) changes, one per name
const settingsListeners = new Map();
function onSettingsChange(name, fn)
{
  settingsListeners.set(name, fn);
}
function settingsChanged()
{
  for (const field of timeFields)
    if (field.isConnected)
      field.refresh();
    else
      timeFields.delete(field);
  for (const fn of settingsListeners.values())
    fn();
}

// Times of the TimeSeries output: 20261016T120000, 2026-10-16T12:00:00, with or without a
// zone, or epoch seconds. Times without a zone are UTC, the explorer always asks for UTC.
function parseTime(value)
{
  if (typeof value === 'number')
    return new Date(value * 1000);
  const s = String(value);
  let m = /^(\d{4})(\d{2})(\d{2})T(\d{2})(\d{2})(\d{2})?$/.exec(s);
  if (m)
    return new Date(Date.UTC(+m[1], m[2] - 1, +m[3], +m[4], +m[5], +(m[6] || 0)));
  if (/^\d+$/.test(s))
    return new Date(Number(s) * 1000);
  m = /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}(:\d{2})?$/.exec(s);
  if (m)
    return new Date(`${s}${m[1] ? '' : ':00'}Z`);
  const d = new Date(s);
  return isNaN(d.getTime()) ? null : d;
}

// The rows of a TimeSeries JSON response: a list of rows, a list of lists of rows (one list
// per location) or a single row
function flattenRows(data)
{
  if (Array.isArray(data))
    return data.flatMap((item) => (Array.isArray(item) ? flattenRows(item) : expandRow(item)));
  if (data && typeof data === 'object')
    return expandRow(data);
  return [];
}

// The value of an area: "[16 15 nan]", the values of the points of the area
const AREA_VALUES = /^\[(.*)\]$/;

// A value of a bracket list of an area query: a number, null for nan, or text
function areaValue(text)
{
  if (text === 'nan' || text === 'NaN' || text === '')
    return null;
  const n = Number(text);
  return isNaN(n) ? text : n;
}

// A row of an area or a radius query has the values of all its points as lists: the row is
// split into a row per point. Other rows are returned as they are.
function expandRow(row)
{
  if (!row || typeof row !== 'object')
    return [];
  const lists = {};
  let count = -1;
  for (const [k, v] of Object.entries(row))
  {
    if (typeof v !== 'string')
      continue;
    const m = AREA_VALUES.exec(v);
    if (!m)
      continue;
    lists[k] = m[1].trim() === '' ? [] : m[1].trim().split(/\s+/);
    count = Math.max(count, lists[k].length);
  }
  if (count < 0)
    return [row];
  const out = [];
  for (let i = 0; i < count; i++)
  {
    const point = {};
    for (const [k, v] of Object.entries(row))
      point[k] = k in lists ? areaValue(lists[k][i] !== undefined ? lists[k][i] : '') : v;
    point._point = i;
    out.push(point);
  }
  return out;
}

// The location of a row: the name, or the coordinates when there is no name or the row is a
// point of an area
// Whether a location name is a coordinate selection (the points of a box or a radius are
// named by it) rather than a name
function isCoordinateName(name)
{
  return /^[-\d.,:\s]+$/.test(String(name));
}

function locationOf(row)
{
  // Each level and each variant (a level or a member of a grid parameter key) is a series of
  // its own
  const level = (row.level !== undefined && row.level !== null ? ` · ${row._levelLabel || row.level}` : '') +
      (row._variant ? ` · ${row._variant}` : '');
  const coords = row.lat !== undefined && row.lat !== null ? `${row.lat},${row.lon}` : '';
  if (row._point !== undefined)
    return `${row.name !== null && row.name !== undefined && !isCoordinateName(row.name) ? row.name + ' ' : ''}(${
        coords || row._point})${level}`;
  if (row.name !== undefined && row.name !== null)
    return String(row.name) + level;
  return coords + level;
}

// The rows of a request whose columns are parameter keys standing for one parameter at some
// level or ensemble member: the row is split into a row per variant with the columns named by
// the parameter. columns maps a requested column to {name, variant, level, levelLabel, series,
// member}: the rows of a level of the key have it as the level (and levelLabel as _levelLabel),
// the series name (series, or the variant) as _variant, and the member as _member.
function splitVariants(rows, columns)
{
  if (!columns || columns.size === 0)
    return rows;
  const out = [];
  for (const row of rows)
  {
    const base = {};
    const variants = new Map();
    const members = new Map();
    const levels = new Map();
    const series = new Map();
    for (const [k, v] of Object.entries(row))
    {
      const c = columns.get(k);
      if (!c)
      {
        base[k] = v;
        continue;
      }
      if (!variants.has(c.variant))
        variants.set(c.variant, {});
      variants.get(c.variant)[c.name] = v;
      if (c.member !== undefined)
        members.set(c.variant, c.member);
      if (c.level !== undefined && Number.isFinite(c.level))
      {
        levels.set(c.variant, {level: c.level, _levelLabel: c.levelLabel || String(c.level)});
        series.set(c.variant, c.series || '');
      }
    }
    if (variants.size === 0)
      out.push(row);
    for (const [variant, values] of variants)
    {
      const name = series.has(variant) ? series.get(variant) : variant;
      out.push(Object.assign({}, base, values, name ? {_variant: name} : {},
                             members.has(variant) ? {_member: members.get(variant)} : {},
                             levels.has(variant) ? levels.get(variant) : {}));
    }
  }
  return out;
}

// What kind of quantity a parameter is, for choosing how its values are aggregated over time:
// 'amount' (precipitation and other accumulations: summed), 'direction' (mean of angles),
// 'category' (symbols and classes: not to be averaged), 'maximum', 'minimum' or 'state'
// (temperature, humidity, ...: averaged). info is what the grid details tell of the parameter.
function parameterKind(key, entry, info)
{
  const name = `${key} ${(entry && entry.name) || ''}`.toLowerCase();
  const text = `${name} ${(entry && entry.description) || ''}`.toLowerCase();
  const unit = ((entry && entry.unit) || '').toLowerCase();
  const aggregation = info && info.aggregation ? info.aggregation.name : '';
  if (aggregation === 'accumulation')
    return 'amount';
  if (aggregation === 'max')
    return 'maximum';
  if (aggregation === 'min')
    return 'minimum';
  if (/^deg$|^degrees?$|^°$/.test(unit) || /direction|winddir|^dd\b|\bdd-d\b/.test(text))
    return 'direction';
  if (/symbol|wawa|\bww\b|class|type\b|\bcode\b|form\b|fogint|cldsym/.test(text))
    return 'category';
  if (/precipitation|rainfall|snowfall|precip|\brr\b|\brrr|accum|amount/.test(text) ||
      /^mm$|^kg m-2$|^kg\/m2$|^kgm2$/.test(unit) || /-(mm|kgm2)\b/.test(name))
    return 'amount';
  if (/gust|maximum|\bmax|tmax|-max/.test(text))
    return 'maximum';
  if (/minimum|\bmin|tmin|-min/.test(text))
    return 'minimum';
  return 'state';
}

// The functions over a period offered by the explorer: label, the TimeSeries function, and
// for directions its circular variant
const TIME_FUNCTIONS = [
  ['mean', L('Mean'), 'mean_t', 'meandir_t'],
  ['min', L('Minimum'), 'min_t'],
  ['max', L('Maximum'), 'max_t'],
  ['sum', L('Sum'), 'sum_t'],
  ['median', L('Median'), 'median_t'],
  ['percentile', L('Percentile'), 'percentile_t'],
  ['sdev', L('Standard deviation'), 'sdev_t', 'sdevdir_t'],
  ['change', L('Change'), 'change_t']
];

// The function recommended for a kind of parameter
const RECOMMENDED_FUNCTION = {
  amount: 'sum',
  direction: 'mean',
  category: 'max',
  maximum: 'max',
  minimum: 'min',
  state: 'mean'
};

// The functions over an area offered by the explorer, computing one value of all the points
// of the area at each time: label, the TimeSeries function, and for directions its circular
// variant. Missing values are skipped.
const AREA_FUNCTIONS = [
  ['mean', L('Mean'), 'nanmean_a', 'nancirclemean_a'],
  ['min', L('Minimum'), 'nanmin_a'],
  ['max', L('Maximum'), 'nanmax_a'],
  ['median', L('Median'), 'nanmedian_a'],
  ['sum', L('Sum'), 'nansum_a'],
  ['sdev', L('Standard deviation'), 'nansdev_a']
];

// The function over an area recommended for a kind of parameter: an amount is averaged over
// the area like a state, a category gives its most severe class
const RECOMMENDED_AREA_FUNCTION = {
  amount: 'mean',
  direction: 'mean',
  category: 'max',
  maximum: 'max',
  minimum: 'min',
  state: 'mean'
};

// Why a function over an area does not suit a kind of parameter, or ''
function areaFunctionWarning(fn, kind)
{
  if (kind === 'category' && !['max', 'min'].includes(fn))
    return L('is a category: its mean or sum means nothing, the maximum gives the most severe class');
  if (kind === 'direction' && fn !== 'mean')
    return L('is a direction: only its mean, a circular mean, means something');
  if (kind !== 'amount' && fn === 'sum')
    return L('is not an amount: its sum means nothing');
  return '';
}

// Why a function does not suit a kind of parameter, or ''
function functionWarning(fn, kind)
{
  if (kind === 'category' && fn !== 'max' && fn !== 'min')
    return L('is a category: its mean or sum means nothing, the maximum gives the most severe class');
  if (kind === 'direction' && (fn === 'sum' || fn === 'change'))
    return L('is a direction: its sum or change means nothing');
  if (kind === 'amount' && fn === 'mean')
    return L('is an amount: the sum gives the total of the period');
  if (kind !== 'amount' && fn === 'sum')
    return L('is not an amount: its sum means nothing');
  return '';
}

// "1-3,7" -> [1, 2, 3, 7], limited to the allowed numbers; null if the text is not a list
function parseNumberList(text, allowed)
{
  const out = new Set();
  for (const part of String(text || '').split(','))
  {
    const p = part.trim();
    if (p === '')
      continue;
    const m = /^(-?\d+)(?:\s*-\s*(-?\d+))?$/.exec(p);
    if (!m)
      return null;
    const a = Number(m[1]);
    const b = m[2] !== undefined ? Number(m[2]) : a;
    for (const n of allowed)
      if (n >= Math.min(a, b) && n <= Math.max(a, b))
        out.add(n);
  }
  return allowed.filter((n) => out.has(n));
}

// The q quantile (0...1) of sorted numbers, interpolated linearly; null for none
function quantile(sorted, q)
{
  if (sorted.length === 0)
    return null;
  const pos = (sorted.length - 1) * q;
  const lo = Math.floor(pos);
  const hi = Math.ceil(pos);
  return sorted[lo] + (sorted[hi] - sorted[lo]) * (pos - lo);
}

// Readable on both themes
const palette = [
  '#2f6fdf',
  '#d9480f',
  '#2b8a3e',
  '#ae3ec9',
  '#e67700',
  '#0c8599',
  '#c2255c',
  '#5c940d',
  '#5f3dc4',
  '#868e96'
];

// ------------------------------------------------------------------
// Autocomplete
//
// An input with a list of suggestions. 'search(text)' returns the
// suggestions: {key, label, detail, badge}. 'select(item)' is called when
// one is chosen with the mouse or the keyboard.
// ------------------------------------------------------------------

// A search field with a list of suggestions. options: label, placeholder, search(text) -> the
// matching items (or a promise of them), select(item) when one is chosen, empty(text) for a note
// when nothing matches, limit (30 items), clearOnSelect (true), input (an existing field to
// suggest for instead of a new one). The list is navigated with the arrow keys and Enter.
// Returns {element, input, refresh}.
function autocomplete(options)
{
  const input = options.input || el('input', {
    type: 'search',
    class: 'ac-input',
    placeholder: options.placeholder,
    autocomplete: 'off',
    spellcheck: 'false',
    'aria-label': options.label
  });
  input.setAttribute('aria-autocomplete', 'list');
  const list = el('ul', {class: 'ac-list', role: 'listbox', hidden: true});
  const box = el('div', {class: 'ac'}, input, list);
  let items = [];
  let active = -1;
  // Closed after a choice until the user types again
  let closed = false;

  // Draws the list of suggestions, the active one highlighted
  function render()
  {
    list.replaceChildren();
    items.forEach((item, i) => {
      const li = el(
          'li',
          {
            role: 'option',
            class: i === active ? 'active' : '',
            'aria-selected': i === active ? 'true' : 'false',
            onmousedown: (e) => {
              e.preventDefault();
              choose(i);
            }
          },
          el('span', {class: 'ac-label'}, item.label),
          item.badge ? el('span', {class: `badge badge-${item.badgeKind || 'plain'}`}, item.badge) :
                       null,
          item.detail ? el('span', {class: 'ac-detail'}, item.detail) : null);
      list.append(li);
    });
    if (items.length === 0 && input.value.trim() !== '' && options.empty)
      list.append(el('li', {class: 'ac-empty'}, options.empty(input.value)));
    list.hidden = closed || (items.length === 0 && !(input.value.trim() !== '' && options.empty)) ||
        document.activeElement !== input;
  }

  // Searches with the text of the field and shows the matches; an answer to an older text is
  // left out
  let searches = 0;
  async function update()
  {
    const search = ++searches;
    const found = await options.search(input.value.trim());
    if (search !== searches)
      return;
    items = (found || []).slice(0, options.limit || 30);
    active = items.length > 0 ? 0 : -1;
    render();
  }

  // Chooses suggestion i
  function choose(i)
  {
    const item = items[i];
    if (!item)
      return;
    options.select(item);
    if (options.clearOnSelect !== false)
      input.value = '';
    closed = true;
    update();
  }

  const updateSoon = debounce(update, 60);
  input.addEventListener('input', () => {
    closed = false;
    updateSoon();
  });
  input.addEventListener('focus', () => {
    closed = false;
    update();
  });
  input.addEventListener('blur', () => {
    list.hidden = true;
  });
  input.addEventListener('keydown', (e) => {
    if (e.key === 'ArrowDown')
    {
      closed = false;
      active = Math.min(items.length - 1, active + 1);
      render();
      e.preventDefault();
    }
    else if (e.key === 'ArrowUp')
    {
      active = Math.max(0, active - 1);
      render();
      e.preventDefault();
    }
    else if (e.key === 'Enter')
    {
      if (active >= 0 && !list.hidden)
      {
        choose(active);
        e.preventDefault();
      }
    }
    else if (e.key === 'Escape')
    {
      closed = true;
      list.hidden = true;
    }
  });
  return {element: box, input: input, refresh: update};
}

// ------------------------------------------------------------------
// The parameter index of a TimeSeries API
// ------------------------------------------------------------------

// Meta parameters every producer has
// The parameters of the TimeSeries API that are not data, by what they tell; sources limits a
// group to producers of those sources
const META_GROUPS = [
  {
    title: L('The data'),
    sources: ['querydata', 'grid'],
    params: [
      ['producer', L('Producer of the value')],
      ['model', L('Model of the value')],
      ['origintime', L('Origin time of the model run')],
      ['modtime', L('Modification time of the data')],
      ['level', L('Level')]
    ]
  },
  {
    title: L('The location'),
    params: [
      ['name', L('Location name')],
      ['lat', L('Latitude')],
      ['lon', L('Longitude')],
      ['latlon', L('Latitude and longitude')],
      ['geoid', L('Location id')],
      ['elevation', L('Elevation of the location')],
      ['dem', L('Elevation from the elevation model')],
      ['region', L('Region')],
      ['country', L('Country')],
      ['iso2', L('Country code')],
      ['localtz', L('Time zone of the location')],
      ['population', L('Population')]
    ]
  },
  {
    title: L('The station'),
    sources: ['observation'],
    params: [
      ['fmisid', L('FMI station id')],
      ['wmo', L('WMO station id')],
      ['lpnn', L('LPNN station id')],
      ['stationname', L('Name of the observation station')],
      ['stationlat', L('Latitude of the station')],
      ['stationlon', L('Longitude of the station')],
      ['station_elevation', L('Elevation of the station')],
      ['distance', L('Distance to the station')],
      ['direction', L('Direction to the station')],
      ['stationtype', L('Type of the station')],
      ['modifiedlast', L('Last modification of the value')]
    ]
  },
  {
    title: L('The time and the sun'),
    params: [
      ['time', L('Time')],
      ['localtime', L('Local time')],
      ['epochtime', L('Seconds since 1970')],
      ['weekday', L('Day of the week')],
      ['sunelevation', L('Elevation angle of the sun')],
      ['sunazimuth', L('Azimuth of the sun')],
      ['dark', L('Dark (1) or light (0)')],
      ['sunrise', L('Time of sunrise')],
      ['sunset', L('Time of sunset')],
      ['daylength', L('Length of the day')],
      ['moonphase', L('Phase of the moon')]
    ]
  }
];

const META_PARAMETERS = META_GROUPS.flatMap((g) => g.params);

// The details of the grid data of a column N, requested as @code-N
const GRID_DETAILS = [
  ['AT', 'model run', L('Analysis time of the model run')],
  ['LT', 'level type', L('Level type')],
  ['L', 'level', L('Level in the units of parameter keys')],
  ['FT', 'forecast type', L('Forecast type')],
  ['FN', 'member', L('Forecast (ensemble member) number')],
  ['GM', 'geometry', L('Geometry id of the grid')],
  ['P', 'producer', L('Producer')],
  ['G', 'generation', L('Generation (model run) name')],
  ['I', 'parameter key', L('The full parameter key')],
  ['F', 'file', L('The grid file')],
  ['LATLON', 'grid point', L('Coordinates of the grid point(s) used')]
];
const META = new Map(META_PARAMETERS);

// The same producer name may come from several sources, for example querydata and grid
const producerKey = (source, name) => (source ? `${source}:${name}` : name);

// The index of the TimeSeries producer listing (<ts_url>/api/producers): the producers by
// source:name, and the parameters with the producers having them, for the faceted search
function buildIndex(doc)
{
  const producers = new Map();
  const sources = new Map();
  for (const p of doc.producers || [])
  {
    const key = producerKey(p.source, p.name);
    producers.set(key, Object.assign({}, p, {key, parameterSet: new Set(p.parameters || [])}));
    sources.set(p.name, (sources.get(p.name) || 0) + 1);
  }
  for (const p of producers.values())
    p.ambiguous = sources.get(p.name) > 1;
  const parameters = new Map();
  for (const p of producers.values())
  {
    for (const name of p.parameters || [])
    {
      let entry = parameters.get(name);
      if (!entry)
      {
        const info = (doc.parameters || {})[name] || {};
        entry = {
          key: name,
          name: info.name || name,
          description: info.description || '',
          unit: info.unit || '',
          producers: []
        };
        parameters.set(name, entry);
      }
      entry.producers.push(p.key);
    }
  }
  return {producers, parameters};
}

// Score of a match of 'q' (lower case) in an item: lower is better, null for no match
function matchScore(q, name, text)
{
  if (!q)
    return 3;
  const n = name.toLowerCase();
  if (n === q)
    return 0;
  if (n.startsWith(q))
    return 1;
  const t = text.toLowerCase();
  if (t.split(/[\s_\-/().,]+/).some((w) => w.startsWith(q)))
    return 2;
  if (n.includes(q) || t.includes(q))
    return 3;
  return null;
}

// ------------------------------------------------------------------
// Map settings, production palettes and the basemap
// ------------------------------------------------------------------

// The settings of maps and colours from the data attributes of the explorer element: map tiles
// or the built-in basemap, and the production palettes
function mapSettings(app)
{
  const d = app.dataset;
  const settings = d.mapTiles ?
      {
        tiles: d.mapTiles,
        tileSize: Number(d.mapTileSize) || 256,
        attribution: d.mapAttribution || ''
      } :
      (d.basemap ? {basemap: d.basemap, basemapDetail: d.basemapDetail || '', attribution: 'Natural Earth'} : {});
  if (d.palettes)
    settings.palettes = d.palettes;
  return settings;
}

// The defaults of the server for its region and users (api_catalog.explorer_defaults): the
// first view of the maps, the examples of the locations, the output projections and units
let DEFAULTS = {};
// The first view of the maps without defaults: the world
const WORLD_VIEW = {center: [0, 20], zoom: 1};

// The isoband colours of the production products (palettes.json): loaded once
let palettesPromise = null;

// The production palettes (palettes.json), loaded once and shared; none if missing
function loadPalettes(url)
{
  if (!palettesPromise)
    palettesPromise = fetchJson(url).catch(() => ({palettes: {}, parameters: {}}));
  return palettesPromise;
}

// The background map of the explorer (basemap.json, see make-basemap.py): land, lakes and
// country borders as rings in Web Mercator world coordinates with their bounding boxes.
// Loaded once.
const basemapPromises = new Map();  // url -> the decoded basemap

// A basemap, loaded once and shared, its rings decoded into coordinate arrays
function loadBasemap(url)
{
  if (!basemapPromises.has(url))
    basemapPromises.set(url, fetchJson(url).then((doc) => {
      const scale = doc.scale || 100;
      const decode = (rings) => rings.map((e) => {
        const n = e.length / 2;
        const xy = new Float64Array(e.length);
        let lon = 0;
        let lat = 0;
        let x0 = Infinity;
        let y0 = Infinity;
        let x1 = -Infinity;
        let y1 = -Infinity;
        for (let i = 0; i < n; i++)
        {
          lon += e[2 * i];
          lat += e[2 * i + 1];
          const [x, y] = project(lon / scale, lat / scale);
          xy[2 * i] = x;
          xy[2 * i + 1] = y;
          x0 = Math.min(x0, x);
          y0 = Math.min(y0, y);
          x1 = Math.max(x1, x);
          y1 = Math.max(y1, y);
        }
        return {xy, box: [x0, y0, x1, y1]};
      });
      return {
        land: decode(doc.land || []),
        lakes: decode(doc.lakes || []),
        borders: decode(doc.borders || [])
      };
    }));
  return basemapPromises.get(url);
}

// ------------------------------------------------------------------
// The selection map
//
// An SVG map in Web Mercator with the tiles of the map_tiles setting, or a
// longitude/latitude grid. It shows the areas of the producers, the selected
// point, radius or box, and locations with tooltips, and lets the user choose
// a point (click), a radius (drag from the centre) or a box (drag).
// ------------------------------------------------------------------

const SVG_NS = 'http://www.w3.org/2000/svg';
const EARTH_RADIUS_KM = 6371;
const MAP_HEIGHT = 300;

// An SVG element with attributes, like el() for HTML
function svg(tag, attrs)
{
  const node = document.createElementNS(SVG_NS, tag);
  for (const [k, v] of Object.entries(attrs || {}))
    node.setAttribute(k, String(v));
  return node;
}

// Web Mercator world coordinates of zoom level 0 (a 256 pixel world)
function project(lon, lat)
{
  const s = Math.sin((Math.max(-85.05, Math.min(85.05, lat)) * Math.PI) / 180);
  return [((lon + 180) / 360) * 256, (0.5 - Math.log((1 + s) / (1 - s)) / (4 * Math.PI)) * 256];
}

// Web Mercator world pixels (256 at zoom 0) to [lon, lat]
function unproject(x, y)
{
  const n = Math.PI - (2 * Math.PI * y) / 256;
  return [(x / 256) * 360 - 180, (180 / Math.PI) * Math.atan(Math.sinh(n))];
}

const rad = (d) => (d * Math.PI) / 180;
const deg = (r) => (r * 180) / Math.PI;

// Great circle distance in km
function distanceKm(lon1, lat1, lon2, lat2)
{
  const a = Math.sin(rad(lat2 - lat1) / 2) ** 2 +
      Math.cos(rad(lat1)) * Math.cos(rad(lat2)) * Math.sin(rad(lon2 - lon1) / 2) ** 2;
  return 2 * EARTH_RADIUS_KM * Math.asin(Math.min(1, Math.sqrt(a)));
}

// The point at a distance and a bearing from a point
function destination(lon, lat, km, bearing)
{
  const d = km / EARTH_RADIUS_KM;
  const b = rad(bearing);
  const lat2 =
      Math.asin(Math.sin(rad(lat)) * Math.cos(d) + Math.cos(rad(lat)) * Math.sin(d) * Math.cos(b));
  const lon2 = rad(lon) +
      Math.atan2(
          Math.sin(b) * Math.sin(d) * Math.cos(rad(lat)),
          Math.cos(d) - Math.sin(rad(lat)) * Math.sin(lat2));
  return [deg(lon2), deg(lat2)];
}

// The bounding box of a selection: [minlon, minlat, maxlon, maxlat]
function selectionBox(sel)
{
  if (!sel)
    return null;
  if (sel.type === 'box')
    return sel.bbox;
  if (sel.type === 'point')
    return [sel.lon, sel.lat, sel.lon, sel.lat];
  if (sel.type === 'circle')
  {
    const n = destination(sel.lon, sel.lat, sel.km, 0);
    const e = destination(sel.lon, sel.lat, sel.km, 90);
    return [2 * sel.lon - e[0], 2 * sel.lat - n[1], e[0], n[1]];
  }
  const points = sel.type === 'path' ? sel.points : sel.type === 'wkt' ? wktPoints(sel) : [];
  if (points.length > 0)
  {
    const lons = points.map((p) => p[0]);
    const lats = points.map((p) => p[1]);
    return [Math.min(...lons), Math.min(...lats), Math.max(...lons), Math.max(...lats)];
  }
  return null;
}

// The length of a path in kilometres
function pathLength(points)
{
  let km = 0;
  for (let i = 1; i < points.length; i++)
    km += distanceKm(points[i - 1][0], points[i - 1][1], points[i][0], points[i][1]);
  return km;
}

// Whether two [minlon, minlat, maxlon, maxlat] boxes overlap
function boxesIntersect(a, b)
{
  return a[0] <= b[2] && b[0] <= a[2] && a[1] <= b[3] && b[1] <= a[3];
}

const round = (v, decimals) => Number(v.toFixed(decimals));

// The map of the explorer: an SVG Web Mercator map over tiles or the basemap, with tools to
// pick a point, drag a radius or a box, or click the vertices of a path. onSelect(shape) is
// called with each drawn selection. Overlays: the areas of the data (regions), the selection,
// points (stations, results), coloured values (cells) and a cursor. Returns the methods that
// set them, the tools and the view.
function createMap(settings, onSelect)
{
  const box = el('div', {class: 'xmap'});
  const surface = svg('svg', {class: 'xmap-svg', role: 'img', 'aria-label': L('Map')});
  const layers = {
    background: svg('g'),
    raster: svg('g'),
    cells: svg('g'),
    regions: svg('g'),
    selection: svg('g'),
    points: svg('g'),
    cursor: svg('g'),
    draft: svg('g')
  };
  for (const layer of Object.values(layers))
    surface.append(layer);
  const tooltip = el('div', {class: 'xmap-tooltip', hidden: true});
  const tools = el('div', {class: 'xmap-tools', role: 'toolbar', 'aria-label': L('Map tools')});
  const zoom = el('div', {class: 'xmap-zoom'});
  const attribution = settings && settings.attribution ?
      el('div', {class: 'xmap-attribution'}, settings.attribution) :
      null;
  box.append(surface, tooltip, tools, zoom);
  if (attribution)
    box.append(attribution);

  const tileSize = (settings && settings.tileSize) || 256;
  // The basemap, and a more detailed one (basemapDetail) loaded when the map is expanded or
  // zoomed in, and drawn then
  let basemap = null;
  let detailedBasemap = null;
  let detailRequested = false;
  const DETAIL_ZOOM = 5;
  if (settings && settings.basemap && !settings.tiles)
    loadBasemap(settings.basemap)
        .then((map) => {
          basemap = map;
          redraw();
        })
        .catch(
            () => {
                // The grid alone is drawn
            });
  // The details are drawn in an expanded map from continents closer, or when zoomed in
  const wantsDetail = () =>
      (box.classList.contains('xmap-expanded') && state.zoom >= 3) || state.zoom >= DETAIL_ZOOM;
  function requestDetail()
  {
    if (detailRequested || !settings || !settings.basemapDetail || settings.tiles)
      return;
    detailRequested = true;
    loadBasemap(settings.basemapDetail)
        .then((map) => {
          detailedBasemap = map;
          redraw();
        })
        .catch(
            () => {
                // The basemap is drawn
            });
  }
  const first = DEFAULTS.center ? {center: DEFAULTS.center, zoom: DEFAULTS.zoom !== undefined ? DEFAULTS.zoom : 4} :
                                  WORLD_VIEW;
  const state = {
    zoom: first.zoom,
    center: project(first.center[0], first.center[1]),
    tool: 'move',
    regions: [],
    selection: null,
    points: []
  };
  // Listeners of the view (a map following another) and of clicks in the move tool
  const hooks = {view: null, click: null, shown: ''};

  const width = () => box.clientWidth || 400;
  const height = () => box.clientHeight || MAP_HEIGHT;
  const scale = () => 2 ** state.zoom;
  // World pixels (Web Mercator, 256 at zoom 0) to the pixels of the view, and back
  const toScreen = ([x, y]) =>
      [(x - state.center[0]) * scale() + width() / 2,
       (y - state.center[1]) * scale() + height() / 2];
  const toWorld = (sx, sy) =>
      [state.center[0] + (sx - width() / 2) / scale(),
       state.center[1] + (sy - height() / 2) / scale()];
  const screenOf = (lon, lat) => toScreen(project(lon, lat));
  // The world is at least as wide as the view, and the view stays in the world: beyond
  // 180 degrees there would be only empty space
  const minZoom = () => Math.max(0, Math.log2(width() / 256));
  const clampView = () => {
    state.zoom = Math.max(minZoom(), state.zoom);
    const hw = width() / 2 / scale();
    const hh = height() / 2 / scale();
    state.center[0] = Math.min(256 - hw, Math.max(hw, state.center[0]));
    state.center[1] = hh >= 128 ? 128 : Math.min(256 - hh, Math.max(hh, state.center[1]));
  };
  const lonLatOf = (sx, sy) => unproject(...toWorld(sx, sy));
  // The pixel of a longitude and latitude in the map, for scripts driving the page
  box.screenOf = (lon, lat) => screenOf(lon, lat);

  // The land, the lakes and the borders in view, with at most a point per pixel
  function drawBasemap(g, x0, y0, x1, y1)
  {
    const k = scale();
    const cx = state.center[0];
    const cy = state.center[1];
    const w2 = width() / 2;
    const h2 = height() / 2;
    // The SVG path of the rings of the basemap that are in the view, in pixels of the view
    const path = (rings, closed) => {
      let d = '';
      for (const ring of rings)
      {
        const b = ring.box;
        if (b[2] < x0 || b[0] > x1 || b[3] < y0 || b[1] > y1)
          continue;
        // Rings smaller than a pixel are left out
        if ((b[2] - b[0]) * k < 1 && (b[3] - b[1]) * k < 1)
          continue;
        const xy = ring.xy;
        let px = NaN;
        let py = NaN;
        for (let i = 0; i < xy.length; i += 2)
        {
          const sx = Math.round((xy[i] - cx) * k + w2);
          const sy = Math.round((xy[i + 1] - cy) * k + h2);
          if (sx === px && sy === py)
            continue;
          d += (i === 0 ? 'M' : 'L') + sx + ' ' + sy;
          px = sx;
          py = sy;
        }
        if (closed)
          d += 'Z';
      }
      return d;
    };
    if (wantsDetail())
      requestDetail();
    const map = (wantsDetail() && detailedBasemap) || basemap;
    const land = path(map.land, true);
    if (land)
      g.append(svg('path', {class: 'xmap-land', d: land}));
    const lakes = path(map.lakes, true);
    if (lakes)
      g.append(svg('path', {class: 'xmap-lake', d: lakes}));
    const borders = path(map.borders, false);
    if (borders)
      g.append(svg('path', {class: 'xmap-border', d: borders}));
  }

  // The background: the tiles, or the land, lakes and borders of the basemap
  function drawBackground()
  {
    const g = layers.background;
    g.replaceChildren();
    const [x0, y0] = toWorld(0, 0);
    const [x1, y1] = toWorld(width(), height());
    if (settings && settings.tiles)
    {
      const z = Math.max(0, Math.min(18, Math.round(state.zoom + Math.log2(256 / tileSize))));
      const n = 2 ** z;
      const tw = 256 / n;
      for (let ty = Math.max(0, Math.floor(y0 / tw)); ty <= Math.min(n - 1, Math.floor(y1 / tw));
           ty++)
        for (let tx = Math.floor(x0 / tw); tx <= Math.floor(x1 / tw); tx++)
        {
          const [sx, sy] = toScreen([tx * tw, ty * tw]);
          const wrapped = ((tx % n) + n) % n;
          g.append(svg('image', {
            x: sx,
            y: sy,
            width: tw * scale() + 0.5,
            height: tw * scale() + 0.5,
            href: settings.tiles.replace('{z}', z).replace('{x}', wrapped).replace('{y}', ty)
          }));
        }
      return;
    }
    if (basemap)
      drawBasemap(g, x0, y0, x1, y1);
    // A longitude/latitude grid with a step suited to the zoom
    const [lonW, latN] = unproject(x0, y0);
    const [lonE, latS] = unproject(x1, y1);
    const steps = [90, 45, 30, 15, 10, 5, 2, 1, 0.5, 0.25, 0.1, 0.05];
    const step = steps.find((st) => (lonE - lonW) / st >= 4) || steps[steps.length - 1];
    for (let lon = Math.ceil(lonW / step) * step; lon <= lonE; lon += step)
    {
      const [sx] = screenOf(lon, 0);
      g.append(svg('line', {class: 'xmap-grid', x1: sx, y1: 0, x2: sx, y2: height()}));
      const t = svg('text', {class: 'xmap-label', x: sx + 2, y: height() - 4});
      t.textContent = `${round(lon, 2)}°`;
      g.append(t);
    }
    for (let lat = Math.ceil(latS / step) * step; lat <= latN; lat += step)
    {
      const [, sy] = screenOf(0, lat);
      g.append(svg('line', {class: 'xmap-grid', x1: 0, y1: sy, x2: width(), y2: sy}));
      const t = svg('text', {class: 'xmap-label', x: 2, y: sy - 2});
      t.textContent = `${round(lat, 2)}°`;
      g.append(t);
    }
  }

  // A box in longitude and latitude as an SVG rectangle
  function rect(bbox, cls)
  {
    const [ax, ay] = screenOf(bbox[0], bbox[3]);
    const [bx, by] = screenOf(bbox[2], bbox[1]);
    return svg('rect', {
      class: cls,
      x: Math.min(ax, bx),
      y: Math.min(ay, by),
      width: Math.abs(bx - ax),
      height: Math.abs(by - ay)
    });
  }

  // A circle of km kilometres around a point, as a polygon of points every 5 degrees
  function circlePath(sel, cls)
  {
    const points = [];
    for (let b = 0; b <= 360; b += 5)
      points.push(screenOf(...destination(sel.lon, sel.lat, sel.km, b)).join(','));
    return svg('polygon', {class: cls, points: points.join(' ')});
  }

  // Draws a selection: a box, a circle, a point, or a path with its vertices
  function drawShape(g, sel, cls)
  {
    if (!sel)
      return;
    if (sel.type === 'box')
      g.append(rect(sel.bbox, cls));
    else if (sel.type === 'circle')
      g.append(circlePath(sel, cls));
    else if (sel.type === 'path')
    {
      // The legs of the path and its vertices
      g.append(svg('polyline', {
        class: `${cls} xmap-path`,
        fill: 'none',
        points: sel.points.map(([lon, lat]) => screenOf(lon, lat).join(',')).join(' ')
      }));
      sel.points.forEach(([lon, lat], i) => {
        const [sx, sy] = screenOf(lon, lat);
        g.append(svg('circle', {class: i === 0 ? 'xmap-centre start' : 'xmap-centre', cx: sx, cy: sy, r: 4}));
      });
    }
    else if (sel.type === 'wkt')
    {
      // The polygons with their holes, the lines and the points of a WKT geometry
      const xy = (points) => points.map(([lon, lat]) => screenOf(lon, lat).join(',')).join(' ');
      for (const part of sel.parts)
        if (part.kind === 'polygon')
          g.append(svg('path', {
            class: cls,
            'fill-rule': 'evenodd',
            d: part.rings.map((ring) => `M${xy(ring).replace(/ /g, 'L')}Z`).join('')
          }));
        else if (part.kind === 'line')
          g.append(svg('polyline', {class: `${cls} xmap-path`, fill: 'none', points: xy(part.points)}));
        else
        {
          const [sx, sy] = screenOf(...part.points[0]);
          g.append(svg('circle', {class: 'xmap-centre', cx: sx, cy: sy, r: 3}));
        }
    }
    if (sel.type === 'point' || sel.type === 'circle')
    {
      const [sx, sy] = screenOf(sel.lon, sel.lat);
      g.append(svg('circle', {class: 'xmap-centre', cx: sx, cy: sy, r: 5}));
    }
  }

  // Draws everything on the background: the regions, the selection, the values, the cursor and the
  // points
  function drawOverlays()
  {
    layers.regions.replaceChildren();
    for (const region of state.regions)
    {
      // A box, or a polygon in longitude and latitude
      const r = region.polygon ? svg('polygon', {
        class: region.cls || 'xmap-region',
        points: region.polygon.map(([lon, lat]) => screenOf(lon, lat).join(',')).join(' ')
      }) :
                                 rect(region.bbox, region.cls || 'xmap-region');
      const title = svg('title');
      title.textContent = region.label;
      r.append(title);
      layers.regions.append(r);
    }
    layers.selection.replaceChildren();
    drawShape(layers.selection, state.selection, 'xmap-selection');

    // Values as coloured cells (a grid) or dots (stations)
    layers.cells.replaceChildren();
    if (state.cells)
    {
      const {items, dx, dy} = state.cells;
      for (const cell of items)
      {
        if (cell.color === null)
          continue;
        let shape;
        if (dx > 0)
        {
          const [ax, ay] = screenOf(cell.lon - dx / 2, cell.lat + dy / 2);
          const [bx, by] = screenOf(cell.lon + dx / 2, cell.lat - dy / 2);
          if (bx < 0 || by < 0 || ax > width() || ay > height())
            continue;
          shape = svg('rect', {
            class: 'xmap-cell',
            x: ax,
            y: ay,
            width: Math.max(1, bx - ax + 0.5),
            height: Math.max(1, by - ay + 0.5),
            fill: cell.color
          });
        }
        else
        {
          const [sx, sy] = screenOf(cell.lon, cell.lat);
          if (sx < -10 || sy < -10 || sx > width() + 10 || sy > height() + 10)
            continue;
          shape = svg('circle', {class: 'xmap-dot', cx: sx, cy: sy, r: 6, fill: cell.color});
        }
        shape.addEventListener('mouseover', () => {
          const [sx, sy] = screenOf(cell.lon, cell.lat);
          showTooltip({label: cell.label, info: cell.info, hint: cell.onclick ? cell.hint : ''}, sx, sy);
        });
        shape.addEventListener('mouseout', () => {
          tooltip.hidden = true;
        });
        // A cell that can be clicked does not start dragging the map
        if (cell.onclick)
        {
          shape.classList.add('clickable');
          shape.addEventListener('pointerdown', (e) => e.stopPropagation());
          shape.addEventListener('click', (e) => {
            e.stopPropagation();
            cell.onclick();
          });
        }
        layers.cells.append(shape);
      }
    }

    drawCursor();
    layers.points.replaceChildren();
    // Many points are drawn smaller
    const radius = state.points.length > 500 ? 2 : state.points.length > 100 ? 3 : 4.5;
    for (const point of state.points)
    {
      const [sx, sy] = screenOf(point.lon, point.lat);
      if (sx < -10 || sy < -10 || sx > width() + 10 || sy > height() + 10)
        continue;
      const c = svg('circle', {
        class: `xmap-point${point.selected ? ' selected' : ''}${point.onclick ? ' clickable' : ''}`,
        cx: sx,
        cy: sy,
        r: point.selected ? radius + 1.5 : radius
      });
      // A point that can be chosen does not start dragging or drawing
      if (point.onclick)
      {
        c.addEventListener('pointerdown', (e) => e.stopPropagation());
        c.addEventListener('click', (e) => {
          e.stopPropagation();
          point.onclick();
        });
      }
      c.addEventListener('mouseover', () => showTooltip(point, sx, sy));
      c.addEventListener('mouseout', () => {
        tooltip.hidden = true;
      });
      layers.points.append(c);
    }
  }

  // A position being shown elsewhere, e.g. the point of an animation along a path
  function drawCursor()
  {
    layers.cursor.replaceChildren();
    if (!state.cursor)
      return;
    const [sx, sy] = screenOf(state.cursor.lon, state.cursor.lat);
    layers.cursor.append(svg('circle', {class: 'xmap-cursor', cx: sx, cy: sy, r: 7}));
  }

  // The name and the details of a point next to it
  function showTooltip(point, sx, sy)
  {
    const rows = Object.entries(point.info || {})
                     .filter(([k, v]) => k !== 'name' && v !== null && v !== undefined && v !== '')
                     .map(([k, v]) => el('tr', null, el('td', null, k), el('td', null, v)));
    tooltip.replaceChildren(el('strong', null, point.label || ''), el('table', null, rows),
                            point.hint ? el('div', {class: 'xmap-hint'}, point.hint) : '');
    tooltip.hidden = false;
    const left = sx + 12 + tooltip.offsetWidth > width() ? sx - 12 - tooltip.offsetWidth : sx + 12;
    tooltip.style.left = `${Math.max(0, left)}px`;
    tooltip.style.top = `${Math.max(0, Math.min(height() - tooltip.offsetHeight, sy - 10))}px`;
  }

  // One redraw per animation frame while the map is dragged
  let pending = false;
  // Redraws once at the next animation frame, however many times asked meanwhile
  function scheduleRedraw()
  {
    if (pending)
      return;
    pending = true;
    requestAnimationFrame(() => {
      pending = false;
      redraw();
    });
  }

  function redraw()
  {
    clampView();
    surface.setAttribute('viewBox', `0 0 ${width()} ${height()}`);
    drawBackground();
    drawRaster();
    drawOverlays();
    // A changed view is told to the maps following this one
    const view = `${state.zoom},${state.center[0]},${state.center[1]}`;
    if (view !== hooks.shown)
    {
      hooks.shown = view;
      if (hooks.view)
        hooks.view({zoom: state.zoom, center: state.center.slice()});
    }
  }

  // A raster of values in longitude and latitude, resampled to the pixels of the view: each
  // pixel takes the colour of the cell under it. The colours of the cells are computed once.
  function drawRaster()
  {
    layers.raster.replaceChildren();
    const r = state.raster;
    if (!r)
      return;
    const [west, south, east, north] = r.bbox;
    const [ax, ay] = screenOf(west, north);
    const [bx, by] = screenOf(east, south);
    const x0 = Math.max(0, Math.floor(ax));
    const y0 = Math.max(0, Math.floor(ay));
    const x1 = Math.min(width(), Math.ceil(bx));
    const y1 = Math.min(height(), Math.ceil(by));
    if (x1 <= x0 || y1 <= y0 || typeof document === 'undefined')
      return;
    const canvas = document.createElement('canvas');
    canvas.width = x1 - x0;
    canvas.height = y1 - y0;
    const ctx = canvas.getContext('2d');
    if (!ctx)
      return;
    const image = ctx.createImageData(canvas.width, canvas.height);
    // Longitude depends on the column only, latitude on the row only
    const columns = new Int32Array(canvas.width);
    for (let i = 0; i < canvas.width; i++)
    {
      const lon = lonLatOf(x0 + i + 0.5, 0)[0];
      columns[i] = Math.floor(((lon - west) / (east - west)) * r.width);
    }
    for (let j = 0; j < canvas.height; j++)
    {
      const lat = lonLatOf(0, y0 + j + 0.5)[1];
      const row = Math.floor(((north - lat) / (north - south)) * r.height);
      if (row < 0 || row >= r.height)
        continue;
      for (let i = 0; i < canvas.width; i++)
      {
        const col = columns[i];
        if (col < 0 || col >= r.width)
          continue;
        const k = 4 * (row * r.width + col);
        const p = 4 * (j * canvas.width + i);
        image.data[p] = r.rgba[k];
        image.data[p + 1] = r.rgba[k + 1];
        image.data[p + 2] = r.rgba[k + 2];
        image.data[p + 3] = r.rgba[k + 3];
      }
    }
    ctx.putImageData(image, 0, 0);
    layers.raster.append(svg('image', {
      class: 'xmap-raster',
      x: x0,
      y: y0,
      width: canvas.width,
      height: canvas.height,
      href: canvas.toDataURL()
    }));
  }

  // Zooms to level z keeping the place under (sx, sy) of the view where it is
  function zoomAt(z, sx, sy)
  {
    z = Math.max(minZoom(), Math.min(18, z));
    if (z === state.zoom)
      return;
    const world = toWorld(sx, sy);
    state.zoom = z;
    state.center =
        [world[0] - (sx - width() / 2) / scale(), world[1] - (sy - height() / 2) / scale()];
    redraw();
  }

  // Show the box (with a margin) at the largest zoom that fits
  function fit(bbox)
  {
    if (!bbox)
      return;
    const a = project(bbox[0], bbox[3]);
    const b = project(bbox[2], bbox[1]);
    state.center = [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2];
    const dx = Math.max(1e-6, Math.abs(b[0] - a[0]));
    const dy = Math.max(1e-6, Math.abs(b[1] - a[1]));
    let z = 0;
    while (z < 12 && dx * 2 ** (z + 1) < width() * 0.85 && dy * 2 ** (z + 1) < height() * 0.85)
      z++;
    state.zoom = z;
    redraw();
  }

  // --- Interaction ------------------------------------------------------
  let drag = null;
  // The position of a pointer event in the view
  const local = (e) => {
    const r = surface.getBoundingClientRect();
    return [e.clientX - r.left, e.clientY - r.top];
  };

  surface.addEventListener('pointerdown', (e) => {
    if (e.button !== 0)
      return;
    try
    {
      surface.setPointerCapture(e.pointerId);
    }
    catch (err)
    {
      // Synthetic events have no pointer to capture
    }
    const [sx, sy] = local(e);
    drag = {sx, sy, center: state.center.slice(), moved: false};
  });

  surface.addEventListener('pointermove', (e) => {
    // The value of the raster under the pointer
    if (!drag && state.raster && state.raster.label)
    {
      const [sx, sy] = local(e);
      const [lon, lat] = lonLatOf(sx, sy);
      const label = state.raster.label(lon, lat);
      if (label)
        showTooltip(label, sx, sy);
      else
        tooltip.hidden = true;
    }
    if (!drag)
      return;
    const [sx, sy] = local(e);
    if (Math.abs(sx - drag.sx) + Math.abs(sy - drag.sy) > 4)
      drag.moved = true;
    const t = state.tool;
    if (t === 'box' || t === 'circle')
    {
      layers.draft.replaceChildren();
      const shape = shapeOf(drag, sx, sy);
      if (shape)
        drawShape(layers.draft, shape, 'xmap-draft');
      return;
    }
    state.center =
        [drag.center[0] - (sx - drag.sx) / scale(), drag.center[1] - (sy - drag.sy) / scale()];
    scheduleRedraw();
  });

  surface.addEventListener('pointerup', (e) => {
    if (!drag)
      return;
    const [sx, sy] = local(e);
    const t = state.tool;
    layers.draft.replaceChildren();
    let shape = null;
    if (t === 'box' || t === 'circle')
      shape = drag.moved ? shapeOf(drag, sx, sy) : null;
    else if (t === 'point' && !drag.moved)
    {
      const [lon, lat] = lonLatOf(sx, sy);
      shape = {type: 'point', lon: round(lon, 4), lat: round(lat, 4)};
    }
    else if (t === 'line' && !drag.moved)
    {
      // Each click adds a vertex to the path
      const [lon, lat] = lonLatOf(sx, sy);
      const points = state.selection && state.selection.type === 'path' ?
          state.selection.points.slice() :
          [];
      points.push([round(lon, 3), round(lat, 3)]);
      shape = {type: 'path', points};
    }
    else if (t === 'move' && !drag.moved && hooks.click)
      hooks.click(...lonLatOf(sx, sy));
    drag = null;
    if (shape && onSelect)
      onSelect(shape);
  });

  // The shape dragged from d to (sx, sy) with the current tool: a box or a circle
  function shapeOf(d, sx, sy)
  {
    const [lon0, lat0] = lonLatOf(d.sx, d.sy);
    const [lon1, lat1] = lonLatOf(sx, sy);
    if (state.tool === 'box')
      return {
        type: 'box',
        bbox: [
          round(Math.min(lon0, lon1), 3),
          round(Math.min(lat0, lat1), 3),
          round(Math.max(lon0, lon1), 3),
          round(Math.max(lat0, lat1), 3)
        ]
      };
    const km = distanceKm(lon0, lat0, lon1, lat1);
    if (km < 1)
      return null;
    return {type: 'circle', lon: round(lon0, 4), lat: round(lat0, 4), km: Math.round(km)};
  }

  surface.addEventListener('pointerleave', () => {
    if (state.raster)
      tooltip.hidden = true;
  });

  surface.addEventListener('wheel', (e) => {
    e.preventDefault();
    const [sx, sy] = local(e);
    zoomAt(state.zoom + (e.deltaY < 0 ? 1 : -1), sx, sy);
  }, {passive: false});

  const button = (label, title, fn) =>
      el('button',
         {type: 'button', class: 'small', title: title, 'aria-label': title, onclick: fn},
         label);
  zoom.append(
      button('+', L('Zoom in'), () => zoomAt(state.zoom + 1, width() / 2, height() / 2)),
      button('−', L('Zoom out'), () => zoomAt(state.zoom - 1, width() / 2, height() / 2)),
      button(L('Fit'), L('Show everything on the map'), () => fit(api.extent())));

  // Expanded over the page and back, the centre of the view kept: the button, Escape or a click
  // beside the map
  const backdrop = el('div', {class: 'xmap-backdrop', hidden: true});
  const expandIcon = (expanded) => {
    const icon = svg('svg', {class: 'icon', viewBox: '0 0 16 16', width: 14, height: 14, 'aria-hidden': 'true'});
    icon.append(svg('path', {
      fill: 'none',
      stroke: 'currentColor',
      'stroke-width': 1.6,
      d: expanded ? 'M6 1v5H1M10 1v5h5M6 15v-5H1M10 15v-5h5' : 'M1 6V1h5M15 6V1h-5M1 10v5h5M15 10v5h-5'
    }));
    return icon;
  };
  const expandButton = button(expandIcon(false), L('Enlarge the map'), () => setExpanded(!expanded()));
  const expanded = () => box.classList.contains('xmap-expanded');
  function setExpanded(on)
  {
    if (on === expanded())
      return;
    const centre = state.center.slice();
    box.classList.toggle('xmap-expanded', on);
    backdrop.hidden = !on;
    if (on)
      box.before(backdrop);
    else
      backdrop.remove();
    const title = on ? L('Make the map smaller') : L('Enlarge the map');
    expandButton.replaceChildren(expandIcon(on));
    expandButton.title = title;
    expandButton.setAttribute('aria-label', title);
    expandButton.setAttribute('aria-pressed', on ? 'true' : 'false');
    // A larger view shows more around the same centre, one zoom level closer
    state.center = centre;
    state.zoom = Math.max(0, Math.min(18, state.zoom + (on ? 1 : -1)));
    redraw();
  }
  backdrop.addEventListener('click', () => setExpanded(false));
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape' && expanded())
      setExpanded(false);
  });
  zoom.append(expandButton);

  const TOOLS = [
    ['move', L('Move'), L('Move the map')],
    ['point', L('Point'), L('Click to choose a point')],
    ['circle', L('Radius'), L('Drag from the centre to choose a radius')],
    ['box', L('Box'), L('Drag to choose a box')],
    ['line', L('Path'), L('Click the vertices of a path')]
  ];

  const api = {
    element: box,
    setTools(allowed, current) {
      tools.replaceChildren(...TOOLS.filter(([t]) => allowed.includes(t))
                                .map(
                                    ([t, label, title]) =>
                                        el('button',
                                           {
                                             type: 'button',
                                             class: 'small',
                                             title: title,
                                             'data-tool': t,
                                             'aria-pressed': t === current ? 'true' : 'false',
                                             onclick: () => api.setTools(allowed, t)
                                           },
                                           label)));
      state.tool = current;
      surface.dataset.tool = current;
      // A path is drawn vertex by vertex: the last one can be undone, or all
      if (allowed.includes('line'))
      {
        const edit = (fn) => () => {
          const sel = state.selection;
          if (sel && sel.type === 'path' && onSelect)
            onSelect(fn(sel.points.slice()));
        };
        tools.append(
            el('button',
               {
                 type: 'button',
                 class: 'small',
                 title: L('Remove the last vertex'),
                 onclick: edit((points) => ({type: 'path', points: points.slice(0, -1)}))
               },
               L('Undo')),
            el('button',
               {
                 type: 'button',
                 class: 'small',
                 title: L('Remove the path'),
                 onclick: edit(() => ({type: 'path', points: []}))
               },
               L('Clear')));
      }
    },
    setRegions(regions) {
      // The boxes of the polygons for fitting the map
      state.regions = regions.map((r) => {
        if (!r.polygon || r.bbox)
          return r;
        const lons = r.polygon.map((p) => p[0]);
        const lats = r.polygon.map((p) => p[1]);
        return Object.assign({}, r, {
          bbox: [Math.min(...lons), Math.min(...lats), Math.max(...lons), Math.max(...lats)]
        });
      });
      drawOverlays();
    },
    setSelection(sel) {
      state.selection = sel;
      drawOverlays();
    },
    setPoints(points) {
      state.points = points;
      drawOverlays();
    },
    // The cursor position {lon, lat}, or null
    setCursor(position) {
      state.cursor = position;
      drawCursor();
    },
    // Coloured values: {items: [{lon, lat, color, label, info}], dx, dy}, the cell size in
    // degrees, or dx = 0 for dots
    setCells(cells) {
      state.cells = cells;
      drawOverlays();
    },
    // The box of everything shown
    extent() {
      const boxes = [
        ...state.regions.map((r) => r.bbox),
        selectionBox(state.selection),
        ...state.points.map((p) => [p.lon, p.lat, p.lon, p.lat]),
        ...(state.cells ? state.cells.items.map((c) => [c.lon, c.lat, c.lon, c.lat]) : [])
      ].filter(Boolean);
      if (boxes.length === 0)
        return null;
      return [
        Math.min(...boxes.map((b) => b[0])),
        Math.min(...boxes.map((b) => b[1])),
        Math.max(...boxes.map((b) => b[2])),
        Math.max(...boxes.map((b) => b[3]))
      ];
    },
    // A raster {bbox: [west, south, east, north], width, height, rgba (4 bytes per cell,
    // rows from the north), label(lon, lat) giving the {label, info} of the tooltip or null},
    // or null
    setRaster(raster) {
      state.raster = raster;
      drawRaster();
    },
    // The view {zoom, center} of another map, without telling it back
    setView(view) {
      state.zoom = view.zoom;
      state.center = view.center.slice();
      hooks.shown = `${state.zoom},${state.center[0]},${state.center[1]}`;
      redraw();
    },
    // The view {zoom, center}
    view() {
      return {zoom: state.zoom, center: state.center.slice()};
    },
    // Calls fn({zoom, center}) when the view changes
    onView(fn) {
      hooks.view = fn;
    },
    // Calls fn(lon, lat) on a click that does not drag the map with the move tool
    onClick(fn) {
      hooks.click = fn;
      box.classList.toggle('xmap-clickable', !!fn);
    },
    fit,
    redraw,
    lonLatOf,
    screenOf
  };

  // Redraw when the width changes, e.g. on the first layout
  if (typeof ResizeObserver !== 'undefined')
  {
    let last = '';
    new ResizeObserver(() => {
      const size = `${box.clientWidth}x${box.clientHeight}`;
      if (size !== last)
      {
        last = size;
        redraw();
      }
    }).observe(box);
  }
  return api;
}

// ----------------------------------------------------------------------
// Vertical profiles and soundings
// ----------------------------------------------------------------------

// The units of the values shown: temperatures, wind speeds, pressures and heights. The values
// are converted from C, m/s, hPa and m; temperatures in K (raw GRIB data) and pressures in Pa
// are recognised by their values. The first unit of each quantity is the base unit.
const UNIT_CHOICES = {
  temperature: [['C', 'C', (v) => v], ['F', 'F', (v) => v * 1.8 + 32], ['K', 'K', (v) => v + 273.15]],
  speed: [['ms', 'm/s', (v) => v], ['kt', 'kt', (v) => v * 1.943844], ['kmh', 'km/h', (v) => v * 3.6],
          ['mph', 'mph', (v) => v * 2.236936]],
  pressure: [['hPa', 'hPa', (v) => v], ['Pa', 'Pa', (v) => v * 100], ['inHg', 'inHg', (v) => v * 0.02953]],
  height: [['m', 'm', (v) => v], ['ft', 'ft', (v) => v * 3.28084]]
};
const UNITS_KEY = 'smartmet-explorer-units';
// The regions using Fahrenheit, miles per hour and feet
const IMPERIAL_REGIONS = ['US', 'LR', 'MM', 'BS', 'BZ', 'KY', 'PW', 'FM', 'MH'];

// The region of a browser language such as en-US, when the time zone of the browser agrees:
// en-US is the language of many browsers outside the United States
function browserRegion(language, timeZone)
{
  const region = ((/[-_]([A-Za-z]{2})\b/.exec(language || '') || [])[1] || '').toUpperCase();
  if (region === 'US' && timeZone && !/^(America|Pacific|US)\//.test(timeZone))
    return '';
  return region;
}

// The time zone of the browser, '' when unknown
function browserTimeZone()
{
  try
  {
    return Intl.DateTimeFormat().resolvedOptions().timeZone || '';
  }
  catch (e)
  {
    return '';
  }
}

// The units of a browser language such as en-US: F, mph and ft in the regions using them, else
// C, m/s and m; hPa everywhere
function localeUnits(language, timeZone)
{
  const region = browserRegion(language, timeZone);
  return region && IMPERIAL_REGIONS.includes(region) ?
      {temperature: 'F', speed: 'mph', pressure: 'hPa', height: 'ft'} :
      {temperature: 'C', speed: 'ms', pressure: 'hPa', height: 'm'};
}

// The units shown: the choice of the user, else the defaults of the server, else those of the
// language of the browser
function initialUnits(serverUnits)
{
  const language = typeof navigator !== 'undefined' ? (navigator.languages || [])[0] || navigator.language : '';
  const units = Object.assign(localeUnits(language, browserTimeZone()), serverUnits || {});
  try
  {
    return Object.assign(units, JSON.parse(localStorage.getItem(UNITS_KEY) || '{}'));
  }
  catch (e)
  {
    return units;
  }
}
let displayUnits = initialUnits();

// The chosen unit of a quantity: [id, label, convert(value in the default unit)]
const unitOf = (quantity) =>
    UNIT_CHOICES[quantity].find(([id]) => id === displayUnits[quantity]) || UNIT_CHOICES[quantity][0];

// The quantity of a parameter by its name (also inside a function, e.g. nanmedian_a(t2m)):
// 'temperature', 'speed', 'pressure', 'height' or null
function quantityOf(name)
{
  const n = String(name).toLowerCase().replace(/^.*\(([^()]+)\)$/, '$1');
  if (/pressure|^p-(pa|hpa)$|^pmsl$|^msl$|^pa_|^p_sea/.test(n))
    return 'pressure';
  if (/temperature|dewpoint|^t2m$|^t-[kc]$|^td-[kc]$|^td$|^ta_|^td_|^tamax|^tamin|^tmax|^tmin/.test(n))
    return 'temperature';
  if (/windspeed|^ws_|^ws$|^ff-ms$|^ffg-ms$|gust|^wg_|windums|windvms|^[uv]-ms$/.test(n))
    return 'speed';
  if (HEIGHT_PARAMS.includes(n) || /height$|^hl-m$|^z-m$/.test(n))
    return 'height';
  return null;
}

// The rows in the chosen units: {rows, labels: {column: unit}}, the rows as they are when all
// the units are the defaults
function displayRows(rows)
{
  if (rows.length === 0)
    return {rows, labels: {}};
  const converters = {};
  const labels = {};
  const keys = new Set();
  for (const row of rows.slice(0, 500))
    Object.keys(row).forEach((k) => keys.add(k));
  for (const key of keys)
  {
    const quantity = quantityOf(key);
    if (!quantity || key === 'lat' || key === 'lon')
      continue;
    const [id, label, convert] = unitOf(quantity);
    const from = baseUnit(quantity, rows.map((r) => r[key]));
    // Values in the base unit shown in it are left as they are
    if (!from && id === UNIT_CHOICES[quantity][0][0])
      continue;
    converters[key] = from ? (v) => convert(from(v)) : convert;
    labels[key] = label;
  }
  if (Object.keys(converters).length === 0)
    return {rows, labels};
  return {
    rows: rows.map((r) => {
      const out = Object.assign({}, r);
      for (const [key, f] of Object.entries(converters))
        if (typeof out[key] === 'number')
          out[key] = f(out[key]);
      return out;
    }),
    labels
  };
}

// The conversion of the values of a quantity to its base unit when they are not in it:
// temperatures in K and pressures in Pa are recognised by their values. Null when the values
// are in the base unit.
function baseUnit(quantity, values)
{
  const numbers = [];
  for (const v of values)
    if (typeof v === 'number' && isFinite(v))
      numbers.push(v);
  if (numbers.length === 0)
    return null;
  numbers.sort((a, b) => a - b);
  // Above 150 C does not occur, below 150 K neither
  if (quantity === 'temperature' && quantile(numbers, 0.5) > 150)
    return (v) => v - 273.15;
  // Pressures in hPa stay below 1100, in Pa near the ground above 50000
  if (quantity === 'pressure' && quantile(numbers, 0.9) > 2000)
    return (v) => v / 100;
  return null;
}

// The values of a parameter in the units shown: {convert(value), label}, label '' when the
// values are shown as they are
function displayUnit(param, values)
{
  const quantity = quantityOf(param);
  if (!quantity)
    return {convert: (v) => v, label: ''};
  const [, label, convert] = unitOf(quantity);
  const from = baseUnit(quantity, values);
  return {convert: from ? (v) => convert(from(v)) : convert, label};
}

// The choices of the units: the results are drawn again when they change
function unitsChooser(onChange)
{
  const box = el('span', {class: 'units-chooser'});
  for (const [quantity, label] of [['temperature', L('Temperature unit')], ['speed', L('Wind speed unit')],
                                   ['pressure', L('Pressure unit')], ['height', L('Height unit')]])
  {
    const select = el('select', {'aria-label': label, title: label},
                      UNIT_CHOICES[quantity].map(([id, text]) => el('option', {value: id}, text)));
    select.value = unitOf(quantity)[0];
    select.addEventListener('change', () => {
      displayUnits = Object.assign({}, displayUnits, {[quantity]: select.value});
      try
      {
        localStorage.setItem(UNITS_KEY, JSON.stringify(displayUnits));
      }
      catch (e)
      {
        // Not remembered
      }
      onChange();
    });
    box.append(select);
  }
  return box;
}

// Rows with several levels per location and time are drawn as profiles, time-height sections,
// thermodynamic diagrams (see below), and along paths as cross sections and animated time
// series. Flight levels and the animated time slider are shared by these views.

// The value of a column, or of the median of an area computed by the server
function columnValue(row, name)
{
  let v = row[name];
  if (v === undefined)
    v = row[statisticParameter('nanmedian_a', name)];
  return typeof v === 'number' && isFinite(v) ? v : null;
}

// The first of the candidate columns the rows have
function findColumn(rows, candidates)
{
  for (const name of candidates)
    if (rows.some((r) => columnValue(r, name) !== null))
      return name;
  return null;
}

// The parameters that place a level vertically, and those of a sounding, by their names in
// querydata and grid data
const PRESSURE_PARAMS = ['pressure', 'p-hpa', 'pres-hpa'];
const PRESSURE_PA_PARAMS = ['p-pa', 'pres-pa'];
const HEIGHT_PARAMS = ['geomheight', 'geopheight', 'hl-m', 'z-m', 'height'];
const SOUNDING_PARAMS = {
  temperature: [['temperature', 0], ['t', 0], ['t-c', 0], ['t-k', -273.15]],
  dewpoint: [['dewpoint', 0], ['dewpoint2m', 0], ['td', 0], ['td-c', 0], ['td-k', -273.15]],
  humidity: [['humidity', 0], ['rh-prcnt', 0], ['rh', 0]],
  speed: [['windspeedms', 0], ['ff-ms', 0], ['ws', 0]],
  direction: [['winddirection', 0], ['dd-d', 0], ['wd', 0]],
  u: [['windums', 0], ['u-ms', 0]],
  v: [['windvms', 0], ['v-ms', 0]]
};

// The tabs of results that open first with later results once chosen: the views of what the
// data has. Charts, tables and trees are the defaults.
const REMEMBERED_TABS = ['Meteogram', 'Profile', 'Section', 'Sounding', 'Cross section', 'Along the path', 'Map'];

// The kind of the levels of an EDR collection from its vertical reference system
function edrLevelKind(collection)
{
  const type = collection && collection.levels && collection.levels[0] ?
      String(collection.levels[0].type).toLowerCase() :
      '';
  // Members of an ensemble stored as levels
  if (type.includes('ensemble member'))
    return 'member';
  // Model levels first: hybrid sigma-pressure levels are named after the pressure too
  if (type.includes('hybrid') || type.includes('model'))
    return 'hybrid';
  if (type.includes('pressure'))
    return /pascal",1|\(pa\)/.test(type) && !/hectopascal/.test(type) ? 'pressure-pa' : 'pressure';
  if (type.includes('depth'))
    return 'depth';
  if (type.includes('height') || type.includes('altitude'))
    return 'height';
  return type ? 'level' : '';
}

// The vertical axes the rows allow: [{id, label, unit, value(row), log, down}], where down
// means that the values grow downwards (pressure, depth, model level)
function verticalAxes(rows, levelKind)
{
  const axes = [];
  const level = (r) => (typeof r.level === 'number' ? r.level : null);
  const pressure = findColumn(rows, PRESSURE_PARAMS);
  const pressurePa = findColumn(rows, PRESSURE_PA_PARAMS);
  const height = findColumn(rows, HEIGHT_PARAMS);
  if (levelKind === 'pressure')
    axes.push(
        {id: 'pressure', label: L('Pressure'), unit: 'hPa', value: level, log: true, down: true});
  else if (levelKind === 'pressure-pa')
    axes.push({
      id: 'pressure',
      label: L('Pressure'),
      unit: 'hPa',
      value: (r) => (typeof r.level === 'number' ? r.level / 100 : null),
      log: true,
      down: true
    });
  else if (pressure)
    axes.push({
      id: 'pressure',
      label: L('Pressure'),
      unit: 'hPa',
      value: (r) => columnValue(r, pressure),
      log: true,
      down: true
    });
  else if (pressurePa)
    axes.push({
      id: 'pressure',
      label: L('Pressure'),
      unit: 'hPa',
      value: (r) => {
        const v = columnValue(r, pressurePa);
        return v === null ? null : v / 100;
      },
      log: true,
      down: true
    });
  // Heights in the chosen unit
  const [, heightUnit, toHeight] = unitOf('height');
  const inHeightUnit = (v) => (v === null ? null : toHeight(v));
  if (levelKind === 'height' || levelKind === 'altitude')
    axes.push({id: 'height', label: L('Height'), unit: heightUnit, value: (r) => inHeightUnit(level(r)),
               log: false, down: false});
  else if (height)
    axes.push({
      id: 'height',
      label: L('Height'),
      unit: heightUnit,
      value: (r) => inHeightUnit(columnValue(r, height)),
      log: false,
      down: false
    });
  if (levelKind === 'depth' || levelKind === 'ground depth')
    axes.push({
      id: 'depth',
      label: L('Depth'),
      unit: levelKind === 'depth' ? 'm' : 'cm',
      value: level,
      log: false,
      down: true
    });
  if (levelKind === 'hybrid' || axes.length === 0)
    axes.push({id: 'level', label: L('Model level'), unit: '', value: level, log: false, down: true});
  return axes;
}

// The profiles of the rows: one per location (and variant) and time, with at least 3 levels
function collectProfiles(rows, axis)
{
  const profiles = new Map();
  for (const row of rows)
  {
    const t = parseTime(row.time);
    const z = axis.value(row);
    if (!t || z === null)
      continue;
    const key = `${locationOf({name: row.name, lat: row.lat, lon: row.lon, _point: row._point})}${
        row._variant ? ` · ${row._variant}` : ''}`;
    const id = `${key}|${t.getTime()}`;
    if (!profiles.has(id))
      profiles.set(id, {key, time: t.getTime(), rows: []});
    profiles.get(id).rows.push({z, row});
  }
  return Array.from(profiles.values())
      .filter((p) => p.rows.length >= 3)
      .map((p) => Object.assign(p, {rows: p.rows.sort((a, b) => a.z - b.z)}));
}

// Whether the rows have profiles worth drawing
function hasProfiles(rows, levelKind)
{
  // The levels of an ensemble are its members
  if (levelKind === 'member')
    return false;
  if (!rows.some((r) => typeof r.level === 'number'))
    return false;
  const axes = verticalAxes(rows, levelKind);
  return collectProfiles(rows, axes[0]).length > 0;
}

// Round numbers for an axis
function niceTicks(min, max, count)
{
  if (!(max > min))
    return [min];
  const raw = (max - min) / Math.max(1, count);
  const mag = Math.pow(10, Math.floor(Math.log10(raw)));
  const step = [1, 2, 2.5, 5, 10].map((m) => m * mag).find((s) => s >= raw) || raw;
  const out = [];
  for (let v = Math.ceil(min / step) * step; v <= max + step * 1e-9; v += step)
    out.push(Math.round(v / step) * step);
  return out;
}

const PRESSURE_TICKS =
    [1000, 925, 850, 700, 500, 400, 300, 250, 200, 150, 100, 70, 50, 30, 20, 10, 5];

// The tops of the vertical axes: how far up (or down) diagrams reach
function verticalTops(axis)
{
  if (axis.id === 'pressure')
    return [
      ['', L('All')],
      ['100', '100 hPa'],
      ['200', '200 hPa'],
      ['500', '500 hPa'],
      ['700', '700 hPa'],
      ['900', '900 hPa']
    ];
  if (axis.id === 'height' && axis.unit === 'ft')
    return [['', L('All')], ['40000', '40000 ft'], ['20000', '20000 ft'], ['10000', '10000 ft'], ['5000', '5000 ft']];
  if (axis.id === 'height')
    return [['', L('All')], ['10000', '10 km'], ['5000', '5 km'], ['3000', '3 km'], ['1000', '1 km']];
  if (axis.id === 'depth')
    return [['', L('All')], ['200', '200 m'], ['100', '100 m'], ['50', '50 m']];
  return [['', L('All')]];
}

// Whether a level is within the top of the axis
function withinTop(axis, top, z)
{
  if (!top)
    return true;
  return axis.id === 'pressure' ? z >= Number(top) : z <= Number(top);
}

// The scale of a vertical axis to pixels 0 (top) ... h (bottom)
function verticalScale(axis, zmin, zmax, h)
{
  const f = axis.log ? (v) => Math.log(v) : (v) => v;
  const a = f(zmin);
  const b = f(zmax);
  const span = b - a || 1;
  // Growing upwards: the largest value at the top
  return axis.down ? (v) => ((f(v) - a) / span) * h : (v) => h - ((f(v) - a) / span) * h;
}

// The ticks of a vertical axis: the standard pressure levels on a pressure axis
function verticalTicks(axis, zmin, zmax)
{
  if (axis.log)
  {
    const ticks = PRESSURE_TICKS.filter((p) => p >= zmin && p <= zmax);
    return ticks.length >= 2 ? ticks : niceTicks(zmin, zmax, 6);
  }
  return niceTicks(zmin, zmax, 6);
}

// The opacity of a profile: earlier times fainter when times are overlaid
const profileOpacity = (timeIndex, times) =>
    (times <= 1 ? 1 : 0.3 + (0.7 * timeIndex) / (times - 1));

// A vertical diagram: values horizontally, the vertical coordinate vertically.
// series: [{label, colour, points: [[value, z]]}]
function drawVerticalDiagram(panel, title, axis, valueLabel, series)
{
  const width = Math.max(320, Math.min(560, panel.clientWidth || 480));
  const height = 420;
  const m = {left: 56, right: 12, top: 10, bottom: 40};
  const w = width - m.left - m.right;
  const h = height - m.top - m.bottom;
  // The extent of the levels that have values: below the sea floor there are none
  const valued = series.flatMap((s) => s.points.filter((p) => p[0] !== null));
  const zs = valued.map((p) => p[1]);
  const xs = valued.map((p) => p[0]);
  if (zs.length === 0 || xs.length === 0)
    return;
  let zmin = Math.min(...zs);
  let zmax = Math.max(...zs);
  if (axis.id === 'depth')
    zmin = Math.min(0, zmin);
  if (axis.id === 'height')
    zmin = Math.min(0, zmin);
  let xmin = Math.min(...xs);
  let xmax = Math.max(...xs);
  if (xmax - xmin < 1e-9)
  {
    xmin -= 1;
    xmax += 1;
  }
  const pad = (xmax - xmin) * 0.05;
  xmin -= pad;
  xmax += pad;
  const sy = verticalScale(axis, zmin, zmax, h);
  const sx = (v) => ((v - xmin) / (xmax - xmin)) * w;

  const root = svg('svg', {
    class: 'vdiagram',
    width,
    height,
    viewBox: `0 0 ${width} ${height}`,
    role: 'img',
    'aria-label': title
  });
  const g = svg('g', {transform: `translate(${m.left},${m.top})`});
  root.append(g);
  // Grid and axes
  for (const z of verticalTicks(axis, zmin, zmax))
  {
    const y = sy(z);
    g.append(svg('line', {class: 'vgrid', x1: 0, x2: w, y1: y, y2: y}));
    const t = svg('text', {class: 'vtick', x: -6, y: y + 4, 'text-anchor': 'end'});
    t.textContent = String(z);
    g.append(t);
  }
  for (const v of niceTicks(xmin, xmax, 6))
  {
    const x = sx(v);
    g.append(svg('line', {class: 'vgrid', x1: x, x2: x, y1: 0, y2: h}));
    const t = svg('text', {class: 'vtick', x, y: h + 16, 'text-anchor': 'middle'});
    t.textContent = String(v);
    g.append(t);
  }
  g.append(svg('rect', {class: 'vframe', x: 0, y: 0, width: w, height: h}));
  const ylabel = svg('text', {
    class: 'vlabel',
    transform: `translate(${- 44},${h / 2}) rotate(-90)`,
    'text-anchor': 'middle'
  });
  ylabel.textContent = `${axis.label}${axis.unit ? ` (${axis.unit})` : ''}`;
  g.append(ylabel);
  const xlabel = svg('text', {class: 'vlabel', x: w / 2, y: h + 34, 'text-anchor': 'middle'});
  xlabel.textContent = valueLabel;
  g.append(xlabel);
  // The profiles: a missing value breaks the line
  for (const s of series)
  {
    let d = '';
    let pen = false;
    for (const [v, z] of s.points)
    {
      if (v === null)
      {
        pen = false;
        continue;
      }
      d += `${pen ? 'L' : 'M'}${sx(v).toFixed(1)},${sy(z).toFixed(1)}`;
      pen = true;
    }
    const path =
        svg('path', {class: 'vline', d, stroke: s.colour, 'stroke-opacity': s.opacity ?? 1});
    g.append(path);
  }
  // The nearest point to the pointer: a marker and its values
  const points = series.flatMap(
      (s) => s.points.filter(([v]) => v !== null).map(([v, z]) => ({x: sx(v), y: sy(z), v, z, s})));
  const marker = svg('circle', {class: 'vmarker', r: 4, visibility: 'hidden'});
  const tip = svg('g', {class: 'vtip', visibility: 'hidden'});
  const tipBox = svg('rect', {rx: 3, ry: 3});
  const tipText = svg('text');
  tip.append(tipBox, tipText);
  const hover = svg('rect', {class: 'vhover', x: 0, y: 0, width: w, height: h});
  g.append(marker, hover, tip);
  hover.addEventListener('mousemove', (e) => {
    const r = hover.getBoundingClientRect();
    const mx = ((e.clientX - r.left) / r.width) * w;
    const my = ((e.clientY - r.top) / r.height) * h;
    let best = null;
    let bestD = Infinity;
    for (const p of points)
    {
      const d = (p.x - mx) ** 2 + (p.y - my) ** 2;
      if (d < bestD)
      {
        bestD = d;
        best = p;
      }
    }
    if (!best || bestD > 40 * 40)
    {
      marker.setAttribute('visibility', 'hidden');
      tip.setAttribute('visibility', 'hidden');
      return;
    }
    marker.setAttribute('cx', best.x);
    marker.setAttribute('cy', best.y);
    marker.setAttribute('stroke', best.s.colour);
    marker.setAttribute('visibility', 'visible');
    tipText.replaceChildren();
    const lines = [
      best.s.label,
      `${valueLabel}: ${round(best.v, 2)}`,
      `${axis.label}: ${round(best.z, 1)}${axis.unit ? ` ${axis.unit}` : ''}`
    ];
    lines.forEach((text, i) => {
      const span = svg('tspan', {x: 6, dy: i === 0 ? 14 : 14});
      span.textContent = text;
      tipText.append(span);
    });
    const tw = Math.max(...lines.map((l) => l.length)) * 6.2 + 12;
    const th = lines.length * 14 + 8;
    tipBox.setAttribute('width', tw);
    tipBox.setAttribute('height', th);
    const tx = best.x + 10 + tw > w ? best.x - 10 - tw : best.x + 10;
    const ty = Math.min(Math.max(0, best.y - th / 2), h - th);
    tip.setAttribute('transform', `translate(${tx},${ty})`);
    tip.setAttribute('visibility', 'visible');
  });
  hover.addEventListener('mouseleave', () => {
    marker.setAttribute('visibility', 'hidden');
    tip.setAttribute('visibility', 'hidden');
  });
  panel.append(root);
}

// The speeds of animations: the time each frame is shown (ms), shared by the page and
// remembered by the browser
const ANIMATION_SPEEDS = [['2800', '0.25×'], ['1400', '0.5×'], ['700', '1×'], ['350', '2×'], ['175', '4×']];
const ANIMATION_SPEED_KEY = 'smartmet-explorer-animation-speed';
let animationDelay = (() => {
  try
  {
    const v = localStorage.getItem(ANIMATION_SPEED_KEY);
    return ANIMATION_SPEEDS.some(([d]) => d === v) ? Number(v) : 700;
  }
  catch (e)
  {
    return 700;
  }
})();

// The position of the pointer on the page, null when it is outside the page. Diagrams redrawn
// under a resting pointer get its events again, so that their hover readouts follow the
// animation.
let pointerAt = null;
if (typeof document !== 'undefined')
{
  document.addEventListener('mousemove', (e) => {
    if (e.isTrusted)
      pointerAt = {x: e.clientX, y: e.clientY};
  }, true);
  document.addEventListener('mouseout', (e) => {
    if (!e.relatedTarget)
      pointerAt = null;
  });
}

// Sends the events of the pointer again to whatever is now under it, after a diagram has been
// redrawn under a pointer that did not move
function replayPointer()
{
  if (!pointerAt)
    return;
  const target = document.elementFromPoint(pointerAt.x, pointerAt.y);
  if (!target)
    return;
  const init = {clientX: pointerAt.x, clientY: pointerAt.y, bubbles: false};
  target.dispatchEvent(new MouseEvent('mouseenter', init));
  target.dispatchEvent(new MouseEvent('mousemove', Object.assign({}, init, {bubbles: true})));
}

// A time slider with animation: a play button, the speed, and a slider over the times with a
// tick at each available time. The slider is positioned by time, so that the ticks show the
// steps and gaps of the data, and it always snaps to an available time. times are in ms; when
// some are unknown the times are spaced evenly. Options: label (of the slider), format (of a
// time and its index), all (offer 'All times'), positions (of the steps on the slider instead of
// their times, e.g. distances), onChange(). Returns {element, index(), all()}.
function timeSlider(times, options = {})
{
  const format = options.format || ((tm) => formatTime(new Date(tm)));
  const formatAt = (i) => format(times[i], i);
  const box = el('div', {class: 'row time-slider'});
  if (times.length <= 1)
  {
    box.append(el('span', {class: 'muted small'}, times.length ? formatAt(0) : ''));
    return {element: box, index: () => 0, all: () => false};
  }
  const known = !options.positions && times.every((tm) => typeof tm === 'number' && isFinite(tm));
  // The position of each time on the slider, 0 ... span
  const positions = options.positions ? options.positions.map((p) => p - options.positions[0]) :
      known                           ? times.map((tm) => (tm - times[0]) / 60000) :
                                        times.map((_, i) => i);
  const span = positions[positions.length - 1] || 1;
  let index = Math.max(0, Math.min(times.length - 1, options.initial || 0));
  let all = false;
  let timer = null;
  let frame = null;

  const slider = el('input', {
    type: 'range',
    min: 0,
    max: span,
    step: 'any',
    value: 0,
    'aria-label': options.label || L('Time'),
    'aria-valuetext': formatAt(0)
  });
  const ticks = el('div', {class: 'time-ticks', 'aria-hidden': 'true'});
  positions.forEach((p, i) => {
    const d = known ? new Date(times[i]) : null;
    const day = d && d.getUTCHours() === 0 && d.getUTCMinutes() === 0;
    ticks.append(el('span', {
      class: `time-tick${day ? ' day' : ''}`,
      style: `left: ${(p / span) * 100}%`,
      title: formatAt(i)
    }));
  });
  const track = el('div', {class: 'time-track'}, slider, ticks);
  const label = el('span', {class: 'small time-label'});
  const play = el('button', {type: 'button', class: 'small time-play', 'aria-label': L('Play')}, '▶');
  // One time back or forward, stopping at the ends
  const back =
      el('button', {type: 'button', class: 'small time-step', 'aria-label': L('Previous time')}, '‹');
  const forward =
      el('button', {type: 'button', class: 'small time-step', 'aria-label': L('Next time')}, '›');
  const speed = el('select', {'aria-label': L('Animation speed')},
                   ANIMATION_SPEEDS.map(([v, l]) => el('option', {value: v}, l)));
  speed.value = String(animationDelay);
  const allBox = el('input', {type: 'checkbox', 'aria-label': L('All times')});

  // Shows the chosen step on the slider and in the label
  const update = () => {
    slider.value = String(positions[index]);
    slider.setAttribute('aria-valuetext', formatAt(index));
    label.textContent = all ? L`${times.length} times` : formatAt(index);
    slider.disabled = all;
    play.disabled = all;
    back.disabled = all || index === 0;
    forward.disabled = all || index === times.length - 1;
  };
  // Chooses step i; the views are redrawn by onChange
  const choose = (i) => {
    const next = Math.max(0, Math.min(times.length - 1, i));
    const changed = next !== index;
    index = next;
    update();
    if (changed && options.onChange)
    {
      options.onChange();
      replayPointer();
    }
  };
  // Stops the animation
  const stop = () => {
    if (timer)
      clearTimeout(timer);
    if (frame)
      cancelAnimationFrame(frame);
    timer = null;
    frame = null;
    play.textContent = '▶';
    play.setAttribute('aria-label', 'Play');
  };
  // The next step is scheduled only after the previous one has been painted, so that a slow
  // browser animates more slowly instead of skipping times
  const tick = () => {
    timer = null;
    if (!box.isConnected)
    {
      stop();
      return;
    }
    const started = performance.now();
    choose((index + 1) % times.length);
    frame = requestAnimationFrame(() => {
      frame = null;
      if (play.getAttribute('aria-label') === 'Pause')
        timer = setTimeout(tick, Math.max(0, animationDelay - (performance.now() - started)));
    });
  };

  // The nearest available time to the slider
  slider.addEventListener('input', () => {
    stop();
    const v = Number(slider.value);
    let best = 0;
    positions.forEach((p, i) => {
      if (Math.abs(p - v) < Math.abs(positions[best] - v))
        best = i;
    });
    choose(best);
  });
  // The arrow keys step from one available time to the next
  slider.addEventListener('keydown', (e) => {
    const step = {ArrowLeft: -1, ArrowDown: -1, ArrowRight: 1, ArrowUp: 1}[e.key];
    const jump = {Home: -times.length, End: times.length}[e.key];
    if (step === undefined && jump === undefined)
      return;
    e.preventDefault();
    stop();
    choose(index + (step !== undefined ? step : jump));
  });
  play.addEventListener('click', () => {
    if (play.getAttribute('aria-label') === 'Pause')
    {
      stop();
      return;
    }
    play.textContent = '❚❚';
    play.setAttribute('aria-label', 'Pause');
    timer = setTimeout(tick, animationDelay);
  });
  back.addEventListener('click', () => {
    stop();
    choose(index - 1);
  });
  forward.addEventListener('click', () => {
    stop();
    choose(index + 1);
  });
  speed.addEventListener('change', () => {
    animationDelay = Number(speed.value);
    try
    {
      localStorage.setItem(ANIMATION_SPEED_KEY, speed.value);
    }
    catch (e)
    {
      // Not remembered
    }
  });
  allBox.addEventListener('change', () => {
    stop();
    all = allBox.checked;
    update();
    if (options.onChange)
      options.onChange();
  });
  update();
  box.append(back, play, forward, speed, track, label);
  if (options.all)
    box.append(el('label', {class: 'check'}, allBox, L('All times')));
  return {element: box, index: () => index, all: () => all};
}

// The time chooser of profile and sounding diagrams: a slider over the times, or all of them,
// starting from the time of index 'initial'
function timeChooser(times, onChange, initial)
{
  const slider = timeSlider(times, {label: L('Time of the profile'), all: true, onChange, initial});
  return {
    element: slider.element,
    index: () => slider.index(),
    value: () => (times.length === 0 ? [] : slider.all() ? times : [times[slider.index()]])
  };
}

// Profiles of the parameters: a diagram per parameter, the value against the vertical axis
function drawProfiles(container, rows, parameters, levelKind)
{
  container.replaceChildren();
  const axes = verticalAxes(rows, levelKind);
  let axis = axes[0];
  const axisSelect =
      el('select',
         {'aria-label': L('Vertical axis')},
         axes.map((a) => el('option', {value: a.id}, a.label)));
  const diagrams = el('div', {class: 'vdiagrams'});
  const times =
      Array.from(new Set(collectProfiles(rows, axis).map((p) => p.time))).sort((a, b) => a - b);
  const chooser = timeChooser(times, () => draw());
  const topSelect = el('select', {'aria-label': L('Top of the profiles')});
  let top = '';
  // The tops the axis offers; the top is reset when the axis changes
  const setTops = () => {
    topSelect.replaceChildren(...verticalTops(axis).map(([v, l]) => el('option', {value: v}, l)));
    top = '';
    topSelect.hidden = topSelect.options.length < 2;
  };
  setTops();
  topSelect.addEventListener('change', () => {
    top = topSelect.value;
    draw();
  });
  const tools =
      el('div',
         {class: 'row'},
         el('label', null, L('Vertical axis'), axisSelect),
         el('label', null, L('Top'), topSelect),
         chooser.element);
  axisSelect.hidden = axes.length < 2;
  axisSelect.addEventListener('change', () => {
    axis = axes.find((a) => a.id === axisSelect.value) || axes[0];
    setTops();
    draw();
  });
  container.append(tools, diagrams);

  // Draws a diagram of each parameter at the chosen times
  function draw()
  {
    diagrams.replaceChildren();
    const chosen = new Set(chooser.value());
    const profiles = collectProfiles(rows, axis).filter((p) => chosen.has(p.time));
    const keys = Array.from(new Set(profiles.map((p) => p.key)));
    const chosenTimes = Array.from(chosen).sort((a, b) => a - b);
    for (const param of parameters)
    {
      const series =
          profiles
              .map((p) => ({
                     label: `${p.key}, ${formatTime(new Date(p.time))}`,
                     colour: palette[keys.indexOf(p.key) % palette.length],
                     opacity: profileOpacity(chosenTimes.indexOf(p.time), chosenTimes.length),
                     points: p.rows.filter(({z}) => withinTop(axis, top, z))
                                 .map(({z, row}) => [columnValue(row, param), z])
                   }))
              .filter((s) => s.points.some(([v]) => v !== null));
      if (series.length === 0)
        continue;
      const panel = el('div', {class: 'chart vchart', 'data-param': param});
      panel.append(el('div', {class: 'chart-title'}, L`${param}: vertical profile`));
      diagrams.append(panel);
      drawVerticalDiagram(panel, param, axis, param, series);
      if (keys.length > 1)
        panel.append(
            el('div',
               {class: 'vlegend small'},
               keys.map(
                   (k, i) =>
                       el('span',
                          null,
                          el('i', {style: `background:${palette[i % palette.length]}`}),
                          k))));
    }
    if (diagrams.children.length === 0)
      diagrams.append(el('p', {class: 'muted'}, L('No values to draw.')));
  }
  draw();
}

// The colours of sections: the production isoband scale of the parameter, as on the value
// maps, unless much of the data is outside it (the production temperature scale does not
// reach the upper atmosphere), when a continuous scale fitted to the data is used. All the
// scales can be chosen.
function colourScale(settings, label, onChange)
{
  let production = {palettes: {}, parameters: {}};
  let chosen = null;  // null: chosen automatically
  const select = el('select', {'aria-label': label});
  const productionGroup = el('optgroup', {label: L('Production isobands')});
  const continuousGroup = el('optgroup',
                             {label: L('Continuous, fitted to the data')},
                             Object.keys(PALETTES).map((p) => el('option', {value: p}, p)));
  select.append(continuousGroup);
  select.addEventListener('change', () => {
    chosen = select.value;
    onChange();
  });
  if (settings && settings.palettes)
    loadPalettes(settings.palettes).then((doc) => {
      production = doc;
      const names = Object.keys(doc.palettes || {}).sort();
      productionGroup.replaceChildren(...names.map((n) => el('option', {value: `production:${n}`}, n)));
      if (names.length > 0)
        select.prepend(productionGroup);
      onChange();
    });
  const bandsOf = (name) => production.palettes[name.replace(/^production:/, '')];
  // values: the values of the parameter, sorted
  function prepare(param, values)
  {
    const own = production.parameters[String(param).toLowerCase()];
    const ownBands = own ? production.palettes[own] : null;
    // The share of the values the production scale covers
    const covered = ownBands ? values.filter((v) => ownBands.some((b) => v >= b[0] && v < b[1])).length /
            Math.max(1, values.length) :
                               0;
    let name = chosen;
    if (!name)
      name = ownBands && covered >= 0.8 ? `production:${own}` :
          /temp|t-k|^t$|^td/i.test(param) && PALETTES.temperature ? 'temperature' : 'viridis';
    select.value = name;
    let note = '';
    if (!chosen && ownBands && covered < 0.8)
      note = L`${Math.round(100 * (1 - covered))} % of the values are outside the production scale ${
          own}: a continuous scale fitted to the data is used`;
    if (name.startsWith('production:') && bandsOf(name))
    {
      const bands = bandsOf(name);
      const text = (v) => (Math.abs(v) >= 999 ? '' : String(v));
      return {
        colour: (v) => {
          const band = bands.find((b) => v >= b[0] && v < b[1]);
          return band ? `rgb(${band[2]},${band[3]},${band[4]})` : 'transparent';
        },
        legend: [el('span',
                    {class: 'legend-bar legend-bands'},
                    bands.map((b) => el('span',
                                        {
                                          class: 'legend-step',
                                          style: `background:rgb(${b[2]},${b[3]},${b[4]})`,
                                          title: `${text(b[0]) || '…'} – ${text(b[1]) || '…'}`
                                        },
                                        el('span', {class: 'legend-label'}, text(b[0])))))],
        note
      };
    }
    const lo = quantile(values, 0.02);
    const hi = quantile(values, 0.98);
    const n = 24;
    return {
      colour: (v) => paletteColour(name, hi > lo ? Math.min(1, Math.max(0, (v - lo) / (hi - lo))) : 0.5, 0),
      legend: [el('span', {class: 'small'}, round(lo, 2)),
               el('span',
                  {class: 'legend-bar'},
                  Array.from({length: n}, (_, i) => el('span', {
                    class: 'legend-step',
                    style: `background:${paletteColour(name, (i + 0.5) / n, 0)}`
                  }))),
               el('span', {class: 'small'}, round(hi, 2))],
      note
    };
  }
  return {select, prepare};
}

// A time-height section of a parameter: time horizontally, the vertical coordinate
// vertically, the values as coloured cells. Each profile keeps its own levels, as the height
// or pressure of model levels changes in time.
// Flight levels: pressure altitudes of the ICAO standard atmosphere in hundreds of feet. The
// troposphere has a lapse rate of 6.5 K/km from 288.15 K and 1013.25 hPa, the stratosphere
// above 11 km a constant 216.65 K.
function flightLevelPressure(fl)
{
  const z = fl * 30.48;
  if (z <= 11000)
    return 1013.25 * Math.pow(1 - (0.0065 * z) / 288.15, 5.25588);
  return 226.321 * Math.exp(-0.000157688 * (z - 11000));
}

// The flight level of a pressure (hPa), the inverse of flightLevelPressure()
function pressureFlightLevel(p)
{
  const z = p >= 226.321 ? (288.15 / 0.0065) * (1 - Math.pow(p / 1013.25, 1 / 5.25588)) :
                           11000 - Math.log(p / 226.321) / 0.000157688;
  return z / 30.48;
}

// The flight levels between two pressures (hPa), at most count of them at round intervals
function flightLevelsBetween(pmin, pmax, count = 8)
{
  const low = Math.max(0, pressureFlightLevel(pmax));
  const high = pressureFlightLevel(pmin);
  const step = [10, 20, 50, 100, 200].find((s) => (high - low) / s <= count) || 500;
  const out = [];
  for (let fl = Math.ceil(low / step) * step; fl <= high; fl += step)
    if (fl > 0)
      out.push(fl);
  return out;
}

// Whether flight levels are drawn on vertical sections, shared by the sections of the page and
// remembered by the browser
const FLIGHT_LEVELS_KEY = 'smartmet-explorer-flight-levels';
let showFlightLevels = (() => {
  try
  {
    return localStorage.getItem(FLIGHT_LEVELS_KEY) === '1';
  }
  catch (e)
  {
    return false;
  }
})();

// A checkbox for the flight levels of a section, hidden when there are no pressures
function flightLevelCheck(pressureAxis, onChange)
{
  const input = el('input', {type: 'checkbox', 'aria-label': L('Flight levels')});
  input.checked = showFlightLevels;
  input.addEventListener('change', () => {
    showFlightLevels = input.checked;
    try
    {
      localStorage.setItem(FLIGHT_LEVELS_KEY, showFlightLevels ? '1' : '0');
    }
    catch (e)
    {
      // Not remembered
    }
    onChange();
  });
  const label = el('label', {class: 'check'}, input, L('Flight levels'));
  label.hidden = !pressureAxis;
  return label;
}

// Flight levels on a section: columns [{x, levels: [{z, p}]}] with the vertical coordinate z of
// the axis and the pressure p (hPa) of each level. Each flight level is drawn where the
// pressures of a column reach it, interpolated in the logarithm of the pressure.
function drawFlightLevels(g, columns, axis, sy, w)
{
  const ps = columns.flatMap((c) => c.levels.map((l) => l.p)).filter((p) => p > 0);
  if (ps.length === 0)
    return;
  // Candidates at fine intervals, thinned below where the axis squeezes them together
  const lines = [];
  for (const fl of flightLevelsBetween(Math.min(...ps), Math.max(...ps), 16))
  {
    const target = Math.log(flightLevelPressure(fl));
    const segments = [[]];
    for (const c of columns)
    {
      const levels = c.levels.filter((l) => l.p > 0);
      let y = null;
      for (let k = 0; k + 1 < levels.length && y === null; k++)
      {
        const a = Math.log(levels[k].p);
        const b = Math.log(levels[k + 1].p);
        if (a === b || (target - a) * (target - b) > 0)
          continue;
        const f = (target - a) / (b - a);
        y = axis.id === 'pressure' ? sy(Math.exp(target)) :
                                     sy(levels[k].z + f * (levels[k + 1].z - levels[k].z));
      }
      if (y === null)
      {
        if (segments[segments.length - 1].length > 0)
          segments.push([]);
        continue;
      }
      segments[segments.length - 1].push([Math.max(0, Math.min(w, c.x)), y]);
    }
    const drawn = segments.filter((s) => s.length > 0);
    if (drawn.length === 0)
      continue;
    const all = drawn.flat();
    lines.push({fl, drawn, y: all.reduce((sum, [, y]) => sum + y, 0) / all.length});
  }
  // From the bottom up, a line at least 16 pixels above the previous one drawn, the round
  // hundreds preferred
  lines.sort((a, b) => b.y - a.y);
  const kept = [];
  for (const line of lines)
  {
    const previous = kept[kept.length - 1];
    if (!previous || previous.y - line.y >= 16)
      kept.push(line);
    else if (line.fl % 100 === 0 && previous.fl % 100 !== 0 &&
             (kept.length < 2 || kept[kept.length - 2].y - line.y >= 16))
      kept[kept.length - 1] = line;
  }
  for (const {fl, drawn} of kept)
  {
    for (const s of drawn)
    {
      // A single column is drawn as a short line
      const pts = s.length > 1 ? s : [[s[0][0] - 6, s[0][1]], [s[0][0] + 6, s[0][1]]];
      g.append(svg('polyline', {
        class: 'flight-level',
        points: pts.map(([x, y]) => `${x.toFixed(1)},${y.toFixed(1)}`).join(' ')
      }));
    }
    const last = drawn[drawn.length - 1];
    const [x, y] = last[last.length - 1];
    // Above the line, or below it at the top of the frame
    const label = svg('text', {
      class: 'flight-level-label',
      x: Math.min(w - 2, x),
      y: y < 12 ? y + 11 : y - 3,
      'text-anchor': 'end'
    });
    label.textContent = L`FL${String(fl).padStart(3, '0')}`;
    g.append(label);
  }
}

// A time-height section at one location: time horizontally, the vertical coordinate
// vertically, the values of a parameter as coloured cells, with the colour scale, axis, top,
// location and flight level choices
// The overlays of vertical sections, shared by the sections of the page and remembered by the
// browser: isotherms (0 C stands out for icing), wind barbs, and the edge of air of 90 % relative
// humidity or more (likely cloud)
const OVERLAYS_KEY = 'smartmet-explorer-section-overlays';
let sectionOverlays = (() => {
  try
  {
    return Object.assign({isotherms: true, wind: false, moist: false},
                         JSON.parse(localStorage.getItem(OVERLAYS_KEY) || '{}'));
  }
  catch (e)
  {
    return {isotherms: true, wind: false, moist: false};
  }
})();

// The checkboxes of the overlays the rows allow: {element, columns}
function overlayChecks(rows, onChange)
{
  const columns = soundingColumns(rows);
  const moist = !!columns.humidity || (!!columns.temperature && !!columns.dewpoint);
  const wind = (!!columns.speed && !!columns.direction) || (!!columns.u && !!columns.v);
  const box = el('span', {class: 'row overlay-checks'});
  for (const [key, label, available] of [
           ['isotherms', L('Isotherms'), !!columns.temperature],
           ['wind', L('Wind'), wind],
           ['moist', L('Humidity 90 %'), moist]])
  {
    if (!available)
      continue;
    const input = el('input', {type: 'checkbox', 'aria-label': label});
    input.checked = !!sectionOverlays[key];
    input.addEventListener('change', () => {
      sectionOverlays = Object.assign({}, sectionOverlays, {[key]: input.checked});
      try
      {
        localStorage.setItem(OVERLAYS_KEY, JSON.stringify(sectionOverlays));
      }
      catch (e)
      {
        // Not remembered
      }
      onChange();
    });
    box.append(el('label', {class: 'check'}, input, label));
  }
  return {element: box, columns};
}

// The values the overlays need from a row: temperature (C), relative humidity (%), wind speed
// (m/s) and direction (deg), null when missing
function overlayValues(row, columns)
{
  const value = (what) => {
    if (!columns[what])
      return null;
    const [name, offset] = columns[what];
    const v = columnValue(row, name);
    return v === null ? null : v + offset;
  };
  const t = value('temperature');
  const td = value('dewpoint');
  let rh = value('humidity');
  if (rh === null && t !== null && td !== null)
    rh = (100 * saturationPressure(td)) / saturationPressure(t);
  let speed = value('speed');
  let direction = value('direction');
  const u = value('u');
  const v = value('v');
  if ((speed === null || direction === null) && u !== null && v !== null)
  {
    speed = Math.hypot(u, v);
    direction = (Math.atan2(-u, -v) * 180) / Math.PI;
    direction = (direction + 360) % 360;
  }
  return {t, rh, speed, direction};
}

// Contour lines by marching squares on a grid of points: nodes[i][k] = {x, y, v} or null, the
// columns i side by side and the levels k one above the other. Returns the segments
// [[x1, y1, x2, y2]] of the value c.
function contourSegments(nodes, c)
{
  const out = [];
  const cross = (a, b) => {
    const f = (c - a.v) / (b.v - a.v);
    return [a.x + f * (b.x - a.x), a.y + f * (b.y - a.y)];
  };
  for (let i = 0; i + 1 < nodes.length; i++)
    for (let k = 0; k + 1 < Math.min(nodes[i].length, nodes[i + 1].length); k++)
    {
      const corners = [nodes[i][k], nodes[i + 1][k], nodes[i + 1][k + 1], nodes[i][k + 1]];
      if (corners.some((n) => !n || n.v === null))
        continue;
      // The crossings on the edges a-b, b-c, c-d, d-a
      const points = [];
      for (let e = 0; e < 4; e++)
      {
        const a = corners[e];
        const b = corners[(e + 1) % 4];
        if ((a.v < c) !== (b.v < c))
          points.push(cross(a, b));
      }
      if (points.length === 2)
        out.push([...points[0], ...points[1]]);
      else if (points.length === 4)
      {
        // A saddle: the mean of the corners decides which corners are joined
        const mean = corners.reduce((s, n) => s + n.v, 0) / 4;
        const firstBelow = corners[0].v < c;
        if ((mean < c) === firstBelow)
          out.push([...points[0], ...points[1]], [...points[2], ...points[3]]);
        else
          out.push([...points[0], ...points[3]], [...points[1], ...points[2]]);
      }
    }
  return out;
}

// Draws the chosen overlays on a section. columns: [{x, levels: [{y, row}]}] with the levels of
// each column in the same order (by their level), w and h the size of the plot.
let overlayClips = 0;
function drawSectionOverlays(target, columns, rowColumns, w, h)
{
  // Clipped to the plot: barbs at the edges would reach out of it
  const id = `xs-clip-${++overlayClips}`;
  const clip = svg('clipPath', {id});
  clip.append(svg('rect', {x: 0, y: 0, width: w, height: h}));
  const g = svg('g', {'clip-path': `url(#${id})`});
  target.append(clip, g);
  const grid = columns.map((c) => c.levels.map((l) => Object.assign(
                                                    {x: c.x, y: l.y}, overlayValues(l.row, rowColumns))));
  const path = (segments, cls) => {
    if (segments.length === 0)
      return null;
    const d = segments.map(([x1, y1, x2, y2]) => `M${x1.toFixed(1)},${y1.toFixed(1)}L${
                               x2.toFixed(1)},${y2.toFixed(1)}`)
                  .join('');
    const p = svg('path', {class: cls, d});
    g.append(p);
    return p;
  };
  // A label of a contour at its segment nearest to the middle of the plot
  const label = (segments, text, cls) => {
    let best = null;
    let bestD = Infinity;
    for (const [x1, y1, x2, y2] of segments)
    {
      const x = (x1 + x2) / 2;
      const y = (y1 + y2) / 2;
      const d = Math.abs(x - w / 2) + Math.abs(y - h / 2) / 4;
      if (x > 20 && x < w - 20 && y > 10 && y < h - 4 && d < bestD)
      {
        bestD = d;
        best = [x, y];
      }
    }
    if (!best)
      return;
    const t = svg('text', {class: cls, x: best[0], y: best[1] + 3, 'text-anchor': 'middle'});
    t.textContent = text;
    g.append(t);
  };
  if (sectionOverlays.moist && rowColumns && grid.some((c) => c.some((n) => n.rh !== null)))
  {
    const segments = contourSegments(grid.map((c) => c.map((n) => Object.assign({}, n, {v: n.rh}))), 90);
    path(segments, 'xs-moist');
  }
  if (sectionOverlays.isotherms && grid.some((c) => c.some((n) => n.t !== null)))
  {
    const ts = grid.flat().map((n) => n.t).filter((v) => v !== null);
    const lo = Math.min(...ts);
    const hi = Math.max(...ts);
    const step = hi - lo > 60 ? 10 : 5;
    const nodes = grid.map((c) => c.map((n) => Object.assign({}, n, {v: n.t})));
    for (let c = Math.ceil(lo / step) * step; c <= hi; c += step)
    {
      const segments = contourSegments(nodes, c);
      path(segments, c === 0 ? 'xs-isotherm zero' : 'xs-isotherm');
      label(segments, `${c}°`, c === 0 ? 'xs-isotherm-label zero' : 'xs-isotherm-label');
    }
  }
  if (sectionOverlays.wind)
  {
    // Barbs at least 40 pixels apart horizontally and 30 vertically
    let lastX = -Infinity;
    for (const column of grid)
    {
      if (column.length === 0 || column[0].x - lastX < 40)
        continue;
      lastX = column[0].x;
      let lastY = Infinity;
      for (const n of column.slice().sort((a, b) => b.y - a.y))
        if (n.speed !== null && n.direction !== null && lastY - n.y >= 30 && n.y > 14 && n.y < h - 4)
        {
          g.append(windBarb(n.x, n.y, n.speed, n.direction));
          lastY = n.y;
        }
    }
  }
}

function drawSection(container, rows, parameters, levelKind, settings)
{
  container.replaceChildren();
  const axes = verticalAxes(rows, levelKind);
  const numeric = parameters.filter((p) => rows.some((r) => columnValue(r, p) !== null));
  if (numeric.length === 0)
  {
    container.append(el('p', {class: 'muted'}, L('No values to draw.')));
    return;
  }
  const state = {axis: axes[0], param: numeric[0], palette: 'viridis', key: null};
  const pressureAxis = axes.find((a) => a.id === 'pressure');
  const flightLevels = flightLevelCheck(pressureAxis, () => draw());
  const overlays = overlayChecks(rows, () => draw());
  const axisSelect =
      el('select',
         {'aria-label': L('Vertical axis of the section')},
         axes.map((a) => el('option', {value: a.id}, a.label)));
  const paramSelect =
      el('select',
         {'aria-label': L('Parameter of the section')},
         numeric.map((p) => el('option', {value: p}, p)));
  const scale = colourScale(settings, L('Colours of the section'), () => draw());
  const paletteSelect = scale.select;
  const keySelect = el('select', {'aria-label': L('Location of the section')});
  const topSelect = el('select', {'aria-label': L('Top of the section')});
  // The tops the axis offers; the top is reset when the axis changes
  const setTops = () => {
    topSelect.replaceChildren(
        ...verticalTops(state.axis).map(([v, l]) => el('option', {value: v}, l)));
    state.top = '';
    topSelect.hidden = topSelect.options.length < 2;
  };
  setTops();
  const readout = el('div', {class: 'sk-readout small'});
  const legend = el('div', {class: 'legend'});
  const plot = el('div', {class: 'section'});
  axisSelect.hidden = axes.length < 2;
  container.append(
      el('div',
         {class: 'row'},
         el('label', null, L('Parameter'), paramSelect),
         el('label', null, L('Vertical axis'), axisSelect),
         el('label', null, L('Top'), topSelect),
         el('label', null, L('Colours'), paletteSelect),
         keySelect,
         flightLevels,
         overlays.element),
      legend,
      readout,
      plot);
  axisSelect.addEventListener('change', () => {
    state.axis = axes.find((a) => a.id === axisSelect.value) || axes[0];
    setTops();
    draw();
  });
  topSelect.addEventListener('change', () => {
    state.top = topSelect.value;
    draw();
  });
  paramSelect.addEventListener('change', () => {
    state.param = paramSelect.value;
    draw();
  });
  keySelect.addEventListener('change', () => {
    state.key = keySelect.value;
    draw();
  });

  // Draws the section of the chosen parameter at the chosen location
  function draw()
  {
    plot.replaceChildren();
    const all = collectProfiles(rows, state.axis);
    const keys = Array.from(new Set(all.map((p) => p.key)));
    keySelect.replaceChildren(...keys.map((k) => el('option', {value: k}, k)));
    keySelect.hidden = keys.length < 2;
    if (!keys.includes(state.key))
      state.key = keys[0];
    keySelect.value = state.key;
    const profiles = all.filter((p) => p.key === state.key).sort((a, b) => a.time - b.time);
    if (profiles.length === 0)
    {
      plot.append(el('p', {class: 'muted'}, L('No profiles to draw.')));
      return;
    }
    const cells = profiles.map(
        (p) => p.rows.filter(({z}) => withinTop(state.axis, state.top, z))
                   .map(({z, row}) => ({
                          z,
                          v: columnValue(row, state.param),
                          p: pressureAxis ? pressureAxis.value(row) : null,
                          row
                        }))
                   .filter((c) => c.v !== null));
    const values = cells.flat().map((c) => c.v).sort((a, b) => a - b);
    const zs = cells.flat().map((c) => c.z);
    if (values.length === 0)
    {
      plot.append(el('p', {class: 'muted'}, L('No values to draw.')));
      return;
    }
    const prepared = scale.prepare(state.param, values);
    const colour = prepared.colour;

    const width = Math.max(360, Math.min(900, container.clientWidth || 640));
    const height = 400;
    const m = {left: 56, right: 12, top: 10, bottom: 40};
    const w = width - m.left - m.right;
    const h = height - m.top - m.bottom;
    let zmin = Math.min(...zs);
    const zmax = Math.max(...zs);
    if (state.axis.id === 'depth' || state.axis.id === 'height')
      zmin = Math.min(0, zmin);
    const sy = verticalScale(state.axis, zmin, zmax, h);
    const times = profiles.map((p) => p.time);
    // Each time is a column reaching halfway to its neighbours
    const t0 = times[0];
    const t1 = times[times.length - 1];
    const half = times.length > 1 ? (times[1] - times[0]) / 2 : 1800e3;
    const sx = (t) => ((t - (t0 - half)) / (t1 + half - (t0 - half))) * w;

    const root = svg('svg', {
      class: 'vdiagram',
      width,
      height,
      viewBox: `0 0 ${width} ${height}`,
      role: 'img',
      'aria-label': L`${state.param}: time-height section`
    });
    const g = svg('g', {transform: `translate(${m.left},${m.top})`});
    root.append(g);
    // Cells reach halfway to the neighbouring times and levels, the edges as far outwards
    const edges = (list) => list.map((v, k) => {
      const before = k > 0 ? (list[k - 1] + v) / 2 : v - (list.length > 1 ? (list[1] - v) / 2 : 10);
      const after = k < list.length - 1 ? (v + list[k + 1]) / 2 :
                                          v + (list.length > 1 ? (v - list[k - 1]) / 2 : 10);
      return [Math.min(before, after), Math.max(before, after)];
    });
    const columns = edges(times.map(sx));
    profiles.forEach((p, i) => {
      const [left, right] = columns[i];
      const levels = cells[i];
      const ys = edges(levels.map((c) => sy(c.z)));
      levels.forEach((c, k) => {
        const [top, bottom] = ys[k];
        const rect = svg('rect', {
          class: 'section-cell',
          x: left,
          y: top,
          width: Math.max(1, right - left + 0.5),
          height: Math.max(1, bottom - top + 0.5),
          fill: colour(c.v)
        });
        rect.addEventListener('mouseenter', () => {
          readout.textContent =
              `${formatTime(new Date(p.time))} · ${state.axis.label} ${round(c.z, 1)}${
                  state.axis.unit ? ` ${state.axis.unit}` : ''} · ${state.param} ${round(c.v, 2)}`;
        });
        g.append(rect);
      });
    });
    root.addEventListener('mouseleave', () => {
      readout.textContent = '';
    });
    drawSectionOverlays(
        g,
        profiles.map((p, i) => ({x: sx(p.time), levels: cells[i].map((c) => ({y: sy(c.z), row: c.row}))})),
        overlays.columns,
        w,
        h);
    if (showFlightLevels && pressureAxis)
      drawFlightLevels(
          g, profiles.map((p, i) => ({x: sx(p.time), levels: cells[i]})), state.axis, sy, w);
    for (const z of verticalTicks(state.axis, zmin, zmax))
    {
      const t = svg('text', {class: 'vtick', x: -6, y: sy(z) + 4, 'text-anchor': 'end'});
      t.textContent = String(z);
      g.append(t);
    }
    const step = Math.max(1, Math.ceil(times.length / 8));
    times.forEach((tm, i) => {
      if (i % step !== 0)
        return;
      const label = svg('text', {class: 'vtick', x: sx(tm), y: h + 16, 'text-anchor': 'middle'});
      const d = new Date(tm);
      label.textContent = `${pad(d.getUTCDate())}.${pad(d.getUTCMonth() + 1)} ${
          pad(d.getUTCHours())}:${pad(d.getUTCMinutes())}`;
      g.append(label);
    });
    g.append(svg('rect', {class: 'vframe', x: 0, y: 0, width: w, height: h}));
    const ylabel = svg('text', {
      class: 'vlabel',
      transform: `translate(${- 44},${h / 2}) rotate(-90)`,
      'text-anchor': 'middle'
    });
    ylabel.textContent = `${state.axis.label}${state.axis.unit ? ` (${state.axis.unit})` : ''}`;
    g.append(ylabel);
    const xlabel = svg('text', {class: 'vlabel', x: w / 2, y: h + 34, 'text-anchor': 'middle'});
    xlabel.textContent = L('Time (UTC)');
    g.append(xlabel);
    plot.append(root);
    legend.replaceChildren(...prepared.legend, prepared.note ? el('span', {class: 'small warn'}, prepared.note) : '');
  }
  draw();
}

// The points of a path in the order of the path: [{lat, lon, km}] with the distance from the
// start. The rows of a path query are its points at each level, possibly in several rows.
function pathPoints(rows)
{
  const points = [];
  const seen = new Set();
  for (const r of rows)
  {
    if (typeof r.lat !== 'number' || typeof r.lon !== 'number')
      continue;
    const key = `${r.lat.toFixed(5)},${r.lon.toFixed(5)}`;
    if (seen.has(key))
      continue;
    seen.add(key);
    points.push({lat: r.lat, lon: r.lon, key});
  }
  let km = 0;
  points.forEach((p, i) => {
    if (i > 0)
      km += distanceKm(points[i - 1].lon, points[i - 1].lat, p.lon, p.lat);
    p.km = km;
  });
  return points;
}

// The time series along a path: a slider (and an animation) over the points of the path by
// their distance from the start, and a chart of each parameter at the chosen point against the
// median and the 10-90 % range of the whole path. The vertical scales are those of the whole
// path, so that the steps can be compared. options: onPosition({lat, lon} or null) to show the
// point elsewhere, e.g. on a map; flight (departure and arrival); labels (the units of the
// columns).
function drawAlongPath(container, rows, parameters, levelKind, options = {})
{
  container.replaceChildren();
  const root = typeof window !== 'undefined' ? window : globalThis;
  const {levels, fallback} = chartLevels(rows, levelKind);
  let level = levels.length > 0 ? fallback || String(levels[0]) : '';
  const levelSelect = el('select', {'aria-label': L('Level along the path')},
                         levels.map((v) => el('option', {value: String(v)}, String(v))));
  levelSelect.value = level;
  const levelLabel = el('label', null, L('Level'), levelSelect);
  levelLabel.hidden = levels.length === 0;
  const charts = el('div', {class: 'along-path'});
  // The distance along the path, and the level of the values
  const distance = el('div', {class: 'along-path-distance'});
  const head = el('div', {class: 'row'}, el('label', null, L('Distance'), distance), levelLabel);
  container.append(head, charts);
  levelSelect.addEventListener('change', () => {
    level = levelSelect.value;
    build();
  });

  let slider = null;
  // Builds the slider and the charts of the chosen level
  function build()
  {
    charts.replaceChildren();
    distance.replaceChildren();
    const chosen = level === '' ? rows : rows.filter((r) => r.level === Number(level));
    const points = pathPoints(chosen);
    if (points.length < 2)
    {
      charts.append(el('p', {class: 'muted'}, L('The path has fewer than two points with values.')));
      return;
    }
    const index = new Map(points.map((p, i) => [p.key, i]));
    // On a flight, the time each point is passed is marked on its charts
    const passed = options.flight ? flightTimes(points, options.flight) : null;
    let current = 0;
    // The passing time and the value met there marked on a time series of parameter p
    const markPassed = (p) => (u) => {
      if (!passed)
        return;
      const x = u.valToPos(passed[current] / 1000, 'x', true);
      const {top, height} = u.bbox;
      const ctx = u.ctx;
      const ratio = typeof devicePixelRatio === 'number' ? devicePixelRatio : 1;
      ctx.save();
      ctx.strokeStyle = '#d9480f';
      ctx.lineWidth = 2 * ratio;
      ctx.setLineDash([6, 4]);
      ctx.beginPath();
      ctx.moveTo(x, top);
      ctx.lineTo(x, top + height);
      ctx.stroke();
      const v = met(p, current);
      if (v !== null)
      {
        ctx.setLineDash([]);
        ctx.fillStyle = '#d9480f';
        ctx.beginPath();
        ctx.arc(x, u.valToPos(v, 'y', true), 5 * ratio, 0, 2 * Math.PI);
        ctx.fill();
      }
      ctx.restore();
    };
    const times = Array.from(new Set(chosen.map((r) => {
                    const d = parseTime(r.time);
                    return d ? Math.round(d.getTime() / 1000) : null;
                  }).filter((x) => x !== null)))
                      .sort((a, b) => a - b);
    const timeIndex = new Map(times.map((x, i) => [x, i]));
    const numeric = parameters.filter((p) => chosen.some((r) => typeof r[p] === 'number'));
    // values[parameter][point][time]
    const values = {};
    for (const p of numeric)
      values[p] = points.map(() => times.map(() => null));
    for (const r of chosen)
    {
      const i = typeof r.lat === 'number' ? index.get(`${r.lat.toFixed(5)},${r.lon.toFixed(5)}`) : undefined;
      const d = parseTime(r.time);
      const k = d ? timeIndex.get(Math.round(d.getTime() / 1000)) : undefined;
      if (i === undefined || k === undefined)
        continue;
      for (const p of numeric)
        if (typeof r[p] === 'number' && isFinite(r[p]))
          values[p][i][k] = r[p];
    }
    // The value of parameter p met at point i of a flight: interpolated to the time the point is
    // passed, directions the shorter way round; null outside the times of the data
    const directions = new Set(SOUNDING_PARAMS.direction.map(([n]) => n));
    const met = (p, i) => {
      if (!passed)
        return null;
      const tp = passed[i] / 1000;
      const series = values[p][i];
      for (let k = 0; k + 1 < times.length; k++)
        if (times[k] <= tp && tp <= times[k + 1] && series[k] !== null && series[k + 1] !== null)
        {
          const f = (tp - times[k]) / (times[k + 1] - times[k]);
          if (directions.has(p.toLowerCase()))
            return (series[k] + f * ((((series[k + 1] - series[k]) % 360) + 540) % 360 - 180) + 360) % 360;
          return series[k] + f * (series[k + 1] - series[k]);
        }
      return tp === times[times.length - 1] ? series[times.length - 1] : null;
    };
    const css = getComputedStyle(document.documentElement);
    const fg = css.getPropertyValue('--fg').trim() || '#000';
    const accent = css.getPropertyValue('--accent').trim() || '#0969da';
    const axisColor = css.getPropertyValue('--muted').trim() || '#666';
    const gridColor = css.getPropertyValue('--line').trim() || '#ddd';
    const width = Math.max(320, Math.min(900, container.clientWidth - 8 || 640));
    const plots = numeric.map((p) => {
      // The median and the 10-90 % range of the whole path at each time
      const stats = times.map((x, k) => {
        const column = values[p].map((series) => series[k]).filter((v) => v !== null).sort((a, b) => a - b);
        return column.length ? [quantile(column, 0.1), quantile(column, 0.5), quantile(column, 0.9)] : [null, null, null];
      });
      const all = values[p].flat().filter((v) => v !== null);
      const lo = Math.min(...all);
      const hi = Math.max(...all);
      const pad = (hi - lo || Math.abs(hi) || 1) * 0.05;
      const panel = el('div', {class: 'chart', 'data-param': p});
      const title = el('div', {class: 'chart-title'});
      const plot = el('div', {class: 'chart-plot'});
      panel.append(title, plot);
      charts.append(panel);
      const band = 'rgba(128, 128, 128, 0.18)';
      const u = makePlot(root,
          {
            width,
            height: 220,
            series: [
              {value: (u, x) => (x === null ? '--' : formatTime(new Date(x * 1000)))},
              {label: L('10 % of the path'), stroke: 'transparent', points: {show: false}},
              {label: L('90 % of the path'), stroke: 'transparent', points: {show: false}},
              {label: L('median of the path'), stroke: axisColor, width: 1, dash: [4, 3], points: {show: false}},
              {label: p, stroke: accent, width: 2.5}
            ],
            bands: [{series: [2, 1], fill: band}],
            axes: [
              {
                stroke: axisColor,
                grid: {stroke: gridColor, width: 1},
                ticks: {stroke: gridColor},
                values: utcTimeLabels,
                size: 50
              },
              {stroke: axisColor, grid: {stroke: gridColor, width: 1}, ticks: {stroke: gridColor}, size: 60}
            ],
            scales: {x: {time: true}, y: {range: () => [lo - pad, hi + pad]}},
            legend: {live: true},
            hooks: {draw: [markPassed(p)]},
            tzDate: (ts) => root.uPlot.tzDate(new Date(ts * 1000), 'Etc/UTC')
          },
          [times, stats.map((s) => s[0]), stats.map((s) => s[2]), stats.map((s) => s[1]), values[p][0]],
          plot);
      return {p, u, title, stats};
    });
    if (plots.length === 0)
    {
      charts.append(el('p', {class: 'muted'}, L('No values to draw.')));
      return;
    }
    // A flight: the values met along it, each point at the time it is passed, against the
    // distance, the current point marked. Drawn above the time series of the points.
    const flightPlots = [];
    if (passed)
    {
      const kms = points.map((pt) => pt.km);
      const total = kms[kms.length - 1] || 1;
      const timeAt = (km) => new Date(options.flight.departure +
                                      ((options.flight.arrival - options.flight.departure) * km) / total);
      const flightBox = el('div', {class: 'along-flight'});
      charts.prepend(flightBox);
      for (const p of numeric)
      {
        const series = points.map((_, i) => met(p, i));
        if (series.every((v) => v === null))
          continue;
        const unit = (options.labels || {})[p];
        const panel = el('div', {class: 'chart', 'data-param': `flight-${p}`});
        const title = el('div', {class: 'chart-title'},
                         L`${p}${unit ? ` (${unit})` : ''} met on the flight, ${formatTime(new Date(options.flight.departure))} – ${
                             formatTime(new Date(options.flight.arrival))}`);
        const plot = el('div', {class: 'chart-plot'});
        panel.append(title, plot);
        flightBox.append(panel);
        const markCurrent = (u) => {
          const x = u.valToPos(kms[current], 'x', true);
          const v = series[current];
          const ctx = u.ctx;
          const ratio = typeof devicePixelRatio === 'number' ? devicePixelRatio : 1;
          ctx.save();
          ctx.strokeStyle = '#d9480f';
          ctx.lineWidth = 2 * ratio;
          ctx.setLineDash([6, 4]);
          ctx.beginPath();
          ctx.moveTo(x, u.bbox.top);
          ctx.lineTo(x, u.bbox.top + u.bbox.height);
          ctx.stroke();
          if (v !== null)
          {
            ctx.setLineDash([]);
            ctx.fillStyle = '#d9480f';
            ctx.beginPath();
            ctx.arc(x, u.valToPos(v, 'y', true), 5 * ratio, 0, 2 * Math.PI);
            ctx.fill();
          }
          ctx.restore();
        };
        const u = makePlot(root,
            {
              width,
              height: 200,
              series: [
                {value: (u, km) => (km === null ? '--' : `${Math.round(km)} km, ${formatTime(timeAt(km))}`)},
                // Directions as points: a line from 350 to 10 degrees would cross the chart
                directions.has(p.toLowerCase()) ?
                    {label: L`${p} met`, stroke: accent, width: 0, paths: () => null, points: {show: true, size: 6, fill: accent}} :
                    {label: L`${p} met`, stroke: accent, width: 2.5, spanGaps: true}
              ],
              axes: [
                {
                  stroke: axisColor,
                  grid: {stroke: gridColor, width: 1},
                  ticks: {stroke: gridColor},
                  size: 50,
                  // The distance and the time it is passed
                  values: (u, splits) => splits.map((km) => {
                    const d = timeAt(km);
                    return `${Math.round(km)} km\n${pad(d.getUTCHours())}:${pad(d.getUTCMinutes())}`;
                  })
                },
                {stroke: axisColor, grid: {stroke: gridColor, width: 1}, ticks: {stroke: gridColor}, size: 60}
              ],
              scales: {x: {time: false}},
              legend: {live: true},
              hooks: {draw: [markCurrent]}
            },
            [kms, series], plot);
        flightPlots.push(u);
      }
      if (flightPlots.length)
        flightBox.append(el('div', {class: 'chart-title'}, L('The time series of the point, against the path')));
    }
    // Shows point i: its series in each chart, and its place on the map
    const show = (i) => {
      const point = points[i];
      current = i;
      const when = passed ? `, passed ${formatTime(new Date(passed[i]))}` : '';
      for (const {p, u, title, stats} of plots)
      {
        // Redrawn on the same scales: the times do not change, the vertical range is fixed
        u.setData([times, stats.map((s) => s[0]), stats.map((s) => s[2]), stats.map((s) => s[1]), values[p][i]]);
        const unit = (options.labels || {})[p];
        const v = met(p, i);
        title.textContent = L`${p}${unit ? ` (${unit})` : ''} at ${Math.round(point.km)} km of ${Math.round(points[points.length - 1].km)} km (${
            point.lat.toFixed(3)}, ${point.lon.toFixed(3)})${when}${v !== null ? `: ${round(v, 1)}` : ''}, against the median and 10-90 % of the path`;
      }
      for (const u of flightPlots)
        u.redraw(false);
      if (options.onPosition)
        options.onPosition({lat: point.lat, lon: point.lon});
    };
    slider = timeSlider(points.map((pt) => pt.km), {
      label: L('Position along the path'),
      positions: points.map((pt) => pt.km),
      format: (km, i) => `${Math.round(km)} km (${points[i].lat.toFixed(3)}, ${points[i].lon.toFixed(3)})${
          passed ? `, ${formatTime(new Date(passed[i]))}` : ''}`,
      onChange: () => show(slider.index())
    });
    distance.append(slider.element);
    show(0);
  }
  build();
}

// Whether the rows are a vertical cross section: several levels at several points of a path
function hasCrossSection(rows)
{
  return pathPoints(rows).length >= 3 &&
      new Set(rows.map((r) => r.level).filter((v) => typeof v === 'number')).size >= 3;
}

// A cross section along a path: the distance along the path horizontally, the vertical
// coordinate vertically, the values of a parameter at a time as coloured cells
function drawCrossSection(container, rows, parameters, levelKind, settings, options = {})
{
  container.replaceChildren();
  const axes = verticalAxes(rows, levelKind);
  const numeric = parameters.filter((p) => rows.some((r) => columnValue(r, p) !== null));
  if (numeric.length === 0)
  {
    container.append(el('p', {class: 'muted'}, L('No values to draw.')));
    return;
  }
  const points = pathPoints(rows);
  const index = new Map(points.map((p, i) => [p.key, i]));
  const times = Array.from(new Set(rows.map((r) => parseTime(r.time)).filter(Boolean).map((d) => d.getTime())))
                    .sort((a, b) => a - b);
  const state = {axis: axes[0], param: numeric[0], palette: 'viridis', top: '', time: 0};
  const pressureAxis = axes.find((a) => a.id === 'pressure');
  const flightLevels = flightLevelCheck(pressureAxis, () => draw());
  const overlays = overlayChecks(rows, () => draw());
  const select = (label, items, value) => {
    const s = el('select', {'aria-label': label}, items.map(([v, l]) => el('option', {value: v}, l)));
    s.value = value;
    return s;
  };
  const paramSelect = select(L('Parameter of the cross section'), numeric.map((p) => [p, p]), state.param);
  const axisSelect = select(L('Vertical axis of the cross section'), axes.map((a) => [a.id, a.label]), state.axis.id);
  const scale = colourScale(settings, L('Colours of the cross section'), () => draw());
  const paletteSelect = scale.select;
  const topSelect = el('select', {'aria-label': L('Top of the cross section')});
  // The tops the axis offers; the top is reset when the axis changes
  const setTops = () => {
    topSelect.replaceChildren(...verticalTops(state.axis).map(([v, l]) => el('option', {value: v}, l)));
    state.top = '';
    topSelect.hidden = topSelect.options.length < 2;
  };
  setTops();
  axisSelect.hidden = axes.length < 2;
  const slider = timeSlider(times, {
    label: L('Time of the cross section'),
    onChange: () => {
      state.time = slider.index();
      draw();
    }
  });
  // A flight: the values at the times the points are passed, instead of at one time
  const flight = options.flight || null;
  const flown = flight ? flightRows(rows, flight) : [];
  state.flight = flown.length > 0;
  const flightBox = el('input', {type: 'checkbox', 'aria-label': L('At the times of the flight')});
  flightBox.checked = state.flight;
  const flightLabel = el('label', {class: 'check'}, flightBox, flight ?
      L`At the times of the flight, ${formatTime(new Date(flight.departure))} – ${
          formatTime(new Date(flight.arrival))}` :
      '');
  flightLabel.hidden = flown.length === 0;
  flightBox.addEventListener('change', () => {
    state.flight = flightBox.checked;
    slider.element.hidden = state.flight;
    draw();
  });
  slider.element.hidden = state.flight;
  const readout = el('div', {class: 'sk-readout small'});
  const legend = el('div', {class: 'legend'});
  const plot = el('div', {class: 'section'});
  container.append(el('div',
                      {class: 'row'},
                      el('label', null, L('Parameter'), paramSelect),
                      el('label', null, L('Vertical axis'), axisSelect),
                      el('label', null, L('Top'), topSelect),
                      el('label', null, L('Colours'), paletteSelect),
                      flightLevels,
                      overlays.element),
                   flightLabel,
                   slider.element,
                   legend,
                   readout,
                   plot);
  for (const [control, apply] of [
           [paramSelect, () => (state.param = paramSelect.value)],
           [axisSelect, () => {
              state.axis = axes.find((a) => a.id === axisSelect.value) || axes[0];
              setTops();
            }],
           [topSelect, () => (state.top = topSelect.value)]])
    control.addEventListener('change', () => {
      apply();
      draw();
    });
  // Draws the cross section at the chosen time
  function draw()
  {
    plot.replaceChildren();
    const time = times[state.time];
    // The levels of each point of the path at the chosen time, or at the times of the flight
    const columns = points.map(() => []);
    // The colours are kept for all times, so that they keep their meaning
    const all = [];
    const shown = state.flight ? flown : rows.filter((r) => {
      const t = parseTime(r.time);
      return t && t.getTime() === time;
    });
    for (const r of rows)
    {
      const v = columnValue(r, state.param);
      const z = state.axis.value(r);
      if (v !== null && z !== null && withinTop(state.axis, state.top, z))
        all.push(v);
    }
    for (const r of shown)
    {
      const i = typeof r.lat === 'number' ? index.get(`${r.lat.toFixed(5)},${r.lon.toFixed(5)}`) : undefined;
      const z = state.axis.value(r);
      const v = columnValue(r, state.param);
      if (i === undefined || z === null || v === null || !withinTop(state.axis, state.top, z))
        continue;
      columns[i].push({z, v, p: pressureAxis ? pressureAxis.value(r) : null, row: r});
    }
    for (const c of columns)
      c.sort((a, b) => a.z - b.z);
    if (all.length === 0)
    {
      plot.append(el('p', {class: 'muted'}, L('No values to draw.')));
      return;
    }
    all.sort((a, b) => a - b);
    const prepared = scale.prepare(state.param, all);
    const colour = prepared.colour;

    const width = Math.max(360, Math.min(900, container.clientWidth || 640));
    const height = 400;
    const m = {left: 56, right: 12, top: 10, bottom: 40};
    const w = width - m.left - m.right;
    const h = height - m.top - m.bottom;
    const zs = columns.flat().map((c) => c.z);
    let zmin = Math.min(...zs);
    const zmax = Math.max(...zs);
    if (state.axis.id === 'depth' || state.axis.id === 'height')
      zmin = Math.min(0, zmin);
    const sy = verticalScale(state.axis, zmin, zmax, h);
    const total = points[points.length - 1].km || 1;
    const sx = (km) => (km / total) * w;
    // Cells reach halfway to the neighbouring points and levels
    const edges = (list) => list.map((v, k) => {
      const before = k > 0 ? (list[k - 1] + v) / 2 : v - (list.length > 1 ? (list[1] - v) / 2 : 10);
      const after = k < list.length - 1 ? (v + list[k + 1]) / 2 : v + (list.length > 1 ? (v - list[k - 1]) / 2 : 10);
      return [Math.max(0, Math.min(before, after)), Math.min(w, Math.max(before, after))];
    });
    const xs = edges(points.map((p) => sx(p.km)));
    const root = svg('svg', {
      class: 'vdiagram',
      width,
      height,
      viewBox: `0 0 ${width} ${height}`,
      role: 'img',
      'aria-label': L`${state.param}: cross section along the path`
    });
    const g = svg('g', {transform: `translate(${m.left},${m.top})`});
    root.append(g);
    points.forEach((p, i) => {
      const levels = columns[i];
      if (levels.length === 0)
        return;
      const ys = edges(levels.map((c) => sy(c.z)));
      levels.forEach((c, k) => {
        const rect = svg('rect', {
          class: 'section-cell',
          x: xs[i][0],
          y: ys[k][0],
          width: Math.max(1, xs[i][1] - xs[i][0] + 0.5),
          height: Math.max(1, ys[k][1] - ys[k][0] + 0.5),
          fill: colour(c.v)
        });
        rect.addEventListener('mouseenter', () => {
          readout.textContent = L`${Math.round(p.km)} km (${p.lat.toFixed(3)}, ${p.lon.toFixed(3)}) · ${
              state.flight ? L`passed ${formatTime(new Date(c.row.time))} · ` : ''}${
              state.axis.label} ${round(c.z, 1)}${state.axis.unit ? ` ${state.axis.unit}` : ''} · ${
              state.param} ${round(c.v, 2)}`;
        });
        g.append(rect);
      });
    });
    root.addEventListener('mouseleave', () => {
      readout.textContent = '';
    });
    drawSectionOverlays(
        g,
        points.map((p, i) => ({x: sx(p.km), levels: columns[i].map((c) => ({y: sy(c.z), row: c.row}))})),
        overlays.columns,
        w,
        h);
    if (showFlightLevels && pressureAxis)
      drawFlightLevels(
          g, points.map((p, i) => ({x: sx(p.km), levels: columns[i]})), state.axis, sy, w);
    for (const z of verticalTicks(state.axis, zmin, zmax))
    {
      const label = svg('text', {class: 'vtick', x: -6, y: sy(z) + 4, 'text-anchor': 'end'});
      label.textContent = String(z);
      g.append(label);
    }
    for (const km of niceTicks(0, total, 8))
    {
      const label = svg('text', {class: 'vtick', x: sx(km), y: h + 16, 'text-anchor': 'middle'});
      label.textContent = String(km);
      g.append(label);
    }
    g.append(svg('rect', {class: 'vframe', x: 0, y: 0, width: w, height: h}));
    const ylabel = svg('text', {
      class: 'vlabel',
      transform: `translate(${-44},${h / 2}) rotate(-90)`,
      'text-anchor': 'middle'
    });
    ylabel.textContent = `${state.axis.label}${state.axis.unit ? ` (${state.axis.unit})` : ''}`;
    g.append(ylabel);
    const xlabel = svg('text', {class: 'vlabel', x: w / 2, y: h + 34, 'text-anchor': 'middle'});
    xlabel.textContent = state.flight ?
        L`Distance along the flight (km), the values at the times the points are passed` :
        L('Distance along the path (km)');
    g.append(xlabel);
    plot.append(root);
    legend.replaceChildren(...prepared.legend, prepared.note ? el('span', {class: 'small warn'}, prepared.note) : '');
  }
  draw();
}

// ----------------------------------------------------------------------
// Thermodynamics of soundings
// ----------------------------------------------------------------------

const KELVIN = 273.15;
const RD = 287.04;
const CP = 1005.7;
const LV = 2.501e6;
const EPSILON = 0.622;
const KAPPA = RD / CP;

// Saturation vapour pressure (hPa) over water at T (C)
const saturationPressure = (t) => 6.112 * Math.exp((17.67 * t) / (t + 243.5));

// Dewpoint (C) of a vapour pressure (hPa)
function dewpointOf(e)
{
  const l = Math.log(Math.max(e, 1e-9) / 6.112);
  return (243.5 * l) / (17.67 - l);
}

// Dewpoint (C) from the temperature (C) and relative humidity (%)
const dewpointFromHumidity = (t, rh) =>
    dewpointOf((Math.max(rh, 0.1) / 100) * saturationPressure(t));

// Saturation mixing ratio (kg/kg) at T (C) and p (hPa)
function saturationMixingRatio(t, p)
{
  const e = saturationPressure(t);
  return (EPSILON * e) / Math.max(p - e, 1e-6);
}

// The temperature (C) at which air of mixing ratio w (g/kg) is saturated at p (hPa)
const mixingRatioTemperature = (w, p) => dewpointOf(((w / 1000) * p) / (EPSILON + w / 1000));

// The temperature (C) of a dry adiabat of potential temperature theta (K) at p (hPa)
const dryAdiabat = (theta, p) => theta * Math.pow(p / 1000, KAPPA) - KELVIN;

// The pseudoadiabatic lapse rate dT/dp (K/hPa) of saturated air at T (C) and p (hPa)
function moistLapse(t, p)
{
  const tk = t + KELVIN;
  const rs = saturationMixingRatio(t, p);
  return ((RD * tk + LV * rs) / (CP + (LV * LV * rs * EPSILON) / (RD * tk * tk))) / p;
}

// A moist adiabat from (t0, p0) to p1: [[p, t]] in steps of at most 5 hPa
function moistAdiabat(t0, p0, p1)
{
  const out = [[p0, t0]];
  const n = Math.max(1, Math.ceil(Math.abs(p1 - p0) / 5));
  const dp = (p1 - p0) / n;
  let t = t0;
  let p = p0;
  for (let i = 0; i < n; i++)
  {
    // Second order Runge-Kutta
    const k1 = moistLapse(t, p);
    const k2 = moistLapse(t + k1 * dp, p + dp);
    t += ((k1 + k2) / 2) * dp;
    p += dp;
    out.push([p, t]);
  }
  return out;
}

// The lifting condensation level of air at (t, td, p): {p, t} (Bolton 1980)
function liftingCondensationLevel(t, td, p)
{
  const tk = t + KELVIN;
  const tdk = td + KELVIN;
  const tl = 1 / (1 / (tdk - 56) + Math.log(tk / tdk) / 800) + 56;
  return {p: p * Math.pow(tl / tk, 1 / KAPPA), t: tl - KELVIN};
}

// The path of a parcel lifted from (t, td, p) to ptop: dry to the LCL, then moist
function parcelPath(t, td, p, ptop)
{
  const lcl = liftingCondensationLevel(t, td, p);
  const theta = (t + KELVIN) * Math.pow(1000 / p, KAPPA);
  const dry = [];
  for (let q = p; q > lcl.p; q -= 10)
    dry.push([q, dryAdiabat(theta, q)]);
  dry.push([lcl.p, lcl.t]);
  return {lcl, path: [...dry, ...moistAdiabat(lcl.t, lcl.p, ptop).slice(1)]};
}

// The analysis of a sounding: levels [{p, t, td, speed, direction, height}] from the bottom up
// (hPa, C, m/s, deg, m). A parcel is lifted from the lowest level. Returns cape and cin (J/kg,
// virtual temperatures), lfc and el (hPa), li (lifted index at 500 hPa, C), freezing (the
// lowest 0 C level: {p, height}), k (K-index), tt (Total totals), pw (precipitable water, mm),
// shear (0-6 km bulk shear, m/s) and heights (m above the lowest level unless the data has
// heights); null when the sounding does not reach far enough.
function soundingAnalysis(levels)
{
  const out = {cape: null, cin: null, lfc: null, el: null, li: null, freezing: null, k: null,
               tt: null, pw: null, shear: null};
  const valid = levels.filter((l) => l.p > 0 && l.t !== null);
  if (valid.length < 3)
    return out;
  // The value of a field at a pressure, interpolated in the logarithm of the pressure
  const at = (p, field) => {
    for (let i = 0; i + 1 < valid.length; i++)
    {
      const a = valid[i];
      const b = valid[i + 1];
      if (p <= a.p && p >= b.p && a[field] !== null && b[field] !== null)
      {
        const f = Math.log(a.p / p) / Math.log(a.p / b.p || 1);
        return a[field] + f * (b[field] - a[field]);
      }
    }
    return null;
  };
  const mixing = (td, p) => (td === null ? 0 : saturationMixingRatio(td, p));
  const virtual = (t, w) => (t + KELVIN) * (1 + 0.61 * w);

  // Heights: those of the data, or by the hypsometric equation from the lowest level
  const heights = [0];
  for (let i = 1; i < valid.length; i++)
  {
    const a = valid[i - 1];
    const b = valid[i];
    const tv = (virtual(a.t, mixing(a.td, a.p)) + virtual(b.t, mixing(b.td, b.p))) / 2;
    heights.push(heights[i - 1] + ((RD * tv) / 9.80665) * Math.log(a.p / b.p));
  }
  valid.forEach((l, i) => {
    l.h = typeof l.height === 'number' ? l.height : heights[i];
  });
  out.dataHeights = valid.every((l) => typeof l.height === 'number');

  // The lowest 0 C level
  for (let i = 0; i + 1 < valid.length; i++)
    if (valid[i].t > 0 && valid[i + 1].t <= 0)
    {
      const a = valid[i];
      const b = valid[i + 1];
      const f = a.t / (a.t - b.t);
      out.freezing = {p: Math.exp(Math.log(a.p) + f * Math.log(b.p / a.p)), height: a.h + f * (b.h - a.h)};
      break;
    }
  if (valid[0].t <= 0)
    out.freezing = {p: valid[0].p, height: valid[0].h};

  // Indices of the standard levels
  const [t850, td850, t700, td700, t500] =
      [[850, 't'], [850, 'td'], [700, 't'], [700, 'td'], [500, 't']].map(([p, f]) => at(p, f));
  if ([t850, td850, t700, td700, t500].every((v) => v !== null))
    out.k = t850 - t500 + td850 - (t700 - td700);
  if ([t850, td850, t500].every((v) => v !== null))
    out.tt = t850 + td850 - 2 * t500;

  // Precipitable water: the specific humidity over the column, kg/m2 = mm
  let pw = 0;
  let humid = true;
  for (let i = 0; i + 1 < valid.length; i++)
  {
    const a = valid[i];
    const b = valid[i + 1];
    if (a.td === null || b.td === null)
    {
      humid = false;
      break;
    }
    const qa = mixing(a.td, a.p) / (1 + mixing(a.td, a.p));
    const qb = mixing(b.td, b.p) / (1 + mixing(b.td, b.p));
    pw += (((qa + qb) / 2) * (a.p - b.p) * 100) / 9.80665;
  }
  out.pw = humid ? pw : null;

  // The bulk shear between the lowest level and 6 km
  const wind = (l) => (l.speed === null || l.direction === null ? null : [
    -l.speed * Math.sin((l.direction * Math.PI) / 180), -l.speed * Math.cos((l.direction * Math.PI) / 180)
  ]);
  const top6 = valid.find((l) => l.h >= 6000);
  if (top6 && wind(valid[0]) && wind(top6))
  {
    const [u0, v0] = wind(valid[0]);
    const [u6, v6] = wind(top6);
    out.shear = Math.hypot(u6 - u0, v6 - v0);
  }

  // The parcel of the lowest level: CAPE above the level of free convection up to the
  // equilibrium level, CIN below it
  const base = valid[0];
  if (base.td === null)
    return out;
  const pTop = Math.max(100, valid[valid.length - 1].p);
  const parcel = parcelPath(base.t, base.td, base.p, pTop);
  const w0 = mixing(base.td, base.p);
  const buoyancy = parcel.path
                       .map(([p, tp]) => {
                         const te = at(p, 't');
                         if (te === null)
                           return null;
                         const wp = p > parcel.lcl.p ? w0 : saturationMixingRatio(tp, p);
                         return {p, tp, b: virtual(tp, wp) - virtual(te, mixing(at(p, 'td'), p))};
                       })
                       .filter(Boolean);
  if (buoyancy.length < 2)
    return out;
  // The level of free convection: where the parcel becomes buoyant at or above the LCL;
  // the equilibrium level: where it last stops being buoyant above that
  let lfc = null;
  let el = null;
  for (let i = 0; i + 1 < buoyancy.length; i++)
  {
    const a = buoyancy[i];
    const b = buoyancy[i + 1];
    if (lfc === null && b.p <= parcel.lcl.p && a.b <= 0 && b.b > 0)
      lfc = a.p + (a.b / (a.b - b.b)) * (b.p - a.p);
    if (lfc === null && b.p <= parcel.lcl.p && a.b > 0 && i === 0)
      lfc = a.p;
    if (lfc !== null && a.b > 0 && b.b <= 0)
      el = a.p + (a.b / (a.b - b.b)) * (b.p - a.p);
  }
  let cape = 0;
  let cin = 0;
  for (let i = 0; i + 1 < buoyancy.length; i++)
  {
    const a = buoyancy[i];
    const b = buoyancy[i + 1];
    const area = RD * ((a.b + b.b) / 2) * Math.log(a.p / b.p);
    const mid = (a.p + b.p) / 2;
    if (lfc !== null && mid <= lfc && (el === null || mid >= el))
      cape += Math.max(0, area);
    else if (lfc !== null && mid > lfc)
      cin += Math.min(0, area);
  }
  out.cape = lfc !== null ? cape : 0;
  out.cin = lfc !== null ? cin : null;
  out.lfc = lfc;
  out.el = el;
  const t500p = buoyancy.find((x) => x.p <= 500);
  if (t500 !== null && t500p)
    out.li = t500 - t500p.tp;
  return out;
}

// A hodograph: the wind vectors of the levels of a sounding from the bottom up to 10 km, the
// line coloured by height, rings every 10 m/s
function drawHodograph(levels)
{
  const size = 240;
  const c = size / 2;
  const winds = levels.filter((l) => l.speed !== null && l.direction !== null && (l.h ?? 0) <= 10000)
                    .map((l) => ({
                           u: -l.speed * Math.sin((l.direction * Math.PI) / 180),
                           v: -l.speed * Math.cos((l.direction * Math.PI) / 180),
                           h: l.h ?? 0
                         }));
  if (winds.length < 2)
    return null;
  const max = Math.max(10, Math.ceil(Math.max(...winds.map((w) => Math.hypot(w.u, w.v))) / 10) * 10);
  const scale = (c - 14) / max;
  const root = svg('svg', {class: 'hodograph', width: size, height: size, viewBox: `0 0 ${size} ${size}`,
                           role: 'img', 'aria-label': L('Hodograph')});
  for (let r = 10; r <= max; r += 10)
  {
    root.append(svg('circle', {class: 'hodo-ring', cx: c, cy: c, r: r * scale}));
    const label = svg('text', {class: 'vtick', x: c + r * scale * 0.7071 + 2, y: c - r * scale * 0.7071 - 2});
    label.textContent = `${r}`;
    root.append(label);
  }
  root.append(svg('line', {class: 'hodo-axis', x1: 0, y1: c, x2: size, y2: c}),
              svg('line', {class: 'hodo-axis', x1: c, y1: 0, x2: c, y2: size}));
  // The height bands of the line, km
  const bands = [[0, 1, '#c92a2a'], [1, 3, '#e67700'], [3, 6, '#2b8a3e'], [6, 10, '#1864ab']];
  for (let i = 0; i + 1 < winds.length; i++)
  {
    const a = winds[i];
    const b = winds[i + 1];
    const band = bands.find(([lo, hi]) => a.h / 1000 >= lo && a.h / 1000 < hi) || bands[bands.length - 1];
    root.append(svg('line', {
      class: 'hodo-line',
      x1: c + a.u * scale,
      y1: c - a.v * scale,
      x2: c + b.u * scale,
      y2: c - b.v * scale,
      stroke: band[2]
    }));
  }
  const legend = el('div', {class: 'vlegend small'},
                    bands.map(([lo, hi, colour]) => el('span', null, el('i', {style: `background:${colour}`}),
                                                       L`${lo}-${hi} km`)));
  return el('div', {class: 'hodograph-box'}, el('div', {class: 'small muted'}, L('Hodograph (m/s)')), root, legend);
}

// ----------------------------------------------------------------------
// The skew-T log-p diagram
// ----------------------------------------------------------------------

// Wind barb of speed s (m/s) from direction d (deg) at (x, y): a staff towards the direction
// the wind comes from, pennants of 50 kt, barbs of 10 kt and half barbs of 5 kt
// The height of the strip of the wind barbs below a wind chart: a staff and its feathers
// fit in it whatever the direction
const BARB_STRIP = 58;

function windBarb(x, y, speed, direction)
{
  const g = svg('g', {class: 'barb', transform: `translate(${x},${y}) rotate(${direction})`});
  const kt = Math.round((speed * 1.943844) / 5) * 5;
  if (kt < 5)
  {
    g.append(svg('circle', {cx: 0, cy: 0, r: 3}));
    return g;
  }
  const len = 26;
  g.append(svg('line', {x1: 0, y1: 0, x2: 0, y2: -len}));
  let rest = kt;
  let pos = -len;
  while (rest >= 50)
  {
    g.append(svg('path', {d: `M0,${pos} L10,${pos + 3} L0,${pos + 6} Z`, class: 'pennant'}));
    pos += 8;
    rest -= 50;
  }
  while (rest >= 10)
  {
    g.append(svg('line', {x1: 0, y1: pos, x2: 11, y2: pos - 4}));
    pos += 4;
    rest -= 10;
  }
  if (rest >= 5)
  {
    if (pos === -len)
      pos += 4;
    g.append(svg('line', {x1: 0, y1: pos, x2: 6, y2: pos - 2}));
  }
  return g;
}

// The sounding of a profile: [{p, t, td, speed, direction}] from the bottom up
function soundingOf(profile, columns, axis)
{
  const out = [];
  for (const {z, row} of profile.rows)
  {
    const p = axis.value(row);
    const t = columns.temperature ? columnValue(row, columns.temperature[0]) : null;
    if (p === null || t === null)
      continue;
    const tc = t + columns.temperature[1];
    let td = columns.dewpoint ? columnValue(row, columns.dewpoint[0]) : null;
    if (td !== null)
      td += columns.dewpoint[1];
    else if (columns.humidity)
    {
      const rh = columnValue(row, columns.humidity[0]);
      td = rh === null ? null : dewpointFromHumidity(tc, rh);
    }
    let speed = columns.speed ? columnValue(row, columns.speed[0]) : null;
    let direction = columns.direction ? columnValue(row, columns.direction[0]) : null;
    if ((speed === null || direction === null) && columns.u && columns.v)
    {
      const u = columnValue(row, columns.u[0]);
      const v = columnValue(row, columns.v[0]);
      if (u !== null && v !== null)
      {
        speed = Math.hypot(u, v);
        direction = ((Math.atan2(-u, -v) * 180) / Math.PI + 360) % 360;
      }
    }
    // The height of the level when the data has it
    let height = null;
    for (const name of HEIGHT_PARAMS)
    {
      height = columnValue(row, name);
      if (height !== null)
        break;
    }
    out.push({p, t: tc, td, speed, direction, z, height});
  }
  return out.sort((a, b) => b.p - a.p);
}

// The columns of a sounding the rows have: {temperature: [name, offset], ...}. A temperature
// named in Kelvins may have been converted to Celsius by the server (the grid parameter T-K
// is), so the values decide whether the offset applies.
function soundingColumns(rows)
{
  const out = {};
  for (const [what, candidates] of Object.entries(SOUNDING_PARAMS))
    for (const [name, offset] of candidates)
    {
      const values = rows.map((r) => columnValue(r, name)).filter((v) => v !== null);
      if (values.length === 0)
        continue;
      const kelvins = offset !== 0 && quantile(values.slice().sort((a, b) => a - b), 0.5) > 100;
      out[what] = [name, kelvins ? offset : 0];
      break;
    }
  return out;
}

// Whether the rows make a skew-T: temperature on pressures
function hasSounding(rows, levelKind)
{
  const columns = soundingColumns(rows);
  return !!columns.temperature && verticalAxes(rows, levelKind).some((a) => a.id === 'pressure');
}

// The thermodynamic diagrams of soundings. Each maps a temperature (C) and a pressure (hPa) to
// coordinates [u, v], v growing upwards:
// - skew-T log-p: v = -ln p, u = T leaning 45 degrees to the right with height
// - tephigram: the temperature and the logarithm of the potential temperature turned 45
//   degrees, isotherms rising to the right and dry adiabats to the left, isobars nearly level
// - Stuve: v = -p^(R/cp), u = T, so that the dry adiabats are straight lines
const THERMO_DIAGRAMS = [['skewt', L('Skew-T log-p')], ['tephigram', L('Tephigram')], ['stuve', L('Stüve')]];

// The projection of a thermodynamic diagram: (T C, p hPa) -> [u, v] before scaling, v growing
// upwards
function thermoProjection(type)
{
  if (type === 'tephigram')
    return (t, p) => {
      const s = 280 * Math.log((t + KELVIN) * Math.pow(1000 / p, KAPPA));
      return [t + s, -t + s];
    };
  if (type === 'stuve')
    return (t, p) => [t, -Math.pow(p, KAPPA) * 10];
  // The skew of the isotherms is set by the scales of the view, see thermoView
  return (t, p) => [t, -Math.log(p)];
}

// The view of a diagram in pixels: {x(t, p), y(t, p)} for a plot of w x h pixels. The window
// covers the pressures pBottom...pTop and the temperatures tmin...tmax at them.
function thermoView(type, w, h, pBottom, pTop, tmin, tmax, tSurface, points)
{
  if (type === 'skewt')
  {
    // 80 degrees at the bottom covering the temperatures near the surface, the isotherms at
    // 45 degrees: the cold upper air is to the left of the surface only by the skew
    const tLeft = Math.min(-40, Math.floor((tSurface - 30) / 10) * 10);
    const tRight = tLeft + 80;
    const y = (p) =>
        h - (h * (Math.log(pBottom) - Math.log(p))) / (Math.log(pBottom) - Math.log(pTop));
    return {x: (t, p) => ((t - tLeft) / (tRight - tLeft)) * w + (h - y(p)), y: (t, p) => y(p)};
  }
  // The box of the window, the same scale in both directions so that angles are kept. The
  // tephigram is turned, so its window is fitted to the data rather than to the corners.
  const proj = thermoProjection(type);
  const source = type === 'tephigram' && points && points.length ?
      [...points, [tSurface - 10, pBottom], [tSurface + 15, pBottom]] :
      [[tmin, pBottom], [tmax, pBottom], [tmin, pTop], [tmax, pTop]];
  const corners = source.map(([t, p]) => proj(t, p));
  const umin = Math.min(...corners.map((c) => c[0]));
  const umax = Math.max(...corners.map((c) => c[0]));
  const vmin = Math.min(...corners.map((c) => c[1]));
  const vmax = Math.max(...corners.map((c) => c[1]));
  if (type === 'stuve')
  {
    // Temperature and pressure scales are independent
    return {
      x: (t, p) => ((proj(t, p)[0] - umin) / (umax - umin)) * w,
      y: (t, p) => h - ((proj(t, p)[1] - vmin) / (vmax - vmin)) * h
    };
  }
  const scale = 0.92 * Math.min(w / (umax - umin), h / (vmax - vmin));
  const u0 = (umin + umax) / 2;
  const v0 = (vmin + vmax) / 2;
  return {
    x: (t, p) => w / 2 + (proj(t, p)[0] - u0) * scale,
    y: (t, p) => h / 2 - (proj(t, p)[1] - v0) * scale
  };
}

// Bisection: the value in [a, b] at which the increasing function f reaches the target
function solveIncreasing(f, target, a, b)
{
  let lo = a;
  let hi = b;
  if (f(lo) > target || f(hi) < target)
    return null;
  for (let i = 0; i < 50; i++)
  {
    const mid = (lo + hi) / 2;
    if (f(mid) < target)
      lo = mid;
    else
      hi = mid;
  }
  return (lo + hi) / 2;
}

// The sounding diagram of the profiles: temperature (red), dewpoint (green), the path of a
// parcel lifted from the lowest level (dashed), wind barbs, and the soundings of another
// producer for comparison (thin)
function drawSkewT(container, rows, levelKind, options)
{
  container.replaceChildren();
  const opts = options || {};
  const axis = verticalAxes(rows, levelKind).find((a) => a.id === 'pressure');
  const columns = soundingColumns(rows);
  if (!axis || !columns.temperature)
  {
    container.append(el('p', {class: 'muted'}, L('A sounding needs temperatures on pressures.')));
    return {redraw: () => {}};
  }
  const profiles = collectProfiles(rows, axis);
  const keys = Array.from(new Set(profiles.map((p) => p.key)));
  const times = Array.from(new Set(profiles.map((p) => p.time))).sort((a, b) => a - b);
  // Side by side the diagrams share the scales of all the soundings, so that they compare
  const levels = profiles.flatMap((p) => soundingOf(p, columns, axis));
  const temps = levels.flatMap((l) => [l.t, l.td]).filter((v) => v !== null);
  const range = levels.length === 0 ? null : {
    pBottom: Math.max(1050, ...levels.map((l) => l.p)),
    pTop: Math.max(100, Math.min(...levels.map((l) => l.p))),
    tmin: Math.min(-40, ...temps) - 5,
    tmax: Math.max(30, ...temps) + 5
  };
  const panels = [];
  const panelsBox = el('div', {class: 'sk-panels'});
  const add = el('button', {type: 'button', class: 'small'}, L('Add a panel'));
  const head = el('div', {class: 'row'}, add);
  if (opts.controls)
    head.append(...opts.controls);
  container.append(head, panelsBox);
  // The width of each diagram: the panels side by side as far as they fit
  const width = () => {
    const total = container.clientWidth || 560;
    const n = panels.length;
    const fit = n > 1 ? Math.floor((total - 16 * (n - 1)) / n) : total;
    return Math.max(340, Math.min(640, fit));
  };
  const redraw = () => {
    // Several panels share the row equally, their hodographs below the diagrams
    panelsBox.classList.toggle('several', panels.length > 1);
    panels.forEach((p) => p.redraw());
  };
  // A panel of its own location and time, by default the next location or else the next time
  const addPanel = () => {
    const last = panels[panels.length - 1];
    let key = keys[0];
    let time = 0;
    if (last)
    {
      const used = new Set(panels.map((p) => p.key()));
      const free = keys.find((k) => !used.has(k));
      key = free !== undefined ? free : last.key();
      time = free !== undefined ? last.timeIndex() : Math.min(times.length - 1, last.timeIndex() + 1);
    }
    const box = el('div', {class: 'sk-panel'});
    panelsBox.append(box);
    const panel = skewTPanel(box, profiles, keys, times, columns, axis, Object.assign({}, opts, {
      key,
      time,
      width,
      range: () => (panels.length > 1 ? range : null),
      onRemove: panels.length === 0 ? null : () => {
        panels.splice(panels.indexOf(panel), 1);
        box.remove();
        redraw();
      }
    }));
    panels.push(panel);
  };
  add.addEventListener('click', () => {
    addPanel();
    redraw();
  });
  addPanel();
  return {redraw};
}

// A thermodynamic diagram of the soundings of a location: the diagram, the location, the
// times, the lifted parcel, the analysis, the hodograph and the values of the levels.
// options: key and time (index) to start from, width() of the diagram, range() of shared
// scales or null, onRemove of a panel that can be removed, type and onType of the diagram,
// compare (soundings of other data).
function skewTPanel(container, profiles, keys, times, columns, axis, options)
{
  const opts = options;
  const keySelect =
      el('select',
         {'aria-label': L('Location of the sounding')},
         keys.map((k) => el('option', {value: k}, k)));
  keySelect.value = opts.key;
  const typeSelect =
      el('select',
         {'aria-label': L('Diagram')},
         THERMO_DIAGRAMS.map(([v, label]) => el('option', {value: v}, label)));
  typeSelect.value = opts.type || 'skewt';
  const chooser = timeChooser(times, () => draw(), opts.time);
  const parcelBox = el('input', {type: 'checkbox', checked: true, 'aria-label': L('Lifted parcel')});
  const info = el('p', {class: 'muted small skewt-info'});
  const diagram = el('div', {class: 'skewt'});
  let tableOpen = false;
  keySelect.hidden = keys.length < 2;
  keySelect.addEventListener('change', () => draw());
  parcelBox.addEventListener('change', () => draw());
  typeSelect.addEventListener('change', () => {
    if (opts.onType)
      opts.onType(typeSelect.value);
    draw();
  });
  const remove = opts.onRemove ?
      el('button', {type: 'button', class: 'small', 'aria-label': L('Remove the panel'), onclick: opts.onRemove}, '×') :
      '';
  container.append(
      el('div',
         {class: 'row'},
         typeSelect,
         keySelect,
         chooser.element,
         el('label', {class: 'check'}, parcelBox, L('Lifted parcel')),
         remove),
      diagram,
      info);

  // Draws the chosen diagram of the soundings at the chosen times and location
  function draw()
  {
    diagram.replaceChildren();
    const type = typeSelect.value;
    const chosen = new Set(chooser.value());
    const key = keySelect.value || keys[0];
    const soundings = profiles.filter((p) => p.key === key && chosen.has(p.time))
                          .sort((a, b) => a.time - b.time)
                          .map((p) => ({time: p.time, levels: soundingOf(p, columns, axis)}))
                          .filter((s) => s.levels.length >= 2);
    if (soundings.length === 0)
    {
      diagram.append(el('p', {class: 'muted'}, L('No sounding at the chosen time.')));
      return;
    }
    // The diagrams reach up to 100 hPa
    if (!soundings.some((s) => s.levels.filter((l) => l.p >= 100).length >= 2))
    {
      diagram.append(
          el('p', {class: 'muted'}, L('The sounding has no pressures below 100 hPa to draw.')));
      return;
    }
    // Soundings to compare with: the nearest time of the other data
    const others = (opts.compare && opts.compare.soundings) ?
        opts.compare.soundings(soundings.map((s) => s.time)) :
        [];
    const width = opts.width();
    const height = 560;
    const m = {left: 48, right: 56, top: 10, bottom: 36};
    const w = width - m.left - m.right;
    const h = height - m.top - m.bottom;
    const all = [...soundings, ...others].flatMap((s) => s.levels);
    const temps = all.flatMap((l) => [l.t, l.td]).filter((v) => v !== null);
    const shared = opts.range();
    const pBottom = Math.max(1050, ...all.map((l) => l.p), shared ? shared.pBottom : 0);
    const pTop = Math.max(100, Math.min(...all.map((l) => l.p), shared ? shared.pTop : Infinity));
    const tmin = Math.min(-40, ...temps, shared ? shared.tmin + 5 : Infinity) - 5;
    const tmax = Math.max(30, ...temps, shared ? shared.tmax - 5 : -Infinity) + 5;
    const near =
        all.filter((l) => l.p >= 850).flatMap((l) => [l.t, l.td]).filter((v) => v !== null);
    const tSurface = near.length ? Math.min(...near) : 0;
    const view = thermoView(
        type,
        w,
        h,
        pBottom,
        pTop,
        tmin,
        tmax,
        tSurface,
        // The levels above the top of the diagram are not drawn, nor fitted to
        all.filter((l) => l.p >= pTop)
            .flatMap((l) => (l.td === null ? [[l.t, l.p]] : [[l.t, l.p], [l.td, l.p]])));
    const x = view.x;
    const y = view.y;
    // A path through [p, t] points of the diagram
    const line = (points, cls, attrs) => {
      let d = '';
      points.forEach(([p, t], i) => {
        d += `${i ? 'L' : 'M'}${x(t, p).toFixed(1)},${y(t, p).toFixed(1)}`;
      });
      return svg('path', Object.assign({class: cls, d}, attrs || {}));
    };
    const root = svg('svg', {
      class: 'skewt-diagram',
      width,
      height,
      viewBox: `0 0 ${width} ${height}`,
      role: 'img',
      'aria-label': THERMO_DIAGRAMS.find((d) => d[0] === type)[1]
    });
    const clipId = `skewt-clip-${Math.random().toString(36).slice(2)}`;
    const defs = svg('defs');
    const clip = svg('clipPath', {id: clipId});
    clip.append(svg('rect', {x: 0, y: 0, width: w, height: h}));
    defs.append(clip);
    root.append(defs);
    const g = svg('g', {transform: `translate(${m.left},${m.top})`});
    root.append(g);
    const plot = svg('g', {'clip-path': `url(#${clipId})`});
    g.append(plot);
    // The background reaches beyond the window, the clip cuts it
    const pressures = [];
    for (let p = pBottom * 1.1; p >= pTop * 0.7; p -= 10)
      pressures.push(p);
    const isotherm = [];
    for (let t = -160; t <= 60; t += 5)
      isotherm.push(t);
    for (const p of PRESSURE_TICKS.filter((q) => q <= pBottom && q >= pTop))
    {
      plot.append(line(isotherm.map((t) => [p, t]), 'sk-isobar'));
      // The label where the isobar meets the left edge
      const t = solveIncreasing((tt) => x(tt, p), 0, -200, 100);
      if (t !== null && y(t, p) >= 0 && y(t, p) <= h)
      {
        const label = svg('text', {class: 'vtick', x: -6, y: y(t, p) + 4, 'text-anchor': 'end'});
        label.textContent = String(p);
        g.append(label);
      }
    }
    for (let t = -120; t <= 50; t += 10)
    {
      plot.append(line(pressures.map((p) => [p, t]), t === 0 ? 'sk-isotherm zero' : 'sk-isotherm'));
      // The label where the isotherm meets the bottom edge
      const p = solveIncreasing((pp) => y(t, pp), h, pTop * 0.3, pBottom * 1.5);
      if (p !== null && x(t, p) >= 0 && x(t, p) <= w)
      {
        const label = svg('text', {class: 'vtick', x: x(t, p), y: h + 16, 'text-anchor': 'middle'});
        label.textContent = String(t);
        g.append(label);
      }
    }
    for (let theta = 230; theta <= 500; theta += 10)
      plot.append(line(pressures.map((p) => [p, dryAdiabat(theta, p)]), 'sk-dry'));
    for (let t0 = -24; t0 <= 36; t0 += 4)
      plot.append(
          line(moistAdiabat(t0, 1000, pTop * 0.7).filter(([p]) => p <= pBottom * 1.1), 'sk-moist'));
    for (const wgk of [0.4, 1, 2, 4, 7, 10, 16, 24, 32])
    {
      const ps = pressures.filter((p) => p >= 600);
      plot.append(line(ps.map((p) => [p, mixingRatioTemperature(wgk, p)]), 'sk-mixing'));
      const lp = ps[ps.length - 1];
      const label = svg('text', {
        class: 'sk-mixing-label',
        x: x(mixingRatioTemperature(wgk, lp), lp),
        y: y(mixingRatioTemperature(wgk, lp), lp) - 3,
        'text-anchor': 'middle'
      });
      label.textContent = String(wgk);
      plot.append(label);
    }
    g.append(svg('rect', {class: 'vframe', x: 0, y: 0, width: w, height: h}));
    const ylabel = svg('text', {
      class: 'vlabel',
      transform: `translate(${- 38},${h / 2}) rotate(-90)`,
      'text-anchor': 'middle'
    });
    ylabel.textContent = L('Pressure (hPa)');
    g.append(ylabel);
    const xlabel = svg('text', {class: 'vlabel', x: w / 2, y: h + 32, 'text-anchor': 'middle'});
    xlabel.textContent = L('Temperature (C)');
    g.append(xlabel);

    // The soundings to compare with, thin
    for (const s of others)
    {
      const levels = s.levels.filter((l) => l.p >= pTop && l.p <= pBottom);
      plot.append(line(levels.map((l) => [l.p, l.t]), 'sk-temperature compare'));
      const dew = levels.filter((l) => l.td !== null);
      if (dew.length > 1)
        plot.append(line(dew.map((l) => [l.p, l.td]), 'sk-dewpoint compare'));
    }
    // The soundings, earlier times fainter
    const infos = [];
    soundings.forEach((s, i) => {
      const alpha = soundings.length > 1 ? 0.3 + (0.7 * i) / (soundings.length - 1) : 1;
      const levels = s.levels.filter((l) => l.p >= pTop && l.p <= pBottom);
      if (levels.length === 0)
        return;
      plot.append(line(levels.map((l) => [l.p, l.t]), 'sk-temperature', {opacity: alpha}));
      const dew = levels.filter((l) => l.td !== null);
      if (dew.length > 1)
        plot.append(line(dew.map((l) => [l.p, l.td]), 'sk-dewpoint', {opacity: alpha}));
      // A parcel lifted from the lowest level of the latest sounding
      if (parcelBox.checked && i === soundings.length - 1 && levels[0].td !== null)
      {
        const base = levels[0];
        const parcel = parcelPath(base.t, base.td, base.p, pTop);
        plot.append(line(parcel.path, 'sk-parcel'));
        const lx = x(parcel.lcl.t, parcel.lcl.p);
        const ly = y(parcel.lcl.t, parcel.lcl.p);
        plot.append(svg('line', {class: 'sk-lcl', x1: lx - 8, x2: lx + 8, y1: ly, y2: ly}));
        infos.push(L`Lifting condensation level ${Math.round(parcel.lcl.p)} hPa, ${
            parcel.lcl.t.toFixed(1)} C, of a parcel lifted from ${Math.round(base.p)} hPa`);
      }
    });
    // Wind barbs of the latest sounding at the right edge, at least 16 pixels apart
    const latest = soundings[soundings.length - 1].levels;
    let previous = Infinity;
    for (const l of latest)
    {
      const ly = y(l.t, l.p);
      if (l.speed === null || l.direction === null || ly < 0 || ly > h || previous - ly < 16)
        continue;
      g.append(windBarb(w + 28, ly, l.speed, l.direction));
      previous = ly;
    }
    infos.push(`${key}, ${soundings.map((s) => formatTime(new Date(s.time))).join(', ')}`);
    if (others.length > 0 && opts.compare.label)
      infos.push(L`thin lines: ${opts.compare.label(others)}`);
    info.textContent = infos.join(' · ');

    // The pointer: a guide along its isobar, the temperature, pressure and potential
    // temperature under it, and the values of the nearest level of the latest sounding
    const guide = svg('path', {class: 'sk-guide', visibility: 'hidden'});
    const tMark = svg('circle', {class: 'sk-mark temperature', r: 4, visibility: 'hidden'});
    const dMark = svg('circle', {class: 'sk-mark dewpoint', r: 4, visibility: 'hidden'});
    const hover = svg('rect', {class: 'vhover', x: 0, y: 0, width: w, height: h});
    plot.append(guide, tMark, dMark);
    g.append(hover);
    const readout = el('div', {class: 'sk-readout small'});
    // Shows or hides the guide and the marks of the pointer
    const setVisible = (on) => {
      for (const n of [guide, tMark, dMark])
        n.setAttribute('visibility', on ? 'visible' : 'hidden');
    };
    hover.addEventListener('mousemove', (e) => {
      const r = hover.getBoundingClientRect();
      const mx = ((e.clientX - r.left) / r.width) * w;
      const my = ((e.clientY - r.top) / r.height) * h;
      // The pressure whose isotherm-crossing at the pointer's x is at the pointer's y
      const tAt = (p) => solveIncreasing((tt) => x(tt, p), mx, -250, 150);
      const p = solveIncreasing((pp) => {
        const tt = tAt(pp);
        return tt === null ? NaN : y(tt, pp);
      }, my, pTop * 0.5, pBottom * 1.3);
      const t = p === null ? null : tAt(p);
      if (p === null || t === null)
      {
        setVisible(false);
        readout.textContent = '';
        return;
      }
      guide.setAttribute(
          'd',
          isotherm.map((tt, i) => `${i ? 'L' : 'M'}${x(tt, p).toFixed(1)},${y(tt, p).toFixed(1)}`)
              .join(''));
      const theta = (t + KELVIN) * Math.pow(1000 / p, KAPPA);
      const parts = [`${Math.round(p)} hPa`, `${t.toFixed(1)} C`, `θ ${theta.toFixed(1)} K`];
      // The nearest level of the latest sounding
      let near = null;
      for (const l of latest)
        if (!near || Math.abs(Math.log(l.p / p)) < Math.abs(Math.log(near.p / p)))
          near = l;
      highlight(near && Math.abs(Math.log(near.p / p)) < 0.05 ? near : null);
      if (near && Math.abs(Math.log(near.p / p)) < 0.05)
      {
        tMark.setAttribute('cx', x(near.t, near.p));
        tMark.setAttribute('cy', y(near.t, near.p));
        const level = [`level ${Math.round(near.p)} hPa: T ${near.t.toFixed(1)} C`];
        if (near.td !== null)
        {
          dMark.setAttribute('cx', x(near.td, near.p));
          dMark.setAttribute('cy', y(near.td, near.p));
          level.push(`Td ${near.td.toFixed(1)} C`);
        }
        if (near.speed !== null && near.direction !== null)
          level.push(`wind ${Math.round(near.direction)}° ${unitOf('speed')[2](near.speed).toFixed(1)} ${unitOf('speed')[1]}`);
        parts.push(level.join(', '));
        setVisible(true);
        dMark.setAttribute('visibility', near.td !== null ? 'visible' : 'hidden');
      }
      else
      {
        setVisible(true);
        tMark.setAttribute('visibility', 'hidden');
        dMark.setAttribute('visibility', 'hidden');
      }
      readout.textContent = parts.join(' · ');
    });
    hover.addEventListener('mouseleave', () => {
      setVisible(false);
      highlight(null);
      readout.textContent = '';
    });
    // The analysis of the latest sounding, and its hodograph
    // The copies of the levels get their heights from the analysis for the hodograph
    const levelsWithHeights = latest.map((l) => Object.assign({}, l));
    const analysis = soundingAnalysis(levelsWithHeights);
    const fmt = (v, digits, unit) => (v === null || v === undefined ? '–' : `${v.toFixed(digits)}${unit}`);
    const items = [
      ['CAPE', fmt(analysis.cape, 0, ' J/kg')],
      ['CIN', fmt(analysis.cin, 0, ' J/kg')],
      ['LFC', fmt(analysis.lfc, 0, ' hPa')],
      ['EL', fmt(analysis.el, 0, ' hPa')],
      [L('Lifted index'), fmt(analysis.li, 1, '')],
      [L('Freezing level'), analysis.freezing ?
           `${Math.round(unitOf('height')[2](analysis.freezing.height))} ${unitOf('height')[1]}${
               analysis.dataHeights ? '' : '*'} (${Math.round(analysis.freezing.p)} hPa)` :
           '–'],
      [L('K-index'), fmt(analysis.k, 0, '')],
      [L('Total totals'), fmt(analysis.tt, 0, '')],
      [L('Precipitable water'), fmt(analysis.pw, 1, ' mm')],
      [L('Shear 0-6 km'), fmt(analysis.shear, 0, ' m/s')]
    ];
    const analysisBox = el('div', {class: 'sk-analysis small'},
                           el('dl', null, items.map(([k, v]) => el('div', null, el('dt', null, k), el('dd', null, v)))),
                           analysis.dataHeights ? '' :
                                                  el('p', {class: 'muted'},
                                                     L('* Height above the lowest level, from the hypsometric equation')));
    const hodograph = drawHodograph(levelsWithHeights.filter((l) => l.h !== undefined));

    // The values of the levels of the latest sounding; a row pointed marks its level
    const [, heightUnit, heightOf] = unitOf('height');
    const [, speedUnit, speedOf] = unitOf('speed');
    const cell = (v, digits) => el('td', null, v === null || v === undefined || !isFinite(v) ? '' : v.toFixed(digits));
    const tableRows = new Map();
    const body = el('tbody');
    levelsWithHeights.forEach((l, i) => {
      const rh = l.td === null ? null : (100 * saturationPressure(l.td)) / saturationPressure(l.t);
      const theta = (l.t + KELVIN) * Math.pow(1000 / l.p, KAPPA);
      const tr = el('tr', null,
                    cell(l.p, 0),
                    cell(l.h === undefined ? null : heightOf(l.h), 0),
                    cell(l.t, 1),
                    cell(l.td, 1),
                    cell(rh, 0),
                    cell(l.direction, 0),
                    cell(l.speed === null ? null : speedOf(l.speed), 1),
                    cell(theta, 1));
      tr.addEventListener('mouseenter', () => {
        tMark.setAttribute('cx', x(l.t, l.p));
        tMark.setAttribute('cy', y(l.t, l.p));
        tMark.setAttribute('visibility', 'visible');
        if (l.td !== null)
        {
          dMark.setAttribute('cx', x(l.td, l.p));
          dMark.setAttribute('cy', y(l.td, l.p));
          dMark.setAttribute('visibility', 'visible');
        }
      });
      tr.addEventListener('mouseleave', () => setVisible(false));
      // By the level of the sounding, as the pointer on the diagram finds them
      tableRows.set(latest[i], tr);
      body.append(tr);
    });
    const head = el('tr', null, ['hPa', `Height (${heightUnit})`, 'T (C)', L('Td (C)'), 'RH (%)', L('Wind (°)'), `Wind (${speedUnit})`, 'θ (K)']
                                    .map((t) => el('th', null, t)));
    const table = el('details', {class: 'sk-values', open: tableOpen},
                     el('summary', null, L`Values of the ${levelsWithHeights.length} levels`),
                     el('div', {class: 'table-wrap'}, el('table', {class: 'data'}, el('thead', null, head), body)));
    table.addEventListener('toggle', () => {
      tableOpen = table.open;
    });
    let active = null;
    // Marks the row of a level pointed on the diagram
    function highlight(level)
    {
      const tr = level ? tableRows.get(level) : null;
      if (tr === active)
        return;
      if (active)
        active.classList.remove('active');
      active = tr;
      if (active)
        active.classList.add('active');
    }
    diagram.append(readout, el('div', {class: 'sk-row'}, root, hodograph || ''), analysisBox, table);
  }
  draw();
  return {
    redraw: draw,
    key: () => keySelect.value || keys[0],
    timeIndex: () => chooser.index()
  };
}

// ------------------------------------------------------------------
// Meteograms
// ------------------------------------------------------------------

// The parameters of a meteogram by their names in querydata, grid data and observations
const METEOGRAM_PARAMS = {
  temperature: ['temperature', 't2m', 'ta_pt1h_avg', 't-k', 't-c'],
  dewpoint: ['dewpoint', 'dewpoint2m', 'td', 'td-k', 'td-c'],
  precipitation: ['precipitation1h', 'precipitationamount', 'r_1h', 'pr_pt1h_acc', 'rr-1-mm', 'rr-kgm2'],
  cloud: ['totalcloudcover', 'n_man', 'cloudiness', 'n-0to1', 'n-pct'],
  speed: ['windspeedms', 'ws_10min', 'ff-ms'],
  direction: ['winddirection', 'wd_10min', 'dd-d'],
  gust: ['windgust', 'hourlymaximumgust', 'wg_10min', 'ffg-ms'],
  u: ['windums', 'u-ms'],
  v: ['windvms', 'v-ms']
};

// The columns of a meteogram the rows have: {group: name}
function meteogramColumns(rows)
{
  const out = {};
  for (const [group, names] of Object.entries(METEOGRAM_PARAMS))
  {
    const name = names.find((n) => rows.some((r) => columnValue(r, n) !== null));
    if (name)
      out[group] = name;
  }
  return out;
}

// Whether the rows make a meteogram: a temperature and precipitation, cloud cover or wind at
// no more than a few places
function hasMeteogram(rows)
{
  const c = meteogramColumns(rows);
  const places = new Set(rows.map((r) => locationOf(Object.assign({}, r, {level: null})))).size;
  return !!c.temperature && !!(c.precipitation || c.cloud || c.speed || (c.u && c.v)) && places <= 20;
}

// The parameters of a meteogram a producer or collection has and the request lacks
function meteogramParameters(parameterSet, chosen)
{
  const out = [];
  const find = (group) => METEOGRAM_PARAMS[group].find((n) => parameterSet.has(n));
  // The wind by its speed and direction, or by its components
  const wind = find('speed') && find('direction') ? ['speed', 'direction'] : ['u', 'v'];
  for (const group of ['temperature', 'dewpoint', 'precipitation', 'cloud', ...wind, 'gust'])
  {
    const name = find(group);
    if (name && !chosen.includes(name))
      out.push(name);
  }
  return out;
}

// A button for a meteogram: shown for a producer or collection that has one, it adds the
// parameters of a meteogram to the chosen ones and calls start(). Returns {element,
// update(parameterSet, chosen)}.
function meteogramButton(start)
{
  const button = el('button', {type: 'button', class: 'small', 'data-meteogram': '1'}, L('Meteogram'));
  const note = el('span', {class: 'small meteogram-note'});
  const element = el('div', {class: 'row meteogram-button'}, button, note);
  let missing = [];
  button.addEventListener('click', () => start(missing));
  const update = (parameterSet, chosen) => {
    element.hidden = !parameterSet || !meteogramCapable(parameterSet);
    if (element.hidden)
      return;
    missing = meteogramParameters(parameterSet, chosen);
    note.textContent = missing.length ? L`Adds ${missing.join(', ')} and runs the request` :
                                        L('Runs the request and draws a meteogram');
  };
  return {element, update};
}

// Whether a producer or collection has a meteogram: a temperature and something else of one
function meteogramCapable(parameterSet)
{
  const has = (group) => METEOGRAM_PARAMS[group].some((n) => parameterSet.has(n));
  return has('temperature') && (has('precipitation') || has('cloud') || has('speed') || (has('u') && has('v')));
}

// A meteogram of a place: temperature and dewpoint, precipitation, cloud cover, and wind with
// gusts and barbs, panels on one time axis with one cursor. An ensemble (several members or
// levels at a time) is drawn as the median and the 10-90 % range of the members; other levels
// are drawn at the level nearest to the ground.
function drawMeteogram(container, rows, levelKind)
{
  container.replaceChildren();
  const root = typeof window !== 'undefined' ? window : globalThis;
  const placeOf = (r) => locationOf(Object.assign({}, r, {level: null}));
  const places = Array.from(new Set(rows.map(placeOf)));
  const placeSelect = el('select', {'aria-label': L('Place of the meteogram')},
                         places.map((p) => el('option', {value: p}, p)));
  placeSelect.hidden = places.length < 2;
  const panels = el('div', {class: 'meteogram'});
  container.append(el('div', {class: 'row'}, placeSelect), panels);
  placeSelect.addEventListener('change', draw);
  draw();

  function draw()
  {
    panels.replaceChildren();
    let chosen = rows.filter((r) => placeOf(r) === (placeSelect.value || places[0]));
    // Levels other than members: the one nearest to the ground
    const levels = Array.from(new Set(chosen.map((r) => r.level).filter((v) => typeof v === 'number')));
    if (levels.length > 1 && levelKind !== 'member')
    {
      const down = ['hybrid', 'pressure', 'pressure-pa', 'sounding'].includes(levelKind);
      const lowest = down ? Math.max(...levels) : Math.min(...levels);
      chosen = chosen.filter((r) => r.level === lowest);
    }
    const columns = meteogramColumns(chosen);
    // The values of each group at each time, several for an ensemble
    const times = new Map();
    for (const r of chosen)
    {
      const d = parseTime(r.time);
      if (!d)
        continue;
      const x = Math.round(d.getTime() / 1000);
      if (!times.has(x))
        times.set(x, []);
      times.get(x).push(r);
    }
    const xs = Array.from(times.keys()).sort((a, b) => a - b);
    const values = (group) => xs.map((x) => times.get(x)
                                               .map((r) => (columns[group] ? columnValue(r, columns[group]) : null))
                                               .filter((v) => v !== null)
                                               .sort((a, b) => a - b));
    // Kelvins (the name of a grid parameter may say so though the server converted them) and
    // fractions of cloud cover
    const scaled = (group, lists) => {
      const all = lists.flat();
      if (all.length === 0)
        return lists;
      const median = quantile(all.slice().sort((a, b) => a - b), 0.5);
      if ((group === 'temperature' || group === 'dewpoint') && median > 100)
        return lists.map((l) => l.map((v) => v - KELVIN));
      if (group === 'cloud' && Math.max(...all) <= 1)
        return lists.map((l) => l.map((v) => v * 100));
      return lists;
    };
    const stat = (lists, q) => lists.map((l) => (l.length ? quantile(l, q) : null));
    // Temperatures and speeds in the chosen units; the barbs take m/s
    const [, tUnit, toT] = unitOf('temperature');
    const [, sUnit, toS] = unitOf('speed');
    const inUnit = (lists, f) => lists.map((l) => l.map(f));
    const ensemble = Array.from(times.values()).some((list) => list.length > 1);
    const css = getComputedStyle(document.documentElement);
    const axisColor = css.getPropertyValue('--muted').trim() || '#666';
    const gridColor = css.getPropertyValue('--line').trim() || '#ddd';
    const width = Math.max(320, Math.min(1000, container.clientWidth - 8 || 700));
    const sync = root.uPlot.sync(`meteogram-${Math.random()}`);
    const axes = (unit) => [
      {stroke: axisColor, grid: {stroke: gridColor, width: 1}, ticks: {stroke: gridColor}, values: utcTimeLabels, size: 44},
      {stroke: axisColor, grid: {stroke: gridColor, width: 1}, ticks: {stroke: gridColor}, size: 50, label: unit, labelSize: 14}
    ];
    const timeSeries = {value: (u, x) => (x === null ? '--' : formatTime(new Date(x * 1000)))};
    const panel = (title, series, data, options = {}) => {
      const box = el('div', {class: 'chart'}, el('div', {class: 'chart-title'}, title));
      const plot = el('div', {class: 'chart-plot'});
      box.append(plot);
      panels.append(box);
      return makePlot(root, Object.assign({
        width,
        height: 170,
        series: [timeSeries, ...series],
        axes: axes(options.unit || ''),
        scales: Object.assign({x: {time: true}}, options.scales || {}),
        cursor: {sync: {key: sync.key}},
        legend: {live: true},
        tzDate: (ts) => root.uPlot.tzDate(new Date(ts * 1000), 'Etc/UTC')
      }, options.extra || {}), data, plot);
    };
    const band = (lists, colour, label) => {
      // The median, and for an ensemble the 10-90 % range as a band
      if (!ensemble)
        return {series: [{label, stroke: colour, width: 2}], data: [stat(lists, 0.5)], bands: []};
      return {
        series: [
          {label: `${label} 10 %`, stroke: 'transparent', points: {show: false}},
          {label: `${label} 90 %`, stroke: 'transparent', points: {show: false}},
          {label: L`${label} median`, stroke: colour, width: 2}
        ],
        data: [stat(lists, 0.1), stat(lists, 0.9), stat(lists, 0.5)],
        bands: [{series: [2, 1], fill: `${colour}33`}]
      };
    };
    const offset = (bands, n) => bands.map((b) => ({series: b.series.map((i) => i + n), fill: b.fill}));

    // Temperature and dewpoint
    if (columns.temperature)
    {
      const tb = band(inUnit(scaled('temperature', values('temperature')), toT), '#e03131', L('temperature'));
      const parts = [tb];
      if (columns.dewpoint)
        parts.push(band(inUnit(scaled('dewpoint', values('dewpoint')), toT), '#2f9e44', L('dewpoint')));
      let n = 0;
      const bands = [];
      for (const part of parts)
      {
        bands.push(...offset(part.bands, n));
        n += part.series.length;
      }
      panel(L`Temperature${columns.dewpoint ? L(' and dewpoint') : ''} (${tUnit})${ensemble ? L(', median and 10-90 % of the members') : ''}`,
            parts.flatMap((p) => p.series), [xs, ...parts.flatMap((p) => p.data)], {unit: tUnit, extra: {bands}});
    }
    // Precipitation as bars, for an ensemble the median with the 90 % as a line
    if (columns.precipitation)
    {
      const lists = values('precipitation');
      const bars = root.uPlot.paths.bars({size: [0.6, 40]});
      const series = [{label: ensemble ? L('median') : L('precipitation'), stroke: '#1971c2', fill: '#1971c288', paths: bars, points: {show: false}}];
      const data = [stat(lists, 0.5)];
      if (ensemble)
      {
        series.push({label: '90 %', stroke: '#1971c2', width: 1, dash: [4, 3], points: {show: false}});
        data.push(stat(lists, 0.9));
      }
      panel(L`Precipitation (mm)${ensemble ? L(', the median and 90 % of the members') : ''}`, series, [xs, ...data],
            {unit: 'mm', scales: {y: {range: (u, lo, hi) => [0, Math.max(1, hi * 1.1)]}}});
    }
    // Cloud cover
    if (columns.cloud)
    {
      const cb = band(scaled('cloud', values('cloud')), '#495057', L('cloud cover'));
      cb.series[cb.series.length - 1].fill = ensemble ? undefined : '#49505733';
      panel(L`Cloud cover (%)${ensemble ? L(', median and 10-90 % of the members') : ''}`, cb.series, [xs, ...cb.data],
            {unit: '%', scales: {y: {range: () => [0, 100]}}, extra: {bands: cb.bands}});
    }
    // Wind and gusts, with barbs of the median wind at the bottom
    let speeds = columns.speed ? values('speed') : null;
    let directions = columns.direction ? values('direction') : null;
    if (!speeds && columns.u && columns.v)
    {
      const us = values('u');
      const vs = values('v');
      speeds = us.map((l, k) => l.map((u, i) => Math.hypot(u, vs[k][i] ?? 0)).sort((a, b) => a - b));
      directions = us.map((l, k) => l.map((u, i) => ((Math.atan2(-u, -(vs[k][i] ?? 0)) * 180) / Math.PI + 360) % 360));
    }
    if (speeds)
    {
      const wb = band(inUnit(speeds, toS), '#7048e8', L('wind speed'));
      const series = [...wb.series];
      const data = [...wb.data];
      if (columns.gust)
      {
        series.push({label: ensemble ? L('gust median') : L('gust'), stroke: '#f08c00', width: 1.5, dash: [5, 3]});
        data.push(stat(inUnit(values('gust'), toS), 0.5));
      }
      // The mean direction of the members: the direction of the mean unit vector
      const meanDirection = directions ? directions.map((l) => {
        if (!l.length)
          return null;
        const x = l.reduce((s, d) => s + Math.sin((d * Math.PI) / 180), 0);
        const y = l.reduce((s, d) => s + Math.cos((d * Math.PI) / 180), 0);
        return ((Math.atan2(x, y) * 180) / Math.PI + 360) % 360;
      }) : null;
      const medianSpeed = stat(speeds, 0.5);
      const barbs = (u) => {
        const old = u.over.querySelector('.meteogram-barbs');
        if (old)
          old.remove();
        if (!meanDirection)
          return;
        const layer = svg('svg', {class: 'meteogram-barbs', width: u.over.clientWidth, height: u.over.clientHeight});
        let last = -Infinity;
        xs.forEach((x, k) => {
          const px = u.valToPos(x, 'x');
          if (meanDirection[k] === null || medianSpeed[k] === null || px - last < 28)
            return;
          last = px;
          layer.append(windBarb(px, u.over.clientHeight - BARB_STRIP / 2, medianSpeed[k], meanDirection[k]));
        });
        u.over.append(layer);
      };
      // The barbs turn with the wind, so they get a strip of their own below zero, where the
      // scale has no ticks
      const windAxes = axes(sUnit);
      windAxes[1].filter = (u, splits) => splits.map((v) => (v < 0 ? null : v));
      const windHeight = 170 + BARB_STRIP;
      panel(L`Wind${columns.gust ? L(' and gusts') : ''} (${sUnit})${ensemble ? L(', median and 10-90 % of the members') : ''}`,
            series, [xs, ...data],
            {unit: sUnit,
             scales: {y: {range: (u, lo, hi) => {
               const top = Math.max(5, hi * 1.25);
               const plotHeight = windHeight - 44;
               return [-top * BARB_STRIP / (plotHeight - BARB_STRIP), top];
             }}},
             extra: {bands: wb.bands, hooks: {draw: [barbs]}, height: windHeight, axes: windAxes}});
    }
  }
}

// ------------------------------------------------------------------
// Comparison with another producer or collection
// ------------------------------------------------------------------

// The charts of one place compared with another producer or collection when one is chosen: its
// series are drawn with those of the request, and the mean and the mean absolute difference at
// the times both have are listed. options: params, kind (of the levels), ownName, others
// ([{key, name, label, stations}], stations when the rows of the other are named by stations),
// chosen (the key chosen before), onChoose(key), fetchRows(other) resolving to its rows.
function drawComparison(pane, rows, options)
{
  const {params, kind, ownName, others} = options;
  const select = el('select', {class: 'compare-select', 'aria-label': L('Compare with')}, [
    el('option', {value: ''}, L('No comparison')),
    ...others.map((o) => el('option', {value: o.key}, o.label))
  ]);
  select.value = options.chosen && others.some((o) => o.key === options.chosen) ? options.chosen : '';
  const status = el('span', {class: 'muted small'});
  const stats = el('div', {class: 'compare-stats small'});
  const charts = el('div');
  pane.replaceChildren(
      others.length ? el('div', {class: 'row'}, el('label', null, L('Compare with'), select), status) : '',
      stats, charts);
  const draw = (other, otherRows) => {
    // The series are named by their producers, observations also by their stations
    const mine = rows.map((r) => Object.assign({}, r, {name: ownName}));
    const theirs = (otherRows || []).map((r) => Object.assign({}, r, {
                                           name: `${other.name}${other.stations && r.name ? ` (${r.name})` : ''}`,
                                           level: undefined
                                         }));
    const both = displayRows([...mine, ...theirs]);
    drawCharts(charts, both.rows, params, 'lines',
               {levelKind: kind, labels: both.labels, seriesWord: theirs.length ? 'producers' : undefined});
    stats.replaceChildren();
    if (!other || theirs.length === 0)
      return;
    // The differences at the times both have, in the chosen units
    const at = (list, p) => new Map(displayRows(list).rows.filter((r) => typeof r[p] === 'number')
                                        .map((r) => [parseTime(r.time).getTime(), r[p]]));
    const lines = params.map((p) => {
      const a = at(mine, p);
      const b = at(theirs, p);
      const diffs = Array.from(a.keys()).filter((x) => b.has(x)).map((x) => a.get(x) - b.get(x));
      if (diffs.length === 0)
        return L`${p}: no common times`;
      const mean = diffs.reduce((s, d) => s + d, 0) / diffs.length;
      const mae = diffs.reduce((s, d) => s + Math.abs(d), 0) / diffs.length;
      return `${p}: ${ownName} − ${other.name} on average ${mean >= 0 ? '+' : ''}${mean.toFixed(2)}, ` +
          L`mean absolute difference ${mae.toFixed(2)} at ${diffs.length} times`;
    });
    stats.replaceChildren(...lines.map((l) => el('div', null, l)));
  };
  const load = async () => {
    const other = others.find((o) => o.key === select.value);
    options.onChoose(select.value);
    if (!other)
    {
      status.textContent = '';
      draw(null, null);
      return;
    }
    status.textContent = L('Loading…');
    try
    {
      const otherRows = await options.fetchRows(other);
      status.textContent = otherRows.length ? '' : L`No values of ${other.name} here`;
      draw(other, otherRows);
    }
    catch (e)
    {
      status.textContent = e.message;
      draw(null, null);
    }
  };
  select.addEventListener('change', load);
  if (select.value)
    load();
  else
    draw(null, null);
}

// ------------------------------------------------------------------
// Charts of time series
// ------------------------------------------------------------------

// More locations than this are drawn as percentiles by default, and as a swarm of thin lines
// without a legend when lines are chosen
const MAX_LINES = 20;

// The colours of a swarm: a narrow gradient, from south to north when the latitudes are known
function swarmColour(i, n)
{
  const f = n > 1 ? i / (n - 1) : 0;
  return `hsla(${Math.round(205 + 70 * f)}, 65%, 50%, 0.45)`;
}

// The statistics the server computes over an area, and the TimeSeries functions computing them
const AREA_STATISTICS = [
  [0.1, '10 %', 'nanpercentile_a[10]'],
  [0.25, '25 %', 'nanpercentile_a[25]'],
  [0.5, 'median', 'nanmedian_a'],
  [0.75, '75 %', 'nanpercentile_a[75]'],
  [0.9, '90 %', 'nanpercentile_a[90]']
];

const statisticParameter = (fn, param) => `${fn}(${param})`;

// The percentiles of an area computed by the server: a chart per parameter, like the
// percentiles of many locations
function drawServerStatistics(container, rows, parameters)
{
  container.replaceChildren();
  // A chart per level and variant
  const groups = new Map();
  for (const row of rows)
  {
    const t = parseTime(row.time);
    if (!t)
      continue;
    const label = [
      row.level !== undefined && row.level !== null ? `level ${row.level}` : '',
      row._variant || ''
    ].filter(Boolean).join(' · ');
    if (!groups.has(label))
      groups.set(label, []);
    groups.get(label).push([t, row]);
  }
  if (groups.size === 0)
  {
    container.append(el('p', {class: 'muted'}, L('No data.')));
    return;
  }
  for (const [label, series] of groups)
    drawServerStatisticsGroup(container, series.sort((a, b) => a[0] - b[0]), parameters, label);
  if (container.children.length === 0)
    container.append(el('p', {class: 'muted'}, L('No numeric values to draw.')));
}

// The charts of statistics computed by the server for one group of locations: the median and
// the 10-90 % and 25-75 % ranges of each parameter
function drawServerStatisticsGroup(container, series, parameters, label)
{
  const xs = series.map(([t]) => Math.round(t.getTime() / 1000));
  const css = getComputedStyle(document.documentElement);
  const axisColor = css.getPropertyValue('--muted').trim() || '#666';
  const gridColor = css.getPropertyValue('--line').trim() || '#ddd';
  const width = Math.max(320, container.clientWidth - 8);
  const fill = (a) => `rgba(47, 111, 223, ${a})`;
  for (const param of parameters)
  {
    const columns = AREA_STATISTICS.map(([, , fn]) => series.map(([, row]) => {
      const v = row[statisticParameter(fn, param)];
      return typeof v === 'number' && isFinite(v) ? v : null;
    }));
    if (columns.every((c) => c.every((v) => v === null)))
      continue;
    const panel = el('div', {class: 'chart', 'data-param': param, 'data-mode': 'server'});
    container.append(panel);
    panel.append(
        el('div',
           {class: 'chart-title'},
           L`${param}${label ? ` · ${label}` : ''}: median and percentiles of the area, computed by the server`));
    const plot = el('div', {class: 'chart-plot'});
    panel.append(plot);
    // eslint-disable-next-line no-new
    makePlot(root,
        {
          width: width,
          height: 260,
          series: [
            {value: (u, x) => (x === null ? '--' : formatTime(new Date(x * 1000)))},
            {label: '10 %', stroke: fill(0.5), width: 1},
            {label: '25 %', stroke: fill(0.7), width: 1},
            {label: L('median'), stroke: palette[0], width: 2},
            {label: '75 %', stroke: fill(0.7), width: 1},
            {label: '90 %', stroke: fill(0.5), width: 1}
          ],
          bands: [{series: [5, 1], fill: fill(0.12)}, {series: [4, 2], fill: fill(0.22)}],
          axes: [
            {
              stroke: axisColor,
              grid: {stroke: gridColor, width: 1},
              ticks: {stroke: gridColor},
              values: utcTimeLabels,
              size: 50
            },
            {
              stroke: axisColor,
              grid: {stroke: gridColor, width: 1},
              ticks: {stroke: gridColor},
              size: 60
            }
          ],
          scales: {x: {time: true}},
          legend: {live: true},
          tzDate: (ts) => root.uPlot.tzDate(new Date(ts * 1000), 'Etc/UTC')
        },
        [xs, ...columns],
        plot);
  }
}

// The labels of a time axis of a chart: UTC times in 24 hour format, with the date where the
// day changes. Ticks of whole days are dates only, the year once.
function utcTimeLabels(u, splits)
{
  if (splits.length > 1 && splits.every((x) => x % 86400 === 0))
    return splits.map((x, i) => {
      const d = new Date(x * 1000);
      const md = `${pad(d.getUTCMonth() + 1)}-${pad(d.getUTCDate())}`;
      return i === 0 ? `${d.getUTCFullYear()}-${md}` : md;
    });
  let previous = '';
  return splits.map((x) => {
    const d = new Date(x * 1000);
    const day = `${d.getUTCFullYear()}-${pad(d.getUTCMonth() + 1)}-${pad(d.getUTCDate())}`;
    const time = `${pad(d.getUTCHours())}:${pad(d.getUTCMinutes())}`;
    const label = day !== previous ? `${time}\n${day}` : time;
    previous = day;
    return label;
  });
}

// The lines of a swarm, drawn by the chart directly on its canvas: thousands of locations would
// be too slow as series of their own (each has its own paths, cursor point and legend value).
// lines: [{label, values (one per x, null if none), colour, breaks (k: whether the line breaks
// before x[k])}]. Returns the uPlot hooks and the y range of the lines.
function swarmLayer(xs, lines)
{
  let lo = Infinity;
  let hi = -Infinity;
  for (const line of lines)
    for (const v of line.values)
      if (v !== null)
      {
        lo = Math.min(lo, v);
        hi = Math.max(hi, v);
      }
  // The lines grouped by colour, a path each
  const colours = new Map();
  for (const line of lines)
  {
    if (!colours.has(line.colour))
      colours.set(line.colour, []);
    colours.get(line.colour).push(line);
  }
  // Draws the lines on the canvas of the chart before the series (the drawAxes hook), so that the
  // median is on top
  const draw = (u) => {
    const ctx = u.ctx;
    const {left, top, width, height} = u.bbox;
    ctx.save();
    ctx.beginPath();
    ctx.rect(left, top, width, height);
    ctx.clip();
    ctx.lineWidth = Math.max(1, (typeof devicePixelRatio === 'number' ? devicePixelRatio : 1));
    ctx.lineJoin = 'round';
    const px = xs.map((x) => u.valToPos(x, 'x', true));
    for (const [colour, group] of colours)
    {
      ctx.beginPath();
      for (const line of group)
      {
        let pen = false;
        line.values.forEach((v, k) => {
          if (v === null)
            return;
          const y = u.valToPos(v, 'y', true);
          if (pen && !line.breaks[k])
            ctx.lineTo(px[k], y);
          else
            ctx.moveTo(px[k], y);
          pen = true;
        });
      }
      ctx.strokeStyle = colour;
      ctx.stroke();
    }
    ctx.restore();
  };

  // The line nearest to the pointer, highlighted with its name and value
  let overlay = null;
  let path = null;
  let label = null;
  // The overlay of the highlighted line and its label, once the chart exists
  const ready = (u) => {
    overlay = svg('svg', {class: 'swarm-hover'});
    path = svg('path', {class: 'swarm-line'});
    label = el('div', {class: 'swarm-label small'});
    overlay.append(path);
    u.over.append(overlay, label);
  };
  // Highlights the line nearest to the pointer at the time under it, if within 12 pixels
  const setCursor = (u) => {
    const k = u.cursor.idx;
    const cy = u.cursor.top;
    if (!overlay)
      return;
    let best = null;
    let bestD = 12;
    if (k !== null && k !== undefined && cy !== null && cy >= 0)
      for (const line of lines)
      {
        const v = line.values[k];
        if (v === null)
          continue;
        const d = Math.abs(u.valToPos(v, 'y') - cy);
        if (d < bestD)
        {
          bestD = d;
          best = line;
        }
      }
    if (!best)
    {
      overlay.setAttribute('visibility', 'hidden');
      label.hidden = true;
      return;
    }
    overlay.setAttribute('width', u.over.clientWidth);
    overlay.setAttribute('height', u.over.clientHeight);
    let d = '';
    let pen = false;
    best.values.forEach((v, i) => {
      if (v === null)
        return;
      d += `${pen && !best.breaks[i] ? 'L' : 'M'}${u.valToPos(xs[i], 'x').toFixed(1)},${
          u.valToPos(v, 'y').toFixed(1)}`;
      pen = true;
    });
    path.setAttribute('d', d);
    overlay.setAttribute('visibility', 'visible');
    label.hidden = false;
    label.textContent = `${best.label}: ${round(best.values[k], 2)}`;
    const x = u.valToPos(xs[k], 'x');
    label.style.left = `${Math.min(x + 8, Math.max(0, u.over.clientWidth - 220))}px`;
    label.style.top = `${Math.max(0, u.valToPos(best.values[k], 'y') - 24)}px`;
  };
  return {
    hooks: {drawAxes: [draw], ready: [ready], setCursor: [setCursor]},
    range: lo <= hi ? [lo, hi] : null
  };
}

// Calls draw once the element is near the visible part of the page: charts further down are
// drawn when scrolled to
function whenVisible(element, draw)
{
  if (typeof IntersectionObserver === 'undefined')
  {
    draw();
    return;
  }
  const observer = new IntersectionObserver((entries) => {
    if (entries.some((e) => e.isIntersecting))
    {
      observer.disconnect();
      draw();
    }
  }, {rootMargin: '300px'});
  observer.observe(element);
}

// The level of the charts of rows with several levels: a level nearest to the ground for
// several locations (the median of locations at different levels would mix the surface with the
// top of the atmosphere), every level as a series of its own for a single location
function chartLevels(rows, levelKind)
{
  const levels = Array.from(new Set(rows.map((r) => r.level).filter((v) => typeof v === 'number')))
                     .sort((a, b) => a - b);
  if (levels.length < 2)
    return {levels: [], fallback: ''};
  const places = new Set(rows.map((r) => locationOf(Object.assign({}, r, {level: null})))).size;
  // The members of an ensemble are series of their own
  if (levelKind === 'member')
    return {levels, fallback: ''};
  // Model levels and pressures grow downwards
  const down = ['hybrid', 'pressure', 'pressure-pa', 'sounding'].includes(levelKind);
  const lowest = down ? levels[levels.length - 1] : levels[0];
  return {levels, fallback: places > 1 ? String(lowest) : ''};
}

// The charts of the numeric parameters of the rows, one chart per parameter. 'mode' is "lines",
// "percentiles" or "probability" (of exceeding a threshold); the default depends on the number
// of series. "server" draws the percentiles the server computed over an area
// (AREA_STATISTICS). options: labels (the units of the columns), levelKind (the kind of the
// levels of the rows), level (the level
// drawn, '' for every level as a series of its own), thresholds and below (of the probability
// of each parameter), seriesWord (what the series are, e.g. 'producers').
function drawCharts(container, rows, parameters, mode, options = {})
{
  if (mode === 'server')
  {
    drawServerStatistics(container, rows, parameters);
    return;
  }
  container.replaceChildren();
  if (rows.length === 0)
  {
    container.append(el('p', {class: 'muted'}, L('No data.')));
    return;
  }

  // Several levels: one of them, or all
  const allRows = rows;
  const {levels, fallback} = chartLevels(rows, options.levelKind);
  const level = levels.length === 0 ? '' :
      options.level !== undefined && (options.level === '' || levels.includes(Number(options.level))) ?
                                      String(options.level) :
                                      fallback;
  let levelChooser = null;
  if (levels.length > 0)
  {
    const unit = {pressure: ' hPa', 'pressure-pa': ' Pa', height: ' m', depth: ' m'}[options.levelKind] || '';
    const member = options.levelKind === 'member';
    const select = el('select',
                      {'aria-label': L('Level of the charts')},
                      [[ '', member ? L('All members') : L('All levels, a line each')],
                       ...levels.map((v) => [String(v), member ? `Member ${v}` : `${v}${unit}`])]
                          .map(([v, l]) => el('option', {value: v}, l)));
    select.value = level;
    // The drawing chosen stays when the level changes
    select.addEventListener('change', () => drawCharts(
        container, allRows, parameters, chosen, Object.assign({}, options, {level: select.value})));
    levelChooser = el('label', null, member ? L('Member') : L('Level'), select);
    if (level !== '')
      rows = rows.filter((r) => r.level === Number(level));
  }

  const times = new Map();  // epoch seconds -> null
  const locations = new Map();
  const latitudes = new Map();
  for (const row of rows)
  {
    const t = parseTime(row.time);
    if (!t)
      continue;
    const s = Math.round(t.getTime() / 1000);
    if (!times.has(s))
      times.set(s, null);
    const loc = locationOf(row);
    if (!locations.has(loc))
    {
      locations.set(loc, new Map());
      latitudes.set(loc, typeof row.lat === 'number' ? row.lat : null);
    }
    locations.get(loc).set(s, row);
  }
  const allTimes = Array.from(times.keys()).sort((a, b) => a - b);
  if (allTimes.length === 0)
  {
    container.append(
        el('p',
           {class: 'muted'},
           L('The response has no times to draw: include time in the parameters.')));
    return;
  }

  const numeric = parameters.filter((p) => rows.some((r) => typeof r[p] === 'number'));
  // Parameters without values are named rather than left out silently
  const empty = parameters.filter((p) => !numeric.includes(p));
  if (numeric.length === 0)
  {
    container.append(
        el('p',
           {class: 'muted'},
           L`No numeric values to draw${empty.length ? L` for ${empty.join(', ')}` : ''}.`));
    return;
  }

  const names = Array.from(locations.keys());
  const many = names.length > MAX_LINES;
  // The series are the members of an ensemble at one place, or locations. The members are levels
  // of an EDR collection, or the members of grid parameter keys.
  const places =
      new Set(rows.map((r) => locationOf(Object.assign({}, r, {level: null, _variant: null})))).size;
  const members = (options.levelKind === 'member' && level === '') ||
      (rows.length > 0 && rows.every((r) => r._member !== undefined));
  const what = options.seriesWord || (members && places === 1 ? 'members' : 'locations');
  const chosen = mode || (many ? 'percentiles' : 'lines');

  // The choice of the drawing for several locations, and of the level
  const choices = el('div', {class: 'row chart-choices'});
  if (names.length > 2)
  {
    const toolbar = el('div', {class: 'tabs chart-modes', role: 'tablist'});
    for (const [m, label] of [
             ['lines', many ? `All ${what}` : 'Lines'],
             ['percentiles', L('Percentiles')],
             ['probability', L('Probability')]])
      toolbar.append(
          el('button',
             {
               type: 'button',
               role: 'tab',
               'data-mode': m,
               'aria-selected': m === chosen ? 'true' : 'false',
               onclick: () => drawCharts(container, allRows, parameters, m, Object.assign({}, options, {level}))
             },
             label));
    choices.append(toolbar);
  }
  if (levelChooser)
    choices.append(levelChooser);
  if (choices.children.length > 0)
    container.append(choices);

  // A swarm is ordered by latitude so that the gradient runs from south to north
  const ordered = names.slice();
  const byLatitude = names.every((n) => latitudes.get(n) !== null);
  if (byLatitude)
    ordered.sort((a, b) => latitudes.get(a) - latitudes.get(b));

  const width = Math.max(320, container.clientWidth - 8);
  const css = getComputedStyle(document.documentElement);
  const axisColor = css.getPropertyValue('--muted').trim() || '#666';
  const gridColor = css.getPropertyValue('--line').trim() || '#ddd';
  const timeLabels = utcTimeLabels;
  const axes = [
    {
      stroke: axisColor,
      grid: {stroke: gridColor, width: 1},
      ticks: {stroke: gridColor},
      values: timeLabels,
      size: 50
    },
    {stroke: axisColor, grid: {stroke: gridColor, width: 1}, ticks: {stroke: gridColor}, size: 60}
  ];

  // The legend shows the time under the cursor in UTC
  const timeSeries = {value: (u, x) => (x === null ? '--' : formatTime(new Date(x * 1000)))};

  for (const param of numeric)
  {
    // The name of the parameter with its unit when not the default
    const pname = (options.labels || {})[param] ? `${param} (${options.labels[param]})` : param;
    // The value of the parameter at time x of a location, null if none
    const valueOf = (map, x) => {
      const row = map.get(x);
      const v = row ? row[param] : null;
      return typeof v === 'number' && isFinite(v) ? v : null;
    };
    // The times of the parameter: those at which some location has a value
    const xs = allTimes.filter(
        (x) => Array.from(locations.values()).some((map) => valueOf(map, x) !== null));

    // The percentiles over the locations at each time. Locations may report at different
    // intervals (every minute, every 10 minutes, hourly): a time at which only a few of them
    // report would make a comb of the percentiles, so it is left out.
    const percentiles = (qs) => {
      const counts = xs.map(
          (x) => Array.from(locations.values()).filter((map) => valueOf(map, x) !== null).length);
      // Most times may have only a few locations (a station reporting every minute among ones
      // reporting every 10 minutes), so the measure is the most locations at a time
      const typical = Math.max(1, ...counts);
      const columns = qs.map(() => []);
      xs.forEach((x, k) => {
        if (counts[k] < Math.ceil(typical / 2))
        {
          qs.forEach((q, i) => columns[i].push(null));
          return;
        }
        const values = [];
        for (const map of locations.values())
        {
          const v = valueOf(map, x);
          if (v !== null)
            values.push(v);
        }
        values.sort((a, b) => a - b);
        qs.forEach((q, i) => columns[i].push(quantile(values, q)));
      });
      return columns;
    };
    // A series is drawn across the times at which it has no value, unless the gap is much
    // longer than its own interval: a station reporting every 10 minutes is a line also
    // among stations reporting every minute, but a missing day is a gap
    const gapsOf = (values) => {
      const own = xs.filter((x, i) => values[i] !== null);
      const steps = own.slice(1).map((x, i) => x - own[i]).sort((a, b) => a - b);
      const step = quantile(steps, 0.5) || 0;
      const longest = Math.max(3 * step, 1);
      return (u, sidx, idx0, idx1, nullGaps) =>
                 nullGaps.filter(([a, b]) => u.posToVal(b, 'x') - u.posToVal(a, 'x') > longest);
    };
    // The same for a line drawn directly: whether it breaks before each time
    const breaksOf = (values) => {
      const own = xs.filter((x, i) => values[i] !== null);
      const steps = own.slice(1).map((x, i) => x - own[i]).sort((a, b) => a - b);
      const longest = Math.max(3 * (quantile(steps, 0.5) || 0), 1);
      let previous = null;
      return values.map((v, i) => {
        if (v === null)
          return false;
        const gap = previous !== null && xs[i] - previous > longest;
        previous = xs[i];
        return gap;
      });
    };

    const panel = el('div', {class: 'chart', 'data-param': param, 'data-mode': chosen});
    container.append(panel);
    let data;
    let series;
    let bands = [];
    let caption;
    let legend = {live: true};
    let swarm = null;
    let probability = null;

    if (chosen === 'lines' && !many)
    {
      data = [xs, ...names.map((n) => xs.map((x) => valueOf(locations.get(n), x)))];
      series = [
        timeSeries,
        ...names.map((n, i) => ({
                       label: n || param,
                       stroke: palette[i % palette.length],
                       width: 1.5,
                       spanGaps: false,
                       gaps: gapsOf(data[i + 1])
                     }))
      ];
      caption = names.length > 1 ? `${pname}, ${names.length} ${what}` : pname;
    }
    else if (chosen === 'lines')
    {
      // Every location as a thin line drawn on the canvas, and the median on top
      const median = percentiles([0.5])[0];
      const lines = ordered.map((n, i) => {
        const values = xs.map((x) => valueOf(locations.get(n), x));
        return {label: n, values, colour: swarmColour(i, ordered.length), breaks: breaksOf(values)};
      });
      swarm = swarmLayer(xs, lines);
      data = [xs, median];
      series = [
        timeSeries,
        {
          label: L('median'),
          stroke: css.getPropertyValue('--fg').trim() || '#000',
          width: 2,
          points: {show: false},
          gaps: gapsOf(median)
        }
      ];
      legend = {show: false};
      caption = L`${pname}: ${names.length} ${L(what)}${
          byLatitude && what === 'locations' ? L(', coloured from south to north') : ''}, and the median`;
    }
    else if (chosen === 'probability')
    {
      // The share of the locations (members) whose value is above (or below) a threshold
      const all = [];
      for (const map of locations.values())
        for (const x of xs)
        {
          const v = valueOf(map, x);
          if (v !== null)
            all.push(v);
        }
      all.sort((a, b) => a - b);
      const ticks = niceTicks(all[0], all[all.length - 1], 4);
      const fallback = ticks[Math.floor(ticks.length / 2)] ?? 0;
      const threshold = (options.thresholds || {})[param] ?? fallback;
      const below = !!(options.below || {})[param];
      const counts = xs.map(
          (x) => Array.from(locations.values()).filter((map) => valueOf(map, x) !== null).length);
      const typical = Math.max(1, ...counts);
      const shares = xs.map((x, k) => {
        if (counts[k] < Math.ceil(typical / 2))
          return null;
        let n = 0;
        for (const map of locations.values())
        {
          const v = valueOf(map, x);
          if (v !== null && (below ? v < threshold : v > threshold))
            n++;
        }
        return (100 * n) / counts[k];
      });
      data = [xs, shares];
      series = [
        timeSeries,
        {
          label: `${below ? 'below' : 'above'} ${threshold} (%)`,
          stroke: palette[0],
          fill: 'rgba(47, 111, 223, 0.15)',
          width: 2,
          gaps: gapsOf(shares)
        }
      ];
      probability = {threshold, below};
      caption = L`${pname}: the share of the ${names.length} ${L(what)} ${below ? L('below') : L('above')} ${
          threshold}`;
    }
    else
    {
      // The median and the 10-90 % and 25-75 % ranges over the locations
      data = [xs, ...percentiles([0.1, 0.25, 0.5, 0.75, 0.9])];
      const fill = (a) => `rgba(47, 111, 223, ${a})`;
      const gaps = gapsOf(data[3]);
      series = [
        timeSeries,
        {label: '10 %', stroke: fill(0.5), width: 1, gaps},
        {label: '25 %', stroke: fill(0.7), width: 1, gaps},
        {label: L('median'), stroke: palette[0], width: 2, gaps},
        {label: '75 %', stroke: fill(0.7), width: 1, gaps},
        {label: '90 %', stroke: fill(0.5), width: 1, gaps}
      ];
      bands = [{series: [5, 1], fill: fill(0.12)}, {series: [4, 2], fill: fill(0.22)}];
      caption = L`${pname}: median and percentiles of ${names.length} ${L(what)}`;
    }

    panel.append(el('div', {class: 'chart-title'}, caption));
    if (probability)
    {
      // The threshold of the parameter, kept when the charts are drawn again
      const side = el('select', {'aria-label': L`Side of the threshold of ${param}`},
                      [['above', L('Above')], ['below', L('Below')]].map(([v, l]) => el('option', {value: v}, l)));
      side.value = probability.below ? 'below' : 'above';
      const value = el('input', {
        type: 'number',
        step: 'any',
        value: String(probability.threshold),
        'aria-label': L`Threshold of ${param}`,
        style: 'width: 7em'
      });
      const redraw = () => {
        const v = Number(value.value);
        if (value.value === '' || !isFinite(v))
          return;
        drawCharts(container, allRows, parameters, 'probability', Object.assign({}, options, {
                     level,
                     thresholds: Object.assign({}, options.thresholds, {[param]: v}),
                     below: Object.assign({}, options.below, {[param]: side.value === 'below'})
                   }));
      };
      side.addEventListener('change', redraw);
      value.addEventListener('change', redraw);
      panel.append(el('div', {class: 'row'}, side, value));
    }
    const plot = el('div', {class: 'chart-plot', style: 'min-height: 260px'});
    panel.append(plot);
    const scales = {x: {time: true}};
    if (probability)
      scales.y = {range: () => [0, 100]};
    if (swarm && swarm.range)
    {
      // The range of all the lines, not only of the median
      const [lo, hi] = swarm.range;
      const pad = (hi - lo || Math.abs(hi) || 1) * 0.05;
      scales.y = {range: () => [lo - pad, hi + pad]};
    }
    const opts = {
      width: width,
      height: 260,
      series: series,
      bands: bands,
      axes: axes,
      scales: scales,
      legend: legend,
      hooks: swarm ? swarm.hooks : {},
      tzDate: (ts) => root.uPlot.tzDate(new Date(ts * 1000), 'Etc/UTC')
    };
    whenVisible(plot, () => makePlot(root, opts, data, plot));
  }
  if (empty.length > 0)
    container.append(
        el('p', {class: 'muted small'}, L`No values in the result: ${empty.join(', ')}`));
}

// ------------------------------------------------------------------
// Values on a map
// ------------------------------------------------------------------

// The colour scales: colours from the low end to the high end
const PALETTES = {
  temperature: [
    '#313695',
    '#4575b4',
    '#74add1',
    '#abd9e9',
    '#e0f3f8',
    '#ffffbf',
    '#fee090',
    '#fdae61',
    '#f46d43',
    '#d73027',
    '#a50026'
  ],
  viridis: [
    '#440154',
    '#482878',
    '#3e4989',
    '#31688e',
    '#26828e',
    '#1f9e89',
    '#35b779',
    '#6ece58',
    '#b5de2b',
    '#fde725'
  ],
  precipitation: ['#f7fbff', '#c6dbef', '#6baed6', '#2171b5', '#08306b', '#6a51a3', '#bc80bd'],
  grey: ['#f7f7f7', '#252525']
};

// '#rrggbb' -> [r, g, b]
function hexColour(hex)
{
  return [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16));
}

// The colour of f = 0...1 on a palette as [r, g, b]; 'steps' > 0 quantizes the scale
function paletteRgb(name, f, steps)
{
  const colours = PALETTES[name] || PALETTES.viridis;
  let t = Math.max(0, Math.min(1, f));
  if (steps > 0)
    t = Math.min(steps - 1, Math.floor(t * steps)) / Math.max(1, steps - 1);
  const pos = t * (colours.length - 1);
  const i = Math.min(colours.length - 2, Math.floor(pos));
  const a = hexColour(colours[i]);
  const b = hexColour(colours[i + 1]);
  const u = pos - i;
  return a.map((v, k) => Math.round(v + (b[k] - v) * u));
}

// The colour of f = 0...1 on a palette; 'steps' > 0 quantizes the scale
function paletteColour(name, f, steps)
{
  return `rgb(${paletteRgb(name, f, steps).join(',')})`;
}

// The size of the cells of points on a grid in degrees, or null when the points are not a
// grid (stations): the median distance to the nearest neighbour, if the distances are similar
function cellSize(points)
{
  if (points.length < 20)
    return null;
  const sample = points.length > 400 ?
      points.filter((_, i) => i % Math.ceil(points.length / 400) === 0) :
      points;
  const distances = [];
  for (const p of sample)
  {
    let best = Infinity;
    const c = Math.cos(rad(p.lat));
    for (const q of points)
    {
      if (q === p)
        continue;
      const d = Math.hypot((q.lon - p.lon) * c, q.lat - p.lat);
      if (d > 0 && d < best)
        best = d;
    }
    if (isFinite(best))
      distances.push(best);
  }
  distances.sort((a, b) => a - b);
  const median = quantile(distances, 0.5);
  const spread = quantile(distances, 0.9) / Math.max(1e-9, quantile(distances, 0.1));
  if (!(median > 0) || spread > 2)
    return null;
  const meanLat = points.reduce((sum, p) => sum + p.lat, 0) / points.length;
  // The cells overlap a little: the grid is seldom aligned with longitude and latitude
  const d = median * 1.2;
  return {dx: d / Math.max(0.05, Math.cos(rad(meanLat))), dy: d};
}

// Whether the rows have values at several locations with coordinates
function hasMapValues(rows)
{
  const seen = new Set();
  for (const r of rows)
    if (typeof r.lat === 'number' && typeof r.lon === 'number')
    {
      seen.add(`${r.lat},${r.lon}`);
      if (seen.size > 1)
        return true;
    }
  return false;
}

// The series of a point clicked on a value map: the values of the parameter of the map at the
// level of the map against time with the time of the map marked, or the profile of the point
// when there is one time of several levels. 'state' is that of the map (param, time, level,
// probe), 'times' its times as in the rows. Returns show(point), draw() and update().
function valueProbe(box, rows, state, times, options)
{
  const root = typeof window !== 'undefined' ? window : globalThis;
  const profile = times.length === 1 &&
      new Set(rows.map((r) => r.level).filter((v) => typeof v === 'number')).size > 1;
  const epochs = times.map((tm) => {
    const d = parseTime(tm);
    return d ? Math.round(d.getTime() / 1000) : null;
  });
  let plot = null;
  let series = [];
  let title = null;
  const close = el('button', {type: 'button', class: 'small', 'aria-label': L('Close the series')}, '×');
  close.addEventListener('click', () => {
    state.probe = null;
    plot = null;
    box.hidden = true;
    box.replaceChildren();
    if (options.onClose)
      options.onClose();
  });
  const at = (r) => r.lat === state.probe.lat && r.lon === state.probe.lon;
  // The points of a box or a radius are named by the box or the radius: such names are left out
  const named = () => state.probe.name && !isCoordinateName(state.probe.name);
  const place = () => `${named() ? `${state.probe.name} ` : ''}(${state.probe.lat.toFixed(3)}, ${
      state.probe.lon.toFixed(3)})`;
  // The title with the value at the time of the map
  const update = () => {
    if (!plot || !title)
      return;
    const v = series[state.time];
    const unit = options.unit ? options.unit(state.param) : '';
    title.textContent = L`${state.param}${unit ? ` (${unit})` : ''} at ${place()}, ${formatTime(new Date(epochs[state.time] * 1000))}: ${
        v === null || v === undefined ? L('missing') : `${round(v, 2)}${unit ? ` ${unit}` : ''}`}`;
    plot.redraw(false);
  };
  // Draws the series of the point clicked
  const draw = () => {
    if (!state.probe)
      return;
    box.hidden = false;
    plot = null;
    const body = el('div');
    box.replaceChildren(el('div', {class: 'row'}, close), body);
    const mine = rows.filter(at);
    if (profile)
    {
      drawProfiles(body, mine, [state.param], options.levelKind);
      return;
    }
    // One value per time of the map at the level of the map
    const byTime = new Map();
    for (const r of mine)
      if (options.atLevel(r) && typeof r[state.param] === 'number' && isFinite(r[state.param]))
        byTime.set(String(r.time), r[state.param]);
    series = times.map((tm) => (byTime.has(tm) ? byTime.get(tm) : null));
    title = el('div', {class: 'chart-title'});
    const target = el('div', {class: 'chart-plot'});
    body.append(el('div', {class: 'chart', 'data-param': state.param}, title, target));
    if (!root.uPlot || epochs.some((x) => x === null))
    {
      title.textContent = L`${state.param} at ${place()}`;
      return;
    }
    const css = getComputedStyle(document.documentElement);
    const accent = css.getPropertyValue('--accent').trim() || '#0969da';
    const axisColor = css.getPropertyValue('--muted').trim() || '#666';
    const gridColor = css.getPropertyValue('--line').trim() || '#ddd';
    // The time of the map as a dashed line and a dot on the value
    const markTime = (u) => {
      const x = u.valToPos(epochs[state.time], 'x', true);
      const v = series[state.time];
      const ctx = u.ctx;
      const ratio = typeof devicePixelRatio === 'number' ? devicePixelRatio : 1;
      ctx.save();
      ctx.strokeStyle = '#d9480f';
      ctx.lineWidth = 2 * ratio;
      ctx.setLineDash([6, 4]);
      ctx.beginPath();
      ctx.moveTo(x, u.bbox.top);
      ctx.lineTo(x, u.bbox.top + u.bbox.height);
      ctx.stroke();
      if (v !== null)
      {
        ctx.setLineDash([]);
        ctx.fillStyle = '#d9480f';
        ctx.beginPath();
        ctx.arc(x, u.valToPos(v, 'y', true), 5 * ratio, 0, 2 * Math.PI);
        ctx.fill();
      }
      ctx.restore();
    };
    const axis = {stroke: axisColor, grid: {stroke: gridColor, width: 1}, ticks: {stroke: gridColor}};
    plot = makePlot(root,
        {
          width: Math.max(320, Math.min(900, box.clientWidth - 8 || 640)),
          height: 200,
          series: [{}, {label: state.param, stroke: accent, width: 2, spanGaps: true}],
          axes: [Object.assign({}, axis, {size: 50}), Object.assign({}, axis, {size: 60})],
          legend: {live: true},
          tzDate: (ts) => root.uPlot.tzDate(new Date(ts * 1000), 'Etc/UTC'),
          hooks: {draw: [markTime]}
        },
        [epochs, series], target);
    update();
  };
  return {
    show(point) {
      state.probe = point;
      if (options.onShow)
        options.onShow(point);
      draw();
    },
    draw,
    update
  };
}

// The values of the rows on a map: a parameter, a level, a time slider and a colour scale.
// Clicking a value shows the time series of the point, or its profile when there is one time
// of several levels. options: levelKind (the kind of the levels of the rows).
function drawValueMap(container, rows, parameters, settings, options = {})
{
  container.replaceChildren();
  const numeric = parameters.filter((p) => rows.some((r) => typeof r[p] === 'number'));
  const located = rows.filter((r) => typeof r.lat === 'number' && typeof r.lon === 'number');
  // Several levels: the map shows one of them, the lowest by default
  const {levels, fallback} = chartLevels(located, options.levelKind);
  if (numeric.length === 0 || located.length === 0)
  {
    container.append(el('p', {class: 'muted'}, L('No located numeric values to show.')));
    return;
  }

  const times = Array.from(new Set(located.map((r) => String(r.time))))
                    .sort((a, b) => (parseTime(a) || 0) - (parseTime(b) || 0));
  const state = {
    param: numeric[0],
    time: 0,
    level: levels.length ? (fallback !== '' ? fallback : String(levels[0])) : '',
    palette: 'viridis',
    steps: 0,
    min: null,
    max: null,
    probe: null  // The point clicked: {lat, lon, name}
  };
  const atLevel = (r) => state.level === '' || r.level === Number(state.level);
  // The production palettes: bands of fixed values and colours
  let production = {palettes: {}, parameters: {}};
  const bandsOf = (name) => production.palettes[name.replace(/^production:/, '')];
  const isProduction = () => state.palette.startsWith('production:');

  const map = createMap(settings, null);
  map.setTools(['move'], 'move');
  const paramSelect =
      el('select', {'aria-label': L('Parameter')}, numeric.map((p) => el('option', {value: p}, p)));
  const paletteSelect =
      el('select',
         {'aria-label': L('Colours')},
         Object.keys(PALETTES).map((p) => el('option', {value: p}, p)));
  const productionGroup = el('optgroup', {label: L('Production isobands')});
  const stepsSelect =
      el('select',
         {'aria-label': L('Classes')},
         [['0', L('Continuous')], ['5', '5 classes'], ['8', '8 classes'], ['12', '12 classes']].map(
             ([v, t]) => el('option', {value: v}, t)));
  const unit = {pressure: ' hPa', 'pressure-pa': ' Pa', height: ' m', depth: ' m'}[options.levelKind] || '';
  const member = options.levelKind === 'member';
  const levelSelect = el('select', {'aria-label': member ? L('Member of the map') : L('Level of the map')},
                         levels.map((v) => el('option', {value: String(v)}, member ? L`Member ${v}` : `${v}${unit}`)));
  levelSelect.value = state.level;
  const minInput = el('input', {type: 'number', step: 'any', 'aria-label': L('Minimum'), size: 6});
  const maxInput = el('input', {type: 'number', step: 'any', 'aria-label': L('Maximum'), size: 6});
  const slider = timeSlider(times.map((tm) => {
    const d = parseTime(tm);
    return d ? d.getTime() : null;
  }), {
    label: L('Time of the map'),
    format: (tm, i) => (typeof tm === 'number' ? formatTime(new Date(tm)) : times[i]),
    onChange: () => {
      state.time = slider.index();
      draw();
      probe.update();
    }
  });
  const legend = el('div', {class: 'legend'});
  const probeBox = el('div', {class: 'map-probe', hidden: true});
  container.append(
      el('div',
         {class: 'row'},
         el('label', null, L('Parameter'), paramSelect),
         levels.length ? el('label', null, member ? L('Member') : L('Level'), levelSelect) : '',
         el('label', null, L('Colours'), paletteSelect),
         el('label', null, L('Scale'), stepsSelect),
         el('label', null, L('Min'), minInput),
         el('label', null, L('Max'), maxInput)),
      slider.element,
      legend,
      el('p', {class: 'map-hint'},
         times.length === 1 && levels.length > 1 ? L('Click a value on the map for the profile of the point.') :
                                                   L('Click a value on the map for the time series of the point.')),
      map.element,
      probeBox);

  // The default range: the 2nd and the 98th percentiles of all times, so that the colours
  // keep their meaning when the time changes
  const defaultRange = () => {
    const values = located.filter(atLevel).map((r) => r[state.param])
                       .filter((v) => typeof v === 'number' && isFinite(v))
                       .sort((a, b) => a - b);
    return [quantile(values, 0.02), quantile(values, 0.98)];
  };

  let size = null;
  // Draws the values at the chosen time on the map, and the legend
  const draw = () => {
    const [lo, hi] = [state.min, state.max];
    const t = times[state.time];
    const items = located.filter((r) => String(r.time) === t && atLevel(r)).map((r) => {
      const v = r[state.param];
      const ok = typeof v === 'number' && isFinite(v);
      let color = null;
      if (ok && isProduction())
      {
        // The colour of the band of the value; values outside the bands are left out
        const band = bandsOf(state.palette).find((b) => v >= b[0] && v < b[1]);
        color = band ? `rgb(${band[2]},${band[3]},${band[4]})` : null;
      }
      else if (ok)
        color = paletteColour(state.palette, hi > lo ? (v - lo) / (hi - lo) : 0.5, state.steps);
      return {
        lon: r.lon,
        lat: r.lat,
        color: color,
        label: `${state.param}: ${ok ? `${round(v, 2)}${(options.labels || {})[state.param] ? ` ${options.labels[state.param]}` : ''}` : 'missing'}`,
        info: {name: r.name, lat: r.lat, lon: r.lon},
        hint: times.length === 1 && levels.length > 1 ? L('Click for the profile') : L('Click for the time series'),
        onclick: () => probe.show({lat: r.lat, lon: r.lon, name: r.name})
      };
    });
    if (size === null)
      size = cellSize(items) || {dx: 0, dy: 0};
    map.setCells({items, dx: size.dx, dy: size.dy});
    // The legend: the colours and the range, or the bands of a production palette
    minInput.disabled = maxInput.disabled = stepsSelect.disabled = isProduction();
    if (isProduction())
    {
      const bands = bandsOf(state.palette);
      const label = (v) => (Math.abs(v) >= 999 ? '' : String(v));
      legend.replaceChildren(
          el('span',
             {class: 'legend-bar legend-bands'},
             bands.map(
                 (b) =>
                     el('span',
                        {
                          class: 'legend-step',
                          style: `background:rgb(${b[2]},${b[3]},${b[4]})`,
                          title: `${label(b[0]) || '…'} – ${label(b[1]) || '…'}`
                        },
                        el('span', {class: 'legend-label'}, label(b[0]))))));
      return;
    }
    const n = state.steps > 0 ? state.steps : 24;
    legend.replaceChildren(
        el('span', {class: 'small'}, lo === null ? '' : round(lo, 2)),
        el('span',
           {class: 'legend-bar'},
           Array.from(
               {length: n},
               (_, i) => el('span', {
                 class: 'legend-step',
                 style: `background:${paletteColour(state.palette, (i + 0.5) / n, state.steps)}`
               }))),
        el('span', {class: 'small'}, hi === null ? '' : round(hi, 2)));
  };
  // The default range of the colour scale into the minimum and maximum fields
  const setRange = () => {
    [state.min, state.max] = defaultRange();
    minInput.value = state.min === null ? '' : round(state.min, 2);
    maxInput.value = state.max === null ? '' : round(state.max, 2);
  };

  levelSelect.addEventListener('change', () => {
    state.level = levelSelect.value;
    setRange();
    draw();
    probe.draw();
  });
  paramSelect.addEventListener('change', () => {
    state.param = paramSelect.value;
    probe.draw();
    setRange();
    if (isProduction())
    {
      state.palette = 'viridis';
      paletteSelect.value = state.palette;
    }
    chooseProduction();
    draw();
  });
  paletteSelect.addEventListener('change', () => {
    state.palette = paletteSelect.value;
    draw();
  });
  stepsSelect.addEventListener('change', () => {
    state.steps = Number(stepsSelect.value);
    draw();
  });
  for (const [input, key] of [[minInput, 'min'], [maxInput, 'max']])
    input.addEventListener('input', () => {
      const v = Number(input.value);
      if (input.value !== '' && isFinite(v))
      {
        state[key] = v;
        draw();
      }
    });
  const probe = valueProbe(probeBox, located, state, times, {
    levelKind: options.levelKind,
    unit: (p) => (options.labels || {})[p] || '',
    atLevel,
    onClose: () => map.setCursor(null),
    onShow: (p) => map.setCursor(p)
  });
  paletteSelect.value = state.palette;
  setRange();
  draw();

  // The production palette of the parameter, when there is one
  const chooseProduction = () => {
    const name = production.parameters[state.param.toLowerCase()];
    if (name && production.palettes[name])
    {
      state.palette = `production:${name}`;
      paletteSelect.value = state.palette;
    }
  };
  if (settings && settings.palettes)
    loadPalettes(settings.palettes).then((doc) => {
      production = doc;
      const names = Object.keys(doc.palettes || {}).sort();
      productionGroup.replaceChildren(
          ...names.map((n) => el('option', {value: `production:${n}`}, n)));
      if (names.length > 0)
        paletteSelect.prepend(productionGroup);
      chooseProduction();
      draw();
    });
  // Fit the map once it has a width
  requestAnimationFrame(() => map.fit(map.extent()));
}

// ------------------------------------------------------------------
// Tables
// ------------------------------------------------------------------

const MAX_TABLE_ROWS = 1000;

// The rows as a table, at most MAX_TABLE_ROWS of them, the columns in the order of the rows
// ------------------------------------------------------------------
// Export
// ------------------------------------------------------------------

// Offers a file made in the browser for saving
function saveFile(blob, name)
{
  const a = el('a', {href: URL.createObjectURL(blob), download: name});
  document.body.append(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(a.href), 10000);
}

// The rows as CSV: the columns of the table, the values quoted when needed
function rowsCsv(rows, labels = {})
{
  const columns = [];
  for (const row of rows)
    for (const k of Object.keys(row))
      if (!k.startsWith('_') && !columns.includes(k))
        columns.push(k);
  const quote = (v) => {
    if (v === null || v === undefined)
      return '';
    const s = String(v);
    return /[",\n\r]/.test(s) ? `"${s.replace(/"/g, '""')}"` : s;
  };
  const head = columns.map((c) => quote(labels[c] ? `${c} (${labels[c]})` : c));
  return [head.join(','), ...rows.map((r) => columns.map((c) => quote(r[c])).join(','))].join('\r\n') + '\r\n';
}

// The SVG properties the stylesheet sets, copied to an exported clone that has no stylesheet
const SVG_STYLE = ['fill', 'fill-opacity', 'stroke', 'stroke-width', 'stroke-dasharray', 'stroke-opacity',
                   'opacity', 'font-size', 'font-family', 'font-weight', 'visibility', 'paint-order',
                   'stroke-linejoin', 'stroke-linecap', 'text-anchor', 'dominant-baseline'];

// A standalone copy of an SVG element of the page: its computed styles inline
function standaloneSvg(node)
{
  const clone = node.cloneNode(true);
  const from = [node, ...node.querySelectorAll('*')];
  const to = [clone, ...clone.querySelectorAll('*')];
  from.forEach((n, i) => {
    const css = getComputedStyle(n);
    to[i].setAttribute('style', SVG_STYLE.map((p) => `${p}:${css.getPropertyValue(p)}`).join(';'));
  });
  const r = node.getBoundingClientRect();
  clone.setAttribute('xmlns', 'http://www.w3.org/2000/svg');
  clone.setAttribute('width', Math.round(r.width));
  clone.setAttribute('height', Math.round(r.height));
  return new XMLSerializer().serializeToString(clone);
}

// An image of what a view of the results shows: its diagrams (SVG) and charts (canvas) from
// top to bottom with the titles of the charts, on the background of the page. Resolves to a
// PNG blob, or null when the view has nothing to draw.
async function viewImage(pane)
{
  const css = getComputedStyle(document.documentElement);
  const background = css.getPropertyValue('--bg').trim() || '#fff';
  const fg = css.getPropertyValue('--fg').trim() || '#000';
  // The parts in the order of the page: the charts with their titles, the diagrams
  const parts = [];
  for (const node of pane.querySelectorAll(
           '.chart, svg.vdiagram, svg.skewt-diagram, svg.hodograph, svg.xmap-svg, .legend, .sk-analysis, .skewt-info, .compare-stats'))
  {
    if (node.closest('.chart') && node.tagName.toLowerCase() === 'svg')
      continue;
    if (node.closest('.chart') && node.classList.contains('legend'))
      continue;
    const r = node.getBoundingClientRect();
    if (r.width === 0 || r.height === 0)
      continue;
    parts.push({node, r});
  }
  if (parts.length === 0)
    return null;
  const left = Math.min(...parts.map((p) => p.r.left));
  const top = Math.min(...parts.map((p) => p.r.top));
  const width = Math.max(...parts.map((p) => p.r.right)) - left;
  const height = Math.max(...parts.map((p) => p.r.bottom)) - top;
  const scale = 2;
  const canvas = el('canvas', {width: Math.ceil(width * scale), height: Math.ceil(height * scale)});
  const ctx = canvas.getContext('2d');
  ctx.scale(scale, scale);
  ctx.fillStyle = background;
  ctx.fillRect(0, 0, width, height);
  const image = (src) => new Promise((resolve, reject) => {
    const img = new Image();
    img.onload = () => resolve(img);
    img.onerror = reject;
    img.src = src;
  });
  for (const {node, r} of parts)
  {
    const x = r.left - left;
    const y = r.top - top;
    if (node.tagName.toLowerCase() !== 'svg' && !node.classList.contains('chart'))
    {
      // A block of the page: the coloured boxes and the text of its elements
      for (const e of [node, ...node.querySelectorAll('*')])
      {
        const style = getComputedStyle(e);
        const er = e.getBoundingClientRect();
        if (style.display === 'none' || er.width === 0)
          continue;
        const bg = style.backgroundColor;
        if (bg && bg !== 'transparent' && !/rgba\(.*, 0\)$/.test(bg))
        {
          ctx.fillStyle = bg;
          ctx.fillRect(er.left - left, er.top - top, er.width, er.height);
        }
        for (const text of e.childNodes)
        {
          if (text.nodeType !== 3 || !text.textContent.trim())
            continue;
          const range = document.createRange();
          range.selectNodeContents(text);
          const tr = range.getBoundingClientRect();
          ctx.fillStyle = style.color;
          ctx.font = `${style.fontWeight} ${style.fontSize} ${style.fontFamily}`;
          ctx.textBaseline = 'alphabetic';
          ctx.fillText(text.textContent.trim(), tr.left - left, tr.bottom - top - 3);
        }
      }
      continue;
    }
    if (node.classList.contains('chart'))
    {
      // The title, and the canvases of the chart
      const title = node.querySelector('.chart-title');
      if (title)
      {
        const tr = title.getBoundingClientRect();
        ctx.fillStyle = fg;
        ctx.font = `600 13px ${getComputedStyle(title).fontFamily}`;
        ctx.fillText(title.textContent, tr.left - left, tr.top - top + 13);
      }
      for (const c of node.querySelectorAll('canvas'))
      {
        const cr = c.getBoundingClientRect();
        ctx.drawImage(c, cr.left - left, cr.top - top, cr.width, cr.height);
      }
      for (const svgNode of node.querySelectorAll('svg'))
      {
        const sr = svgNode.getBoundingClientRect();
        const img = await image(`data:image/svg+xml;charset=utf-8,${encodeURIComponent(standaloneSvg(svgNode))}`);
        ctx.drawImage(img, sr.left - left, sr.top - top, sr.width, sr.height);
      }
      continue;
    }
    const img = await image(`data:image/svg+xml;charset=utf-8,${encodeURIComponent(standaloneSvg(node))}`);
    ctx.drawImage(img, x, y, r.width, r.height);
  }
  return new Promise((resolve) => canvas.toBlob(resolve, 'image/png'));
}

// The buttons saving the visible view as an image and the rows as CSV
function exportButtons(visiblePane, rows, name, labels = {})
{
  const status = el('span', {class: 'muted small'});
  const png = el('button', {type: 'button', class: 'small', title: L('Save the view as a PNG image')}, 'PNG');
  const csv = el('button', {type: 'button', class: 'small', title: L('Save all the rows as CSV')}, 'CSV');
  png.addEventListener('click', async () => {
    status.textContent = '';
    try
    {
      const blob = await viewImage(visiblePane());
      if (blob)
        saveFile(blob, `${name}.png`);
      else
        status.textContent = L('Nothing to draw in this view');
    }
    catch (e)
    {
      status.textContent = e.message;
    }
  });
  csv.addEventListener('click', () => saveFile(new Blob([rowsCsv(rows, labels)], {type: 'text/csv'}), `${name}.csv`));
  return el('span', {class: 'export-buttons'}, png, csv, status);
}

function drawTable(container, rows, labels = {})
{
  container.replaceChildren();
  if (rows.length === 0)
  {
    container.append(el('p', {class: 'muted'}, L('No data.')));
    return;
  }
  const columns = [];
  for (const row of rows.slice(0, 100))
    for (const k of Object.keys(row))
      if (!k.startsWith('_') && !columns.includes(k))
        columns.push(k);
  const table =
      el('table',
         {class: 'data'},
         el('thead', null, el('tr', null, columns.map((c) => el('th', null, labels[c] ? `${c} (${labels[c]})` : c)))),
         el('tbody',
            null,
            rows.slice(0, MAX_TABLE_ROWS).map((row) => el('tr', null, columns.map((c) => {
                                                const v = row[c];
                                                return el(
                                                    'td',
                                                    {class: typeof v === 'number' ? 'num' : ''},
                                                    v === null || v === undefined ? '' : v);
                                              })))));
  container.append(el('div', {class: 'table-wrap'}, table));
  if (rows.length > MAX_TABLE_ROWS)
    container.append(
        el('p', {class: 'muted'}, L`The first ${MAX_TABLE_ROWS} of ${rows.length} rows.`));
}

// ------------------------------------------------------------------
// Code snippets
// ------------------------------------------------------------------

// A string quoted for a POSIX shell
function shellQuote(s)
{
  return `'${String(s).replace(/'/g, '\'\\\'\'')}'`;
}

// The code of a request in several languages: [[language, code]], curl first, the others from
// the generators of the SmartMet Swagger UI plugin when it is loaded
function snippets(url)
{
  // The API key is shown as a placeholder, never as such
  const request = {url: url, method: 'GET', headers: apiKey ? {'fmi-apikey': '<your API key>'} : {}};
  // The file name extension of the response, as in the snippets of the Swagger plugin
  const ext = S && S.extension ? S.extension(new URL(url)) : 'json';
  const ret = [['bash', `curl --fail ${apiKey ? `--header 'fmi-apikey: <your API key>' ` : ''}--output result.${ext} ${
                             shellQuote(url)}\n`]];
  if (S && S.generators)
  {
    const g = S.generators;
    for (const [title, name] of [
             ['bash loop', 'bash_loop'],
             ['Python', 'python'],
             ['Python loop', 'python_loop'],
             ['R', 'r'],
             ['R loop', 'r_loop'],
             ['JavaScript', 'javascript'],
             ['JavaScript loop', 'javascript_loop']])
    {
      const fn = g[`requestSnippetGenerator_smartmet_${name}`];
      if (fn)
        ret.push([title, fn(request)]);
    }
  }
  return ret;
}

// The code of a request as tabs of languages, with a copy button
function codeTabs(container, url)
{
  container.replaceChildren();
  const items = snippets(url);
  const pre = el('pre', {class: 'code'});
  const copy = el('button', {type: 'button', class: 'small'}, L('Copy'));
  const tabs = el('div', {class: 'tabs', role: 'tablist'});
  let current = 0;
  // Shows the code of language i
  const show = (i) => {
    current = i;
    pre.textContent = items[i][1];
    Array.from(tabs.children)
        .forEach((b, j) => b.setAttribute('aria-selected', j === i ? 'true' : 'false'));
  };
  items.forEach(
      ([title], i) =>
          tabs.append(el('button', {type: 'button', role: 'tab', onclick: () => show(i)}, title)));
  copy.addEventListener('click', () => copyText(items[current][1], copy));
  container.append(el('div', {class: 'code-head'}, tabs, copy), pre);
  show(0);
}

// ------------------------------------------------------------------
// The TimeSeries module
// ------------------------------------------------------------------

// The location types: name, label, example, and the map tool choosing the location. The
// examples come from the defaults of the server, the coordinates from its first map view.
function locationModes()
{
  const ex = DEFAULTS.examples || {};
  const [lon, lat] = DEFAULTS.center || [10.75, 59.91];
  const c = (x, y) => `${round(x, 2)},${round(y, 2)}`;
  return [
    ['place', L('Place'), ex.place ? L`${ex.place} or ${ex.place}:20 for a 20 km radius` :
                                  L('A place name, or name:20 for a 20 km radius'), null],
    ['lonlat', L('Point'), L`${c(lon, lat)}, or click the map`, 'point'],
    ['radius', L('Radius'), L`${c(lon, lat)}:25 (km), or drag on the map`, 'circle'],
    ['bbox', L('Box'), L`${[lon - 2, lat - 1, lon + 2, lat + 1].map((v) => round(v, 1)).join(',')}, or drag on the map`, 'box'],
    ['path', L('Path'), L`${c(lon - 0.5, lat - 0.2)},${c(lon, lat)},${c(lon + 0.5, lat + 0.1)} (lon,lat,...), or click the map`, 'line'],
    ['wkt', L('WKT'), L`MULTIPOINT(${c(lon, lat).replace(',', ' ')},${c(lon + 0.1, lat).replace(',', ' ')}), LINESTRING or POLYGON in lon lat, or draw another type first`, 'wkt'],
    ['keyword', L('Keyword'), ex.keyword || L('A location keyword of the server'), null],
    ['fmisid', L('Station id'), ex.fmisid || L('Station ids, e.g. 1234,5678'), null],
    ['wmo', L('WMO id'), ex.wmo || L('WMO station ids, e.g. 1234,5678'), null],
    ['area', L('Area'), ex.area || L('An area name of the server'), null]
  ];
}

// The points along a path a request asks for: the marked points (the vertices), or points
// every N km along the whole path. The state keeps pathpoints ('marked' or '') and pathstep
// (km, '' for about 40 points). Returns {element, update(sel)}.
function pathPointsChooser(s, onChange)
{
  const marked = el('button', {type: 'button', role: 'radio', 'data-pathpoints': 'marked'}, L('The marked points'));
  const every = el('button', {type: 'button', role: 'radio', 'data-pathpoints': 'every'}, L('Every'));
  const stepInput = el('input', {
    type: 'number',
    min: '0.1',
    step: 'any',
    'aria-label': L('Distance between the points (km)'),
    style: 'width: 6em'
  });
  const note = el('span', {class: 'muted small'});
  const element = el('div',
                     null,
                     el('label', null, L('Points along the path')),
                     el('div',
                        {class: 'row'},
                        el('div', {class: 'segmented', role: 'radiogroup', 'aria-label': L('Points along the path')},
                           marked, every),
                        stepInput,
                        el('span', {class: 'small'}, L('km')),
                        note));
  marked.addEventListener('click', () => {
    s.pathpoints = 'marked';
    onChange();
  });
  every.addEventListener('click', () => {
    delete s.pathpoints;
    onChange();
  });
  stepInput.addEventListener('input', () => {
    const v = Number(stepInput.value);
    if (stepInput.value === '')
      delete s.pathstep;
    else if (v > 0)
      s.pathstep = String(v);
    delete s.pathpoints;
    onChange(false);
  });
  // Shows the choice, the step and the number of points of the path sel
  const update = (sel, rerender = true) => {
    const isMarked = s.pathpoints === 'marked';
    marked.setAttribute('aria-checked', isMarked ? 'true' : 'false');
    every.setAttribute('aria-checked', isMarked ? 'false' : 'true');
    const length = sel && sel.points.length >= 2 ? pathLength(sel.points) : 0;
    const step = pathStepOf(s, sel);
    if (rerender)
      stepInput.value = s.pathstep || '';
    stepInput.placeholder = step ? L`${step} (auto)` : L('auto');
    note.textContent = !length ? '' : isMarked ?
        L`${sel.points.length} points, ${Math.round(length)} km` :
        L`about ${Math.floor(length / Number(step)) + 1} points, ${Math.round(length)} km`;
  };
  return {element, update};
}

// A flight along a path: the departure and arrival times (UTC). The points of the path are
// passed at a constant ground speed, and the period of the request is set to cover the flight
// with a step of the data to spare at both ends, so that the values at the times the points are
// passed can be interpolated. margin() gives that step in minutes. The state keeps departure
// and arrival as the values of the time fields. Returns {element, update(sel), flight()}.
function flightChooser(s, onChange, margin)
{
  const departure = timeInput({'aria-label': L('Departure (UTC)')});
  const arrival = timeInput({'aria-label': L('Arrival (UTC)')});
  const note = el('p', {class: 'muted small'});
  const element = el('div',
                     null,
                     el('div',
                        {class: 'row'},
                        el('label', null, L('Departure (UTC)'), departure),
                        el('label', null, L('Arrival (UTC)'), arrival)),
                     note);
  const flight = () => {
    const a = s.departure ? new Date(fromInput(s.departure)) : null;
    const b = s.arrival ? new Date(fromInput(s.arrival)) : null;
    return a && b && !isNaN(a) && !isNaN(b) && b > a ? {departure: a.getTime(), arrival: b.getTime()} :
                                                      null;
  };
  const changed = () => {
    s.departure = departure.value;
    s.arrival = arrival.value;
    const f = flight();
    if (f)
    {
      const m = (margin() || 180) * 60000;
      s.start = toInput(new Date(f.departure - m));
      s.end = toInput(new Date(f.arrival + m));
    }
    onChange();
  };
  departure.addEventListener('change', changed);
  arrival.addEventListener('change', changed);
  const update = (sel) => {
    departure.value = s.departure || '';
    arrival.value = s.arrival || '';
    const f = flight();
    const km = sel && sel.points.length >= 2 ? pathLength(sel.points) : 0;
    const start = s.start ? new Date(fromInput(s.start)).getTime() : -Infinity;
    const end = s.end ? new Date(fromInput(s.end)).getTime() : Infinity;
    note.className = f && (f.departure < start || f.arrival > end) ? 'warn small' : 'muted small';
    note.textContent = f && (f.departure < start || f.arrival > end) ?
        L('The flight is not within the period of the request: set its times again, or the period') :
        f && km ?
        L`${Math.round(km)} km in ${Math.round((f.arrival - f.departure) / 60000)} min, ${
            Math.round(km / ((f.arrival - f.departure) / 3600000))} km/h: the views show the values at the times the points are passed` :
        (s.departure || s.arrival) && !f ? L('The arrival must be after the departure') :
                                         L('Optional: the values met along a flight');
  };
  return {element, update, flight};
}

// The times the points of a path are passed on a flight (ms), by their distance from the start
function flightTimes(points, flight)
{
  const total = points.length ? points[points.length - 1].km || 1 : 1;
  return points.map((p) => flight.departure + ((flight.arrival - flight.departure) * p.km) / total);
}

// The rows of a flight: each point and level of the path at the time it is passed, its values
// interpolated linearly between the times of the data before and after (directions along the
// shorter way round). Points passed outside the times of the data are left out.
function flightRows(rows, flight)
{
  const points = pathPoints(rows);
  const passed = flightTimes(points, flight);
  const directions = new Set(SOUNDING_PARAMS.direction.map(([n]) => n));
  // The rows of each point and level by their time
  const series = new Map();
  for (const r of rows)
  {
    const d = parseTime(r.time);
    if (!d || typeof r.lat !== 'number')
      continue;
    const key = `${r.lat.toFixed(5)},${r.lon.toFixed(5)}|${r.level}`;
    if (!series.has(key))
      series.set(key, []);
    series.get(key).push({t: d.getTime(), r});
  }
  const index = new Map(points.map((p, i) => [p.key, i]));
  const out = [];
  for (const [key, list] of series)
  {
    const i = index.get(key.split('|')[0]);
    const tp = passed[i];
    list.sort((a, b) => a.t - b.t);
    const k = list.findIndex((x) => x.t >= tp);
    if (k < 0 || (k === 0 && list[0].t > tp))
      continue;
    const a = list[Math.max(0, k - 1)];
    const b = list[k];
    const f = b.t === a.t ? 0 : (tp - a.t) / (b.t - a.t);
    const row = Object.assign({}, a.r, {time: new Date(tp).toISOString(), _flight: true});
    for (const [name, va] of Object.entries(a.r))
    {
      const vb = b.r[name];
      if (typeof va !== 'number' || typeof vb !== 'number' || ['lat', 'lon', 'level'].includes(name))
        continue;
      if (directions.has(name.toLowerCase()))
      {
        const diff = ((((vb - va) % 360) + 540) % 360) - 180;
        row[name] = (va + f * diff + 360) % 360;
      }
      else
        row[name] = va + f * (vb - va);
    }
    out.push(row);
  }
  return out;
}

// The spacing of the points along a path (km): the chosen one, or about 40 points at least
// 1 km apart. '' for the marked points or no path.
function pathStepOf(s, sel)
{
  if (s.pathpoints === 'marked' || !sel || sel.type !== 'path' || sel.points.length < 2)
    return '';
  if (Number(s.pathstep) > 0)
    return String(s.pathstep);
  return String(Math.max(1, Math.round(pathLength(sel.points) / 40)));
}

// The TimeSeries option of a location type
const locationOption = (mode) => (mode === 'radius' ? 'lonlat' : mode);

const NUMBER = '\\s*(-?\\d+(?:\\.\\d*)?)\\s*';

// The point, radius or box of a location value, null if the value is not one
function parseSelection(mode, text)
{
  if (!text)
    return null;
  let m;
  if (mode === 'lonlat' && (m = new RegExp(`^${NUMBER},${NUMBER}$`).exec(text)))
    return {type: 'point', lon: Number(m[1]), lat: Number(m[2])};
  if (mode === 'radius' && (m = new RegExp(`^${NUMBER},${NUMBER}:${NUMBER}$`).exec(text)))
    return {type: 'circle', lon: Number(m[1]), lat: Number(m[2]), km: Number(m[3])};
  if (mode === 'path')
  {
    const numbers = text.split(',').map((v) => v.trim()).filter((v) => v !== '').map(Number);
    // A single vertex is a path being drawn
    if (numbers.length < 2 || numbers.length % 2 !== 0 || numbers.some((v) => !isFinite(v)))
      return null;
    const points = [];
    for (let i = 0; i < numbers.length; i += 2)
      points.push([numbers[i], numbers[i + 1]]);
    return {type: 'path', points};
  }
  if (mode === 'bbox' && (m = new RegExp(`^${NUMBER},${NUMBER},${NUMBER},${NUMBER}$`).exec(text)))
  {
    const [a, b, c, d] = m.slice(1, 5).map(Number);
    return {type: 'box', bbox: [Math.min(a, c), Math.min(b, d), Math.max(a, c), Math.max(b, d)]};
  }
  if (mode === 'wkt')
    return parseWkt(text);
  return null;
}

// The geometry of a WKT text in longitude and latitude, as the parts to draw: points, lines
// and polygons (their rings). POINT, MULTIPOINT (with or without parentheses around the
// points), LINESTRING, MULTILINESTRING, POLYGON and MULTIPOLYGON are understood; null for
// anything else or a text that is not valid.
function parseWkt(text)
{
  const m = /^\s*(MULTIPOINT|POINT|MULTILINESTRING|LINESTRING|MULTIPOLYGON|POLYGON)\s*(\(.*\))\s*$/is.exec(text || '');
  if (!m)
    return null;
  const kind = m[1].toUpperCase();
  // The nested lists of the body: a list of coordinates is an array of [lon, lat]
  let pos = 0;
  const body = m[2];
  function list()
  {
    if (body[pos] !== '(')
      return null;
    pos++;
    const items = [];
    const space = () => {
      while (/\s/.test(body[pos] || ''))
        pos++;
    };
    for (;;)
    {
      space();
      if (body[pos] === '(')
      {
        const sub = list();
        if (!sub)
          return null;
        items.push(sub);
      }
      else
      {
        const c = /^(-?\d+(?:\.\d*)?(?:[eE][-+]?\d+)?)\s+(-?\d+(?:\.\d*)?(?:[eE][-+]?\d+)?)/.exec(body.slice(pos));
        if (!c)
          return null;
        items.push([Number(c[1]), Number(c[2])]);
        pos += c[0].length;
      }
      space();
      if (body[pos] === ',')
        pos++;
      else if (body[pos] === ')')
      {
        pos++;
        return items;
      }
      else
        return null;
    }
  }
  const tree = list();
  if (!tree || body.slice(pos).trim() !== '')
    return null;
  const isPoint = (x) => Array.isArray(x) && typeof x[0] === 'number';
  const points = (x) => Array.isArray(x) && x.length > 0 && x.every(isPoint);
  const parts = [];
  if (kind === 'POINT' && tree.length === 1 && isPoint(tree[0]))
    parts.push({kind: 'point', points: tree});
  else if (kind === 'MULTIPOINT' && tree.length > 0)
  {
    // MULTIPOINT(1 2,3 4) or MULTIPOINT((1 2),(3 4))
    const flat = tree.map((x) => (isPoint(x) ? x : points(x) && x.length === 1 ? x[0] : null));
    if (flat.some((x) => !x))
      return null;
    flat.forEach((p) => parts.push({kind: 'point', points: [p]}));
  }
  else if (kind === 'LINESTRING' && points(tree) && tree.length >= 2)
    parts.push({kind: 'line', points: tree});
  else if (kind === 'MULTILINESTRING' && tree.length > 0 && tree.every((l) => points(l) && l.length >= 2))
    tree.forEach((l) => parts.push({kind: 'line', points: l}));
  else if (kind === 'POLYGON' && tree.length > 0 && tree.every((r) => points(r) && r.length >= 4))
    parts.push({kind: 'polygon', rings: tree});
  else if (kind === 'MULTIPOLYGON' && tree.length > 0 &&
           tree.every((p) => Array.isArray(p) && p.length > 0 && p.every((r) => points(r) && r.length >= 4)))
    tree.forEach((p) => parts.push({kind: 'polygon', rings: p}));
  else
    return null;
  if (parts.some((p) => (p.points || p.rings.flat()).some(([lon, lat]) => !isFinite(lon) || !isFinite(lat))))
    return null;
  return {type: 'wkt', kind, parts};
}

// The longitudes and latitudes of a WKT selection
const wktPoints = (sel) => sel.parts.flatMap((p) => p.points || p.rings.flat());

// A selection of another location type as WKT, '' if it has none: a path as the MULTIPOINT
// of its marked points, or as a LINESTRING when the points are taken along it, a box or a
// radius as a POLYGON
function selectionWkt(sel, marked)
{
  const xy = ([lon, lat]) => `${round(lon, 5)} ${round(lat, 5)}`;
  if (!sel)
    return '';
  if (sel.type === 'point')
    return `POINT(${xy([sel.lon, sel.lat])})`;
  if (sel.type === 'path')
    return marked ? `MULTIPOINT(${sel.points.map(xy).join(',')})` :
                    `LINESTRING(${sel.points.map(xy).join(',')})`;
  if (sel.type === 'box')
  {
    const [w, so, e, n] = sel.bbox;
    return `POLYGON((${[[w, so], [e, so], [e, n], [w, n], [w, so]].map(xy).join(',')}))`;
  }
  if (sel.type === 'circle')
  {
    const ring = [];
    for (let b = 0; b <= 360; b += 10)
      ring.push(destination(sel.lon, sel.lat, sel.km, b % 360));
    return `POLYGON((${ring.map(xy).join(',')}))`;
  }
  return '';
}

// A selection of the map as the text of the location field (see parseSelection)
function formatSelection(sel)
{
  if (sel.type === 'path')
    return sel.points.map((p) => p.join(',')).join(',');
  if (sel.type === 'point')
    return `${sel.lon},${sel.lat}`;
  if (sel.type === 'circle')
    return `${sel.lon},${sel.lat}:${sel.km}`;
  return sel.bbox.join(',');
}

// Whether a producer covers a selection. Producers without an area are kept.
function covers(producer, sel)
{
  const area = selectionBox(sel);
  if (!area || !producer.bbox)
    return true;
  return boxesIntersect(producer.bbox, area);
}

// The TimeSeries module: parameters and producers narrowing each other, model details (runs,
// geometries, levels, ensembles, observation sensors and quality), times, locations of many
// kinds (places, points, radii, boxes, paths, keywords, stations), the output options, and the
// results as profiles, soundings, sections, charts, maps, tables and trees. The choices are kept
// in state, which the page keeps in its address (save()).
function timeSeriesModule(app, api, state, save)
{
  const rootPath = api.root;
  const view = el('div', {class: 'module'});
  const status = el('p', {class: 'muted'}, L('Loading the producers…'));
  view.append(status);

  fetchJson(`${app.base}${rootPath.replace(/^\//, '')}/api/producers`)
      .then((doc) => build(buildIndex(doc)))
      .catch((e) => {
        status.textContent = L`The producers could not be loaded: ${e.message}`;
      });

  // Builds the module once the producer listing has been loaded
  function build(index)
  {
    view.replaceChildren();
    const s = state;
    // Typing stores the value at once and redraws the request a moment later
    const refreshSoon = debounce(() => refresh(false), 300);
    s.params = s.params || [];

    // --- What: parameters and model --------------------------------
    const chips = el('div', {class: 'chips'});
    const modelCard = el('div', {class: 'model-card'});
    const candidatesNote = el('p', {class: 'muted small'});

    const dataParams = () => s.params.filter((p) => !META.has(p));
    const selection = () => parseSelection(s.locmode, s.location);
    // The producers having all the chosen parameters and covering the selection
    const candidates = () => {
      const wanted = dataParams();
      const sel = selection();
      return Array.from(index.producers.values())
          .filter((p) => wanted.every((w) => p.parameterSet.has(w)) && covers(p, sel));
    };
    // A link without a source, or with a source the producer does not have, picks the first
    // source that has the producer. Names are compared ignoring case, as the server does.
    const producer = () => {
      if (!s.producer)
        return undefined;
      const exact = index.producers.get(producerKey(s.source, s.producer));
      if (exact)
        return exact;
      const name = s.producer.toLowerCase();
      const all = Array.from(index.producers.values()).filter((p) => p.name.toLowerCase() === name);
      return all.find((p) => p.source === s.source) || all[0];
    };
    // A link naming the producer differently from the listing is corrected to the listed one
    {
      const p = producer();
      if (p && (p.name !== s.producer || p.source !== (s.source || p.source)))
      {
        s.producer = p.name;
        s.source = p.source;
      }
    }

    // --- Model details: runs, geometries, levels and ensemble members ---------------
    // The details of grid producers are loaded when one is chosen
    const details = new Map();  // producer key @ run -> details, null while loading
    const detailsKey = (p) => `${p.key}@${s.run || ''}`;
    // The details of the chosen grid producer, null until loaded
    const gridDetails = () => {
      const p = producer();
      return p && p.source === 'grid' ? details.get(detailsKey(p)) || null : null;
    };
    // The stations of an observation producer or point querydata (by WMO number), [] until
    // loaded
    const stationType = () => {
      const p = producer();
      return p && p.source === 'observation' ? 'fmisid' : 'qdstation';
    };
    const stationMode = () => (stationType() === 'fmisid' ? 'fmisid' : 'wmo');
    // The stations of the chosen producer
    const stations = () => {
      const p = producer();
      const d = p && p.source !== 'grid' ? details.get(detailsKey(p)) : null;
      return d && d.locations ? d.locations.filter((l) => l.type === stationType()) : [];
    };
    // The stations operating at some time of the chosen period
    const stationsInPeriod = () => {
      const start = s.start ? new Date(`${s.start}Z`) : null;
      const end = s.end ? new Date(`${s.end}Z`) : null;
      return stations().filter(
          (l) => (!end || !l.start || new Date(l.start) <= end) &&
              (!start || !l.end || new Date(l.end) >= start));
    };
    // The ids of the chosen stations
    const chosenStations = () =>
        (s.locmode === stationMode() ?
             String(s.location || '').split(',').map((x) => x.trim()).filter(Boolean) :
             []);
    // Adds a station to the chosen ones, or removes it
    function toggleStation(id)
    {
      const chosen = chosenStations();
      s.locmode = stationMode();
      s.location =
          (chosen.includes(id) ? chosen.filter((x) => x !== id) : [...chosen, id]).join(',');
      refresh();
    }
    // Loads the details of the chosen producer once (<ts_url>/api/producers/{name}), refreshing the
    // form when they arrive
    function loadDetails()
    {
      const p = producer();
      if (!p || !['grid', 'observation', 'querydata'].includes(p.source) ||
          details.has(detailsKey(p)))
        return;
      const key = detailsKey(p);
      details.set(key, null);
      const query = queryString(
          [['source', p.source], ['origintime', p.source === 'grid' ? s.run || '' : '']]);
      fetchJson(`${app.base}${rootPath.replace(/^\//, '')}/api/producers/${
                    encodeURIComponent(p.name)}?${query}`)
          .then((doc) => {
            details.set(key, doc);
            refresh(false);
            if (pendingSounding)
              startSounding(pendingSounding);
          })
          .catch((e) => {
            details.set(key, {error: e.message});
            refresh(false);
            if (pendingSounding)
              startSounding(pendingSounding);
          });
    }

    // The level types of the chosen parameters: [{id, type, unit, values: [{value, key}]}],
    // where the value is the one of the levels option and the key the one of a parameter key
    function levelTypes()
    {
      const p = producer();
      if (!p)
        return [];
      if (p.source !== 'grid')
        return (p.levels || [])
            .filter((l) => l.values.length > 1)
            .map((l) => ({
                   id: l.type,
                   type: l.type,
                   unit: l.unit || '',
                   values: l.values.map((v) => ({value: String(v), key: String(v)}))
                 }));
      const d = gridDetails();
      if (!d || !d.parameters)
        return [];
      const types = new Map();
      for (const name of dataParams())
      {
        const info = d.parameters[name];
        for (const l of (info && info.levels) || [])
        {
          const id = String(l.levelId);
          if (!types.has(id))
            types.set(id, {id, type: l.type, unit: l.unit || '', values: new Map()});
          l.values.forEach((v, i) => types.get(id).values.set(String(v), String(l.keys[i])));
        }
      }
      return Array.from(types.values())
          .map(
              (x) => Object.assign(
                  x, {values: Array.from(x.values, ([value, key]) => ({value, key}))}))
          .filter((x) => x.values.length > 1 || types.size > 1);
    }
    // The chosen level type, the first one by default
    const chosenLevelType = () => {
      const types = levelTypes();
      return types.find((x) => x.id === s.leveltype) || types[0] || null;
    };
    // The chosen levels of that type, none meaning all of them
    const chosenLevels = () => {
      const type = chosenLevelType();
      const wanted = new Set(String(s.levels || '').split(',').filter(Boolean));
      return type ? type.values.filter((v) => wanted.has(v.value)) : [];
    };

    // The forecast types of the chosen grid parameters: [{type, name, description, members}]
    function forecastTypes()
    {
      const d = gridDetails();
      if (!d || !d.parameters)
        return [];
      const types = new Map();
      for (const name of dataParams())
      {
        const info = d.parameters[name];
        for (const f of (info && info.forecasts) || [])
        {
          if (!types.has(f.type))
            types.set(f.type, Object.assign({}, f, {members: new Set()}));
          f.members.forEach((m) => types.get(f.type).members.add(m));
        }
      }
      return Array.from(types.values())
          .map((f) => Object.assign(f, {members: Array.from(f.members).sort((a, b) => a - b)}))
          .sort((a, b) => a.type - b.type);
    }
    // Ensemble choices are offered when there is more than one forecast to choose from
    const hasEnsemble = () => {
      const types = forecastTypes();
      return types.length > 1 || types.some((f) => f.members.length > 1);
    };
    // The chosen forecast type of an ensemble, by default the one with the most members
    const chosenForecast = () => {
      if (!hasEnsemble())
        return null;
      const types = forecastTypes();
      return types.find((f) => String(f.type) === s.forecast) ||
          types.reduce((a, b) => (b.members.length > a.members.length ? b : a));
    };
    // The chosen members of the forecast, all of them by default
    const chosenMembers = () => {
      const f = chosenForecast();
      if (!f)
        return [];
      const list = s.members ? parseNumberList(s.members, f.members) : f.members;
      return list || [];
    };

    // The sensors of observations: the numbers chosen, empty for the default sensor; null if the
    // text is not a list
    const isObservation = () => {
      const p = producer();
      return !!p && p.source === 'observation';
    };
    const SENSOR_NUMBERS = Array.from({length: 1000}, (_, i) => i);
    // The chosen sensors of observations, none for the default sensor
    const chosenSensors = () =>
        (isObservation() && s.sensors ? parseNumberList(s.sensors, SENSOR_NUMBERS) : []);
    // The data_quality option of observations: the default filter, all values, or a filter of the
    // codes
    const qualityOption = () => {
      if (!isObservation())
        return '';
      if (s.quality === 'all')
        return 'ge 0';
      return s.quality === 'custom' ? s.qfilter || '' : '';
    };

    // At most this many parameter keys in a request
    const MAX_KEYS = 200;

    // The parameter keys of a grid request: [{key, name, variant}], or null when the plain
    // parameter names do
    function gridColumns()
    {
      const p = producer();
      const d = gridDetails();
      if (!p || p.source !== 'grid' || !d || !d.parameters)
        return null;
      const type = chosenLevelType();
      const levels = chosenLevels();
      const forecast = chosenForecast();
      const members = chosenMembers();
      if (levels.length === 0 && !forecast && !s.geometry)
        return null;
      const out = [];
      for (const name of dataParams())
      {
        const info = d.parameters[name];
        if (!info)
        {
          out.push({key: name, name, variant: ''});
          continue;
        }
        // The chosen levels the parameter has, or its only level
        const own = type ? (info.levels || []).find((l) => String(l.levelId) === type.id) : null;
        let lv = [{levelId: '', key: '', label: ''}];
        if (own && levels.length > 0)
          lv = levels.filter((v) => own.values.map(String).includes(v.value))
                   .map((v) => ({
                          levelId: own.levelId,
                          key: v.key,
                          value: Number(v.value),
                          label: `${v.value}${own.unit ? ' ' + own.unit : ''}`
                        }));
        else if ((info.levels || []).length === 1 && info.levels[0].keys.length === 1)
          lv = [{levelId: info.levels[0].levelId, key: String(info.levels[0].keys[0]), label: ''}];
        // The chosen members the parameter has
        let mv = [{type: '', number: '', label: ''}];
        const own_forecast =
            forecast ? (info.forecasts || []).find((f) => f.type === forecast.type) : null;
        if (own_forecast)
          mv = members.filter((m) => own_forecast.members.includes(m))
                   .map((m) => ({
                          type: own_forecast.type,
                          number: m,
                          label: own_forecast.members.length > 1 ? L`member ${m}` : ''
                        }));
        for (const l of lv)
          for (const m of mv)
            out.push({
              key: [info.name, d.producer, s.geometry || '', l.levelId, l.key, m.type, m.number]
                       .join(':')
                       .replace(/:+$/, ''),
              name,
              variant: [levels.length > 1 ? l.label : '', m.label].filter(Boolean).join(' · '),
              // The level of a key is the level of its rows; the member alone names the series
              level: levels.length > 1 ? l.value : undefined,
              levelLabel: levels.length > 1 ? l.label : undefined,
              series: levels.length > 1 ? m.label : undefined,
              member: m.label ? m.number : undefined,
              levelFixed: l.key !== ''
            });
      }
      return out.slice(0, MAX_KEYS);
    }

    // The data columns of the request: [{column, name, variant}], where the column is what is
    // requested (a parameter or grid key, maybe inside a time function and an area statistic)
    // and the name and variant what it stands for
    function dataColumns()
    {
      const grid = gridColumns();
      const sensors = chosenSensors() || [];
      const out = [];
      for (const name of dataParams())
      {
        const tf = timeFunction(name);
        // The server names the column of a sensor by appending its number
        let bases = [{key: name, variant: '', suffix: ''}];
        if (grid)
          bases = grid.filter((c) => c.name === name);
        else if (sensors.length > 0)
          bases = sensors.map(
              (n) => ({key: `${name}(:${n})`, variant: `sensor ${n}`, suffix: `_#${n}`}));
        // One value of an area is the function of the area of the value of each point
        const af = areaAggregate() ? areaFunction(name).fn : '';
        for (const b of bases)
        {
          const timed = tf ? tf.expression(b.key) : b.key;
          const column = af ? statisticParameter(af, timed) : timed;
          const suffix = b.suffix || '';
          if (serverStatistics())
            for (const [, , fn] of AREA_STATISTICS)
              out.push({
                column: statisticParameter(fn, column),
                response: statisticParameter(fn, column) + suffix,
                name: statisticParameter(fn, name),
                variant: b.variant,
                level: b.level,
                levelLabel: b.levelLabel,
                series: b.series,
                member: b.member
              });
          else
            out.push({
              column,
              response: column + suffix,
              name,
              variant: b.variant,
              level: b.level,
              levelLabel: b.levelLabel,
              series: b.series,
              member: b.member
            });
        }
      }
      return out;
    }

    // The quality codes of observations, shown in the table
    function qualityColumns()
    {
      if (!isObservation() || s.qc !== '1' || serverStatistics() || areaAggregate())
        return [];
      const sensors = chosenSensors() || [];
      return dataParams().flatMap((name) => {
        if (sensors.length === 0)
          return [
            {column: `qc_${name}`, response: `qc_${name}_qc`, name: `${name} quality`, variant: ''}
          ];
        return sensors.map((n) => ({
                             column: `${name}(:${n}:qc)`,
                             response: `${name}(:${n}:qc)_#${n}_qc`,
                             name: `${name} quality`,
                             variant: `sensor ${n}`
                           }));
      });
    }

    // What the columns of the response stand for, when they are not named by the parameter
    // The details of the grid data of each data column: @code-N, where N is the index of the
    // column in the parameter list
    function gridDetailColumns(params, columns)
    {
      const p = producer();
      const codes =
          String(s.details || '').split(',').filter((c) => GRID_DETAILS.some((d) => d[0] === c));
      if (!p || p.source !== 'grid' || codes.length === 0)
        return [];
      const out = [];
      params.forEach((param, index) => {
        const c = columns.find((x) => x.column === param);
        if (!c)
          return;
        for (const code of codes)
        {
          const def = GRID_DETAILS.find((d) => d[0] === code);
          out.push({
            column: `@${code}-${index}`,
            response: `@${code}-${index}`,
            name: `${c.name} ${def[1]}`,
            variant: c.variant
          });
        }
      });
      return out;
    }

    // The response columns that stand for a chosen parameter at some level or member, for
    // splitVariants()
    function variantColumns()
    {
      const columns = new Map();
      const requested = (requestParams().find(([k]) => k === 'param') || [])[1] || '';
      const details = gridDetailColumns(String(requested).split(','), dataColumns());
      for (const c of [...dataColumns(), ...qualityColumns(), ...details])
        if (c.response !== c.name || c.variant)
          columns.set(c.response, {
            name: c.name,
            variant: c.variant,
            level: c.level,
            levelLabel: c.levelLabel,
            series: c.series,
            member: c.member
          });
      return columns;
    }

    const paramSearch = autocomplete({
      label: L('Parameter'),
      placeholder: L('Type a parameter name or description, e.g. temp'),
      search: (text) => {
        const q = text.toLowerCase();
        const p = producer();
        const out = [];
        for (const entry of index.parameters.values())
        {
          if (s.params.includes(entry.key))
            continue;
          if (p && !p.parameterSet.has(entry.key))
            continue;
          const score = matchScore(q, entry.name, `${entry.description} ${entry.key}`);
          if (score === null)
            continue;
          out.push({
            score,
            key: entry.key,
            label: entry.name,
            detail: [entry.description, entry.unit && `[${entry.unit}]`].filter(Boolean).join(' '),
            badge: p ?
                null :
                counted(entry.producers.length, L('producer'), L('producers'))
          });
        }
        for (const [key, desc] of META)
        {
          if (s.params.includes(key))
            continue;
          const score = matchScore(q, key, desc);
          if (score !== null && q)
            out.push({
              score: score + 0.5,
              key,
              label: key,
              detail: desc,
              badge: 'meta',
              badgeKind: 'meta'
            });
        }
        out.sort((a, b) => a.score - b.score || a.label.localeCompare(b.label));
        return out;
      },
      select: (item) => {
        s.params.push(item.key);
        refresh();
      },
      empty: (text) => producer() ? L`${s.producer} has no parameter matching "${text}"` :
                                    L`No parameter matches "${text}"`
    });

    const modelSearch = autocomplete({
      label: L('Producer'),
      placeholder: L('Type a model or producer name'),
      search: (text) => {
        const q = text.toLowerCase();
        return candidates()
            .map(
                (p) =>
                    ({p, score: matchScore(q, p.name, `${p.title || ''} ${p.description || ''}`)}))
            .filter((x) => x.score !== null)
            .sort((a, b) => a.score - b.score || a.p.name.localeCompare(b.p.name))
            .map(({p}) => ({
                   key: p.key,
                   label: p.name,
                   badge: p.source,
                   badgeKind: p.source,
                   detail: [p.title && p.title !== p.name ? p.title : '', timeRange(p)]
                               .filter(Boolean)
                               .join(' · ')
                 }));
      },
      select: (item) => {
        setProducer(item.key);
      },
      empty: () => selection() ? L('No producer has all the chosen parameters in the selected area') :
                                 L('No producer has all the chosen parameters'),
      limit: 200
    });

    // The period of the data of a producer for display
    function timeRange(p)
    {
      const t = p.time || {};
      if (!t.start || !t.end)
        return '';
      return `${formatTime(new Date(t.start))} – ${formatTime(new Date(t.end))}`;
    }

    // Chooses a producer; its details are loaded, and observations are chosen by station
    function setProducer(key)
    {
      const p = index.producers.get(key);
      s.producer = p ? p.name : '';
      s.source = p ? p.source : '';
      resetDetails();
      // Observations are chosen by station
      if (p && p.source === 'observation' && !s.location)
        s.locmode = 'fmisid';
      // Parameters the producer does not have are dropped
      if (p)
        s.params = s.params.filter((x) => META.has(x) || p.parameterSet.has(x));
      defaultTimes(true);
      refresh();
      fitMap();
    }

    // Forgets the choices that belong to the previous producer
    function resetDetails()
    {
      for (const k
               of ['run',
                   'geometry',
                   'leveltype',
                   'levels',
                   'forecast',
                   'members',
                   'sensors',
                   'quality',
                   'qfilter',
                   'qc',
                   'details'])
        delete s[k];
    }

    // The choices of the model: shown only when there is something to choose
    const runSelect = el('select', {'aria-label': L('Model run')});
    const geometrySelect = el('select', {'aria-label': L('Geometry')});
    const levelTypeSelect = el('select', {'aria-label': L('Level type')});
    const levelButtons = el('div', {class: 'level-buttons', role: 'group', 'aria-label': L('Levels')});
    const levelNote = el('p', {class: 'muted small'});
    const forecastSelect = el('select', {'aria-label': L('Forecast')});
    const membersInput = el(
        'input',
        {type: 'text', 'aria-label': L('Members'), spellcheck: 'false', placeholder: 'e.g. 1-10, 15'});
    const membersNote = el('p', {class: 'muted small'});
    const detailsNote = el('p', {class: 'muted small'});
    // Observations: the sensors and the quality of the values
    const sensorsInput = el('input', {
      type: 'text',
      'aria-label': L('Sensors'),
      spellcheck: 'false',
      placeholder: L('the default sensor, or e.g. 1, 11')
    });
    const sensorsNote = el('p', {class: 'muted small'});
    const qualitySelect = el('select', {'aria-label': L('Quality')}, [
      ['', L('The default filter of the server')],
      ['all', L('All values, whatever their quality')],
      ['custom', L('A filter of the quality codes')]
    ].map(([v, label]) => el('option', {value: v}, label)));
    const qualityInput = el('input', {
      type: 'text',
      'aria-label': L('Quality filter'),
      spellcheck: 'false',
      placeholder: L('e.g. le 1, or in 0,1')
    });
    const qualityCodes = el('input', {type: 'checkbox', 'aria-label': L('Quality codes')});
    // The sensors of the chosen stations, listed by the observation engine
    const sensorsList = el('div', {class: 'sensors-list'});
    const sensorsRow =
        el('div', null, el('label', null, L('Sensors'), sensorsInput), sensorsNote, sensorsList);
    const sensorDetails = new Map();
    // Loads the sensors of the chosen stations and parameters once, and shows them
    function renderSensors()
    {
      const p = producer();
      const ids = chosenStations().slice(0, 10);
      const params = dataParams();
      if (!p || ids.length === 0 || params.length === 0)
      {
        sensorsList.replaceChildren(ids.length === 0 ?
            el('p', {class: 'muted small'}, L('Choose stations to see their sensors')) : '');
        return;
      }
      const query = queryString([
        ['source', 'observation'],
        ['fmisid', ids.join(',')],
        ['param', params.join(',')],
        ['starttime', fromInput(s.start)],
        ['endtime', fromInput(s.end)]
      ]);
      const key = `${p.name}?${query}`;
      if (!sensorDetails.has(key))
      {
        sensorDetails.set(key, null);
        fetchJson(`${app.base}${rootPath.replace(/^\//, '')}/api/producers/${encodeURIComponent(p.name)}?${query}`)
            .then((doc) => {
              sensorDetails.set(key, doc.sensors || {available: false, note: L('No sensors listed')});
              refresh(false);
            })
            .catch((e) => {
              sensorDetails.set(key, {available: false, note: e.message});
              refresh(false);
            });
      }
      const found = sensorDetails.get(key);
      if (!found)
      {
        sensorsList.replaceChildren(el('p', {class: 'muted small'}, L('Loading the sensors…')));
        return;
      }
      if (!found.available)
      {
        sensorsList.replaceChildren(el('p', {class: 'muted small'}, found.note || L('No sensors listed')));
        return;
      }
      const chosen = new Set((s.sensors ? parseNumberList(s.sensors, SENSOR_NUMBERS) : []) || []);
      const toggle = (n) => {
        if (chosen.has(n))
          chosen.delete(n);
        else
          chosen.add(n);
        s.sensors = Array.from(chosen).sort((a, b) => a - b).join(',');
        refresh();
      };
      const end = s.end ? new Date(fromInput(s.end)).getTime() : Date.now();
      const lines = [];
      for (const param of params)
        for (const id of ids)
        {
          const items = found.list.filter(
              (x) => String(x.fmisid) === String(id) &&
                  (x.parameters || []).some((n) => n.toLowerCase() === param.toLowerCase()));
          // A sensor may have several configurations: one button each sensor
          const sensors = new Map();
          for (const x of items)
            if (!sensors.has(x.sensor) || (x.last || '') > (sensors.get(x.sensor).last || ''))
              sensors.set(x.sensor, x);
          if (sensors.size === 0)
            continue;
          const name = (stations().find((st) => String(st.id) === String(id)) || {}).name || id;
          lines.push(el('div', {class: 'row sensors-line'},
                        el('span', {class: 'small'}, L`${param} at ${name}:`),
                        ...Array.from(sensors.values()).sort((a, b) => a.sensor - b.sensor).map((x) => {
                          // A sensor whose values ended before the period is marked
                          const ended = x.last && new Date(x.last).getTime() < end - 86400e3;
                          return el('button', {
                            type: 'button',
                            class: `small${chosen.has(x.sensor) ? '' : ' meta'}`,
                            'aria-pressed': chosen.has(x.sensor) ? 'true' : 'false',
                            'data-sensor': String(x.sensor),
                            title: [x.default ? L('The default sensor') : L('An additional sensor'),
                                    x.configuration_start ? `configured ${x.configuration_start.slice(0, 10)}` : '',
                                    x.last ? L`values until ${x.last.slice(0, 10)}` : '',
                                    x.values ? `${x.values} values` : ''].filter(Boolean).join(', '),
                            onclick: () => toggle(x.sensor)
                          }, `${x.sensor}${x.default ? ' (default)' : ''}${ended ? ` · until ${x.last.slice(0, 10)}` : ''}`);
                        }),
                        Array.from(sensors.values()).some((x) => x.default) ? '' :
                            el('span', {class: 'warn small'}, L('no default sensor: give one'))));
        }
      sensorsList.replaceChildren(...(lines.length ? lines :
          [el('p', {class: 'muted small'}, L('No sensors of the parameters at the stations in the period'))]));
    }
    const qualityRow =
        el('div',
           {class: 'row'},
           el('label', null, L('Quality'), qualitySelect),
           el('label', null, L('Filter'), qualityInput));
    const qualityCodesRow =
        el('label', {class: 'check'}, qualityCodes, L('Show the quality codes in the table'));
    const runRow = el('label', null, L('Model run'), runSelect);
    const geometryRow = el('label', null, L('Geometry'), geometrySelect);
    const levelTypeRow = el('label', null, L('Level type'), levelTypeSelect);
    const levelsRow = el('div', null, el('label', null, L('Levels')), levelButtons, levelNote);
    const diagramButtons =
        el('div', {class: 'level-buttons', role: 'group', 'aria-label': L('Sounding diagrams')});
    const diagramNote = el('p', {class: 'muted small'});
    const diagramsRow =
        el('div', null, el('label', null, L('Sounding diagrams')), diagramButtons, diagramNote);
    const forecastRow = el('label', null, L('Forecast'), forecastSelect);
    const membersRow = el('div', null, el('label', null, L('Members'), membersInput), membersNote);
    const detailsBox =
        el('div',
           {class: 'model-details'},
           runRow,
           geometryRow,
           levelTypeRow,
           levelsRow,
           diagramsRow,
           forecastRow,
           membersRow,
           sensorsRow,
           qualityRow,
           qualityCodesRow,
           detailsNote);
    sensorsInput.addEventListener('input', () => {
      s.sensors = sensorsInput.value.trim();
      refreshSoon();
    });
    qualitySelect.addEventListener('change', () => {
      s.quality = qualitySelect.value;
      refresh();
    });
    qualityInput.addEventListener('input', () => {
      s.qfilter = qualityInput.value.trim();
      refreshSoon();
    });
    qualityCodes.addEventListener('change', () => {
      s.qc = qualityCodes.checked ? '1' : '';
      refresh();
    });

    runSelect.addEventListener('change', () => {
      s.run = runSelect.value;
      refresh();
    });
    geometrySelect.addEventListener('change', () => {
      s.geometry = geometrySelect.value;
      refresh();
    });
    levelTypeSelect.addEventListener('change', () => {
      s.leveltype = levelTypeSelect.value;
      s.levels = '';
      refresh();
    });
    forecastSelect.addEventListener('change', () => {
      s.forecast = forecastSelect.value;
      s.members = '';
      refresh();
    });
    membersInput.addEventListener('input', () => {
      s.members = membersInput.value.trim();
      refreshSoon();
    });

    const options = (select, items, value) => {
      select.replaceChildren(
          ...items.map(([v, label]) => el('option', {value: v, selected: v === value}, label)));
      select.value = value;
    };

    // Shows the choices of the model that offer something to choose from: runs, geometries,
    // levels, ensembles, sensors and quality
    function renderDetails()
    {
      const p = producer();
      const d = gridDetails();
      loadDetails();
      // Runs and geometries of grid producers
      const runs = (d && d.runs) || [];
      runRow.hidden = runs.length < 2;
      if (!runRow.hidden)
        options(
            runSelect,
            [
              ['', L('The latest')],
              ...runs.map(
                  (r) =>
                      [r.origintime,
                       `${formatTime(new Date(r.origintime))}${
                           r.status === 'ready' ? '' : ` (${r.status})`}`])
            ],
            s.run || '');
      const geometries = (d && d.run && d.run.geometries) || [];
      geometryRow.hidden = geometries.length < 2;
      if (!geometryRow.hidden)
        options(
            geometrySelect,
            [
              ['', L('The default')],
              ...geometries.map((g) => [String(g.id), g.name ? `${g.name} (${g.id})` : g.id])
            ],
            s.geometry || '');
      // Levels
      const types = levelTypes();
      const type = chosenLevelType();
      levelTypeRow.hidden = types.length < 2;
      if (!levelTypeRow.hidden)
        options(
            levelTypeSelect,
            types.map((x) => [x.id, `${x.type}${x.unit ? ` (${x.unit})` : ''}`]),
            type ? type.id : '');
      levelsRow.hidden = !type;
      if (type)
      {
        const chosen = new Set(chosenLevels().map((v) => v.value));
        levelButtons.replaceChildren(...type.values.map(
            (v) =>
                el('button',
                   {
                     type: 'button',
                     class: 'small',
                     'aria-pressed': chosen.has(v.value) ? 'true' : 'false',
                     'data-level': v.value,
                     onclick: () => {
                       if (chosen.has(v.value))
                         chosen.delete(v.value);
                       else
                         chosen.add(v.value);
                       s.levels = type.values.filter((x) => chosen.has(x.value))
                                      .map((x) => x.value)
                                      .join(',');
                       refresh();
                     }
                   },
                   v.value)));
        // The members of an ensemble stored as levels are no levels to the user
        const levelWord = type.type === 'member' ? L('members') :
            type.type === 'pressure'             ? L('pressure levels') :
            type.type === 'hybrid'               ? L('model levels') :
            type.type === 'height'               ? L('height levels') :
            type.type === 'depth'                ? L('depth levels') :
                                                   L`${type.type} levels`;
        // A grid request without levels gets the default level of each parameter
        levelNote.textContent = chosen.size === 0 && p && p.source === 'grid' ?
            L`None chosen: the default level of each parameter. Choose ${levelWord} for a series of each.` :
            chosen.size === 0 ?
            L`None chosen: the values of all ${type.values.length} ${levelWord}` :
            L`${chosen.size} of ${type.values.length} ${levelWord}${
                type.unit ? ` (${type.unit})` : ''}, a series each`;
      }
      // Sounding diagrams of the levels, the missing parameters added when chosen
      diagramsRow.hidden = !(p && p.source !== 'observation' && soundingCapable(p));
      if (!diagramsRow.hidden)
      {
        const missingNames = soundingParameters(p);
        diagramButtons.replaceChildren(...THERMO_DIAGRAMS.map(
            ([type, label]) =>
                el('button',
                   {
                     type: 'button',
                     class: 'small',
                     'data-diagram': type,
                     onclick: () => startSounding(type)
                   },
                   label)));
        diagramNote.textContent = missingNames.length > 0 ?
            L`Adds ${missingNames.join(', ')} to the parameters and runs the request` :
            L('Runs the request and draws the soundings');
      }
      // Ensembles
      const forecasts = forecastTypes();
      const forecast = chosenForecast();
      forecastRow.hidden = !forecast;
      if (forecast)
        options(
            forecastSelect,
            forecasts.map(
                (f) =>
                    [String(f.type),
                     `${f.description || f.name || `type ${f.type}`}${
                         f.members.length > 1 ? ` (${f.members.length} members)` : ''}`]),
            String(forecast.type));
      membersRow.hidden = !forecast || forecast.members.length < 2;
      if (!membersRow.hidden)
      {
        if (document.activeElement !== membersInput)
          membersInput.value = s.members || '';
        membersInput.placeholder = L`all ${forecast.members.length} members, or e.g. ${
            forecast.members[0]}-${forecast.members[Math.min(9, forecast.members.length - 1)]}`;
        const list = s.members ? parseNumberList(s.members, forecast.members) : forecast.members;
        membersNote.textContent = list === null ?
            L('Not a list of members: numbers and ranges separated by commas') :
            L`${counted(list.length, L('member'), L('members'))}, a series each`;
      }
      // Observations
      const obs = isObservation();
      sensorsRow.hidden = !obs;
      qualityRow.hidden = !obs;
      qualityCodesRow.hidden = !obs;
      if (obs)
      {
        if (document.activeElement !== sensorsInput)
          sensorsInput.value = s.sensors || '';
        const sensors = s.sensors ? parseNumberList(s.sensors, SENSOR_NUMBERS) : [];
        sensorsNote.textContent = sensors === null ?
            L('Not a list of sensors: numbers and ranges separated by commas') :
            sensors.length === 0 ?
            L('The default sensor of each station') :
            L`${counted(sensors.length, L('sensor'), L('sensors'))}, a series each; a sensor without values gives no rows`;
        renderSensors();
        qualitySelect.value = s.quality || '';
        qualityInput.closest('label').hidden = s.quality !== 'custom';
        if (document.activeElement !== qualityInput)
          qualityInput.value = s.qfilter || '';
        qualityCodes.checked = s.qc === '1';
      }
      // What the data of the parameters is, and too large requests
      const notes = [];
      if (p && p.source === 'grid' && d === null)
        notes.push(L('Loading the model details…'));
      if (d && d.error)
        notes.push(L`The model details could not be loaded: ${d.error}`);
      for (const name of dataParams())
      {
        const info = d && d.parameters && d.parameters[name];
        if (info && info.aggregation)
          notes.push(`${name}: ${info.aggregation.description || info.aggregation.name}${
              info.aggregation.period ? ` (${info.aggregation.period} min)` : ''}`);
        if (info && info.processing)
          notes.push(`${name}: ${info.processing.description || info.processing.name}`);
      }
      const grid = gridColumns();
      if (grid && grid.length >= MAX_KEYS)
        notes.push(L`Only the first ${MAX_KEYS} combinations of levels and members are requested`);
      detailsNote.textContent = notes.join(' · ');
      detailsNote.hidden = notes.length === 0;
      detailsBox.hidden = [
        runRow,
        geometryRow,
        levelTypeRow,
        levelsRow,
        diagramsRow,
        forecastRow,
        membersRow,
        sensorsRow,
        detailsNote
      ].every((x) => x.hidden);
    }

    // --- When ------------------------------------------------------
    const startInput = timeInput({'aria-label': L('Start time (UTC)')});
    const endInput = timeInput({'aria-label': L('End time (UTC)')});
    const stepSelect = el('select', {'aria-label': L('Time step')}, [
      ['data', L('Times of the data')],
      ['10', L('10 minutes')],
      ['30', L('30 minutes')],
      ['60', L('1 hour')],
      ['180', L('3 hours')],
      ['360', L('6 hours')],
      ['720', L('12 hours')],
      ['1440', L('1 day')]
    ].map(([v, t]) => el('option', {value: v}, t)));
    const dataPeriod = el('span', {class: 'muted small'});

    // The default period: the data period of the producer, or a recent part of it for observations
    function defaultTimes(force)
    {
      const p = producer();
      if (!p || !p.time || !p.time.start || !p.time.end)
        return;
      if (!force && s.start && s.end)
        return;
      const first = new Date(p.time.start);
      const last = new Date(p.time.end);
      const now = new Date();
      now.setUTCMinutes(0, 0, 0);
      let start;
      let end;
      if (p.source === 'observation')
      {
        end = last < now ? last : now;
        start = new Date(Math.max(first.getTime(), end.getTime() - 24 * 3600e3));
      }
      else
      {
        start = (now > first && now < last) ? now : first;
        end = new Date(Math.min(last.getTime(), start.getTime() + 48 * 3600e3));
      }
      s.start = toInput(start);
      s.end = toInput(end);
      s.timestep = p.source === 'observation' ? '60' : 'data';
    }

    // A button setting the period
    // A button of a period, known by its English name
    const preset = (label, fn) =>
        el('button',
           {
             type: 'button',
             class: 'small',
             'data-preset': label,
             onclick: () => {
               fn();
               refresh();
             }
           },
           L(label));
    const presets = el('div', {class: 'row'}, preset('Whole data', () => {
                         const p = producer();
                         if (p && p.time)
                         {
                           s.start = toInput(new Date(p.time.start));
                           s.end = toInput(new Date(p.time.end));
                         }
                       }), preset('Next 24 h', () => {
                         const now = new Date();
                         now.setUTCMinutes(0, 0, 0);
                         s.start = toInput(now);
                         s.end = toInput(new Date(now.getTime() + 24 * 3600e3));
                       }), preset('Last 24 h', () => {
                         const now = new Date();
                         now.setUTCMinutes(0, 0, 0);
                         s.end = toInput(now);
                         s.start = toInput(new Date(now.getTime() - 24 * 3600e3));
                       }), preset('Last week', () => {
                         const now = new Date();
                         now.setUTCMinutes(0, 0, 0);
                         s.end = toInput(now);
                         s.start = toInput(new Date(now.getTime() - 7 * 24 * 3600e3));
                       }), preset('Last month', () => {
                         const now = new Date();
                         now.setUTCMinutes(0, 0, 0);
                         const start = new Date(now);
                         start.setUTCMonth(start.getUTCMonth() - 1);
                         s.end = toInput(now);
                         s.start = toInput(start);
                       }));

    startInput.addEventListener('change', () => {
      s.start = startInput.value;
      refresh();
    });
    endInput.addEventListener('change', () => {
      s.end = endInput.value;
      refresh();
    });
    stepSelect.addEventListener('change', () => {
      s.timestep = stepSelect.value;
      refresh();
    });

    // --- Values over a period: the time functions, chosen per kind of parameter -----
    const PERIODS = [
      ['60', L('1 hour')],
      ['180', L('3 hours')],
      ['360', L('6 hours')],
      ['720', L('12 hours')],
      ['1440', L('24 hours')]
    ];
    const aggModes =
        el('div', {class: 'segmented', role: 'radiogroup', 'aria-label': L('Values over time')});
    for (const [value, label] of [['', L('As in the data')], ['period', L('Over a period')]])
      aggModes.append(
          el('button',
             {
               type: 'button',
               role: 'radio',
               'data-agg': value,
               onclick: () => {
                 s.agg = value;
                 if (value)
                   periodChanged();
                 refresh();
               }
             },
             label));
    const periodSelect =
        el('select',
           {'aria-label': L('Period')},
           PERIODS.map(([v, label]) => el('option', {value: v}, label)));
    const windowSelect = el('select', {'aria-label': L('Period placement')}, [
      ['end', L('ending at each time')],
      ['centre', L('centred on each time')],
      ['start', L('starting at each time')]
    ].map(([v, label]) => el('option', {value: v}, label)));
    const functionSelect = el('select', {'aria-label': L('Function')}, [
      ['auto', L('Recommended for each parameter')],
      ...TIME_FUNCTIONS.map(([v, label]) => [v, label])
    ].map(([v, label]) => el('option', {value: v}, label)));
    const percentileInput = el(
        'input', {type: 'number', min: 0, max: 100, step: 1, size: 4, 'aria-label': L('Percentile')});
    const percentileLabel = el('label', null, L('Percentile'), percentileInput);
    const aggSummary = el('ul', {class: 'agg-summary small'});
    const aggDetails = el(
        'div',
        {class: 'agg-details'},
        el('div',
           {class: 'row'},
           el('label', null, L('Period'), periodSelect),
           el('label', null, L('Placement'), windowSelect)),
        el('div', {class: 'row'}, el('label', null, L('Function'), functionSelect), percentileLabel),
        aggSummary);
    const aggRow = el('div', null, el('label', null, L('Values over time')), aggModes, aggDetails);

    // A daily maximum is wanted once a day: the time step follows the period
    function periodChanged()
    {
      const period = Number(s.period || 60);
      if (!s.timestep || s.timestep === 'data' || Number(s.timestep) < period)
        s.timestep = String(period);
    }
    periodSelect.addEventListener('change', () => {
      s.period = periodSelect.value;
      periodChanged();
      refresh();
    });
    windowSelect.addEventListener('change', () => {
      s.window = windowSelect.value;
      refresh();
    });
    functionSelect.addEventListener('change', () => {
      s.fn = functionSelect.value;
      refresh();
    });
    percentileInput.addEventListener('input', () => {
      s.pct = percentileInput.value;
      refreshSoon();
    });

    // The time step of the data of the producer in minutes, 0 if unknown
    const dataStep = () => {
      const p = producer();
      return p && p.time && p.time.step ? Number(p.time.step) : 0;
    };

    // The time function of a parameter: {fn, expression(base), text, warning}, or null for the
    // values as they are
    function timeFunction(name)
    {
      if (s.agg !== 'period')
        return null;
      const period = Number(s.period || 60);
      const step = dataStep();
      const entry = index.parameters.get(name);
      const d = gridDetails();
      const kind = parameterKind(name, entry, d && d.parameters ? d.parameters[name] : null);
      const choice = !s.fn || s.fn === 'auto' ? RECOMMENDED_FUNCTION[kind] : s.fn;
      const def = TIME_FUNCTIONS.find((f) => f[0] === choice) || TIME_FUNCTIONS[0];
      const fn = kind === 'direction' && def[3] ? def[3] : def[2];
      const pct =
          Math.min(100, Math.max(0, Number(s.pct === undefined || s.pct === '' ? 90 : s.pct)));
      const prefix = choice === 'percentile' ? `${fn}[${pct}]` : fn;
      // The aggregator includes both ends of the interval: a period ending at a time leaves out
      // the value at its start, so that 24 hourly values make a day
      const span = Math.max(0, period - (step > 0 && step < period ? step : 0));
      const window = s.window || 'end';
      const [before, after] = window === 'end' ? [span, 0] :
          window === 'start'                   ? [0, span] :
                                                 [Math.floor(period / 2), Math.floor(period / 2)];
      const label = (PERIODS.find((x) => x[0] === String(period)) || [null, `${period} min`])[1];
      const what = choice === 'percentile' ? L`${pct} % percentile` : def[1].toLowerCase();
      const placement = window === 'end' ? L('ending at') :
          window === 'start'             ? L('starting at') :
                                           L('centred on');
      const warnings = [];
      const warning = functionWarning(choice, kind);
      if (warning)
        warnings.push(`${name} ${warning}`);
      if (step > 0 && step >= period)
        warnings.push(
            L`the data has a value every ${step} min: a period of ${period} min has only one`);
      return {
        expression: (base) => `${prefix}(${base}/${before}m/${after}m)`,
        text: L`${name}: ${what} of the ${label} ${placement} each time${
            kind === 'state' ? '' : ` (${L(kind)})`}`,
        warnings
      };
    }

    // --- Where -----------------------------------------------------
    s.locmode = s.locmode || 'place';
    const locInput = el('input', {type: 'text', 'aria-label': L('Location'), spellcheck: 'false'});
    const locModes =
        el('div', {class: 'segmented', role: 'radiogroup', 'aria-label': L('Location type')});
    const showButton = el('button', {type: 'button', class: 'small'}, L('Show locations'));
    const stationNote = el('p', {class: 'muted small'});

    // Suggestions of place and area names: the places of the server whose names begin with the
    // text (<ts_url>/api/places), and the named areas of the server (<ts_url>/api/areas). A
    // server without them suggests nothing.
    const apiUrl = (path) => `${app.base}${rootPath.replace(/^\//, '')}/api/${path}`;
    const placeAnswers = new Map();  // pattern -> the places
    let areaNames = null;            // a promise of the names, once asked
    async function placeSuggestions(text)
    {
      // A radius (name:20) or an area (name,area) is no longer a name to complete
      if (text.length < 2 || /[:,]/.test(text))
        return [];
      const key = `${s.lang || ''}|${text.toLowerCase()}`;
      if (!placeAnswers.has(key))
        placeAnswers.set(
            key,
            fetchJson(`${apiUrl('places')}?${queryString([['pattern', text], ['lang', s.lang], ['max', '15']])}`)
                .then((doc) => doc.places || [])
                .catch(() => []));
      const places = await placeAnswers.get(key);
      // A name shared by several places is told apart by its area
      const count = new Map();
      places.forEach((p) => count.set(p.name, (count.get(p.name) || 0) + 1));
      return places.map((p) => ({
                          key: count.get(p.name) > 1 && p.area ? `${p.name},${p.area}` : p.name,
                          label: p.name,
                          detail: [p.area, p.population > 0 ? L`population ${p.population.toLocaleString()}` : '']
                                      .filter(Boolean)
                                      .join(' · ')
                        }));
    }
    async function areaSuggestions(text)
    {
      if (/:/.test(text))
        return [];
      if (!areaNames)
        areaNames = fetchJson(apiUrl('areas')).then((doc) => doc.areas || []).catch(() => []);
      const q = text.toLowerCase();
      return (await areaNames)
          .map((name) => ({name, score: matchScore(q, name, '')}))
          .filter((x) => x.score !== null)
          .sort((a, b) => a.score - b.score || a.name.localeCompare(b.name))
          .map(({name}) => ({key: name, label: name}));
    }
    locInput.classList.add('ac-input');
    const locSuggest = autocomplete({
      input: locInput,
      label: L('Location'),
      clearOnSelect: false,
      limit: 15,
      search: (text) => (s.locmode === 'place' ? placeSuggestions(text) :
                         s.locmode === 'area'  ? areaSuggestions(text) :
                                                 []),
      select: (item) => {
        locInput.value = item.key;
        s.location = item.key;
        refresh();
      }
    });
    locSuggest.element.classList.add('loc-suggest');
    const stationSearch = autocomplete({
      label: L('Station'),
      placeholder: L('Type a station name or id'),
      search: (text) => {
        const q = text.toLowerCase();
        const chosen = new Set(chosenStations());
        return stationsInPeriod()
            .filter((l) => !chosen.has(l.id))
            .map((l) => ({l, score: matchScore(q, l.name, `${l.id} ${l.wsi || ''}`)}))
            .filter((x) => x.score !== null)
            .sort((a, b) => a.score - b.score || a.l.name.localeCompare(b.l.name))
            .map(
                ({l}) =>
                    ({key: l.id, label: l.name, detail: `${l.id}${l.wsi ? ` · ${l.wsi}` : ''}`}));
      },
      select: (item) => toggleStation(item.key),
      empty: (text) => L`No station of the period matches "${text}"`,
      limit: 50
    });
    const areaNote = el('p', {class: 'warn small'});
    // The points on the map (the result of a request or the shown locations) belong to the
    // location they were for: a new selection or location type clears them
    const map = createMap(mapSettings(app.element), (shape) => {
      s.location = formatSelection(shape);
      map.setPoints([]);
      refresh();
    });
    for (const [mode, label] of locationModes())
    {
      locModes.append(
          el('button',
             {
               type: 'button',
               role: 'radio',
               'data-mode': mode,
               onclick: () => {
                 if (s.locmode === mode)
                   return;
                 // A point, path, box or radius drawn is kept as WKT
                 const wkt = mode === 'wkt' ? selectionWkt(selection(), s.pathpoints === 'marked') : '';
                 s.locmode = mode;
                 s.location = wkt;
                 map.setPoints([]);
                 // Each location type has its own default for the values of an area
                 s.areavalues = '';
                 refresh();
               }
             },
             label));
    }
    // A WKT geometry typed or pasted is shown on the map, which is zoomed to it
    const fitWktSoon = debounce(() => {
      const sel = selection();
      if (s.locmode === 'wkt' && sel)
        map.fit(selectionBox(sel));
    }, 600);
    locInput.addEventListener('input', () => {
      s.location = locInput.value.trim();
      refreshSoon();
      fitWktSoon();
    });

    // The locations of a place, keyword, station or area name on the map
    showButton.addEventListener('click', () => {
      showButton.disabled = true;
      // The locations do not depend on the data, so no producer: its times could be anywhere
      const url = `${rootPath}?${queryString([
        [locationOption(s.locmode), s.location],
        ['param', 'name,lat,lon,geoid,fmisid,wmo,elevation,region,country,tz'],
        ['timesteps', '1'],
        ['format', 'json'],
        ['lang', s.lang]
      ])}`;
      fetchJson(app.base + url.replace(/^\//, ''))
          .then((data) => {
            const points = mapPoints(flattenRows(data));
            areaNote.textContent = points.length === 0 ? L('No locations found.') : '';
            map.setPoints(points);
            map.fit(map.extent());
          })
          .catch((e) => {
            areaNote.textContent = e.message;
          })
          .finally(() => {
            showButton.disabled = false;
          });
    });

    // The points of rows with coordinates, one per location
    function mapPoints(rows)
    {
      const seen = new Map();
      for (const row of rows)
      {
        if (typeof row.lat !== 'number' || typeof row.lon !== 'number')
          continue;
        const key = `${row.lat},${row.lon}`;
        if (seen.has(key) || seen.size >= 10000)
          continue;
        const info = {};
        for (const k
                 of ['geoid',
                     'fmisid',
                     'wmo',
                     'lat',
                     'lon',
                     'elevation',
                     'region',
                     'country',
                     'tz',
                     'stationname',
                     'distance'])
          if (row[k] !== undefined)
            info[k] = row[k];
        seen.set(key, {lon: row.lon, lat: row.lat, label: row.name || key, info: info});
      }
      return Array.from(seen.values());
    }

    // Fits the map to what it shows, or to a WKT geometry, which is usually much smaller than
    // the area of the producer
    function fitMap()
    {
      const sel = s.locmode === 'wkt' ? selection() : null;
      const extent = sel ? selectionBox(sel) : map.extent();
      if (extent)
        map.fit(extent);
    }

    // --- Output ----------------------------------------------------
    // Areas: the statistics computed by the server, or every point
    // The default is the statistics for areas, every point for keywords and station lists
    const areaValuesOf = () =>
        s.areavalues ||
        (['keyword', 'fmisid', 'path'].includes(s.locmode) ? 'points' : 'statistics');
    const areaValues =
        el('div', {class: 'segmented', role: 'radiogroup', 'aria-label': L('Area values')});
    for (const [value, label] of [['statistics', L('Statistics')], ['aggregate', L('One value')],
                                  ['points', L('Every point')]])
      areaValues.append(
          el('button',
             {
               type: 'button',
               role: 'radio',
               'data-areavalues': value,
               onclick: () => {
                 s.areavalues = value;
                 refresh();
               }
             },
             label));
    // One value of the area for each parameter: the function of each, the recommended one
    // unless chosen (s.afn: name:function,...)
    const areaFunctionList = el('div', {class: 'area-functions'});
    const areaFunctionNote = el('ul', {class: 'agg-summary small'});
    const areaValuesRow = el('div',
                             null,
                             el('label', null, L('Values of an area')),
                             areaValues,
                             areaFunctionList,
                             areaFunctionNote);
    const chosenAreaFunctions = () => new Map(
        String(s.afn || '').split(',').filter((x) => x.includes(':')).map((x) => {
          const i = x.lastIndexOf(':');
          return [x.slice(0, i), x.slice(i + 1)];
        }));
    // The function over the area of a parameter: {choice, fn, kind, warning}
    function areaFunction(name)
    {
      const entry = index.parameters.get(name);
      const d = gridDetails();
      const kind = parameterKind(name, entry, d && d.parameters ? d.parameters[name] : null);
      const chosen = chosenAreaFunctions().get(name);
      const choice = AREA_FUNCTIONS.some((f) => f[0] === chosen) ? chosen : RECOMMENDED_AREA_FUNCTION[kind];
      const def = AREA_FUNCTIONS.find((f) => f[0] === choice) || AREA_FUNCTIONS[0];
      const warning = areaFunctionWarning(choice, kind);
      return {choice, fn: kind === 'direction' && def[3] ? def[3] : def[2], kind,
              warning: warning ? `${name} ${warning}` : ''};
    }
    // A choice of the function for each parameter
    function renderAreaFunctions()
    {
      const shown = areaAggregate();
      areaFunctionList.hidden = !shown;
      areaFunctionNote.hidden = !shown;
      if (!shown)
        return;
      areaFunctionList.replaceChildren(...dataParams().map((name) => {
        const f = areaFunction(name);
        const select = el('select', {'aria-label': L`Function of ${name}`, 'data-param': name},
                          AREA_FUNCTIONS.map(([v, label]) => el('option', {value: v}, label)));
        select.value = f.choice;
        select.addEventListener('change', () => {
          const chosen = chosenAreaFunctions();
          chosen.set(name, select.value);
          // A choice of the recommended function is not kept
          if (select.value === RECOMMENDED_AREA_FUNCTION[f.kind])
            chosen.delete(name);
          s.afn = Array.from(chosen).map(([k, v]) => `${k}:${v}`).join(',');
          refresh();
        });
        return [el('span', {class: 'small'}, name), select, el('code', {class: 'muted small'}, f.fn)];
      }).flat());
      areaFunctionNote.replaceChildren(
          ...dataParams().map((name) => areaFunction(name).warning).filter(Boolean).map(
              (w) => el('li', {class: 'warn'}, w)));
    }
    const pathChooser = pathPointsChooser(s, (rerender) => refresh(rerender));
    const flightChoice = flightChooser(s, () => refresh(), () => dataStep());
    // A meteogram of the chosen place with the parameters it needs
    const meteogram = meteogramButton((missing) => {
      s.params = Array.from(new Set([...s.params, ...missing]));
      s.tab = 'Meteogram';
      refresh();
      if (missingParts().length === 0)
        run();
    });

    // An area, radius or box, or several locations, whose values the server can aggregate
    const isArea = () => ['bbox', 'radius', 'area', 'path'].includes(s.locmode) ||
        (s.locmode === 'wkt' && !/^\s*POINT\b/i.test(s.location || '')) ||
        (s.locmode === 'place' && /:\s*\d/.test(s.location || '')) ||
        (s.locmode === 'keyword' && !!s.location) ||
        (s.locmode === 'fmisid' && (s.location || '').includes(','));
    // Keywords and station lists are grouped into one area for the statistics and the values
    // of the area
    const groupLocations = () =>
        (serverStatistics() || areaAggregate()) && ['keyword', 'fmisid'].includes(s.locmode);
    const serverStatistics = () => isArea() && areaValuesOf() === 'statistics';
    // One value of the area for each parameter at each time, computed by the server
    const areaAggregate = () => isArea() && areaValuesOf() === 'aggregate';

    // Extra columns: the meta parameters by group, and the details of the grid data
    const extraGroups = el('div', {class: 'extra-groups'});
    const extraDetails =
        el('details', {class: 'extra-columns'}, el('summary', null, L('Extra columns')), extraGroups);
    const toggleButton = (label, title, pressed, onclick, data) =>
        el('button',
           Object.assign(
               {
                 type: 'button',
                 class: 'small',
                 title,
                 'aria-pressed': pressed ? 'true' : 'false',
                 onclick
               },
               data),
           label);
    // The extra columns of the response (location, station, time and sun details), as groups of
    // buttons
    function renderExtraColumns()
    {
      const p = producer();
      const source = p ? p.source : '';
      const groups = [];
      for (const g of META_GROUPS)
      {
        if (g.sources && !g.sources.includes(source))
          continue;
        // The name, time and coordinates are always requested
        const buttons =
            g.params.filter(([key]) => !['name', 'time', 'lat', 'lon'].includes(key))
                .map(([key, desc]) => toggleButton(key, desc, s.params.includes(key), () => {
                       s.params = s.params.includes(key) ? s.params.filter((x) => x !== key) :
                                                           [...s.params, key];
                       refresh();
                     }, {'data-meta': key}));
        groups.push(
            el('div',
               {class: 'extra-group'},
               el('div', {class: 'muted small'}, g.title),
               el('div', {class: 'level-buttons'}, buttons)));
      }
      if (source === 'grid')
      {
        const chosen = new Set(String(s.details || '').split(',').filter(Boolean));
        const buttons = GRID_DETAILS.map(
            ([code, label, desc]) =>
                toggleButton(label, `${desc} (@${code}-N)`, chosen.has(code), () => {
                  if (chosen.has(code))
                    chosen.delete(code);
                  else
                    chosen.add(code);
                  s.details = GRID_DETAILS.map((d) => d[0]).filter((c) => chosen.has(c)).join(',');
                  refresh();
                }, {'data-detail': code}));
        groups.push(
            el('div',
               {class: 'extra-group'},
               el('div', {class: 'muted small'}, L('Details of the grid data of each parameter')),
               el('div', {class: 'level-buttons'}, buttons)));
      }
      extraGroups.replaceChildren(...groups);
    }

    const langInput =
        el('input', {type: 'text', 'aria-label': L('Language'), placeholder: L('fi, sv, en'), size: 6});
    langInput.addEventListener('input', () => {
      s.lang = langInput.value.trim();
      refreshSoon();
    });

    // --- Request and results ----------------------------------------
    const urlBox = el('code', {class: 'url'});
    const missing = el('p', {class: 'missing'});
    const runButton = el('button', {type: 'button', class: 'primary'}, L('Run'));
    // Errors of the server and empty results, below the request
    const requestStatus = requestNote();
    const copyUrl = el('button', {type: 'button', class: 'small'}, L('Copy URL'));
    const code = el('div', {class: 'code-box'});
    const codeDetails =
        el('details', {class: 'code-details'}, el('summary', null, L('Code for this request')), code);
    const results = el('div', {class: 'results'});

    copyUrl.addEventListener(
        'click', () => copyText(currentUrl(), copyUrl));
    runButton.addEventListener('click', run);

    // The options of the request as [name, value] pairs
    function requestParams()
    {
      // The coordinates show the locations of the data on the map. The statistics of an area
      // have none: the server returns the coordinates of all the points.
      // The keys of a grid request, or the parameters and the levels option
      const grid = gridColumns();
      const columns = dataColumns();
      const quality = qualityColumns();
      const columnsOf = (p) => [...columns, ...quality]
                                   .filter((c) => c.name === p || c.name === `${p} quality`)
                                   .map((c) => c.column);
      // The level column tells the levels apart when the request does not fix them
      const level =
          levelTypes().length > 0 && (!grid || grid.some((c) => !c.levelFixed)) ? ['level'] : [];
      const params = serverStatistics() || areaAggregate() ?
          Array.from(new Set([
            'name',
            'time',
            ...level,
            ...s.params.filter((p) => META.has(p) && p !== 'lat' && p !== 'lon'),
            ...columns.map((c) => c.column)
          ])) :
          Array.from(new Set([
            'name',
            'time',
            'lat',
            'lon',
            ...level,
            ...s.params.flatMap((p) => (META.has(p) ? [p] : columnsOf(p)))
          ]));
      // Model levels are placed vertically by the pressure and height of each level
      const p = producer();
      if (p && ['hybrid', 'sounding'].includes(levelKind()))
      {
        const extras = [
          [...PRESSURE_PARAMS, ...PRESSURE_PA_PARAMS].find((n) => p.parameterSet.has(n)),
          HEIGHT_PARAMS.find((n) => p.parameterSet.has(n))
        ].filter((n) => n && !params.includes(n));
        for (const n of extras)
          params.push(serverStatistics() ? statisticParameter('nanmedian_a', n) :
                      areaAggregate()    ? statisticParameter('nanmean_a', n) :
                                           n);
      }
      const details = gridDetailColumns(params, columns);
      return [
        ['producer', s.producer],
        ['source', sourceOption()],
        ['origintime', producer() && producer().source === 'grid' ? s.run || '' : ''],
        ['param', [...params, ...details.map((d) => d.column)].join(',')],
        ['levels', grid ? '' : chosenLevels().map((v) => v.value).join(',')],
        ['data_quality', qualityOption()],
        ['starttime', fromInput(s.start)],
        ['endtime', fromInput(s.end)],
        ['timestep', s.timestep],
        // The marked points of a path are asked as points of their own
        [s.locmode === 'path' && s.pathpoints === 'marked' ? 'lonlats' : locationOption(s.locmode),
         s.location],
        ['step', pathStep()],
        ['grouplocations', groupLocations() ? '1' : ''],
        ['lang', s.lang],
        ['format', 'json'],
        ['tz', 'UTC'],
        ['timeformat', 'xml']
      ];
    }

    // The spacing of the points of a path, '' for the marked points
    function pathStep()
    {
      return s.locmode === 'path' ? pathStepOf(s, selection()) : '';
    }

    // The server reads querydata unless told otherwise, or unless it is configured to prefer grid
    function sourceOption()
    {
      const p = producer();
      return p && (p.source === 'grid' || (p.ambiguous && p.source === 'querydata')) ? p.source :
                                                                                       '';
    }

    // The URL of the request
    function currentUrl()
    {
      return new URL(`${rootPath}?${queryString(requestParams())}`, app.base).href;
    }

    // What the request still lacks, for the note under it
    function missingParts()
    {
      const out = [];
      if (!s.producer)
        out.push(L('a producer'));
      if (dataParams().length === 0)
        out.push(L('a parameter'));
      if (!s.location)
        out.push(L('a location'));
      else if (s.locmode === 'path' && (selection() || {points: []}).points.length < 2)
        out.push(L('a second vertex of the path'));
      else if (s.locmode === 'wkt' && !selection())
        out.push(L('a valid WKT geometry'));
      return out;
    }

    // Runs the request and shows its results
    async function run()
    {
      results.replaceChildren(el('p', {class: 'muted'}, L('Running…')));
      requestStatus.clear();
      map.setCursor(null);
      const url = currentUrl();
      let text;
      try
      {
        const response = await fetch(url, {headers: apiHeaders({accept: 'application/json'})});
        text = await response.text();
        if (!response.ok)
          throw new Error(serverError(response, text));
      }
      catch (e)
      {
        requestStatus.show('error', e.message);
        results.replaceChildren();
        return;
      }
      let data;
      try
      {
        data = JSON.parse(text);
      }
      catch (e)
      {
        results.replaceChildren(el('pre', {class: 'code'}, text.slice(0, 20000)));
        return;
      }
      const rows = splitVariants(flattenRows(data), variantColumns());
      // An empty result is explained below the request
      const observed = isObservation();
      // Statistics of an area are in columns such as nanmedian_a(temperature)
      const valueColumns = serverStatistics() && rows.length ?
          Object.keys(rows[0]).filter((k) => dataParams().some((p) => k.endsWith(`(${p})`))) :
          dataParams();
      if (rows.length === 0 || rowsWithoutValues(rows, valueColumns))
        requestStatus.show('warn', rows.length === 0 ? L('The server returned no data for this request.') :
                                             L('The server returned rows without values of the parameters.'),
                  ' ', observed ?
                      L('Observations are returned only for the times and the stations that reported the parameters: try another period or other stations.') :
                      L('Check the period, the locations and the parameters.'));
      // The stations being chosen stay on the map
      if (!serverStatistics() && !areaAggregate() &&
          !(stations().length > 0 && s.locmode === stationMode()))
        map.setPoints(mapPoints(rows));
      showResults(rows, text);
    }

    // The results as tabs of views: the vertical views and the path views when the rows have them,
    // then charts, maps, tables and the response itself
    function showResults(rows, text)
    {
      // Drawn again in the units and the clock chosen
      onSettingsChange('timeseries', () => results.isConnected && showResults(rows, text));
      // The rows in the chosen units for the views of values; the vertical views convert their
      // heights themselves and draw temperatures in C
      const shown = displayRows(rows);
      const statistics = serverStatistics();
      // Several levels at each location and time are drawn as profiles first
      const kind = levelKind();
      const vertical = hasProfiles(rows, kind);
      const panes = {};
      const crossSection = s.locmode === 'path' && kind !== 'member' && hasCrossSection(rows);
      if (crossSection)
        panes['Cross section'] = el('div', {class: 'pane'});
      const alongPath = s.locmode === 'path' && pathPoints(rows).length >= 2;
      if (alongPath)
        panes['Along the path'] = el('div', {class: 'pane'});
      if (vertical)
      {
        panes.Profile = el('div', {class: 'pane'});
        if (new Set(rows.map((r) => String(r.time))).size > 1)
          panes.Section = el('div', {class: 'pane'});
        panes.Sounding = el('div', {class: 'pane'});
      }
      if (hasMeteogram(rows))
        panes.Meteogram = el('div', {class: 'pane'});
      panes.Chart = el('div', {class: 'pane'});
      // The values on a map when there are several located points
      if (hasMapValues(rows))
        panes.Map = el('div', {class: 'pane'});
      panes.Table = el('div', {class: 'pane'});
      panes.Tree = el('div', {class: 'pane tree-pane'});
      const tabs = el('div', {class: 'tabs', role: 'tablist'});
      const drawn = new Set();
      // Shows a tab, drawing its view the first time
      const show = (name) => {
        for (const [n, pane] of Object.entries(panes))
          pane.hidden = n !== name;
        Array.from(tabs.children)
            .forEach(
                (b) => b.setAttribute('aria-selected', b.dataset.tab === name ? 'true' : 'false'));
        if (drawn.has(name))
          return;
        drawn.add(name);
        if (name === 'Chart')
        {
          // One place can be compared with another producer
          const places = new Set(rows.map((r) => locationOf(Object.assign({}, r, {level: null})))).size;
          const levels = new Set(rows.map((r) => r.level).filter((v) => v !== undefined && v !== null)).size;
          if (!statistics && places === 1 && levels <= 1)
            drawComparableCharts(panes.Chart, rows, kind);
          else
            drawCharts(panes.Chart, shown.rows, dataParams(), statistics ? 'server' : undefined,
                       {levelKind: kind, labels: shown.labels});
        }
        else if (name === 'Meteogram')
          drawMeteogram(panes.Meteogram, rows, kind);
        else if (name === 'Profile')
          drawProfiles(panes.Profile, rows, dataParams(), kind);
        else if (name === 'Section')
          drawSection(panes.Section, rows, dataParams(), kind, mapSettings(app.element));
        else if (name === 'Cross section')
          drawCrossSection(panes['Cross section'], rows, dataParams(), kind, mapSettings(app.element),
                           {flight: flightChoice.flight()});
        else if (name === 'Along the path')
          drawAlongPath(panes['Along the path'], shown.rows, dataParams(), kind,
                        {onPosition: (p) => map.setCursor(p), flight: flightChoice.flight(), labels: shown.labels});
        else if (name === 'Sounding')
          drawSounding(panes.Sounding, rows, kind);
        else if (name === 'Map')
          drawValueMap(panes.Map, shown.rows, dataParams(), mapSettings(app.element), {levelKind: kind, labels: shown.labels});
        else if (name === 'Table')
          drawTable(panes.Table, shown.rows, shown.labels);
        else if (!S.renderTree(panes.Tree, text, 'json'))
          panes.Tree.textContent = text;
      };
      for (const name of Object.keys(panes))
        tabs.append(el('button',
                       {
                         type: 'button',
                         role: 'tab',
                         'data-tab': name,
                         onclick: () => {
                           // A view of the data chosen opens first with later results too
                           if (REMEMBERED_TABS.includes(name))
                             s.tab = name;
                           else
                             delete s.tab;
                           save();
                           show(name);
                         }
                       },
                       L(name)));
      // Levels and ensemble members (levels of an EDR collection, or members of grid parameter
      // keys) are counted as such, not as locations
      const levelCount =
          new Set(rows.map((r) => r.level).filter((v) => v !== undefined && v !== null)).size;
      const memberCount = new Set(rows.map((r) => r._member).filter((m) => m !== undefined)).size;
      const locationsCount =
          new Set(rows.map((r) => locationOf({name: r.name, lat: r.lat, lon: r.lon, _point: r._point}))).size;
      results.replaceChildren(
          el('div',
             {class: 'results-head'},
             tabs,
             el('span',
                {class: 'muted small'},
                L`${rows.length} rows${locationsCount > 1 ? L`, ${locationsCount} locations` : ''}${
                    levelCount > 1 ? `, ${levelCount} ${kind === 'member' ? L('members') : L('levels')}` : ''}${
                    memberCount > 1 ? `, ${memberCount} ${L('members')}` : ''}`),
             unitsChooser(settingsChanged),
             exportButtons(() => Object.values(panes).find((p) => !p.hidden), shown.rows,
                           `${s.producer || 'timeseries'}`, shown.labels)),
          ...Object.values(panes));
      show(s.tab && panes[s.tab] ? s.tab :
               crossSection        ? 'Cross section' :
               alongPath           ? 'Along the path' :
               vertical                     ? 'Profile' :
                                              'Chart');
    }

    // The kind of the levels of the request: pressure, hybrid, height, depth, ...
    function levelKind()
    {
      const type = chosenLevelType();
      return type ? type.type : '';
    }

    // A skew-T, or what to add for one
    function drawSounding(pane, rows, kind)
    {
      if (hasSounding(rows, kind))
      {
        const options = soundingOptions();
        options.diagram = drawSkewT(pane, rows, kind, options);
        return;
      }
      const wanted = soundingParameters(producer());
      pane.replaceChildren(
          el('p',
             {class: 'muted'},
             L('A sounding needs temperatures on pressure levels, or a pressure on each level.')));
      if (wanted.length > 0)
        pane.append(
            el('button',
               {
                 type: 'button',
                 class: 'small',
                 onclick: () => {
                   s.params = Array.from(new Set([...s.params, ...wanted]));
                   refresh();
                   run();
                 }
               },
               L`Add ${wanted.join(', ')} and run`));
    }

    // The options of the sounding diagram: its type, and the soundings of another producer to
    // compare with, fetched for the same location and times
    // The charts of one place, compared with another producer when one is chosen. Observations
    // are matched to the nearest station.
    function drawComparableCharts(pane, rows, kind)
    {
      const own = producer();
      const params = dataParams();
      const sel = selection();
      // Forecasts with data in the period; the archives of observations are longer than the
      // period they list
      const start = s.start ? new Date(fromInput(s.start)).getTime() : -Infinity;
      const end = s.end ? new Date(fromInput(s.end)).getTime() : Infinity;
      const inPeriod = (p) => p.source === 'observation' || !p.time || !p.time.start ||
          (new Date(p.time.start).getTime() <= end && new Date(p.time.end).getTime() >= start);
      const others = Array.from(index.producers.values())
                         .filter((p) => p !== own && p.source !== 'grid' &&
                                     params.every((n) => p.parameterSet.has(n)) && covers(p, sel) &&
                                     inPeriod(p));
      drawComparison(pane, rows, {
        params,
        kind,
        ownName: own ? own.name : 'request',
        others: others.map((p) => ({key: p.key, name: p.name, label: `${p.name} (${p.source})`,
                                    stations: p.source === 'observation'})),
        chosen: s.compare,
        onChoose: (key) => {
          s.compare = key || undefined;
          save();
        },
        fetchRows: async (other) => {
          const p = index.producers.get(other.key);
          // One value of an area is compared with the same function of the other producer
          const aggregate = areaAggregate();
          const columns = new Map(params.map(
              (n) => [aggregate ? statisticParameter(areaFunction(n).fn, n) : n, n]));
          const query = [
            ['producer', p.name],
            ['source', p.ambiguous && p.source === 'querydata' ? 'querydata' : ''],
            ['param', [...(aggregate ? ['name', 'time'] : ['name', 'time', 'lat', 'lon']),
                       ...columns.keys()].join(',')],
            ['starttime', fromInput(s.start)],
            ['endtime', fromInput(s.end)],
            ['timestep', p.source === 'observation' ? '' : 'data'],
            [locationOption(s.locmode), s.location],
            ['step', pathStep()],
            ['grouplocations', groupLocations() ? '1' : ''],
            ['maxdistance', p.source === 'observation' ? '50' : ''],
            ['format', 'json'],
            ['tz', 'UTC'],
            ['timeformat', 'xml']
          ];
          const response = await fetch(`${app.base}${rootPath.replace(/^\//, '')}?${queryString(query)}`,
                                       {headers: apiHeaders()});
          const rows = flattenRows(response.ok ? await response.json() : []);
          return aggregate ? rows.map((r) => Object.fromEntries(
                                 Object.entries(r).map(([k, v]) => [columns.get(k) || k, v]))) :
                             rows;
        }
      });
    }

    function soundingOptions()
    {
      const own = producer();
      const others = Array.from(index.producers.values())
                         .filter((p) => p !== own && p.source !== 'grid' && soundingCapable(p));
      const select = el('select', {class: 'compare-select', 'aria-label': L('Compare with')}, [
        el('option', {value: ''}, L('No comparison')),
        ...others.map((p) => el('option', {value: p.key}, `${p.name} (${p.source})`))
      ]);
      const status = el('span', {class: 'muted small'});
      const compare = {rows: null, kind: '', name: ''};
      const options = {
        diagram: null,
        type: s.diagram || 'skewt',
        onType: (type) => {
          s.diagram = type;
          save();
        },
        controls: others.length ? [el('label', null, L('Compare with'), select), status] : [],
        compare: {
          // The sounding of the other producer nearest each chosen time, within 3 hours
          soundings: (times) => {
            if (!compare.rows || compare.rows.length === 0)
              return [];
            const axis = verticalAxes(compare.rows, compare.kind).find((a) => a.id === 'pressure');
            const columns = soundingColumns(compare.rows);
            if (!axis || !columns.temperature)
              return [];
            const profiles = collectProfiles(compare.rows, axis);
            const out = [];
            for (const time of times)
            {
              let best = null;
              for (const p of profiles)
                if (!best || Math.abs(p.time - time) < Math.abs(best.time - time))
                  best = p;
              if (best && Math.abs(best.time - time) <= 3 * 3600e3 &&
                  !out.some((o) => o.time === best.time))
                out.push({time: best.time, levels: soundingOf(best, columns, axis)});
            }
            return out.filter((o) => o.levels.length >= 2);
          },
          label: (soundings) =>
              `${compare.name} ${soundings.map((x) => formatTime(new Date(x.time))).join(', ')}`
        }
      };
      select.addEventListener('change', async () => {
        compare.rows = null;
        const p = index.producers.get(select.value);
        if (p)
        {
          status.textContent = L('Loading…');
          try
          {
            const query = [
              ['producer', p.name],
              ['source', p.ambiguous && p.source === 'querydata' ? 'querydata' : ''],
              ['param', ['name', 'time', 'level', ...soundingNames(p)].join(',')],
              ['starttime', fromInput(s.start)],
              ['endtime', fromInput(s.end)],
              ['timestep', 'data'],
              [locationOption(s.locmode), s.location],
              ['format', 'json'],
              ['tz', 'UTC']
            ];
            const response =
                await fetch(`${app.base}${rootPath.replace(/^\//, '')}?${queryString(query)}`,
                            {headers: apiHeaders()});
            compare.rows = flattenRows(response.ok ? await response.json() : []);
            compare.kind = (p.levels && p.levels[0] && p.levels[0].type) || '';
            compare.name = p.name;
            status.textContent = compare.rows.length ? '' : L`No soundings of ${p.name} here`;
          }
          catch (e)
          {
            status.textContent = e.message;
          }
        }
        if (options.diagram)
          options.diagram.redraw();
      });
      return options;
    }

    // Whether a producer has soundings: a temperature, and levels with pressures
    function soundingCapable(p)
    {
      const levels = (p.levels || []).filter((l) => l.values.length >= 3);
      if (levels.length === 0 || !SOUNDING_PARAMS.temperature.some(([n]) => p.parameterSet.has(n)))
        return false;
      return levels.some((l) => l.type === 'pressure') ||
          [...PRESSURE_PARAMS, ...PRESSURE_PA_PARAMS].some((n) => p.parameterSet.has(n));
    }

    // The parameters of a sounding of a producer: the vertical coordinates, temperature,
    // dewpoint or humidity, and wind
    function soundingNames(p)
    {
      const out = [];
      for (const candidates of [[...PRESSURE_PARAMS, ...PRESSURE_PA_PARAMS], HEIGHT_PARAMS])
      {
        const n = candidates.find((x) => p.parameterSet.has(x));
        if (n)
          out.push(n);
      }
      for (const what of ['temperature', 'dewpoint', 'humidity', 'speed', 'direction'])
      {
        const n = SOUNDING_PARAMS[what].map(([x]) => x).find((x) => p.parameterSet.has(x));
        if (n)
          out.push(n);
      }
      return Array.from(new Set(out));
    }

    // A sounding diagram chosen before the request: the parameters of a sounding are added, the
    // levels chosen so that there are enough of them, and the result opens on the diagram
    let pendingSounding = null;
    function startSounding(type)
    {
      const p = producer();
      if (!p)
        return;
      s.params = Array.from(new Set([...s.params, ...soundingParameters(p)]));
      s.diagram = type;
      // The levels of grid parameters are known once the details have been loaded
      const d = gridDetails();
      if (p.source === 'grid' && !(d && (d.parameters || d.error)))
      {
        pendingSounding = type;
        refresh();
        return;
      }
      pendingSounding = null;
      const types = levelTypes();
      const chosen = chosenLevelType();
      if (!chosen || chosen.values.length < 3)
      {
        const best = types.find((x) => x.values.length >= 3 && x.type === 'hybrid') ||
            types.find((x) => x.values.length >= 3 && x.type === 'pressure') ||
            types.find((x) => x.values.length >= 3);
        if (best)
        {
          s.leveltype = best.id;
          s.levels = '';
        }
      }
      // Fewer than three chosen levels make no sounding: all of them then
      if (chosenLevels().length > 0 && chosenLevels().length < 3)
        s.levels = '';
      // Model levels get their pressures with the request, other levels need the parameter
      if (!['pressure', 'hybrid', 'sounding'].includes(levelKind()))
      {
        const pressure =
            [...PRESSURE_PARAMS, ...PRESSURE_PA_PARAMS].find((n) => p.parameterSet.has(n));
        if (pressure && !s.params.includes(pressure))
          s.params.push(pressure);
      }
      s.tab = 'Sounding';
      refresh();
      if (missingParts().length === 0)
        run();
    }

    // The parameters of a sounding the producer has and the request lacks
    function soundingParameters(p)
    {
      if (!p)
        return [];
      const out = [];
      for (const what of ['temperature', 'dewpoint', 'humidity', 'speed', 'direction'])
      {
        const names = SOUNDING_PARAMS[what].map(([n]) => n);
        // A dewpoint makes the humidity unnecessary
        if (what === 'humidity' && SOUNDING_PARAMS.dewpoint.some(([n]) => p.parameterSet.has(n)))
          continue;
        const found = names.find((n) => p.parameterSet.has(n));
        if (found && !s.params.includes(found))
          out.push(found);
      }
      return out;
    }

    // Redraws the form and the request after a change; rerender = false keeps the text being typed
    function refresh(rerender = true)
    {
      const p = producer();
      meteogram.update(p ? p.parameterSet : null, s.params);
      // Chips
      chips.replaceChildren(...s.params.map((key) => {
        const entry = index.parameters.get(key);
        const label = entry ? entry.name : key;
        return el(
            'span',
            {
              class: `chip${META.has(key) ? ' meta' : ''}`,
              title: entry && entry.description ? entry.description : ''
            },
            label,
            el('button',
               {
                 type: 'button',
                 class: 'chip-x',
                 'aria-label': L`Remove ${label}`,
                 onclick: () => {
                   s.params = s.params.filter((x) => x !== key);
                   refresh();
                 }
               },
               '×'));
      }));
      // Model
      const n = candidates().length;
      const inArea = selection() ? ' in the selected area' : '';
      candidatesNote.textContent = dataParams().length > 0 ?
          L`${n} of ${index.producers.size} producers have ${
              dataParams().length === 1 ? L('this parameter') : L('all these parameters')}${inArea}` :
          (inArea ? L`${n} of ${index.producers.size} producers cover the selected area` :
                    L`${index.producers.size} producers`);
      if (p)
      {
        const levels =
            (p.levels || []).map((l) => `${l.type}: ${l.values.length} levels`).join(', ');
        modelCard.replaceChildren(
            el('div',
               {class: 'model-head'},
               el('strong', null, p.name),
               el('span', {class: `badge badge-${p.source}`}, p.source),
               el('button',
                  {
                    type: 'button',
                    class: 'small',
                    onclick: () => {
                      s.producer = '';
                      s.source = '';
                      resetDetails();
                      refresh();
                    }
                  },
                  L('Change'))),
            p.title && p.title !== p.name ? el('div', null, p.title) : '',
            el('div',
               {class: 'muted small'},
               [
                 timeRange(p),
                 p.time && p.time.step && p.source !== 'observation' ? `step ${p.time.step} min` :
                                                                       '',
                 p.time && p.time.origintime ? `run ${formatTime(new Date(p.time.origintime))}` : ''
               ].filter(Boolean)
                   .join(' · ')),
            el('div', {class: 'muted small'}, [
              `${p.parameters.length} parameters`,
              levels,
              p.stations ? `${p.stations} stations` : '',
              p.bbox ? `area ${p.bbox.map((v) => v.toFixed(1)).join(', ')}` : ''
            ].filter(Boolean).join(' · ')));
        modelCard.hidden = false;
        modelSearch.element.hidden = true;
      }
      else
      {
        modelCard.hidden = true;
        modelSearch.element.hidden = false;
      }
      renderDetails();
      renderExtraColumns();
      // Times, within the period of the data: of the chosen run of a grid model, or of the
      // producer (the latest run continued by the older ones). Observations are available by
      // station before their recent data.
      {
        const d = gridDetails();
        const t = s.run && d && d.run && d.run.start ? d.run : p && p.time ? p.time : {};
        const period = periodOf(t.start, t.end);
        if (p && p.source === 'observation')
          period.min = '';
        limitPeriod(s, startInput, endInput, period);
      }
      startInput.value = s.start || '';
      endInput.value = s.end || '';
      stepSelect.value = s.timestep || 'data';
      Array.from(aggModes.children)
          .forEach(
              (b) => b.setAttribute(
                  'aria-checked', b.dataset.agg === (s.agg || '') ? 'true' : 'false'));
      aggDetails.hidden = s.agg !== 'period';
      periodSelect.value = s.period || '60';
      windowSelect.value = s.window || 'end';
      functionSelect.value = s.fn || 'auto';
      percentileLabel.hidden = s.fn !== 'percentile';
      if (document.activeElement !== percentileInput)
        percentileInput.value = s.pct || '90';
      const functions = dataParams().map(timeFunction).filter(Boolean);
      aggSummary.replaceChildren(
          ...functions.map((f) => el('li', null, f.text)),
          ...functions.flatMap((f) => f.warnings).map((w) => el('li', {class: 'warn'}, w)));
      // The period listed for observations is only the recent one the metadata covers
      // Observations may go back to the 1800s: the whole data would be a huge request, and
      // they have no future
      // and forecasts have no past weeks
      const observation = !!p && p.source === 'observation';
      for (const button of presets.children)
        button.hidden = observation ? ['Whole data', 'Next 24 h'].includes(button.dataset.preset) :
                                      ['Last week', 'Last month'].includes(button.dataset.preset);
      dataPeriod.textContent = !p || !timeRange(p) ?
          '' :
          p.source === 'observation' ?
          L`Recent data: ${timeRange(p)}, older by station` :
          L`Data: ${
              timeRange(p)}`;  // A long period of observations is a large request: warn about it
      const days = s.start && s.end ? (new Date(`${s.end}Z`) - new Date(`${s.start}Z`)) / 864e5 : 0;
      if (p && p.source === 'observation' && days > 32)
        dataPeriod.textContent +=
            L` · ${Math.round(days)} days of observations is a large request: consider a longer step or values over a period`;

      // Location
      Array.from(locModes.children)
          .forEach(
              (b) =>
                  b.setAttribute('aria-checked', b.dataset.mode === s.locmode ? 'true' : 'false'));
      const modes = locationModes();
      const mode = modes.find((m) => m[0] === s.locmode) || modes[0];
      locInput.placeholder = mode[2];
      if (rerender !== false)
        locInput.value = s.location || '';
      // A WKT geometry is drawn on the map but typed, or converted from another type
      const tool = mode[3] === 'wkt' ? null : mode[3];
      map.setTools(tool ? ['move', tool] : ['move'], tool || 'move');
      const sel = selection();
      map.setSelection(sel);
      map.setRegions(p && p.bbox ? [{bbox: p.bbox, label: L`The area of ${p.name}`}] : []);
      showButton.hidden = mode[3] !== null;
      // The stations of an observation producer: chosen on the map or by name
      const known = stations();
      const operating = stationsInPeriod();
      const pickStations = known.length > 0;
      stationSearch.element.hidden = !pickStations;
      stationNote.hidden = !pickStations;
      if (pickStations)
      {
        const chosen = new Set(chosenStations());
        stationNote.textContent = L`${operating.length} of the ${known.length} stations of ${
            p.name} operate in the chosen period${
            s.locmode === stationMode() ?
                L(': click them on the map to choose') :
                L`, choose "${
                    stationMode() === 'fmisid' ? L('Station id') :
                                                 L('WMO id')}" to pick them on the map`}`;
        if (s.locmode === stationMode())
          map.setPoints(operating.map(
              (l) => ({
                lon: l.lon,
                lat: l.lat,
                label: l.name,
                info: {[stationMode()]: l.id, wsi: l.wsi, since: (l.start || '').slice(0, 10)},
                selected: chosen.has(l.id),
                onclick: () => toggleStation(l.id)
              })));
      }
      else if (p && p.source === 'observation' && details.get(detailsKey(p)) === null)
      {
        stationNote.hidden = false;
        stationNote.textContent = L('Loading the stations…');
      }
      showButton.disabled = !s.location;
      areaNote.textContent = p && sel && !covers(p, sel) ?
          L`The selection is outside the area of ${p.name}` :
          (s.location && mode[3] && !sel ?
               L`Not a valid ${mode[0] === 'wkt' ? mode[1] : mode[1].toLowerCase()}: ${mode[2]}` :
               '');
      if (rerender !== false)
        langInput.value = s.lang || '';
      areaValuesRow.hidden = !isArea();
      pathChooser.element.hidden = s.locmode !== 'path';
      flightChoice.element.hidden = s.locmode !== 'path';
      if (s.locmode === 'path')
      {
        pathChooser.update(selection(), rerender !== false);
        flightChoice.update(selection());
      }
      Array.from(areaValues.children)
          .forEach(
              (b) => b.setAttribute(
                  'aria-checked', b.dataset.areavalues === areaValuesOf() ? 'true' : 'false'));
      renderAreaFunctions();
      // Request
      const url = currentUrl();
      urlBox.textContent = url;
      const absent = missingParts();
      missing.textContent = absent.length ? L`Still needed: ${absent.join(', ')}` : '';
      runButton.disabled = absent.length > 0;
      codeTabs(code, url);
      save();
    }

    // A step of the form with its heading
    const section = (title, ...content) =>
        el('section', {class: 'step'}, el('h2', null, title), ...content);
    view.append(
        el('div',
           {class: 'form'},
           section(
               L('What'),
               // The model first, as most users start from it; the parameters narrow the models
               // too, so either can be chosen first. The levels and members of the model come
               // after the parameters, which they depend on.
               el('label', null, L('Producer')),
               candidatesNote,
               modelSearch.element,
               modelCard,
               el('label', null, L('Parameters')),
               chips,
               paramSearch.element,
               meteogram.element,
               detailsBox),
           section(
               L('When'),
               el('div',
                  {class: 'row'},
                  el('label', null, L('Start (UTC)'), startInput),
                  el('label', null, L('End (UTC)'), endInput)),
               el('div', {class: 'row'}, el('label', null, L('Step'), stepSelect), dataPeriod),
               presets,
               aggRow),
           section(
               L('Where'),
               locModes,
               el('div', {class: 'row'}, locSuggest.element, showButton),
               pathChooser.element,
               flightChoice.element,
               stationSearch.element,
               stationNote,
               areaNote,
               map.element),
           section(
               L('Output'),
               areaValuesRow,
               el('div', {class: 'row'}, el('label', null, L('Language of the names'), langInput)),
               extraDetails)),
        el('div',
           {class: 'request'},
           el('div', {class: 'url-row'}, urlBox, copyUrl),
           requestStatus.element,
           missing,
           el('div', {class: 'row'}, runButton),
           codeDetails,
           results));
    defaultTimes(false);
    refresh();
    fitMap();
  }
  return view;
}

// ------------------------------------------------------------------
// The EDR module
// ------------------------------------------------------------------

// A text of the EDR metadata: a string or a map of languages
function edrText(value)
{
  if (!value)
    return '';
  if (typeof value === 'string')
    return value;
  return value.en || Object.values(value)[0] || '';
}

// The collections and the parameters of an EDR API, indexed like the TimeSeries producers
function buildEdrIndex(doc)
{
  const producers = new Map();
  const parameters = new Map();
  for (const c of doc.collections || [])
  {
    const names = Object.keys(c.parameter_names || {});
    const extent = c.extent || {};
    const bbox = extent.spatial && extent.spatial.bbox && extent.spatial.bbox[0];
    const interval = extent.temporal && extent.temporal.interval && extent.temporal.interval[0];
    const vertical = extent.vertical || null;
    producers.set(c.id, {
      name: c.id,
      title: c.title,
      description: c.description,
      source: 'edr',
      bbox: bbox && bbox.length >= 4 ? bbox.slice(0, 4) : null,
      time: interval ? {start: interval[0], end: interval[1]} : null,
      levels: vertical && vertical.values ?
          [{type: edrText(vertical.vrs) || 'level', values: vertical.values.map(String)}] :
          [],
      parameters: names,
      parameterSet: new Set(names),
      queries: Object.keys(c.data_queries || {}),
      formats: c.output_formats || []
    });
    for (const key of names)
    {
      const info = c.parameter_names[key] || {};
      let entry = parameters.get(key);
      if (!entry)
      {
        const unit =
            info.unit && (info.unit.symbol && (info.unit.symbol.value || info.unit.symbol));
        entry = {
          key: key,
          name: edrText(info.label) || info.id || key,
          description: edrText(info.description),
          unit: typeof unit === 'string' ? unit : '',
          producers: []
        };
        parameters.set(key, entry);
      }
      entry.producers.push(c.id);
    }
  }
  return {producers, parameters};
}

// The rows of a CoverageJSON document: a Coverage or a CoverageCollection, each coverage
// giving a row per point, level and time. GeoJSON features give a row per feature.
function coverageRows(doc)
{
  const rows = [];
  if (!doc || typeof doc !== 'object')
    return rows;
  if (doc.type === 'FeatureCollection' || doc.type === 'Feature')
  {
    const features = doc.type === 'Feature' ? [doc] : doc.features || [];
    for (const f of features)
    {
      const row = Object.assign({}, f.properties || {});
      const g = f.geometry;
      if (g && g.type === 'Point' && Array.isArray(g.coordinates))
      {
        row.lon = g.coordinates[0];
        row.lat = g.coordinates[1];
      }
      if (row.time === undefined && row.datetime !== undefined)
        row.time = row.datetime;
      rows.push(row);
    }
    return rows;
  }
  const coverages = doc.type === 'CoverageCollection' ? doc.coverages || [] : [doc];
  coverages.forEach((cov, index) => {
    const axes = (cov.domain && cov.domain.axes) || {};
    // A trajectory: the points of a composite axis, each a tuple of its coordinates
    if (axes.composite && Array.isArray(axes.composite.values))
    {
      const coordinates = axes.composite.coordinates || ['t', 'x', 'y', 'z'];
      // Coordinate c of a tuple of the composite axis
      const at = (tuple, c) => {
        const i = coordinates.indexOf(c);
        return i >= 0 ? tuple[i] : undefined;
      };
      const zs = (axes.z && axes.z.values) || [undefined];
      const ranges = cov.ranges || {};
      axes.composite.values.forEach((tuple, ci) => {
        for (let zi = 0; zi < zs.length; zi++)
        {
          const lat = at(tuple, 'y');
          const lon = at(tuple, 'x');
          const row = {name: `${lat},${lon}`, time: at(tuple, 't'), lat, lon};
          const z = zs[zi] !== undefined ? zs[zi] : at(tuple, 'z');
          if (z !== undefined)
            row.level = z;
          for (const [p, r] of Object.entries(ranges))
          {
            const names = r.axisNames || [];
            const shape = r.shape || [];
            const position = {composite: ci, z: zi};
            let offset = 0;
            names.forEach((n, i) => {
              offset = offset * (shape[i] || 1) + (position[n] || 0);
            });
            const v = (r.values || [])[offset];
            row[p] = v === undefined ? null : v;
          }
          row._coverage = index;
          rows.push(row);
        }
      });
      return;
    }
    const values = (name) => (axes[name] && axes[name].values) || [undefined];
    const ts = values('t');
    const xs = values('x');
    const ys = values('y');
    const zs = values('z');
    const ranges = cov.ranges || {};
    const params = Object.keys(ranges);
    for (let zi = 0; zi < zs.length; zi++)
      for (let yi = 0; yi < ys.length; yi++)
        for (let xi = 0; xi < xs.length; xi++)
          for (let ti = 0; ti < ts.length; ti++)
          {
            // Each point is a location, named by its coordinates
            const row = {name: `${ys[yi]},${xs[xi]}`, time: ts[ti], lat: ys[yi], lon: xs[xi]};
            if (zs[zi] !== undefined)
              row.level = zs[zi];
            for (const p of params)
            {
              const r = ranges[p];
              const names = r.axisNames || [];
              const shape = r.shape || [];
              const position = {t: ti, x: xi, y: yi, z: zi};
              let offset = 0;
              names.forEach((n, i) => {
                offset = offset * (shape[i] || 1) + (position[n] || 0);
              });
              const v = (r.values || [])[offset];
              row[p] = v === undefined ? null : v;
            }
            row._coverage = index;
            rows.push(row);
          }
  });
  return rows;
}

// The query types the explorer can make, and their map tools
const EDR_QUERIES = [
  ['position', L('Position'), 'point'],
  ['radius', L('Radius'), 'circle'],
  ['area', L('Area'), 'box'],
  ['cube', L('Cube'), 'box'],
  ['trajectory', L('Trajectory'), 'line'],
  ['locations', L('Locations'), null]
];

// The EDR module: collections and parameters narrowing each other, times and levels, the query
// types of the collection (position, radius, area, cube, trajectory, locations) with the map,
// and the CoverageJSON or GeoJSON results drawn like those of TimeSeries
function edrModule(app, api, state, save)
{
  const rootPath = api.root;
  const view = el('div', {class: 'module'});
  const status = el('p', {class: 'muted'}, L('Loading the collections…'));
  view.append(status);

  fetchJson(`${app.base}${rootPath.replace(/^\//, '')}/collections?f=json`)
      .then((doc) => build(buildEdrIndex(doc)))
      .catch((e) => {
        status.textContent = L`The collections could not be loaded: ${e.message}`;
      });

  // Builds the module once the collections have been loaded
  function build(index)
  {
    view.replaceChildren();
    const s = state;
    // Typing stores the value at once and redraws the request a moment later
    const refreshSoon = debounce(() => refresh(false), 300);
    s.params = s.params || [];
    // The levels are a list here, but a comma separated text in the address of the page
    if (typeof s.levels === 'string')
      s.levels = s.levels.split(',').filter(Boolean);

    const collection = () => index.producers.get(s.collection);
    // The query types of the chosen collection the explorer can make
    const queries = () => {
      const c = collection();
      return EDR_QUERIES.filter(([q]) => !c || c.queries.includes(q));
    };
    // The chosen query type, the first one available by default
    const query = () => {
      const available = queries();
      return available.find(([q]) => q === s.query) || available[0];
    };
    // The selection of the map for the query type, null if none
    const selection = () => {
      const [q] = query();
      if (q === 'position')
        return parseSelection('lonlat', s.location);
      if (q === 'radius')
        return parseSelection('radius', s.location);
      if (q === 'area' || q === 'cube')
        return parseSelection('bbox', s.location);
      if (q === 'trajectory')
        return parseSelection('path', s.location);
      return null;
    };
    // The collections having all the chosen parameters and covering the selection
    const candidates = () => {
      const sel = selection();
      return Array.from(index.producers.values())
          .filter((c) => s.params.every((p) => c.parameterSet.has(p)) && covers(c, sel));
    };

    // --- What -----------------------------------------------------------
    const chips = el('div', {class: 'chips'});
    const note = el('p', {class: 'muted small'});
    const card = el('div', {class: 'model-card'});

    const paramSearch = autocomplete({
      label: L('Parameter'),
      placeholder: L('Type a parameter name or description'),
      search: (text) => {
        const q = text.toLowerCase();
        const c = collection();
        const out = [];
        for (const entry of index.parameters.values())
        {
          if (s.params.includes(entry.key) || (c && !c.parameterSet.has(entry.key)))
            continue;
          const score = matchScore(q, entry.name, `${entry.description} ${entry.key}`);
          if (score !== null)
            out.push({
              score,
              key: entry.key,
              label: entry.name,
              detail:
                  [entry.description, entry.unit && `[${entry.unit}]`].filter(Boolean).join(' '),
              badge: c ?
                  null :
                  counted(entry.producers.length, L('collection'), L('collections'))
            });
        }
        out.sort((a, b) => a.score - b.score || a.label.localeCompare(b.label));
        return out;
      },
      select: (item) => {
        s.params.push(item.key);
        refresh();
      },
      empty: (text) => L`No parameter matches "${text}"`
    });

    const collectionSearch = autocomplete({
      label: L('Collection'),
      placeholder: L('Type a collection name'),
      search: (text) => {
        const q = text.toLowerCase();
        return candidates()
            .map(
                (c) =>
                    ({c, score: matchScore(q, c.name, `${c.title || ''} ${c.description || ''}`)}))
            .filter((x) => x.score !== null)
            .sort((a, b) => a.score - b.score || a.c.name.localeCompare(b.c.name))
            .map(({c}) => ({
                   key: c.name,
                   label: c.name,
                   detail: [c.title && c.title !== c.name ? c.title : '', timeRangeOf(c)]
                               .filter(Boolean)
                               .join(' · ')
                 }));
      },
      select: (item) => {
        s.collection = item.key;
        const c = collection();
        if (c)
          s.params = s.params.filter((p) => c.parameterSet.has(p));
        defaultTimes(true);
        refresh();
        fitMap();
      },
      empty: () => L('No collection has all the chosen parameters'),
      limit: 200
    });

    // The period of the data of a collection for display
    function timeRangeOf(c)
    {
      if (!c.time || !c.time.start || !c.time.end)
        return '';
      return `${formatTime(new Date(c.time.start))} – ${formatTime(new Date(c.time.end))}`;
    }

    // --- How: the query type -------------------------------------------------
    const queryButtons = el('div', {class: 'segmented', role: 'radiogroup', 'aria-label': L('Query')});

    // --- When and the levels -------------------------------------------------
    const startInput = timeInput({'aria-label': L('Start time (UTC)')});
    const endInput = timeInput({'aria-label': L('End time (UTC)')});
    const dataPeriod = el('span', {class: 'muted small'});
    const levels = el('div', {class: 'chips'});
    const levelRow = el('div', null, el('label', null, L('Levels')), levels);
    const diagramButtons =
        el('div', {class: 'level-buttons', role: 'group', 'aria-label': L('Sounding diagrams')});
    const diagramNote = el('p', {class: 'muted small'});
    const diagramRow =
        el('div', null, el('label', null, L('Sounding diagrams')), diagramButtons, diagramNote);
    startInput.addEventListener('change', () => {
      s.start = startInput.value;
      refresh();
    });
    endInput.addEventListener('change', () => {
      s.end = endInput.value;
      refresh();
    });

    // The default period: the data period of the collection
    function defaultTimes(force)
    {
      const c = collection();
      if (!c || !c.time || !c.time.start || !c.time.end || (!force && s.start && s.end))
        return;
      const first = new Date(c.time.start);
      const last = new Date(c.time.end);
      const now = new Date();
      now.setUTCMinutes(0, 0, 0);
      const start = now > first && now < last ?
          now :
          (last < now ? new Date(Math.max(first.getTime(), last.getTime() - 24 * 3600e3)) : first);
      const end = new Date(Math.min(last.getTime(), start.getTime() + 48 * 3600e3));
      s.start = toInput(start);
      s.end = toInput(end);
    }

    // --- Where -------------------------------------------------------------
    const locInput = el('input', {type: 'text', 'aria-label': L('Location'), spellcheck: 'false'});
    const areaNote = el('p', {class: 'warn small'});
    const pathChooser = pathPointsChooser(s, (rerender) => refresh(rerender));
    const flightChoice = flightChooser(s, () => refresh(), () => 180);
    // A meteogram of the chosen place with the parameters it needs
    const meteogram = meteogramButton((missing) => {
      s.params = Array.from(new Set([...s.params, ...missing]));
      s.tab = 'Meteogram';
      refresh();
      if (missingParts().length === 0)
        run();
    });
    // The points of a result belong to the location they were for: a new selection clears them
    const map = createMap(mapSettings(app.element), (shape) => {
      s.location = formatSelection(shape);
      map.setPoints([]);
      refresh();
    });
    locInput.addEventListener('input', () => {
      s.location = locInput.value.trim();
      refreshSoon();
    });
    let locationsOf = null;  // the locations of the collection for a locations query

    // CoverageJSON has only the coordinates of the data: a row at a location of the collection,
    // such as an observation station, is named by it
    function nameRows(rows)
    {
      const list = (locationsOf && locationsOf.list) || [];
      if (list.length === 0)
        return rows;
      const key = (lat, lon) => `${Number(lat).toFixed(4)},${Number(lon).toFixed(4)}`;
      const names = new Map(list.map((l) => [key(l.lat, l.lon), l.label]));
      for (const row of rows)
      {
        // Rows are named by their coordinates unless the data names them
        if (row.name !== undefined && row.name !== null && row.name !== `${row.lat},${row.lon}`)
          continue;
        if (typeof row.lat === 'number' && typeof row.lon === 'number')
        {
          const name = names.get(key(row.lat, row.lon));
          if (name)
            row.name = name;
        }
      }
      return rows;
    }

    // Loads the locations of the collection once, for the location search and for naming the
    // results
    function loadLocations()
    {
      const c = collection();
      if (!c || (locationsOf && locationsOf.id === c.name))
        return;
      locationsOf = {id: c.name, list: []};
      fetchJson(`${app.base}${rootPath.replace(/^\//, '')}/collections/${
                    encodeURIComponent(c.name)}/locations?f=GeoJSON`)
          .then((doc) => {
            const list = (doc.features || [])
                             .filter((f) => f.geometry && f.geometry.type === 'Point')
                             .map((f) => ({
                                    id: String(f.id),
                                    lon: f.geometry.coordinates[0],
                                    lat: f.geometry.coordinates[1],
                                    label: (f.properties && f.properties.name) || String(f.id),
                                    info: {id: f.id}
                                  }));
            locationsOf = {id: c.name, list};
            refresh();
          })
          .catch((e) => {
            areaNote.textContent = L`The locations could not be loaded: ${e.message}`;
          });
    }

    const locationSearch = autocomplete({
      label: L('Location id'),
      placeholder: L('Type a location name or id'),
      search: (text) => {
        const q = text.toLowerCase();
        return ((locationsOf && locationsOf.list) || [])
            .map((l) => ({l, score: matchScore(q, l.label, l.id)}))
            .filter((x) => x.score !== null)
            .sort((a, b) => a.score - b.score || a.l.label.localeCompare(b.l.label))
            .map(({l}) => ({key: l.id, label: l.label, detail: l.id}));
      },
      select: (item) => {
        s.location = item.key;
        refresh();
      },
      empty: () => L('No location matches'),
      limit: 50
    });

    // Fits the map to what it shows
    function fitMap()
    {
      const extent = map.extent();
      if (extent)
        map.fit(extent);
    }

    // --- Request ---------------------------------------------------------------
    const formatSelect = el('select', {'aria-label': L('Output format')});
    formatSelect.addEventListener('change', () => {
      s.format = formatSelect.value;
      refresh();
    });
    const urlBox = el('code', {class: 'url'});
    const missing = el('p', {class: 'missing'});
    const runButton = el('button', {type: 'button', class: 'primary'}, L('Run'));
    // Errors of the server and empty results, below the request
    const requestStatus = requestNote();
    const copyUrl = el('button', {type: 'button', class: 'small'}, L('Copy URL'));
    const code = el('div', {class: 'code-box'});
    const codeDetails =
        el('details', {class: 'code-details'}, el('summary', null, L('Code for this request')), code);
    const results = el('div', {class: 'results'});
    copyUrl.addEventListener(
        'click', () => copyText(currentUrl(), copyUrl));
    runButton.addEventListener('click', run);

    // The location options of the query: coords (and within) for the selection, or bbox for a cube
    function coords()
    {
      const sel = selection();
      if (!sel)
        return [];
      if (sel.type === 'point')
        return [['coords', `POINT(${sel.lon} ${sel.lat})`]];
      if (sel.type === 'circle')
        return [
          ['coords', `POINT(${sel.lon} ${sel.lat})`],
          ['within', String(sel.km)],
          ['within-units', 'km']
        ];
      // A trajectory at its marked points, or at points every N km along it
      if (sel.type === 'path')
        return [
          ['coords', `LINESTRING(${sel.points.map(([lon, lat]) => `${lon} ${lat}`).join(',')})`],
          ['step', pathStepOf(s, sel)]
        ];
      const [a, b, c, d] = sel.bbox;
      if (query()[0] === 'cube')
        return [['bbox', `${a},${b},${c},${d}`]];
      return [['coords', `POLYGON((${a} ${b},${c} ${b},${c} ${d},${a} ${d},${a} ${b}))`]];
    }

    // The URL of the request
    function currentUrl()
    {
      const c = collection();
      const [q] = query();
      let path =
          `${rootPath}/collections/${encodeURIComponent(s.collection || '{collectionId}')}/${q}`;
      if (q === 'locations' && s.location)
        path += `/${encodeURIComponent(s.location)}`;
      // Model levels are placed vertically by the pressure and height of each level
      const names = s.params.slice();
      if (c && edrLevelKind(c) === 'hybrid')
        for (const candidates of [[...PRESSURE_PARAMS, ...PRESSURE_PA_PARAMS], HEIGHT_PARAMS])
        {
          const n = candidates.find((x) => c.parameterSet.has(x));
          if (n && !names.includes(n))
            names.push(n);
        }
      const params = [
        ...coords(),
        ['parameter-name', names.join(',')],
        ['datetime', s.start && s.end ? `${fromInput(s.start)}/${fromInput(s.end)}` : ''],
        ['z', (s.levels || []).join(',')],
        ['f', s.format || (c && c.formats[0]) || 'CoverageJSON']
      ];
      return new URL(`${path}?${queryString(params)}`, app.base).href;
    }

    // Whether a collection has soundings: a temperature, and levels with pressures
    function soundingCapable(c)
    {
      if (!c || !c.levels.length || c.levels[0].values.length < 3 ||
          !SOUNDING_PARAMS.temperature.some(([n]) => c.parameterSet.has(n)))
        return false;
      return ['pressure', 'pressure-pa'].includes(edrLevelKind(c)) ||
          [...PRESSURE_PARAMS, ...PRESSURE_PA_PARAMS].some((n) => c.parameterSet.has(n));
    }

    // The parameters of a sounding the collection has and the request lacks
    function soundingParameters(c)
    {
      const out = [];
      for (const what of ['temperature', 'dewpoint', 'humidity', 'speed', 'direction'])
      {
        // A dewpoint makes the humidity unnecessary
        if (what === 'humidity' && SOUNDING_PARAMS.dewpoint.some(([n]) => c.parameterSet.has(n)))
          continue;
        const found = SOUNDING_PARAMS[what].map(([n]) => n).find((n) => c.parameterSet.has(n));
        if (found && !s.params.includes(found))
          out.push(found);
      }
      return out;
    }

    // A sounding diagram chosen before the request: the parameters of a sounding are added, and
    // the results open on the diagram
    function startSounding(type)
    {
      const c = collection();
      if (!c)
        return;
      s.params = Array.from(new Set([...s.params, ...soundingParameters(c)]));
      // Model levels get their pressures with the request, other levels need the parameter
      if (!['pressure', 'pressure-pa', 'hybrid'].includes(edrLevelKind(c)))
      {
        const pressure =
            [...PRESSURE_PARAMS, ...PRESSURE_PA_PARAMS].find((n) => c.parameterSet.has(n));
        if (pressure && !s.params.includes(pressure))
          s.params.push(pressure);
      }
      // Fewer than three chosen levels make no sounding: all of them then
      if ((s.levels || []).length < 3)
        s.levels = [];
      s.diagram = type;
      s.tab = 'Sounding';
      refresh();
      if (missingParts().length === 0)
        run();
    }

    // What the request still lacks, for the note under it
    function missingParts()
    {
      const out = [];
      if (!s.collection)
        out.push(L('a collection'));
      if (s.params.length === 0)
        out.push(L('a parameter'));
      const [q] = query();
      const sel = selection();
      if (q === 'locations' ? !s.location : !sel)
        out.push(
            q === 'locations' ? L('a location') :
                                L`a ${
                                    q === 'position'       ? L('point') :
                                        q === 'radius'     ? L('radius') :
                                        q === 'trajectory' ? L('path') :
                                                             L('box')} on the map`);
      else if (q === 'trajectory' && sel.points.length < 2)
        out.push(L('a second point of the path'));
      return out;
    }

    // Runs the request and shows its results
    async function run()
    {
      results.replaceChildren(el('p', {class: 'muted'}, L('Running…')));
      requestStatus.clear();
      map.setCursor(null);
      let text;
      try
      {
        const response = await fetch(currentUrl(), {headers: apiHeaders()});
        text = await response.text();
        if (response.status === 204)
          text = '{}';
        else if (!response.ok)
          throw new Error(serverError(response, text));
      }
      catch (e)
      {
        requestStatus.show('error', e.message);
        results.replaceChildren();
        return;
      }
      let doc;
      try
      {
        doc = JSON.parse(text);
      }
      catch (e)
      {
        results.replaceChildren(el('pre', {class: 'code'}, text.slice(0, 20000)));
        return;
      }
      const rows = nameRows(coverageRows(doc));
      if (rows.length === 0 || rowsWithoutValues(rows, s.params || []))
        requestStatus.show('warn', rows.length === 0 ? L('The server returned no data for this request.') :
                                             L('The server returned rows without values of the parameters.'),
                  ' ', L('Check the period, the locations and the parameters.'));
      map.setPoints(
          rows.filter((r) => typeof r.lat === 'number' && typeof r.lon === 'number')
              .filter((r, i, all) => all.findIndex((o) => o.lat === r.lat && o.lon === r.lon) === i)
              .slice(0, 10000)
              .map((r) => ({
                     lon: r.lon,
                     lat: r.lat,
                     label: String(r.name || ''),
                     info: {lat: r.lat, lon: r.lon}
                   })));
      showResults(rows, text);
    }

    // The results as tabs of views: the vertical views and the path views when the rows have
    // them, then charts, maps, tables and the response itself
    function showResults(rows, text)
    {
      // Drawn again in the units and the clock chosen
      onSettingsChange('edr', () => results.isConnected && showResults(rows, text));
      // The rows in the chosen units for the views of values; the vertical views convert their
      // heights themselves and draw temperatures in C
      const shown = displayRows(rows);
      // Several levels at each location and time are drawn as profiles first
      const c = collection();
      const kind = edrLevelKind(c);
      const vertical = hasProfiles(rows, kind);
      const panes = {};
      const trajectory = query()[0] === 'trajectory';
      const crossSection = trajectory && kind !== 'member' && hasCrossSection(rows);
      if (crossSection)
        panes['Cross section'] = el('div', {class: 'pane'});
      const alongPath = trajectory && pathPoints(rows).length >= 2;
      if (alongPath)
        panes['Along the path'] = el('div', {class: 'pane'});
      if (vertical)
      {
        panes.Profile = el('div', {class: 'pane'});
        if (new Set(rows.map((r) => String(r.time))).size > 1)
          panes.Section = el('div', {class: 'pane'});
        if (hasSounding(rows, kind))
          panes.Sounding = el('div', {class: 'pane'});
      }
      if (hasMeteogram(rows))
        panes.Meteogram = el('div', {class: 'pane'});
      panes.Chart = el('div', {class: 'pane'});
      // The values on a map when there are several located points
      if (hasMapValues(rows))
        panes.Map = el('div', {class: 'pane'});
      panes.Table = el('div', {class: 'pane'});
      panes.Tree = el('div', {class: 'pane tree-pane'});
      const tabs = el('div', {class: 'tabs', role: 'tablist'});
      const drawn = new Set();
      // The pressure and height requested with model levels place them, they are not drawn
      const placing = [...PRESSURE_PARAMS, ...PRESSURE_PA_PARAMS, ...HEIGHT_PARAMS].filter(
          (n) => !s.params.includes(n));
      const params = Array.from(new Set(rows.flatMap((r) => Object.keys(r))))
                         .filter(
                             (k) => !['name', 'time', 'lat', 'lon', 'level'].includes(k) &&
                                 !k.startsWith('_') && !placing.includes(k.toLowerCase()));
      // Shows a tab, drawing its view the first time
      const show = (name) => {
        for (const [n, pane] of Object.entries(panes))
          pane.hidden = n !== name;
        Array.from(tabs.children)
            .forEach(
                (b) => b.setAttribute('aria-selected', b.dataset.tab === name ? 'true' : 'false'));
        if (drawn.has(name))
          return;
        drawn.add(name);
        if (name === 'Chart')
        {
          // One place can be compared with another collection
          const places = new Set(rows.map((r) => locationOf(Object.assign({}, r, {level: null})))).size;
          const levels = new Set(rows.map((r) => r.level).filter((v) => v !== undefined && v !== null)).size;
          if (places === 1 && levels <= 1)
            drawComparableCharts(panes.Chart, rows, kind, params);
          else
            drawCharts(panes.Chart, shown.rows, params, undefined, {levelKind: kind, labels: shown.labels});
        }
        else if (name === 'Meteogram')
          drawMeteogram(panes.Meteogram, rows, kind);
        else if (name === 'Profile')
          drawProfiles(panes.Profile, rows, params, kind);
        else if (name === 'Section')
          drawSection(panes.Section, rows, params, kind, mapSettings(app.element));
        else if (name === 'Cross section')
          drawCrossSection(panes['Cross section'], rows, params, kind, mapSettings(app.element),
                           {flight: flightChoice.flight()});
        else if (name === 'Along the path')
          drawAlongPath(panes['Along the path'], shown.rows, params, kind,
                        {onPosition: (p) => map.setCursor(p), flight: flightChoice.flight(), labels: shown.labels});
        else if (name === 'Sounding')
          drawSkewT(panes.Sounding, rows, kind, {
            type: s.diagram || 'skewt',
            onType: (type) => {
              s.diagram = type;
              save();
            }
          });
        else if (name === 'Map')
          drawValueMap(panes.Map, shown.rows, params, mapSettings(app.element), {levelKind: kind, labels: shown.labels});
        else if (name === 'Table')
          drawTable(panes.Table, shown.rows, shown.labels);
        else if (!S.renderTree(panes.Tree, text, 'json'))
          panes.Tree.textContent = text;
      };
      for (const name of Object.keys(panes))
        tabs.append(el('button',
                       {
                         type: 'button',
                         role: 'tab',
                         'data-tab': name,
                         onclick: () => {
                           // A view of the data chosen opens first with later results too
                           if (REMEMBERED_TABS.includes(name))
                             s.tab = name;
                           else
                             delete s.tab;
                           save();
                           show(name);
                         }
                       },
                       L(name)));
      // Levels and ensemble members (the levels of a collection) are counted as such, not as
      // locations
      const count =
          new Set(rows.map((r) => locationOf({name: r.name, lat: r.lat, lon: r.lon, _point: r._point}))).size;
      const levelCount =
          new Set(rows.map((r) => r.level).filter((v) => v !== undefined && v !== null)).size;
      results.replaceChildren(
          el('div',
             {class: 'results-head'},
             tabs,
             el('span',
                {class: 'muted small'},
                L`${rows.length} rows${count > 1 ? L`, ${count} locations` : ''}${
                    levelCount > 1 ? `, ${levelCount} ${kind === 'member' ? L('members') : L('levels')}` : ''}`),
             unitsChooser(settingsChanged),
             exportButtons(() => Object.values(panes).find((p) => !p.hidden), shown.rows,
                           `${s.collection || 'edr'}`, shown.labels)),
          ...Object.values(panes));
      show(s.tab && panes[s.tab] ? s.tab :
               crossSection        ? 'Cross section' :
               alongPath           ? 'Along the path' :
               vertical            ? 'Profile' :
                                     'Chart');
    }

    // The charts of one place, compared with another collection when one is chosen: the same
    // query of the other collection, without the levels of this one
    function drawComparableCharts(pane, rows, kind, params)
    {
      const own = collection();
      const sel = selection();
      const [q] = query();
      const start = s.start ? new Date(fromInput(s.start)).getTime() : -Infinity;
      const end = s.end ? new Date(fromInput(s.end)).getTime() : Infinity;
      const inPeriod = (c) => !c.time || !c.time.start ||
          (new Date(c.time.start).getTime() <= end && new Date(c.time.end).getTime() >= start);
      const others = Array.from(index.producers.values())
                         .filter((c) => c !== own && c.queries.includes(q) &&
                                     s.params.every((p) => c.parameterSet.has(p)) && covers(c, sel) &&
                                     inPeriod(c));
      drawComparison(pane, rows, {
        params,
        kind,
        ownName: own ? own.name : 'request',
        others: others.map((c) => ({key: c.name, name: c.name, label: c.name})),
        chosen: s.compare,
        onChoose: (key) => {
          s.compare = key || undefined;
          save();
        },
        fetchRows: async (other) => {
          const url = new URL(currentUrl().replace(`/collections/${encodeURIComponent(s.collection)}/`,
                                                   `/collections/${encodeURIComponent(other.key)}/`));
          url.searchParams.delete('z');
          const response = await fetch(url.href, {headers: apiHeaders({accept: 'application/json'})});
          return response.ok ? coverageRows(await response.json()) : [];
        }
      });
    }

    // Redraws the form and the request after a change; rerender = false keeps the text being typed
    function refresh(rerender = true)
    {
      const c = collection();
      meteogram.update(c ? c.parameterSet : null, s.params);
      chips.replaceChildren(...s.params.map((key) => {
        const entry = index.parameters.get(key);
        const label = entry ? entry.name : key;
        return el(
            'span',
            {class: 'chip', title: entry ? entry.description : ''},
            label,
            el('button',
               {
                 type: 'button',
                 class: 'chip-x',
                 'aria-label': L`Remove ${label}`,
                 onclick: () => {
                   s.params = s.params.filter((x) => x !== key);
                   refresh();
                 }
               },
               '×'));
      }));
      const n = candidates().length;
      const inArea = selection() ? ' in the selected area' : '';
      note.textContent = s.params.length > 0 || inArea ?
          L`${n} of ${index.producers.size} collections${
              s.params.length ?
                  L` have ${s.params.length === 1 ? L('this parameter') : L('all these parameters')}` :
                  L(' cover the selected area')}${s.params.length ? inArea : ''}` :
          L`${index.producers.size} collections`;
      if (c)
      {
        card.replaceChildren(
            el('div',
               {class: 'model-head'},
               el('strong', null, c.name),
               el('button',
                  {
                    type: 'button',
                    class: 'small',
                    onclick: () => {
                      s.collection = '';
                      refresh();
                    }
                  },
                  L('Change'))),
            c.title && c.title !== c.name ? el('div', null, c.title) : '',
            el('div', {class: 'muted small'}, [
              timeRangeOf(c),
              `${c.parameters.length} parameters`,
              c.levels.length ? `${c.levels[0].values.length} levels` : ''
            ].filter(Boolean).join(' · ')));
        card.hidden = false;
        collectionSearch.element.hidden = true;
      }
      else
      {
        card.hidden = true;
        collectionSearch.element.hidden = false;
      }

      // Query types
      const [q, , tool] = query();
      queryButtons.replaceChildren(...queries().map(
          ([name, label]) =>
              el('button',
                 {
                   type: 'button',
                   role: 'radio',
                   'data-query': name,
                   'aria-checked': name === q ? 'true' : 'false',
                   onclick: () => {
                     if (s.query === name)
                       return;
                     s.query = name;
                     s.location = '';
                     map.setPoints([]);
                     refresh();
                   }
                 },
                 label)));

      // Times and levels
      limitPeriod(s, startInput, endInput, c && c.time ? periodOf(c.time.start, c.time.end) : {min: '', max: ''});
      startInput.value = s.start || '';
      endInput.value = s.end || '';
      dataPeriod.textContent = c && timeRangeOf(c) ? L`Data: ${timeRangeOf(c)}` : '';
      const values = c && c.levels.length ? c.levels[0].values : [];
      s.levels = (s.levels || []).filter((v) => values.includes(v));
      levels.replaceChildren(...values.map(
          (v) =>
              el('button',
                 {
                   type: 'button',
                   class: `chip${s.levels.includes(v) ? '' : ' meta'}`,
                   'aria-pressed': s.levels.includes(v) ? 'true' : 'false',
                   onclick: () => {
                     s.levels =
                         s.levels.includes(v) ? s.levels.filter((x) => x !== v) : [...s.levels, v];
                     refresh();
                   }
                 },
                 v)));
      levelRow.hidden = values.length === 0;
      // Sounding diagrams of the levels, the missing parameters added when chosen
      diagramRow.hidden = !soundingCapable(c);
      if (!diagramRow.hidden)
      {
        const missingNames = soundingParameters(c);
        diagramButtons.replaceChildren(...THERMO_DIAGRAMS.map(
            ([type, label]) =>
                el('button',
                   {
                     type: 'button',
                     class: 'small',
                     'data-diagram': type,
                     onclick: () => startSounding(type)
                   },
                   label)));
        diagramNote.textContent = missingNames.length > 0 ?
            L`Adds ${missingNames.join(', ')} to the parameters and runs the request` :
            L('Runs the request and draws the soundings');
      }

      // Location
      const isLocations = q === 'locations';
      locInput.hidden = isLocations;
      locationSearch.element.hidden = !isLocations;
      // The examples of the modes of locations of the TimeSeries module
      const example = (mode) => locationModes().find((m) => m[0] === mode)[2];
      locInput.placeholder = q === 'position' ? example('lonlat') :
          q === 'radius'                      ? example('radius') :
          q === 'trajectory'                  ? example('path') :
                                                example('bbox');
      pathChooser.element.hidden = q !== 'trajectory';
      flightChoice.element.hidden = q !== 'trajectory';
      if (q === 'trajectory')
      {
        pathChooser.update(selection(), rerender !== false);
        flightChoice.update(selection());
      }
      if (rerender !== false && !isLocations)
        locInput.value = s.location || '';
      map.setTools(tool ? ['move', tool] : ['move'], tool || 'move');
      const sel = selection();
      map.setSelection(sel);
      map.setRegions(c && c.bbox ? [{bbox: c.bbox, label: L`The area of ${c.name}`}] : []);
      // The locations of the collection name the results of all queries
      if (c && c.queries.includes('locations'))
        loadLocations();
      if (isLocations && locationsOf && locationsOf.list)
        map.setPoints(locationsOf.list);
      areaNote.textContent = c && sel && !covers(c, sel) ?
          L`The selection is outside the area of ${c.name}` :
          (isLocations && s.location ? L`Location ${s.location}` : '');

      // Formats
      const formats = c ? c.formats : ['CoverageJSON', 'GeoJSON'];
      formatSelect.replaceChildren(...formats.map((f) => el('option', {value: f}, f)));
      if (!formats.includes(s.format))
        s.format = formats[0];
      formatSelect.value = s.format || '';

      const url = currentUrl();
      urlBox.textContent = url;
      const absent = missingParts();
      missing.textContent = absent.length ? L`Still needed: ${absent.join(', ')}` : '';
      runButton.disabled = absent.length > 0;
      codeTabs(code, url);
      save();
    }

    // A step of the form with its heading
    const section = (title, ...content) =>
        el('section', {class: 'step'}, el('h2', null, title), ...content);
    view.append(
        el('div',
           {class: 'form'},
           section(
               L('What'),
               el('label', null, L('Collection')),
               note,
               collectionSearch.element,
               card,
               el('label', null, L('Parameters')),
               chips,
               paramSearch.element,
               meteogram.element),
           section(
               L('When'),
               el('div',
                  {class: 'row'},
                  el('label', null, L('Start (UTC)'), startInput),
                  el('label', null, L('End (UTC)'), endInput)),
               dataPeriod,
               levelRow,
               diagramRow),
           section(
               L('Where'),
               queryButtons,
               el('div', {class: 'row'}, locInput),
               pathChooser.element,
               flightChoice.element,
               locationSearch.element,
               areaNote,
               map.element),
           section(L('Output'), el('div', {class: 'row'}, el('label', null, L('Format'), formatSelect)))),
        el('div',
           {class: 'request'},
           el('div', {class: 'url-row'}, urlBox, copyUrl),
           requestStatus.element,
           missing,
           el('div', {class: 'row'}, runButton),
           codeDetails,
           results));
    defaultTimes(false);
    refresh();
    fitMap();
  }
  return view;
}

// ------------------------------------------------------------------
// The Coverages module
// ------------------------------------------------------------------

// Inflates zlib (TIFF deflate) data
async function inflate(bytes)
{
  const stream = new Blob([bytes]).stream().pipeThrough(new DecompressionStream('deflate'));
  return new Uint8Array(await new Response(stream).arrayBuffer());
}

// Undoes the floating point predictor (TIFF predictor 3) of one row of 'count' samples with
// 'stride' samples per pixel: the bytes of the row are differences, ordered by byte plane
// with the most significant bytes first. Returns the row as little endian floats.
function undoFloatPredictor(row, count, stride)
{
  for (let i = stride; i < row.length; i++)
    row[i] = (row[i] + row[i - stride]) & 0xff;
  const out = new Uint8Array(row.length);
  for (let k = 0; k < count; k++)
    for (let b = 0; b < 4; b++)
      out[4 * k + b] = row[(3 - b) * count + k];
  return out;
}

// The bands of a float GeoTIFF in longitude and latitude, as the Coverages API writes them:
// {width, height, bbox: [west, south, east, north], bands: [{parameter, time, level,
// description, values}]}, missing values as NaN. Strips or tiles, chunky or planar, no
// compression or deflate, no predictor or the floating point one.
async function readGeoTiff(buffer)
{
  const view = new DataView(buffer);
  const little = view.getUint16(0) === 0x4949;
  if (!little && view.getUint16(0) !== 0x4d4d)
    throw new Error(L('Not a TIFF file'));
  if (view.getUint16(2, little) !== 42)
    throw new Error(L('BigTIFF files are not supported'));
  const u16 = (o) => view.getUint16(o, little);
  const u32 = (o) => view.getUint32(o, little);
  const SIZES = {1: 1, 2: 1, 3: 2, 4: 4, 11: 4, 12: 8, 16: 8};
  const tags = {};
  const ifd = u32(4);
  const n = u16(ifd);
  for (let i = 0; i < n; i++)
  {
    const e = ifd + 2 + 12 * i;
    const tag = u16(e);
    const type = u16(e + 2);
    const count = u32(e + 4);
    const size = (SIZES[type] || 1) * count;
    const at = size <= 4 ? e + 8 : u32(e + 8);
    const read = (k) => (type === 3   ? u16(at + 2 * k) :
                         type === 4   ? u32(at + 4 * k) :
                         type === 11  ? view.getFloat32(at + 4 * k, little) :
                         type === 12  ? view.getFloat64(at + 8 * k, little) :
                                        view.getUint8(at + k));
    tags[tag] = type === 2 ?
        new TextDecoder().decode(new Uint8Array(buffer, at, Math.max(0, count - 1))) :
        Array.from({length: count}, (_, k) => read(k));
  }
  const one = (tag, fallback) => (tags[tag] ? tags[tag][0] : fallback);
  const width = one(256);
  const height = one(257);
  const spp = one(277, 1);
  const planar = one(284, 1) === 2;
  const compression = one(259, 1);
  const predictor = one(317, 1);
  if (one(258, 32) !== 32 || one(339, 1) !== 3)
    throw new Error(L('Only 32 bit floating point GeoTIFF files are supported'));
  if (![1, 8, 32946].includes(compression))
    throw new Error(L`TIFF compression ${compression} is not supported`);
  if (![1, 3].includes(predictor))
    throw new Error(L`TIFF predictor ${predictor} is not supported`);
  const tiled = !!tags[322];
  const blockWidth = tiled ? one(322) : width;
  const blockHeight = tiled ? one(323) : Math.min(height, one(278, height));
  const offsets = tiled ? tags[324] : tags[273];
  const counts = tiled ? tags[325] : tags[279];
  const across = Math.ceil(width / blockWidth);
  const down = Math.ceil(height / blockHeight);
  const values = Array.from({length: spp}, () => new Float32Array(width * height).fill(NaN));
  for (let b = 0; b < offsets.length; b++)
  {
    // The block: its band when planar, its place in the image
    const plane = planar ? Math.floor(b / (across * down)) : 0;
    const index = b % (across * down);
    const bx = (index % across) * blockWidth;
    const by = Math.floor(index / across) * blockHeight;
    let bytes = new Uint8Array(buffer, offsets[b], counts[b]);
    if (compression !== 1)
      bytes = await inflate(bytes);
    const samples = planar ? 1 : spp;
    const rowBytes = blockWidth * samples * 4;
    const rows = Math.min(blockHeight, Math.floor(bytes.length / rowBytes));
    for (let j = 0; j < rows; j++)
    {
      let row = bytes.slice(j * rowBytes, (j + 1) * rowBytes);
      if (predictor === 3)
        row = undoFloatPredictor(row, blockWidth * samples, samples);
      else if (!little)
        for (let k = 0; k < row.length; k += 4)
          row.subarray(k, k + 4).reverse();
      const floats = new Float32Array(row.buffer, row.byteOffset, blockWidth * samples);
      const y = by + j;
      if (y >= height)
        break;
      for (let i = 0; i < blockWidth && bx + i < width; i++)
        for (let s = 0; s < samples; s++)
          values[planar ? plane : s][y * width + bx + i] = floats[i * samples + s];
    }
  }
  // The missing value
  const nodata = tags[42113] ? Number(tags[42113]) : NaN;
  if (isFinite(nodata))
    for (const band of values)
      for (let k = 0; k < band.length; k++)
        if (Math.abs(band[k] - nodata) <= Math.abs(nodata) * 1e-6)
          band[k] = NaN;
  // The area from the tie point and the pixel size
  const scale = tags[33550];
  const tie = tags[33922];
  if (!scale || !tie)
    throw new Error(L('The GeoTIFF file has no georeferencing'));
  const west = tie[3] - tie[0] * scale[0];
  const north = tie[4] + tie[1] * scale[1];
  const bbox = [west, north - height * scale[1], west + width * scale[0], north];
  // The parameters, times and levels of the bands from the GDAL metadata
  const meta = Array.from({length: spp}, () => ({}));
  const xml = tags[42112] || '';
  for (const m of xml.matchAll(/<Item\b([^>]*)>([^<]*)<\/Item>/g))
  {
    const attrs = {};
    for (const a of m[1].matchAll(/(\w+)="([^"]*)"/g))
      attrs[a[1]] = a[2];
    const k = Number(attrs.sample);
    if (attrs.sample === undefined || !meta[k])
      continue;
    meta[k][attrs.role === 'description' ? 'DESCRIPTION' : attrs.name] = m[2];
  }
  const bands = values.map((v, k) => ({
    parameter: meta[k].PARAMETER || `band ${k + 1}`,
    time: meta[k].TIME || '',
    level: meta[k].LEVEL !== undefined ? Number(meta[k].LEVEL) : null,
    description: meta[k].DESCRIPTION || '',
    values: v
  }));
  return {width, height, bbox, bands};
}

// The f values of the output media types of the Coverages API
const COVERAGE_FORMATS = {
  'application/x-grib2': ['grib2', 'GRIB 2'],
  'application/x-grib': ['grib1', 'GRIB 1'],
  'application/netcdf': ['netcdf', 'NetCDF'],
  'image/tiff': ['geotiff', 'GeoTIFF'],
  'application/x-fmi-querydata': ['qd', 'Querydata']
};

// The output projections offered, besides the native grid and any other EPSG code
function coverageCrs()
{
  const out = [
    ['', L('Native grid of the model')],
    ['EPSG:4326', L('Longitude and latitude (EPSG:4326)')],
    ['EPSG:3857', L('Web Mercator (EPSG:3857)')]
  ];
  // The projections of the region of the server
  for (const [code, label] of DEFAULTS.projections || [])
    if (!out.some(([c]) => c === code))
      out.push([code, label === code ? code : `${label} (${code})`]);
  return out;
}

// The name of the projection of a WKT
function projectionName(wkt)
{
  const m = /^\s*(?:PROJCS|PROJCRS|GEOGCS|GEOGCRS)\s*\[\s*"([^"]*)"/.exec(wkt || '');
  return m ? m[1] : '';
}

// A size for display: 12.3 MB
function formatBytes(n)
{
  if (!(n > 0))
    return '0 bytes';
  const units = ['bytes', 'kB', 'MB', 'GB', 'TB'];
  const i = Math.min(units.length - 1, Math.floor(Math.log10(n) / 3));
  return `${(n / 10 ** (3 * i)).toFixed(i === 0 ? 0 : 1)} ${units[i]}`;
}

// The index of the Coverages collections: their extents, native grids and CRSs, the
// parameters loaded later from the schema of each collection
function buildCoverageIndex(doc)
{
  const producers = new Map();
  for (const c of doc.collections || [])
  {
    const extent = c.extent || {};
    const bbox = extent.spatial && extent.spatial.bbox && extent.spatial.bbox[0];
    const interval = extent.temporal && extent.temporal.interval && extent.temporal.interval[0];
    const native = extent['x-smartmet-grid'] ||
        (extent['x-smartmet-grids'] && extent['x-smartmet-grids'][0]) || null;
    const vertical = extent.vertical;
    producers.set(c.id, {
      name: c.id,
      title: c.title,
      description: c.description,
      bbox: bbox && bbox.length >= 4 ? bbox.slice(0, 4) : null,
      time: interval ? {
        start: interval[0],
        end: interval[1],
        step: extent.temporal.grid && extent.temporal.grid.resolution
      } :
                       null,
      levels: vertical && vertical.grid && vertical.grid.coordinates ? [{
        type: vertical['x-smartmet-leveltype'] || 'level',
        values: vertical.grid.coordinates.map(String)
      }] :
                                                                       [],
      native: native,
      formats: (c.outputFormats || []).filter((f) => COVERAGE_FORMATS[f]),
      parameters: [],
      parameterSet: new Set(),
      schemaLoaded: false
    });
  }
  return {producers, parameters: new Map()};
}

// The largest side of the grid of the maps of a coverage
const COVERAGE_MAP_SIZE = 200;
// The panels of the maps of a coverage: one, side by side, three or four in a 2 x 2 grid
const COVERAGE_PANELS = [1, 2, 3, 4];

// The grid of the maps of a coverage over a box: cells about square on the ground, at most
// COVERAGE_MAP_SIZE on a side
function coverageMapSize(bbox)
{
  const [west, south, east, north] = bbox;
  const ratio = ((east - west) * Math.cos(rad((south + north) / 2))) / Math.max(1e-9, north - south);
  return ratio >= 1 ?
      [COVERAGE_MAP_SIZE, Math.max(2, Math.round(COVERAGE_MAP_SIZE / ratio))] :
      [Math.max(2, Math.round(COVERAGE_MAP_SIZE * ratio)), COVERAGE_MAP_SIZE];
}

// A time of GeoTIFF band metadata, 20080805T030000, in ISO 8601
function bandTime(t)
{
  const m = /^(\d{4})(\d\d)(\d\d)T(\d\d)(\d\d)(\d\d)$/.exec(t || '');
  return m ? `${m[1]}-${m[2]}-${m[3]}T${m[4]}:${m[5]}:${m[6]}Z` : t || '';
}

// Maps of a coverage read from a GeoTIFF, in linked panels: each panel has a parameter, a
// level and colours of its own, and all of them share the view, the time and the point
// clicked, which also shows the time series of each panel at the point. options: levelKind.
function drawCoverageMaps(container, tiff, settings, options = {})
{
  container.replaceChildren();
  // The values in the units shown: K and Pa of the files are recognised by their values
  const units = {};
  for (const p of new Set(tiff.bands.map((b) => b.parameter)))
  {
    const sample = [];
    for (const b of tiff.bands.filter((x) => x.parameter === p))
      for (let k = 0; k < b.values.length; k += 13)
        sample.push(b.values[k]);
    units[p] = displayUnit(p, sample);
  }
  const bands = tiff.bands.map((b) => Object.assign({}, b, {
    time: bandTime(b.time),
    values: Float32Array.from(b.values, units[b.parameter].convert)
  }));
  const params = Array.from(new Set(bands.map((b) => b.parameter)));
  const unitText = (p) => (units[p].label ? ` ${units[p].label}` : '');
  const times = Array.from(new Set(bands.map((b) => b.time))).sort();
  const byKey = new Map(bands.map((b) => [`${b.parameter}|${b.level}|${b.time}`, b]));
  const levelsOf = (p) => Array.from(new Set(bands.filter((b) => b.parameter === p).map((b) => b.level)))
                              .filter((v) => v !== null)
                              .sort((a, b) => a - b);
  const [west, south, east, north] = tiff.bbox;
  const cellOf = (lon, lat) => {
    const col = Math.floor(((lon - west) / (east - west)) * tiff.width);
    const row = Math.floor(((north - lat) / (north - south)) * tiff.height);
    return col < 0 || row < 0 || col >= tiff.width || row >= tiff.height ? null : {col, row};
  };
  // The centre of a cell, the point of the series
  const centreOf = ({col, row}) => ({
    lon: west + ((col + 0.5) / tiff.width) * (east - west),
    lat: north - ((row + 0.5) / tiff.height) * (north - south)
  });
  // The parameters and levels a panel can show: as many panels as there are of them
  const combinations = params.flatMap((p) => {
    const ls = levelsOf(p);
    return ls.length ? ls.map((l) => ({param: p, level: String(l)})) : [{param: p, level: ''}];
  });
  const shared = {time: 0, point: null};
  const count = el('select', {'aria-label': L('Panels')},
                   COVERAGE_PANELS.filter((n) => n <= combinations.length)
                       .map((n) => el('option', {value: String(n)}, n === 1 ? L('1 map') : L`${n} maps`)));
  count.value = String(Math.min(2, combinations.length));
  const slider = timeSlider(times.map((t) => new Date(t).getTime()), {
    label: L('Time of the maps'),
    onChange: () => {
      shared.time = slider.index();
      panels.forEach((p) => p.draw());
    }
  });
  const grid = el('div', {class: 'coverage-panels'});
  container.append(
      el('div', {class: 'row'}, el('label', null, L('Panels'), count), slider.element),
      el('p', {class: 'muted small'},
         L`${tiff.width} × ${tiff.height} cells, ${counted(params.length, L('parameter'), L('parameters'))}, ${
             counted(times.length, L('time'), L('times'))}. The maps move together.`),
      el('p', {class: 'map-hint'}, L('Click a value on a map for the time series of the point in every panel.')),
      grid);

  let panels = [];
  // A panel: its map, choices, legend and series of the point clicked
  const makePanel = () => {
    // The first parameter and level no other panel shows: a new parameter before a new level
    const used = new Set(panels.map((p) => `${p.state.param}|${p.state.level}`));
    const free = combinations.filter((c) => !used.has(`${c.param}|${c.level}`));
    const shownParams = new Set(panels.map((p) => p.state.param));
    const choice = free.find((c) => !shownParams.has(c.param)) || free[0] || combinations[0];
    const param = choice.param;
    const state = {
      param,
      level: choice.level,
      palette: PALETTES[quantityOf(param)] ? quantityOf(param) : 'viridis',
      min: null,
      max: null,
      time: 0,
      probe: null
    };
    const box = el('div', {class: 'coverage-panel'});
    const map = createMap(settings, null);
    map.setTools(['move'], 'move');
    const paramSelect = el('select', {'aria-label': L('Parameter of the map')},
                           params.map((p) => el('option', {value: p}, p)));
    paramSelect.value = state.param;
    const levelSelect = el('select', {'aria-label': L('Level of the map')});
    const paletteSelect = el('select', {'aria-label': L('Colours of the map')},
                             Object.keys(PALETTES).map((p) => el('option', {value: p}, p)));
    paletteSelect.value = state.palette;
    const legend = el('div', {class: 'legend'});
    const probeBox = el('div', {class: 'map-probe', hidden: true});
    box.append(el('div', {class: 'row'}, paramSelect, levelSelect, paletteSelect), legend, map.element, probeBox);
    grid.append(box);

    const band = () => byKey.get(`${state.param}|${state.level === '' ? null : Number(state.level)}|${times[shared.time]}`);
    // The colour range: the 2nd and 98th percentiles of all times of the parameter and level
    const setRange = () => {
      const values = [];
      for (const b of bands)
        if (b.parameter === state.param && (state.level === '' || b.level === Number(state.level)))
          for (let k = 0; k < b.values.length; k += 7)
            if (isFinite(b.values[k]))
              values.push(b.values[k]);
      values.sort((a, b) => a - b);
      [state.min, state.max] = values.length ? [quantile(values, 0.02), quantile(values, 0.98)] : [null, null];
    };
    const setLevels = () => {
      const ls = levelsOf(state.param);
      levelSelect.replaceChildren(...ls.map((v) => el('option', {value: String(v)}, `${v}`)));
      levelSelect.hidden = ls.length === 0;
      if (!ls.map(String).includes(state.level))
        state.level = ls.length ? String(ls[0]) : '';
      levelSelect.value = state.level;
    };
    // The series of the point clicked: the rows of the point for the chart of valueProbe
    let probe = null;
    const showProbe = () => {
      if (!shared.point)
        return;
      const cell = cellOf(shared.point.lon, shared.point.lat);
      if (!cell)
        return;
      const k = cell.row * tiff.width + cell.col;
      const at = centreOf(cell);
      const rows = bands.filter((b) => b.parameter === state.param).map((b) => ({
        lat: at.lat,
        lon: at.lon,
        time: b.time,
        level: b.level === null ? undefined : b.level,
        [state.param]: isFinite(b.values[k]) ? b.values[k] : null
      }));
      state.time = shared.time;
      probe = valueProbe(probeBox, rows, state, times, {
        levelKind: options.levelKind,
        unit: (p) => units[p].label,
        atLevel: (r) => state.level === '' || r.level === Number(state.level),
        onClose: () => {
          shared.point = null;
          panels.forEach((p) => p.closeProbe());
        }
      });
      probe.show(at);
    };
    const draw = () => {
      const b = band();
      const lo = state.min;
      const hi = state.max;
      const rgba = new Uint8ClampedArray(tiff.width * tiff.height * 4);
      // The colours of 256 steps of the range
      const lut = Array.from({length: 256}, (_, i) => paletteRgb(state.palette, i / 255, 0));
      if (b && lo !== null)
        for (let k = 0; k < b.values.length; k++)
        {
          const v = b.values[k];
          if (!isFinite(v))
            continue;
          const f = hi > lo ? (v - lo) / (hi - lo) : 0.5;
          const [r, g, bl] = lut[Math.max(0, Math.min(255, Math.round(f * 255)))];
          rgba[4 * k] = r;
          rgba[4 * k + 1] = g;
          rgba[4 * k + 2] = bl;
          rgba[4 * k + 3] = 210;
        }
      map.setRaster({
        bbox: tiff.bbox,
        width: tiff.width,
        height: tiff.height,
        rgba,
        label: (lon, lat) => {
          const cell = cellOf(lon, lat);
          if (!cell || !b)
            return null;
          const v = b.values[cell.row * tiff.width + cell.col];
          return {label: `${state.param}: ${isFinite(v) ? `${round(v, 2)}${unitText(state.param)}` : 'missing'}`,
                  info: {lon: round(lon, 3), lat: round(lat, 3)}, hint: L('Click for the time series')};
        }
      });
      const n = 24;
      legend.replaceChildren(
          el('span', {class: 'small'}, lo === null ? '' : round(lo, 2)),
          el('span', {class: 'legend-bar'},
             Array.from({length: n}, (_, i) => el('span', {
               class: 'legend-step',
               style: `background:${paletteColour(state.palette, (i + 0.5) / n, 0)}`
             }))),
          el('span', {class: 'small'}, hi === null ? '' : `${round(hi, 2)}${unitText(state.param)}`),
          b ? '' : el('span', {class: 'muted small'}, L(' no values at this time')));
      if (probe)
      {
        state.time = shared.time;
        probe.update();
      }
    };
    paramSelect.addEventListener('change', () => {
      state.param = paramSelect.value;
      setLevels();
      if (PALETTES[quantityOf(state.param)])
        paletteSelect.value = state.palette = quantityOf(state.param);
      setRange();
      draw();
      showProbe();
    });
    levelSelect.addEventListener('change', () => {
      state.level = levelSelect.value;
      setRange();
      draw();
      showProbe();
    });
    paletteSelect.addEventListener('change', () => {
      state.palette = paletteSelect.value;
      draw();
    });
    // The view and the point are shared by the panels
    map.onView((view) => panels.forEach((p) => p.map !== map && p.map.setView(view)));
    map.onClick((lon, lat) => {
      const cell = cellOf(lon, lat);
      if (!cell)
        return;
      shared.point = centreOf(cell);
      panels.forEach((p) => {
        p.map.setCursor(shared.point);
        p.showProbe();
      });
    });
    setLevels();
    setRange();
    return {
      map,
      box,
      state,
      draw,
      showProbe,
      closeProbe: () => {
        probe = null;
        state.probe = null;
        probeBox.hidden = true;
        probeBox.replaceChildren();
        map.setCursor(null);
      }
    };
  };
  // (Re)builds the panels; the view of the first one is kept
  const build = () => {
    const view = panels.length ? panels[0].map : null;
    const n = Number(count.value);
    grid.className = `coverage-panels panels-${n}`;
    for (const p of panels.slice(n))
      p.box.remove();
    panels = panels.slice(0, n);
    while (panels.length < n)
      panels.push(makePanel());
    panels.forEach((p) => p.draw());
    if (!view)
      panels[0].map.fit(tiff.bbox);
    // The new panels take the view of the first
    const v = panels[0].map.view();
    panels.slice(1).forEach((p) => p.map.setView(v));
    if (shared.point)
      panels.forEach((p) => {
        p.map.setCursor(shared.point);
        p.showProbe();
      });
  };
  count.addEventListener('change', build);
  build();
}

// The Coverages module: a collection, its parameters, a bounding box in a chosen CRS with the
// output area outlined on the map and the size of the result estimated, and the download link
function coveragesModule(app, api, state, save)
{
  const rootPath = api.root;
  const base = `${app.base}${rootPath.replace(/^\//, '')}`;
  const view = el('div', {class: 'module'});
  const status = el('p', {class: 'muted'}, L('Loading the collections…'));
  view.append(status);

  fetchJson(`${base}/collections`).then((doc) => build(buildCoverageIndex(doc))).catch((e) => {
    status.textContent = L`The collections could not be loaded: ${e.message}`;
  });

  // Builds the module once the collections have been loaded
  function build(index)
  {
    view.replaceChildren();
    const s = state;
    // Typing stores the value at once and redraws the request a moment later
    const refreshSoon = debounce(() => refresh(false), 300);
    s.params = s.params || [];
    // The levels are a list here, but a comma separated text in the address of the page
    if (typeof s.levels === 'string')
      s.levels = s.levels.split(',').filter(Boolean);

    // The parameters come from the schemas of the collections, four at a time
    const queue = Array.from(index.producers.values());
    let loading = queue.length;
    // Loads the schemas of the collections one at a time for their parameters
    const loadNext = () => {
      const c = queue.shift();
      if (!c)
        return;
      fetchJson(`${base}/collections/${encodeURIComponent(c.name)}/schema`)
          .then((schema) => {
            for (const [name, prop] of Object.entries(schema.properties || {}))
            {
              const key = name.toLowerCase();
              c.parameters.push(name);
              c.parameterSet.add(name);
              let entry = index.parameters.get(key);
              if (!entry)
              {
                entry = {
                  key: key,
                  name: name,
                  description: prop.title || '',
                  unit: prop['x-ogc-unit'] || '',
                  producers: [],
                  names: new Map()
                };
                index.parameters.set(key, entry);
              }
              entry.producers.push(c.name);
              entry.names.set(c.name, name);
            }
            c.schemaLoaded = true;
          })
          .catch(
              () => {
                  // A collection without a schema is offered without parameters
              })
          .finally(() => {
            loading--;
            refresh(false);
            loadNext();
          });
    };
    for (let i = 0; i < 4; i++)
      loadNext();

    const collection = () => index.producers.get(s.collection);
    // The names of the chosen parameters in a collection (the schemas keep their case)
    const namesIn = (c) => s.params.map((key) => {
      const entry = index.parameters.get(key);
      return (entry && entry.names.get(c.name)) || key;
    });
    // Whether a collection has a parameter
    const has = (c, key) => {
      const entry = index.parameters.get(key);
      return !!(entry && entry.names.has(c.name));
    };
    const selection = () => parseSelection('bbox', s.location);
    // The collections having all the chosen parameters and covering the box
    const candidates = () => {
      const sel = selection();
      return Array.from(index.producers.values())
          .filter((c) => s.params.every((p) => has(c, p)) && covers(c, sel));
    };

    // --- What -----------------------------------------------------------
    const chips = el('div', {class: 'chips'});
    const note = el('p', {class: 'muted small'});
    const card = el('div', {class: 'model-card'});

    const paramSearch = autocomplete({
      label: L('Parameter'),
      placeholder: L('Type a parameter name or description'),
      search: (text) => {
        const q = text.toLowerCase();
        const c = collection();
        const out = [];
        for (const entry of index.parameters.values())
        {
          if (s.params.includes(entry.key) || (c && !entry.names.has(c.name)))
            continue;
          const score = matchScore(q, entry.name, `${entry.description} ${entry.key}`);
          if (score !== null)
            out.push({
              score,
              key: entry.key,
              label: entry.name,
              detail:
                  [entry.description, entry.unit && `[${entry.unit}]`].filter(Boolean).join(' '),
              badge: c ?
                  null :
                  counted(entry.producers.length, L('collection'), L('collections'))
            });
        }
        out.sort((a, b) => a.score - b.score || a.label.localeCompare(b.label));
        return out;
      },
      select: (item) => {
        s.params.push(item.key);
        refresh();
      },
      empty: (text) => L`No parameter matches "${text}"`
    });

    const collectionSearch = autocomplete({
      label: L('Collection'),
      placeholder: L('Type a collection name'),
      search: (text) => {
        const q = text.toLowerCase();
        return candidates()
            .map(
                (c) =>
                    ({c, score: matchScore(q, c.name, `${c.title || ''} ${c.description || ''}`)}))
            .filter((x) => x.score !== null)
            .sort((a, b) => a.score - b.score || a.c.name.localeCompare(b.c.name))
            .map(({c}) => ({
                   key: c.name,
                   label: c.name,
                   detail: [projectionName(c.native && c.native.wkt), timeRangeOf(c)]
                               .filter(Boolean)
                               .join(' · ')
                 }));
      },
      select: (item) => {
        s.collection = item.key;
        defaultTimes(true);
        refresh();
        loadNative();
      },
      empty: () => L('No collection has all the chosen parameters'),
      limit: 200
    });

    // The period of the data of a collection for display
    function timeRangeOf(c)
    {
      if (!c.time || !c.time.start || !c.time.end)
        return '';
      return `${formatTime(new Date(c.time.start))} – ${formatTime(new Date(c.time.end))}`;
    }

    // --- When and the levels -------------------------------------------------
    const startInput = timeInput({'aria-label': L('Start time (UTC)')});
    const endInput = timeInput({'aria-label': L('End time (UTC)')});
    const dataPeriod = el('span', {class: 'muted small'});
    const levels = el('div', {class: 'chips'});
    const levelRow = el('div', null, el('label', null, L('Levels (all if none is chosen)')), levels);
    startInput.addEventListener('change', () => {
      s.start = startInput.value;
      refresh();
    });
    endInput.addEventListener('change', () => {
      s.end = endInput.value;
      refresh();
    });

    // The default period: the first day of the data
    function defaultTimes(force)
    {
      const c = collection();
      if (!c || !c.time || (!force && s.start && s.end))
        return;
      // The first day of the data
      const first = new Date(c.time.start);
      const last = new Date(c.time.end);
      s.start = toInput(first);
      s.end = toInput(new Date(Math.min(last.getTime(), first.getTime() + 24 * 3600e3)));
    }

    // --- Where -------------------------------------------------------------
    const locInput = el('input', {
      type: 'text',
      'aria-label': L('Box'),
      spellcheck: 'false',
      placeholder: L('The whole area, or minlon,minlat,maxlon,maxlat: drag on the map')
    });
    const crsSelect =
        el('select',
           {'aria-label': L('Output projection')},
           coverageCrs().map(([v, label]) => el('option', {value: v}, label)),
           el('option', {value: 'other'}, L('Other EPSG code…')));
    const crsInput =
        el('input', {type: 'text', 'aria-label': L('EPSG code'), placeholder: 'EPSG:3006', size: 12});
    const widthInput =
        el('input',
           {type: 'number', min: 1, 'aria-label': L('Grid width'), placeholder: L('width'), size: 6});
    const heightInput =
        el('input',
           {type: 'number', min: 1, 'aria-label': L('Grid height'), placeholder: L('height'), size: 6});
    const areaNote = el('p', {class: 'warn small'});
    const estimate = el('p', {class: 'size-estimate small'});
    // The points of a result belong to the location they were for: a new selection clears them
    const map = createMap(mapSettings(app.element), (shape) => {
      s.location = formatSelection(shape);
      map.setPoints([]);
      refresh();
    });
    locInput.addEventListener('input', () => {
      s.location = locInput.value.trim();
      refreshSoon();
    });
    crsSelect.addEventListener('change', () => {
      s.crs = crsSelect.value === 'other' ? (crsInput.value.trim() || 'other') : crsSelect.value;
      refresh();
    });
    crsInput.addEventListener('input', debounce(() => {
                                s.crs = crsInput.value.trim() || 'other';
                                refresh(false);
                              }, 400));
    for (const [input, key] of [[widthInput, 'width'], [heightInput, 'height']])
      input.addEventListener('input', debounce(() => {
                               s[key] = input.value.trim();
                               refresh(false);
                             }, 400));

    // The native grid of the collection on the map
    let nativeOutline = null;
    // Loads the outline of the native grid of the collection, drawn on the map
    function loadNative()
    {
      const c = collection();
      nativeOutline = null;
      if (!c)
        return;
      fetchJson(`${base}/collections/${encodeURIComponent(c.name)}/outline`)
          .then((doc) => {
            if (s.collection !== c.name)
              return;
            nativeOutline = doc.geometry && doc.geometry.coordinates && doc.geometry.coordinates[0];
            refresh(false);
            const extent = map.extent();
            if (extent)
              map.fit(extent);
          })
          .catch(() => {});
    }

    // The area and the size of the download, asked from the server as the options change
    let outlineRequest = 0;
    const updateOutline = debounce(() => {
      const c = collection();
      if (!c)
        return;
      const id = ++outlineRequest;
      fetchJson(`${base}/collections/${encodeURIComponent(c.name)}/outline?${
                    queryString(options(false))}`)
          .then((doc) => {
            if (id !== outlineRequest)
              return;
            const p = doc.properties || {};
            const grid = p.grid ?
                (p.grid.estimated ? L`${p.grid.width} × ${p.grid.height} grid (estimated)` :
                                    L`${p.grid.width} × ${p.grid.height} grid`) :
                L('grid of an unknown size');
            estimate.textContent =
                L`${grid}, ${counted(p.parameters, L('parameter'), L('parameters'))}, ${
                    counted(p.times, L('time'), L('times'))}${
                    p.levels > 1 ? L`, ${p.levels} levels` : ''}` +
                (p.values ? L`: ${Math.round(p.values).toLocaleString('en')} values, about ${
                                formatBytes(p.bytes)} as 32 bit floats` :
                            '');
            outline = doc.geometry && doc.geometry.coordinates && doc.geometry.coordinates[0];
            drawRegions();
          })
          .catch((e) => {
            if (id === outlineRequest)
            {
              estimate.textContent = '';
              areaNote.textContent = e.message;
              outline = null;
              drawRegions();
            }
          });
    }, 300);
    let outline = null;

    // The native grid and the area of the download on the map
    function drawRegions()
    {
      const regions = [];
      if (nativeOutline)
        regions.push({polygon: nativeOutline, label: L('The native grid')});
      if (outline && (s.location || s.crs))
        regions.push({polygon: outline, label: L('The area of the download'), cls: 'xmap-outline'});
      map.setRegions(regions);
    }

    // --- Output and the request ----------------------------------------------
    const formatSelect = el('select', {'aria-label': L('Output format')});
    formatSelect.addEventListener('change', () => {
      s.format = formatSelect.value;
      refresh();
    });
    const urlBox = el('code', {class: 'url'});
    const missing = el('p', {class: 'missing'});
    const download =
        el('a', {class: 'download', download: '', 'aria-disabled': 'true'}, L('Download'));
    // Errors of the server, below the request
    const requestStatus = requestNote();
    // A link cannot send the API key: with a key the file is fetched with it and saved
    download.addEventListener('click', async (e) => {
      if (!apiKey || download.getAttribute('aria-disabled') === 'true')
        return;
      e.preventDefault();
      requestStatus.clear();
      const response = await fetch(download.href, {headers: apiHeaders()});
      if (!response.ok)
      {
        requestStatus.show('error', serverError(response, await response.text()));
        return;
      }
      const name = (/filename="?([^";]+)/.exec(response.headers.get('content-disposition') || '') || [])[1] ||
          new URL(download.href).pathname.split('/').pop() || 'download';
      saveFile(await response.blob(), name);
    });
    const copyUrl = el('button', {type: 'button', class: 'small'}, L('Copy URL'));
    const code = el('div', {class: 'code-box'});
    const codeDetails =
        el('details',
           {class: 'code-details', open: true},
           el('summary', null, L('Code for this request')),
           code);
    copyUrl.addEventListener(
        'click', () => copyText(currentUrl(), copyUrl));

    // The maps of the coverage: a small GeoTIFF in longitude and latitude read by the browser
    const showMaps = el('button', {type: 'button', class: 'small'}, L('Show maps'));
    const mapsNote = el('p', {class: 'muted small'});
    const maps = el('div', {class: 'coverage-maps'});
    // The request of the maps drawn, to tell when they no longer match the form
    let shownMaps = null;
    // The URL of the maps: the request in EPSG:4326 on a grid of at most COVERAGE_MAP_SIZE cells a side
    function mapsUrl()
    {
      const c = collection();
      const box = selection() ? selection().bbox : c && c.bbox;
      if (!c || !box)
        return null;
      const [w, h] = coverageMapSize(box);
      const query = options(false)
                        .filter(([k]) => !['crs', 'scale-size', 'f'].includes(k))
                        .concat([['crs', 'EPSG:4326'], ['scale-size', `Lon(${w}),Lat(${h})`], ['f', 'geotiff']]);
      return new URL(`${rootPath}/collections/${encodeURIComponent(c.name)}/coverage?${queryString(query)}`,
                     app.base).href;
    }
    // Whether the maps can be drawn: a collection with GeoTIFF output and parameters
    function mapsPossible()
    {
      const c = collection();
      return !!(c && c.formats.includes('image/tiff') && s.params.length && mapsUrl());
    }
    showMaps.addEventListener('click', async () => {
      const url = mapsUrl();
      if (!url)
        return;
      showMaps.disabled = true;
      mapsNote.textContent = L('Loading the maps…');
      try
      {
        const response = await fetch(url, {headers: apiHeaders()});
        if (!response.ok)
          throw new Error(serverError(response, await response.text()));
        const tiff = await readGeoTiff(await response.arrayBuffer());
        mapsNote.textContent = '';
        shownMaps = url;
        showMaps.textContent = L('Update the maps');
        const c = collection();
        const draw = () => drawCoverageMaps(maps, tiff, mapSettings(app.element),
                                            {levelKind: c && c.levels.length ? c.levels[0].type : ''});
        draw();
        // Drawn again in the units and the clock chosen
        onSettingsChange('coverages', () => maps.isConnected && draw());
      }
      catch (e)
      {
        mapsNote.textContent = L`The maps could not be drawn: ${e.message}`;
      }
      showMaps.disabled = !mapsPossible();
    });

    // The options of the coverage and the outline requests; 'withFormat' adds f
    function options(withFormat)
    {
      const c = collection();
      const crs = s.crs && s.crs !== 'other' ? s.crs : '';
      return [
        ['properties', c ? namesIn(c).join(',') : ''],
        ['datetime', s.start && s.end ? `${fromInput(s.start)}/${fromInput(s.end)}` : ''],
        ['bbox', selection() ? selection().bbox.join(',') : ''],
        ['crs', crs],
        ['scale-size', s.width && s.height ? `Lon(${s.width}),Lat(${s.height})` : ''],
        ['levels', (s.levels || []).join(',')],
        ['f', withFormat ? s.format : '']
      ];
    }

    // The URL of the download
    function currentUrl()
    {
      const id = encodeURIComponent(s.collection || '{collectionId}');
      return new URL(
                 `${rootPath}/collections/${id}/coverage?${queryString(options(true))}`, app.base)
          .href;
    }

    // Redraws the form and the request after a change; rerender = false keeps the text being typed
    function refresh(rerender = true)
    {
      const c = collection();
      chips.replaceChildren(...s.params.map((key) => {
        const entry = index.parameters.get(key);
        const label = entry ? entry.name : key;
        return el(
            'span',
            {class: 'chip', title: entry ? entry.description : ''},
            label,
            el('button',
               {
                 type: 'button',
                 class: 'chip-x',
                 'aria-label': L`Remove ${label}`,
                 onclick: () => {
                   s.params = s.params.filter((x) => x !== key);
                   refresh();
                 }
               },
               '×'));
      }));
      const n = candidates().length;
      const pending = loading > 0 ? L` (reading the parameters of ${loading} more)` : '';
      note.textContent =
          (s.params.length > 0 ?
               L`${n} of ${index.producers.size} collections have ${
                   s.params.length === 1 ?
                       L('this parameter') :
                       L('all these parameters')}${selection() ? L(' in the selected area') : ''}` :
               L`${index.producers.size} collections`) +
          pending;

      if (c)
      {
        const nat = c.native || {};
        card.replaceChildren(
            el('div',
               {class: 'model-head'},
               el('strong', null, c.name),
               el('button',
                  {
                    type: 'button',
                    class: 'small',
                    onclick: () => {
                      s.collection = '';
                      nativeOutline = null;
                      refresh();
                    }
                  },
                  L('Change'))),
            el('div',
               {class: 'muted small'},
               [
                 projectionName(nat.wkt),
                 nat.width ? `${nat.width} × ${nat.height} grid` : '',
                 nat.resolution_km ? `${nat.resolution_km[0].toFixed(1)} km` : ''
               ].filter(Boolean)
                   .join(' · ')),
            el('div', {class: 'muted small'}, [
              timeRangeOf(c),
              c.time && c.time.step ? `step ${c.time.step}` : '',
              `${c.parameters.length} parameters`,
              c.levels.length ? `${c.levels[0].values.length} levels` : ''
            ].filter(Boolean).join(' · ')));
        card.hidden = false;
        collectionSearch.element.hidden = true;
      }
      else
      {
        card.hidden = true;
        collectionSearch.element.hidden = false;
      }

      limitPeriod(s, startInput, endInput, c && c.time ? periodOf(c.time.start, c.time.end) : {min: '', max: ''});
      startInput.value = s.start || '';
      endInput.value = s.end || '';
      dataPeriod.textContent = c && timeRangeOf(c) ? L`Data: ${timeRangeOf(c)}` : '';
      const values = c && c.levels.length ? c.levels[0].values : [];
      s.levels = (s.levels || []).filter((v) => values.includes(v));
      levels.replaceChildren(...values.map(
          (v) =>
              el('button',
                 {
                   type: 'button',
                   class: `chip${s.levels.includes(v) ? '' : ' meta'}`,
                   'aria-pressed': s.levels.includes(v) ? 'true' : 'false',
                   onclick: () => {
                     s.levels =
                         s.levels.includes(v) ? s.levels.filter((x) => x !== v) : [...s.levels, v];
                     refresh();
                   }
                 },
                 v)));
      levelRow.hidden = values.length === 0;

      if (rerender !== false)
      {
        locInput.value = s.location || '';
        widthInput.value = s.width || '';
        heightInput.value = s.height || '';
      }
      const known = coverageCrs().some(([v]) => v === (s.crs || ''));
      crsSelect.value = known ? (s.crs || '') : 'other';
      crsInput.hidden = known;
      if (!known && rerender !== false)
        crsInput.value = s.crs === 'other' ? '' : s.crs;
      map.setTools(['move', 'box'], 'box');
      const sel = selection();
      map.setSelection(sel);
      areaNote.textContent = c && sel && !covers(c, sel) ?
          L`The box is outside the area of ${c.name}` :
          (s.location && !sel ? L('Not a valid box: minlon,minlat,maxlon,maxlat') : '');
      drawRegions();

      const formats = c ? c.formats : Object.keys(COVERAGE_FORMATS);
      formatSelect.replaceChildren(...formats.map(
          (f) => el('option', {value: COVERAGE_FORMATS[f][0]}, COVERAGE_FORMATS[f][1])));
      const fs = formats.map((f) => COVERAGE_FORMATS[f][0]);
      if (!fs.includes(s.format))
        s.format = fs.includes('netcdf') ? 'netcdf' : fs[0];
      formatSelect.value = s.format || '';

      const url = currentUrl();
      urlBox.textContent = url;
      const absent = [];
      if (!s.collection)
        absent.push(L('a collection'));
      if (s.params.length === 0)
        absent.push(L('a parameter'));
      missing.textContent = absent.length ? L`Still needed: ${absent.join(', ')}` : '';
      if (absent.length === 0)
      {
        download.setAttribute('href', url);
        download.setAttribute('aria-disabled', 'false');
      }
      else
      {
        download.removeAttribute('href');
        download.setAttribute('aria-disabled', 'true');
      }
      codeTabs(code, url);
      showMaps.disabled = !mapsPossible();
      if (shownMaps && mapsUrl() !== shownMaps)
        mapsNote.textContent = L('The request has changed: update the maps to show it');
      else if (shownMaps)
        mapsNote.textContent = '';
      showMaps.title = mapsPossible() ? mapsUrl() :
          L('The maps need a collection with GeoTIFF output and a parameter');
      if (c)
        updateOutline();
      else
        estimate.textContent = '';
      save();
    }

    // A step of the form with its heading
    const section = (title, ...content) =>
        el('section', {class: 'step'}, el('h2', null, title), ...content);
    view.append(
        el('div',
           {class: 'form'},
           section(
               L('What'),
               el('label', null, L('Collection')),
               note,
               collectionSearch.element,
               card,
               el('label', null, L('Parameters')),
               chips,
               paramSearch.element),
           section(
               L('When'),
               el('div',
                  {class: 'row'},
                  el('label', null, L('Start (UTC)'), startInput),
                  el('label', null, L('End (UTC)'), endInput)),
               dataPeriod,
               levelRow),
           section(L('Where'), el('div', {class: 'row'}, locInput), areaNote, map.element),
           section(
               L('Output'),
               el('div', {class: 'row'}, el('label', null, L('Projection'), crsSelect), crsInput),
               el('div',
                  {class: 'row'},
                  el('label',
                     null,
                     L('Grid size (optional)'),
                     el('span', {class: 'row'}, widthInput, '×', heightInput))),
               el('div', {class: 'row'}, el('label', null, L('Format'), formatSelect)))),
        el('div',
           {class: 'request'},
           el('div', {class: 'url-row'}, urlBox, copyUrl),
           requestStatus.element,
           missing,
           estimate,
           el('div', {class: 'row'}, download),
           codeDetails,
           el('section', {class: 'coverage-maps-step'},
           el('h2', null, L('Maps')),
           el('p', {class: 'muted small'},
              L`The request in longitude and latitude on a grid of at most ${COVERAGE_MAP_SIZE} cells a side, in up to four linked maps`),
           el('div', {class: 'row'}, showMaps),
           mapsNote,
           maps)));
    defaultTimes(false);
    refresh();
    loadNative();
  }
  return view;
}

// ------------------------------------------------------------------
// Browsing the data
// ------------------------------------------------------------------

// The kinds of data, recognised from the names, descriptions, sources and levels: id, label
const DATA_KINDS = [
  ['forecast', L('Forecast models')],
  ['ensemble', L('Ensembles')],
  ['observations', L('Observations')],
  ['soundings', L('Soundings')],
  ['radar', L('Radar')],
  ['satellite', L('Satellite')],
  ['marine', L('Marine')],
  ['airquality', L('Air quality')],
  ['climate', L('Climate')]
];

// The themes of the parameters, recognised from their names: id, label, pattern
const DATA_THEMES = [
  ['temperature', L('Temperature'), /temperature|^t2m|dewpoint|^t-[kc]|^td(-|$)|^ta_|^tamax|^tamin|^tmax|^tmin/],
  ['wind', L('Wind'), /wind|^ws(_|$)|^wd(_|$)|gust|^ff(-|_)|^dd(-|_)|^[uv]-ms|^wg_/],
  ['precipitation', L('Precipitation'), /precip|rain|^rr(_|-)|^pr_|^ri_|snowfall|^prec/],
  ['cloud', L('Clouds and fog'), /cloud|^n_|^n-|fog|ceiling|^cl_/],
  ['humidity', L('Humidity'), /humid|^rh(_|-|$)/],
  ['pressure', L('Pressure'), /pressure|^p_|^p-|pmsl|geopot|^z-m/],
  ['radiation', L('Radiation and sun'), /radiat|^glob|^sun|^uv|^lw|^sw|sunshine/],
  ['snow', L('Snow and ice'), /snow|^sd(_|-|$)|ice|frost/],
  ['sea', L('Sea'), /sea|wave|salin|current|^sst|waterlevel|watertemp/],
  ['air', L('Air quality'), /aqindex|^pm(10|25)|^no2|^no_|^o3|^so2|^co_|pollen|aerosol|concentration/],
  ['visibility', L('Visibility'), /visib|^vis(_|$)/]
];

// The kind of a dataset
function dataKind(d)
{
  const text = `${d.name} ${d.title || ''} ${d.description || ''}`.toLowerCase();
  if (/radar|tutka/.test(text))
    return 'radar';
  if (/satel/.test(text))
    return 'satellite';
  if (/sounding|luotaus|radiosond/.test(text))
    return 'soundings';
  if (/air ?quality|airquality|ilmanlaatu|silam/.test(text))
    return 'airquality';
  if (/climat|normal|ilmasto/.test(text))
    return 'climate';
  if (/(^|[^a-z])(sea|wave|marine|ocean|hbm|meri|aalto)/.test(text))
    return 'marine';
  if (d.sources.has('observation'))
    return 'observations';
  if (d.levelTypes.has('member') || /(^|[^a-z])(eps|ens|ensemble)([^a-z]|$)/.test(text))
    return 'ensemble';
  return 'forecast';
}

// The themes of a parameter by its key and name
function parameterThemes(key, name)
{
  const n = `${key} ${name || ''}`.toLowerCase();
  return DATA_THEMES.filter(([, , re]) => n.split(' ').some((w) => re.test(w))).map(([id]) => id);
}

// The datasets of the APIs of the server, one per name however many APIs offer it: name, title,
// description, bbox, time, sources, level types, parameters (key -> {name, description, unit}),
// and the APIs as {api, index, key, source}
async function loadDatasets(app, apis)
{
  const datasets = new Map();
  const dataset = (name) => {
    if (!datasets.has(name))
      datasets.set(name, {
        name,
        title: '',
        description: '',
        bbox: null,
        time: null,
        sources: new Set(),
        levelTypes: new Set(),
        parameters: new Map(),
        apis: []
      });
    return datasets.get(name);
  };
  // The fields a dataset does not have yet are taken from each API
  const fill = (d, from) => {
    d.title = d.title || from.title || '';
    d.description = d.description || from.description || '';
    d.bbox = d.bbox || from.bbox || null;
    if (from.time && (!d.time || String(from.time.end) > String(d.time.end)))
      d.time = from.time;
    for (const l of from.levels || [])
      d.levelTypes.add(l.type);
  };
  const url = (api, path) => `${app.base}${api.root.replace(/^\//, '')}${path}`;
  const loads = apis.map(async (api, index) => {
    try
    {
      if (api.kind === 'timeseries')
      {
        const doc = await fetchJson(url(api, '/api/producers'));
        for (const p of doc.producers || [])
        {
          const d = dataset(p.name);
          fill(d, p);
          d.sources.add(p.source);
          d.apis.push({api, index, key: p.name, source: p.source});
          for (const key of p.parameters || [])
          {
            const info = (doc.parameters || {})[key] || {};
            if (!d.parameters.has(key))
              d.parameters.set(key, {name: info.name || key, description: info.description || '', unit: info.unit || ''});
          }
        }
      }
      else if (api.kind === 'edr')
      {
        const edr = buildEdrIndex(await fetchJson(url(api, '/collections')));
        for (const c of edr.producers.values())
        {
          const d = dataset(c.name);
          fill(d, c);
          d.apis.push({api, index, key: c.name, parameters: c.parameters});
          for (const key of c.parameters)
          {
            const info = edr.parameters.get(key) || {};
            if (!d.parameters.has(key))
              d.parameters.set(key, {name: info.name || key, description: info.description || '', unit: info.unit || ''});
          }
        }
      }
      else if (api.kind === 'coverages')
      {
        const coverages = buildCoverageIndex(await fetchJson(url(api, '/collections')));
        for (const c of coverages.producers.values())
        {
          const d = dataset(c.name);
          fill(d, c);
          d.apis.push({api, index, key: c.name});
        }
      }
    }
    catch (e)
    {
      // The other APIs are shown
    }
  });
  await Promise.all(loads);
  const out = Array.from(datasets.values());
  for (const d of out)
  {
    d.kind = dataKind(d);
    d.themes = new Map();
    for (const [key, info] of d.parameters)
      for (const theme of parameterThemes(key, info.name))
        d.themes.set(theme, (d.themes.get(theme) || 0) + 1);
  }
  return out.sort((a, b) => a.name.localeCompare(b.name));
}

// How current the data of a period is: until a time ahead (forecasts) or until a time ago
function dataFreshness(time)
{
  if (!time || !time.end)
    return '';
  const end = new Date(time.end);
  if (isNaN(end))
    return '';
  const hours = (end.getTime() - Date.now()) / 3600e3;
  const span = (h) => {
    const a = Math.abs(h);
    return a < 48 ? L`${Math.round(a)} h` : a < 24 * 60 ? L`${Math.round(a / 24)} days` :
           a < 24 * 730 ? L`${Math.round(a / 24 / 30.44)} months` : L`${Math.round(a / 24 / 365.25)} years`;
  };
  return hours > 0 ? L`until ${formatTime(end)}, ${span(hours)} ahead` : L`until ${formatTime(end)}, ${span(hours)} ago`;
}

// The Browse page: the datasets of all the APIs as cards, narrowed by a search, the kinds of
// data, the themes of the parameters and a point on the map, each opening in the APIs offering
// it with the parameters searched for
function browseModule(app, apis, state, save, openApi)
{
  const view = el('div', {class: 'module browse'});
  const status = el('p', {class: 'muted'}, L('Loading the data of the APIs…'));
  view.append(status);
  loadDatasets(app, apis).then((datasets) => build(datasets));

  function build(datasets)
  {
    view.replaceChildren();
    const s = state;
    const search = el('input', {type: 'search', 'aria-label': L('Search the data'), spellcheck: 'false',
                                placeholder: L('Search names, descriptions and parameters, e.g. wind gust')});
    search.value = s.q || '';
    const kinds = el('div', {class: 'row browse-facets', role: 'group', 'aria-label': L('Kinds of data')});
    const themes = el('div', {class: 'row browse-facets', role: 'group', 'aria-label': L('Themes of the parameters')});
    const pointNote = el('div', {class: 'row small'});
    const summary = el('p', {class: 'muted small'});
    const cards = el('div', {class: 'browse-cards'});
    const map = createMap(mapSettings(app.element), (shape) => {
      if (shape.type === 'point')
      {
        s.point = `${shape.lon},${shape.lat}`;
        refresh();
      }
    });
    map.setTools(['move', 'point'], 'point');
    view.append(
        el('div', {class: 'browse-layout'},
           el('div', null,
              el('div', {class: 'row'}, search),
              el('div', {class: 'small muted'}, L('Kind of data')), kinds,
              el('div', {class: 'small muted'}, L('Themes')), themes,
              pointNote, summary, cards),
           el('div', {class: 'browse-map'},
              el('p', {class: 'map-hint'}, L('Click the map for the data covering the point.')), map.element)));

    const chosenList = (key) => (s[key] ? s[key].split(',').filter(Boolean) : []);
    const point = () => {
      const m = /^(-?[\d.]+),(-?[\d.]+)$/.exec(s.point || '');
      return m ? [Number(m[1]), Number(m[2])] : null;
    };
    const covers = (d, p) => d.bbox && p[0] >= d.bbox[0] && p[0] <= d.bbox[2] && p[1] >= d.bbox[1] && p[1] <= d.bbox[3];
    // The parameters of a dataset matching the search
    const matching = (d, q) => Array.from(d.parameters).filter(([key, info]) =>
      matchScore(q, info.name || key, `${key} ${info.description}`) !== null);
    const matches = (d, q) => !q || matchScore(q, d.name, `${d.title} ${d.description}`) !== null || matching(d, q).length > 0;
    // A chip of a facet: the label, the count, chosen or not
    const facet = (key, id, label, count) => {
      const chosen = chosenList(key).includes(id);
      return el('button', {
        type: 'button',
        class: `chip facet${chosen ? '' : ' meta'}`,
        'aria-pressed': chosen ? 'true' : 'false',
        'data-facet': `${key}:${id}`,
        onclick: () => {
          const list = chosenList(key);
          s[key] = (chosen ? list.filter((x) => x !== id) : [...list, id]).join(',');
          refresh();
        }
      }, `${label} ${count}`);
    };

    // Opens a dataset in an API with the parameters searched for, or a first parameter
    const openIn = (d, entry, q) => {
      const found = q ? matching(d, q).map(([key]) => key) : [];
      const own = entry.parameters || (entry.api.kind === 'timeseries' ? Array.from(d.parameters.keys()) : []);
      const usable = found.filter((k) => own.includes(k));
      const first = own.find((k) => parameterThemes(k, (d.parameters.get(k) || {}).name).includes('temperature')) || own[0];
      const params = (usable.length ? usable.slice(0, 5) : first ? [first] : []);
      const preset = entry.api.kind === 'timeseries' ? {producer: entry.key, source: entry.source, params} :
                                                        {collection: entry.key, params: entry.api.kind === 'edr' ? params : []};
      openApi(entry.index, preset);
    };

    // The card of a dataset
    const card = (d, q) => {
      const kindLabel = (DATA_KINDS.find(([id]) => id === d.kind) || [null, d.kind])[1];
      const found = q ? matching(d, q) : [];
      const params = Array.from(d.parameters);
      const list = (found.length ? found : params).slice(0, 200).map(([key, info]) =>
        el('li', null, el('strong', null, info.name || key),
           info.description && info.description !== (info.name || key) ? ` · ${info.description}` : '',
           info.unit ? ` [${info.unit}]` : ''));
      const box = el('div', {class: 'browse-card', 'data-dataset': d.name},
          el('div', {class: 'row'},
             el('strong', null, d.name),
             el('span', {class: `badge badge-kind`}, kindLabel),
             ...Array.from(d.sources).filter(Boolean).map((x) => el('span', {class: `badge badge-${x}`}, x))),
          d.title && d.title !== d.name ? el('div', null, d.title) : '',
          d.description && d.description !== d.title ? el('div', {class: 'small muted'}, d.description) : '',
          el('div', {class: 'small'},
             [d.time ? `${formatTime(new Date(d.time.start))} – ${dataFreshness(d.time)}` : '',
              d.parameters.size ? L`${d.parameters.size} parameters` : '',
              d.levelTypes.size ? Array.from(d.levelTypes).join(', ') : ''].filter(Boolean).join(' · ')),
          el('div', {class: 'row browse-themes'},
             ...DATA_THEMES.filter(([id]) => d.themes.has(id)).map(([id, label]) =>
                 el('span', {class: 'chip meta small'}, `${label} ${d.themes.get(id)}`))),
          params.length ?
              el('details', {class: 'small', open: found.length > 0 && found.length <= 12},
                 el('summary', null, found.length ? counted(found.length, L('parameter matches'), L('parameters match')) :
                                                     L('Parameters')),
                 el('ul', {class: 'browse-params'}, list)) :
              '',
          el('div', {class: 'row'},
             ...d.apis.map((entry) => el('button', {type: 'button', class: 'small', onclick: () => openIn(d, entry, q)},
                                         // An API offering the dataset from several sources names the source
                                         d.apis.filter((x) => x.api === entry.api).length > 1 ?
                                             L`Open in ${entry.api.title} (${entry.source || entry.key})` :
                                             L`Open in ${entry.api.title}`))));
      box.addEventListener('mouseenter', () => highlight(d));
      box.addEventListener('mouseleave', () => highlight(null));
      return box;
    };

    let shown = [];
    // The extents of the datasets shown on the map, the one pointed highlighted
    const highlight = (h) => {
      map.setRegions(shown.filter((d) => d.bbox).map((d) => ({
        bbox: d.bbox,
        label: d.name,
        cls: d === h ? 'xmap-region highlight' : 'xmap-region faint'
      })));
    };

    function refresh()
    {
      const q = (s.q || '').trim().toLowerCase();
      const p = point();
      const chosenKinds = chosenList('kinds');
      const chosenThemes = chosenList('themes');
      // The facets count the datasets the other choices leave
      const base = datasets.filter((d) => matches(d, q) && (!p || covers(d, p)));
      const byTheme = base.filter((d) => chosenThemes.every((t) => d.themes.has(t)));
      const byKind = base.filter((d) => !chosenKinds.length || chosenKinds.includes(d.kind));
      kinds.replaceChildren(...DATA_KINDS.map(([id, label]) => [id, label, byTheme.filter((d) => d.kind === id).length])
                                  .filter(([id, , n]) => n > 0 || chosenKinds.includes(id))
                                  .map(([id, label, n]) => facet('kinds', id, label, n)));
      themes.replaceChildren(...DATA_THEMES.map(([id, label]) => [id, label, byKind.filter((d) => d.themes.has(id)).length])
                                   .filter(([id, , n]) => n > 0 || chosenThemes.includes(id))
                                   .map(([id, label, n]) => facet('themes', id, label, n)));
      shown = byKind.filter((d) => chosenThemes.every((t) => d.themes.has(t)));
      pointNote.replaceChildren(...(p ? [
        el('span', null, L`Covering ${round(p[0], 2)}, ${round(p[1], 2)}`),
        el('button', {type: 'button', class: 'small', onclick: () => {
          delete s.point;
          refresh();
        }}, L('Anywhere'))
      ] : []));
      map.setSelection(p ? {type: 'point', lon: p[0], lat: p[1]} : null);
      summary.textContent = L`${shown.length} of ${datasets.length} datasets`;
      cards.replaceChildren(...shown.map((d) => card(d, q)));
      highlight(null);
      save();
    }
    search.addEventListener('input', debounce(() => {
      s.q = search.value;
      refresh();
    }, 250));
    refresh();
    if (!point())
    {
      // Global datasets would always make the view the whole world: the map shows the regional
      // ones, if there are any
      const regional = shown.map((d) => d.bbox).filter((b) => b && b[2] - b[0] < 300);
      map.fit(regional.length ? [Math.min(...regional.map((b) => b[0])), Math.min(...regional.map((b) => b[1])),
                                 Math.max(...regional.map((b) => b[2])), Math.max(...regional.map((b) => b[3]))] :
                                map.extent());
    }
  }
  return view;
}

// ------------------------------------------------------------------
// The application
// ------------------------------------------------------------------

const MODULES = {
  timeseries: timeSeriesModule,
  edr: edrModule,
  coverages: coveragesModule
};

// The last view of the explorer, opened again when the page is opened without one. The
// preferences of the page are kept in the local storage of the browser; no cookies are used.
const VIEW_KEY = 'smartmet-explorer-view';

// The state of the explorer from the address of the page (#api=...&params=a,b), or the last
// view shown when the address has none
function readState()
{
  let hash = location.hash.slice(1);
  if (!hash)
  {
    try
    {
      hash = localStorage.getItem(VIEW_KEY) || '';
    }
    catch (e)
    {
      hash = '';
    }
    if (hash)
      history.replaceState(null, '', `#${hash}`);
  }
  const p = new URLSearchParams(hash);
  const s = {};
  for (const [k, v] of p)
    s[k] = v;
  if (s.params)
    s.params = s.params.split(',').filter(Boolean);
  return s;
}

// The state of the explorer into the address of the page, empty values left out
function writeState(s)
{
  const p = new URLSearchParams();
  for (const [k, v] of Object.entries(s))
  {
    if (v === undefined || v === null || v === '' || (Array.isArray(v) && v.length === 0))
      continue;
    p.set(k, Array.isArray(v) ? v.join(',') : v);
  }
  history.replaceState(null, '', `#${p.toString()}`);
  try
  {
    localStorage.setItem(VIEW_KEY, p.toString());
  }
  catch (e)
  {
    // Not remembered
  }
}

// Starts over: the choices of the form and everything the page remembers in this browser (the
// units, the clock, the language and the drawing choices) are forgotten, except the API key, and
// the page opens the API in its defaults. The previous request stays in the history of the
// browser, so Back returns to it.
function resetExplorer(api)
{
  try
  {
    const keys = [];
    for (let i = 0; i < localStorage.length; i++)
      keys.push(localStorage.key(i));
    for (const key of keys)
      if (key && key.startsWith('smartmet-explorer-') && key !== APIKEY_KEY)
        localStorage.removeItem(key);
  }
  catch (e)
  {
    // Nothing remembered
  }
  history.pushState(null, '', `#${new URLSearchParams(api ? {api} : {}).toString()}`);
  location.reload();
}

// The settings of the page: the clock, the units, the language and the licences of the parts
function settingsMenu()
{
  const panel = el('div', {class: 'settings-panel', role: 'dialog', 'aria-label': L('Settings'), hidden: true});
  // A drawn gear: many systems have no font with the gear character
  const gear = svg('svg', {class: 'icon', viewBox: '0 0 24 24', width: 14, height: 14, 'aria-hidden': 'true'});
  gear.append(svg('path', {
    fill: 'currentColor',
    'fill-rule': 'evenodd',
    d: 'M10.3 2h3.4l.5 2.6 1.8.8 2.2-1.5 2.4 2.4-1.5 2.2.8 1.8 2.6.5v3.4l-2.6.5-.8 1.8 1.5 2.2-2.4 2.4-2.2-1.5-1.8.8-.5 2.6h-3.4l-.5-2.6-1.8-.8-2.2 1.5-2.4-2.4 1.5-2.2-.8-1.8L2 13.7v-3.4l2.6-.5.8-1.8-1.5-2.2 2.4-2.4 2.2 1.5 1.8-.8zM12 8.5a3.5 3.5 0 1 0 0 7 3.5 3.5 0 0 0 0-7z'
  }));
  const button = el('button', {type: 'button', class: 'small settings-button', 'aria-expanded': 'false',
                               'aria-label': L('Settings')}, gear, L('Settings'));
  const close = () => {
    panel.hidden = true;
    button.setAttribute('aria-expanded', 'false');
  };
  // The panel is built when opened, so that it shows the current choices
  const build = () => {
    const clockSelect = el('select', {'aria-label': L('Clock')},
                           [['24', '24-hour (13:00)'], ['12', '12-hour (1:00 PM)']].map(
                               ([v, l]) => el('option', {value: v}, l)));
    clockSelect.value = clock;
    // The languages, each by its own name; a new language reloads the page in it
    const languageSelect = el('select', {'aria-label': L('Language')},
                              [['en', 'English'], ...Object.entries(TRANSLATIONS).map(([code, l]) => [code, l.name || code])]
                                  .map(([v, l]) => el('option', {value: v}, l)));
    languageSelect.value = LANGUAGE;
    languageSelect.addEventListener('change', () => {
      try
      {
        localStorage.setItem(LANGUAGE_KEY, languageSelect.value);
      }
      catch (e)
      {
        // Not remembered
      }
      location.reload();
    });
    clockSelect.addEventListener('change', () => {
      clock = clockSelect.value;
      try
      {
        localStorage.setItem(CLOCK_KEY, clock);
      }
      catch (e)
      {
        // Not remembered
      }
      settingsChanged();
    });
    const licences = [
      ['SmartMet Server and its data explorer', 'MIT', 'Finnish Meteorological Institute',
       'https://github.com/fmidev/smartmet-library-spine'],
      ['uPlot charts', 'MIT', 'Leon Sorokin', 'https://github.com/leeoniya/uPlot'],
      ['Swagger UI API documentation', 'Apache 2.0', 'SmartBear Software', 'https://github.com/swagger-api/swagger-ui'],
      ['Natural Earth background map', L('Public domain'), 'Natural Earth', 'https://www.naturalearthdata.com/']
    ];
    panel.replaceChildren(
        el('div', {class: 'row settings-head'}, el('strong', null, L('Settings')),
           el('button', {type: 'button', class: 'small', 'aria-label': L('Close the settings'), onclick: close}, '×')),
        el('label', null, L('Time'), clockSelect),
        el('div', null, el('div', {class: 'small muted'}, L('Units')), unitsChooser(settingsChanged)),
        el('label', null, L('Language'), languageSelect),
        el('details', {class: 'small'}, el('summary', null, L('About and licences')),
           el('ul', null, licences.map(([what, licence, holder, url]) =>
               el('li', null, el('a', {href: url, target: '_blank', rel: 'noopener'}, what), ` · ${licence}, ${holder}`))),
           el('p', {class: 'muted'}, L('The colours of the production products come from the configuration of this server.'))));
  };
  button.addEventListener('click', () => {
    if (!panel.hidden)
    {
      close();
      return;
    }
    build();
    panel.hidden = false;
    button.setAttribute('aria-expanded', 'true');
  });
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape' && !panel.hidden)
      close();
  });
  return el('span', {class: 'settings'}, button, panel);
}

// Starts the explorer: reads the API catalog, offers its APIs, and shows the module of the
// chosen one, keeping the choices in the address of the page
async function main()
{
  const element = document.getElementById('explorer');
  if (!element)
    return;
  const title = element.dataset.title || 'SmartMet Server';
  document.documentElement.lang = LANGUAGE;
  try
  {
    DEFAULTS = JSON.parse(element.dataset.defaults || '{}') || {};
  }
  catch (e)
  {
    DEFAULTS = {};
  }
  displayUnits = initialUnits(DEFAULTS.units);
  clock = initialClock(DEFAULTS.clock);
  const base = new URL(element.dataset.catalog || '../api', location.href);
  // The base URL of the server: the catalog is at <base>api
  const app = {element: element, base: new URL('./', base).href};

  const header =
      el('header', null, el('h1', null, L('Data explorer')), el('span', {class: 'muted'}, title));
  const apiSelect = el('select', {'aria-label': L('API')});
  // The Browse page is always one click away, whichever API is open
  const browseButton = el('button', {type: 'button', class: 'browse-button'}, L('Browse all data'));
  // The API open when Reset is pressed, set when one is opened
  let openRoot = '';
  const resetButton = el('button',
                         {
                           type: 'button',
                           class: 'small reset-button',
                           title: L('Clear the choices and the settings of the page, except the API key'),
                           onclick: () => resetExplorer(openRoot)
                         },
                         L('Reset'));
  const content = el('main');
  element.replaceChildren(
      header,
      el('nav',
         null,
         browseButton,
         el('label', null, L('API '), apiSelect),
         el('a', {href: new URL('api/docs', app.base).href}, L('API documentation')),
         el('a', {href: base.href + '?f=html'}, L('API catalog')),
         apiKeyField(() => open(current, true)),
         resetButton,
         settingsMenu()),
      content);

  let catalog;
  try
  {
    const doc = await fetchJson(base.href);
    catalog = (doc.linkset || []).flatMap((c) => c.item || []);
  }
  catch (e)
  {
    content.replaceChildren(
        el('p', {class: 'error'}, L`The API catalog could not be loaded: ${e.message}`));
    return;
  }

  const apis = catalog.filter((item) => MODULES[item['x-smartmet-kind']])
                   .map((item) => ({
                          root: new URL(item.href, base).pathname,
                          title: item.title || item.href,
                          kind: item['x-smartmet-kind']
                        }));
  if (apis.length === 0)
  {
    content.replaceChildren(
        el('p', {class: 'muted'}, L('None of the APIs of this server can be explored yet.')));
    return;
  }

  let state = readState();
  // Browsing the data of all the APIs, then each API
  apiSelect.append(el('option', {value: 'browse'}, L('Browse the data of all the APIs')));
  apis.forEach(
      (a, i) => apiSelect.append(el('option', {value: String(i)}, `${a.title} (${a.root})`)));
  let current = state.api === 'browse' ? 'browse' : Math.max(0, apis.findIndex((a) => a.root === state.api));

  // Opens API i, or the Browse page; keep keeps the state read from the address of the page,
  // preset gives the state of an API opened from the Browse page
  const open = (i, keep, preset) => {
    current = i;
    apiSelect.value = String(i);
    browseButton.setAttribute('aria-pressed', String(i === 'browse'));
    const s = preset ? Object.assign({}, preset) : keep ? state : {};
    const save = () => writeState(s);
    openRoot = i === 'browse' ? 'browse' : apis[i].root;
    if (i === 'browse')
    {
      s.api = 'browse';
      content.replaceChildren(browseModule(app, apis, s, save, (index, p) => open(index, false, p)));
      return;
    }
    const api = apis[i];
    s.api = api.root;
    if (preset)
      save();
    content.replaceChildren(MODULES[api.kind](app, api, s, save));
  };
  browseButton.addEventListener('click', () => open('browse', false));
  apiSelect.addEventListener('change', () =>
    open(apiSelect.value === 'browse' ? 'browse' : Number(apiSelect.value), false));
  // An address edited by hand, or a link followed within the page, opens the state it gives.
  // The page writes its own state with replaceState, which does not fire hashchange.
  window.addEventListener('hashchange', () => {
    state = readState();
    open(state.api === 'browse' ? 'browse' : Math.max(0, apis.findIndex((a) => a.root === state.api)),
         true);
  });
  open(current, true);
}

root.SmartMetExplorer = {
  flattenRows,
  expandRow,
  locationOf,
  parseTime,
  quantile,
  matchScore,
  buildIndex,
  buildEdrIndex,
  splitVariants,
  parseNumberList,
  drawCharts,
  drawProfiles,
  drawSkewT,
  drawSection,
  drawCrossSection,
  hasCrossSection,
  pathPoints,
  hasProfiles,
  hasSounding,
  rowsCsv,
  unitsChooser,
  displayRows,
  quantityOf,
  viewImage,
  hasMeteogram,
  drawMeteogram,
  meteogramParameters,
  drawAlongPath,
  soundingAnalysis,
  contourSegments,
  flightRows,
  flightTimes,
  flightLevelPressure,
  flightLevelsBetween,
  verticalAxes,
  edrLevelKind,
  liftingCondensationLevel,
  moistAdiabat,
  dewpointFromHumidity,
  parameterKind,
  coverageRows,
  paletteColour,
  cellSize,
  projectionName,
  formatBytes,
  queryString,
  localeUnits,
  clockTemplate,
  displayUnit,
  formatTime,
  coverageCrs,
  locationModes,
  parseWkt,
  selectionWkt,
  timeText,
  parseTimeText,
  dataKind,
  parameterThemes,
  missingTexts: () => Array.from(MISSING_TEXTS),
  language: () => LANGUAGE,
  readGeoTiff
};

if (typeof document !== 'undefined')
{
  if (document.readyState === 'loading')
    document.addEventListener('DOMContentLoaded', main);
  else
    main();
}
})(typeof window !== 'undefined' ? window : globalThis);
