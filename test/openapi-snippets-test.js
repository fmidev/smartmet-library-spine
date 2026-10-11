// Tests of the request snippets of the SmartMet Swagger UI plugin (spine/OpenApiPlugin.cpp).
//
// Usage: node openapi-snippets-test.js <path of smartmet-swagger-ui.js>
//
// Generates the snippets for EDR, TimeSeries and Coverages style requests and runs them with
// bash, Python, Node.js and R against a local HTTP server which records the requests. The
// test checks the time windows of the requests and the files the scripts write. A language
// whose interpreter is not installed is skipped.

"use strict";

const assert = require("node:assert");
const childProcess = require("node:child_process");
const fs = require("node:fs");
const http = require("node:http");
const os = require("node:os");
const path = require("node:path");

const snippets = require(path.resolve(process.argv[2]));
const generators = snippets.generators;

function have(program) {
  return childProcess.spawnSync("sh", ["-c", `command -v ${program}`]).status === 0;
}

const interpreters = {
  bash: have("bash") && have("curl") && childProcess.spawnSync("date", ["-u", "-d", "2026-01-31 00:00:00 1 month"]).status === 0,
  python: have("python3"),
  javascript: true,
  r: have("Rscript")
};

const run = {
  bash: (file, cwd) => childProcess.spawnSync("bash", [file], { cwd, encoding: "utf8" }),
  python: (file, cwd) => childProcess.spawnSync("python3", [file], { cwd, encoding: "utf8" }),
  javascript: (file, cwd) => childProcess.spawnSync("node", [file], { cwd, encoding: "utf8" }),
  r: (file, cwd) => childProcess.spawnSync("Rscript", [file], { cwd, encoding: "utf8" })
};

const suffix = { bash: "sh", python: "py", javascript: "js", r: "R" };

// ----------------------------------------------------------------------
// The recording server, run in a child process so that the synchronous
// script runs below do not block it
// ----------------------------------------------------------------------

function startServer() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "snippets-server-"));
  const log = path.join(dir, "requests.log");
  const code = `
    const http = require("node:http"), fs = require("node:fs");
    const server = http.createServer((req, res) => {
      fs.appendFileSync(${JSON.stringify(log)}, JSON.stringify({ url: req.url, headers: req.headers }) + "\\n");
      res.writeHead(200, { "Content-Type": "text/plain" });
      res.end("response to " + req.url);
    });
    server.listen(0, "127.0.0.1", () => console.log(server.address().port));
  `;
  const child = childProcess.spawn("node", ["-e", code], { stdio: ["ignore", "pipe", "inherit"] });
  return new Promise((resolve) => {
    child.stdout.once("data", (data) => resolve({ child, port: Number(String(data).trim()), log }));
  });
}

function takeRequests(log) {
  if (!fs.existsSync(log)) return [];
  const lines = fs.readFileSync(log, "utf8").split("\n").filter(Boolean).map((l) => JSON.parse(l));
  fs.unlinkSync(log);
  return lines;
}

// ----------------------------------------------------------------------
// The cases
// ----------------------------------------------------------------------

function cases(port) {
  const base = `http://127.0.0.1:${port}`;
  return [
    {
      name: "EDR position, months",
      url: `${base}/edr/collections/pal/position?coords=POINT(24.94%2060.17)&parameter-name=Temperature&datetime=2026-01-01T00:00:00Z/2026-03-31T23:59:59Z&f=CoverageJSON`,
      headers: { accept: "application/vnd.cov+json" },
      ext: "json",
      windows: [
        ["2026-01-01T00:00:00Z", "2026-01-31T23:59:59Z"],
        ["2026-02-01T00:00:00Z", "2026-02-28T23:59:59Z"],
        ["2026-03-01T00:00:00Z", "2026-03-31T23:59:59Z"]
      ]
    },
    {
      name: "TimeSeries, weeks",
      url: `${base}/timeseries?place=Helsinki&param=time,temperature&starttime=20260101T000000&endtime=20260128T000000&format=json`,
      headers: { accept: "*/*" },
      ext: "json",
      windows: [
        ["2026-01-01T00:00:00Z", "2026-01-07T23:59:59Z"],
        ["2026-01-08T00:00:00Z", "2026-01-14T23:59:59Z"],
        ["2026-01-15T00:00:00Z", "2026-01-21T23:59:59Z"],
        ["2026-01-22T00:00:00Z", "2026-01-28T00:00:00Z"]
      ]
    },
    {
      name: "Coverages, years",
      url: `${base}/coverages/collections/ecmwf/coverage?properties=Temperature&datetime=2020-01-01T00:00:00Z/2023-12-31T23:59:59Z&f=grib2`,
      headers: {},
      ext: "grib2",
      windows: [
        ["2020-01-01T00:00:00Z", "2020-12-31T23:59:59Z"],
        ["2021-01-01T00:00:00Z", "2021-12-31T23:59:59Z"],
        ["2022-01-01T00:00:00Z", "2022-12-31T23:59:59Z"],
        ["2023-01-01T00:00:00Z", "2023-12-31T23:59:59Z"]
      ]
    },
    {
      name: "Quoting",
      url: `${base}/edr/collections/pal/position?coords=POINT(24.94%2060.17)&parameter-name=it's&datetime=2026-01-01T00:00:00Z/2026-01-02T23:59:59Z`,
      headers: { accept: "application/json", "x-test": "a 'quoted' \"value\" $HOME `x`" },
      ext: "json",
      windows: [
        ["2026-01-01T00:00:00Z", "2026-01-01T23:59:59Z"],
        ["2026-01-02T00:00:00Z", "2026-01-02T23:59:59Z"]
      ]
    }
  ];
}

function windowOf(url) {
  const p = new URL(url, "http://localhost").searchParams;
  if (p.get("datetime")) return p.get("datetime").split("/");
  return [p.get("starttime"), p.get("endtime")];
}

// ----------------------------------------------------------------------
// Running the scripts
// ----------------------------------------------------------------------

let failures = 0;
let checks = 0;

function check(what, fn) {
  try {
    fn();
    checks++;
  } catch (e) {
    failures++;
    console.log(`FAIL ${what}: ${e.message}`);
  }
}

function runScript(language, text, log) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "snippets-"));
  const file = path.join(dir, `script.${suffix[language]}`);
  fs.writeFileSync(file, text);
  const result = run[language](file, dir);
  return { dir, result, requests: takeRequests(log) };
}

async function main() {
  const server = await startServer();
  try {
    for (const c of cases(server.port)) {
      const request = { url: c.url, method: "GET", headers: c.headers };

      // Single requests
      for (const language of ["python", "r", "javascript"]) {
        const text = generators[`requestSnippetGenerator_smartmet_${language}`](request);
        if (!interpreters[language]) continue;
        const { dir, result, requests } = runScript(language, text, server.log);
        check(`${c.name}: ${language} single request`, () => {
          assert.strictEqual(result.status, 0, result.stderr);
          assert.strictEqual(requests.length, 1);
          assert.strictEqual("http://127.0.0.1:" + server.port + requests[0].url, new URL(c.url).toString());
          for (const [k, v] of Object.entries(c.headers)) assert.strictEqual(requests[0].headers[k], v);
          assert.ok(fs.existsSync(path.join(dir, `result.${c.ext}`)), "no result file");
        });
      }

      // Loops
      for (const language of ["bash", "python", "r", "javascript"]) {
        const text = generators[`requestSnippetGenerator_smartmet_${language}_loop`](request);
        check(`${c.name}: ${language} loop is a script`, () => assert.ok(!/To get a script/.test(text)));
        if (!interpreters[language]) continue;
        const { dir, result, requests } = runScript(language, text, server.log);
        check(`${c.name}: ${language} loop`, () => {
          assert.strictEqual(result.status, 0, result.stderr + result.stdout);
          assert.deepStrictEqual(requests.map((r) => windowOf(r.url)), c.windows);
          for (const r of requests)
            for (const [k, v] of Object.entries(c.headers)) assert.strictEqual(r.headers[k], v);
          // The other options are kept
          const expected = new URL(c.url);
          for (const r of requests) {
            const got = new URL(r.url, "http://localhost").searchParams;
            for (const [k, v] of expected.searchParams)
              if (!["datetime", "starttime", "endtime"].includes(k)) assert.strictEqual(got.get(k), v, k);
          }
          // The other options keep their original encoding
          const raw = (u) => u.replace(/^[^?]*\?/, "").split("&")
            .filter((p) => !/^(datetime|starttime|endtime)=/.test(p));
          for (const r of requests) assert.deepStrictEqual(raw(r.url), raw(new URL(c.url).search));
          const files = fs.readdirSync(dir).filter((f) => f.startsWith("result_")).sort();
          assert.strictEqual(files.length, c.windows.length, files.join(" "));
          for (const f of files) assert.ok(f.endsWith("." + c.ext), f);
        });
      }
    }

    // Without a time window the loops explain how to get one
    const plain = { url: `http://127.0.0.1:${server.port}/edr/collections/pal/position?coords=POINT(25%2060)`, headers: {} };
    for (const language of ["bash", "python", "r", "javascript"])
      check(`no window: ${language}`, () =>
        assert.ok(/To get a script/.test(generators[`requestSnippetGenerator_smartmet_${language}_loop`](plain))));

    // Time formats
    check("parseTime", () => {
      const iso = (s) => snippets.parseTime(s).toISOString();
      assert.strictEqual(iso("2026-01-02T03:04:05Z"), "2026-01-02T03:04:05.000Z");
      assert.strictEqual(iso("2026-01-02T03:04Z"), "2026-01-02T03:04:00.000Z");
      assert.strictEqual(iso("2026-01-02"), "2026-01-02T00:00:00.000Z");
      assert.strictEqual(iso("20260102T030405"), "2026-01-02T03:04:05.000Z");
      assert.strictEqual(iso("202601020304"), "2026-01-02T03:04:00.000Z");
      assert.strictEqual(iso("2026-01-02T03:04:05+02:00"), "2026-01-02T01:04:05.000Z");
      assert.strictEqual(snippets.parseTime(".."), null);
      assert.strictEqual(snippets.parseTime("-1d"), null);
    });
    check("open interval is not a window", () =>
      assert.strictEqual(snippets.timeWindow(new URL("http://x/a?datetime=2026-01-01T00:00:00Z/..")), null));
  } finally {
    server.child.kill();
  }

  const skipped = Object.entries(interpreters).filter(([, ok]) => !ok).map(([k]) => k);
  console.log(`${checks} snippet checks passed, ${failures} failed` +
              (skipped.length ? ` (not run: ${skipped.join(", ")})` : ""));
  process.exit(failures ? 1 : 0);
}

main();
