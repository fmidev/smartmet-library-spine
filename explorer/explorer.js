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

// Query strings keep commas and colons readable, everything else is percent encoded
function encode(value)
{
  return encodeURIComponent(value).replace(/%2C/gi, ',').replace(/%3A/gi, ':');
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
    return data.flatMap((item) => (Array.isArray(item) ? flattenRows(item) : [item]));
  if (data && typeof data === 'object')
    return [data];
  return [];
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
// Charts
// ------------------------------------------------------------------

// The most series drawn as lines; more locations are drawn as a distribution
const MAX_LINES = 20;

// The charts of the numeric parameters of the rows, one chart per parameter
function drawCharts(container, rows, parameters)
{
  container.replaceChildren();
  if (rows.length === 0)
  {
    container.append(el('p', {class: 'muted'}, 'No data.'));
    return;
  }

  // Locations by name, or by coordinates when there are no names
  const locationOf = (row) => row.name !== undefined ? String(row.name) :
      row.lat !== undefined                          ? `${row.lat},${row.lon}` :
                                                       '';
  const times = new Map();  // epoch seconds -> index
  const locations = new Map();
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
      locations.set(loc, new Map());
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
  const distribution = names.length > MAX_LINES;
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

    const panel = el('div', {class: 'chart', 'data-param': param});
    container.append(panel);
    let data;
    let series;
    let bands = [];
    let caption;

    if (!distribution)
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
    else
    {
      // The median and the 10-90 % and 25-75 % ranges over the locations
      const qs = [0.1, 0.25, 0.5, 0.75, 0.9];
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
      data = [xs, ...columns];
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
          legend: {live: true},
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
      if (!columns.includes(k))
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

const LOCATION_MODES = [
  ['place', 'Place', 'Helsinki or Helsinki:20 for a 20 km radius'],
  ['lonlat', 'Coordinates', '24.94,60.17'],
  ['keyword', 'Keyword', 'synop_fi'],
  ['fmisid', 'Station id', '100971,101004'],
  ['area', 'Area', 'Uusimaa']
];

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
    const candidates = () => {
      const wanted = dataParams();
      return Array.from(index.producers.values())
          .filter((p) => wanted.every((w) => p.parameterSet.has(w)));
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
      empty: () => 'No producer has all the chosen parameters',
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
    const locMap = el('div', {class: 'loc-map'});
    const mapButton = el('button', {type: 'button', class: 'small'}, 'Show on map');
    for (const [mode, label] of LOCATION_MODES)
    {
      locModes.append(
          el('button',
             {
               type: 'button',
               role: 'radio',
               'data-mode': mode,
               onclick: () => {
                 s.locmode = mode;
                 s.location = '';
                 refresh();
               }
             },
             label));
    }
    locInput.addEventListener('input', debounce(() => {
                                s.location = locInput.value.trim();
                                refresh(false);
                              }, 300));

    let removeMap = null;
    mapButton.addEventListener('click', () => {
      if (removeMap)
      {
        removeMap();
        removeMap = null;
        mapButton.textContent = 'Show on map';
        return;
      }
      mapButton.disabled = true;
      // The locations do not depend on the data, so no producer: its times could be anywhere
      const url = `${rootPath}?${queryString([
        [s.locmode, s.location],
        ['param', 'name,lat,lon,geoid,fmisid,wmo,elevation,region,country,tz'],
        ['timesteps', '1'],
        ['format', 'json'],
        ['lang', s.lang]
      ])}`;
      fetchJson(app.base + url.replace(/^\//, ''))
          .then((data) => {
            const locations = flattenRows(data).filter(
                (r) => typeof r.lat === 'number' && typeof r.lon === 'number');
            if (locations.length === 0)
            {
              locMap.replaceChildren(el('p', {class: 'muted'}, 'No locations found.'));
              return;
            }
            removeMap = S.drawMap(locMap, locations, mapSettings(app.element));
            mapButton.textContent = 'Hide map';
          })
          .catch((e) => locMap.replaceChildren(el('p', {class: 'error'}, e.message)))
          .finally(() => {
            mapButton.disabled = false;
          });
    });

    // --- Output ----------------------------------------------------
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
      const params = Array.from(new Set(['name', 'time', ...s.params]));
      return [
        ['producer', s.producer],
        ['param', params.join(',')],
        ['starttime', fromInput(s.start)],
        ['endtime', fromInput(s.end)],
        ['timestep', s.timestep],
        [s.locmode, s.location],
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
      showResults(rows, text);
    }

    function showResults(rows, text)
    {
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
          drawCharts(panes.Chart, rows, dataParams());
        else if (name === 'Table')
          drawTable(panes.Table, rows);
        else if (!S.renderTree(panes.Tree, text, 'json'))
          panes.Tree.textContent = text;
      };
      for (const name of Object.keys(panes))
        tabs.append(el('button', {type: 'button', role: 'tab', onclick: () => show(name)}, name));
      const locationsCount = new Set(rows.map((r) => r.name)).size;
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
      candidatesNote.textContent = dataParams().length > 0 ?
          `${n} of ${index.producers.size} producers have ${
              dataParams().length === 1 ? 'this parameter' : 'all these parameters'}` :
          `${index.producers.size} producers`;
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
      mapButton.hidden = !['place', 'lonlat', 'keyword', 'fmisid', 'area'].includes(s.locmode);
      mapButton.disabled = !s.location;
      if (rerender !== false)
        langInput.value = s.lang || '';
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
           section('Where', locModes, el('div', {class: 'row'}, locInput, mapButton), locMap),
           section(
               'Output',
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
