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
      data_queries: {position: {}, radius: {}, area: {}, cube: {}, trajectory: {}, locations: {}},
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
      /^\/edr\/collections\/([^/]+)\/(position|radius|area|cube|trajectory|locations)(?:\/([^/]+))?$/.exec(p);
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
  // A trajectory: its points (the vertices of the LINESTRING) at each time on a composite axis
  if (m[2] === 'trajectory')
  {
    const points = (/LINESTRING\((.*)\)/.exec(query.get('coords') || '') || ['', ''])[1]
                       .split(',')
                       .map((xy) => xy.trim().split(/\s+/).map(Number));
    const ts = Array.from({length: 3}, (_, i) => iso(now.getTime() + i * hour));
    const tuples = points.flatMap(([x, y]) => ts.map((tm) => [tm, x, y]));
    const ranges = {};
    for (const p of params)
      ranges[p] = {
        type: 'NdArray',
        dataType: 'float',
        axisNames: ['composite'],
        shape: [tuples.length],
        values: tuples.map((tu, i) => 10 + i)
      };
    return {
      type: 'Coverage',
      domain: {type: 'Domain', domainType: 'Trajectory',
               axes: {composite: {dataType: 'tuple', coordinates: ['t', 'x', 'y'], values: tuples}}},
      ranges
    };
  }
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
    outputFormats: ['application/x-grib2', 'application/netcdf', 'image/tiff'],
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
// The API keys of the requests
const apikeys = [];
const server = http.createServer((req, res) => {
  requested.push(req.url);
  if (req.headers['fmi-apikey'])
    apikeys.push([req.url, req.headers['fmi-apikey']]);
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
      ],
      // The sensors of the stations asked for, as the observation engine lists them
      sensors: url.searchParams.get('fmisid') ? {
        available: true,
        note: 'From the observation metadata',
        list: [
          {fmisid: 101004, measurand_id: 1, code: 'TA', parameters: ['temperature'], sensor: 1,
           default: true, configuration_start: '2006-10-11T00:00:00Z', last: '2099-01-01T00:00:00Z',
           values: 100},
          {fmisid: 101004, measurand_id: 1, code: 'TA', parameters: ['temperature'], sensor: 11,
           default: false, configuration_start: '2010-01-01T00:00:00Z', last: '2099-01-01T00:00:00Z',
           values: 50},
          {fmisid: 101004, measurand_id: 1, code: 'TA', parameters: ['temperature'], sensor: 23,
           default: false, configuration_start: '2008-01-01T00:00:00Z', last: '2009-01-01T00:00:00Z',
           values: 10}
        ]
      } : undefined
    }));
  if (p === '/timeseries/api/producers/ens')
    return send(200, 'application/json', JSON.stringify(ensDetails(url.searchParams)));
  if (p === '/timeseries')
    return send(200, 'application/json', JSON.stringify(timeseries(url.searchParams)));
  // The maps of a coverage: a 12 x 10 GeoTIFF of three times of Temperature and Pressure
  if (p === '/coverages/collections/pal/coverage' && url.searchParams.get('f') === 'geotiff')
    return send(200, 'image/tiff', fs.readFileSync(path.join(__dirname, 'explorer-coverage.tif')));
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
               canvases: document.querySelectorAll('.chart canvas').length,
               // The lines are drawn on the canvas: no cursor point of each
               cursorPoints: document.querySelectorAll('.chart .u-cursor-pt').length };
    })()`);
    check(
        'swarm of all locations',
        swarm.mode === 'lines' &&
            swarm.title ===
                'temperature: 30 locations, coloured from south to north, and the median' &&
            swarm.legends === 0 && swarm.canvases === 2 && swarm.cursorPoints <= 2,
        swarm);
    // The line under the pointer is highlighted with its name and value
    const swarmHover = await b.evaluate(`(async () => {
      const over = document.querySelector('.chart .u-over');
      const r = over.getBoundingClientRect();
      const label = document.querySelector('.chart .swarm-label');
      // The pointer on the lines at the middle time, then scanned upwards until a line is near
      for (let f = 0.95; f > 0.05; f -= 0.02)
      {
        over.dispatchEvent(new MouseEvent('mousemove', { clientX: r.left + r.width / 2, clientY: r.top + r.height * f, bubbles: true }));
        await new Promise((r) => requestAnimationFrame(r));
        if (!label.hidden)
          break;
      }
      return { label: label.hidden ? '' : label.textContent,
               path: (document.querySelector('.chart .swarm-line').getAttribute('d') || '').length };
    })()`);
    check('the swarm line under the pointer', /: -?[0-9.]+$/.test(swarmHover.label) && swarmHover.path > 10, swarmHover);
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
      slider.dispatchEvent(new KeyboardEvent("keydown", { key: "End", bubbles: true }));
      await new Promise((r) => setTimeout(r, 200));
      const last = colours();
      const legendBefore = pane.querySelector('.legend-step').style.background;
      const palette = pane.querySelector('[aria-label="Colours"]');
      palette.value = "temperature";
      palette.dispatchEvent(new Event("change", { bubbles: true }));
      const legendAfter = pane.querySelector('.legend-step').style.background;
      // The animation at the highest speed, from the last time round to the first ones
      const label = () => pane.querySelector('.time-label').textContent;
      const atEnd = label();
      const speed = pane.querySelector('[aria-label="Animation speed"]');
      speed.value = "175";
      speed.dispatchEvent(new Event("change", { bubbles: true }));
      pane.querySelector('.time-play').click();
      const playing = pane.querySelector('.time-play').getAttribute('aria-label');
      await new Promise((r) => setTimeout(r, 450));
      pane.querySelector('.time-play').click();
      const animated = label();
      const paused = pane.querySelector('.time-play').getAttribute('aria-label');
      await new Promise((r) => setTimeout(r, 400));
      const still = label() === animated;
      // A step back with the arrow key, to the previous available time
      slider.dispatchEvent(new KeyboardEvent("keydown", { key: "ArrowLeft", bubbles: true }));
      const back = label();
      return { dots: pane.querySelectorAll('.xmap-dot').length, ticks: pane.querySelectorAll('.time-tick').length,
               changed: first !== last, legend: legendBefore !== legendAfter,
               min: pane.querySelector('[aria-label="Minimum"]').value, time: atEnd,
               animated: animated !== atEnd, playing, paused, still, back: back !== animated,
               remembered: localStorage.getItem('smartmet-explorer-animation-speed') };
    })()`);
    check(
        'values on a map',
        valueMap.dots === 4 && valueMap.ticks === 6 && valueMap.changed && valueMap.legend &&
            valueMap.min !== '' && valueMap.time.endsWith('UTC'),
        valueMap);
    check(
        'animation of the map times',
        valueMap.playing === 'Pause' && valueMap.paused === 'Play' && valueMap.animated &&
            valueMap.still && valueMap.back && valueMap.remembered === '175',
        valueMap);

    // One time back or forward with the buttons, and the time series of a point clicked
    const probe = await b.evaluate(`(async () => {
      const pane = [...document.querySelectorAll('.results .pane')].find((p) => !p.hidden);
      const label = () => pane.querySelector('.time-label').textContent;
      const slider = pane.querySelector('input[type="range"]');
      slider.dispatchEvent(new KeyboardEvent("keydown", { key: "Home", bubbles: true }));
      const first = label();
      const back = pane.querySelector('[aria-label="Previous time"]');
      const next = pane.querySelector('[aria-label="Next time"]');
      const backAtStart = back.disabled;
      next.click();
      const second = label();
      back.click();
      const again = label();
      pane.querySelector('.xmap-dot').dispatchEvent(new MouseEvent("click", { bubbles: true }));
      await new Promise((r) => setTimeout(r, 200));
      const box = pane.querySelector('.map-probe');
      const title1 = box.querySelector('.chart-title').textContent;
      next.click();
      const title2 = box.querySelector('.chart-title').textContent;
      const canvas = box.querySelectorAll('canvas').length;
      const cursor = pane.querySelectorAll('.xmap-cursor').length;
      box.querySelector('[aria-label="Close the series"]').click();
      return { backAtStart, stepped: second !== first, returned: again === first, title1, title2, canvas, cursor,
               closed: box.hidden && pane.querySelectorAll('.xmap-cursor').length === 0 };
    })()`);
    check(
        'a time back and forward, the series of a point clicked',
        probe.backAtStart && probe.stepped && probe.returned && probe.canvas >= 1 &&
            probe.cursor === 1 && probe.closed && /^\S+ at .*: /.test(probe.title1) &&
            probe.title1 !== probe.title2,
        probe);

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
    // The sensors of the station, listed by the engine, toggle the sensor numbers
    await waitFor(b, `document.querySelectorAll('.sensors-line button[data-sensor]').length === 3`,
                  'the sensors of the station');
    const sensorButtons = await b.evaluate(`[...document.querySelectorAll('.sensors-line button[data-sensor]')]
      .map((x) => [x.textContent, x.getAttribute('aria-pressed')])`);
    check(
        'the sensors of the station',
        JSON.stringify(sensorButtons) ===
            '[["1 (default)","true"],["11","true"],["23 · until 2009-01-01","false"]]',
        sensorButtons);
    await b.evaluate(`document.querySelector('.sensors-line button[data-sensor="23"]').click()`);
    await sleep(300);
    const toggledOn = await b.evaluate(`document.querySelector('[aria-label="Sensors"]').value`);
    await b.evaluate(`document.querySelector('.sensors-line button[data-sensor="23"]').click()`);
    await sleep(300);
    const toggledOff = await b.evaluate(`document.querySelector('[aria-label="Sensors"]').value`);
    check('a sensor toggled', toggledOn === '1,11,23' && toggledOff === '1,11',
          {toggledOn, toggledOff});
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
      // The values of the levels, and a second panel side by side with the next time
      const valueRows = box.querySelectorAll('.sk-values tbody tr');
      valueRows[3].dispatchEvent(new MouseEvent('mouseenter'));
      skewt.values = valueRows.length;
      skewt.marked = box.querySelector('.sk-mark.temperature').getAttribute('visibility');
      skewt.head = [...box.querySelectorAll('.sk-values th')].map((th) => th.textContent).join('|');
      [...box.querySelectorAll('button')].find((x) => x.textContent === 'Add a panel').click();
      skewt.panels = box.querySelectorAll('.sk-panel').length;
      skewt.diagrams = box.querySelectorAll('.skewt-diagram').length;
      skewt.times = [...box.querySelectorAll('.sk-panel .time-label')].map((x) => x.textContent);
      box.querySelectorAll('.sk-panel')[1].querySelector('[aria-label="Remove the panel"]').click();
      skewt.removed = box.querySelectorAll('.sk-panel').length;
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

    // The level kinds of EDR collections from their vertical reference systems: hybrid
    // sigma-pressure model levels are model levels, not pressures
    const kinds = await b.evaluate(`(() => {
      const E = SmartMetExplorer;
      const kind = (vrs) => E.edrLevelKind({ levels: [{ type: vrs }] });
      const box = document.createElement('div');
      document.body.append(box);
      // A sounding whose pressures are all above the top of the diagram
      const rows = [1, 2, 3, 4].map((k) => ({ name: 'x', time: '2026-10-15T00:00:00Z', lat: 60, lon: 25,
                                              level: k, pressure: k * 10, temperature: -50 }));
      E.drawSkewT(box, rows, 'hybrid');
      const message = box.textContent.includes('no pressures below 100 hPa');
      box.remove();
      return [
        kind('PARAMETRICCRS["Model level",PDATUM["Hybrid sigma-pressure model levels"],CS[parametric,1],AXIS["model level number (n)",down],PARAMETRICUNIT["unity",1.0]]'),
        kind('VERTCRS["Pressure",VDATUM["Pressure"],CS[vertical,1],AXIS["pressure (p)",down],UNIT["hectopascal",100]]'),
        kind('VERTCRS["Pressure",VDATUM["Pressure"],CS[vertical,1],AXIS["pressure (p)",down],UNIT["pascal",1]]'),
        kind('VERTCRS["Depth",VDATUM["Sea level"],CS[vertical,1],AXIS["depth (D)",down],UNIT["metre",1]]'),
        message
      ];
    })()`);
    check(
        'level kinds of EDR collections, and a sounding above the diagram',
        JSON.stringify(kinds) === '["hybrid","pressure","pressure-pa","depth",true]', kinds);
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
            vertical.skewt.info.startsWith('Lifting condensation level') &&
            vertical.skewt.values >= 10 && vertical.skewt.marked === 'visible' &&
            vertical.skewt.head.startsWith('hPa|Height') && vertical.skewt.panels === 2 &&
            vertical.skewt.diagrams === 2 && vertical.skewt.times.length === 2 &&
            vertical.skewt.times[0] !== vertical.skewt.times[1] && vertical.skewt.removed === 1,
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
      // Isotherms over the cross section (on by default), and the overlays the rows allow
      out.overlays = [...box.querySelectorAll('.overlay-checks label')].map((l) => l.textContent);
      out.zero = box.querySelectorAll('.xs-isotherm.zero').length;
      out.isotherms = [...box.querySelectorAll('.xs-isotherm-label')].map((l) => l.textContent).join(' ');
      // A contour of 0.5 through a square of 0 and 1 corners: one segment through the middle
      out.contour = E.contourSegments([[{ x: 0, y: 0, v: 0 }, { x: 0, y: 10, v: 0 }], [{ x: 10, y: 0, v: 1 }, { x: 10, y: 10, v: 1 }]], 0.5);
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
        'isotherms on the cross section',
        JSON.stringify(cross.overlays) === '["Isotherms"]' && cross.zero === 1 &&
            cross.isotherms.includes('0°') && JSON.stringify(cross.contour) === '[[5,0,5,10]]',
        {overlays: cross.overlays, zero: cross.zero, isotherms: cross.isotherms, contour: cross.contour});
    check(
        'flight levels on the cross section',
        cross.flightLevels.join(',') === 'FL050,FL100,FL150,FL200,FL250,FL300,FL350' &&
            cross.fl100 === 696.8 && cross.fl390 === 196.8 && cross.after === 0,
        {flightLevels: cross.flightLevels, fl100: cross.fl100, fl390: cross.fl390, after: cross.after});

    // The readout of a skew-T under a resting pointer follows the time steps
    await b.evaluate(`(() => {
      const rows = [];
      for (const [time, dt] of [['2026-10-15T00:00:00Z', 0], ['2026-10-15T03:00:00Z', 4]])
        for (let k = 0; k < 12; k++)
        {
          const p = 1000 - k * 75;
          rows.push({ name: 'x', time, lat: 60, lon: 25, level: k + 1, pressure: p,
                      temperature: 15 + dt - k * 6, dewpoint: 10 + dt - k * 7 });
        }
      const box = document.createElement('div');
      box.id = 'hover-skewt';
      box.style.width = '600px';
      document.body.prepend(box);
      SmartMetExplorer.drawSkewT(box, rows, 'hybrid');
      box.scrollIntoView();
    })()`);
    const hr = await b.evaluate(`(() => {
      const r = document.querySelector('#hover-skewt .vhover').getBoundingClientRect();
      return { x: r.left + r.width * 0.3, y: r.top + r.height * 0.85 };
    })()`);
    await mouse(b, 'mouseMoved', hr.x, hr.y, 0);
    await sleep(100);
    const hover = await b.evaluate(`(() => {
      const box = document.querySelector('#hover-skewt');
      const readout = () => box.querySelector('.sk-readout').textContent;
      const before = readout();
      box.querySelector('input[type="range"]').dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowRight', bubbles: true }));
      const after = readout();
      const guide = box.querySelector('.sk-guide').getAttribute('visibility');
      box.remove();
      return { before, after, guide };
    })()`);
    check(
        'the skew-T readout under the pointer follows the animation',
        /hPa: T /.test(hover.before) && /hPa: T /.test(hover.after) && hover.before !== hover.after &&
            hover.guide === 'visible',
        hover);
    await mouse(b, 'mouseMoved', 1, 1, 0);

    // A flight: the values at the times the points are passed, interpolated in time
    const flight = await b.evaluate(`(() => {
      const E = SmartMetExplorer;
      const rows = [];
      for (const [lon, km] of [[25, 0], [26, 1]])
        for (const [h, v, d] of [[0, 0, 350], [3, 30, 10]])
          rows.push({ name: 'p', time: new Date(Date.UTC(2026, 9, 15, h)).toISOString(), lat: 60, lon, level: 1,
                      temperature: v + lon, winddirection: d });
      // Departure at 00:30 from the first point, arrival at 02:30 at the second
      const f = { departure: Date.UTC(2026, 9, 15, 0, 30), arrival: Date.UTC(2026, 9, 15, 2, 30) };
      const out = E.flightRows(rows, f).sort((a, b) => a.lon - b.lon);
      return out.map((r) => [r.lon, r.time.slice(11, 16), Math.round(r.temperature * 10) / 10, Math.round(r.winddirection) % 360]);
    })()`);
    check(
        'the values met on a flight',
        JSON.stringify(flight) === JSON.stringify([[25, '00:30', 30, 353], [26, '02:30', 51, 7]]), flight);

    // Charts of several levels: the lowest level for an area, every level for one location
    const chartLevels = await b.evaluate(`(async () => {
      const E = SmartMetExplorer;
      const rows = [];
      for (let i = 0; i < 25; i++)
        for (let k = 1; k <= 3; k++)
          for (let h = 0; h < 4; h++)
            rows.push({ name: null, time: new Date(Date.UTC(2026, 9, 15, h)).toISOString(), lat: 60 + i * 0.01,
                        lon: 25, level: k, temperature: 10 - k * 20 + h });
      const box = document.createElement('div');
      box.style.width = '700px';
      document.body.append(box);
      E.drawCharts(box, rows, ['temperature'], undefined, { levelKind: 'hybrid' });
      await new Promise((r) => setTimeout(r, 300));
      const select = box.querySelector('[aria-label="Level of the charts"]');
      const area = { level: select.value, title: box.querySelector('.chart-title').textContent };
      select.value = '';
      select.dispatchEvent(new Event('change'));
      await new Promise((r) => setTimeout(r, 300));
      const all = box.querySelector('.chart-title').textContent;
      E.drawCharts(box, rows.filter((r) => r.lat === 60), ['temperature'], undefined, { levelKind: 'hybrid' });
      const single = box.querySelector('[aria-label="Level of the charts"]').value;
      E.drawCharts(box, rows, ['temperature'], undefined, { levelKind: 'height' });
      const height = box.querySelector('[aria-label="Level of the charts"]').value;
      box.remove();
      return { area, all, single, height };
    })()`);
    check(
        'charts of several levels',
        chartLevels.area.level === '3' && chartLevels.area.title.includes('of 25 locations') &&
            chartLevels.all.includes('of 75 locations') && chartLevels.single === '' &&
            chartLevels.height === '1',
        chartLevels);

    // An ensemble with its members stored as levels: the members are series, their spread the
    // percentiles, and the share of them above a threshold the probability
    const ensemble = await b.evaluate(`(async () => {
      const E = SmartMetExplorer;
      const rows = [];
      for (let m = 1; m <= 30; m++)
        for (let h = 0; h < 4; h++)
          rows.push({ name: 'Helsinki', time: new Date(Date.UTC(2014, 1, 20, 6 * h)).toISOString(), lat: 60.17, lon: 24.94,
                      level: m, temperature: m <= 10 ? -2 + h : 3 + h });
      const box = document.createElement('div');
      box.style.width = '700px';
      document.body.append(box);
      E.drawCharts(box, rows, ['temperature'], undefined, { levelKind: 'member' });
      const spread = box.querySelector('.chart-title').textContent;
      const members = box.querySelector('[aria-label="Level of the charts"]');
      const menu = [members.closest('label').firstChild.textContent, members.value, members.options[1].textContent];
      box.querySelector('[data-mode="probability"]').click();
      await new Promise((r) => setTimeout(r, 200));
      const threshold = box.querySelector('[aria-label="Threshold of temperature"]');
      threshold.value = '0';
      threshold.dispatchEvent(new Event('change'));
      await new Promise((r) => setTimeout(r, 200));
      const probability = box.querySelector('.chart-title').textContent;
      box.remove();
      const kind = E.edrLevelKind({ levels: [{ type: 'PARAMETRICCRS["Ensemble member",PDATUM["Members of an ensemble forecast"],CS[parametric,1],AXIS["ensemble member (n)",up],PARAMETRICUNIT["unity",1.0]]' }] });
      return { spread, menu, probability, kind, profiles: E.hasProfiles(rows, 'member') };
    })()`);
    check(
        'an ensemble of members',
        ensemble.spread === 'temperature: median and percentiles of 30 members' &&
            JSON.stringify(ensemble.menu) === JSON.stringify(['Member', '', 'Member 1']) &&
            ensemble.probability === 'temperature: the share of the 30 members above 0' &&
            ensemble.kind === 'member' && ensemble.profiles === false,
        ensemble);

    // The analysis of an unstable sounding, compared with MetPy 1.7 (CAPE 7446 J/kg without the
    // virtual temperature correction, LFC 963 hPa, EL 148 hPa, LI -14.8, K 14.3, TT 43.7,
    // PW 34.9 mm, shear 15.1 m/s)
    const analysis = await b.evaluate(`(() => {
      const a = SmartMetExplorer.soundingAnalysis([{"p":1000,"t":30.0,"td":28.0,"speed":5.0,"direction":200.0,"height":0.0},{"p":975,"t":28.2,"td":26.2,"speed":5.449298962539739,"direction":202.2464948126987,"height":224.6},{"p":950,"t":26.37,"td":24.37,"speed":5.9075322409866695,"direction":204.53766120493336,"height":453.8},{"p":925,"t":24.5,"td":22.5,"speed":6.375129588295014,"direction":206.87564794147508,"height":687.6},{"p":900,"t":22.59,"td":20.59,"speed":6.85254004989855,"direction":209.26270024949275,"height":926.3},{"p":875,"t":20.64,"td":9.39,"speed":7.340250629850253,"direction":211.70125314925127,"height":1170.1},{"p":850,"t":18.64,"td":6.14,"speed":7.838790693456529,"direction":214.19395346728265,"height":1419.4},{"p":825,"t":16.98,"td":3.23,"speed":8.348719551435869,"direction":216.74359775717934,"height":1674.4},{"p":800,"t":15.28,"td":0.28,"speed":8.87135069513365,"direction":219.35675347566826,"height":1935.7},{"p":775,"t":13.54,"td":-2.71,"speed":9.407416488412935,"direction":222.03708244206467,"height":2203.7},{"p":750,"t":11.75,"td":-5.75,"speed":9.957721358300732,"direction":224.78860679150367,"height":2478.9},{"p":725,"t":9.92,"td":-8.83,"speed":10.52313172511553,"direction":227.61565862557765,"height":2761.6},{"p":700,"t":8.03,"td":-11.97,"speed":11.104625793528728,"direction":230.52312896764363,"height":3052.3},{"p":675,"t":6.08,"td":-15.17,"speed":11.703245951506004,"direction":233.51622975753003,"height":3351.6},{"p":650,"t":4.08,"td":-18.42,"speed":12.32015265275301,"direction":236.60076326376503,"height":3660.1},{"p":625,"t":2.01,"td":-21.74,"speed":12.95666590867226,"direction":239.7833295433613,"height":3978.3},{"p":600,"t":-0.13,"td":-25.13,"speed":13.614219663868987,"direction":243.07109831934494,"height":4307.1},{"p":575,"t":-2.34,"td":-28.59,"speed":14.294431036060509,"direction":246.47215518030254,"height":4647.2},{"p":550,"t":-4.63,"td":-32.13,"speed":14.999132907164633,"direction":249.99566453582315,"height":4999.6},{"p":525,"t":-7.01,"td":-35.76,"speed":15.730386868892786,"direction":253.65193434446394,"height":5365.2},{"p":500,"t":-9.48,"td":-39.48,"speed":16.490527896132626,"direction":257.45263948066315,"height":5745.3},{"p":475,"t":-12.05,"td":-43.3,"speed":17.28225051728495,"direction":261.4112525864248,"height":6141.1},{"p":450,"t":-14.74,"td":-47.24,"speed":18.108654998122333,"direction":265.5432749906117,"height":6554.3},{"p":425,"t":-17.55,"td":-51.3,"speed":18.97330675055119,"direction":269.8665337527559,"height":6986.7},{"p":400,"t":-20.5,"td":-55.5,"speed":19.88042025692615,"direction":274.40210128463076,"height":7440.2},{"p":375,"t":-23.6,"td":-59.85,"speed":20.834951292532914,"direction":279.17475646266456,"height":7917.5},{"p":350,"t":-26.87,"td":-64.37,"speed":21.842842220970127,"direction":284.2142111048506,"height":8421.4},{"p":325,"t":-30.35,"td":-69.1,"speed":22.911271586572983,"direction":289.5563579328649,"height":8955.6},{"p":300,"t":-34.04,"td":-74.04,"speed":24.048957014158667,"direction":295.24478507079334,"height":9524.5},{"p":275,"t":-38.0,"td":-79.25,"speed":25.266895972444182,"direction":301.3344798622209,"height":10133.4},{"p":250,"t":-42.27,"td":-84.77,"speed":26.57890255322243,"direction":307.89451276611214,"height":10789.5},{"p":225,"t":-46.89,"td":-90.64,"speed":28.002922073821587,"direction":315.0146103691079,"height":11501.5},{"p":200,"t":-51.96,"td":-96.96,"speed":29.562985543497966,"direction":322.8149277174898,"height":12281.5},{"p":175,"t":-55,"td":-101.25,"speed":31.292007898570247,"direction":331.46003949285125,"height":13146.0},{"p":150,"t":-55,"td":-102.5,"speed":33.2605848824885,"direction":341.3029244124425,"height":14130.3},{"p":125,"t":-55,"td":-103.75,"speed":35.588917269857305,"direction":352.9445863492865,"height":15294.5},{"p":100,"t":-55,"td":-105.0,"speed":38.43856580672519,"direction":7.192829033625912,"height":16719.3}]);
      return a;
    })()`);
    const near = (v, ref, tol) => v !== null && Math.abs(v - ref) <= tol;
    check(
        'the analysis of a sounding',
        near(analysis.cape, 7446, 200) && near(analysis.cin, 0, 5) && near(analysis.lfc, 963, 15) &&
            near(analysis.el, 148, 5) && near(analysis.li, -14.8, 0.5) && near(analysis.k, 14.3, 0.3) &&
            near(analysis.tt, 43.7, 0.3) && near(analysis.pw, 34.9, 1) && near(analysis.shear, 15.1, 1) &&
            near(analysis.freezing.p, 601, 5),
        analysis);

    // A meteogram of an ensemble: the panels of the groups the rows have, with wind barbs
    const meteogram = await b.evaluate(`(async () => {
      const E = SmartMetExplorer;
      const rows = [];
      for (let m = 1; m <= 5; m++)
        for (let h = 0; h < 8; h++)
          rows.push({ name: 'Helsinki', time: new Date(Date.UTC(2014, 1, 20, 6 * h)).toISOString(), lat: 60.17, lon: 24.94,
                      level: m, temperature: h - m, precipitationamount: m === 1 ? 2 : 0, totalcloudcover: 50 + m,
                      windums: 3 + m, windvms: -2 });
      const box = document.createElement('div');
      box.style.width = '800px';
      document.body.append(box);
      E.drawMeteogram(box, rows, 'member');
      await new Promise((r) => setTimeout(r, 300));
      const out = { titles: [...box.querySelectorAll('.chart-title')].map((x) => x.textContent.split(' (')[0]),
                    barbs: box.querySelectorAll('.meteogram-barbs .barb').length, has: E.hasMeteogram(rows),
                    adds: E.meteogramParameters(new Set(['temperature', 'precipitationamount', 'windums', 'windvms']), ['temperature']) };
      box.remove();
      return out;
    })()`);
    check(
        'a meteogram of an ensemble',
        JSON.stringify(meteogram.titles) === '["Temperature","Precipitation","Cloud cover","Wind"]' &&
            meteogram.barbs > 0 && meteogram.has &&
            JSON.stringify(meteogram.adds) === '["precipitationamount","windums","windvms"]',
        meteogram);

    // Export: the view as an image, the rows as CSV
    const exported = await b.evaluate(`(async () => {
      const E = SmartMetExplorer;
      const rows = [];
      for (let k = 0; k < 12; k++)
        rows.push({ name: 'x', time: '2026-10-15T00:00:00Z', lat: 60, lon: 25, level: k + 1, pressure: 1000 - k * 75,
                    temperature: 15 - k * 6, dewpoint: 10 - k * 7 });
      const box = document.createElement('div');
      box.style.width = '600px';
      document.body.append(box);
      E.drawSkewT(box, rows, 'hybrid');
      const chart = document.createElement('div');
      chart.style.width = '600px';
      document.body.append(chart);
      E.drawCharts(chart, [{ name: 'a', time: '2026-10-15T00:00:00Z', t: 1 }, { name: 'a', time: '2026-10-15T01:00:00Z', t: 2 }], ['t']);
      await new Promise((r) => setTimeout(r, 300));
      const blob = await E.viewImage(box);
      const bitmap = await createImageBitmap(blob);
      const chartBlob = await E.viewImage(chart);
      const svgWidth = Math.round(box.querySelector('svg.skewt-diagram').getBoundingClientRect().width);
      box.remove();
      chart.remove();
      const csv = E.rowsCsv([{ name: 'a, b', time: 't', v: 1, _hidden: 2 }, { name: 'say "x"', time: 't', v: null }]);
      return { type: blob.type, width: bitmap.width, svgWidth, size: blob.size, chart: chartBlob && chartBlob.size, csv };
    })()`);
    check(
        'export of a view and of the rows',
        exported.type === 'image/png' && exported.width >= 2 * exported.svgWidth && exported.size > 5000 &&
            exported.chart > 1000 &&
            exported.csv === 'name,time,v\r\n"a, b",t,1\r\n"say ""x""",t,\r\n',
        exported);

    // Units: temperatures in F, speeds in knots and heights in feet, chosen as a user would
    const units = await b.evaluate(`(async () => {
      const E = SmartMetExplorer;
      const chooser = E.unitsChooser(() => {});
      const set = (label, v) => { const s = chooser.querySelector('[aria-label="' + label + '"]'); s.value = v; s.dispatchEvent(new Event('change')); };
      set('Temperature unit', 'F');
      set('Wind speed unit', 'kt');
      set('Height unit', 'ft');
      const rows = [{ name: 'x', time: '2026-10-15T00:00:00Z', temperature: 10, 'T-K': 283.15, windspeedms: 10, geomheight: 1000, humidity: 50, level: 1, pressure: 900 },
                    { name: 'x', time: '2026-10-15T00:00:00Z', temperature: 0, 'T-K': 273.15, windspeedms: 0, geomheight: 2000, humidity: 60, level: 2, pressure: 800 },
                    { name: 'x', time: '2026-10-15T00:00:00Z', temperature: -10, 'T-K': 263.15, windspeedms: 5, geomheight: 3000, humidity: 70, level: 3, pressure: 700 }];
      const shown = E.displayRows(rows);
      const first = shown.rows[0];
      const box = document.createElement('div');
      document.body.append(box);
      E.drawCharts(box, shown.rows.slice(0, 1), ['temperature'], undefined, { labels: shown.labels });
      const caption = box.querySelector('.chart-title').textContent;
      box.remove();
      const csvHead = E.rowsCsv(shown.rows, shown.labels).split('\\r\\n')[0];
      const axis = E.verticalAxes(rows, 'hybrid').find((a) => a.id === 'height');
      const out = { t: Math.round(first.temperature), tk: Math.round(first['T-K']), ws: Math.round(first.windspeedms * 10) / 10,
                    h: Math.round(first.geomheight), rh: first.humidity, caption, csvHead, axis: [axis.unit, Math.round(axis.value(rows[0]))],
                    q: [E.quantityOf('nanmedian_a(t2m)'), E.quantityOf('ws_10min'), E.quantityOf('hl-m'), E.quantityOf('humidity')] };
      set('Temperature unit', 'C');
      set('Wind speed unit', 'ms');
      set('Height unit', 'm');
      out.back = E.displayRows(rows).rows[0].temperature;
      return out;
    })()`);
    check(
        'units of temperatures, wind speeds and heights',
        units.t === 50 && units.tk === 50 && units.ws === 19.4 && units.h === 3281 && units.rh === 50 &&
            units.caption === 'temperature (F)' &&
            units.csvHead === 'name,time,temperature (F),T-K (F),windspeedms (kt),geomheight (ft),humidity,level,pressure' &&
            JSON.stringify(units.axis) === '["ft",3281]' &&
            JSON.stringify(units.q) === '["temperature","speed","height",null]' && units.back === 10,
        units);

    // The values met on a flight: a chart against the distance, the value met in the titles
    const flightView = await b.evaluate(`(async () => {
      const E = SmartMetExplorer;
      const rows = [];
      for (const [lon, k] of [[25, 0], [26, 1], [27, 2]])
        for (const h of [0, 3])
          rows.push({ name: 'p', time: new Date(Date.UTC(2026, 9, 15, h)).toISOString(), lat: 60, lon,
                      temperature: 10 * k + h });
      const box = document.createElement('div');
      box.style.width = '700px';
      document.body.append(box);
      // Passing the points at 00:30, 01:30 and 02:30
      E.drawAlongPath(box, rows, ['temperature'], '', { flight: { departure: Date.UTC(2026, 9, 15, 0, 30), arrival: Date.UTC(2026, 9, 15, 2, 30) } });
      await new Promise((r) => setTimeout(r, 300));
      const flightCanvas = () => box.querySelector('.along-flight canvas').toDataURL();
      const out = { charts: box.querySelectorAll('.along-flight .chart').length,
                    flightTitle: box.querySelector('.along-flight .chart-title').textContent,
                    title: [...box.querySelectorAll('.chart-title')].pop().textContent };
      const before = flightCanvas();
      box.querySelector('input[type="range"]').dispatchEvent(new KeyboardEvent('keydown', { key: 'End', bubbles: true }));
      await new Promise((r) => requestAnimationFrame(r));
      out.last = [...box.querySelectorAll('.chart-title')].pop().textContent;
      out.moved = flightCanvas() !== before;
      box.remove();
      return out;
    })()`);
    check(
        'the values met on a flight',
        flightView.charts === 1 && flightView.flightTitle.startsWith('temperature met on the flight') &&
            /passed 2026-10-15 00:30 UTC: 0.5,/.test(flightView.title) &&
            /passed 2026-10-15 02:30 UTC: 22.5,/.test(flightView.last) && flightView.moved,
        flightView);

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
            dhash.get('diagram') === 'tephigram' && dhash.get('tab') === 'Sounding' &&
            tq.get('param') ===
                'name,time,lat,lon,level,temperature,dewpoint,windspeedms,winddirection,pressure,geomheight',
        {note: diagramNote, params: dhash.get('params'), diagram: dhash.get('diagram'), param: tq.get('param')});

    // The charts of one place compared with another producer, and their differences
    await b.send('Page.navigate', {url: `${base}/api`});
    await sleep(200);
    await b.send('Page.navigate', {
      url: `${base}/api/explorer#api=/timeseries&producer=pal&source=querydata&params=temperature&locmode=place&location=Helsinki`
    });
    await waitFor(b, `!!document.querySelector('button.primary') && !document.querySelector('button.primary').disabled`, 'the form');
    await b.evaluate(`document.querySelector('button.primary').click()`);
    await waitFor(b, `!!document.querySelector('[aria-label="Compare with"]')`, 'the comparison');
    const compared = await b.evaluate(`(async () => {
      const select = document.querySelector('[aria-label="Compare with"]');
      const options = [...select.options].map((o) => o.textContent);
      select.value = select.options[1].value;
      select.dispatchEvent(new Event('change'));
      for (let i = 0; i < 50 && !document.querySelector('.compare-stats div'); i++)
        await new Promise((r) => setTimeout(r, 100));
      const out = { options: options.length, other: select.options[1].textContent.split(' ')[0],
                    title: document.querySelector('.chart-title').textContent,
                    stats: [...document.querySelectorAll('.compare-stats div')].map((d) => d.textContent) };
      select.value = '';
      select.dispatchEvent(new Event('change'));
      await new Promise((r) => setTimeout(r, 200));
      out.after = document.querySelector('.chart-title').textContent;
      return out;
    })()`);
    check(
        'a comparison with another producer',
        compared.options > 1 && compared.title === 'temperature, 2 producers' &&
            compared.stats.length === 1 &&
            compared.stats[0] === `temperature: pal − ${compared.other} on average +0.00, mean absolute difference 0.00 at 6 times` &&
            compared.after === 'temperature',
        compared);

    // An API key: sent as a header with the requests, a placeholder in the code
    await b.send('Page.navigate', {url: `${base}/api`});
    await sleep(200);
    await b.send('Page.navigate', {
      url: `${base}/api/explorer#api=/timeseries&producer=pal&source=querydata&params=temperature&locmode=place&location=Helsinki`
    });
    await waitFor(b, `!!document.querySelector('[aria-label="API key"]')`, 'the API key field');
    apikeys.length = 0;
    await b.evaluate(`(() => {
      const input = document.querySelector('[aria-label="API key"]');
      input.value = 'secret-key-1';
      input.dispatchEvent(new Event('change'));
    })()`);
    await waitFor(b, `!!document.querySelector('button.primary') && !document.querySelector('button.primary').disabled`, 'the form with a key');
    await b.evaluate(`document.querySelector('button.primary').click()`);
    await waitFor(b, `!!document.querySelector('.chart')`, 'the results with a key');
    const keyed = await b.evaluate(`(() => {
      document.querySelector('.code-details').open = true;
      const code = document.querySelector('pre.code').textContent;
      const url = document.querySelector('code.url').textContent;
      const input = document.querySelector('[aria-label="API key"]');
      input.value = '';
      input.dispatchEvent(new Event('change'));
      return { code, url };
    })()`);
    check(
        'an API key in the headers of the requests',
        apikeys.some(([u, k]) => u.startsWith('/timeseries/api/producers') && k === 'secret-key-1') &&
            apikeys.some(([u, k]) => u.startsWith('/timeseries?') && k === 'secret-key-1') &&
            keyed.code.includes("fmi-apikey: <your API key>") && !keyed.code.includes('secret-key-1') &&
            !keyed.url.includes('secret-key-1'),
        {apikeys: apikeys.slice(0, 4), code: keyed.code.slice(0, 200)});

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
    // The marked points of the path as points of their own, or points every 5 km
    await b.evaluate(`document.querySelector('[data-pathpoints="marked"]').click()`);
    await sleep(200);
    const markedQuery = await currentQuery(b);
    await b.evaluate(`(() => {
      const input = document.querySelector('[aria-label="Distance between the points (km)"]');
      input.value = '5';
      input.dispatchEvent(new Event('input', { bubbles: true }));
    })()`);
    await sleep(200);
    const everyQuery = await currentQuery(b);
    check(
        'the points along a path',
        markedQuery.get('lonlats') === pq.get('path') && !markedQuery.has('path') &&
            !markedQuery.has('step') && everyQuery.get('path') === pq.get('path') &&
            everyQuery.get('step') === '5',
        {marked: markedQuery.toString(), every: everyQuery.toString()});
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
                ['Position:true', 'Radius:false', 'Area:false', 'Cube:false', 'Trajectory:false',
                 'Locations:false']),
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

    // A trajectory clicked on the map, at its marked points, animated along the path
    await b.evaluate(`document.querySelector('[data-query="trajectory"]').click()`);
    await sleep(200);
    m = await mapRect(b);
    for (const [fx, fy] of [[0.4, 0.5], [0.5, 0.45], [0.6, 0.5]])
    {
      await mouse(b, 'mouseMoved', m.x + m.w * fx, m.y + m.h * fy, 0);
      await mouse(b, 'mousePressed', m.x + m.w * fx, m.y + m.h * fy, 1);
      await mouse(b, 'mouseReleased', m.x + m.w * fx, m.y + m.h * fy, 0);
      await sleep(250);
    }
    eurl = new URL(await b.evaluate(`document.querySelector('code.url').textContent`));
    const everyStep = eurl.searchParams.get('step');
    await b.evaluate(`document.querySelector('[data-pathpoints="marked"]').click()`);
    await sleep(200);
    eurl = new URL(await b.evaluate(`document.querySelector('code.url').textContent`));
    check(
        'EDR trajectory request',
        eurl.pathname === '/edr/collections/pal/trajectory' &&
            /^LINESTRING\((-?[\d.]+ -?[\d.]+,){2}-?[\d.]+ -?[\d.]+\)$/.test(eurl.searchParams.get('coords')) &&
            Number(everyStep) >= 1 && !eurl.searchParams.has('step'),
        {url: eurl.href, everyStep});
    await b.evaluate(`document.querySelector('button.primary').click()`);
    await waitFor(b, `!!document.querySelector('.along-path .chart')`, 'the trajectory results');
    const along = await b.evaluate(`(async () => {
      const pane = [...document.querySelectorAll('.results .pane')].find((p) => !p.hidden);
      const slider = pane.querySelector('input[type="range"]');
      const canvas = () => pane.querySelector('.along-path canvas').toDataURL();
      const first = { label: pane.querySelector('.time-label').textContent, title: pane.querySelector('.chart-title').textContent,
                      cursor: document.querySelector('.xmap-cursor')?.getAttribute('cx'), canvas: canvas() };
      slider.dispatchEvent(new KeyboardEvent('keydown', { key: 'End', bubbles: true }));
      // The chart is drawn again with the values of the last point (uPlot draws in a microtask)
      await new Promise((r) => requestAnimationFrame(r));
      const redrawn = canvas() !== first.canvas;
      delete first.canvas;
      return { tab: [...document.querySelectorAll('.results-head [role=tab]')].find((x) => x.getAttribute('aria-selected') === 'true').textContent,
               ticks: pane.querySelectorAll('.time-tick').length, first,
               last: pane.querySelector('.time-label').textContent,
               moved: document.querySelector('.xmap-cursor')?.getAttribute('cx') !== first.cursor, redrawn,
               label: pane.querySelector('.along-path-distance').closest('label').firstChild.textContent };
    })()`);
    check(
        'animation along the trajectory',
        along.tab === 'Along the path' && along.ticks === 3 && /^0 km \(/.test(along.first.label) &&
            /^temperature at 0 km of \d+ km/.test(along.first.title) && /^\d+ km \(/.test(along.last) &&
            along.last !== along.first.label && along.moved && along.redrawn && along.label === 'Distance',
        along);

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

    // An EDR position compared with another collection
    await b.send('Page.navigate', {url: `${base}/api`});
    await sleep(200);
    await b.send('Page.navigate', {
      url: `${base}/api/explorer#api=/edr&collection=pal&params=temperature&query=position&location=24.94,60.17`
    });
    await waitFor(b, `!!document.querySelector('button.primary') && !document.querySelector('button.primary').disabled`, 'the EDR form');
    await b.evaluate(`document.querySelector('button.primary').click()`);
    await waitFor(b, `!!document.querySelector('[aria-label="Compare with"]')`, 'the EDR comparison');
    const edrCompared = await b.evaluate(`(async () => {
      const select = document.querySelector('[aria-label="Compare with"]');
      const options = [...select.options].map((o) => o.textContent);
      select.value = 'ecmwf_pressure';
      select.dispatchEvent(new Event('change'));
      for (let i = 0; i < 50 && !document.querySelector('.compare-stats div'); i++)
        await new Promise((r) => setTimeout(r, 100));
      const out = { options, title: document.querySelector('.chart-title').textContent,
                    stats: [...document.querySelectorAll('.compare-stats div')].map((d) => d.textContent) };
      select.value = '';
      select.dispatchEvent(new Event('change'));
      return out;
    })()`);
    check(
        'an EDR position compared with another collection',
        JSON.stringify(edrCompared.options) === '["No comparison","ecmwf_pressure"]' &&
            edrCompared.title === 'temperature, 2 producers' &&
            edrCompared.stats[0] === 'temperature: pal − ecmwf_pressure on average +0.00, mean absolute difference 0.00 at 6 times',
        edrCompared);

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

    // The maps of the coverage in linked panels
    await b.evaluate(`[...document.querySelectorAll('button')].find((x) => x.textContent === 'Show maps').click()`);
    await waitFor(b, `document.querySelectorAll('.coverage-panel .xmap-raster').length === 2`, 'the coverage maps');
    const mapsRequest = new URL(requested.filter((u) => u.includes('f=geotiff')).pop(), 'http://x');
    const coverageMaps = await b.evaluate(`(async () => {
      const wait = (ms) => new Promise((r) => setTimeout(r, ms));
      const panels = [...document.querySelectorAll('.coverage-panel')];
      const params = panels.map((p) => p.querySelector('[aria-label="Parameter of the map"]').value);
      const sizes = () => panels.map((p) => p.querySelector('.xmap-raster').getAttribute('width'));
      const before = sizes();
      panels[0].querySelector('[aria-label="Zoom in"]').click();
      await wait(200);
      const zoomed = sizes();
      // A click on the middle of the raster of the second map
      const image = panels[1].querySelector('.xmap-raster').getBoundingClientRect();
      const surface = panels[1].querySelector('.xmap-svg');
      const at = { bubbles: true, button: 0, pointerId: 1,
                   clientX: image.left + image.width / 2, clientY: image.top + image.height / 2 };
      surface.dispatchEvent(new PointerEvent('pointerdown', at));
      surface.dispatchEvent(new PointerEvent('pointerup', at));
      await wait(300);
      const titles = () => panels.map((p) => (p.querySelector('.map-probe .chart-title') || {}).textContent || '');
      const first = titles();
      document.querySelector('.coverage-maps [aria-label="Next time"]').click();
      await wait(200);
      const next = titles();
      // No more panels than parameters and levels to show
      const count = document.querySelector('.coverage-maps [aria-label="Panels"]');
      const choices = [...count.options].map((o) => o.value);
      count.value = '1';
      count.dispatchEvent(new Event('change', { bubbles: true }));
      await wait(200);
      count.value = '2';
      count.dispatchEvent(new Event('change', { bubbles: true }));
      await wait(300);
      const again = [...document.querySelectorAll('.coverage-panel [aria-label="Parameter of the map"]')].map((x) => x.value);
      return { params, before, zoomed, first, next, choices, again,
               cursors: panels.map((p) => p.querySelectorAll('.xmap-cursor').length),
               panels: document.querySelectorAll('.coverage-panel').length,
               rasters: document.querySelectorAll('.coverage-panel .xmap-raster').length,
               note: document.querySelector('.coverage-maps p.muted.small').textContent };
    })()`);
    check(
        'the maps of a coverage asked in longitude and latitude',
        mapsRequest.searchParams.get('crs') === 'EPSG:4326' &&
            /^Lon\(\d+\),Lat\(\d+\)$/.test(mapsRequest.searchParams.get('scale-size')) &&
            mapsRequest.searchParams.get('bbox') === curl.searchParams.get('bbox'),
        mapsRequest.href);
    check(
        'linked maps of a coverage',
        JSON.stringify(coverageMaps.params) === '["Temperature","Pressure"]' &&
            coverageMaps.note.startsWith('12 × 10 cells, 2 parameters, 3 times') &&
            coverageMaps.zoomed[0] === coverageMaps.zoomed[1] &&
            coverageMaps.zoomed[0] !== coverageMaps.before[0] &&
            coverageMaps.first[0].startsWith('Temperature at') && coverageMaps.first[1].startsWith('Pressure at') &&
            coverageMaps.first[0].includes('03:00') && coverageMaps.next[0].includes('04:00') &&
            JSON.stringify(coverageMaps.cursors) === '[1,1]' && coverageMaps.panels === 2 &&
            coverageMaps.rasters === 2 && JSON.stringify(coverageMaps.choices) === '["1","2"]' &&
            JSON.stringify(coverageMaps.again) === '["Temperature","Pressure"]',
        coverageMaps);

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
