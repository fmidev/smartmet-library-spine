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
// code snippets, the response tree and the location map. The DOM is built from
// text nodes only, never from HTML strings.
// ======================================================================

(function(root) {
'use strict';

const S = root.SmartMetSwaggerUI;

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

function debounce(fn, ms)
{
  let timer = null;
  return (...args) => {
    clearTimeout(timer);
    timer = setTimeout(() => fn(...args), ms);
  };
}

async function fetchJson(url)
{
  const response = await fetch(url, {headers: {accept: 'application/json'}});
  if (!response.ok)
  {
    const text = await response.text();
    throw new Error(`${response.status} ${response.statusText}: ${text.slice(0, 300)}`);
  }
  return response.json();
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

function formatTime(date)
{
  return `${toInput(date).replace('T', ' ')} UTC`;
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
function locationOf(row)
{
  // Each level and each variant (a level or a member of a grid parameter key) is a series of
  // its own
  const level = (row.level !== undefined && row.level !== null ? ` · ${row.level}` : '') +
      (row._variant ? ` · ${row._variant}` : '');
  const coords = row.lat !== undefined && row.lat !== null ? `${row.lat},${row.lon}` : '';
  if (row._point !== undefined)
    return `${row.name !== null && row.name !== undefined ? row.name + ' ' : ''}(${
        coords || row._point})${level}`;
  if (row.name !== undefined && row.name !== null)
    return String(row.name) + level;
  return coords + level;
}

// The rows of a request whose columns are parameter keys standing for one parameter at some
// level or ensemble member: the row is split into a row per variant with the columns named by
// the parameter. columns maps a requested column to {name, variant}.
function splitVariants(rows, columns)
{
  if (!columns || columns.size === 0)
    return rows;
  const out = [];
  for (const row of rows)
  {
    const base = {};
    const variants = new Map();
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
    }
    if (variants.size === 0)
      out.push(row);
    for (const [variant, values] of variants)
      out.push(Object.assign({}, base, values, variant ? {_variant: variant} : {}));
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
  ['mean', 'Mean', 'mean_t', 'meandir_t'],
  ['min', 'Minimum', 'min_t'],
  ['max', 'Maximum', 'max_t'],
  ['sum', 'Sum', 'sum_t'],
  ['median', 'Median', 'median_t'],
  ['percentile', 'Percentile', 'percentile_t'],
  ['sdev', 'Standard deviation', 'sdev_t', 'sdevdir_t'],
  ['change', 'Change', 'change_t']
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

// Why a function does not suit a kind of parameter, or ''
function functionWarning(fn, kind)
{
  if (kind === 'category' && fn !== 'max' && fn !== 'min')
    return 'is a category: its mean or sum means nothing, the maximum gives the most severe class';
  if (kind === 'direction' && (fn === 'sum' || fn === 'change'))
    return 'is a direction: its sum or change means nothing';
  if (kind === 'amount' && fn === 'mean')
    return 'is an amount: the sum gives the total of the period';
  if (kind !== 'amount' && fn === 'sum')
    return 'is not an amount: its sum means nothing';
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

function autocomplete(options)
{
  const input = el('input', {
    type: 'search',
    class: 'ac-input',
    placeholder: options.placeholder,
    autocomplete: 'off',
    spellcheck: 'false',
    'aria-autocomplete': 'list',
    'aria-label': options.label
  });
  const list = el('ul', {class: 'ac-list', role: 'listbox', hidden: true});
  const box = el('div', {class: 'ac'}, input, list);
  let items = [];
  let active = -1;

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
    list.hidden = (items.length === 0 && !(input.value.trim() !== '' && options.empty)) ||
        document.activeElement !== input;
  }

  function update()
  {
    items = options.search(input.value.trim()).slice(0, options.limit || 30);
    active = items.length > 0 ? 0 : -1;
    render();
  }

  function choose(i)
  {
    const item = items[i];
    if (!item)
      return;
    options.select(item);
    if (options.clearOnSelect !== false)
      input.value = '';
    update();
    // Closed until the user types again
    list.hidden = true;
  }

  input.addEventListener('input', debounce(update, 60));
  input.addEventListener('focus', update);
  input.addEventListener('blur', () => {
    list.hidden = true;
  });
  input.addEventListener('keydown', (e) => {
    if (e.key === 'ArrowDown')
    {
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
      if (active >= 0)
      {
        choose(active);
        e.preventDefault();
      }
    }
    else if (e.key === 'Escape')
    {
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
    title: 'The data',
    sources: ['querydata', 'grid'],
    params: [
      ['producer', 'Producer of the value'],
      ['model', 'Model of the value'],
      ['origintime', 'Origin time of the model run'],
      ['modtime', 'Modification time of the data'],
      ['level', 'Level']
    ]
  },
  {
    title: 'The location',
    params: [
      ['name', 'Location name'],
      ['lat', 'Latitude'],
      ['lon', 'Longitude'],
      ['latlon', 'Latitude and longitude'],
      ['geoid', 'Location id'],
      ['elevation', 'Elevation of the location'],
      ['dem', 'Elevation from the elevation model'],
      ['region', 'Region'],
      ['country', 'Country'],
      ['iso2', 'Country code'],
      ['localtz', 'Time zone of the location'],
      ['population', 'Population']
    ]
  },
  {
    title: 'The station',
    sources: ['observation'],
    params: [
      ['fmisid', 'FMI station id'],
      ['wmo', 'WMO station id'],
      ['lpnn', 'LPNN station id'],
      ['stationname', 'Name of the observation station'],
      ['stationlat', 'Latitude of the station'],
      ['stationlon', 'Longitude of the station'],
      ['station_elevation', 'Elevation of the station'],
      ['distance', 'Distance to the station'],
      ['direction', 'Direction to the station'],
      ['stationtype', 'Type of the station'],
      ['modifiedlast', 'Last modification of the value']
    ]
  },
  {
    title: 'The time and the sun',
    params: [
      ['time', 'Time'],
      ['localtime', 'Local time'],
      ['epochtime', 'Seconds since 1970'],
      ['weekday', 'Day of the week'],
      ['sunelevation', 'Elevation angle of the sun'],
      ['sunazimuth', 'Azimuth of the sun'],
      ['dark', 'Dark (1) or light (0)'],
      ['sunrise', 'Time of sunrise'],
      ['sunset', 'Time of sunset'],
      ['daylength', 'Length of the day'],
      ['moonphase', 'Phase of the moon']
    ]
  }
];

const META_PARAMETERS = META_GROUPS.flatMap((g) => g.params);

// The details of the grid data of a column N, requested as @code-N
const GRID_DETAILS = [
  ['AT', 'model run', 'Analysis time of the model run'],
  ['LT', 'level type', 'Level type'],
  ['L', 'level', 'Level in the units of parameter keys'],
  ['FT', 'forecast type', 'Forecast type'],
  ['FN', 'member', 'Forecast (ensemble member) number'],
  ['GM', 'geometry', 'Geometry id of the grid'],
  ['P', 'producer', 'Producer'],
  ['G', 'generation', 'Generation (model run) name'],
  ['I', 'parameter key', 'The full parameter key'],
  ['F', 'file', 'The grid file'],
  ['LATLON', 'grid point', 'Coordinates of the grid point(s) used']
];
const META = new Map(META_PARAMETERS);

// The same producer name may come from several sources, for example querydata and grid
const producerKey = (source, name) => (source ? `${source}:${name}` : name);

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
// The map of the explorer: the SVG map of the Swagger plugin
// ------------------------------------------------------------------

function mapSettings(app)
{
  const d = app.dataset;
  const settings = d.mapTiles ?
      {
        tiles: d.mapTiles,
        tileSize: Number(d.mapTileSize) || 256,
        attribution: d.mapAttribution || ''
      } :
      (d.basemap ? {basemap: d.basemap, attribution: 'Natural Earth'} : {});
  if (d.palettes)
    settings.palettes = d.palettes;
  return settings;
}

// The isoband colours of the production products (palettes.json): loaded once
let palettesPromise = null;

function loadPalettes(url)
{
  if (!palettesPromise)
    palettesPromise = fetchJson(url).catch(() => ({palettes: {}, parameters: {}}));
  return palettesPromise;
}

// The background map of the explorer (basemap.json, see make-basemap.py): land, lakes and
// country borders as rings in Web Mercator world coordinates with their bounding boxes.
// Loaded once.
let basemapPromise = null;

function loadBasemap(url)
{
  if (!basemapPromise)
    basemapPromise = fetchJson(url).then((doc) => {
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
    });
  return basemapPromise;
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
  return null;
}

function boxesIntersect(a, b)
{
  return a[0] <= b[2] && b[0] <= a[2] && a[1] <= b[3] && b[1] <= a[3];
}

const round = (v, decimals) => Number(v.toFixed(decimals));

function createMap(settings, onSelect)
{
  const box = el('div', {class: 'xmap'});
  const surface = svg('svg', {class: 'xmap-svg', role: 'img', 'aria-label': 'Map'});
  const layers = {
    background: svg('g'),
    cells: svg('g'),
    regions: svg('g'),
    selection: svg('g'),
    points: svg('g'),
    draft: svg('g')
  };
  for (const layer of Object.values(layers))
    surface.append(layer);
  const tooltip = el('div', {class: 'xmap-tooltip', hidden: true});
  const tools = el('div', {class: 'xmap-tools', role: 'toolbar', 'aria-label': 'Map tools'});
  const zoom = el('div', {class: 'xmap-zoom'});
  const attribution = settings && settings.attribution ?
      el('div', {class: 'xmap-attribution'}, settings.attribution) :
      null;
  box.append(surface, tooltip, tools, zoom);
  if (attribution)
    box.append(attribution);

  const tileSize = (settings && settings.tileSize) || 256;
  let basemap = null;
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
  const state =
      {zoom: 3, center: project(25, 62), tool: 'move', regions: [], selection: null, points: []};

  const width = () => box.clientWidth || 400;
  const scale = () => 2 ** state.zoom;
  const toScreen = ([x, y]) =>
      [(x - state.center[0]) * scale() + width() / 2,
       (y - state.center[1]) * scale() + MAP_HEIGHT / 2];
  const toWorld = (sx, sy) =>
      [state.center[0] + (sx - width() / 2) / scale(),
       state.center[1] + (sy - MAP_HEIGHT / 2) / scale()];
  const screenOf = (lon, lat) => toScreen(project(lon, lat));
  const lonLatOf = (sx, sy) => unproject(...toWorld(sx, sy));

  // The land, the lakes and the borders in view, with at most a point per pixel
  function drawBasemap(g, x0, y0, x1, y1)
  {
    const k = scale();
    const cx = state.center[0];
    const cy = state.center[1];
    const w2 = width() / 2;
    const h2 = MAP_HEIGHT / 2;
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
    const land = path(basemap.land, true);
    if (land)
      g.append(svg('path', {class: 'xmap-land', d: land}));
    const lakes = path(basemap.lakes, true);
    if (lakes)
      g.append(svg('path', {class: 'xmap-lake', d: lakes}));
    const borders = path(basemap.borders, false);
    if (borders)
      g.append(svg('path', {class: 'xmap-border', d: borders}));
  }

  function drawBackground()
  {
    const g = layers.background;
    g.replaceChildren();
    const [x0, y0] = toWorld(0, 0);
    const [x1, y1] = toWorld(width(), MAP_HEIGHT);
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
      g.append(svg('line', {class: 'xmap-grid', x1: sx, y1: 0, x2: sx, y2: MAP_HEIGHT}));
      const t = svg('text', {class: 'xmap-label', x: sx + 2, y: MAP_HEIGHT - 4});
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

  function circlePath(sel, cls)
  {
    const points = [];
    for (let b = 0; b <= 360; b += 5)
      points.push(screenOf(...destination(sel.lon, sel.lat, sel.km, b)).join(','));
    return svg('polygon', {class: cls, points: points.join(' ')});
  }

  function drawShape(g, sel, cls)
  {
    if (!sel)
      return;
    if (sel.type === 'box')
      g.append(rect(sel.bbox, cls));
    else if (sel.type === 'circle')
      g.append(circlePath(sel, cls));
    if (sel.type === 'point' || sel.type === 'circle')
    {
      const [sx, sy] = screenOf(sel.lon, sel.lat);
      g.append(svg('circle', {class: 'xmap-centre', cx: sx, cy: sy, r: 5}));
    }
  }

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
          if (bx < 0 || by < 0 || ax > width() || ay > MAP_HEIGHT)
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
          if (sx < -10 || sy < -10 || sx > width() + 10 || sy > MAP_HEIGHT + 10)
            continue;
          shape = svg('circle', {class: 'xmap-dot', cx: sx, cy: sy, r: 6, fill: cell.color});
        }
        shape.addEventListener('mouseover', () => {
          const [sx, sy] = screenOf(cell.lon, cell.lat);
          showTooltip({label: cell.label, info: cell.info}, sx, sy);
        });
        shape.addEventListener('mouseout', () => {
          tooltip.hidden = true;
        });
        layers.cells.append(shape);
      }
    }

    layers.points.replaceChildren();
    // Many points are drawn smaller
    const radius = state.points.length > 500 ? 2 : state.points.length > 100 ? 3 : 4.5;
    for (const point of state.points)
    {
      const [sx, sy] = screenOf(point.lon, point.lat);
      if (sx < -10 || sy < -10 || sx > width() + 10 || sy > MAP_HEIGHT + 10)
        continue;
      const c = svg('circle', {class: 'xmap-point', cx: sx, cy: sy, r: radius});
      c.addEventListener('mouseover', () => showTooltip(point, sx, sy));
      c.addEventListener('mouseout', () => {
        tooltip.hidden = true;
      });
      layers.points.append(c);
    }
  }

  function showTooltip(point, sx, sy)
  {
    const rows = Object.entries(point.info || {})
                     .filter(([k, v]) => k !== 'name' && v !== null && v !== undefined && v !== '')
                     .map(([k, v]) => el('tr', null, el('td', null, k), el('td', null, v)));
    tooltip.replaceChildren(el('strong', null, point.label || ''), el('table', null, rows));
    tooltip.hidden = false;
    const left = sx + 12 + tooltip.offsetWidth > width() ? sx - 12 - tooltip.offsetWidth : sx + 12;
    tooltip.style.left = `${Math.max(0, left)}px`;
    tooltip.style.top = `${Math.max(0, Math.min(MAP_HEIGHT - tooltip.offsetHeight, sy - 10))}px`;
  }

  // One redraw per animation frame while the map is dragged
  let pending = false;
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
    surface.setAttribute('viewBox', `0 0 ${width()} ${MAP_HEIGHT}`);
    drawBackground();
    drawOverlays();
  }

  function zoomAt(z, sx, sy)
  {
    z = Math.max(0, Math.min(18, z));
    if (z === state.zoom)
      return;
    const world = toWorld(sx, sy);
    state.zoom = z;
    state.center =
        [world[0] - (sx - width() / 2) / scale(), world[1] - (sy - MAP_HEIGHT / 2) / scale()];
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
    while (z < 12 && dx * 2 ** (z + 1) < width() * 0.85 && dy * 2 ** (z + 1) < MAP_HEIGHT * 0.85)
      z++;
    state.zoom = z;
    redraw();
  }

  // --- Interaction ------------------------------------------------------
  let drag = null;
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
    drag = null;
    if (shape && onSelect)
      onSelect(shape);
  });

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
      button('+', 'Zoom in', () => zoomAt(state.zoom + 1, width() / 2, MAP_HEIGHT / 2)),
      button('−', 'Zoom out', () => zoomAt(state.zoom - 1, width() / 2, MAP_HEIGHT / 2)),
      button('Fit', 'Show everything on the map', () => fit(api.extent())));

  const TOOLS = [
    ['move', 'Move', 'Move the map'],
    ['point', 'Point', 'Click to choose a point'],
    ['circle', 'Radius', 'Drag from the centre to choose a radius'],
    ['box', 'Box', 'Drag to choose a box']
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
    fit,
    redraw,
    lonLatOf,
    screenOf
  };

  // Redraw when the width changes, e.g. on the first layout
  if (typeof ResizeObserver !== 'undefined')
  {
    let last = 0;
    new ResizeObserver(() => {
      if (box.clientWidth !== last)
      {
        last = box.clientWidth;
        redraw();
      }
    }).observe(box);
  }
  return api;
}

// ------------------------------------------------------------------
// Charts
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
    container.append(el('p', {class: 'muted'}, 'No data.'));
    return;
  }
  for (const [label, series] of groups)
    drawServerStatisticsGroup(container, series.sort((a, b) => a[0] - b[0]), parameters, label);
  if (container.children.length === 0)
    container.append(el('p', {class: 'muted'}, 'No numeric values to draw.'));
}

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
           `${param}${label ? ` · ${label}` : ''}: median and percentiles of the area, ` +
               'computed by the server'));
    const plot = el('div', {class: 'chart-plot'});
    panel.append(plot);
    // eslint-disable-next-line no-new
    new root.uPlot(
        {
          width: width,
          height: 260,
          series: [
            {value: (u, x) => (x === null ? '--' : formatTime(new Date(x * 1000)))},
            {label: '10 %', stroke: fill(0.5), width: 1},
            {label: '25 %', stroke: fill(0.7), width: 1},
            {label: 'median', stroke: palette[0], width: 2},
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

// UTC times in 24 hour format, with the date where the day changes
function utcTimeLabels(u, splits)
{
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

// The charts of the numeric parameters of the rows, one chart per parameter. 'mode' is
// "lines" or "percentiles"; the default depends on the number of locations. "server" draws
// the percentiles the server computed over an area (AREA_STATISTICS).
function drawCharts(container, rows, parameters, mode)
{
  if (mode === 'server')
  {
    drawServerStatistics(container, rows, parameters);
    return;
  }
  container.replaceChildren();
  if (rows.length === 0)
  {
    container.append(el('p', {class: 'muted'}, 'No data.'));
    return;
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
  const xs = Array.from(times.keys()).sort((a, b) => a - b);
  if (xs.length === 0)
  {
    container.append(
        el('p',
           {class: 'muted'},
           'The response has no times to draw: include time in the parameters.'));
    return;
  }

  const numeric = parameters.filter((p) => rows.some((r) => typeof r[p] === 'number'));
  if (numeric.length === 0)
  {
    container.append(el('p', {class: 'muted'}, 'No numeric values to draw.'));
    return;
  }

  const names = Array.from(locations.keys());
  const many = names.length > MAX_LINES;
  const chosen = mode || (many ? 'percentiles' : 'lines');

  // The choice of the drawing for several locations
  if (names.length > 2)
  {
    const toolbar = el('div', {class: 'tabs chart-modes', role: 'tablist'});
    for (const [m, label] of [
             ['lines', many ? 'All locations' : 'Lines'],
             ['percentiles', 'Percentiles']])
      toolbar.append(
          el('button',
             {
               type: 'button',
               role: 'tab',
               'data-mode': m,
               'aria-selected': m === chosen ? 'true' : 'false',
               onclick: () => drawCharts(container, rows, parameters, m)
             },
             label));
    container.append(toolbar);
  }

  // A swarm is ordered by latitude so that the gradient runs from south to north
  const ordered = names.slice();
  const byLatitude = names.every((n) => latitudes.get(n) !== null);
  if (byLatitude)
    ordered.sort((a, b) => latitudes.get(a) - latitudes.get(b));

  const width = Math.max(320, container.clientWidth - 8);
  const css = getComputedStyle(document.documentElement);
  const axisColor = css.getPropertyValue('--muted').trim() || '#666';
  const gridColor = css.getPropertyValue('--line').trim() || '#ddd';
  // UTC times in 24 hour format, with the date where the day changes
  const timeLabels = (u, splits) => {
    let previous = '';
    return splits.map((x) => {
      const d = new Date(x * 1000);
      const day = `${d.getUTCFullYear()}-${pad(d.getUTCMonth() + 1)}-${pad(d.getUTCDate())}`;
      const time = `${pad(d.getUTCHours())}:${pad(d.getUTCMinutes())}`;
      const label = day !== previous ? `${time}\n${day}` : time;
      previous = day;
      return label;
    });
  };
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
    const valueOf = (map, x) => {
      const row = map.get(x);
      const v = row ? row[param] : null;
      return typeof v === 'number' && isFinite(v) ? v : null;
    };

    // The percentiles over the locations at each time
    const percentiles = (qs) => {
      const columns = qs.map(() => []);
      for (const x of xs)
      {
        const values = [];
        for (const map of locations.values())
        {
          const v = valueOf(map, x);
          if (v !== null)
            values.push(v);
        }
        values.sort((a, b) => a - b);
        qs.forEach((q, i) => columns[i].push(quantile(values, q)));
      }
      return columns;
    };

    const panel = el('div', {class: 'chart', 'data-param': param, 'data-mode': chosen});
    container.append(panel);
    let data;
    let series;
    let bands = [];
    let caption;
    let legend = {live: true};

    if (chosen === 'lines' && !many)
    {
      data = [xs, ...names.map((n) => xs.map((x) => valueOf(locations.get(n), x)))];
      series = [
        timeSeries,
        ...names.map((n, i) => ({
                       label: n || param,
                       stroke: palette[i % palette.length],
                       width: 1.5,
                       spanGaps: false
                     }))
      ];
      caption = names.length > 1 ? `${param}, ${names.length} locations` : param;
    }
    else if (chosen === 'lines')
    {
      // Every location as a thin line without points, and the median on top
      data = [
        xs,
        ...ordered.map((n) => xs.map((x) => valueOf(locations.get(n), x))),
        percentiles([0.5])[0]
      ];
      series = [
        timeSeries,
        ...ordered.map((n, i) => ({
                         label: n,
                         stroke: swarmColour(i, ordered.length),
                         width: 1,
                         points: {show: false},
                         spanGaps: false
                       })),
        {
          label: 'median',
          stroke: css.getPropertyValue('--fg').trim() || '#000',
          width: 2,
          points: {show: false}
        }
      ];
      legend = {show: false};
      caption = `${param}: ${names.length} locations${
          byLatitude ? ', coloured from south to north' : ''}, and the median`;
    }
    else
    {
      // The median and the 10-90 % and 25-75 % ranges over the locations
      data = [xs, ...percentiles([0.1, 0.25, 0.5, 0.75, 0.9])];
      const fill = (a) => `rgba(47, 111, 223, ${a})`;
      series = [
        timeSeries,
        {label: '10 %', stroke: fill(0.5), width: 1},
        {label: '25 %', stroke: fill(0.7), width: 1},
        {label: 'median', stroke: palette[0], width: 2},
        {label: '75 %', stroke: fill(0.7), width: 1},
        {label: '90 %', stroke: fill(0.5), width: 1}
      ];
      bands = [{series: [5, 1], fill: fill(0.12)}, {series: [4, 2], fill: fill(0.22)}];
      caption = `${param}: median and percentiles of ${names.length} locations`;
    }

    panel.append(el('div', {class: 'chart-title'}, caption));
    const plot = el('div', {class: 'chart-plot'});
    panel.append(plot);
    // eslint-disable-next-line no-new
    new root.uPlot(
        {
          width: width,
          height: 260,
          series: series,
          bands: bands,
          axes: axes,
          scales: {x: {time: true}},
          legend: legend,
          tzDate: (ts) => root.uPlot.tzDate(new Date(ts * 1000), 'Etc/UTC')
        },
        data,
        plot);
  }
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

function hexColour(hex)
{
  return [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16));
}

// The colour of f = 0...1 on a palette; 'steps' > 0 quantizes the scale
function paletteColour(name, f, steps)
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
  return `rgb(${a.map((v, k) => Math.round(v + (b[k] - v) * u)).join(',')})`;
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

// The values of the rows on a map: a parameter, a time slider and a colour scale
function drawValueMap(container, rows, parameters, settings)
{
  container.replaceChildren();
  const numeric = parameters.filter((p) => rows.some((r) => typeof r[p] === 'number'));
  const located = rows.filter((r) => typeof r.lat === 'number' && typeof r.lon === 'number');
  if (numeric.length === 0 || located.length === 0)
  {
    container.append(el('p', {class: 'muted'}, 'No located numeric values to show.'));
    return;
  }

  const times = Array.from(new Set(located.map((r) => String(r.time))))
                    .sort((a, b) => (parseTime(a) || 0) - (parseTime(b) || 0));
  const state = {param: numeric[0], time: 0, palette: 'viridis', steps: 0, min: null, max: null};
  // The production palettes: bands of fixed values and colours
  let production = {palettes: {}, parameters: {}};
  const bandsOf = (name) => production.palettes[name.replace(/^production:/, '')];
  const isProduction = () => state.palette.startsWith('production:');

  const map = createMap(settings, null);
  map.setTools(['move'], 'move');
  const paramSelect =
      el('select', {'aria-label': 'Parameter'}, numeric.map((p) => el('option', {value: p}, p)));
  const paletteSelect =
      el('select',
         {'aria-label': 'Colours'},
         Object.keys(PALETTES).map((p) => el('option', {value: p}, p)));
  const productionGroup = el('optgroup', {label: 'Production isobands'});
  const stepsSelect =
      el('select',
         {'aria-label': 'Classes'},
         [['0', 'Continuous'], ['5', '5 classes'], ['8', '8 classes'], ['12', '12 classes']].map(
             ([v, t]) => el('option', {value: v}, t)));
  const minInput = el('input', {type: 'number', step: 'any', 'aria-label': 'Minimum', size: 6});
  const maxInput = el('input', {type: 'number', step: 'any', 'aria-label': 'Maximum', size: 6});
  const slider = el(
      'input',
      {type: 'range', min: 0, max: Math.max(0, times.length - 1), value: 0, 'aria-label': 'Time'});
  const timeLabel = el('span', {class: 'muted small'});
  const play = el('button', {type: 'button', class: 'small'}, 'Play');
  const legend = el('div', {class: 'legend'});
  container.append(
      el('div',
         {class: 'row'},
         el('label', null, 'Parameter', paramSelect),
         el('label', null, 'Colours', paletteSelect),
         el('label', null, 'Scale', stepsSelect),
         el('label', null, 'Min', minInput),
         el('label', null, 'Max', maxInput)),
      el('div', {class: 'row value-time'}, play, slider, timeLabel),
      legend,
      map.element);

  // The default range: the 2nd and the 98th percentiles of all times, so that the colours
  // keep their meaning when the time changes
  const defaultRange = () => {
    const values = located.map((r) => r[state.param])
                       .filter((v) => typeof v === 'number' && isFinite(v))
                       .sort((a, b) => a - b);
    return [quantile(values, 0.02), quantile(values, 0.98)];
  };

  let size = null;
  const draw = () => {
    const [lo, hi] = [state.min, state.max];
    const t = times[state.time];
    const items = located.filter((r) => String(r.time) === t).map((r) => {
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
        label: `${state.param}: ${ok ? v : 'missing'}`,
        info: {name: r.name, lat: r.lat, lon: r.lon}
      };
    });
    if (size === null)
      size = cellSize(items) || {dx: 0, dy: 0};
    map.setCells({items, dx: size.dx, dy: size.dy});
    const d = parseTime(t);
    timeLabel.textContent = d ? formatTime(d) : t;
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
  const setRange = () => {
    [state.min, state.max] = defaultRange();
    minInput.value = state.min === null ? '' : round(state.min, 2);
    maxInput.value = state.max === null ? '' : round(state.max, 2);
  };

  paramSelect.addEventListener('change', () => {
    state.param = paramSelect.value;
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
  slider.addEventListener('input', () => {
    state.time = Number(slider.value);
    draw();
  });
  let timer = null;
  play.addEventListener('click', () => {
    if (timer)
    {
      clearInterval(timer);
      timer = null;
      play.textContent = 'Play';
      return;
    }
    play.textContent = 'Stop';
    timer = setInterval(() => {
      if (!container.isConnected)
      {
        clearInterval(timer);
        return;
      }
      state.time = (state.time + 1) % times.length;
      slider.value = String(state.time);
      draw();
    }, 700);
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

function drawTable(container, rows)
{
  container.replaceChildren();
  if (rows.length === 0)
  {
    container.append(el('p', {class: 'muted'}, 'No data.'));
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
         el('thead', null, el('tr', null, columns.map((c) => el('th', null, c)))),
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
        el('p', {class: 'muted'}, `The first ${MAX_TABLE_ROWS} of ${rows.length} rows.`));
}

// ------------------------------------------------------------------
// Code snippets
// ------------------------------------------------------------------

function shellQuote(s)
{
  return `'${String(s).replace(/'/g, '\'\\\'\'')}'`;
}

function snippets(url)
{
  const request = {url: url, method: 'GET', headers: {}};
  // The file name extension of the response, as in the snippets of the Swagger plugin
  const ext = S && S.extension ? S.extension(new URL(url)) : 'json';
  const ret = [['bash', `curl --fail --output result.${ext} ${shellQuote(url)}\n`]];
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

function codeTabs(container, url)
{
  container.replaceChildren();
  const items = snippets(url);
  const pre = el('pre', {class: 'code'});
  const copy = el('button', {type: 'button', class: 'small'}, 'Copy');
  const tabs = el('div', {class: 'tabs', role: 'tablist'});
  let current = 0;
  const show = (i) => {
    current = i;
    pre.textContent = items[i][1];
    Array.from(tabs.children)
        .forEach((b, j) => b.setAttribute('aria-selected', j === i ? 'true' : 'false'));
  };
  items.forEach(
      ([title], i) =>
          tabs.append(el('button', {type: 'button', role: 'tab', onclick: () => show(i)}, title)));
  copy.addEventListener('click', () => {
    navigator.clipboard.writeText(items[current][1])
        .then(() => {
          copy.textContent = 'Copied';
          setTimeout(() => {
            copy.textContent = 'Copy';
          }, 1200);
        })
        .catch(() => {});
  });
  container.append(el('div', {class: 'code-head'}, tabs, copy), pre);
  show(0);
}

// ------------------------------------------------------------------
// The TimeSeries module
// ------------------------------------------------------------------

// The location types: name, label, example, and the map tool choosing the location
const LOCATION_MODES = [
  ['place', 'Place', 'Helsinki or Helsinki:20 for a 20 km radius', null],
  ['lonlat', 'Point', '24.94,60.17, or click the map', 'point'],
  ['radius', 'Radius', '24.94,60.17:25 (km), or drag on the map', 'circle'],
  ['bbox', 'Box', '21,60,26,62, or drag on the map', 'box'],
  ['keyword', 'Keyword', 'synop_fi', null],
  ['fmisid', 'Station id', '100971,101004', null],
  ['area', 'Area', 'Uusimaa', null]
];

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
  if (mode === 'bbox' && (m = new RegExp(`^${NUMBER},${NUMBER},${NUMBER},${NUMBER}$`).exec(text)))
  {
    const [a, b, c, d] = m.slice(1, 5).map(Number);
    return {type: 'box', bbox: [Math.min(a, c), Math.min(b, d), Math.max(a, c), Math.max(b, d)]};
  }
  return null;
}

function formatSelection(sel)
{
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

function timeSeriesModule(app, api, state, save)
{
  const rootPath = api.root;
  const view = el('div', {class: 'module'});
  const status = el('p', {class: 'muted'}, 'Loading the producers…');
  view.append(status);

  fetchJson(`${app.base}${rootPath.replace(/^\//, '')}/api/producers`)
      .then((doc) => build(buildIndex(doc)))
      .catch((e) => {
        status.textContent = `The producers could not be loaded: ${e.message}`;
      });

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
    const candidates = () => {
      const wanted = dataParams();
      const sel = selection();
      return Array.from(index.producers.values())
          .filter((p) => wanted.every((w) => p.parameterSet.has(w)) && covers(p, sel));
    };
    // A link without a source picks the first source that has the producer
    const producer = () => index.producers.get(producerKey(s.source, s.producer)) ||
        (s.source ? undefined :
                    Array.from(index.producers.values()).find((p) => p.name === s.producer));

    // --- Model details: runs, geometries, levels and ensemble members ---------------
    // The details of grid producers are loaded when one is chosen
    const details = new Map();  // producer key @ run -> details, null while loading
    const detailsKey = (p) => `${p.key}@${s.run || ''}`;
    const gridDetails = () => {
      const p = producer();
      return p && p.source === 'grid' ? details.get(detailsKey(p)) || null : null;
    };
    function loadDetails()
    {
      const p = producer();
      if (!p || p.source !== 'grid' || details.has(detailsKey(p)))
        return;
      const key = detailsKey(p);
      details.set(key, null);
      const query = queryString([['source', 'grid'], ['origintime', s.run || '']]);
      fetchJson(`${app.base}${rootPath.replace(/^\//, '')}/api/producers/${
                    encodeURIComponent(p.name)}?${query}`)
          .then((doc) => {
            details.set(key, doc);
            refresh(false);
          })
          .catch((e) => {
            details.set(key, {error: e.message});
            refresh(false);
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
    const chosenLevelType = () => {
      const types = levelTypes();
      return types.find((x) => x.id === s.leveltype) || types[0] || null;
    };
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
    const chosenForecast = () => {
      if (!hasEnsemble())
        return null;
      const types = forecastTypes();
      return types.find((f) => String(f.type) === s.forecast) ||
          types.reduce((a, b) => (b.members.length > a.members.length ? b : a));
    };
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
    const chosenSensors = () =>
        (isObservation() && s.sensors ? parseNumberList(s.sensors, SENSOR_NUMBERS) : []);
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
                          label: own_forecast.members.length > 1 ? `member ${m}` : ''
                        }));
        for (const l of lv)
          for (const m of mv)
            out.push({
              key: [info.name, d.producer, s.geometry || '', l.levelId, l.key, m.type, m.number]
                       .join(':')
                       .replace(/:+$/, ''),
              name,
              variant: [levels.length > 1 ? l.label : '', m.label].filter(Boolean).join(' · '),
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
        for (const b of bases)
        {
          const column = tf ? tf.expression(b.key) : b.key;
          const suffix = b.suffix || '';
          if (serverStatistics())
            for (const [, , fn] of AREA_STATISTICS)
              out.push({
                column: statisticParameter(fn, column),
                response: statisticParameter(fn, column) + suffix,
                name: statisticParameter(fn, name),
                variant: b.variant
              });
          else
            out.push({column, response: column + suffix, name, variant: b.variant});
        }
      }
      return out;
    }

    // The quality codes of observations, shown in the table
    function qualityColumns()
    {
      if (!isObservation() || s.qc !== '1' || serverStatistics())
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

    function variantColumns()
    {
      const columns = new Map();
      const requested = (requestParams().find(([k]) => k === 'param') || [])[1] || '';
      const details = gridDetailColumns(String(requested).split(','), dataColumns());
      for (const c of [...dataColumns(), ...qualityColumns(), ...details])
        if (c.response !== c.name || c.variant)
          columns.set(c.response, {name: c.name, variant: c.variant});
      return columns;
    }

    const paramSearch = autocomplete({
      label: 'Parameter',
      placeholder: 'Type a parameter name or description, e.g. temp',
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
                `${entry.producers.length} producer${entry.producers.length === 1 ? '' : 's'}`
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
      empty: (text) => producer() ? `${s.producer} has no parameter matching "${text}"` :
                                    `No parameter matches "${text}"`
    });

    const modelSearch = autocomplete({
      label: 'Producer',
      placeholder: 'Type a model or producer name',
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
      empty: () => selection() ? 'No producer has all the chosen parameters in the selected area' :
                                 'No producer has all the chosen parameters',
      limit: 200
    });

    function timeRange(p)
    {
      const t = p.time || {};
      if (!t.start || !t.end)
        return '';
      return `${formatTime(new Date(t.start))} – ${formatTime(new Date(t.end))}`;
    }

    function setProducer(key)
    {
      const p = index.producers.get(key);
      s.producer = p ? p.name : '';
      s.source = p ? p.source : '';
      resetDetails();
      // Parameters the producer does not have are dropped
      if (p)
        s.params = s.params.filter((x) => META.has(x) || p.parameterSet.has(x));
      defaultTimes(true);
      refresh();
      fitMap();
    }

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
    const runSelect = el('select', {'aria-label': 'Model run'});
    const geometrySelect = el('select', {'aria-label': 'Geometry'});
    const levelTypeSelect = el('select', {'aria-label': 'Level type'});
    const levelButtons = el('div', {class: 'level-buttons', role: 'group', 'aria-label': 'Levels'});
    const levelNote = el('p', {class: 'muted small'});
    const forecastSelect = el('select', {'aria-label': 'Forecast'});
    const membersInput = el(
        'input',
        {type: 'text', 'aria-label': 'Members', spellcheck: 'false', placeholder: 'e.g. 1-10, 15'});
    const membersNote = el('p', {class: 'muted small'});
    const detailsNote = el('p', {class: 'muted small'});
    // Observations: the sensors and the quality of the values
    const sensorsInput = el('input', {
      type: 'text',
      'aria-label': 'Sensors',
      spellcheck: 'false',
      placeholder: 'the default sensor, or e.g. 1, 11'
    });
    const sensorsNote = el('p', {class: 'muted small'});
    const qualitySelect = el('select', {'aria-label': 'Quality'}, [
      ['', 'The default filter of the server'],
      ['all', 'All values, whatever their quality'],
      ['custom', 'A filter of the quality codes']
    ].map(([v, label]) => el('option', {value: v}, label)));
    const qualityInput = el('input', {
      type: 'text',
      'aria-label': 'Quality filter',
      spellcheck: 'false',
      placeholder: 'e.g. le 1, or in 0,1'
    });
    const qualityCodes = el('input', {type: 'checkbox', 'aria-label': 'Quality codes'});
    const sensorsRow = el('div', null, el('label', null, 'Sensors', sensorsInput), sensorsNote);
    const qualityRow =
        el('div',
           {class: 'row'},
           el('label', null, 'Quality', qualitySelect),
           el('label', null, 'Filter', qualityInput));
    const qualityCodesRow =
        el('label', {class: 'check'}, qualityCodes, 'Show the quality codes in the table');
    const runRow = el('label', null, 'Model run', runSelect);
    const geometryRow = el('label', null, 'Geometry', geometrySelect);
    const levelTypeRow = el('label', null, 'Level type', levelTypeSelect);
    const levelsRow = el('div', null, el('label', null, 'Levels'), levelButtons, levelNote);
    const forecastRow = el('label', null, 'Forecast', forecastSelect);
    const membersRow = el('div', null, el('label', null, 'Members', membersInput), membersNote);
    const detailsBox =
        el('div',
           {class: 'model-details'},
           runRow,
           geometryRow,
           levelTypeRow,
           levelsRow,
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
              ['', 'The latest'],
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
              ['', 'The default'],
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
        levelNote.textContent = chosen.size === 0 ?
            `No level chosen: the values of all ${type.values.length} ${type.type} levels` :
            `${chosen.size} of ${type.values.length} ${type.type} levels${
                type.unit ? ` (${type.unit})` : ''}, a series each`;
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
        membersInput.placeholder = `all ${forecast.members.length} members, or e.g. ${
            forecast.members[0]}-${forecast.members[Math.min(9, forecast.members.length - 1)]}`;
        const list = s.members ? parseNumberList(s.members, forecast.members) : forecast.members;
        membersNote.textContent = list === null ?
            'Not a list of members: numbers and ranges separated by commas' :
            `${list.length} member${list.length === 1 ? '' : 's'}, a series each`;
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
            'Not a list of sensors: numbers and ranges separated by commas' :
            sensors.length === 0 ?
            'The default sensor of each station' :
            `${sensors.length} sensor${sensors.length === 1 ? '' : 's'}, a series each; ` +
                'a sensor without values gives no rows';
        qualitySelect.value = s.quality || '';
        qualityInput.closest('label').hidden = s.quality !== 'custom';
        if (document.activeElement !== qualityInput)
          qualityInput.value = s.qfilter || '';
        qualityCodes.checked = s.qc === '1';
      }
      // What the data of the parameters is, and too large requests
      const notes = [];
      if (p && p.source === 'grid' && d === null)
        notes.push('Loading the model details…');
      if (d && d.error)
        notes.push(`The model details could not be loaded: ${d.error}`);
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
        notes.push(`Only the first ${MAX_KEYS} combinations of levels and members are requested`);
      detailsNote.textContent = notes.join(' · ');
      detailsNote.hidden = notes.length === 0;
      detailsBox.hidden = [
        runRow,
        geometryRow,
        levelTypeRow,
        levelsRow,
        forecastRow,
        membersRow,
        sensorsRow,
        detailsNote
      ].every((x) => x.hidden);
    }

    // --- When ------------------------------------------------------
    const startInput = el('input', {type: 'datetime-local', 'aria-label': 'Start time (UTC)'});
    const endInput = el('input', {type: 'datetime-local', 'aria-label': 'End time (UTC)'});
    const stepSelect = el('select', {'aria-label': 'Time step'}, [
      ['data', 'Times of the data'],
      ['10', '10 minutes'],
      ['30', '30 minutes'],
      ['60', '1 hour'],
      ['180', '3 hours'],
      ['360', '6 hours'],
      ['720', '12 hours'],
      ['1440', '1 day']
    ].map(([v, t]) => el('option', {value: v}, t)));
    const dataPeriod = el('span', {class: 'muted small'});

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

    const preset = (label, fn) =>
        el('button',
           {
             type: 'button',
             class: 'small',
             onclick: () => {
               fn();
               refresh();
             }
           },
           label);
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
      ['60', '1 hour'],
      ['180', '3 hours'],
      ['360', '6 hours'],
      ['720', '12 hours'],
      ['1440', '24 hours']
    ];
    const aggModes =
        el('div', {class: 'segmented', role: 'radiogroup', 'aria-label': 'Values over time'});
    for (const [value, label] of [['', 'As in the data'], ['period', 'Over a period']])
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
           {'aria-label': 'Period'},
           PERIODS.map(([v, label]) => el('option', {value: v}, label)));
    const windowSelect = el('select', {'aria-label': 'Period placement'}, [
      ['end', 'ending at each time'],
      ['centre', 'centred on each time'],
      ['start', 'starting at each time']
    ].map(([v, label]) => el('option', {value: v}, label)));
    const functionSelect = el('select', {'aria-label': 'Function'}, [
      ['auto', 'Recommended for each parameter'],
      ...TIME_FUNCTIONS.map(([v, label]) => [v, label])
    ].map(([v, label]) => el('option', {value: v}, label)));
    const percentileInput = el(
        'input', {type: 'number', min: 0, max: 100, step: 1, size: 4, 'aria-label': 'Percentile'});
    const percentileLabel = el('label', null, 'Percentile', percentileInput);
    const aggSummary = el('ul', {class: 'agg-summary small'});
    const aggDetails = el(
        'div',
        {class: 'agg-details'},
        el('div',
           {class: 'row'},
           el('label', null, 'Period', periodSelect),
           el('label', null, 'Placement', windowSelect)),
        el('div', {class: 'row'}, el('label', null, 'Function', functionSelect), percentileLabel),
        aggSummary);
    const aggRow = el('div', null, el('label', null, 'Values over time'), aggModes, aggDetails);

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
      const what = choice === 'percentile' ? `${pct} % percentile` : def[1].toLowerCase();
      const placement = window === 'end' ? 'ending at' :
          window === 'start'             ? 'starting at' :
                                           'centred on';
      const warnings = [];
      const warning = functionWarning(choice, kind);
      if (warning)
        warnings.push(`${name} ${warning}`);
      if (step > 0 && step >= period)
        warnings.push(
            `the data has a value every ${step} min: a period of ${period} min has only one`);
      return {
        expression: (base) => `${prefix}(${base}/${before}m/${after}m)`,
        text: `${name}: ${what} of the ${label} ${placement} each time${
            kind === 'state' ? '' : ` (${kind})`}`,
        warnings
      };
    }

    // --- Where -----------------------------------------------------
    s.locmode = s.locmode || 'place';
    const locInput = el('input', {type: 'text', 'aria-label': 'Location', spellcheck: 'false'});
    const locModes =
        el('div', {class: 'segmented', role: 'radiogroup', 'aria-label': 'Location type'});
    const showButton = el('button', {type: 'button', class: 'small'}, 'Show locations');
    const areaNote = el('p', {class: 'warn small'});
    const map = createMap(mapSettings(app.element), (shape) => {
      s.location = formatSelection(shape);
      refresh();
    });
    for (const [mode, label] of LOCATION_MODES)
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
                 // The locations already on the map stay as a reference
                 s.locmode = mode;
                 s.location = '';
                 // Each location type has its own default for the values of an area
                 s.areavalues = '';
                 refresh();
               }
             },
             label));
    }
    locInput.addEventListener('input', () => {
      s.location = locInput.value.trim();
      refreshSoon();
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
            areaNote.textContent = points.length === 0 ? 'No locations found.' : '';
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

    function fitMap()
    {
      const extent = map.extent();
      if (extent)
        map.fit(extent);
    }

    // --- Output ----------------------------------------------------
    // Areas: the statistics computed by the server, or every point
    // The default is the statistics for areas, every point for keywords and station lists
    const areaValuesOf = () =>
        s.areavalues || (['keyword', 'fmisid'].includes(s.locmode) ? 'points' : 'statistics');
    const areaValues =
        el('div', {class: 'segmented', role: 'radiogroup', 'aria-label': 'Area values'});
    for (const [value, label] of [['statistics', 'Statistics'], ['points', 'Every point']])
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
    const areaValuesRow = el('div', null, el('label', null, 'Values of an area'), areaValues);

    // An area, radius or box, or several locations, whose values the server can aggregate
    const isArea = () => ['bbox', 'radius', 'area'].includes(s.locmode) ||
        (s.locmode === 'place' && /:\s*\d/.test(s.location || '')) ||
        (s.locmode === 'keyword' && !!s.location) ||
        (s.locmode === 'fmisid' && (s.location || '').includes(','));
    // Keywords and station lists are grouped into one area for the statistics
    const groupLocations = () => serverStatistics() && ['keyword', 'fmisid'].includes(s.locmode);
    const serverStatistics = () => isArea() && areaValuesOf() === 'statistics';

    // Extra columns: the meta parameters by group, and the details of the grid data
    const extraGroups = el('div', {class: 'extra-groups'});
    const extraDetails =
        el('details', {class: 'extra-columns'}, el('summary', null, 'Extra columns'), extraGroups);
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
               el('div', {class: 'muted small'}, 'Details of the grid data of each parameter'),
               el('div', {class: 'level-buttons'}, buttons)));
      }
      extraGroups.replaceChildren(...groups);
    }

    const langInput =
        el('input', {type: 'text', 'aria-label': 'Language', placeholder: 'fi, sv, en', size: 6});
    langInput.addEventListener('input', () => {
      s.lang = langInput.value.trim();
      refreshSoon();
    });

    // --- Request and results ----------------------------------------
    const urlBox = el('code', {class: 'url'});
    const missing = el('p', {class: 'missing'});
    const runButton = el('button', {type: 'button', class: 'primary'}, 'Run');
    const copyUrl = el('button', {type: 'button', class: 'small'}, 'Copy URL');
    const code = el('div', {class: 'code-box'});
    const codeDetails =
        el('details', {class: 'code-details'}, el('summary', null, 'Code for this request'), code);
    const results = el('div', {class: 'results'});

    copyUrl.addEventListener(
        'click', () => navigator.clipboard.writeText(currentUrl()).catch(() => {}));
    runButton.addEventListener('click', run);

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
      const params = serverStatistics() ?
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
        [locationOption(s.locmode), s.location],
        ['grouplocations', groupLocations() ? '1' : ''],
        ['lang', s.lang],
        ['format', 'json'],
        ['tz', 'UTC'],
        ['timeformat', 'xml']
      ];
    }

    // The server reads querydata unless told otherwise, or unless it is configured to prefer grid
    function sourceOption()
    {
      const p = producer();
      return p && (p.source === 'grid' || (p.ambiguous && p.source === 'querydata')) ? p.source :
                                                                                       '';
    }

    function currentUrl()
    {
      return new URL(`${rootPath}?${queryString(requestParams())}`, app.base).href;
    }

    function missingParts()
    {
      const out = [];
      if (!s.producer)
        out.push('a producer');
      if (dataParams().length === 0)
        out.push('a parameter');
      if (!s.location)
        out.push('a location');
      return out;
    }

    async function run()
    {
      results.replaceChildren(el('p', {class: 'muted'}, 'Running…'));
      const url = currentUrl();
      let text;
      try
      {
        const response = await fetch(url, {headers: {accept: 'application/json'}});
        text = await response.text();
        if (!response.ok)
          throw new Error(`${response.status} ${response.statusText}: ${text.slice(0, 500)}`);
      }
      catch (e)
      {
        results.replaceChildren(el('p', {class: 'error'}, e.message));
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
      if (!serverStatistics())
        map.setPoints(mapPoints(rows));
      showResults(rows, text);
    }

    function showResults(rows, text)
    {
      const statistics = serverStatistics();
      const panes = {Chart: el('div', {class: 'pane'})};
      // The values on a map when there are several located points
      if (hasMapValues(rows))
        panes.Map = el('div', {class: 'pane'});
      panes.Table = el('div', {class: 'pane'});
      panes.Tree = el('div', {class: 'pane tree-pane'});
      const tabs = el('div', {class: 'tabs', role: 'tablist'});
      const drawn = new Set();
      const show = (name) => {
        for (const [n, pane] of Object.entries(panes))
          pane.hidden = n !== name;
        Array.from(tabs.children)
            .forEach(
                (b) => b.setAttribute('aria-selected', b.textContent === name ? 'true' : 'false'));
        if (drawn.has(name))
          return;
        drawn.add(name);
        if (name === 'Chart')
          drawCharts(panes.Chart, rows, dataParams(), statistics ? 'server' : undefined);
        else if (name === 'Map')
          drawValueMap(panes.Map, rows, dataParams(), mapSettings(app.element));
        else if (name === 'Table')
          drawTable(panes.Table, rows);
        else if (!S.renderTree(panes.Tree, text, 'json'))
          panes.Tree.textContent = text;
      };
      for (const name of Object.keys(panes))
        tabs.append(el('button', {type: 'button', role: 'tab', onclick: () => show(name)}, name));
      const locationsCount = new Set(rows.map(locationOf)).size;
      results.replaceChildren(
          el('div',
             {class: 'results-head'},
             tabs,
             el('span',
                {class: 'muted small'},
                `${rows.length} rows${locationsCount > 1 ? `, ${locationsCount} locations` : ''}`)),
          ...Object.values(panes));
      show('Chart');
    }

    function refresh(rerender = true)
    {
      const p = producer();
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
                 'aria-label': `Remove ${label}`,
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
          `${n} of ${index.producers.size} producers have ${
              dataParams().length === 1 ? 'this parameter' : 'all these parameters'}${inArea}` :
          (inArea ? `${n} of ${index.producers.size} producers cover the selected area` :
                    `${index.producers.size} producers`);
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
                  'Change')),
            p.title && p.title !== p.name ? el('div', null, p.title) : '',
            el('div',
               {class: 'muted small'},
               [
                 timeRange(p),
                 p.time && p.time.step ? `step ${p.time.step} min` : '',
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
      // Times
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
      dataPeriod.textContent = p && timeRange(p) ? `Data: ${timeRange(p)}` : '';
      // Location
      Array.from(locModes.children)
          .forEach(
              (b) =>
                  b.setAttribute('aria-checked', b.dataset.mode === s.locmode ? 'true' : 'false'));
      const mode = LOCATION_MODES.find((m) => m[0] === s.locmode) || LOCATION_MODES[0];
      locInput.placeholder = mode[2];
      if (rerender !== false)
        locInput.value = s.location || '';
      map.setTools(mode[3] ? ['move', mode[3]] : ['move'], mode[3] || 'move');
      const sel = selection();
      map.setSelection(sel);
      map.setRegions(p && p.bbox ? [{bbox: p.bbox, label: `The area of ${p.name}`}] : []);
      showButton.hidden = mode[3] !== null;
      showButton.disabled = !s.location;
      areaNote.textContent = p && sel && !covers(p, sel) ?
          `The selection is outside the area of ${p.name}` :
          (s.location && mode[3] && !sel ? `Not a valid ${mode[1].toLowerCase()}: ${mode[2]}` : '');
      if (rerender !== false)
        langInput.value = s.lang || '';
      areaValuesRow.hidden = !isArea();
      Array.from(areaValues.children)
          .forEach(
              (b) => b.setAttribute(
                  'aria-checked', b.dataset.areavalues === areaValuesOf() ? 'true' : 'false'));
      // Request
      const url = currentUrl();
      urlBox.textContent = url;
      const absent = missingParts();
      missing.textContent = absent.length ? `Still needed: ${absent.join(', ')}` : '';
      runButton.disabled = absent.length > 0;
      codeTabs(code, url);
      save();
    }

    const section = (title, ...content) =>
        el('section', {class: 'step'}, el('h2', null, title), ...content);
    view.append(
        el('div',
           {class: 'form'},
           section(
               'What',
               el('label', null, 'Parameters'),
               chips,
               paramSearch.element,
               el('label', null, 'Producer'),
               candidatesNote,
               modelSearch.element,
               modelCard,
               detailsBox),
           section(
               'When',
               el('div',
                  {class: 'row'},
                  el('label', null, 'Start (UTC)', startInput),
                  el('label', null, 'End (UTC)', endInput)),
               el('div', {class: 'row'}, el('label', null, 'Step', stepSelect), dataPeriod),
               presets,
               aggRow),
           section(
               'Where',
               locModes,
               el('div', {class: 'row'}, locInput, showButton),
               areaNote,
               map.element),
           section(
               'Output',
               areaValuesRow,
               el('div', {class: 'row'}, el('label', null, 'Language of the names', langInput)),
               extraDetails)),
        el('div',
           {class: 'request'},
           el('div', {class: 'url-row'}, urlBox, copyUrl),
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
  ['position', 'Position', 'point'],
  ['radius', 'Radius', 'circle'],
  ['area', 'Area', 'box'],
  ['cube', 'Cube', 'box'],
  ['locations', 'Locations', null]
];

function edrModule(app, api, state, save)
{
  const rootPath = api.root;
  const view = el('div', {class: 'module'});
  const status = el('p', {class: 'muted'}, 'Loading the collections…');
  view.append(status);

  fetchJson(`${app.base}${rootPath.replace(/^\//, '')}/collections?f=json`)
      .then((doc) => build(buildEdrIndex(doc)))
      .catch((e) => {
        status.textContent = `The collections could not be loaded: ${e.message}`;
      });

  function build(index)
  {
    view.replaceChildren();
    const s = state;
    // Typing stores the value at once and redraws the request a moment later
    const refreshSoon = debounce(() => refresh(false), 300);
    s.params = s.params || [];

    const collection = () => index.producers.get(s.collection);
    const queries = () => {
      const c = collection();
      return EDR_QUERIES.filter(([q]) => !c || c.queries.includes(q));
    };
    const query = () => {
      const available = queries();
      return available.find(([q]) => q === s.query) || available[0];
    };
    const selection = () => {
      const [q] = query();
      if (q === 'position')
        return parseSelection('lonlat', s.location);
      if (q === 'radius')
        return parseSelection('radius', s.location);
      if (q === 'area' || q === 'cube')
        return parseSelection('bbox', s.location);
      return null;
    };
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
      label: 'Parameter',
      placeholder: 'Type a parameter name or description',
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
                  `${entry.producers.length} collection${entry.producers.length === 1 ? '' : 's'}`
            });
        }
        out.sort((a, b) => a.score - b.score || a.label.localeCompare(b.label));
        return out;
      },
      select: (item) => {
        s.params.push(item.key);
        refresh();
      },
      empty: (text) => `No parameter matches "${text}"`
    });

    const collectionSearch = autocomplete({
      label: 'Collection',
      placeholder: 'Type a collection name',
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
      empty: () => 'No collection has all the chosen parameters',
      limit: 200
    });

    function timeRangeOf(c)
    {
      if (!c.time || !c.time.start || !c.time.end)
        return '';
      return `${formatTime(new Date(c.time.start))} – ${formatTime(new Date(c.time.end))}`;
    }

    // --- How: the query type -------------------------------------------------
    const queryButtons = el('div', {class: 'segmented', role: 'radiogroup', 'aria-label': 'Query'});

    // --- When and the levels -------------------------------------------------
    const startInput = el('input', {type: 'datetime-local', 'aria-label': 'Start time (UTC)'});
    const endInput = el('input', {type: 'datetime-local', 'aria-label': 'End time (UTC)'});
    const dataPeriod = el('span', {class: 'muted small'});
    const levels = el('div', {class: 'chips'});
    const levelRow = el('div', null, el('label', null, 'Levels'), levels);
    startInput.addEventListener('change', () => {
      s.start = startInput.value;
      refresh();
    });
    endInput.addEventListener('change', () => {
      s.end = endInput.value;
      refresh();
    });

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
    const locInput = el('input', {type: 'text', 'aria-label': 'Location', spellcheck: 'false'});
    const areaNote = el('p', {class: 'warn small'});
    const map = createMap(mapSettings(app.element), (shape) => {
      s.location = formatSelection(shape);
      refresh();
    });
    locInput.addEventListener('input', () => {
      s.location = locInput.value.trim();
      refreshSoon();
    });
    let locationsOf = null;  // the locations of the collection for a locations query

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
            areaNote.textContent = `The locations could not be loaded: ${e.message}`;
          });
    }

    const locationSearch = autocomplete({
      label: 'Location id',
      placeholder: 'Type a location name or id',
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
      empty: () => 'No location matches',
      limit: 50
    });

    function fitMap()
    {
      const extent = map.extent();
      if (extent)
        map.fit(extent);
    }

    // --- Request ---------------------------------------------------------------
    const formatSelect = el('select', {'aria-label': 'Output format'});
    formatSelect.addEventListener('change', () => {
      s.format = formatSelect.value;
      refresh();
    });
    const urlBox = el('code', {class: 'url'});
    const missing = el('p', {class: 'missing'});
    const runButton = el('button', {type: 'button', class: 'primary'}, 'Run');
    const copyUrl = el('button', {type: 'button', class: 'small'}, 'Copy URL');
    const code = el('div', {class: 'code-box'});
    const codeDetails =
        el('details', {class: 'code-details'}, el('summary', null, 'Code for this request'), code);
    const results = el('div', {class: 'results'});
    copyUrl.addEventListener(
        'click', () => navigator.clipboard.writeText(currentUrl()).catch(() => {}));
    runButton.addEventListener('click', run);

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
      const [a, b, c, d] = sel.bbox;
      if (query()[0] === 'cube')
        return [['bbox', `${a},${b},${c},${d}`]];
      return [['coords', `POLYGON((${a} ${b},${c} ${b},${c} ${d},${a} ${d},${a} ${b}))`]];
    }

    function currentUrl()
    {
      const c = collection();
      const [q] = query();
      let path =
          `${rootPath}/collections/${encodeURIComponent(s.collection || '{collectionId}')}/${q}`;
      if (q === 'locations' && s.location)
        path += `/${encodeURIComponent(s.location)}`;
      const params = [
        ...coords(),
        ['parameter-name', s.params.join(',')],
        ['datetime', s.start && s.end ? `${fromInput(s.start)}/${fromInput(s.end)}` : ''],
        ['z', (s.levels || []).join(',')],
        ['f', s.format || (c && c.formats[0]) || 'CoverageJSON']
      ];
      return new URL(`${path}?${queryString(params)}`, app.base).href;
    }

    function missingParts()
    {
      const out = [];
      if (!s.collection)
        out.push('a collection');
      if (s.params.length === 0)
        out.push('a parameter');
      const [q] = query();
      if (q === 'locations' ? !s.location : !selection())
        out.push(
            q === 'locations' ? 'a location' :
                                `a ${
                                    q === 'position'   ? 'point' :
                                        q === 'radius' ? 'radius' :
                                                         'box'} on the map`);
      return out;
    }

    async function run()
    {
      results.replaceChildren(el('p', {class: 'muted'}, 'Running…'));
      let text;
      try
      {
        const response = await fetch(currentUrl());
        text = await response.text();
        if (!response.ok)
          throw new Error(`${response.status} ${response.statusText}: ${text.slice(0, 500)}`);
      }
      catch (e)
      {
        results.replaceChildren(el('p', {class: 'error'}, e.message));
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
      const rows = coverageRows(doc);
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
      const panes = {Chart: el('div', {class: 'pane'})};
      // The values on a map when there are several located points
      if (hasMapValues(rows))
        panes.Map = el('div', {class: 'pane'});
      panes.Table = el('div', {class: 'pane'});
      panes.Tree = el('div', {class: 'pane tree-pane'});
      const tabs = el('div', {class: 'tabs', role: 'tablist'});
      const drawn = new Set();
      const params = Array.from(new Set(rows.flatMap((r) => Object.keys(r))))
                         .filter(
                             (k) => !['name', 'time', 'lat', 'lon', 'level'].includes(k) &&
                                 !k.startsWith('_'));
      const show = (name) => {
        for (const [n, pane] of Object.entries(panes))
          pane.hidden = n !== name;
        Array.from(tabs.children)
            .forEach(
                (b) => b.setAttribute('aria-selected', b.textContent === name ? 'true' : 'false'));
        if (drawn.has(name))
          return;
        drawn.add(name);
        if (name === 'Chart')
          drawCharts(panes.Chart, rows, params);
        else if (name === 'Map')
          drawValueMap(panes.Map, rows, params, mapSettings(app.element));
        else if (name === 'Table')
          drawTable(panes.Table, rows);
        else if (!S.renderTree(panes.Tree, text, 'json'))
          panes.Tree.textContent = text;
      };
      for (const name of Object.keys(panes))
        tabs.append(el('button', {type: 'button', role: 'tab', onclick: () => show(name)}, name));
      const count = new Set(rows.map(locationOf)).size;
      results.replaceChildren(
          el('div',
             {class: 'results-head'},
             tabs,
             el('span',
                {class: 'muted small'},
                `${rows.length} rows${count > 1 ? `, ${count} locations` : ''}`)),
          ...Object.values(panes));
      show('Chart');
    }

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
                 'aria-label': `Remove ${label}`,
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
          `${n} of ${index.producers.size} collections${
              s.params.length ?
                  ` have ${s.params.length === 1 ? 'this parameter' : 'all these parameters'}` :
                  ' cover the selected area'}${s.params.length ? inArea : ''}` :
          `${index.producers.size} collections`;
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
                  'Change')),
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
                     refresh();
                   }
                 },
                 label)));

      // Times and levels
      startInput.value = s.start || '';
      endInput.value = s.end || '';
      dataPeriod.textContent = c && timeRangeOf(c) ? `Data: ${timeRangeOf(c)}` : '';
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

      // Location
      const isLocations = q === 'locations';
      locInput.hidden = isLocations;
      locationSearch.element.hidden = !isLocations;
      locInput.placeholder = q === 'position' ? '24.94,60.17, or click the map' :
          q === 'radius'                      ? '24.94,60.17:25 (km), or drag on the map' :
                                                '21,60,26,62, or drag on the map';
      if (rerender !== false && !isLocations)
        locInput.value = s.location || '';
      map.setTools(tool ? ['move', tool] : ['move'], tool || 'move');
      const sel = selection();
      map.setSelection(sel);
      map.setRegions(c && c.bbox ? [{bbox: c.bbox, label: `The area of ${c.name}`}] : []);
      if (isLocations)
      {
        loadLocations();
        if (locationsOf && locationsOf.list)
          map.setPoints(locationsOf.list);
      }
      areaNote.textContent = c && sel && !covers(c, sel) ?
          `The selection is outside the area of ${c.name}` :
          (isLocations && s.location ? `Location ${s.location}` : '');

      // Formats
      const formats = c ? c.formats : ['CoverageJSON', 'GeoJSON'];
      formatSelect.replaceChildren(...formats.map((f) => el('option', {value: f}, f)));
      if (!formats.includes(s.format))
        s.format = formats[0];
      formatSelect.value = s.format || '';

      const url = currentUrl();
      urlBox.textContent = url;
      const absent = missingParts();
      missing.textContent = absent.length ? `Still needed: ${absent.join(', ')}` : '';
      runButton.disabled = absent.length > 0;
      codeTabs(code, url);
      save();
    }

    const section = (title, ...content) =>
        el('section', {class: 'step'}, el('h2', null, title), ...content);
    view.append(
        el('div',
           {class: 'form'},
           section(
               'What',
               el('label', null, 'Parameters'),
               chips,
               paramSearch.element,
               el('label', null, 'Collection'),
               note,
               collectionSearch.element,
               card),
           section(
               'When',
               el('div',
                  {class: 'row'},
                  el('label', null, 'Start (UTC)', startInput),
                  el('label', null, 'End (UTC)', endInput)),
               dataPeriod,
               levelRow),
           section(
               'Where',
               queryButtons,
               el('div', {class: 'row'}, locInput),
               locationSearch.element,
               areaNote,
               map.element),
           section('Output', el('div', {class: 'row'}, el('label', null, 'Format', formatSelect)))),
        el('div',
           {class: 'request'},
           el('div', {class: 'url-row'}, urlBox, copyUrl),
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

// The f values of the output media types of the Coverages API
const COVERAGE_FORMATS = {
  'application/x-grib2': ['grib2', 'GRIB 2'],
  'application/x-grib': ['grib1', 'GRIB 1'],
  'application/netcdf': ['netcdf', 'NetCDF'],
  'image/tiff': ['geotiff', 'GeoTIFF'],
  'application/x-fmi-querydata': ['qd', 'Querydata']
};

// The output projections offered, besides the native grid and any other EPSG code
const COVERAGE_CRS = [
  ['', 'Native grid of the model'],
  ['EPSG:4326', 'Longitude and latitude (EPSG:4326)'],
  ['EPSG:3067', 'ETRS-TM35FIN (EPSG:3067)'],
  ['EPSG:3035', 'ETRS89 LAEA Europe (EPSG:3035)'],
  ['EPSG:3857', 'Web Mercator (EPSG:3857)']
];

// The name of the projection of a WKT
function projectionName(wkt)
{
  const m = /^\s*(?:PROJCS|PROJCRS|GEOGCS|GEOGCRS)\s*\[\s*"([^"]*)"/.exec(wkt || '');
  return m ? m[1] : '';
}

function formatBytes(n)
{
  if (!(n > 0))
    return '0 bytes';
  const units = ['bytes', 'kB', 'MB', 'GB', 'TB'];
  const i = Math.min(units.length - 1, Math.floor(Math.log10(n) / 3));
  return `${(n / 10 ** (3 * i)).toFixed(i === 0 ? 0 : 1)} ${units[i]}`;
}

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

function coveragesModule(app, api, state, save)
{
  const rootPath = api.root;
  const base = `${app.base}${rootPath.replace(/^\//, '')}`;
  const view = el('div', {class: 'module'});
  const status = el('p', {class: 'muted'}, 'Loading the collections…');
  view.append(status);

  fetchJson(`${base}/collections`).then((doc) => build(buildCoverageIndex(doc))).catch((e) => {
    status.textContent = `The collections could not be loaded: ${e.message}`;
  });

  function build(index)
  {
    view.replaceChildren();
    const s = state;
    // Typing stores the value at once and redraws the request a moment later
    const refreshSoon = debounce(() => refresh(false), 300);
    s.params = s.params || [];

    // The parameters come from the schemas of the collections, four at a time
    const queue = Array.from(index.producers.values());
    let loading = queue.length;
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
    const has = (c, key) => {
      const entry = index.parameters.get(key);
      return !!(entry && entry.names.has(c.name));
    };
    const selection = () => parseSelection('bbox', s.location);
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
      label: 'Parameter',
      placeholder: 'Type a parameter name or description',
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
                  `${entry.producers.length} collection${entry.producers.length === 1 ? '' : 's'}`
            });
        }
        out.sort((a, b) => a.score - b.score || a.label.localeCompare(b.label));
        return out;
      },
      select: (item) => {
        s.params.push(item.key);
        refresh();
      },
      empty: (text) => `No parameter matches "${text}"`
    });

    const collectionSearch = autocomplete({
      label: 'Collection',
      placeholder: 'Type a collection name',
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
      empty: () => 'No collection has all the chosen parameters',
      limit: 200
    });

    function timeRangeOf(c)
    {
      if (!c.time || !c.time.start || !c.time.end)
        return '';
      return `${formatTime(new Date(c.time.start))} – ${formatTime(new Date(c.time.end))}`;
    }

    // --- When and the levels -------------------------------------------------
    const startInput = el('input', {type: 'datetime-local', 'aria-label': 'Start time (UTC)'});
    const endInput = el('input', {type: 'datetime-local', 'aria-label': 'End time (UTC)'});
    const dataPeriod = el('span', {class: 'muted small'});
    const levels = el('div', {class: 'chips'});
    const levelRow = el('div', null, el('label', null, 'Levels (all if none is chosen)'), levels);
    startInput.addEventListener('change', () => {
      s.start = startInput.value;
      refresh();
    });
    endInput.addEventListener('change', () => {
      s.end = endInput.value;
      refresh();
    });

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
      'aria-label': 'Box',
      spellcheck: 'false',
      placeholder: 'The whole area, or minlon,minlat,maxlon,maxlat: drag on the map'
    });
    const crsSelect =
        el('select',
           {'aria-label': 'Output projection'},
           COVERAGE_CRS.map(([v, label]) => el('option', {value: v}, label)),
           el('option', {value: 'other'}, 'Other EPSG code…'));
    const crsInput =
        el('input', {type: 'text', 'aria-label': 'EPSG code', placeholder: 'EPSG:3006', size: 12});
    const widthInput =
        el('input',
           {type: 'number', min: 1, 'aria-label': 'Grid width', placeholder: 'width', size: 6});
    const heightInput =
        el('input',
           {type: 'number', min: 1, 'aria-label': 'Grid height', placeholder: 'height', size: 6});
    const areaNote = el('p', {class: 'warn small'});
    const estimate = el('p', {class: 'size-estimate small'});
    const map = createMap(mapSettings(app.element), (shape) => {
      s.location = formatSelection(shape);
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
                `${p.grid.width} × ${p.grid.height}${p.grid.estimated ? ' (estimated)' : ''} grid` :
                'grid of an unknown size';
            estimate.textContent =
                `${grid}, ${p.parameters} parameter${p.parameters === 1 ? '' : 's'}, ${
                    p.times} time${p.times === 1 ? '' : 's'}${
                    p.levels > 1 ? `, ${p.levels} levels` : ''}` +
                (p.values ? `: ${Math.round(p.values).toLocaleString('en')} values, about ${
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

    function drawRegions()
    {
      const regions = [];
      if (nativeOutline)
        regions.push({polygon: nativeOutline, label: 'The native grid'});
      if (outline && (s.location || s.crs))
        regions.push({polygon: outline, label: 'The area of the download', cls: 'xmap-outline'});
      map.setRegions(regions);
    }

    // --- Output and the request ----------------------------------------------
    const formatSelect = el('select', {'aria-label': 'Output format'});
    formatSelect.addEventListener('change', () => {
      s.format = formatSelect.value;
      refresh();
    });
    const urlBox = el('code', {class: 'url'});
    const missing = el('p', {class: 'missing'});
    const download =
        el('a', {class: 'download', download: '', 'aria-disabled': 'true'}, 'Download');
    const copyUrl = el('button', {type: 'button', class: 'small'}, 'Copy URL');
    const code = el('div', {class: 'code-box'});
    const codeDetails =
        el('details',
           {class: 'code-details', open: true},
           el('summary', null, 'Code for this request'),
           code);
    copyUrl.addEventListener(
        'click', () => navigator.clipboard.writeText(currentUrl()).catch(() => {}));

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

    function currentUrl()
    {
      const id = encodeURIComponent(s.collection || '{collectionId}');
      return new URL(
                 `${rootPath}/collections/${id}/coverage?${queryString(options(true))}`, app.base)
          .href;
    }

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
                 'aria-label': `Remove ${label}`,
                 onclick: () => {
                   s.params = s.params.filter((x) => x !== key);
                   refresh();
                 }
               },
               '×'));
      }));
      const n = candidates().length;
      const pending = loading > 0 ? ` (reading the parameters of ${loading} more)` : '';
      note.textContent =
          (s.params.length > 0 ?
               `${n} of ${index.producers.size} collections have ${
                   s.params.length === 1 ?
                       'this parameter' :
                       'all these parameters'}${selection() ? ' in the selected area' : ''}` :
               `${index.producers.size} collections`) +
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
                  'Change')),
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

      startInput.value = s.start || '';
      endInput.value = s.end || '';
      dataPeriod.textContent = c && timeRangeOf(c) ? `Data: ${timeRangeOf(c)}` : '';
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
      const known = COVERAGE_CRS.some(([v]) => v === (s.crs || ''));
      crsSelect.value = known ? (s.crs || '') : 'other';
      crsInput.hidden = known;
      if (!known && rerender !== false)
        crsInput.value = s.crs === 'other' ? '' : s.crs;
      map.setTools(['move', 'box'], 'box');
      const sel = selection();
      map.setSelection(sel);
      areaNote.textContent = c && sel && !covers(c, sel) ?
          `The box is outside the area of ${c.name}` :
          (s.location && !sel ? 'Not a valid box: minlon,minlat,maxlon,maxlat' : '');
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
        absent.push('a collection');
      if (s.params.length === 0)
        absent.push('a parameter');
      missing.textContent = absent.length ? `Still needed: ${absent.join(', ')}` : '';
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
      if (c)
        updateOutline();
      else
        estimate.textContent = '';
      save();
    }

    const section = (title, ...content) =>
        el('section', {class: 'step'}, el('h2', null, title), ...content);
    view.append(
        el('div',
           {class: 'form'},
           section(
               'What',
               el('label', null, 'Parameters'),
               chips,
               paramSearch.element,
               el('label', null, 'Collection'),
               note,
               collectionSearch.element,
               card),
           section(
               'When',
               el('div',
                  {class: 'row'},
                  el('label', null, 'Start (UTC)', startInput),
                  el('label', null, 'End (UTC)', endInput)),
               dataPeriod,
               levelRow),
           section('Where', el('div', {class: 'row'}, locInput), areaNote, map.element),
           section(
               'Output',
               el('div', {class: 'row'}, el('label', null, 'Projection', crsSelect), crsInput),
               el('div',
                  {class: 'row'},
                  el('label',
                     null,
                     'Grid size (optional)',
                     el('span', {class: 'row'}, widthInput, '×', heightInput))),
               el('div', {class: 'row'}, el('label', null, 'Format', formatSelect)))),
        el('div',
           {class: 'request'},
           el('div', {class: 'url-row'}, urlBox, copyUrl),
           missing,
           estimate,
           el('div', {class: 'row'}, download),
           codeDetails));
    defaultTimes(false);
    refresh();
    loadNative();
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

function readState()
{
  const p = new URLSearchParams(location.hash.slice(1));
  const s = {};
  for (const [k, v] of p)
    s[k] = v;
  if (s.params)
    s.params = s.params.split(',').filter(Boolean);
  return s;
}

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
}

async function main()
{
  const element = document.getElementById('explorer');
  if (!element)
    return;
  const title = element.dataset.title || 'SmartMet Server';
  const base = new URL(element.dataset.catalog || '../api', location.href);
  // The base URL of the server: the catalog is at <base>api
  const app = {element: element, base: new URL('./', base).href};

  const header =
      el('header', null, el('h1', null, 'Data explorer'), el('span', {class: 'muted'}, title));
  const apiSelect = el('select', {'aria-label': 'API'});
  const content = el('main');
  element.replaceChildren(
      header,
      el('nav',
         null,
         el('label', null, 'API ', apiSelect),
         el('a', {href: new URL('api/docs', app.base).href}, 'API documentation'),
         el('a', {href: base.href + '?f=html'}, 'API catalog')),
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
        el('p', {class: 'error'}, `The API catalog could not be loaded: ${e.message}`));
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
        el('p', {class: 'muted'}, 'None of the APIs of this server can be explored yet.'));
    return;
  }

  const state = readState();
  apis.forEach(
      (a, i) => apiSelect.append(el('option', {value: String(i)}, `${a.title} (${a.root})`)));
  let current = Math.max(0, apis.findIndex((a) => a.root === state.api));
  apiSelect.value = String(current);

  const open = (i, keep) => {
    current = i;
    const api = apis[i];
    const s = keep ? state : {};
    s.api = api.root;
    const save = () => writeState(s);
    content.replaceChildren(MODULES[api.kind](app, api, s, save));
  };
  apiSelect.addEventListener('change', () => open(Number(apiSelect.value), false));
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
  parameterKind,
  coverageRows,
  paletteColour,
  cellSize,
  projectionName,
  formatBytes,
  queryString
};

if (typeof document !== 'undefined')
{
  if (document.readyState === 'loading')
    document.addEventListener('DOMContentLoaded', main);
  else
    main();
}
})(typeof window !== 'undefined' ? window : globalThis);
