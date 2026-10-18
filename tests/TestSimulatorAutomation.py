# SPDX-License-Identifier: MPL-2.0
"""Local socket integration check: python3 tests/TestSimulatorAutomation.py <binary>."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

with tempfile.TemporaryDirectory(prefix="oxr-test-") as directory:
    path = directory + "/control.sock"
    for _ in range(2):  # A normal shutdown must allow restarting at the same path.
        process = subprocess.Popen([sys.argv[1], path], stdout=subprocess.DEVNULL)
        try:
            for attempt in range(100):
                if os.path.exists(path):
                    break
                assert process.poll() is None
                time.sleep(.05)
            assert os.stat(path).st_mode & 0o777 == 0o600

            def request(value):
                with socket.socket(socket.AF_UNIX) as client:
                    client.settimeout(4)
                    client.connect(path)
                    client.sendall(json.dumps(value).encode() + b"\n")
                    return json.loads(client.makefile().readline())

            state = request({"command": "status"})["input"]
            state["left"]["trigger"] = 1
            assert request({"command": "set", "input": state})["ok"]
            assert request({"command": "status"})["input"]["left"]["trigger"] == 1
            bad = dict(state, buttons=0x80000000)
            assert not request({"command": "set", "input": bad})["ok"]
            assert not request({"command": "unknown"})["ok"]
            time.sleep(2.1)
            status = request({"command": "status"})
            assert status["released"]
            assert status["input"]["left"]["trigger"] == 0
            assert request({"command": "reset"})["ok"]
        finally:
            process.terminate()
            try:
                assert process.wait(timeout=5) == 0
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                raise
        assert not os.path.exists(path)
print("Socket validation, expiry, signal cleanup and restart passed.")
