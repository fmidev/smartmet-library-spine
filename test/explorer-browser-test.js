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
      parameters: ['humidity', 'temperature'],
      bbox: [5, 51, 49, 72],
      time: {start: iso(now - 6 * hour), end: iso(now.getTime() + 60 * hour), step: 60}
    },
    {
      name: 'ecmwf',
      source: 'querydata',
      parameters: ['temperature', 'windspeedms'],
      time: {start: iso(now - 12 * hour), end: iso(now.getTime() + 240 * hour), step: 180}
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
    windspeedms: {name: 'WindSpeedMS', description: 'Wind speed'}
  }
};

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
        else
          row[p] = 10 + l * 0.1 + i;
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
        'parameter search by name', r.found && r.labels.some((l) => l.includes('3 producers')), r);
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
        note === '1 of 3 producers have all these parameters',
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
        covering === '0 of 3 producers have all these parameters in the selected area',
        covering);
    await setLocation(b, 'not a box');
    const invalid = await b.evaluate(`document.querySelector('.warn').textContent`);
    check('invalid box', invalid.startsWith('Not a valid box'), invalid);

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
