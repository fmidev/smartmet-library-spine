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

// Query strings keep commas, colons and parentheses readable, everything else is percent encoded
function encode(value)
{
  return encodeURIComponent(value)
      .replace(/%2C/gi, ',')
      .replace(/%3A/gi, ':')
      .replace(/%28/gi, '(')
      .replace(/%29/gi, ')');
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
  const coords = row.lat !== undefined && row.lat !== null ? `${row.lat},${row.lon}` : '';
  if (row._point !== undefined)
    return `${row.name !== null && row.name !== undefined ? row.name + ' ' : ''}(${
        coords || row._point})`;
  if (row.name !== undefined && row.name !== null)
    return String(row.name);
  return coords;
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
const META_PARAMETERS = [
  ['name', 'Location name'],
  ['time', 'Time'],
  ['lat', 'Latitude'],
  ['lon', 'Longitude'],
  ['latlon', 'Latitude and longitude'],
  ['geoid', 'Location id'],
  ['fmisid', 'FMI station id'],
  ['wmo', 'WMO station id'],
  ['elevation', 'Elevation of the location'],
  ['stationname', 'Name of the observation station'],
  ['distance', 'Distance to the station'],
  ['origintime', 'Origin time of the model run'],
  ['level', 'Level'],
  ['producer', 'Producer of the value'],
  ['localtime', 'Local time']
];
const META = new Map(META_PARAMETERS);

function buildIndex(doc)
{
  const producers = new Map();
  for (const p of doc.producers || [])
  {
    producers.set(p.name, Object.assign({}, p, {parameterSet: new Set(p.parameters || [])}));
  }
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
      entry.producers.push(p.name);
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
  if (!d.mapTiles)
    return null;
  return {
    tiles: d.mapTiles,
    tileSize: Number(d.mapTileSize) || 256,
    attribution: d.mapAttribution || ''
  };
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
      const r = rect(region.bbox, 'xmap-region');
      const title = svg('title');
      title.textContent = region.label;
      r.append(title);
      layers.regions.append(r);
    }
    layers.selection.replaceChildren();
    drawShape(layers.selection, state.selection, 'xmap-selection');

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
    redraw();
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
      state.regions = regions;
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
    // The box of everything shown
    extent() {
      const boxes = [
        ...state.regions.map((r) => r.bbox),
        selectionBox(state.selection),
        ...state.points.map((p) => [p.lon, p.lat, p.lon, p.lat])
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
  const series =
      rows.map((row) => [parseTime(row.time), row]).filter(([t]) => t).sort((a, b) => a[0] - b[0]);
  if (series.length === 0)
  {
    container.append(el('p', {class: 'muted'}, 'No data.'));
    return;
  }
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
           `${param}: median and percentiles of the area, computed by the server`));
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
  if (container.children.length === 0)
    container.append(el('p', {class: 'muted'}, 'No numeric values to draw.'));
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
  const ret = [['bash', `curl --fail --output result.json ${shellQuote(url)}\n`]];
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
    const producer = () => index.producers.get(s.producer);

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
                   key: p.name,
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

    function setProducer(name)
    {
      s.producer = name;
      const p = producer();
      // Parameters the producer does not have are dropped
      if (p)
        s.params = s.params.filter((x) => META.has(x) || p.parameterSet.has(x));
      defaultTimes(true);
      refresh();
      fitMap();
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
    locInput.addEventListener('input', debounce(() => {
                                s.location = locInput.value.trim();
                                refresh(false);
                              }, 300));

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

    const langInput =
        el('input', {type: 'text', 'aria-label': 'Language', placeholder: 'fi, sv, en', size: 6});
    langInput.addEventListener('input', debounce(() => {
                                 s.lang = langInput.value.trim();
                                 refresh(false);
                               }, 300));

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
      const params = serverStatistics() ? Array.from(new Set([
        'name',
        'time',
        ...s.params.filter((p) => META.has(p) && p !== 'lat' && p !== 'lon'),
        ...dataParams().flatMap((p) => AREA_STATISTICS.map(([, , fn]) => statisticParameter(fn, p)))
      ])) :
                                          Array.from(
                                              new Set(['name', 'time', 'lat', 'lon', ...s.params]));
      return [
        ['producer', s.producer],
        ['param', params.join(',')],
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
      const rows = flattenRows(data);
      if (!serverStatistics())
        map.setPoints(mapPoints(rows));
      showResults(rows, text);
    }

    function showResults(rows, text)
    {
      const statistics = serverStatistics();
      const panes = {
        Chart: el('div', {class: 'pane'}),
        Table: el('div', {class: 'pane'}),
        Tree: el('div', {class: 'pane tree-pane'})
      };
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
                      refresh();
                    }
                  },
                  'Change')),
            p.title && p.title !== p.name ? el('div', null, p.title) : null,
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
      // Times
      startInput.value = s.start || '';
      endInput.value = s.end || '';
      stepSelect.value = s.timestep || 'data';
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
               modelCard),
           section(
               'When',
               el('div',
                  {class: 'row'},
                  el('label', null, 'Start (UTC)', startInput),
                  el('label', null, 'End (UTC)', endInput)),
               el('div', {class: 'row'}, el('label', null, 'Step', stepSelect), dataPeriod),
               presets),
           section(
               'Where',
               locModes,
               el('div', {class: 'row'}, locInput, showButton),
               areaNote,
               map.element),
           section(
               'Output',
               areaValuesRow,
               el('div', {class: 'row'}, el('label', null, 'Language of the names', langInput)))),
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
// The application
// ------------------------------------------------------------------

const MODULES = {
  timeseries: timeSeriesModule
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
