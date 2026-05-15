"use strict";

const net = require("node:net");

const UPDATES = 64;
const BYTES_PER_PAYLOAD = 1_048_576;
const EXPECTED_CHECKSUM = 2_080;
const MODULUS = 1_000_000_007;

function runMirror() {
  return new Promise((resolve, reject) => {
    let checksum = 0;
    let updateCount = 0;
    let pending = Buffer.alloc(0);
    let settled = false;

    const finish = (error) => {
      if (settled) {
        return;
      }
      settled = true;
      server.close(() => {
        if (error) {
          reject(error);
        } else {
          resolve(checksum);
        }
      });
    };

    const server = net.createServer((socket) => {
      socket.on("data", (chunk) => {
        pending = Buffer.concat([pending, chunk]);
        while (pending.length >= 8 + BYTES_PER_PAYLOAD) {
          const revision = Number(pending.readBigUInt64LE(0));
          checksum = (checksum + revision) % MODULUS;
          pending = pending.subarray(8 + BYTES_PER_PAYLOAD);
          updateCount += 1;
          if (updateCount === UPDATES) {
            socket.end();
            finish(null);
            return;
          }
        }
      });
      socket.on("error", finish);
    });

    server.on("error", finish);
    server.listen(0, "127.0.0.1", () => {
      const address = server.address();
      const client = net.createConnection({ host: "127.0.0.1", port: address.port }, async () => {
        try {
          const payload = Buffer.alloc(BYTES_PER_PAYLOAD);
          let revision = 1;
          while (revision <= UPDATES) {
            const seed = revision & 0xff;
            let index = 0;
            while (index < payload.length) {
              payload[index] = (seed + (index & 0xff)) & 0xff;
              index += 4096;
            }
            const header = Buffer.alloc(8);
            header.writeBigUInt64LE(BigInt(revision), 0);
            if (!client.write(header)) {
              await new Promise((resume) => client.once("drain", resume));
            }
            if (!client.write(payload)) {
              await new Promise((resume) => client.once("drain", resume));
            }
            revision += 1;
          }
          client.end();
        } catch (error) {
          finish(error);
        }
      });
      client.on("error", finish);
    });
  });
}

(async () => {
  const checksum = await runMirror();
  if (checksum !== EXPECTED_CHECKSUM) {
    process.exit(1);
  }
})().catch(() => process.exit(1));
