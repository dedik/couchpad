#!/usr/bin/env python3
"""Czyta log pilota z tematu pilot_debug/<id>/log przez websocket Home Assistanta.

Broker wymaga logowania, a HA ma już do niego dostęp — subskrypcja idzie przez
HA (komenda `mqtt/subscribe`, potrzebny token administratora).

Adres HA i token tylko ze zmiennych środowiskowych, nigdy z repozytorium:

    HA_URL=http://<adres-ha>:8123 HA_TOKEN=... uvx --with websockets python tools/pilot_log.py [id] [sekundy]

`id` to identyfikator pilota z ekranu MQTT (domyślnie `+`, czyli każdy pilot),
`sekundy` — jak długo słuchać (domyślnie bez końca).
"""

import asyncio
import json
import os
import sys
import time

import websockets


async def main() -> int:
    url = os.environ["HA_URL"].rstrip("/").replace("http", "ws", 1) + "/api/websocket"
    remote = sys.argv[1] if len(sys.argv) > 1 else "+"
    seconds = float(sys.argv[2]) if len(sys.argv) > 2 else None
    topic = f"pilot_debug/{remote}/log"

    async with websockets.connect(url, max_size=None) as ws:
        await ws.recv()
        await ws.send(json.dumps({"type": "auth", "access_token": os.environ["HA_TOKEN"]}))
        if json.loads(await ws.recv()).get("type") != "auth_ok":
            print("HA odrzucił token", file=sys.stderr)
            return 1
        await ws.send(json.dumps({"id": 1, "type": "mqtt/subscribe", "topic": topic}))
        print(f"# słucham {topic}", file=sys.stderr, flush=True)

        deadline = time.monotonic() + seconds if seconds else None
        while deadline is None or time.monotonic() < deadline:
            timeout = None if deadline is None else max(0.0, deadline - time.monotonic())
            try:
                message = json.loads(await asyncio.wait_for(ws.recv(), timeout))
            except asyncio.TimeoutError:
                break
            event = message.get("event")
            if message.get("type") == "event" and event:
                print(event.get("payload", ""), flush=True)
            elif message.get("success") is False:
                print(f"# błąd: {message.get('error')}", file=sys.stderr)
                return 1
    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
