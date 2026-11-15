#!/usr/bin/env python3
"""Independent wire-format, CRC and anti-aliasing checks for the Bluetooth encoder."""
from pathlib import Path
import ctypes as c
import math
import subprocess
import tempfile
import zlib
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    library = str(Path(directory) / 'BluetoothHaptics.dylib')
    subprocess.run(['xcrun','clang','-std=c11','-O2','-Wall','-Wextra','-Werror',
                    '-dynamiclib',str(root/'src/Bridge/BluetoothHaptics.c'),'-lz','-o',library],check=True)
    lib = c.CDLL(library)
    byte = c.c_uint8
    class Filter(c.Structure):
        _fields_ = [('taps',c.c_float*193),('history',(c.c_float*2)*193),('cursor',c.c_uint),('phase',c.c_uint)]
    lib.btEncodePcm.argtypes = [c.POINTER(byte),c.c_bool,byte,byte,c.POINTER(c.c_int8)]
    lib.btEncodePcm.restype = c.c_size_t
    lib.btEncodeControl.argtypes = [c.POINTER(byte),byte,c.c_bool]
    lib.btFilterInit.argtypes = [c.POINTER(Filter)]
    lib.btResample.argtypes = [c.POINTER(Filter),c.POINTER(c.c_float),c.c_size_t,c.POINTER(c.c_int8)]
    lib.btResample.restype = c.c_size_t
    signal = (c.c_int8*128)(*range(-64,64))
    def crc_valid(data):
        assert int.from_bytes(data[-4:],'little') == zlib.crc32(b'\xa2'+data[:-4])
    for first in (False,True):
        for sequence in (0,15,16,255):
            for counter in (0,2,254):
                packet = (byte*206)()
                length = lib.btEncodePcm(packet,first,sequence,counter,signal)
                data = bytes(packet[:length]); offset = 13 if first else 9
                assert length == (206 if first else 142)
                assert data[0] == (0x33 if first else 0x32)
                assert data[1] == (sequence%16)*16 and data[2] == 0x91
                assert data[10 if first else 6] == counter
                assert data[offset:offset+128] == bytes(signal)
                crc_valid(data)
    for enabled in (False,True):
        packet = (byte*78)(); lib.btEncodeControl(packet,15,enabled)
        assert bytes(packet[:3]) == b'\x31\xf0\x10'
        assert packet[3] & 3 == (0 if enabled else 3)
        crc_valid(bytes(packet))
    def resample(frequency, right=False):
        frames = 48000
        state = Filter(); lib.btFilterInit(c.byref(state))
        incoming = (c.c_float*(frames*2))()
        for i in range(frames): incoming[2*i+int(right)] = 0.5*math.sin(2*math.pi*frequency*i/48000)
        outgoing = (c.c_int8*(frames//16*2))()
        n = lib.btResample(c.byref(state),incoming,frames,outgoing)
        assert n == 3000
        assert all(outgoing[2*i+int(not right)] == 0 for i in range(n))
        return math.sqrt(sum((outgoing[2*i+int(right)]/127)**2 for i in range(100,n))/(n-100))
    left = resample(180); right = resample(180,True); alias = resample(6000)
    assert 0.33 < left < 0.37 and abs(left-right) < 0.001
    assert alias < 0.005
    # Arbitrarily fragmented input must preserve the decimation phase and frame count.
    state = Filter(); lib.btFilterInit(c.byref(state)); produced = 0
    for n in (7,9,1,15,17,31,192):
        incoming = (c.c_float*(2*n))(); outgoing = (c.c_int8*(2*(n//16+1)))()
        produced += lib.btResample(c.byref(state),incoming,n,outgoing)
    assert produced == sum((7,9,1,15,17,31,192))//16
    print('PASS: Bluetooth headers, sequence/counter wrap, independent CRC, stereo separation, 48k->3k rate, anti-aliasing, fragmented resampling.')
