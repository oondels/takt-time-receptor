const http = require("http");
const fs = require("fs");
const path = require("path");

const PORT = Number(process.env.PORT || 2399);
const firmwarePath = path.resolve(process.env.FIRMWARE_PATH || "./firmware.bin");

function sendNotFound(res)
{
  res.statusCode = 404;
  res.setHeader("Content-Type", "application/json");
  res.end(JSON.stringify({ ok: false, error: "not_found" }));
}

function sendMethodNotAllowed(res)
{
  res.statusCode = 405;
  res.setHeader("Content-Type", "application/json");
  res.end(JSON.stringify({ ok: false, error: "method_not_allowed" }));
}

function sendFirmware(res)
{
  fs.stat(firmwarePath, (statErr, stats) =>
  {
    if (statErr || !stats.isFile())
    {
      res.statusCode = 404;
      res.setHeader("Content-Type", "application/json");
      res.end(JSON.stringify({ ok: false, error: "firmware_not_found" }));
      return;
    }

    res.statusCode = 200;
    res.setHeader("Content-Type", "application/octet-stream");
    res.setHeader("Content-Length", stats.size);

    const stream = fs.createReadStream(firmwarePath);
    stream.on("error", () =>
    {
      if (!res.headersSent)
      {
        res.statusCode = 500;
        res.setHeader("Content-Type", "application/json");
      }
      res.end(JSON.stringify({ ok: false, error: "stream_error" }));
    });

    stream.pipe(res);
  });
}

const server = http.createServer((req, res) =>
{
  if (req.url !== "/update-takttime")
  {
    sendNotFound(res);
    return;
  }

  if (req.method !== "GET")
  {
    sendMethodNotAllowed(res);
    return;
  }

  sendFirmware(res);
});

server.listen(PORT, () =>
{
  console.log(`OTA firmware server listening on port ${PORT}`);
  console.log(`Firmware path: ${firmwarePath}`);
});
