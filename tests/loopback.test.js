const assert = require("assert");
const { WebSocket, WebSocketServer } = require("ws");

function waitFor(target, event) {
  return new Promise((resolve, reject) => {
    const cleanup = () => {
      target.off("error", onError);
      target.off(event, onEvent);
    };
    const onError = (error) => {
      cleanup();
      reject(error);
    };
    const onEvent = (...args) => {
      cleanup();
      resolve(args);
    };
    target.once("error", onError);
    target.once(event, onEvent);
  });
}

async function main() {
  const server = new WebSocketServer({ host: "127.0.0.1", port: 0 });
  await waitFor(server, "listening");

  server.on("connection", (socket) => {
    socket.on("message", (data, isBinary) => {
      socket.send(data, { binary: isBinary });
    });
  });

  const address = server.address();
  assert(address && typeof address.port === "number", "server exposes an ephemeral port");

  const client = new WebSocket(`ws://127.0.0.1:${address.port}`);
  await waitFor(client, "open");

  const textPromise = waitFor(client, "message");
  client.send("hello bbb.ws");
  const [textData, textIsBinary] = await textPromise;
  assert.strictEqual(textIsBinary, false, "text echo remains text");
  assert.strictEqual(textData.toString(), "hello bbb.ws", "text payload echoes exactly");

  const binaryPromise = waitFor(client, "message");
  client.send(Buffer.from([0, 1, 2, 255]));
  const [binaryData, binaryIsBinary] = await binaryPromise;
  assert.strictEqual(binaryIsBinary, true, "binary echo remains binary");
  assert.deepStrictEqual([...binaryData], [0, 1, 2, 255], "binary payload echoes exactly");

  client.close();
  await waitFor(client, "close");
  await new Promise((resolve) => server.close(resolve));

  console.log("bbb.ws loopback tests passed");
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
