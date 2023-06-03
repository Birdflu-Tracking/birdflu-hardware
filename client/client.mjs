#!/bin/node

import WebSocket from "ws";

if (process.argv.length < 3) {
  console.log("Usage: node client.mjs [option]");
  console.log("[options]\n -r To Read\n -w To Write\n -g To get tagId again");
  process.exit();
}

const ws = new WebSocket("ws://birdflu.local:81");
console.log("ONQWS4TBNIQGWYLQMRUQU===".length);

ws.on("open", () => {
  switch (process.argv[2]) {
    case "-r":
      ws.send("read");
      break;
    case "-w":
      ws.send("write");
      break;
    case "-g":
      ws.send("get-tagid");
      break;
  }
});

ws.on("message", (msg) => {
  console.log(`Server: ${msg}`);
});

ws.on("error", console.error);
