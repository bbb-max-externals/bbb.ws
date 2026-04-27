// Test WebSocket client for verifying bbb.ws.server
//
// Usage: node client.js [url]
//   Defaults to ws://localhost:8080
//
// Behavior:
//   - Connects to the specified WebSocket server
//   - Sends a message every 3 seconds
//   - Logs all received messages
//   - Reconnects automatically on disconnect (after 2s)
//
// Test against bbb.ws.server:
//   1. In Max: [bbb.ws.server @port 8080]
//   2. Start this client:  node client.js
//   3. Watch Max right outlet: "connected <id>"
//   4. Watch Max left outlet:  "<id> hello N" every 3s
//   5. Send from Max: [send <id> ping] → client logs "received: ping"

const WebSocket = require("ws");

const url = process.argv[2] || "ws://localhost:8080";
let counter = 0;
let intervalId = null;

function connect() {
	console.log(`[client] connecting to ${url} ...`);
	const ws = new WebSocket(url);

	ws.on("open", () => {
		console.log("[client] connected");
		intervalId = setInterval(() => {
			counter++;
			const msg = `hello ${counter}`;
			ws.send(msg);
			console.log(`[client] sent: "${msg}"`);
		}, 3000);
	});

	ws.on("message", (data, isBinary) => {
		if(isBinary) {
			const bytes = Buffer.from(data);
			console.log(`[client] received binary (${bytes.length} bytes): [${[...bytes].join(", ")}]`);
		}
		else {
			console.log(`[client] received: "${data.toString()}"`);
		}
	});

	ws.on("close", (code, reason) => {
		console.log(`[client] disconnected: code=${code} reason="${reason.toString()}"`);
		if(intervalId) {
			clearInterval(intervalId);
			intervalId = null;
		}
		setTimeout(() => {
			console.log("[client] reconnecting ...");
			connect();
		}, 2000);
	});

	ws.on("error", (err) => {
		console.error(`[client] error: ${err.message}`);
	});
}

connect();

console.log("[client] started. press Ctrl+C to stop.");
