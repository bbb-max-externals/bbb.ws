// Test WebSocket server for verifying bbb.ws.client
//
// Usage: node server.js [port]
//   Defaults to port 8080. Use 8443 for WSS (requires certs).
//
// Behavior:
//   - Echoes received text messages back to the sender
//   - Broadcasts a counter increment every 2 seconds
//   - Logs all connections/disconnections
//
// Test against bbb.ws.client:
//   1. Start this server:  node server.js
//   2. In Max: [bbb.ws.client @url ws://localhost:8080 @auto_connect 1]
//   3. Send from Max:      [hello] → server echoes → left outlet outputs "hello"
//   4. Watch left outlet:  server broadcasts counter every 2s

const { WebSocketServer } = require("ws");

const port = parseInt(process.argv[2] || "8080", 10);
const wss = new WebSocketServer({ port });

let counter = 0;

wss.on("listening", () => {
	console.log(`[server] listening on ws://localhost:${port}`);
});

wss.on("connection", (ws, req) => {
	const addr = req.socket.remoteAddress;
	console.log(`[server] client connected: ${addr}`);

	ws.on("message", (data, isBinary) => {
		if(isBinary) {
			const bytes = Buffer.from(data);
			console.log(`[server] binary (${bytes.length} bytes): [${[...bytes].join(", ")}]`);
			ws.send(data, { binary: true });
		}
		else {
			const text = data.toString();
			console.log(`[server] text: "${text}"`);
			ws.send(`echo: ${text}`);
		}
	});

	ws.on("close", (code, reason) => {
		console.log(`[server] client disconnected: code=${code} reason="${reason.toString()}"`);
	});

	ws.on("error", (err) => {
		console.error(`[server] error: ${err.message}`);
	});
});

setInterval(() => {
	counter++;
	const msg = `tick ${counter}`;
	for(const ws of wss.clients) {
		if(ws.readyState === 1) {
			ws.send(msg);
		}
	}
}, 2000);

console.log("[server] started. press Ctrl+C to stop.");
