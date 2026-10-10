// ======================================================================
/*!
 * \brief Request snippets for the OpenAPI documentation pages
 *
 * A Swagger UI plugin served as <root>/api/docs/smartmet-snippets.js. It adds
 * Python, R and JavaScript snippets of the executed request, and for bash,
 * Python, R and JavaScript a script which repeats the request for
 * consecutive days, weeks, months or years. test/openapi-snippets-test.js
 * runs the generated scripts.
 */
// ======================================================================

namespace SmartMet
{
namespace Spine
{
namespace OpenAPI
{
extern const char* const SNIPPETS_JS;

const char* const SNIPPETS_JS = R"JS(/*
 * SmartMet request snippets for Swagger UI
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
    timeWindow: timeWindow
  };

  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.SmartMetSnippets = api;
})(typeof window !== "undefined" ? window : this);
)JS";

}  // namespace OpenAPI
}  // namespace Spine
}  // namespace SmartMet
