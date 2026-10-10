// ======================================================================
/*!
 * \brief The SmartMet plugin of the OpenAPI documentation pages
 *
 * A Swagger UI plugin served as <root>/api/docs/smartmet-swagger-ui.js:
 *
 *  - Python, R and JavaScript snippets of the executed request, and for
 *    bash, Python, R and JavaScript scripts which repeat the request for
 *    consecutive days, weeks, months or years
 *  - JSON and XML responses as a collapsible tree
 *  - the URL and the snippets of the request before it is executed
 *
 * test/openapi-snippets-test.js runs the generated scripts, and
 * test/openapi-browser-test.js checks the viewer in a headless browser.
 */
// ======================================================================

namespace SmartMet
{
namespace Spine
{
namespace OpenAPI
{
extern const char* const SWAGGER_UI_PLUGIN_JS;

const char* const SWAGGER_UI_PLUGIN_JS = R"JS(/*
 * SmartMet plugin for Swagger UI: request snippets and a response viewer
 *
 * Swagger UI shows the executed request as code snippets. This plugin adds
 * generators for Python, R and JavaScript (Node.js), and for bash, Python, R
 * and JavaScript a variant which repeats the request for consecutive time
 * periods (days, weeks, months or years) and saves each response to a file.
 *
 * The scripts use only standard tools: curl, the Python standard library,
 * base R (4.0 or newer) and Node.js 18 or newer.
 *
 * The time window of the request is recognised from the URL: datetime given
 * as an interval start/end (EDR, Coverages), or starttime and endtime
 * (TimeSeries). Each period ends one second before the next one starts, so
 * the data at the period boundaries is fetched only once.
 *
 * JSON and XML responses are shown as a collapsible tree, with the original
 * text view of Swagger UI available as an alternative.
 *
 * The URL and the snippets of the request are shown while the parameters are
 * entered, before the request is executed.
 */
(function (root) {
  "use strict";

  // ------------------------------------------------------------------
  // Request access. Swagger UI passes an Immutable.js map, the tests a
  // plain object.
  // ------------------------------------------------------------------

  function field(request, name) {
    return typeof request.get === "function" ? request.get(name) : request[name];
  }

  function requestInfo(request) {
    const url = new URL(field(request, "url"));
    const headers = [];
    let h = field(request, "headers");
    if (h && typeof h.toJS === "function") h = h.toJS();
    if (h) {
      for (const name of Object.keys(h)) {
        const value = h[name];
        if (value !== undefined && value !== null && String(value) !== "")
          headers.push([name, String(value)]);
      }
    }
    return { url: url, headers: headers };
  }

  // ------------------------------------------------------------------
  // Quoting
  // ------------------------------------------------------------------

  // A JSON string literal is also a valid Python, R and JavaScript string literal
  function quote(s) {
    return JSON.stringify(String(s));
  }

  function shellQuote(s) {
    return "'" + String(s).replace(/'/g, "'\\''") + "'";
  }

  // ------------------------------------------------------------------
  // Times
  // ------------------------------------------------------------------

  // Parse the time formats of the APIs to a Date, or null: RFC 3339 / ISO 8601
  // (2026-01-01T00:00:00Z, 2026-01-01T00:00Z, 2026-01-01) and the compact
  // forms 20260101T000000 and 202601010000. Times without an offset are UTC.
  function parseTime(text) {
    if (!text) return null;
    let s = String(text).trim();
    let m = s.match(/^(\d{4})(\d{2})(\d{2})T?(\d{2})(\d{2})(\d{2})?$/);
    if (m) s = `${m[1]}-${m[2]}-${m[3]}T${m[4]}:${m[5]}:${m[6] || "00"}Z`;
    else if (/^\d{4}-\d{2}-\d{2}$/.test(s)) s += "T00:00:00Z";
    else if (/^\d{4}-\d{2}-\d{2}[T ]\d{2}:\d{2}(:\d{2}(\.\d+)?)?$/.test(s)) s = s.replace(" ", "T") + "Z";
    else if (!/^\d{4}-\d{2}-\d{2}[T ]\d{2}:\d{2}(:\d{2}(\.\d+)?)?(Z|[+-]\d{2}:?\d{2})$/.test(s))
      return null;
    const t = new Date(s);
    return isNaN(t.getTime()) ? null : t;
  }

  function iso(t) {
    return t.toISOString().replace(/\.\d{3}Z$/, "Z");
  }

  // The time window of the request, or null
  function timeWindow(url) {
    const p = url.searchParams;
    const datetime = p.get("datetime");
    if (datetime && datetime.indexOf("/") > 0) {
      const parts = datetime.split("/");
      const start = parseTime(parts[0]);
      const end = parseTime(parts[1]);
      if (parts.length === 2 && start && end && start < end)
        return { start: start, end: end, params: ["datetime"] };
    }
    const start = parseTime(p.get("starttime"));
    const end = parseTime(p.get("endtime"));
    if (start && end && start < end) return { start: start, end: end, params: ["starttime", "endtime"] };
    return null;
  }

  // The loop step suggested for the time window
  function suggestedStep(window) {
    const days = (window.end - window.start) / 86400000;
    if (days > 3 * 366) return "year";
    if (days > 62) return "month";
    if (days > 14) return "week";
    return "day";
  }

  // The URL without the time window parameters. The rest of the query string is kept as it
  // was, so the encoding of the other options does not change.
  function loopUrl(url, window) {
    const kept = url.search
      .replace(/^\?/, "")
      .split("&")
      .filter((part) => part !== "" && !window.params.includes(decodeURIComponent(part.split("=")[0])));
    return url.origin + url.pathname + "?" + kept.join("&");
  }

  // ------------------------------------------------------------------
  // Output file name extension
  // ------------------------------------------------------------------

  const extensions = {
    coveragejson: "json", json: "json", geojson: "geojson", csv: "csv", xml: "xml", wxml: "xml",
    iwxxm: "xml", iwxxmzip: "zip", html: "html", debug: "html", ascii: "txt", tac: "txt",
    php: "txt", serial: "txt", netcdf: "nc", grib2: "grib2", grib1: "grib", grib: "grib",
    geotiff: "tif", gtiff: "tif", tiff: "tif", qd: "sqd", querydata: "sqd", png: "png",
    webp: "webp", svg: "svg", mvt: "pbf", pbf: "pbf", datatile: "png", mapbox: "json"
  };

  function extension(url) {
    const p = url.searchParams;
    const f = (p.get("f") || p.get("format") || "").toLowerCase();
    if (extensions[f]) return extensions[f];
    const m = url.pathname.match(/\.([a-z0-9]+)$/i);
    if (m) return m[1].toLowerCase();
    if (/\/timeseries\b|\/pointforecast\b/.test(url.pathname)) return "txt";
    if (/\/coverages\//.test(url.pathname)) return "nc";
    return "json";
  }

  // ------------------------------------------------------------------
  // Single requests
  // ------------------------------------------------------------------

  function python(request) {
    const r = requestInfo(request);
    const ext = extension(r.url);
    const headers = r.headers.map(([k, v]) => `${quote(k)}: ${quote(v)}`).join(", ");
    return [
      "import urllib.request",
      "",
      `url = ${quote(r.url)}`,
      `request = urllib.request.Request(url, headers={${headers}})`,
      "with urllib.request.urlopen(request, timeout=300) as response:",
      "    data = response.read()",
      "",
      `with open("result.${ext}", "wb") as f:`,
      "    f.write(data)",
      ""
    ].join("\n");
  }

  function r(request) {
    const req = requestInfo(request);
    const ext = extension(req.url);
    const headers = req.headers.map(([k, v]) => `${quote(k)} = ${quote(v)}`).join(", ");
    return [
      "# Base R 4.0 or newer",
      `url <- ${quote(req.url)}`,
      `download.file(url, "result.${ext}", mode = "wb", quiet = TRUE,`,
      `              headers = c(${headers}))`,
      ""
    ].join("\n");
  }

  function javascript(request) {
    const r = requestInfo(request);
    const ext = extension(r.url);
    const headers = r.headers.map(([k, v]) => `${quote(k)}: ${quote(v)}`).join(", ");
    return [
      "// Node.js 18 or newer",
      'const fs = require("node:fs/promises");',
      "",
      "async function main() {",
      `  const url = ${quote(r.url)};`,
      `  const response = await fetch(url, { headers: { ${headers} } });`,
      "  if (!response.ok) throw new Error(`${response.status} ${response.statusText}: ${await response.text()}`);",
      `  await fs.writeFile("result.${ext}", Buffer.from(await response.arrayBuffer()));`,
      "}",
      "",
      "main().catch((e) => { console.error(e.message); process.exit(1); });",
      ""
    ].join("\n");
  }

  // ------------------------------------------------------------------
  // Loops over time periods
  // ------------------------------------------------------------------

  function noWindow(comment) {
    return [
      `${comment} To get a script which repeats the request for consecutive days, weeks, months`,
      `${comment} or years, give the time window as datetime=start/end (EDR, Coverages) or as`,
      `${comment} starttime and endtime (TimeSeries), and execute the request again.`,
      ""
    ].join("\n");
  }

  function bashLoop(request) {
    const r = requestInfo(request);
    const w = timeWindow(r.url);
    if (!w) return noWindow("#");
    const ext = extension(r.url);
    const headers = r.headers.map(([k, v]) => `-H ${shellQuote(k + ": " + v)}`).join(" ");
    const query = w.params.length === 1
      ? '"datetime=${start}/${end}"'
      : '"starttime=${start}&endtime=${end}"';
    return [
      "#!/bin/bash",
      "# Repeats the request for consecutive periods and saves each response to a file.",
      "# Needs curl and GNU date. Start the periods at the beginning of a day, week, month or year.",
      "set -euo pipefail",
      "",
      `START=${shellQuote(iso(w.start).replace("T", " ").replace("Z", ""))}   # UTC`,
      `END=${shellQuote(iso(w.end).replace("T", " ").replace("Z", ""))}`,
      `STEP=${shellQuote("1 " + suggestedStep(w))}   # e.g. '1 day', '7 days', '1 month', '1 year'`,
      `URL=${shellQuote(loopUrl(r.url, w))}`,
      "",
      'end_s=$(date -u -d "$END" +%s)',
      'period="$START"',
      'while [ "$(date -u -d "$period" +%s)" -le "$end_s" ]; do',
      '  next=$(date -u -d "$period $STEP" "+%Y-%m-%d %H:%M:%S")',
      '  last=$(date -u -d "$next 1 second ago" +%s)',
      '  [ "$last" -gt "$end_s" ] && last=$end_s',
      '  start=$(date -u -d "$period" +%Y-%m-%dT%H:%M:%SZ)',
      '  end=$(date -u -d "@$last" +%Y-%m-%dT%H:%M:%SZ)',
      `  file="result_$(date -u -d "$period" +%Y%m%dT%H%M%S).${ext}"`,
      '  echo "$start - $end -> $file"',
      `  curl -fsS --retry 3 ${headers ? headers + " " : ""}-o "$file" "$URL&"${query}`,
      '  period="$next"',
      "done",
      ""
    ].join("\n");
  }

  function pythonLoop(request) {
    const r = requestInfo(request);
    const w = timeWindow(r.url);
    if (!w) return noWindow("#");
    const ext = extension(r.url);
    const headers = r.headers.map(([k, v]) => `${quote(k)}: ${quote(v)}`).join(", ");
    const window = w.params.length === 1
      ? '{"datetime": f"{iso(start)}/{iso(end)}"}'
      : '{"starttime": iso(start), "endtime": iso(end)}';
    return [
      '"""Repeats the request for consecutive periods and saves each response to a file."""',
      "import datetime as dt",
      "import urllib.parse",
      "import urllib.request",
      "",
      "UTC = dt.timezone.utc",
      `START = dt.datetime.fromisoformat(${quote(iso(w.start).replace("Z", "+00:00"))})`,
      `END = dt.datetime.fromisoformat(${quote(iso(w.end).replace("Z", "+00:00"))})`,
      `STEP = ${quote(suggestedStep(w))}  # "day", "week", "month" or "year"`,
      `URL = ${quote(loopUrl(r.url, w))}`,
      `HEADERS = {${headers}}`,
      "",
      "",
      "def advance(t, step):",
      '    if step == "day":',
      "        return t + dt.timedelta(days=1)",
      '    if step == "week":',
      "        return t + dt.timedelta(weeks=1)",
      '    months = 12 if step == "year" else 1',
      "    year, month = divmod(t.month - 1 + months, 12)",
      "    return t.replace(year=t.year + year, month=month + 1)",
      "",
      "",
      "def iso(t):",
      '    return t.astimezone(UTC).strftime("%Y-%m-%dT%H:%M:%SZ")',
      "",
      "",
      "period = START",
      "while period <= END:",
      "    following = advance(period, STEP)",
      "    start, end = period, min(following - dt.timedelta(seconds=1), END)",
      `    url = URL + "&" + urllib.parse.urlencode(${window}, safe=":/")`,
      `    file = f"result_{period.astimezone(UTC):%Y%m%dT%H%M%S}.${ext}"`,
      '    print(f"{iso(start)} - {iso(end)} -> {file}")',
      "    request = urllib.request.Request(url, headers=HEADERS)",
      "    with urllib.request.urlopen(request, timeout=300) as response:",
      "        data = response.read()",
      '    with open(file, "wb") as f:',
      "        f.write(data)",
      "    period = following",
      ""
    ].join("\n");
  }

  function rLoop(request) {
    const req = requestInfo(request);
    const w = timeWindow(req.url);
    if (!w) return noWindow("#");
    const ext = extension(req.url);
    const headers = req.headers.map(([k, v]) => `${quote(k)} = ${quote(v)}`).join(", ");
    const window = w.params.length === 1
      ? 'paste0("datetime=", iso(start), "/", iso(end))'
      : 'paste0("starttime=", iso(start), "&endtime=", iso(end))';
    return [
      "# Repeats the request for consecutive periods and saves each response to a file.",
      "# Base R 4.0 or newer.",
      `start_time <- as.POSIXct(${quote(iso(w.start).replace("T", " ").replace("Z", ""))}, tz = "UTC")`,
      `end_time <- as.POSIXct(${quote(iso(w.end).replace("T", " ").replace("Z", ""))}, tz = "UTC")`,
      `step <- ${quote(suggestedStep(w))}  # "day", "week", "month" or "year"`,
      `url <- ${quote(loopUrl(req.url, w))}`,
      `headers <- c(${headers})`,
      "",
      'iso <- function(t) format(t, "%Y-%m-%dT%H:%M:%SZ", tz = "UTC")',
      "",
      "periods <- seq(start_time, end_time, by = step)",
      "for (i in seq_along(periods)) {",
      "  start <- periods[i]",
      "  following <- seq(start, by = step, length.out = 2)[2]",
      "  end <- min(following - 1, end_time)",
      `  file <- paste0("result_", format(start, "%Y%m%dT%H%M%S", tz = "UTC"), ".${ext}")`,
      '  cat(iso(start), "-", iso(end), "->", file, "\\n")',
      `  download.file(paste0(url, "&", ${window}), file,`,
      '                mode = "wb", quiet = TRUE, headers = headers)',
      "}",
      ""
    ].join("\n");
  }

  function javascriptLoop(request) {
    const r = requestInfo(request);
    const w = timeWindow(r.url);
    if (!w) return noWindow("//");
    const ext = extension(r.url);
    const headers = r.headers.map(([k, v]) => `${quote(k)}: ${quote(v)}`).join(", ");
    const window = w.params.length === 1
      ? "`datetime=${iso(start)}/${iso(end)}`"
      : "`starttime=${iso(start)}&endtime=${iso(end)}`";
    return [
      "// Repeats the request for consecutive periods and saves each response to a file.",
      "// Node.js 18 or newer.",
      'const fs = require("node:fs/promises");',
      "",
      `const START = new Date(${quote(iso(w.start))});`,
      `const END = new Date(${quote(iso(w.end))});`,
      `const STEP = ${quote(suggestedStep(w))}; // "day", "week", "month" or "year"`,
      `const BASE_URL = ${quote(loopUrl(r.url, w))};`,
      `const HEADERS = { ${headers} };`,
      "",
      "function advance(t, step) {",
      "  const d = new Date(t);",
      '  if (step === "day") d.setUTCDate(d.getUTCDate() + 1);',
      '  else if (step === "week") d.setUTCDate(d.getUTCDate() + 7);',
      '  else if (step === "month") d.setUTCMonth(d.getUTCMonth() + 1);',
      "  else d.setUTCFullYear(d.getUTCFullYear() + 1);",
      "  return d;",
      "}",
      "",
      'const iso = (t) => t.toISOString().replace(/\\.\\d{3}Z$/, "Z");',
      "",
      "async function main() {",
      "  for (let period = START; period <= END; period = advance(period, STEP)) {",
      "    const start = period;",
      "    const following = advance(period, STEP);",
      "    const end = new Date(Math.min(following.getTime() - 1000, END.getTime()));",
      `    const file = \`result_\${iso(start).replace(/[-:]/g, "").replace("Z", "")}.${ext}\`;`,
      "    console.log(`${iso(start)} - ${iso(end)} -> ${file}`);",
      "    const response = await fetch(BASE_URL + \"&\" + " + window + ", { headers: HEADERS });",
      "    if (!response.ok) throw new Error(`${response.status} ${response.statusText}: ${await response.text()}`);",
      "    await fs.writeFile(file, Buffer.from(await response.arrayBuffer()));",
      "  }",
      "}",
      "",
      "main().catch((e) => { console.error(e.message); process.exit(1); });",
      ""
    ].join("\n");
  }

  // ------------------------------------------------------------------
  // Response viewer: JSON and XML responses as a collapsible tree.
  //
  // The tree is built with DOM nodes and text only, never from markup, so
  // the response cannot inject anything into the page. Children are built
  // when a node is first opened, and long arrays are shown in pages, so that
  // large responses such as CoverageJSON value arrays stay responsive.
  // ------------------------------------------------------------------

  const PAGE = 200;       // array items and object members shown at a time
  const OPEN_LEVELS = 2;  // levels opened initially

  const css = `
.sm-viewer-toolbar { margin: 4px 0; }
.sm-viewer-toolbar button { margin-right: 6px; padding: 2px 10px; font-size: 12px; cursor: pointer;
  border: 1px solid #888; border-radius: 3px; background: #fff; }
.sm-viewer-toolbar button.sm-active { background: #333; color: #fff; }
.sm-tree { background: #333; color: #fff; font: 12px/1.5 monospace; padding: 8px 10px 8px 1.6em;
  border-radius: 4px; max-height: 600px; overflow: auto; white-space: pre-wrap; word-break: break-all; }
.sm-tree details { margin-left: 1.2em; }
.sm-tree > details { margin-left: 0; }
.sm-tree summary { cursor: pointer; list-style-position: outside; }
.sm-tree .sm-leaf, .sm-tree .sm-inline { margin-left: 2.4em; }
.sm-tree > .sm-leaf { margin-left: 0; }
.sm-tree .sm-key { color: #9cdcfe; }
.sm-tree .sm-string { color: #ce9178; }
.sm-tree .sm-number { color: #b5cea8; }
.sm-tree .sm-literal { color: #569cd6; }
.sm-tree .sm-tag { color: #4ec9b0; }
.sm-tree .sm-attr { color: #9cdcfe; }
.sm-tree .sm-note { color: #999; font-style: italic; }
.sm-tree button.sm-more { margin: 2px 0 2px 2.4em; font-size: 11px; cursor: pointer; }
.sm-tree .sm-inline button.sm-more { margin-left: 0.5em; }
.sm-tree .sm-inline { word-break: normal; }
.sm-tree .sm-item { white-space: nowrap; }
`;

  function addStyle(doc) {
    if (doc.getElementById("sm-viewer-style")) return;
    const style = doc.createElement("style");
    style.id = "sm-viewer-style";
    style.textContent = css;
    doc.head.appendChild(style);
  }

  function span(doc, cls, text) {
    const e = doc.createElement("span");
    e.className = cls;
    e.textContent = text;
    return e;
  }

  // ---- JSON

  function isPrimitiveArray(value) {
    return value.every((v) => v === null || typeof v !== "object");
  }

  function jsonValue(doc, value) {
    if (typeof value === "string") return span(doc, "sm-string", JSON.stringify(value));
    if (typeof value === "number") return span(doc, "sm-number", String(value));
    return span(doc, "sm-literal", String(value));
  }

  function jsonSummary(value) {
    if (Array.isArray(value)) {
      const n = value.length;
      if (n > 0 && isPrimitiveArray(value)) {
        const numbers = value.every((v) => typeof v === "number" || v === null);
        const kind = numbers ? (n === 1 ? "number" : "numbers") : (n === 1 ? "value" : "values");
        return `[ ${n} ${kind} ]`;
      }
      return `[ ${n} ${n === 1 ? "item" : "items"} ]`;
    }
    const n = Object.keys(value).length;
    return `{ ${n} ${n === 1 ? "member" : "members"} }`;
  }

  function jsonLeaf(doc, key, value) {
    const e = doc.createElement("div");
    e.className = "sm-leaf";
    if (key !== null) e.append(span(doc, "sm-key", JSON.stringify(key)), ": ");
    e.append(jsonValue(doc, value));
    return e;
  }

  // Append entries [start, start + PAGE) of a list built by makeNode, with a button for the rest
  function appendPage(doc, parent, entries, start, makeNode) {
    const end = Math.min(entries.length, start + PAGE);
    for (let i = start; i < end; i++) parent.appendChild(makeNode(entries[i]));
    if (end < entries.length) {
      const more = doc.createElement("button");
      more.className = "sm-more";
      more.type = "button";
      more.textContent = `show ${Math.min(PAGE, entries.length - end)} more of ${entries.length - end}`;
      more.addEventListener("click", () => {
        more.remove();
        appendPage(doc, parent, entries, end, makeNode);
      });
      parent.appendChild(more);
    }
  }

  function jsonNode(doc, key, value, level) {
    if (value === null || typeof value !== "object") return jsonLeaf(doc, key, value);

    const details = doc.createElement("details");
    const summary = doc.createElement("summary");
    if (key !== null) summary.append(span(doc, "sm-key", JSON.stringify(key)), ": ");
    summary.append(span(doc, "sm-note", jsonSummary(value)));
    details.appendChild(summary);

    const entries = Array.isArray(value) ? value.map((v, i) => [i, v]) : Object.entries(value);
    let built = false;
    const build = () => {
      if (built) return;
      built = true;
      if (Array.isArray(value) && isPrimitiveArray(value)) {
        // Plain values side by side, wrapped
        const box = doc.createElement("div");
        box.className = "sm-inline";
        details.appendChild(box);
        appendPage(doc, box, entries, 0, ([i, v]) => {
          // The value does not break, the lines break between the values
          const item = doc.createElement("span");
          item.className = "sm-item";
          item.append(jsonValue(doc, v), i + 1 < entries.length ? "," : "");
          const fragment = doc.createDocumentFragment();
          fragment.append(item, " ");
          return fragment;
        });
      } else
        appendPage(doc, details, entries, 0, ([k, v]) =>
          jsonNode(doc, Array.isArray(value) ? null : k, v, level + 1));
    };
    details.addEventListener("toggle", () => { if (details.open) build(); });
    if (level < OPEN_LEVELS) {
      details.open = true;
      build();
    }
    return details;
  }

  // ---- XML

  function xmlStartTag(doc, element, close) {
    const e = doc.createElement("span");
    e.append(span(doc, "sm-tag", "<" + element.nodeName));
    for (const a of Array.from(element.attributes))
      e.append(" ", span(doc, "sm-attr", a.name), "=", span(doc, "sm-string", JSON.stringify(a.value)));
    e.append(span(doc, "sm-tag", close ? "/>" : ">"));
    return e;
  }

  function xmlChildren(element) {
    return Array.from(element.childNodes).filter((n) =>
      n.nodeType === 1 || ((n.nodeType === 3 || n.nodeType === 4) && n.nodeValue.trim() !== ""));
  }

  function xmlNode(doc, node, level) {
    if (node.nodeType !== 1) {
      const e = doc.createElement("div");
      e.className = "sm-leaf";
      e.append(span(doc, "sm-string", node.nodeValue.trim()));
      return e;
    }
    const children = xmlChildren(node);
    // Elements without child elements on one line
    if (children.every((c) => c.nodeType !== 1)) {
      const e = doc.createElement("div");
      e.className = "sm-leaf";
      if (children.length === 0) e.append(xmlStartTag(doc, node, true));
      else
        e.append(xmlStartTag(doc, node, false), span(doc, "sm-string", node.textContent.trim()),
                 span(doc, "sm-tag", "</" + node.nodeName + ">"));
      return e;
    }
    const details = doc.createElement("details");
    const summary = doc.createElement("summary");
    const elements = children.filter((c) => c.nodeType === 1).length;
    summary.append(xmlStartTag(doc, node, false), " ",
                   span(doc, "sm-note", `${elements} ${elements === 1 ? "element" : "elements"}`));
    details.appendChild(summary);
    let built = false;
    const build = () => {
      if (built) return;
      built = true;
      appendPage(doc, details, children, 0, (c) => xmlNode(doc, c, level + 1));
    };
    details.addEventListener("toggle", () => { if (details.open) build(); });
    if (level < OPEN_LEVELS) {
      details.open = true;
      build();
    }
    return details;
  }

  // Build the tree of a response into 'container'. Returns false if the content cannot be
  // parsed, the caller then shows the text instead.
  function renderTree(container, text, kind) {
    const doc = container.ownerDocument;
    let root;
    if (kind === "json") {
      let value;
      try {
        value = JSON.parse(text);
      } catch (e) {
        return false;
      }
      root = jsonNode(doc, null, value, 0);
    } else {
      const xml = new DOMParser().parseFromString(text, "application/xml");
      if (xml.getElementsByTagName("parsererror").length > 0) return false;
      root = xmlNode(doc, xml.documentElement, 0);
    }
    container.replaceChildren(root);
    return true;
  }

  // Open (or close) all details elements down to the given depth below the container
  function setOpen(container, open, depth) {
    const walk = (node, level) => {
      for (const child of Array.from(node.children)) {
        if (child.tagName === "DETAILS") {
          if (open && level < depth) {
            child.open = true;
            child.dispatchEvent(new Event("toggle"));
            walk(child, level + 1);
          } else if (!open) {
            child.open = false;
            walk(child, level + 1);
          }
        }
      }
    };
    walk(container, 0);
  }

  function responseKind(contentType) {
    const ct = String(contentType || "").toLowerCase();
    if (/json/.test(ct)) return "json";
    if (/xml/.test(ct) && !/svg/.test(ct)) return "xml";
    return null;
  }

  // Wraps the Swagger UI response body: JSON and XML get a Tree/Text toggle
  function wrapResponseBody(Original, system) {
    const React = system.React;
    const h = React.createElement;

    function Tree(props) {
      const ref = React.useRef(null);
      const [failed, setFailed] = React.useState(false);
      React.useEffect(() => {
        let cancelled = false;
        const show = (text) => {
          if (cancelled || !ref.current) return;
          addStyle(ref.current.ownerDocument);
          setFailed(!renderTree(ref.current, text, props.kind));
        };
        const content = props.content;
        if (typeof Blob !== "undefined" && content instanceof Blob) content.text().then(show);
        else show(content == null ? "" : String(content));
        return () => { cancelled = true; };
      }, [props.content, props.kind]);
      if (failed) return h(Original, props.original);
      return h("div", { ref: ref, className: "sm-tree" });
    }

    return function SmartMetResponseBody(props) {
      const kind = responseKind(props.contentType);
      const [mode, setMode] = React.useState("tree");
      const treeRef = React.useRef(null);
      if (!kind || props.content == null || props.content === "") return h(Original, props);
      const button = (label, active, onClick) =>
        h("button", { type: "button", className: active ? "sm-active" : "", onClick: onClick }, label);
      const levels = (open) => () => {
        const tree = treeRef.current && treeRef.current.querySelector(".sm-tree");
        if (tree) setOpen(tree, open, 4);
      };
      return h("div", { className: "sm-viewer", ref: treeRef },
        h("div", { className: "sm-viewer-toolbar" },
          button("Tree", mode === "tree", () => setMode("tree")),
          button("Text", mode === "text", () => setMode("text")),
          mode === "tree" ? button("Expand 4 levels", false, levels(true)) : null,
          mode === "tree" ? button("Collapse", false, levels(false)) : null),
        mode === "text" ? h(Original, props)
                        : h(Tree, { content: props.content, kind: kind, original: props }));
    };
  }

  // ------------------------------------------------------------------
  // Request preview: the URL and the snippets of the request as the
  // parameters are entered, before it is executed. The request is built
  // exactly as Swagger UI builds it when the request is executed.
  // ------------------------------------------------------------------

  const previewCss = `
.sm-preview { margin: 10px 0; padding: 8px 10px; border: 1px solid #c8d7e6; border-radius: 4px; background: #f5f9fc;
  text-align: left; }
.sm-preview * { text-align: left; }
.sm-preview-title { font-weight: bold; font-size: 13px; margin-bottom: 4px; }
.sm-preview-url { display: flex; gap: 6px; align-items: flex-start; }
.sm-preview-url code { flex: 1; font: 12px/1.5 monospace; word-break: break-all; background: #fff;
  border: 1px solid #ddd; border-radius: 3px; padding: 4px 6px; }
.sm-preview-url button { font-size: 12px; padding: 3px 10px; cursor: pointer; }
.sm-preview-missing { color: #b3261e; font-size: 12px; margin-top: 4px; }
.sm-preview details { margin-top: 6px; }
.sm-preview summary { cursor: pointer; font-size: 12px; }
`;

  function addPreviewStyle(doc) {
    if (doc.getElementById("sm-preview-style")) return;
    const style = doc.createElement("style");
    style.id = "sm-preview-style";
    style.textContent = previewCss;
    doc.head.appendChild(style);
  }

  // The plain object of the definition, converted again only when the definition changes
  let specCache = { immutable: null, plain: null };
  function plainSpec(immutable) {
    if (specCache.immutable !== immutable) specCache = { immutable: immutable, plain: immutable.toJS() };
    return specCache.plain;
  }

  // The request Swagger UI would send, as in its executeRequest action
  function previewRequest(system, path, method) {
    const specSelectors = system.specSelectors;
    const oas3Selectors = system.oas3Selectors;
    const fullSpec = plainSpec(specSelectors.specJsonWithResolvedSubtrees());
    const operation = ((fullSpec.paths || {})[path] || {})[method] || {};

    // buildRequest refuses a request without its required parameters, the preview shows it
    // anyway and lists the missing ones. Execute validates the parameters as usual.
    const optional = Object.assign({}, operation, {
      parameters: (operation.parameters || []).map((p) => (p && p.required ? Object.assign({}, p, { required: false }) : p))
    });
    const pathItem = Object.assign({}, fullSpec.paths[path], { [method]: optional });
    const spec = Object.assign({}, fullSpec, { paths: Object.assign({}, fullSpec.paths, { [path]: pathItem }) });
    const contentTypes = specSelectors.contentTypeValues([path, method]).toJS();
    const parameters = specSelectors.parameterValues([path, method], /xml/i.test(contentTypes.requestContentType || "")).toJS();
    const namespace = `${path}:${method}`;
    const server = oas3Selectors.selectedServer(namespace) || oas3Selectors.selectedServer();
    const local = oas3Selectors.serverVariables({ server: server, namespace: namespace }).toJS();
    const request = {
      spec: spec,
      pathName: path,
      method: method,
      parameters: parameters,
      operationId: operation.operationId || system.fn.opId(operation, path, method),
      contextUrl: new URL(specSelectors.url(), window.location.href).toString(),
      server: server,
      serverVariables: Object.keys(local).length ? local : oas3Selectors.serverVariables({ server: server }).toJS(),
      requestContentType: oas3Selectors.requestContentType(path, method),
      responseContentType: oas3Selectors.responseContentType(path, method) || "*/*"
    };
    const built = system.fn.buildRequest(request);

    // Required parameters without a value
    const missing = [];
    for (const p of operation.parameters || []) {
      if (!p || !p.required) continue;
      const value = parameters[`${p.in}.${p.name}`] !== undefined ? parameters[`${p.in}.${p.name}`] : parameters[p.name];
      if (value === undefined || value === null || value === "" || (Array.isArray(value) && value.length === 0))
        missing.push(p.name);
    }
    return { request: built, missing: missing };
  }

  // Wraps the Execute button: the preview is shown above it when the operation is in input mode
  function wrapExecute(Original, system) {
    const React = system.React;
    const h = React.createElement;

    return function SmartMetExecute(props) {
      // Follow the parameter values in the store
      const [, setTick] = React.useState(0);
      React.useEffect(() => {
        const store = system.getStore();
        let last = null;
        return store.subscribe(() => {
          const state = store.getState();
          if (state !== last) {
            last = state;
            setTick((t) => t + 1);
          }
        });
      }, []);
      const [copied, setCopied] = React.useState(false);

      let preview = null;
      try {
        preview = previewRequest(system, props.path, props.method);
      } catch (e) {
        // The parameters may not be resolved yet
      }
      if (!preview || !preview.request || !preview.request.url) return h(Original, props);
      if (typeof document !== "undefined") addPreviewStyle(document);

      const url = preview.request.url;
      const copy = () => {
        navigator.clipboard.writeText(url).then(() => {
          setCopied(true);
          setTimeout(() => setCopied(false), 1500);
        });
      };
      const RequestSnippets = system.getComponent("RequestSnippets", true);
      return h(React.Fragment, null,
        h("div", { className: "sm-preview" },
          h("div", { className: "sm-preview-title" }, "Request URL"),
          h("div", { className: "sm-preview-url" },
            h("code", null, url),
            h("button", { type: "button", onClick: copy }, copied ? "Copied" : "Copy")),
          preview.missing.length
            ? h("div", { className: "sm-preview-missing" }, "Required parameters without a value: " + preview.missing.join(", "))
            : null,
          RequestSnippets
            ? h("details", null,
                h("summary", null, "Code for this request"),
                h(RequestSnippets, { request: system.Im.fromJS(preview.request) }))
            : null),
        h(Original, props));
    };
  }

  // ------------------------------------------------------------------
  // The Swagger UI plugin
  // ------------------------------------------------------------------

  const generators = {
    requestSnippetGenerator_smartmet_bash_loop: bashLoop,
    requestSnippetGenerator_smartmet_python: python,
    requestSnippetGenerator_smartmet_python_loop: pythonLoop,
    requestSnippetGenerator_smartmet_r: r,
    requestSnippetGenerator_smartmet_r_loop: rLoop,
    requestSnippetGenerator_smartmet_javascript: javascript,
    requestSnippetGenerator_smartmet_javascript_loop: javascriptLoop
  };

  // The snippets in the order they are shown, curl_bash is built into Swagger UI
  const order = ["curl_bash", "smartmet_bash_loop", "smartmet_python", "smartmet_python_loop",
                 "smartmet_r", "smartmet_r_loop", "smartmet_javascript", "smartmet_javascript_loop"];

  const snippetConfig = {
    languages: order,
    generators: {
      curl_bash: { title: "bash", syntax: "bash" },
      smartmet_bash_loop: { title: "bash loop", syntax: "bash" },
      smartmet_python: { title: "Python", syntax: "python" },
      smartmet_python_loop: { title: "Python loop", syntax: "python" },
      smartmet_r: { title: "R", syntax: "r" },
      smartmet_r_loop: { title: "R loop", syntax: "r" },
      smartmet_javascript: { title: "JavaScript", syntax: "javascript" },
      smartmet_javascript_loop: { title: "JavaScript loop", syntax: "javascript" }
    },
    defaultExpanded: true
  };

  const api = {
    plugin: function () {
      return {
        fn: generators,
        wrapComponents: { responseBody: wrapResponseBody, execute: wrapExecute },
        // Swagger UI merges the snippets with its own into an unordered map, show them in order
        statePlugins: {
          requestSnippets: {
            wrapSelectors: {
              getSnippetGenerators: (original) => (...args) =>
                original(...args).sortBy((value, key) => order.indexOf(key))
            }
          }
        }
      };
    },
    snippetConfig: snippetConfig,
    generators: generators,
    parseTime: parseTime,
    timeWindow: timeWindow,
    renderTree: renderTree,
    setOpen: setOpen,
    responseKind: responseKind
  };

  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.SmartMetSwaggerUI = api;
})(typeof window !== "undefined" ? window : this);
)JS";

}  // namespace OpenAPI
}  // namespace Spine
}  // namespace SmartMet
