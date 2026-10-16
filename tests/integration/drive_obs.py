#!/usr/bin/env python3
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
"""drive_obs.py: play a recording scenario on a running OBS, through obs-websocket.

    drive_obs.py --port 4466 --log <obs log file> --scenario standard

It presses the plugin's hotkeys at fixed times (exactly what a user's keys do), then
waits until the plugin reports that the file is saved. The plugin's own log lines tell
the analysis where each section really started, so timing jitter here does not matter.

Scenarios (seconds from the start of the recording):
    standard  about 100 s: x4, x8, 20 quick x2 switches, x16, real time in between.
    long      about 300 s: the standard pattern three times (load test, 4K).
    real      25 s in real time, no switch.
Standard and long end with 4 s at x4, so stopping is also tested in timelapse (the
encoder still holds frames that must reach the file).
"""

import argparse
import json
import sys
import time

import websocket  # websocket-client, pinned in tools/requirements-dev.txt

HOTKEY = "TimelapseSwitch."

# (seconds after the start, hotkey) for one pass of the standard pattern; ends in real time.
STANDARD = (
    [(10, "Speed4"), (30, "RealTime"), (40, "Speed8"), (56, "RealTime")]
    + [(64 + i, "Speed2" if i % 2 == 0 else "RealTime") for i in range(20)]
    + [(84, "Speed16"), (92, "RealTime")]
)
STANDARD_LENGTH = 100


class RequestFailed(RuntimeError):
    def __init__(self, kind: str, status: dict):
        super().__init__(f"{kind} failed: {status}")
        self.code = status.get("code")


NOT_READY = 207  # obs-websocket: "OBS is not ready to perform the request" (still starting)


class Obs:
    """The smallest obs-websocket 5 client that does the job: identify, then requests."""

    def __init__(self, port: int):
        self.socket = websocket.create_connection(f"ws://127.0.0.1:{port}", timeout=10)
        hello = json.loads(self.socket.recv())
        if hello.get("op") != 0:
            raise RuntimeError(f"unexpected first message: {hello}")
        self.socket.send(json.dumps({"op": 1, "d": {"rpcVersion": 1, "eventSubscriptions": 0}}))
        if json.loads(self.socket.recv()).get("op") != 2:
            raise RuntimeError("obs-websocket did not accept the connection")
        self.next_id = 0

    def request(self, kind: str, data: dict | None = None) -> dict:
        """Send one request and return its response data; raise if OBS reports a failure."""
        self.next_id += 1
        message = {"op": 6, "d": {"requestType": kind, "requestId": str(self.next_id), "requestData": data or {}}}
        self.socket.send(json.dumps(message))
        while True:
            reply = json.loads(self.socket.recv())
            if reply.get("op") == 7 and reply["d"]["requestId"] == str(self.next_id):
                status = reply["d"]["requestStatus"]
                if not status["result"]:
                    raise RequestFailed(kind, status)
                return reply["d"].get("responseData", {})

    def press(self, hotkey: str) -> None:
        self.request("TriggerHotkeyByName", {"hotkeyName": HOTKEY + hotkey})


def connect(port: int, wait_s: float) -> Obs:
    """OBS takes a few seconds to start; retry until obs-websocket answers and OBS is ready."""
    deadline = time.monotonic() + wait_s
    while True:
        try:
            obs = Obs(port)
            obs.request("GetHotkeyList")
            return obs
        except (OSError, RequestFailed) as error:
            if time.monotonic() > deadline or (isinstance(error, RequestFailed) and error.code != NOT_READY):
                raise
            time.sleep(0.5)


def scenario(name: str) -> tuple[list, int]:
    if name == "real":
        return [], 25
    passes = 1 if name == "standard" else 3
    schedule = [(t + p * STANDARD_LENGTH, key) for p in range(passes) for t, key in STANDARD]
    length_s = passes * STANDARD_LENGTH
    return schedule + [(length_s - 4, "Speed4")], length_s


def wait_for_log(path: str, text: str, timeout_s: float) -> str:
    """Wait until the OBS log contains a line with text; return that line."""
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        with open(path, encoding="utf-8", errors="replace") as log:
            for line in log:
                if text in line:
                    return line.strip()
        time.sleep(0.5)
    raise TimeoutError(f"no log line containing {text!r} after {timeout_s} s")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--log", required=True, help="OBS log file of this run")
    parser.add_argument("--scenario", choices=("standard", "long", "real"), default="standard")
    args = parser.parse_args()

    obs = connect(args.port, wait_s=60)
    names = {hotkey for hotkey in obs.request("GetHotkeyList")["hotkeys"] if hotkey.startswith(HOTKEY)}
    needed = {HOTKEY + key for key in ("Record", "Stop", "RealTime", "Speed2", "Speed4", "Speed8", "Speed16")}
    if not needed <= names:
        print(f"FAIL hotkeys: missing {sorted(needed - names)}")
        return 1

    schedule, length_s = scenario(args.scenario)
    obs.press("Record")
    start = time.monotonic()
    print(f"recording for {length_s} s with {len(schedule)} switches")
    for at_s, key in schedule + [(length_s, "Stop")]:
        time.sleep(max(0.0, start + at_s - time.monotonic()))
        obs.press(key)
    print(wait_for_log(args.log, "[timelapse-switch] recording saved", timeout_s=120))
    return 0


if __name__ == "__main__":
    sys.exit(main())
