const http = require("http");
const fs = require("fs/promises");
const path = require("path");
const { spawn } = require("child_process");

const HOST = process.env.HOST || "127.0.0.1";
const PORT = Number(process.env.PORT || 3000);
const FQBN = process.env.ARDUINO_FQBN || "arduino:avr:uno";

const ROOT = path.resolve(__dirname, "..");
const FRONTEND_FILE = path.join(ROOT, "frontend", "index.html");
const TEMPLATES_DIR = path.join(ROOT, "templates");
const GENERATED_ROOT = path.join(ROOT, "generated");

function toTemplateId(filename) {
  return path.basename(filename, path.extname(filename));
}

function uniqueStrings(items) {
  return [...new Set(items.filter((item) => typeof item === "string" && item.trim() !== ""))];
}

function extractPlaceholders(source) {
  const matches = source.match(/{{\s*[A-Z0-9_]+\s*}}/g) || [];
  const keys = matches.map((token) => token.replace(/[{}\s]/g, ""));
  return uniqueStrings(keys);
}

function parseTemplateMeta(source) {
  const match = source.match(/\/\*\s*@WEBARDU_META\s*([\s\S]*?)\*\//);
  if (!match) return null;

  try {
    return JSON.parse(match[1]);
  } catch {
    return null;
  }
}

function normalizeParamDefinition(raw) {
  if (!raw || typeof raw !== "object") return null;

  const key = typeof raw.key === "string" ? raw.key.trim().toUpperCase() : "";
  if (!key) return null;

  const type = typeof raw.type === "string" ? raw.type.trim().toLowerCase() : "text";
  const fallbackType = ["int", "float", "bool", "enum", "text"].includes(type) ? type : "text";
  const min = Number.isFinite(Number(raw.min)) ? Number(raw.min) : null;
  const max = Number.isFinite(Number(raw.max)) ? Number(raw.max) : null;
  const options = Array.isArray(raw.options) ? uniqueStrings(raw.options.map((item) => String(item))) : [];

  return {
    key,
    label: typeof raw.label === "string" && raw.label.trim() ? raw.label.trim() : key,
    type: fallbackType,
    description: typeof raw.description === "string" ? raw.description : "",
    default: raw.default,
    min,
    max,
    options
  };
}

function normalizeTemplate(templatePath, source) {
  const meta = parseTemplateMeta(source) || {};
  const inferredKeys = extractPlaceholders(source);
  const rawParams = Array.isArray(meta.params) ? meta.params : [];
  const metaParams = rawParams.map(normalizeParamDefinition).filter(Boolean);

  const mergedParamMap = new Map();
  for (const param of metaParams) {
    mergedParamMap.set(param.key, param);
  }

  for (const key of inferredKeys) {
    if (!mergedParamMap.has(key)) {
      mergedParamMap.set(key, {
        key,
        label: key,
        type: "text",
        description: "",
        default: "",
        min: null,
        max: null,
        options: []
      });
    }
  }

  const id = typeof meta.id === "string" && meta.id.trim() ? meta.id.trim() : toTemplateId(templatePath);

  return {
    id,
    filePath: templatePath,
    fileName: path.basename(templatePath),
    name: typeof meta.name === "string" && meta.name.trim() ? meta.name.trim() : id,
    description: typeof meta.description === "string" ? meta.description : "",
    source,
    params: [...mergedParamMap.values()]
  };
}

async function loadTemplates() {
  const entries = await fs.readdir(TEMPLATES_DIR, { withFileTypes: true });
  const files = entries
    .filter((entry) => entry.isFile() && path.extname(entry.name).toLowerCase() === ".ino")
    .map((entry) => path.join(TEMPLATES_DIR, entry.name))
    .sort();

  const templates = [];
  for (const filePath of files) {
    const source = await fs.readFile(filePath, "utf8");
    templates.push(normalizeTemplate(filePath, source));
  }

  return templates;
}

function chooseTemplate(templates, parsedBody) {
  const requestedId =
    parsedBody && typeof parsedBody.templateId === "string" ? parsedBody.templateId.trim() : "";

  if (requestedId) {
    return (
      templates.find((template) => template.id === requestedId || template.fileName === requestedId) || null
    );
  }

  return templates[0] || null;
}

function clampNumber(value, min, max) {
  let result = value;
  if (Number.isFinite(min) && result < min) result = min;
  if (Number.isFinite(max) && result > max) result = max;
  return result;
}

function normalizeParamValue(definition, rawValue) {
  const fallback = definition.default;

  if (definition.type === "bool") {
    if (typeof rawValue === "boolean") return rawValue ? "true" : "false";
    if (typeof rawValue === "number") return rawValue === 0 ? "false" : "true";
    const text = String(rawValue ?? "").trim().toLowerCase();
    if (["1", "true", "on", "yes"].includes(text)) return "true";
    if (["0", "false", "off", "no"].includes(text)) return "false";
    return fallback ? "true" : "false";
  }

  if (definition.type === "int") {
    const n = Number(rawValue);
    const picked = Number.isFinite(n) ? n : Number(fallback);
    const normalized = Number.isFinite(picked) ? Math.round(picked) : 0;
    return String(clampNumber(normalized, definition.min, definition.max));
  }

  if (definition.type === "float") {
    const n = Number(rawValue);
    const picked = Number.isFinite(n) ? n : Number(fallback);
    const normalized = Number.isFinite(picked) ? picked : 0;
    return String(clampNumber(normalized, definition.min, definition.max));
  }

  if (definition.type === "enum") {
    const value = String(rawValue ?? fallback ?? "");
    if (definition.options.includes(value)) return value;
    return definition.options[0] || value;
  }

  const text = String(rawValue ?? fallback ?? "");
  return text;
}

function escapeRegex(text) {
  return text.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function applyTemplate(template, providedParams) {
  const params = providedParams && typeof providedParams === "object" ? providedParams : {};
  const usedValues = {};
  let rendered = template.source;

  for (const definition of template.params) {
    const value = normalizeParamValue(definition, params[definition.key]);
    usedValues[definition.key] = value;
    const token = new RegExp(`{{\\s*${escapeRegex(definition.key)}\\s*}}`, "g");
    rendered = rendered.replace(token, value);
  }

  return { rendered, usedValues };
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

async function createSketch(template, params) {
  const { rendered, usedValues } = applyTemplate(template, params);

  const dirName = `${template.id}_sketch_${Date.now()}`;
  const sketchDir = path.join(GENERATED_ROOT, dirName);
  await fs.mkdir(sketchDir, { recursive: true });

  const sketchFile = path.join(sketchDir, `${dirName}.ino`);
  await fs.writeFile(sketchFile, rendered, "utf8");
  return { sketchDir, usedValues };
}

async function handleUpload(req, res) {
  const logs = [];

  try {
    const parsed = await parseJsonBody(req);
    const templates = await loadTemplates();
    const template = chooseTemplate(templates, parsed);
    if (!template) {
      sendJson(res, 400, {
        ok: false,
        message: "No template found in templates/. Add at least one .ino template.",
        logs
      });
      return;
    }

    const legacyParams =
      parsed && parsed.params && typeof parsed.params === "object"
        ? parsed.params
        : {
            INTERVAL: parsed.interval
          };

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

    logs.push(`Selected template: ${template.id} (${template.fileName})`);
    logs.push(`Selected board: ${selected.boardName}`);
    logs.push(`Selected fqbn: ${selected.fqbn}`);
    logs.push(`Selected port: ${selected.port}`);

    const { sketchDir, usedValues } = await createSketch(template, legacyParams);
    for (const [key, value] of Object.entries(usedValues)) {
      logs.push(`Param ${key}: ${value}`);
    }
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

async function handleTemplates(res) {
  try {
    const templates = await loadTemplates();
    const safe = templates.map((template) => ({
      id: template.id,
      fileName: template.fileName,
      name: template.name,
      description: template.description,
      params: template.params
    }));
    sendJson(res, 200, {
      ok: true,
      templates: safe,
      selected: safe[0] || null
    });
  } catch (err) {
    sendJson(res, 500, {
      ok: false,
      templates: [],
      selected: null,
      message: String(err && err.message ? err.message : err)
    });
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

  if (req.method === "GET" && req.url === "/templates") {
    await handleTemplates(res);
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
