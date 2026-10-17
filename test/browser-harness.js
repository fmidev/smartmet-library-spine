// Helpers of the headless browser tests: Chromium driven over the DevTools protocol.

'use strict';

const childProcess = require('node:child_process');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

function findBrowser()
{
  for (const name of ['chromium-browser', 'chromium', 'google-chrome', 'google-chrome-stable'])
    if (childProcess.spawnSync('sh', ['-c', `command -v ${name}`]).status === 0)
      return name;
  return null;
}

const browser = findBrowser();

async function startBrowser(windowSize)
{
  const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'viewer-test-'));
  const child = childProcess.spawn(
      browser,
      [
        '--headless=new',
        '--disable-gpu',
        '--no-sandbox',
        `--user-data-dir=${profile}`,
        '--remote-debugging-port=0',
        ...(windowSize ? [`--window-size=${windowSize}`] : []),
        'about:blank'
      ],
      {stdio: 'ignore'});
  const portFile = path.join(profile, 'DevToolsActivePort');
  for (let i = 0; i < 100 && !fs.existsSync(portFile); i++)
    await new Promise((r) => setTimeout(r, 100));
  const port = fs.readFileSync(portFile, 'utf8').split('\n')[0];
  const targets = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json();
  const page = targets.find((t) => t.type === 'page');
  const ws = new WebSocket(page.webSocketDebuggerUrl);
  await new Promise((r) => (ws.onopen = r));
  let id = 0;
  const pending = new Map();
  const errors = [];
  ws.onmessage = (event) => {
    const m = JSON.parse(event.data);
    if (m.id && pending.has(m.id))
    {
      pending.get(m.id)(m.result);
      pending.delete(m.id);
    }
    else if (m.method === 'Runtime.exceptionThrown')
      errors.push(
          m.params.exceptionDetails.exception?.description || m.params.exceptionDetails.text);
  };
  const send = (method, params = {}) => new Promise((r) => {
    const i = ++id;
    pending.set(i, r);
    ws.send(JSON.stringify({id: i, method, params}));
  });
  await send('Runtime.enable');
  await send('Page.enable');
  const evaluate = async (expression) =>
      (await send('Runtime.evaluate', {expression, awaitPromise: true, returnByValue: true}))
          .result.value;
  return {child, profile, send, evaluate, errors};
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function waitFor(b, expression, what)
{
  for (let i = 0; i < 100; i++)
  {
    if (await b.evaluate(expression))
      return;
    await sleep(100);
  }
  throw new Error('timed out waiting for ' + what);
}

// Counts passed and failed checks
function checker()
{
  const result = {checks: 0, failures: 0};
  result.check = (what, ok, detail) => {
    if (ok)
      result.checks++;
    else
    {
      result.failures++;
      console.log(`FAIL ${what}${detail !== undefined ? ': ' + JSON.stringify(detail) : ''}`);
    }
  };
  return result;
}

async function stopBrowser(b)
{
  const exited = new Promise((r) => b.child.once('exit', r));
  b.child.kill();
  await Promise.race([exited, sleep(5000)]);
  try
  {
    fs.rmSync(b.profile, {recursive: true, force: true});
  }
  catch (e)
  {
    // A browser process may still be writing the profile, the system removes it later
  }
}

module.exports = {
  browser,
  startBrowser,
  stopBrowser,
  sleep,
  waitFor,
  checker
};
