// Browser tests of the SmartMet Swagger UI plugin (spine/OpenApiPlugin.cpp): the request
// preview and the response viewer.
//
// Usage: node openapi-browser-test.js <dir> <swagger-ui dir>
//
// <dir> contains the documentation page (docs.html) and the plugin (smartmet-swagger-ui.js)
// as spine serves them. The test serves them with the Swagger UI files and a test definition,
// enters parameters and executes requests in a headless Chromium, and checks the request
// preview and the tree of the JSON and XML responses.
// The test is skipped if Chromium or the Swagger UI files are not available.

"use strict";

const childProcess = require("node:child_process");
const fs = require("node:fs");
const http = require("node:http");
const os = require("node:os");
const path = require("node:path");

const [dir, swaggerDir] = process.argv.slice(2);

function findBrowser() {
  for (const name of ["chromium-browser", "chromium", "google-chrome", "google-chrome-stable"])
    if (childProcess.spawnSync("sh", ["-c", `command -v ${name}`]).status === 0) return name;
  return null;
}

const browser = findBrowser();
if (!browser || !fs.existsSync(path.join(swaggerDir || "", "swagger-ui-bundle.js"))) {
  console.log("Chromium or the Swagger UI files are not available, the browser tests are not run");
  process.exit(0);
}

// ----------------------------------------------------------------------
// Test data
// ----------------------------------------------------------------------

const VALUES = 5000;
const json = {
  type: "Coverage",
  domain: { type: "Domain", axes: { t: { values: ["2026-01-01T00:00:00Z"] } } },
  parameters: { Temperature: { type: "Parameter", description: { en: "<img src=x onerror=alert(1)>" } } },
  ranges: { Temperature: { type: "NdArray", values: Array.from({ length: VALUES }, (_, i) => i / 10) } }
};
const xml = `<?xml version="1.0"?>
<Capabilities version="1.0.0"><Contents><Layer name="a"><Title>A</Title><Style><Title>Default</Title></Style></Layer><Layer name="b"><Title>B</Title></Layer></Contents></Capabilities>`;

const definition = {
  openapi: "3.1.1",
  info: { title: "Viewer test", version: "1.0" },
  servers: [{ url: "/edr" }],
  paths: {
    "/json": { get: { operationId: "getJson", responses: { 200: { description: "x", content: { "application/vnd.cov+json": {} } } } } },
    "/xml": { get: { operationId: "getXml", responses: { 200: { description: "x", content: { "application/xml": {} } } } } },
    "/data": { get: { operationId: "getData",
      parameters: [
        { name: "coords", in: "query", required: true, schema: { type: "string" } },
        { name: "datetime", in: "query", schema: { type: "string" } },
        { name: "parameter-name", in: "query", style: "form", explode: false,
          schema: { type: "array", items: { type: "string", enum: ["Temperature", "Humidity"] } } }],
      responses: { 200: { description: "x", content: { "application/json": {} } } } } },
    "/timeseries": { get: { operationId: "getTimeSeries",
      parameters: [
        { name: "keyword", in: "query", schema: { type: "string" },
          "x-smartmet-locations": { path: "/timeseries", parameter: "keyword", language: "lang",
            query: { param: "name,lat,lon,fmisid", timesteps: "1", format: "json" } } },
        { name: "lang", in: "query", schema: { type: "string" } }],
      responses: { 200: { description: "x", content: { "application/json": {} } } } } }
  },
  "x-smartmet-map": { tiles: "/tiles/{z}/{y}/{x}?f=png", tileSize: 256, attribution: "Test <tiles>" }
};

const locations = [
  { name: "Helsinki <b>", lat: 60.17, lon: 24.94, fmisid: 100971 },
  { name: "Tampere", lat: 61.5, lon: 23.76, fmisid: null },
  { name: "Oulu", lat: 65.01, lon: 25.47, fmisid: 101799 },
  { name: "No coordinates", lat: null, lon: null }
];
// A 1x1 PNG
const png = Buffer.from("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==", "base64");

const types = { ".js": "text/javascript", ".css": "text/css", ".png": "image/png" };

const requested = [];
const server = http.createServer((req, res) => {
  requested.push(req.url);
  const send = (status, type, body) => {
    res.writeHead(status, { "Content-Type": type, "Cache-Control": "no-store" });
    res.end(body);
  };
  const url = req.url.split("?")[0];
  if (url === "/edr/api") return send(200, "application/vnd.oai.openapi+json;version=3.1", JSON.stringify(definition));
  if (url === "/edr/api/docs") return send(200, "text/html; charset=UTF-8", fs.readFileSync(path.join(dir, "docs.html")));
  if (url === "/edr/api/docs/smartmet-swagger-ui.js")
    return send(200, "text/javascript", fs.readFileSync(path.join(dir, "smartmet-swagger-ui.js")));
  if (url.startsWith("/edr/api/docs/")) {
    const file = path.join(swaggerDir, path.basename(url));
    if (fs.existsSync(file)) return send(200, types[path.extname(file)] || "application/octet-stream", fs.readFileSync(file));
  }
  if (url === "/edr/json") return send(200, "application/vnd.cov+json", JSON.stringify(json));
  if (url === "/edr/xml") return send(200, "application/xml", xml);
  if (url === "/edr/timeseries") return send(200, "application/json", JSON.stringify(locations));
  if (url.startsWith("/tiles/")) return send(200, "image/png", png);
  if (url === "/edr/data") return send(200, "application/json", JSON.stringify({ ok: true }));
  send(404, "text/plain", "not found");
});

// ----------------------------------------------------------------------
// Chromium over the DevTools protocol
// ----------------------------------------------------------------------

async function startBrowser() {
  const profile = fs.mkdtempSync(path.join(os.tmpdir(), "viewer-test-"));
  const child = childProcess.spawn(browser, ["--headless=new", "--disable-gpu", "--no-sandbox",
    `--user-data-dir=${profile}`, "--remote-debugging-port=0", "about:blank"], { stdio: "ignore" });
  const portFile = path.join(profile, "DevToolsActivePort");
  for (let i = 0; i < 100 && !fs.existsSync(portFile); i++) await new Promise((r) => setTimeout(r, 100));
  const port = fs.readFileSync(portFile, "utf8").split("\n")[0];
  const targets = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json();
  const page = targets.find((t) => t.type === "page");
  const ws = new WebSocket(page.webSocketDebuggerUrl);
  await new Promise((r) => (ws.onopen = r));
  let id = 0;
  const pending = new Map();
  const errors = [];
  ws.onmessage = (event) => {
    const m = JSON.parse(event.data);
    if (m.id && pending.has(m.id)) {
      pending.get(m.id)(m.result);
      pending.delete(m.id);
    } else if (m.method === "Runtime.exceptionThrown")
      errors.push(m.params.exceptionDetails.exception?.description || m.params.exceptionDetails.text);
  };
  const send = (method, params = {}) =>
    new Promise((r) => {
      const i = ++id;
      pending.set(i, r);
      ws.send(JSON.stringify({ id: i, method, params }));
    });
  await send("Runtime.enable");
  await send("Page.enable");
  const evaluate = async (expression) =>
    (await send("Runtime.evaluate", { expression, awaitPromise: true, returnByValue: true })).result.value;
  return { child, profile, send, evaluate, errors };
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function waitFor(b, expression, what) {
  for (let i = 0; i < 100; i++) {
    if (await b.evaluate(expression)) return;
    await sleep(100);
  }
  throw new Error("timed out waiting for " + what);
}

// Execute an operation and return when its response tree is shown
async function execute(b, operationId) {
  const op = `document.querySelector('[id$="-${operationId}"]')`;
  await waitFor(b, `!!${op}`, operationId);
  await b.evaluate(`window.ui.layoutActions.show(["operations", "default", "${operationId}"], true)`);
  await waitFor(b, `!!${op}.querySelector("button.execute")`, "execute button");
  await sleep(1000);  // the opened operation is rendered asynchronously
  await b.evaluate(`${op}.querySelector("button.execute").click()`);
  await waitFor(b, `!!${op}.querySelector(".sm-tree details, .sm-tree .sm-leaf")`, "response tree");
  return op;
}

// ----------------------------------------------------------------------

let failures = 0;
let checks = 0;
function check(what, ok, detail) {
  if (ok) checks++;
  else {
    failures++;
    console.log(`FAIL ${what}${detail !== undefined ? ": " + JSON.stringify(detail) : ""}`);
  }
}

async function main() {
  await new Promise((r) => server.listen(0, "127.0.0.1", r));
  const base = `http://127.0.0.1:${server.address().port}`;
  const b = await startBrowser();
  try {
    await b.send("Page.navigate", { url: `${base}/edr/api/docs` });

    // JSON
    const op = await execute(b, "getJson");
    const state = await b.evaluate(`(() => {
      const tree = ${op}.querySelector(".sm-tree");
      const summaries = [...tree.querySelectorAll("summary")].map((s) => s.textContent);
      return { summaries, leaves: tree.querySelectorAll(".sm-leaf").length,
               imgs: tree.querySelectorAll("img").length, text: tree.textContent };
    })()`);
    check("root is a coverage", state.summaries[0] === "{ 4 members }", state.summaries[0]);
    check("the values are not built before they are opened", state.leaves < 20, state.leaves);

    // Open ranges > Temperature > values: lazy, in pages
    const values = await b.evaluate(`(async () => {
      const tree = ${op}.querySelector(".sm-tree");
      // A child of 'parent' whose summary starts with the label
      const find = (parent, label) => [...parent.querySelectorAll(":scope > details")].find((d) =>
        d.querySelector(":scope > summary").textContent.startsWith(label));
      let v = tree.querySelector(":scope > details");
      for (const label of ['"ranges"', '"Temperature"', '"values"']) {
        v = find(v, label);
        if (!v) return { missing: label };
        v.open = true;
        await new Promise((r) => setTimeout(r, 50));
      }
      const items = () => v.querySelectorAll(":scope > .sm-inline > .sm-item").length;
      const before = items();
      const summary = v.querySelector(":scope > summary").textContent;
      v.querySelector(":scope > .sm-inline > button.sm-more").click();
      const after = items();
      const more = v.querySelector(":scope > .sm-inline > button.sm-more").textContent;
      return { summary, before, after, more };
    })()`);
    check("array summary", values.summary === `"values": [ ${VALUES} numbers ]`, values);
    check("first page", values.before === 200, values);
    check("next page", values.after === 400, values);
    check("rest button", values.more === `show 200 more of ${VALUES - 400}`, values);

    // Collapse and expand
    const toggled = await b.evaluate(`(async () => {
      const tool = (label) => [...${op}.querySelectorAll(".sm-viewer-toolbar button")].find((x) => x.textContent === label);
      tool("Collapse").click();
      await new Promise((r) => setTimeout(r, 50));
      const open = ${op}.querySelectorAll(".sm-tree details[open]").length;
      tool("Expand 4 levels").click();
      await new Promise((r) => setTimeout(r, 50));
      return { open, opened: ${op}.querySelectorAll(".sm-tree details[open]").length };
    })()`);
    check("collapse", toggled.open === 0, toggled);
    check("expand", toggled.opened >= 6, toggled);

    // A value with markup, built by the expansion above, is text
    const markup = await b.evaluate(`(() => { const tree = ${op}.querySelector(".sm-tree");
      return { imgs: tree.querySelectorAll("img").length, text: tree.textContent.includes("<img src=x onerror"),
               summaries: [...tree.querySelectorAll("summary")].map((s) => s.textContent) }; })()`);
    check("markup in a value is text", markup.imgs === 0 && markup.text, markup);
    check("singular count", markup.summaries.includes('"values": [ 1 value ]'), markup.summaries);

    // Text view is the Swagger UI view
    const text = await b.evaluate(`(async () => {
      [...${op}.querySelectorAll(".sm-viewer-toolbar button")].find((x) => x.textContent === "Text").click();
      await new Promise((r) => setTimeout(r, 200));
      const heading = [...${op}.querySelectorAll("h5")].find((e) => e.textContent === "Response body");
      const body = heading && heading.parentElement.querySelector("pre");
      return { tree: !!${op}.querySelector(".sm-tree"), body: body ? body.textContent.slice(0, 40) : null };
    })()`);
    check("text view", !text.tree && text.body && text.body.startsWith("{"), text);

    // XML
    const xop = await execute(b, "getXml");
    const x = await b.evaluate(`(async () => {
      [...${xop}.querySelectorAll(".sm-viewer-toolbar button")].find((x) => x.textContent === "Expand 4 levels").click();
      await new Promise((r) => setTimeout(r, 50));
      const tree = ${xop}.querySelector(".sm-tree");
      return { summaries: [...tree.querySelectorAll("summary")].map((s) => s.textContent),
               leaves: [...tree.querySelectorAll(".sm-leaf")].map((l) => l.textContent) };
    })()`);
    check("xml root", x.summaries[0] === '<Capabilities version="1.0.0"> 1 element', x.summaries);
    check("xml attributes", x.summaries.includes('<Layer name="a"> 2 elements'), x.summaries);
    check("xml leaf", x.leaves.includes("<Title>B</Title>"), x.leaves);

    // Request preview
    const dop = `document.querySelector('[id$="-getData"]')`;
    await waitFor(b, `!!${dop}`, "getData");
    await b.evaluate(`window.ui.layoutActions.show(["operations", "default", "getData"], true)`);
    await waitFor(b, `!!${dop}.querySelector(".sm-preview code")`, "preview");
    const empty = await b.evaluate(`(() => ({ url: ${dop}.querySelector(".sm-preview code").textContent,
      missing: (${dop}.querySelector(".sm-preview-missing") || {}).textContent }))()`);
    check("preview without values", /\/edr\/data$/.test(empty.url), empty);
    check("missing required", empty.missing === "Required parameters without a value: coords", empty);

    await b.evaluate(`(() => {
      const set = (name, value) => {
        const input = ${dop}.querySelector('tr[data-param-name="' + name + '"] input');
        Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, "value").set.call(input, value);
        input.dispatchEvent(new Event("input", { bubbles: true }));
      };
      set("coords", "POINT(24.94 60.17)");
      set("datetime", "2026-01-01T00:00:00Z/2026-01-31T23:59:59Z");
      const select = ${dop}.querySelector('tr[data-param-name="parameter-name"] select');
      for (const o of select.options) o.selected = true;
      select.dispatchEvent(new Event("change", { bubbles: true }));
    })()`);
    await sleep(500);
    const filled = await b.evaluate(`(async () => {
      const preview = ${dop}.querySelector(".sm-preview");
      const details = preview.querySelector("details");
      details.open = true;
      await new Promise((r) => setTimeout(r, 300));
      const tabs = [...preview.querySelectorAll(".request-snippets .btn")].map((x) => x.textContent.trim());
      const python = [...preview.querySelectorAll(".request-snippets .btn")].find((x) => x.textContent.trim() === "Python loop");
      if (python) python.click();
      await new Promise((r) => setTimeout(r, 200));
      const code = preview.querySelector(".request-snippets pre, .request-snippets code");
      return { url: preview.querySelector("code").textContent,
               missing: !!preview.querySelector(".sm-preview-missing"), tabs,
               code: code ? code.textContent : null,
               copy: !![...preview.querySelectorAll("button")].find((x) => x.textContent === "Copy") };
    })()`);
    const expectedUrl = `${base}/edr/data?coords=POINT%2824.94%2060.17%29&datetime=2026-01-01T00%3A00%3A00Z%2F2026-01-31T23%3A59%3A59Z&parameter-name=Temperature,Humidity`;
    check("preview url", filled.url === expectedUrl, filled.url);
    check("no missing after input", !filled.missing, filled);
    check("copy button", filled.copy, filled);
    check("preview snippets", filled.tabs.length === 8 && filled.tabs[0] === "bash", filled.tabs);
    check("preview loop snippet", filled.code && filled.code.includes('STEP = "week"') && filled.code.includes("/edr/data?coords="), filled.code);
    check("nothing sent before execute", !requested.some((u) => u.startsWith("/edr/data")), requested);

    // Execute sends the previewed URL
    await b.evaluate(`${dop}.querySelector("button.execute").click()`);
    await waitFor(b, `!!${dop}.querySelector(".sm-tree")`, "data response");
    const sent = requested.filter((u) => u.startsWith("/edr/data"));
    check("execute sends the previewed request", sent.length === 1 && base + sent[0] === expectedUrl, sent);

    // The locations of a keyword on a map
    const top = `document.querySelector('[id$="-getTimeSeries"]')`;
    await waitFor(b, `!!${top}`, "getTimeSeries");
    await b.evaluate(`window.ui.layoutActions.show(["operations", "default", "getTimeSeries"], true)`);
    await waitFor(b, `!!${top}.querySelector(".sm-map-controls button")`, "map button");
    await sleep(500);
    const disabled = await b.evaluate(`${top}.querySelector(".sm-map-controls button").disabled`);
    check("map button disabled without a keyword", disabled === true, disabled);
    // One at a time: the inputs are debounced, and an update of one resets a pending other
    for (const [name, value] of [["keyword", "synop fi"], ["lang", "sv"]]) {
      await b.evaluate(`(() => {
        const input = ${top}.querySelector('tr[data-param-name="${name}"] input');
        Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, "value").set.call(input, "${value}");
        input.dispatchEvent(new Event("input", { bubbles: true }));
      })()`);
      await sleep(600);
    }
    await b.evaluate(`${top}.querySelector(".sm-map-controls button").click()`);
    await waitFor(b, `${top}.querySelectorAll(".sm-map circle").length > 0`, "map points");
    await sleep(300);
    const mapState = await b.evaluate(`(() => {
      const circles = [...${top}.querySelectorAll(".sm-map circle")];
      return { circles: circles.length,
               inside: circles.every((c) => +c.getAttribute("cx") > 0 && +c.getAttribute("cy") > 0 &&
                                            +c.getAttribute("cx") < 2000 && +c.getAttribute("cy") < 420),
               tiles: ${top}.querySelectorAll(".sm-map image").length,
               count: ${top}.querySelector(".sm-map-controls").textContent,
               attribution: (${top}.querySelector(".sm-map-attribution") || {}).textContent };
    })()`);
    const query = requested.filter((u) => u.startsWith("/edr/timeseries"));
    const q = query.length ? new URL(query[0], base).searchParams : new URLSearchParams();
    check("locations query", query.length === 1 && q.get("keyword") === "synop fi" && query[0].includes("keyword=synop%20fi") && q.get("lang") === "sv" &&
          q.get("param") === "name,lat,lon,fmisid" && q.get("format") === "json", query);
    check("locations without coordinates are left out", mapState.circles === 3, mapState);
    check("locations inside the map", mapState.inside, mapState);
    check("location count", mapState.count.includes("3 locations"), mapState);
    check("attribution is text", mapState.attribution === "Test <tiles>", mapState);
    const tiles = requested.filter((u) => u.startsWith("/tiles/"));
    check("tiles drawn and fetched", mapState.tiles > 0 && tiles.length > 0, { mapState, tiles });

    const hover = await b.evaluate(`(async () => {
      const c = ${top}.querySelector(".sm-map circle");
      c.dispatchEvent(new MouseEvent("mouseover", { bubbles: true }));
      await new Promise((r) => setTimeout(r, 100));
      const t = ${top}.querySelector(".sm-map-tooltip");
      const shown = { display: t.style.display, name: t.querySelector("b").textContent,
                      rows: [...t.querySelectorAll("tr")].map((r) => r.textContent), html: t.querySelectorAll("b b").length };
      c.dispatchEvent(new MouseEvent("mouseout", { bubbles: true }));
      await new Promise((r) => setTimeout(r, 100));
      shown.after = t.style.display;
      return shown;
    })()`);
    if (process.env.OPENAPI_SCREENSHOT) {
      await b.evaluate(`(() => { ${top}.querySelector(".sm-map").scrollIntoView();
        ${top}.querySelector(".sm-map circle").dispatchEvent(new MouseEvent("mouseover", { bubbles: true })); })()`);
      await sleep(300);
      const shot = await b.send("Page.captureScreenshot", { format: "png" });
      fs.writeFileSync(process.env.OPENAPI_SCREENSHOT, Buffer.from(shot.data, "base64"));
      await b.evaluate(`${top}.querySelector(".sm-map circle").dispatchEvent(new MouseEvent("mouseout", { bubbles: true }))`);
    }
    check("tooltip shown", hover.display === "block" && hover.name === "Helsinki <b>" && hover.html === 0, hover);
    check("tooltip rows", hover.rows.includes("fmisid100971") && hover.rows.includes("latitude60.17"), hover.rows);
    check("tooltip hidden", hover.after === "none", hover);

    const zoomed = await b.evaluate(`(async () => {
      const zoom = ${top}.querySelector(".sm-map-zoom button");
      const z = (src) => src.split("/")[2];
      const before = z(${top}.querySelector(".sm-map image").getAttribute("href"));
      zoom.click();
      await new Promise((r) => setTimeout(r, 300));
      return { before, after: z(${top}.querySelector(".sm-map image").getAttribute("href")) };
    })()`);
    check("zoom in", +zoomed.after === +zoomed.before + 1, zoomed);

    await b.evaluate(`${top}.querySelector(".sm-map-controls button").click()`);
    await sleep(300);
    const hidden = await b.evaluate(`!${top}.querySelector(".sm-map")`);
    check("hide map", hidden, hidden);

    // Without a tile layer the background is a longitude/latitude grid
    const grid = await b.evaluate(`(() => {
      const div = document.createElement("div");
      div.style.width = "600px";
      document.body.appendChild(div);
      const remove = SmartMetSwaggerUI.drawMap(div, [{ name: "a", lat: 60, lon: 20 }, { name: "b", lat: 70, lon: 30 }], null);
      const result = { lines: div.querySelectorAll("line").length, images: div.querySelectorAll("image").length,
                       labels: [...div.querySelectorAll("text")].map((t) => t.textContent),
                       points: div.querySelectorAll("circle").length };
      remove();
      result.removed = div.childNodes.length === 0;
      div.remove();
      return result;
    })()`);
    check("grid background", grid.lines >= 6 && grid.images === 0 && grid.points === 2 && grid.labels.includes("20°") && grid.labels.includes("70°"), grid);
    check("map removal", grid.removed, grid);
    const roundTrip = await b.evaluate(`(() => { const [x, y] = SmartMetSwaggerUI.project(24.94, 60.17);
      return SmartMetSwaggerUI.unproject(x, y); })()`);
    check("projection round trip", Math.abs(roundTrip[0] - 24.94) < 1e-9 && Math.abs(roundTrip[1] - 60.17) < 1e-9, roundTrip);

    check("no page errors", b.errors.length === 0, b.errors);
  } finally {
    const exited = new Promise((r) => b.child.once("exit", r));
    b.child.kill();
    await Promise.race([exited, sleep(5000)]);
    server.close();
    try {
      fs.rmSync(b.profile, { recursive: true, force: true });
    } catch (e) {
      // A browser process may still be writing the profile, the system removes it later
    }
  }
  console.log(`${checks} browser checks passed, ${failures} failed`);
  process.exit(failures ? 1 : 0);
}

main().catch((e) => {
  console.log("FAIL " + e.message);
  process.exit(1);
});
