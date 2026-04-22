const http = require("http");
const fs = require("fs/promises");
const path = require("path");
const { spawn } = require("child_process");

const HOST = process.env.HOST || "127.0.0.1";
const PORT = Number(process.env.PORT || 3000);
const FQBN = process.env.ARDUINO_FQBN || "arduino:avr:uno";

const ROOT = path.resolve(__dirname, "..");
const FRONTEND_FILE = path.join(ROOT, "frontend", "index.html");
const TEMPLATE_FILE = path.join(ROOT, "templates", "blink.ino");
const GENERATED_ROOT = path.join(ROOT, "generated");

function clampInterval(value) {
  const fallback = 500;
  if (!Number.isFinite(value)) {
    return fallback;
  }

  const n = Math.round(value);
  if (n < 50) return 50;
  if (n > 5000) return 5000;
  return n;
}

function runCommand(command, args, workdir, logs) {
  return new Promise((resolve, reject) => {
    logs.push(`$ ${command} ${args.join(" ")}`);

    const proc = spawn(command, args, {
      cwd: workdir,
      stdio: ["ignore", "pipe", "pipe"]
    });

    proc.stdout.on("data", (chunk) => {
      logs.push(chunk.toString().trimEnd());
    });

    proc.stderr.on("data", (chunk) => {
      logs.push(chunk.toString().trimEnd());
    });

    proc.on("error", (err) => {
      reject(err);
    });

    proc.on("close", (code) => {
      if (code === 0) {
        resolve();
        return;
      }

      reject(new Error(`Command failed with exit code ${code}`));
    });
  });
}

function runCommandCapture(command, args, workdir, logs) {
  return new Promise((resolve, reject) => {
    logs.push(`$ ${command} ${args.join(" ")}`);

    const proc = spawn(command, args, {
      cwd: workdir,
      stdio: ["ignore", "pipe", "pipe"]
    });

    let stdout = "";
    let stderr = "";

    proc.stdout.on("data", (chunk) => {
      const text = chunk.toString();
      stdout += text;
      logs.push(text.trimEnd());
    });

    proc.stderr.on("data", (chunk) => {
      const text = chunk.toString();
      stderr += text;
      logs.push(text.trimEnd());
    });

    proc.on("error", (err) => {
      reject(err);
    });

    proc.on("close", (code) => {
      if (code === 0) {
        resolve({ stdout, stderr });
        return;
      }

      reject(new Error(`Command failed with exit code ${code}`));
    });
  });
}

function normalizeBoards(raw) {
  const ports = Array.isArray(raw)
    ? raw
    : Array.isArray(raw.detected_ports)
      ? raw.detected_ports
      : [];

  const result = [];

  for (const item of ports) {
    const port = item && item.port && item.port.address ? item.port.address : item.address;
    if (!port) continue;

    const matches = Array.isArray(item.matching_boards) ? item.matching_boards : [];
    const board = matches.find((b) => b && b.fqbn) || matches[0] || null;

    if (!board || !board.fqbn) continue;

    result.push({
      port,
      fqbn: board.fqbn,
      boardName: board.name || "Unknown board"
    });
  }

  return result;
}

async function detectBoards(logs) {
  const output = await runCommandCapture(
    "arduino-cli",
    ["board", "list", "--format", "json"],
    ROOT,
    logs
  );
  const parsed = JSON.parse(output.stdout || "[]");
  return normalizeBoards(parsed);
}

function pickBoard(boards, parsedBody) {
  const requestedPort =
    parsedBody && typeof parsedBody.port === "string" ? parsedBody.port.trim() : "";
  const requestedFqbn =
    parsedBody && typeof parsedBody.fqbn === "string" ? parsedBody.fqbn.trim() : "";

  if (requestedPort || requestedFqbn) {
    return (
      boards.find((board) => {
        const samePort = requestedPort ? board.port === requestedPort : true;
        const sameFqbn = requestedFqbn ? board.fqbn === requestedFqbn : true;
        return samePort && sameFqbn;
      }) || null
    );
  }

  return boards[0] || null;
}

async function parseJsonBody(req) {
  const body = await new Promise((resolve, reject) => {
    let raw = "";

    req.on("data", (chunk) => {
      raw += chunk;
    });

    req.on("end", () => resolve(raw));
    req.on("error", reject);
  });

  return body ? JSON.parse(body) : {};
}

async function createSketch(interval) {
  const template = await fs.readFile(TEMPLATE_FILE, "utf8");
  const source = template.replace("{{INTERVAL}}", String(interval));

  const dirName = `sketch_${Date.now()}`;
  const sketchDir = path.join(GENERATED_ROOT, dirName);
  await fs.mkdir(sketchDir, { recursive: true });

  const sketchFile = path.join(sketchDir, `${dirName}.ino`);
  await fs.writeFile(sketchFile, source, "utf8");
  return sketchDir;
}

async function handleUpload(req, res) {
  const logs = [];

  try {
    const parsed = await parseJsonBody(req);
    const interval = clampInterval(Number(parsed.interval));
    const boards = await detectBoards(logs);
    const selected = pickBoard(boards, parsed);

    if (!selected) {
      sendJson(res, 400, {
        ok: false,
        message: "No supported board detected. Connect board and retry.",
        boards,
        logs
      });
      return;
    }

    logs.push(`Interval used: ${interval}ms`);
    logs.push(`Selected board: ${selected.boardName}`);
    logs.push(`Selected fqbn: ${selected.fqbn}`);
    logs.push(`Selected port: ${selected.port}`);

    const sketchDir = await createSketch(interval);
    logs.push(`Generated sketch: ${sketchDir}`);

    await runCommand("arduino-cli", ["compile", "--fqbn", selected.fqbn, sketchDir], ROOT, logs);

    await runCommand(
      "arduino-cli",
      ["upload", "-p", selected.port, "--fqbn", selected.fqbn, sketchDir],
      ROOT,
      logs
    );

    sendJson(res, 200, { ok: true, message: "Compile + upload success", logs });
  } catch (err) {
    logs.push(String(err && err.message ? err.message : err));
    sendJson(res, 500, { ok: false, message: "Compile/Upload failed", logs });
  }
}

async function handleBoards(res) {
  const logs = [];

  try {
    const boards = await detectBoards(logs);
    sendJson(res, 200, {
      ok: true,
      boards,
      selected: boards[0] || null
    });
  } catch (err) {
    logs.push(String(err && err.message ? err.message : err));
    sendJson(res, 500, { ok: false, boards: [], selected: null, logs });
  }
}

function sendJson(res, status, payload) {
  const json = JSON.stringify(payload);
  res.writeHead(status, {
    "Content-Type": "application/json; charset=utf-8",
    "Content-Length": Buffer.byteLength(json)
  });
  res.end(json);
}

function sendHtml(res, html) {
  res.writeHead(200, {
    "Content-Type": "text/html; charset=utf-8",
    "Content-Length": Buffer.byteLength(html)
  });
  res.end(html);
}

const server = http.createServer(async (req, res) => {
  if (!req.url || !req.method) {
    res.writeHead(400);
    res.end("Bad Request");
    return;
  }

  if (req.method === "GET" && req.url === "/") {
    try {
      const html = await fs.readFile(FRONTEND_FILE, "utf8");
      sendHtml(res, html);
    } catch (err) {
      res.writeHead(500);
      res.end(String(err));
    }
    return;
  }

  if (req.method === "POST" && req.url === "/upload") {
    await handleUpload(req, res);
    return;
  }

  if (req.method === "GET" && req.url === "/boards") {
    await handleBoards(res);
    return;
  }

  res.writeHead(404);
  res.end("Not Found");
});

server.listen(PORT, HOST, () => {
  console.log(`MatterMatters - WEBARDU running at http://${HOST}:${PORT}`);
  console.log(`FQBN: ${FQBN}`);
  console.log("Upload target: auto detect connected board");
});
