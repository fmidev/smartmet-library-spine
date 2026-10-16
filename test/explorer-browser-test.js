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
function timeseries(query)
{
  const params = (query.get('param') || '').split(',');
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
        else
          row[p] = 10 + l * 0.1 + i;
      }
      list.push(row);
    }
    rows.push(list);
  }
  return locations > 1 ? rows : rows[0];
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
const PRODUCER = '[aria-label="Producer"]';

async function main()
{
  await new Promise((r) => server.listen(0, '127.0.0.1', r));
  const base = `http://127.0.0.1:${server.address().port}`;
  const b = await startBrowser('1280,1000');
  try
  {
    await b.send('Page.navigate', {url: `${base}/api/explorer`});
    await waitFor(b, `!!document.querySelector('${PARAM}')`, 'the TimeSeries module');

    const apis = await b.evaluate(
        `[...document.querySelectorAll('nav select option')].map((o) => o.textContent)`);
    check(
        'only the APIs with a module are offered',
        JSON.stringify(apis) === JSON.stringify(['TimeSeries (/timeseries)']),
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
        q.get('producer') === 'pal' && q.get('param') === 'name,time,temperature,humidity' &&
            q.get('place') === 'Helsinki' && q.get('format') === 'json' && q.get('tz') === 'UTC' &&
            q.get('starttime') === card.start + ':00Z' &&
            url.includes('param=name,time,temperature,humidity'),
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
                JSON.stringify(['name', 'time', 'temperature', 'humidity']),
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
