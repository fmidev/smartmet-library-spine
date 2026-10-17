// Browser tests of the data explorer (explorer/explorer.js).
//
// Usage: node explorer-browser-test.js <dir> <explorer dir> <uplot dir>
//
// <dir> contains the explorer page (explorer.html) and the SmartMet Swagger UI plugin
// (smartmet-swagger-ui.js) as spine serves them. The test serves them with the explorer
// and uPlot files, a test API catalog, a producer document and a TimeSeries service that
// records the requests, and checks the TimeSeries module in a headless Chromium: the
// parameter and producer searches narrowing each other, the default times, the request
// URL and the code, the charts (lines and percentiles) and the table, and the state in
// the URL. The test is skipped if Chromium or the uPlot files are not available.

'use strict';

const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const {browser, startBrowser, stopBrowser, sleep, waitFor, checker} =
    require('./browser-harness.js');

const [dir, explorerDir, uplotDir] = process.argv.slice(2);

if (!browser || !fs.existsSync(path.join(uplotDir || '', 'uPlot.iife.min.js')))
{
  console.log('Chromium or the uPlot files are not available, the explorer tests are not run');
  process.exit(0);
}

// ----------------------------------------------------------------------
// Test data
// ----------------------------------------------------------------------

const catalog = {
  linkset: [{
    anchor: '/.well-known/api-catalog',
    item: [
      {href: '/edr', title: 'EDR', 'x-smartmet-kind': 'edr'},
      {href: '/coverages', title: 'Coverages', 'x-smartmet-kind': 'coverages'},
      {href: '/timeseries', title: 'TimeSeries', 'x-smartmet-kind': 'timeseries'}
    ]
  }]
};

const hour = 3600e3;
const now = new Date();
now.setUTCMinutes(0, 0, 0);
const iso = (t) => new Date(t).toISOString().replace('.000', '');

const producers = {
  producers: [
    {
      name: 'pal',
      source: 'querydata',
      title: 'Pal Skandinavia',
      parameters: ['humidity', 'temperature', 'precipitation1h', 'wavedirection'],
      bbox: [5, 51, 49, 72],
      time: {start: iso(now - 6 * hour), end: iso(now.getTime() + 60 * hour), step: 60}
    },
    {
      name: 'ecmwf',
      source: 'querydata',
      parameters: ['temperature', 'windspeedms'],
      levels: [{type: 'pressure', unit: 'hPa', values: ['500', '850', '1000']}],
      time: {start: iso(now - 12 * hour), end: iso(now.getTime() + 240 * hour), step: 180}
    },
    {
      // The same name from another source
      name: 'ecmwf',
      source: 'grid',
      parameters: ['temperature'],
      bbox: [-180, -90, 180, 90],
      time: {start: iso(now - 12 * hour), end: iso(now.getTime() + 144 * hour), step: 60}
    },
    {
      name: 'hybrid',
      source: 'querydata',
      parameters: ['geomheight', 'pressure', 'temperature', 'dewpoint', 'windspeedms', 'winddirection'],
      levels: [{type: 'hybrid', values: Array.from({length: 65}, (_, i) => String(i + 1))}],
      time: {start: iso(now - 6 * hour), end: iso(now.getTime() + 48 * hour), step: 60}
    },
    {
      // An ensemble with levels of two types
      name: 'ens',
      source: 'grid',
      parameters: ['t-k'],
      time: {start: iso(now - 6 * hour), end: iso(now.getTime() + 48 * hour), step: 60}
    },
    {
      name: 'opendata',
      source: 'observation',
      parameters: ['temperature', 'ws_10min'],
      stations: 400,
      time: {start: iso(now - 240 * hour), end: iso(now), step: 10}
    }
  ],
  parameters: {
    temperature: {name: 'Temperature', description: 'Air temperature', unit: 'C'},
    humidity: {name: 'Humidity', description: 'Relative humidity', unit: '%'},
    windspeedms: {name: 'WindSpeedMS', description: 'Wind speed'},
    precipitation1h:
        {name: 'Precipitation1h', description: 'Precipitation in one hour', unit: 'mm'},
    wavedirection: {name: 'WaveDirection', description: 'Direction of the waves', unit: 'deg'}
  }
};

// The details of the grid producer ens, by run
function ensDetails(query)
{
  const runs = [iso(now - 6 * hour), iso(now - 18 * hour)];
  const run = query.get('origintime') || runs[0];
  return {
    name: 'ens',
    source: 'grid',
    producer: 'ENS',
    runs: runs.map((r) => ({origintime: r, status: 'ready'})),
    run: {origintime: run, status: 'ready', geometries: [{id: 1, name: 'A'}, {id: 2, name: 'B'}]},
    parameters: {
      't-k': {
        name: 'T-K',
        description: 'Temperature',
        levels: [
          {
            type: 'pressure',
            levelId: 2,
            unit: 'hPa',
            values: [1000, 850, 500],
            keys: [100000, 85000, 50000]
          },
          {type: 'height', levelId: 6, unit: 'm', values: [2, 10], keys: [2, 10]}
        ],
        forecasts: [
          {
            type: 3,
            name: 'EPS_PE',
            description: 'Ensemble forecast, perturbation',
            members: [1, 2, 3, 4, 5]
          },
          {
            type: 4,
            name: 'EPS_CF',
            description: 'Ensemble forecast, control forecast',
            members: [0]
          }
        ],
        aggregation: {name: 'mean', description: 'Mean over time period', period: 60},
        times: 6
      }
    }
  };
}

// The rows of a TimeSeries request: one location, or 30 for keyword=many
// An area or a radius: one row per time with the values of the 5 points as lists, as the
// TimeSeries API returns them
function areaRows(params)
{
  const rows = [];
  for (let i = 0; i < 6; i++)
  {
    const row = {};
    for (const p of params)
    {
      if (p === 'name')
        row.name = 'Area';
      else if (p === 'time')
        row.time = new Date(now.getTime() + i * hour).toISOString().slice(0, 19);
      else if (p === 'lat')
        row.lat = '[60.1 60.2 60.3 60.4 60.5]';
      else if (p === 'lon')
        row.lon = '[24.1 24.2 24.3 24.4 24.5]';
      else
        row[p] = `[${10 + i} ${11 + i} nan ${13 + i} ${14 + i}]`;
    }
    rows.push(row);
  }
  return rows;
}

// The statistics of an area computed by the server: one row per time
function statisticRows(params)
{
  const rows = [];
  for (let i = 0; i < 6; i++)
  {
    const row = {};
    for (const p of params)
    {
      if (p === 'name')
        row.name = 'Area';
      else if (p === 'time')
        row.time = new Date(now.getTime() + i * hour).toISOString().slice(0, 19);
      else
      {
        const m = /\[(\d+)\]/.exec(p);
        row[p] = 10 + i + (m ? Number(m[1]) / 10 : 5);
      }
    }
    rows.push(row);
  }
  return rows;
}

// The column of a parameter as the server names it: the sensor number and the quality code
// are appended to observation parameters
function responseColumn(p)
{
  const m = /\(:(\d+)(:qc)?\)/.exec(p);
  if (m)
    return `${p}_#${m[1]}${m[2] ? '_qc' : ''}`;
  return p.startsWith('qc_') ? `${p}_qc` : p;
}

function timeseries(query)
{
  const params = (query.get('param') || '').split(',');
  if (params.some((p) => /_a[[(]/.test(p)))
    return statisticRows(params);
  if (query.get('bbox') || (query.get('lonlat') || '').includes(':'))
    return areaRows(params);
  const locations = query.get('keyword') === 'many' ? 30 : 1;
  const rows = [];
  for (let l = 0; l < locations; l++)
  {
    const list = [];
    for (let i = 0; i < 6; i++)
    {
      const row = {};
      for (const p of params)
      {
        if (p === 'name')
          row.name = locations > 1 ? `Station ${l}` : 'Helsinki';
        else if (p === 'time')
          row.time = new Date(now.getTime() + i * hour).toISOString().slice(0, 19);
        else if (p === 'lat')
          row.lat = 60 + l * 0.1;
        else if (p === 'lon')
          row.lon = 25 + l * 0.05;
        else if (p.startsWith('@'))
          row[p] = p.startsWith('@AT') ? '20261016T000000' : 1;
        else if (/:qc\)$|^qc_/.test(p))
          row[responseColumn(p)] = 1;
        else
          row[responseColumn(p)] = 10 + l * 0.1 + i + (p.includes('(:11)') ? 0.5 : 0);
      }
      list.push(row);
    }
    rows.push(list);
  }
  return locations > 1 ? rows : rows[0];
}

// ----------------------------------------------------------------------
// The EDR API
// ----------------------------------------------------------------------

const edrCollections = {
  collections: [
    {
      id: 'pal',
      title: 'Pal Skandinavia',
      data_queries: {position: {}, radius: {}, area: {}, cube: {}, locations: {}},
      output_formats: ['CoverageJSON', 'GeoJSON'],
      extent: {
        spatial: {bbox: [[5, 51, 49, 72]], crs: 'OGC:CRS84'},
        temporal: {interval: [[iso(now - 6 * hour), iso(now.getTime() + 60 * hour)]]}
      },
      parameter_names: {
        temperature: {
          id: 'Temperature',
          label: 'Temperature',
          description: {en: 'Air temperature'},
          unit: {symbol: 'C'}
        },
        humidity: {id: 'Humidity', label: 'Humidity', description: 'Relative humidity'}
      }
    },
    {
      id: 'ecmwf_pressure',
      data_queries: {position: {}, cube: {}},
      output_formats: ['CoverageJSON'],
      extent: {
        spatial: {bbox: [[-30, 30, 50, 80]]},
        temporal: {interval: [[iso(now - 12 * hour), iso(now.getTime() + 240 * hour)]]},
        vertical: {vrs: 'pressure', values: ['1000', '850', '500']}
      },
      parameter_names: {temperature: {id: 'Temperature', label: 'Temperature'}}
    }
  ]
};

// A point series coverage at a point (and a level)
function pointCoverage(lon, lat, params, z)
{
  const ts = Array.from({length: 6}, (_, i) => iso(now.getTime() + i * hour));
  const axes = {t: {values: ts}, x: {values: [lon]}, y: {values: [lat]}};
  if (z !== undefined)
    axes.z = {values: [Number(z)]};
  const ranges = {};
  for (const p of params)
    ranges[p] = {
      type: 'NdArray',
      axisNames: ['t', 'y', 'x'],
      shape: [6, 1, 1],
      values: ts.map((t, i) => 10 + i + lat / 100 - (z ? Number(z) / 1000 : 0))
    };
  return {type: 'Coverage', domain: {type: 'Domain', axes: axes}, ranges: ranges};
}

function edr(p, query)
{
  if (p === '/edr/collections')
    return edrCollections;
  const m =
      /^\/edr\/collections\/([^/]+)\/(position|radius|area|cube|locations)(?:\/([^/]+))?$/.exec(p);
  if (!m)
    return null;
  const params = (query.get('parameter-name') || '').split(',').filter(Boolean);
  if (m[2] === 'locations' && !m[3])
    return {
      type: 'FeatureCollection',
      features: [
        {
          type: 'Feature',
          id: '100971',
          geometry: {type: 'Point', coordinates: [24.94, 60.17]},
          properties: {name: 'Helsinki Kaisaniemi'}
        },
        {
          type: 'Feature',
          id: '101004',
          geometry: {type: 'Point', coordinates: [24.96, 60.2]},
          properties: {name: 'Helsinki Kumpula'}
        }
      ]
    };
  if (m[2] === 'area' || m[2] === 'cube' || m[2] === 'radius')
    return {
      type: 'CoverageCollection',
      coverages: [[24, 60], [24.5, 60], [24, 60.5], [24.5, 60.5]].map(
          ([x, y]) => pointCoverage(x, y, params))
    };
  const z = (query.get('z') || '').split(',').filter(Boolean);
  if (z.length > 0)
    return {
      type: 'CoverageCollection',
      coverages: z.map((level) => pointCoverage(24.94, 60.17, params, level))
    };
  return pointCoverage(24.94, 60.17, params);
}

// ----------------------------------------------------------------------
// The Coverages API
// ----------------------------------------------------------------------

const coverageCollections = {
  collections: [{
    id: 'pal',
    title: 'pal',
    outputFormats: ['application/x-grib2', 'application/netcdf'],
    extent: {
      spatial: {bbox: [[-9, 51, 49, 72]]},
      temporal: {
        interval: [[iso(now - 6 * hour), iso(now.getTime() + 60 * hour)]],
        grid: {cellsCount: 67, resolution: 'PT1H'}
      },
      'x-smartmet-grid': {
        wkt: 'PROJCS["FMI_Polar_Stereographic",GEOGCS["FMI_Sphere"]]',
        width: 135,
        height: 150,
        resolution_km: [15, 15]
      }
    }
  }]
};

function coverages(p, query)
{
  if (p === '/coverages/collections')
    return coverageCollections;
  if (p === '/coverages/collections/pal/schema')
    return {
      properties:
          {Temperature: {title: 'Temperature', 'x-ogc-unit': 'C'}, Pressure: {title: 'Pressure'}}
    };
  if (p === '/coverages/collections/pal/outline')
  {
    const crs = query.get('crs') || 'native';
    const ring = query.get('bbox') ? [[20, 60], [32, 60], [32, 70], [20, 70], [20, 60]] :
                                     [[6, 51], [49, 51], [49, 70], [-9, 70], [6, 51]];
    return {
      type: 'Feature',
      geometry: {type: 'Polygon', coordinates: [ring]},
      properties: {
        crs: crs,
        grid: {width: 40, height: 82, estimated: crs !== 'native'},
        times: 25,
        parameters: 1,
        levels: 1,
        values: 82000,
        bytes: 328000
      }
    };
  }
  return null;
}

const requested = [];
const server = http.createServer((req, res) => {
  requested.push(req.url);
  const send = (status, type, body) => {
    res.writeHead(status, {'Content-Type': type, 'Cache-Control': 'no-store'});
    res.end(body);
  };
  const url = new URL(req.url, 'http://localhost');
  const p = url.pathname;
  if (p === '/api')
    return send(200, 'application/linkset+json', JSON.stringify(catalog));
  if (p === '/api/explorer')
    return send(200, 'text/html; charset=UTF-8', fs.readFileSync(path.join(dir, 'explorer.html')));
  if (p === '/api/explorer/explorer.js')
    return send(200, 'text/javascript', fs.readFileSync(path.join(explorerDir, 'explorer.js')));
  if (p === '/api/explorer/palettes.json')
    return send(200, 'application/json', fs.readFileSync(path.join(dir, 'palettes.json')));
  if (p === '/api/explorer/basemap.json')
    return send(200, 'application/json', fs.readFileSync(path.join(explorerDir, 'basemap.json')));
  if (p === '/api/explorer/explorer.css')
    return send(200, 'text/css', fs.readFileSync(path.join(explorerDir, 'explorer.css')));
  if (p === '/api/explorer/uPlot.iife.min.js')
    return send(200, 'text/javascript', fs.readFileSync(path.join(uplotDir, 'uPlot.iife.min.js')));
  if (p === '/api/explorer/uPlot.min.css')
    return send(200, 'text/css', fs.readFileSync(path.join(uplotDir, 'uPlot.min.css')));
  if (p === '/api/explorer/smartmet-swagger-ui.js')
    return send(200, 'text/javascript', fs.readFileSync(path.join(dir, 'smartmet-swagger-ui.js')));
  if (p === '/timeseries/api/producers')
    return send(200, 'application/json', JSON.stringify(producers));
  if (p === '/timeseries/api/producers/opendata')
    return send(200, 'application/json', JSON.stringify({
      name: 'opendata',
      source: 'observation',
      parameters: {},
      locations: [
        {
          id: '101004',
          type: 'fmisid',
          name: 'Helsinki Kumpula',
          lon: 24.96,
          lat: 60.2,
          start: '2006-10-11T00:00:00Z',
          end: '9999-12-31T00:00:00Z',
          wsi: '0-20000-0-02998'
        },
        {
          id: '100971',
          type: 'fmisid',
          name: 'Helsinki Kaisaniemi',
          lon: 24.94,
          lat: 60.18,
          start: '1844-01-01T00:00:00Z',
          end: '9999-12-31T00:00:00Z'
        },
        {
          id: '100001',
          type: 'fmisid',
          name: 'Closed station',
          lon: 25.5,
          lat: 61.0,
          start: '1950-01-01T00:00:00Z',
          end: '1990-01-01T00:00:00Z'
        }
      ]
    }));
  if (p === '/timeseries/api/producers/ens')
    return send(200, 'application/json', JSON.stringify(ensDetails(url.searchParams)));
  if (p === '/timeseries')
    return send(200, 'application/json', JSON.stringify(timeseries(url.searchParams)));
  const coverageDoc = coverages(p, url.searchParams);
  if (coverageDoc)
    return send(200, 'application/json', JSON.stringify(coverageDoc));
  const edrDoc = edr(p, url.searchParams);
  if (edrDoc)
    return send(200, 'application/json', JSON.stringify(edrDoc));
  send(404, 'text/plain', 'not found');
});

// ----------------------------------------------------------------------

const t = checker();
const check = t.check;

// Type into an autocomplete input and choose the suggestion with the given label
async function choose(b, inputSelector, text, label)
{
  await b.evaluate(`(() => {
    const input = document.querySelector(${JSON.stringify(inputSelector)});
    input.focus();
    input.value = ${JSON.stringify(text)};
    input.dispatchEvent(new Event("input", { bubbles: true }));
  })()`);
  await sleep(200);
  return b.evaluate(`(() => {
    const input = document.querySelector(${JSON.stringify(inputSelector)});
    const items = [...input.parentNode.querySelectorAll(".ac-list li")];
    const item = items.find((li) => li.querySelector(".ac-label") && li.querySelector(".ac-label").textContent === ${
      JSON.stringify(label)});
    const labels = items.map((li) => li.textContent);
    if (item) item.dispatchEvent(new MouseEvent("mousedown", { bubbles: true }));
    return { found: !!item, labels: labels };
  })()`);
}

const PARAM = '[aria-label="Parameter"]';

// Real mouse input, which also gives pointer events
async function mouse(b, type, x, y, buttons)
{
  await b.send('Input.dispatchMouseEvent', {type, x, y, button: 'left', buttons, clickCount: 1});
}

async function drag(b, x0, y0, x1, y1)
{
  await mouse(b, 'mouseMoved', x0, y0, 0);
  await mouse(b, 'mousePressed', x0, y0, 1);
  for (let i = 1; i <= 5; i++)
    await mouse(b, 'mouseMoved', x0 + ((x1 - x0) * i) / 5, y0 + ((y1 - y0) * i) / 5, 1);
  await mouse(b, 'mouseReleased', x1, y1, 0);
  await sleep(300);
}

async function mapRect(b)
{
  return b.evaluate(`(() => {
    const svg = document.querySelector('.xmap-svg');
    svg.scrollIntoView({ block: "center" });
    const r = svg.getBoundingClientRect();
    return { x: r.left, y: r.top, w: r.width, h: r.height };
  })()`);
}

async function setMode(b, mode)
{
  await b.evaluate(`document.querySelector('[data-mode="${mode}"]').click()`);
  await sleep(200);
}

async function setLocation(b, text)
{
  await b.evaluate(`(() => {
    const input = document.querySelector('[aria-label="Location"]');
    input.value = ${JSON.stringify(text)};
    input.dispatchEvent(new Event("input", { bubbles: true }));
  })()`);
  await sleep(500);
}

const currentQuery = (b) => b.evaluate(`document.querySelector('code.url').textContent`)
                                .then((u) => new URL(u).searchParams);
const PRODUCER = '[aria-label="Producer"]';

async function main()
{
  await new Promise((r) => server.listen(0, '127.0.0.1', r));
  const base = `http://127.0.0.1:${server.address().port}`;
  const b = await startBrowser('1280,1000');
  try
  {
    await b.send('Page.navigate', {url: `${base}/api/explorer#api=/timeseries`});
    await waitFor(b, `!!document.querySelector('${PARAM}')`, 'the TimeSeries module');

    const apis = await b.evaluate(
        `[...document.querySelectorAll('nav select option')].map((o) => o.textContent)`);
    check(
        'only the APIs with a module are offered',
        JSON.stringify(apis) ===
            JSON.stringify(['EDR (/edr)', 'Coverages (/coverages)', 'TimeSeries (/timeseries)']),
        apis);

    // A parameter narrows the producers
    let r = await choose(b, PARAM, 'temp', 'Temperature');
    check(
        'parameter search by name', r.found && r.labels.some((l) => l.includes('5 producers')), r);
    r = await choose(b, PARAM, 'relative', 'Humidity');
    check('parameter search by description', r.found, r);
    const chips = await b.evaluate(
        `[...document.querySelectorAll('.chip')].map((c) => c.firstChild.textContent)`);
    check(
        'chosen parameters',
        JSON.stringify(chips) === JSON.stringify(['Temperature', 'Humidity']),
        chips);
    const note = await b.evaluate(`document.querySelector('.form .step .muted.small').textContent`);
    check(
        'producers with all the parameters',
        note === '1 of 6 producers have all these parameters',
        note);
    r = await choose(b, PRODUCER, '', 'pal');
    check(
        'only the producers with the parameters',
        JSON.stringify(r.labels.map((l) => l.split(/querydata|grid|observation/)[0])) ===
            JSON.stringify(['pal']),
        r);

    // The producer sets the times and narrows the parameters
    await sleep(200);
    const card = await b.evaluate(`(() => ({ shown: !document.querySelector('.model-card').hidden,
      text: document.querySelector('.model-card').textContent,
      start: document.querySelector('[aria-label="Start time (UTC)"]').value,
      end: document.querySelector('[aria-label="End time (UTC)"]').value }))()`);
    check(
        'producer card',
        card.shown && card.text.includes('pal') && card.text.includes('Pal Skandinavia'),
        card);
    check(
        'default times',
        card.start !== '' && card.end !== '' &&
            new Date(card.end + 'Z') - new Date(card.start + 'Z') === 48 * hour,
        card);
    r = await choose(b, PARAM, 'wind', 'WindSpeedMS');
    check(
        'parameters of the producer only',
        !r.found && r.labels.some((l) => l.includes('pal has no parameter')),
        r);
    await b.evaluate(`document.querySelector('${PARAM}').value = ""`);

    // The location completes the request
    let run = await b.evaluate(`({ disabled: document.querySelector('button.primary').disabled,
      missing: document.querySelector('.missing').textContent })`);
    check('location still needed', run.disabled && run.missing === 'Still needed: a location', run);
    await b.evaluate(`(() => {
      const input = document.querySelector('[aria-label="Location"]');
      input.value = "Helsinki";
      input.dispatchEvent(new Event("input", { bubbles: true }));
    })()`);
    await sleep(500);
    const url = await b.evaluate(`document.querySelector('code.url').textContent`);
    const q = new URL(url).searchParams;
    check(
        'request URL',
        q.get('producer') === 'pal' &&
            q.get('param') === 'name,time,lat,lon,temperature,humidity' &&
            q.get('place') === 'Helsinki' && q.get('format') === 'json' && q.get('tz') === 'UTC' &&
            q.get('starttime') === card.start + ':00Z' &&
            url.includes('param=name,time,lat,lon,temperature,humidity'),
        url);
    run = await b.evaluate(`document.querySelector('button.primary').disabled`);
    check('run enabled', run === false, run);

    // The code of the request
    const code = await b.evaluate(`(() => {
      document.querySelector('.code-details').open = true;
      const tabs = [...document.querySelectorAll('.code-head .tabs button')];
      const python = tabs.find((x) => x.textContent === "Python");
      if (python) python.click();
      return { tabs: tabs.map((x) => x.textContent), text: document.querySelector('pre.code').textContent };
    })()`);
    check('code tabs', code.tabs.length === 8 && code.tabs[0] === 'bash', code.tabs);
    check(
        'python code',
        code.text.includes('urllib') && code.text.includes('/timeseries?producer=pal'),
        code.text.slice(0, 300));

    // Running draws a chart per parameter and the table
    await b.evaluate(`document.querySelector('button.primary').click()`);
    await waitFor(b, `document.querySelectorAll('.chart canvas').length >= 2`, 'the charts');
    const sent = requested.filter((u) => u.startsWith('/timeseries?'));
    check('the request is sent as shown', sent.length === 1 && base + sent[0] === url, sent);
    const charts =
        await b.evaluate(`[...document.querySelectorAll('.chart')].map((c) => c.dataset.param)`);
    check(
        'a chart per parameter',
        JSON.stringify(charts) === JSON.stringify(['temperature', 'humidity']),
        charts);
    const table = await b.evaluate(`(() => {
      [...document.querySelectorAll('.results-head .tabs button')].find((x) => x.textContent === "Table").click();
      return { rows: document.querySelectorAll('table.data tbody tr').length,
               head: [...document.querySelectorAll('table.data th')].map((th) => th.textContent) };
    })()`);
    check(
        'table',
        table.rows === 6 &&
            JSON.stringify(table.head) ===
                JSON.stringify(['name', 'time', 'lat', 'lon', 'temperature', 'humidity']),
        table);

    // Many locations are drawn as percentiles
    await b.evaluate(`(() => {
      document.querySelector('[data-mode="keyword"]').click();
    })()`);
    await sleep(200);
    await b.evaluate(`(() => {
      const input = document.querySelector('[aria-label="Location"]');
      input.value = "many";
      input.dispatchEvent(new Event("input", { bubbles: true }));
    })()`);
    await sleep(500);
    await b.evaluate(`document.querySelector('button.primary').click()`);
    await waitFor(
        b,
        `[...document.querySelectorAll('.chart-title')].some((x) => x.textContent.includes("percentiles"))`,
        'the percentile chart');
    const fan = await b.evaluate(
        `({ titles: [...document.querySelectorAll('.chart-title')].map((x) => x.textContent),
      count: document.querySelector('.results-head .muted').textContent })`);
    check(
        'percentile charts',
        fan.titles[0] === 'temperature: median and percentiles of 30 locations',
        fan);
    check('row and location count', fan.count === '180 rows, 30 locations', fan);

    // ... or as a swarm of all the locations and the median
    const swarm = await b.evaluate(`(async () => {
      document.querySelector('.chart-modes [data-mode="lines"]').click();
      await new Promise((r) => setTimeout(r, 300));
      return { mode: document.querySelector('.chart').dataset.mode,
               title: document.querySelector('.chart-title').textContent,
               legends: [...document.querySelectorAll('.chart .u-legend')].filter((l) => l.offsetParent !== null).length,
               canvases: document.querySelectorAll('.chart canvas').length };
    })()`);
    check(
        'swarm of all locations',
        swarm.mode === 'lines' &&
            swarm.title ===
                'temperature: 30 locations, coloured from south to north, and the median' &&
            swarm.legends === 0 && swarm.canvases === 2,
        swarm);
    const backToPercentiles = await b.evaluate(`(async () => {
      document.querySelector('.chart-modes [data-mode="percentiles"]').click();
      await new Promise((r) => setTimeout(r, 300));
      return document.querySelector('.chart').dataset.mode;
    })()`);
    check('back to percentiles', backToPercentiles === 'percentiles', backToPercentiles);

    // A keyword can be summarized by the server too, as one group of locations
    await b.evaluate(`document.querySelector('[data-areavalues="statistics"]').click()`);
    await sleep(300);
    const grouped = await currentQuery(b);
    check(
        'keyword statistics requested',
        grouped.get('grouplocations') === '1' && grouped.get('keyword') === 'many' &&
            grouped.get('param').includes('nanmedian_a(temperature)'),
        grouped.toString());
    await b.evaluate(`document.querySelector('[data-areavalues="points"]').click()`);
    await sleep(300);
    const ungrouped = await currentQuery(b);
    check(
        'keyword points requested', ungrouped.get('grouplocations') === null, ungrouped.toString());
    const keywordPoints = await b.evaluate(`document.querySelectorAll('.xmap-point').length`);
    check('the locations of the result on the map', keywordPoints > 0, keywordPoints);

    // The state is kept in the URL
    const hash = await b.evaluate(`location.hash`);
    await b.send('Page.navigate', {url: `${base}/api/explorer${hash}`});
    await sleep(300);
    await waitFor(b, `document.querySelectorAll('.chip').length === 2`, 'the restored state');
    const restored = await b.evaluate(`({ url: document.querySelector('code.url').textContent,
      producer: document.querySelector('.model-card strong').textContent })`);
    check(
        'state restored from the URL',
        restored.producer === 'pal' && restored.url.includes('keyword=many'),
        restored);

    // The background map of the explorer
    const land = await b.evaluate(
        `(() => { const l = document.querySelector('.xmap-land'); return l ? l.getAttribute('d').length : 0; })()`);
    check('land drawn from the basemap', land > 1000, land);

    // The area of the producer on the map
    const region = await b.evaluate(`document.querySelectorAll('.xmap-region').length`);
    check('producer area on the map', region === 1, region);

    // A point clicked on the map
    await setMode(b, 'lonlat');
    let tools = await b.evaluate(
        `[...document.querySelectorAll('.xmap-tools button')].map((x) => x.dataset.tool + ":" + x.getAttribute("aria-pressed"))`);
    check(
        'point tools',
        JSON.stringify(tools) === JSON.stringify(['move:false', 'point:true']),
        tools);
    let m = await mapRect(b);
    await mouse(b, 'mouseMoved', m.x + m.w / 2, m.y + m.h / 2, 0);
    await mouse(b, 'mousePressed', m.x + m.w / 2, m.y + m.h / 2, 1);
    await mouse(b, 'mouseReleased', m.x + m.w / 2, m.y + m.h / 2, 0);
    await sleep(400);
    let mq = await currentQuery(b);
    const point = (mq.get('lonlat') || '').split(',').map(Number);
    check(
        'point from the map',
        point.length === 2 && point[0] > 5 && point[0] < 49 && point[1] > 51 && point[1] < 72,
        mq.get('lonlat'));

    // A radius dragged on the map
    await setMode(b, 'radius');
    m = await mapRect(b);
    await drag(b, m.x + m.w / 2, m.y + m.h / 2, m.x + m.w / 2 + 40, m.y + m.h / 2);
    mq = await currentQuery(b);
    const radius = /^(-?[\d.]+),(-?[\d.]+):(\d+)$/.exec(mq.get('lonlat') || '');
    check('radius from the map', radius && Number(radius[3]) > 10, mq.get('lonlat'));
    const polygon = await b.evaluate(`document.querySelectorAll('.xmap-selection').length`);
    check('radius drawn', polygon === 1, polygon);

    // The values of an area: the statistics computed by the server by default
    const areaQuery = mq.get('param');
    check(
        'area statistics requested',
        areaQuery ===
            'name,time,nanpercentile_a[10](temperature),nanpercentile_a[25](temperature),nanmedian_a(temperature),nanpercentile_a[75](temperature),nanpercentile_a[90](temperature),nanpercentile_a[10](humidity),nanpercentile_a[25](humidity),nanmedian_a(humidity),nanpercentile_a[75](humidity),nanpercentile_a[90](humidity)',
        areaQuery);
    const readable = await b.evaluate(
        `document.querySelector('code.url').textContent.includes("nanmedian_a(temperature)")`);
    check('readable function names in the URL', readable, readable);
    await b.evaluate(`document.querySelector('button.primary').click()`);
    await waitFor(
        b,
        `document.querySelectorAll('.chart[data-mode="server"] canvas').length === 2`,
        'the server statistics');
    const serverTitle = await b.evaluate(`document.querySelector('.chart-title').textContent`);
    check(
        'server statistics drawn',
        serverTitle === 'temperature: median and percentiles of the area, computed by the server',
        serverTitle);

    // ... or every point
    await b.evaluate(`document.querySelector('[data-areavalues="points"]').click()`);
    await sleep(300);
    mq = await currentQuery(b);
    check(
        'every point requested',
        mq.get('param') === 'name,time,lat,lon,temperature,humidity',
        mq.get('param'));

    // The points of the radius in the results
    await b.evaluate(`document.querySelector('button.primary').click()`);
    await waitFor(
        b,
        `(document.querySelector('.results-head .muted') || {}).textContent === "30 rows, 5 locations"`,
        'the radius results');
    const areaPoints = await b.evaluate(`document.querySelectorAll('.xmap-point').length`);
    check('points of the radius on the map', areaPoints === 5, areaPoints);

    // The values on a map: a dot per point, a time slider and a colour scale
    const valueMap = await b.evaluate(`(async () => {
      [...document.querySelectorAll('.results-head .tabs button')].find((x) => x.textContent === "Map").click();
      await new Promise((r) => setTimeout(r, 400));
      const pane = [...document.querySelectorAll('.results .pane')].find((p) => !p.hidden);
      const colours = () => [...pane.querySelectorAll('.xmap-dot')].map((c) => c.getAttribute('fill')).join(' ');
      const first = colours();
      const slider = pane.querySelector('input[type="range"]');
      slider.value = "5";
      slider.dispatchEvent(new Event("input", { bubbles: true }));
      await new Promise((r) => setTimeout(r, 200));
      const last = colours();
      const legendBefore = pane.querySelector('.legend-step').style.background;
      const palette = pane.querySelector('[aria-label="Colours"]');
      palette.value = "temperature";
      palette.dispatchEvent(new Event("change", { bubbles: true }));
      const legendAfter = pane.querySelector('.legend-step').style.background;
      return { dots: pane.querySelectorAll('.xmap-dot').length, max: slider.max, changed: first !== last,
               legend: legendBefore !== legendAfter, min: pane.querySelector('[aria-label="Minimum"]').value,
               time: pane.querySelector('.value-time .muted').textContent };
    })()`);
    check(
        'values on a map',
        valueMap.dots === 4 && valueMap.max === '5' && valueMap.changed && valueMap.legend &&
            valueMap.min !== '' && valueMap.time.endsWith('UTC'),
        valueMap);

    // The production palette of the parameter, drawn band by band
    const production = await b.evaluate(`(async () => {
      const pane = [...document.querySelectorAll('.results .pane')].find((p) => !p.hidden);
      const palette = pane.querySelector('[aria-label="Colours"]');
      const param = pane.querySelector('[aria-label="Parameter"]');
      param.value = "temperature";
      param.dispatchEvent(new Event("change", { bubbles: true }));
      await new Promise((r) => setTimeout(r, 200));
      return { palette: palette.value, disabled: pane.querySelector('[aria-label="Minimum"]').disabled,
               colours: [...new Set([...pane.querySelectorAll('.xmap-dot')].map((c) => c.getAttribute('fill')))].sort(),
               bands: pane.querySelectorAll('.legend-bands .legend-step').length };
    })()`);
    check(
        'production palette by default',
        production.palette === 'production:temperature' && production.disabled &&
            production.bands === 3 &&
            production.colours.every(
                (c) => ['rgb(0,0,255)', 'rgb(0,255,0)', 'rgb(255,0,0)'].includes(c)),
        production);

    // A box dragged on the map
    await setMode(b, 'bbox');
    m = await mapRect(b);
    await drag(b, m.x + m.w / 2 - 60, m.y + m.h / 2 - 40, m.x + m.w / 2 + 60, m.y + m.h / 2 + 40);
    mq = await currentQuery(b);
    const bbox = (mq.get('bbox') || '').split(',').map(Number);
    check(
        'box from the map',
        bbox.length === 4 && bbox[0] < bbox[2] && bbox[1] < bbox[3],
        mq.get('bbox'));

    // A box outside the producer's area
    await setLocation(b, '100,-10,110,0');
    const outside = await b.evaluate(`document.querySelector('.warn').textContent`);
    check(
        'selection outside the producer area',
        outside === 'The selection is outside the area of pal',
        outside);
    // ... narrows the producers to those covering it (producers without an area are kept)
    await b.evaluate(
        `[...document.querySelectorAll('.model-card button')].find((x) => x.textContent === "Change").click()`);
    await sleep(300);
    const covering =
        await b.evaluate(`document.querySelector('.form .step .muted.small').textContent`);
    check(
        'producers covering the selection',
        covering === '0 of 6 producers have all these parameters in the selected area',
        covering);
    await setLocation(b, 'not a box');
    const invalid = await b.evaluate(`document.querySelector('.warn').textContent`);
    check('invalid box', invalid.startsWith('Not a valid box'), invalid);

    // A producer name served by several sources is requested from the chosen source
    for (const [source, card] of [['grid', 'grid'], ['querydata', 'querydata'], ['', 'querydata']])
    {
      await b.send('Page.navigate', {url: `${base}/api`});
      await sleep(200);
      await b.send('Page.navigate', {
        url: `${base}/api/explorer#api=/timeseries&producer=ecmwf&source=${
            source}&params=temperature&locmode=place&location=Helsinki`
      });
      await waitFor(b, `!!document.querySelector('.model-card strong')`, 'the producer card');
      const chosen = await b.evaluate(`({ url: document.querySelector('code.url').textContent,
        badge: document.querySelector('.model-card .badge').textContent,
        text: document.querySelector('.model-card').textContent })`);
      const wanted = source || 'querydata';
      check(
          `producer from the ${wanted} source${source ? '' : ' by default'}`,
          chosen.badge === card && new URL(chosen.url).searchParams.get('source') === wanted &&
              !chosen.text.includes('null'),
          chosen);
    }

    // Values over a period: the function recommended for each kind of parameter
    await b.send('Page.navigate', {url: `${base}/api`});
    await sleep(200);
    await b.send('Page.navigate', {
      url: `${
          base}/api/explorer#api=/timeseries&producer=pal&source=querydata&params=temperature,precipitation1h,wavedirection&locmode=place&location=Helsinki`
    });
    await waitFor(b, `!!document.querySelector('[data-agg="period"]')`, 'the period choices');
    await b.evaluate(`document.querySelector('[data-agg="period"]').click()`);
    await sleep(200);
    await b.evaluate(`(() => {
      const select = document.querySelector('[aria-label="Period"]');
      select.value = "1440";
      select.dispatchEvent(new Event("change", { bubbles: true }));
    })()`);
    await sleep(300);
    let aq = await currentQuery(b);
    const aggSummary = await b.evaluate(
        `[...document.querySelectorAll('.agg-summary li')].map((x) => x.textContent)`);
    check(
        'recommended functions over a period',
        aq.get('param') ===
                'name,time,lat,lon,mean_t(temperature/1380m/0m),sum_t(precipitation1h/1380m/0m),' +
                    'meandir_t(wavedirection/1380m/0m)' &&
            aq.get('timestep') === '1440' &&
            aggSummary.includes(
                'precipitation1h: sum of the 24 hours ending at each time (amount)') &&
            !aggSummary.some((x) => x.includes('means nothing')),
        {param: aq.get('param'), timestep: aq.get('timestep'), aggSummary});
    await b.evaluate(`document.querySelector('button.primary').click()`);
    await waitFor(b, `document.querySelectorAll('.chart canvas').length >= 3`, 'the period charts');
    const periodCharts =
        await b.evaluate(`[...document.querySelectorAll('.chart')].map((c) => c.dataset.param)`);
    check(
        'charts of the period values',
        JSON.stringify(periodCharts) ===
            JSON.stringify(['temperature', 'precipitation1h', 'wavedirection']),
        periodCharts);
    await b.evaluate(`(() => {
      const select = document.querySelector('[aria-label="Function"]');
      select.value = "sum";
      select.dispatchEvent(new Event("change", { bubbles: true }));
    })()`);
    await sleep(300);
    const sumWarnings = await b.evaluate(
        `[...document.querySelectorAll('.agg-summary li.warn')].map((x) => x.textContent)`);
    check(
        'warnings of unsuitable functions',
        sumWarnings.length === 2 &&
            sumWarnings[0] === 'temperature is not an amount: its sum means nothing' &&
            sumWarnings[1].startsWith('wavedirection is a direction'),
        sumWarnings);
    await b.evaluate(`(() => {
      const select = document.querySelector('[aria-label="Function"]');
      select.value = "percentile";
      select.dispatchEvent(new Event("change", { bubbles: true }));
      const window = document.querySelector('[aria-label="Period placement"]');
      window.value = "centre";
      window.dispatchEvent(new Event("change", { bubbles: true }));
    })()`);
    await sleep(300);
    aq = await currentQuery(b);
    check(
        'percentile centred on the times',
        aq.get('param').includes('percentile_t[90](temperature/720m/720m)') &&
            await b.evaluate(
                `!document.querySelector('[aria-label="Percentile"]').closest('label').hidden`),
        aq.get('param'));
    await setMode(b, 'radius');
    await setLocation(b, '24.9,60.2:20');
    aq = await currentQuery(b);
    check(
        'period values inside the area statistics',
        aq.get('param').includes('nanmedian_a(percentile_t[90](temperature/720m/720m))'),
        aq.get('param'));

    // Observations: the sensors, a filter of the quality codes and the codes themselves
    await b.send('Page.navigate', {url: `${base}/api`});
    await sleep(200);
    await b.send('Page.navigate', {
      url: `${
          base}/api/explorer#api=/timeseries&producer=opendata&source=observation&params=temperature&locmode=fmisid&location=101004`
    });
    await waitFor(
        b,
        `!!document.querySelector('[aria-label="Sensors"]') && document.querySelector('[aria-label="Sensors"]').offsetParent !== null`,
        'the observation choices');
    await b.evaluate(`(() => {
      const input = document.querySelector('[aria-label="Sensors"]');
      input.value = "1, 11";
      input.dispatchEvent(new Event("input", { bubbles: true }));
    })()`);
    await sleep(500);
    await b.evaluate(`(() => {
      const select = document.querySelector('[aria-label="Quality"]');
      select.value = "custom";
      select.dispatchEvent(new Event("change", { bubbles: true }));
    })()`);
    await sleep(200);
    await b.evaluate(`(() => {
      const input = document.querySelector('[aria-label="Quality filter"]');
      input.value = "le 1";
      input.dispatchEvent(new Event("input", { bubbles: true }));
      document.querySelector('[aria-label="Quality codes"]').click();
    })()`);
    await sleep(500);
    const oq = await currentQuery(b);
    check(
        'sensors and quality in the request',
        oq.get('param') ===
                'name,time,lat,lon,temperature(:1),temperature(:11),temperature(:1:qc),' +
                    'temperature(:11:qc)' &&
            oq.get('data_quality') === 'le 1',
        oq.toString());
    await b.evaluate(`document.querySelector('button.primary').click()`);
    await waitFor(b, `document.querySelectorAll('.chart canvas').length >= 1`, 'the sensor chart');
    const sensorSeries = await b.evaluate(
        `[...document.querySelectorAll('.chart[data-param="temperature"] .u-legend .u-series')].map((x) => x.textContent)`);
    const sensorTable = await b.evaluate(`(() => {
      [...document.querySelectorAll('.results-head .tabs button')].find((x) => x.textContent === "Table").click();
      return { head: [...document.querySelectorAll('table.data th')].map((th) => th.textContent),
               rows: document.querySelectorAll('table.data tbody tr').length };
    })()`);
    check(
        'a series per sensor, the codes in the table',
        sensorSeries.some((x) => x.includes('sensor 1')) &&
            sensorSeries.some((x) => x.includes('sensor 11')) &&
            sensorTable.head.includes('temperature quality') && sensorTable.rows === 12,
        {sensorSeries, sensorTable});

    // The stations of an observation producer: searched by name and chosen on the map
    await b.send('Page.navigate', {url: `${base}/api`});
    await sleep(200);
    await b.send('Page.navigate', {
      url: `${
          base}/api/explorer#api=/timeseries&producer=opendata&source=observation&params=temperature&locmode=fmisid&location=101004`
    });
    await waitFor(b, `document.querySelectorAll('.xmap-point').length > 0`, 'the stations');
    let st = await b.evaluate(`({ points: document.querySelectorAll('.xmap-point').length,
      selected: [...document.querySelectorAll('.xmap-point.selected')].length,
      note: [...document.querySelectorAll('p.muted.small')].map((p) => p.textContent).find((x) => x.includes('stations')) })`);
    check(
        'the stations operating in the period',
        st.points === 2 && st.selected === 1 &&
            st.note ===
                '2 of the 3 stations of opendata operate in the chosen period: click ' +
                    'them on the map to choose',
        st);
    const stationChoice = await choose(b, '[aria-label="Station"]', 'kais', 'Helsinki Kaisaniemi');
    await sleep(200);
    st = await b.evaluate(`({ location: document.querySelector('[aria-label="Location"]').value,
      selected: document.querySelectorAll('.xmap-point.selected').length })`);
    check(
        'a station chosen by name',
        stationChoice.found && st.location === '101004,100971' && st.selected === 2,
        {stationChoice, st});
    await b.evaluate(
        `document.querySelector('.xmap-point.selected').dispatchEvent(new MouseEvent("click", { bubbles: true }))`);
    await sleep(200);
    const afterClick = await b.evaluate(`document.querySelector('[aria-label="Location"]').value`);
    check('a station removed on the map', afterClick === '100971', afterClick);
    const obsPresets = await b.evaluate(`[...document.querySelectorAll('button.small')]
      .filter((x) => ['Whole data', 'Next 24 h', 'Last 24 h', 'Last week', 'Last month'].includes(x.textContent) && !x.hidden)
      .map((x) => x.textContent)`);
    check(
        'no whole data of observations, but the last week and month',
        JSON.stringify(obsPresets) === '["Last 24 h","Last week","Last month"]',
        obsPresets);
    await b.evaluate(
        `[...document.querySelectorAll('button.small')].find((x) => x.textContent === 'Last week').click()`);
    await sleep(200);
    const week = await currentQuery(b);
    check(
        'the last week',
        new Date(week.get('endtime')) - new Date(week.get('starttime')) === 7 * 24 * hour,
        week.toString());

    // Vertical profiles and the skew-T of model levels: a standard atmosphere on 20 levels
    const vertical = await b.evaluate(`(() => { try {
      const E = SmartMetExplorer;
      const rows = [];
      for (const hour of [0, 6])
        for (let k = 1; k <= 20; k++) {
          const p = 1000 - (k - 1) * 45;
          const z = 44330 * (1 - Math.pow(p / 1013.25, 0.1903));
          const t = 15 - 0.0065 * z + hour * 0.2;
          rows.push({ name: 'Helsinki', time: new Date(Date.UTC(2026, 9, 15, hour)).toISOString(),
                      level: 21 - k, pressure: p, geomheight: z, temperature: t,
                      humidity: 80 - k * 3, windspeedms: 2 + k, winddirection: 200 + k * 5 });
        }
      const axes = E.verticalAxes(rows, 'hybrid').map((a) => a.id);
      const box = document.createElement('div');
      box.style.width = '600px';
      document.body.append(box);
      E.drawProfiles(box, rows, ['temperature', 'humidity'], 'hybrid');
      const profiles = { charts: box.querySelectorAll('.vchart').length,
        lines: box.querySelectorAll('.vline').length,
        axis: [...box.querySelectorAll('[aria-label="Vertical axis"] option')].map((o) => o.value) };
      box.querySelector('[aria-label="All times"]').click();
      profiles.overlaid = box.querySelectorAll('.vline').length;
      E.drawSkewT(box, rows, 'hybrid');
      const skewt = { temperature: box.querySelectorAll('.sk-temperature').length,
        dewpoint: box.querySelectorAll('.sk-dewpoint').length,
        parcel: box.querySelectorAll('.sk-parcel').length,
        barbs: box.querySelectorAll('.barb').length,
        info: box.querySelector('.skewt-info').textContent,
        has: E.hasSounding(rows, 'hybrid'), noPressure: E.hasSounding(rows.map(({pressure, ...r}) => r), 'depth') };
      // The other diagrams, the pointer and a sounding to compare with
      const diagrams = {};
      for (const type of ['skewt', 'tephigram', 'stuve']) {
        const d = E.drawSkewT(box, rows, 'hybrid', { type, compare: {
          soundings: (times) => [{ time: times[0], levels: [{ p: 1000, t: 10, td: 5, speed: null, direction: null },
                                                           { p: 500, t: -25, td: -35, speed: null, direction: null }] }],
          label: () => 'other' } });
        const svgEl = box.querySelector('.skewt-diagram');
        const hover = svgEl.querySelector('.vhover');
        const r = hover.getBoundingClientRect();
        hover.dispatchEvent(new MouseEvent('mousemove', { clientX: r.left + r.width / 2, clientY: r.top + r.height * 0.8, bubbles: true }));
        diagrams[type] = { label: svgEl.getAttribute('aria-label'), compare: box.querySelectorAll('.sk-temperature.compare').length,
                           readout: box.querySelector('.sk-readout').textContent, info: box.querySelector('.skewt-info').textContent,
                           redraw: typeof d.redraw };
      }
      E.drawSection(box, rows, ['temperature', 'humidity'], 'hybrid');
      const cells = box.querySelectorAll('.section-cell');
      cells[5].dispatchEvent(new MouseEvent('mouseenter'));
      const top = box.querySelector('[aria-label="Top of the section"]');
      const section = { cells: cells.length, readout: box.querySelector('.sk-readout').textContent,
                        tops: [...top.options].map((o) => o.value) };
      top.value = '500';
      top.dispatchEvent(new Event('change'));
      section.limited = box.querySelectorAll('.section-cell').length;
      E.drawProfiles(box, rows, ['temperature'], 'hybrid');
      const vh = box.querySelector('.vchart .vhover');
      const vr = vh.getBoundingClientRect();
      vh.dispatchEvent(new MouseEvent('mousemove', { clientX: vr.left + vr.width / 2, clientY: vr.top + vr.height / 2, bubbles: true }));
      const profileTip = box.querySelector('.vtip').getAttribute('visibility') + ' ' + box.querySelector('.vtip text').textContent;
      box.remove();
      const lcl = E.liftingCondensationLevel(20, 10, 1000);
      return { axes, profiles, skewt, diagrams, section, profileTip, lcl: [Math.round(lcl.p), Math.round(lcl.t * 10) / 10],
               moist: Math.round(E.moistAdiabat(20, 1000, 500).pop()[1] * 10) / 10 };
    } catch (e) { return { error: String(e.stack) }; } })()`);
    if (vertical.error)
      console.log('VERTICAL ERROR', vertical.error);
    const paAxes = await b.evaluate(
        `SmartMetExplorer.verticalAxes([{ level: 1, 'p-pa': 85000 }], 'hybrid').map((a) => a.id + ':' + a.value({ level: 1, 'p-pa': 85000 }))`);
    check(
        'model level pressures in Pa',
        JSON.stringify(paAxes) === '["pressure:850","level:1"]',
        paAxes);
    check(
        'vertical axes of model levels',
        JSON.stringify(vertical.axes) === '["pressure","height","level"]',
        vertical.axes);
    check(
        'profiles per parameter, one time or all',
        vertical.profiles.charts === 2 && vertical.profiles.lines === 2 &&
            vertical.profiles.overlaid === 4 &&
            JSON.stringify(vertical.profiles.axis) === '["pressure","height","level"]',
        vertical.profiles);
    check(
        'skew-T of a sounding',
        vertical.skewt.temperature === 1 && vertical.skewt.dewpoint === 1 &&
            vertical.skewt.parcel === 1 && vertical.skewt.barbs > 5 && vertical.skewt.has &&
            !vertical.skewt.noPressure &&
            vertical.skewt.info.startsWith('Lifting condensation level'),
        vertical.skewt);
    const kelvins = await b.evaluate(`(() => {
      const E = SmartMetExplorer;
      const rows = (offset) => [1000, 850, 700, 500].map((p, i) => ({ name: 'A', time: '2026-10-15T00:00:00Z',
        level: p, 't-k': 15 - i * 12 + offset, 'rh-prcnt': 70 }));
      const lcl = (offset) => {
        const box = document.createElement('div');
        document.body.append(box);
        E.drawSkewT(box, rows(offset), 'pressure');
        const text = box.querySelector('.skewt-info').textContent;
        box.remove();
        return text.split(',')[1].trim();
      };
      return [lcl(0), lcl(273.15)];
    })()`);
    check(
        'temperatures in Kelvins or already in Celsius',
        kelvins[0] === kelvins[1] && /^[0-9.]+ C$/.test(kelvins[0]),
        kelvins);
    check(
        'three sounding diagrams with the pointer and a comparison',
        ['skewt', 'tephigram', 'stuve'].every(
            (type) => vertical.diagrams[type].compare === 1 &&
                /^\d+ hPa · -?\d+\.\d C · θ \d+\.\d K/.test(vertical.diagrams[type].readout) &&
                vertical.diagrams[type].info.includes('thin lines: other') &&
                vertical.diagrams[type].redraw === 'function') &&
            vertical.diagrams.tephigram.label === 'Tephigram' &&
            vertical.diagrams.stuve.label === 'Stüve',
        vertical.diagrams);
    check(
        'time-height section and its pointer',
        vertical.section.cells === 40 && vertical.section.readout.includes('temperature') &&
            JSON.stringify(vertical.section.tops) === '["","200","500","700","900"]' &&
            vertical.section.limited > 0 && vertical.section.limited < 40,
        vertical.section);
    check(
        'the values of a profile under the pointer',
        vertical.profileTip.startsWith('visible Helsinki'),
        vertical.profileTip);
    check(
        'condensation level and moist adiabat',
        JSON.stringify(vertical.lcl) === '[861,7.8]' && vertical.moist > -9.5 &&
            vertical.moist < -7.5,
        vertical);

    // A cross section along a path: 5 points of a path at 10 levels
    const cross = await b.evaluate(`(async () => {
      const E = SmartMetExplorer;
      const rows = [];
      for (let i = 0; i < 5; i++)
        for (let k = 1; k <= 10; k++)
          rows.push({ name: 'path1', time: '2026-10-15T06:00:00Z', lat: 60 + i * 0.1, lon: 24 + i * 0.2,
                      level: k, pressure: 1000 - (k - 1) * 90, temperature: 15 - k * 5 + i });
      const box = document.createElement('div');
      box.style.width = '700px';
      document.body.append(box);
      E.drawCrossSection(box, rows, ['temperature'], 'hybrid', { palettes: '/api/explorer/palettes.json' });
      await new Promise((r) => setTimeout(r, 500));
      const cells = box.querySelectorAll('.section-cell');
      cells[12].dispatchEvent(new MouseEvent('mouseenter'));
      const out = { points: E.pathPoints(rows).length, km: Math.round(E.pathPoints(rows).pop().km),
                    cells: cells.length, readout: box.querySelector('.sk-readout').textContent,
                    colours: box.querySelector('[aria-label="Colours of the cross section"]').value,
                    has: E.hasCrossSection(rows) };
      const check = box.querySelector('input[aria-label="Flight levels"]');
      check.click();
      out.flightLevels = [...box.querySelectorAll('.flight-level-label')].map((x) => x.textContent);
      out.fl100 = Math.round(E.flightLevelPressure(100) * 10) / 10;
      out.fl390 = Math.round(E.flightLevelPressure(390) * 10) / 10;
      check.click();
      out.after = box.querySelectorAll('.flight-level').length;
      box.remove();
      return out;
    })()`);
    check(
        'cross section along a path with the production colours',
        cross.points === 5 && cross.km > 40 && cross.cells === 50 && cross.has &&
            /^\d+ km \(/.test(cross.readout) && cross.colours === 'production:temperature',
        cross);
    check(
        'flight levels on the cross section',
        cross.flightLevels.join(',') === 'FL050,FL100,FL150,FL200,FL250,FL300,FL350' &&
            cross.fl100 === 696.8 && cross.fl390 === 196.8 && cross.after === 0,
        {flightLevels: cross.flightLevels, fl100: cross.fl100, fl390: cross.fl390, after: cross.after});

    // Locations reporting at different intervals, and a parameter without values
    const mixed = await b.evaluate(`(() => {
      const rows = [];
      for (let m = 0; m <= 120; m++) {
        const time = new Date(Date.UTC(2013, 7, 5, 0, m)).toISOString();
        rows.push({ name: 'every minute', time, a: 10 + m / 60, b: null });
        if (m % 10 === 0)
          for (let s = 0; s < 4; s++)
            rows.push({ name: 'station ' + s, time, a: 12 + s + m / 60, b: null });
      }
      const box = document.createElement('div');
      document.body.append(box);
      SmartMetExplorer.drawCharts(box, rows, ['a', 'b'], 'percentiles');
      const out = { charts: box.querySelectorAll('.chart').length, note: box.lastChild.textContent };
      box.remove();
      return out;
    })()`);
    check(
        'mixed intervals and a parameter without values',
        mixed.charts === 1 && mixed.note === 'No values in the result: b',
        mixed);

    // The pressure and height of model levels are requested with the parameters
    await b.send('Page.navigate', {url: `${base}/api`});
    await sleep(200);
    await b.send('Page.navigate', {
      url: `${
          base}/api/explorer#api=/timeseries&producer=hybrid&source=querydata&params=temperature&locmode=place&location=Helsinki`
    });
    await waitFor(b, `!!document.querySelector('[data-level="65"]')`, 'the model levels');
    const hq = await currentQuery(b);
    check(
        'pressure and height of model levels',
        hq.get('param') === 'name,time,lat,lon,level,temperature,pressure,geomheight',
        hq.get('param'));

    // A sounding diagram chosen before the request adds the parameters of a sounding
    const diagramNote = await b.evaluate(
        `document.querySelector('[data-diagram="tephigram"]') ? document.querySelector('[data-diagram="tephigram"]').parentElement.nextElementSibling.textContent : ''`);
    await b.evaluate(`document.querySelector('[data-diagram="tephigram"]').click()`);
    await sleep(300);
    const tq = await currentQuery(b);
    const dhash = new URLSearchParams((await b.evaluate('location.hash')).slice(1));
    check(
        'a tephigram chosen before the request',
        diagramNote.includes('dewpoint, windspeedms, winddirection') &&
            dhash.get('params') === 'temperature,dewpoint,windspeedms,winddirection' &&
            dhash.get('diagram') === 'tephigram' &&
            tq.get('param') ===
                'name,time,lat,lon,level,temperature,dewpoint,windspeedms,winddirection,pressure,geomheight',
        {note: diagramNote, params: dhash.get('params'), diagram: dhash.get('diagram'), param: tq.get('param')});

    // A path clicked on the map, vertex by vertex
    await b.send('Page.navigate', {url: `${base}/api`});
    await sleep(200);
    await b.send('Page.navigate', {
      url: `${base}/api/explorer#api=/timeseries&producer=pal&source=querydata&params=temperature&locmode=path`
    });
    await waitFor(b, `!!document.querySelector('.xmap-tools [data-tool="line"]')`, 'the path tool');
    let pm = await mapRect(b);
    for (const [fx, fy] of [[0.4, 0.5], [0.5, 0.45], [0.6, 0.5]])
    {
      await mouse(b, 'mouseMoved', pm.x + pm.w * fx, pm.y + pm.h * fy, 0);
      await mouse(b, 'mousePressed', pm.x + pm.w * fx, pm.y + pm.h * fy, 1);
      await mouse(b, 'mouseReleased', pm.x + pm.w * fx, pm.y + pm.h * fy, 0);
      await sleep(250);
    }
    let pq = await currentQuery(b);
    const vertices = await b.evaluate(`document.querySelectorAll('.xmap-centre').length`);
    check(
        'a path of three clicks',
        (pq.get('path') || '').split(',').length === 6 && Number(pq.get('step')) >= 1 &&
            vertices === 3,
        {path: pq.get('path'), step: pq.get('step'), vertices});
    await b.evaluate(`[...document.querySelectorAll('.xmap-tools button')].find((x) => x.textContent === 'Undo').click()`);
    await sleep(250);
    pq = await currentQuery(b);
    check('the last vertex undone', (pq.get('path') || '').split(',').length === 4, pq.get('path'));
    await b.evaluate(`[...document.querySelectorAll('.xmap-tools button')].find((x) => x.textContent === 'Clear').click()`);
    await sleep(250);
    const cleared = await b.evaluate(`document.querySelector('.missing').textContent`);
    check('the path cleared', cleared.includes('a location'), cleared);

    // The levels of a querydata producer are requested with the levels option
    await b.send('Page.navigate', {url: `${base}/api`});
    await sleep(200);
    await b.send('Page.navigate', {
      url: `${
          base}/api/explorer#api=/timeseries&producer=ecmwf&source=querydata&params=temperature&locmode=place&location=Helsinki&levels=850`
    });
    await waitFor(b, `!!document.querySelector('[data-level="850"]')`, 'the level buttons');
    let lq = await currentQuery(b);
    const levelButtons = await b.evaluate(`[...document.querySelectorAll('[data-level]')].map(
        (x) => x.dataset.level + (x.getAttribute('aria-pressed') === 'true' ? '*' : ''))`);
    check(
        'querydata levels',
        JSON.stringify(levelButtons) === JSON.stringify(['500', '850*', '1000']) &&
            lq.get('levels') === '850' && lq.get('param') === 'name,time,lat,lon,level,temperature',
        {levelButtons, query: lq.toString()});

    // A grid ensemble: the run, geometry, levels and members make the parameter keys
    await b.send('Page.navigate', {url: `${base}/api`});
    await sleep(200);
    await b.send('Page.navigate', {
      url: `${
          base}/api/explorer#api=/timeseries&producer=ens&source=grid&params=t-k&locmode=place&location=Helsinki`
    });
    await waitFor(b, `!!document.querySelector('[data-level="850"]')`, 'the grid details');
    const shown = await b.evaluate(`(() => {
      const box = document.querySelector('.model-details');
      const visible = (label) => { const x = box.querySelector('[aria-label="' + label + '"]');
        return !!x && x.offsetParent !== null; };
      return { run: visible('Model run'), geometry: visible('Geometry'), type: visible('Level type'),
               forecast: document.querySelector('[aria-label="Forecast"]').value,
               members: visible('Members'), text: box.innerText };
    })()`);
    check(
        'grid choices shown',
        shown.run && shown.geometry && shown.type && shown.members && shown.forecast === '3' &&
            shown.text.includes('5 members, a series each') &&
            shown.text.includes('Mean over time period (60 min)'),
        shown);
    lq = await currentQuery(b);
    check(
        'all members by default',
        lq.get('param') ===
            'name,time,lat,lon,level,' + [1, 2, 3, 4, 5].map((m) => `T-K:ENS::::3:${m}`).join(','),
        lq.get('param'));
    await b.evaluate(`document.querySelector('[data-level="850"]').click()`);
    await sleep(100);
    await b.evaluate(`document.querySelector('[data-level="500"]').click()`);
    await b.evaluate(`(() => {
      const input = document.querySelector('[aria-label="Members"]');
      input.value = "1-2";
      input.dispatchEvent(new Event("input", { bubbles: true }));
    })()`);
    await sleep(500);
    lq = await currentQuery(b);
    check(
        'keys of the levels and members',
        lq.get('param') ===
                'name,time,lat,lon,T-K:ENS::2:85000:3:1,T-K:ENS::2:85000:3:2,T-K:ENS::2:50000:3:1,' +
                    'T-K:ENS::2:50000:3:2' &&
            lq.get('levels') === null,
        lq.get('param'));
    await b.evaluate(`(() => {
      for (const [label, value] of [['Geometry', '2'], ['Model run', ${
        JSON.stringify(iso(now - 18 * hour))}]]) {
        const select = document.querySelector('[aria-label="' + label + '"]');
        select.value = value;
        select.dispatchEvent(new Event("change", { bubbles: true }));
      }
    })()`);
    await sleep(500);
    lq = await currentQuery(b);
    check(
        'geometry and run',
        lq.get('param').includes('T-K:ENS:2:2:85000:3:1') &&
            lq.get('origintime') === iso(now - 18 * hour) &&
            requested.some(
                (u) => u.startsWith('/timeseries/api/producers/ens?') && u.includes('origintime=')),
        {param: lq.get('param'), origintime: lq.get('origintime')});
    await b.evaluate(`document.querySelector('button.primary').click()`);
    await waitFor(
        b, `document.querySelectorAll('.chart canvas').length >= 1`, 'the ensemble chart');
    const ensembleSeries = await b.evaluate(
        `[...document.querySelectorAll('.chart[data-param="t-k"] .u-legend .u-series')].map((x) => x.textContent)`);
    check(
        'a series per level and member',
        ensembleSeries.some((x) => x.includes('Helsinki · 850 hPa · member 1')) &&
            ensembleSeries.some((x) => x.includes('Helsinki · 500 hPa · member 2')),
        ensembleSeries);
    await b.evaluate(`(() => {
      const select = document.querySelector('[aria-label="Level type"]');
      select.value = "6";
      select.dispatchEvent(new Event("change", { bubbles: true }));
    })()`);
    await sleep(300);
    const heights = await b.evaluate(
        `[...document.querySelectorAll('[data-level]')].map((x) => x.dataset.level)`);
    check('another level type', JSON.stringify(heights) === JSON.stringify(['2', '10']), heights);

    // The details of the grid data of each column, and other extra columns
    await b.evaluate(`(() => {
      document.querySelector('.extra-columns').open = true;
      document.querySelector('[data-detail="AT"]').click();
    })()`);
    await sleep(100);
    await b.evaluate(`document.querySelector('[data-detail="FN"]').click()`);
    await sleep(100);
    await b.evaluate(`document.querySelector('[data-meta="sunelevation"]').click()`);
    await sleep(300);
    const dq = await currentQuery(b);
    const dparams = dq.get('param').split(',');
    const keyIndexes =
        dparams.map((p, i) => (p.startsWith('T-K:ENS') ? i : -1)).filter((i) => i >= 0);
    check(
        'grid details of each data column',
        keyIndexes.length > 0 && dparams.includes('sunelevation') &&
            keyIndexes.every((i) => dparams.includes(`@AT-${i}`) && dparams.includes(`@FN-${i}`)) &&
            dparams.filter((p) => p.startsWith('@')).length === 2 * keyIndexes.length,
        dparams);
    const extraGroups = await b.evaluate(
        `[...document.querySelectorAll('.extra-group > .muted')].map((x) => x.textContent)`);
    check(
        'extra columns of a grid producer',
        JSON.stringify(extraGroups) === JSON.stringify([
          'The data',
          'The location',
          'The time and the sun',
          'Details of the grid data of each parameter'
        ]),
        extraGroups);
    const helpers = await b.evaluate(`(() => {
      const E = SmartMetExplorer;
      return { list: E.parseNumberList('3-1, 7', [0, 1, 2, 3, 4, 5, 6]), bad: E.parseNumberList('x', [1]),
               split: E.splitVariants([{ time: 't', 'a:1': 1, 'a:2': 2, name: 'n' }],
                 new Map([['a:1', { name: 'a', variant: 'one' }], ['a:2', { name: 'a', variant: 'two' }]])) };
    })()`);
    check(
        'member lists and variant rows',
        JSON.stringify(helpers.list) === '[1,2,3]' && helpers.bad === null &&
            JSON.stringify(helpers.split) === JSON.stringify([
              {time: 't', name: 'n', a: 1, _variant: 'one'},
              {time: 't', name: 'n', a: 2, _variant: 'two'}
            ]),
        helpers);

    // --- The EDR module -------------------------------------------------------
    await b.send('Page.navigate', {url: `${base}/api/explorer#api=/edr`});
    await sleep(300);
    await b.evaluate(`location.reload()`);
    await waitFor(b, `!!document.querySelector('[aria-label="Collection"]')`, 'the EDR module');
    const EPARAM = '[aria-label="Parameter"]';
    r = await choose(b, EPARAM, 'temp', 'Temperature');
    check('EDR parameter search', r.found && r.labels.some((l) => l.includes('2 collections')), r);
    r = await choose(b, EPARAM, 'relative', 'Humidity');
    let enote = await b.evaluate(`document.querySelector('.form .step .muted.small').textContent`);
    check(
        'collections with the parameters',
        enote === '1 of 2 collections have all these parameters',
        enote);
    r = await choose(b, '[aria-label="Collection"]', '', 'pal');
    check('only the collections with the parameters', r.found && r.labels.length === 1, r);
    await sleep(300);
    const queriesOf = await b.evaluate(
        `[...document.querySelectorAll('[data-query]')].map((x) => x.textContent + ':' + x.getAttribute('aria-checked'))`);
    check(
        'query types of the collection',
        JSON.stringify(queriesOf) ===
            JSON.stringify(
                ['Position:true', 'Radius:false', 'Area:false', 'Cube:false', 'Locations:false']),
        queriesOf);

    // A position clicked on the map
    m = await mapRect(b);
    await mouse(b, 'mouseMoved', m.x + m.w / 2, m.y + m.h / 2, 0);
    await mouse(b, 'mousePressed', m.x + m.w / 2, m.y + m.h / 2, 1);
    await mouse(b, 'mouseReleased', m.x + m.w / 2, m.y + m.h / 2, 0);
    await sleep(400);
    let eurl = new URL(await b.evaluate(`document.querySelector('code.url').textContent`));
    check(
        'EDR position request',
        eurl.pathname === '/edr/collections/pal/position' &&
            /^POINT\(-?[\d.]+ -?[\d.]+\)$/.test(eurl.searchParams.get('coords')) &&
            eurl.searchParams.get('parameter-name') === 'temperature,humidity' &&
            /Z\/.*Z$/.test(eurl.searchParams.get('datetime')) &&
            eurl.searchParams.get('f') === 'CoverageJSON',
        eurl.href);
    await b.evaluate(`document.querySelector('button.primary').click()`);
    await waitFor(b, `document.querySelectorAll('.chart canvas').length === 2`, 'the EDR charts');
    const echarts =
        await b.evaluate(`[...document.querySelectorAll('.chart')].map((c) => c.dataset.param)`);
    check(
        'EDR charts from CoverageJSON',
        JSON.stringify(echarts) === JSON.stringify(['humidity', 'temperature']) ||
            JSON.stringify(echarts) === JSON.stringify(['temperature', 'humidity']),
        echarts);

    // An area dragged on the map
    await b.evaluate(`document.querySelector('[data-query="area"]').click()`);
    await sleep(200);
    m = await mapRect(b);
    await drag(b, m.x + m.w / 2 - 50, m.y + m.h / 2 - 30, m.x + m.w / 2 + 50, m.y + m.h / 2 + 30);
    eurl = new URL(await b.evaluate(`document.querySelector('code.url').textContent`));
    check(
        'EDR area request',
        eurl.pathname === '/edr/collections/pal/area' &&
            /^POLYGON\(\(.*\)\)$/.test(eurl.searchParams.get('coords')),
        eurl.href);
    await b.evaluate(`document.querySelector('button.primary').click()`);
    await waitFor(
        b,
        `(document.querySelector('.results-head .muted') || {}).textContent === "24 rows, 4 locations"`,
        'the EDR area results');
    const earea = await b.evaluate(`document.querySelectorAll('.xmap-point').length`);
    check('the points of the area on the map', earea === 4, earea);

    // A location of the collection
    await b.evaluate(`document.querySelector('[data-query="locations"]').click()`);
    await sleep(500);
    r = await choose(b, '[aria-label="Location id"]', 'kumpula', 'Helsinki Kumpula');
    check('location search', r.found, r);
    await sleep(300);
    eurl = new URL(await b.evaluate(`document.querySelector('code.url').textContent`));
    check(
        'EDR locations request',
        eurl.pathname === '/edr/collections/pal/locations/101004' &&
            !eurl.searchParams.has('coords'),
        eurl.href);
    const elocs = await b.evaluate(`document.querySelectorAll('.xmap-point').length`);
    check('the locations of the collection on the map', elocs === 2, elocs);
    // The data at a location of the collection is named by it
    await b.evaluate(`document.querySelector('button.primary').click()`);
    await waitFor(b, `document.querySelectorAll('.chart').length > 0`, 'the EDR location results');
    const named = await b.evaluate(`(() => {
      [...document.querySelectorAll('.results-head .tabs button')].find((x) => x.textContent === "Table").click();
      return [...document.querySelectorAll('table.data tbody tr:first-child td')].map((td) => td.textContent);
    })()`);
    check('rows named by the location', named.includes('Helsinki Kaisaniemi'), named);

    // The levels of a collection
    await b.evaluate(
        `[...document.querySelectorAll('.model-card button')].find((x) => x.textContent === "Change").click()`);
    await sleep(200);
    await b.evaluate(
        `[...document.querySelectorAll('.chip-x')].find((x) => x.getAttribute('aria-label') === 'Remove Humidity').click()`);
    await sleep(200);
    r = await choose(b, '[aria-label="Collection"]', 'ecmwf', 'ecmwf_pressure');
    await sleep(300);
    await b.evaluate(
        `(() => { const chips = [...document.querySelectorAll('.step button.chip')]; chips.find((x) => x.textContent === '850').click(); })()`);
    await sleep(200);
    await b.evaluate(
        `(() => { const chips = [...document.querySelectorAll('.step button.chip')]; chips.find((x) => x.textContent === '500').click(); })()`);
    await sleep(200);
    m = await mapRect(b);
    await mouse(b, 'mouseMoved', m.x + m.w / 2, m.y + m.h / 2, 0);
    await mouse(b, 'mousePressed', m.x + m.w / 2, m.y + m.h / 2, 1);
    await mouse(b, 'mouseReleased', m.x + m.w / 2, m.y + m.h / 2, 0);
    await sleep(400);
    eurl = new URL(await b.evaluate(`document.querySelector('code.url').textContent`));
    check(
        'EDR levels request',
        eurl.pathname === '/edr/collections/ecmwf_pressure/position' &&
            eurl.searchParams.get('z') === '850,500',
        eurl.href);
    await b.evaluate(`document.querySelector('button.primary').click()`);
    await waitFor(b, `document.querySelectorAll('.chart canvas').length === 1`, 'the level chart');
    const levelTitle = await b.evaluate(`document.querySelector('.chart-title').textContent`);
    check('a series per level', levelTitle === 'temperature, 2 locations', levelTitle);

    // --- The Coverages module --------------------------------------------------
    await b.send('Page.navigate', {url: `${base}/api/explorer#api=/coverages`});
    await sleep(300);
    await b.evaluate(`location.reload()`);
    await waitFor(
        b, `!!document.querySelector('[aria-label="Collection"]')`, 'the Coverages module');
    await waitFor(
        b,
        `!document.querySelector('.form .step .muted.small').textContent.includes('reading')`,
        'the schemas');
    r = await choose(b, '[aria-label="Parameter"]', 'temp', 'Temperature');
    check('Coverages parameter from the schemas', r.found, r);
    r = await choose(b, '[aria-label="Collection"]', '', 'pal');
    check(
        'Coverages collection with the native projection',
        r.found && r.labels[0].includes('FMI_Polar_Stereographic'),
        r);
    await waitFor(b, `document.querySelectorAll('.xmap-region').length === 1`, 'the native grid');
    await sleep(500);
    const ccard = await b.evaluate(`document.querySelector('.model-card').textContent`);
    check(
        'native grid in the card',
        ccard.includes('135 × 150 grid') && ccard.includes('15.0 km'),
        ccard);
    let curl = new URL(await b.evaluate(`document.querySelector('code.url').textContent`));
    check(
        'coverage request',
        curl.pathname === '/coverages/collections/pal/coverage' &&
            curl.searchParams.get('properties') === 'Temperature' &&
            curl.searchParams.get('f') === 'netcdf' &&
            /Z\/.*Z$/.test(curl.searchParams.get('datetime')),
        curl.href);
    const estimateText = await b.evaluate(`document.querySelector('.size-estimate').textContent`);
    check(
        'size estimate',
        estimateText.includes('40 × 82 grid') && estimateText.includes('about 328.0 kB'),
        estimateText);
    const href = await b.evaluate(`document.querySelector('a.download').getAttribute('href')`);
    check('download link', href === curl.href, href);
    const curlSnippet = await b.evaluate(`document.querySelector('pre.code').textContent`);
    check(
        'snippet file name',
        curlSnippet.startsWith('curl --fail --output result.nc '),
        curlSnippet);

    // A projection and a box: the area of the download is drawn
    await b.evaluate(
        `(() => { const s = document.querySelector('[aria-label="Output projection"]'); s.value = 'EPSG:3067'; s.dispatchEvent(new Event('change', { bubbles: true })); })()`);
    await sleep(300);
    m = await mapRect(b);
    await drag(b, m.x + m.w / 2 - 40, m.y + m.h / 2 - 40, m.x + m.w / 2 + 40, m.y + m.h / 2 + 40);
    await sleep(800);
    curl = new URL(await b.evaluate(`document.querySelector('code.url').textContent`));
    check(
        'projected coverage request',
        curl.searchParams.get('crs') === 'EPSG:3067' &&
            curl.searchParams.get('bbox').split(',').length === 4,
        curl.href);
    const outlines = await b.evaluate(`document.querySelectorAll('.xmap-outline').length`);
    check('area of the download on the map', outlines === 1, outlines);
    const outlineRequests = requested.filter(
        (u) => u.startsWith('/coverages/collections/pal/outline?') && u.includes('crs=EPSG') &&
            u.includes('bbox='));
    check(
        'outline asked with the options',
        outlineRequests.length >= 1,
        requested.filter((u) => u.includes('outline')));

    // Narrow screens
    await b.send(
        'Emulation.setDeviceMetricsOverride',
        {width: 390, height: 844, deviceScaleFactor: 2, mobile: true});
    await sleep(300);
    const overflow =
        await b.evaluate(`document.documentElement.scrollWidth <= window.innerWidth + 1`);
    check('no horizontal scrolling on a phone', overflow, overflow);

    if (process.env.EXPLORER_SCREENSHOT)
    {
      await b.send('Emulation.clearDeviceMetricsOverride');
      await sleep(300);
      const shot = await b.send('Page.captureScreenshot', {format: 'png'});
      fs.writeFileSync(process.env.EXPLORER_SCREENSHOT, Buffer.from(shot.data, 'base64'));
    }

    check('no page errors', b.errors.length === 0, b.errors);
  }
  finally
  {
    await stopBrowser(b);
    server.close();
  }
  console.log(`${t.checks} explorer checks passed, ${t.failures} failed`);
  process.exit(t.failures ? 1 : 0);
}

main().catch((e) => {
  console.log('FAIL ' + e.message);
  process.exit(1);
});
