import fs from "node:fs";
import path from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const configPath = path.resolve(process.argv[2] ?? ".mcp.json");
const repoRoot = path.resolve(
  path.dirname(fileURLToPath(import.meta.url)),
  "../../..",
);
const config = JSON.parse(fs.readFileSync(configPath, "utf8"));
const server = config.mcpServers?.["blueprint-mcp"];
if (!server?.command || !Array.isArray(server.args)) {
  throw new Error("blueprint-mcp command/args are not configured");
}

const expandEnv = (value) => String(value).replace(
  /\$\{([A-Za-z_][A-Za-z0-9_]*)\}/g,
  (_, name) => {
    if (!(name in process.env)) {
      throw new Error(`environment variable ${name} is unavailable`);
    }
    return process.env[name];
  },
);

const sdkRoot = path.join(
  repoRoot,
  "Plugins/Local/BlueprintMCP/Tools/node_modules/@modelcontextprotocol/sdk/dist/esm",
);
const { Client } = await import(pathToFileURL(
  path.join(sdkRoot, "client/index.js"),
));
const { StdioClientTransport } = await import(pathToFileURL(
  path.join(sdkRoot, "client/stdio.js"),
));

const env = { ...process.env };
for (const [name, value] of Object.entries(server.env ?? {})) {
  env[name] = expandEnv(value);
}
const transport = new StdioClientTransport({
  command: expandEnv(server.command),
  args: server.args.map(expandEnv),
  cwd: server.cwd ? path.resolve(repoRoot, expandEnv(server.cwd)) : repoRoot,
  env,
  stderr: "pipe",
});
const stderrChunks = [];
transport.stderr?.on("data", (chunk) => {
  stderrChunks.push(chunk.toString());
  while (stderrChunks.join("").length > 8000) {
    stderrChunks.shift();
  }
});
const client = new Client(
  { name: "alis-engine-update-probe", version: "1.0.0" },
  { capabilities: {} },
);

try {
  await client.connect(transport);
  const result = await client.callTool(
    { name: "server_status", arguments: {} },
    undefined,
    { timeout: 180_000 },
  );
  const message = (result.content ?? [])
    .filter((item) => item.type === "text")
    .map((item) => item.text)
    .join("\n");
  if (result.isError || !message.includes("UE5 Blueprint server is running")) {
    const stderrTail = stderrChunks.join("").trim();
    const diagnostics = stderrTail ? `\nMCP server stderr:\n${stderrTail}` : "";
    throw new Error(
      `server_status did not prove protocol health: ${message}${diagnostics}`,
    );
  }
  process.stdout.write(`${message}\n`);
} finally {
  await client.close();
}
