#!/usr/bin/env python3
"""Exercise fragmented TCP PCM delivery through the real USB CoreAudio bridge."""
import os
from pathlib import Path
import re
import socket
import subprocess
import tempfile
import time
root = Path(__file__).resolve().parents[1]
bridge = root / 'dist/BepInEx/plugins/SilkSoftMac/HapticsBridge/HapticsBridge'
subprocess.run([str(bridge), '--self-test'], check=True)
with tempfile.TemporaryDirectory() as directory:
    process = subprocess.Popen([str(bridge), '--port', '48119', '--parent-pid', str(os.getpid())],
                               cwd=directory, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        for _ in range(100):
            try:
                connection = socket.create_connection(('127.0.0.1', 48119), timeout=0.2)
                break
            except OSError:
                if process.poll() is not None:
                    raise RuntimeError(process.communicate()[0].decode())
                time.sleep(0.05)
        else:
            raise RuntimeError('No USB DualSense available for integration test')
        connection.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        # Split each 10ms float32 stereo block across an unaligned byte boundary.
        block = bytes(3840)
        for _ in range(60):
            connection.sendall(block[:13])
            connection.sendall(block[13:])
            time.sleep(0.01)
        time.sleep(0.1)  # allow the output to drain before testing disconnect cleanup
        connection.close()
        time.sleep(0.1)
    finally:
        process.terminate()
        output = process.communicate(timeout=8)[0].decode()
    print(output)
    match = re.search(r'Received frames: (\d+); rendered frames: (\d+)', output)
    assert match and int(match[1]) == 28800 and int(match[2]) == 28800, output
    assert output.count('Game connected.') == 1, output
    print('PASS: fragmented PCM packets, sustained connection, CoreAudio consumption, clean shutdown.')
