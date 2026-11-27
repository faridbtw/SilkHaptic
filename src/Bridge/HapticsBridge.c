/* macOS CoreAudio backend for Silksong PS5 Haptics. MIT; see LICENSE. */
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>
#include <IOKit/hid/IOHIDManager.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdatomic.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <mach/mach_time.h>
#include "BluetoothHaptics.h"

#define RATE 48000
#define CAPACITY 8192
#define LIGHTBAR_INACTIVE UINT32_MAX
#define LIGHTBAR_IDLE_BLUE 0x0020ffu
#define MAX_QUEUE 1920 /* discard backlog above 40ms, keep fresh haptics */
static float ring[CAPACITY][2];
static size_t head, count;
static pthread_mutex_t ringMutex = PTHREAD_MUTEX_INITIALIZER;
static atomic_bool running = true, outputReady = false;
static atomic_int peer = -1;
static atomic_uint lightbarColor = LIGHTBAR_INACTIVE;
static uint32_t usbLightbar = LIGHTBAR_INACTIVE;
static volatile sig_atomic_t interrupted;
static unsigned long long rendered, underruns, received, dropped;
static uint64_t bluetoothMaxWriteNs, bluetoothReports;
static FILE *logFile;
static int bridgePort = 48111, parentPid;
static AudioUnit output;
static AudioDeviceID currentDevice;
static IOHIDManagerRef hidManager;
static IOHIDDeviceRef bluetoothDevice;
static pthread_t bluetoothThread;
static bool bluetoothThreadCreated;
static atomic_bool bluetoothRunning, bluetoothFailed;
static const char *transportPreference = "auto";
static void closeBluetooth(void);
static bool sendUsbOutput(bool audio, uint32_t lightbar);

static void logline(const char *s) {
    fprintf(stderr, "%s\n", s);
    if (logFile) { fprintf(logFile, "%lld %s\n", (long long)time(NULL), s); fflush(logFile); }
}
static void sigHandler(int s) { (void)s; interrupted = 1; }
static void clearRing(void) {
    pthread_mutex_lock(&ringMutex); head = count = 0; pthread_mutex_unlock(&ringMutex);
}
static void enqueue(const float *data, size_t frames) {
    pthread_mutex_lock(&ringMutex);
    if (frames > MAX_QUEUE) { data += 2*(frames-MAX_QUEUE); frames = MAX_QUEUE; }
    if (count + frames > MAX_QUEUE) {
        size_t stale = count + frames - MAX_QUEUE;
        dropped += stale; head = (head + stale) % CAPACITY; count -= stale;
    }
    for (size_t i = 0; i < frames; i++) {
        size_t at = (head + count) % CAPACITY;
        for (int c = 0; c < 2; c++) {
            float v = data[2*i+c];
            ring[at][c] = isfinite(v) ? fmaxf(-1, fminf(1, v)) : 0;
        }
        if (count < CAPACITY) count++;
    }
    pthread_mutex_unlock(&ringMutex);
}
/* Never wait, allocate, log, or access devices on the realtime audio thread. */
static OSStatus render(void *ctx, AudioUnitRenderActionFlags *flags,
                       const AudioTimeStamp *ts, UInt32 bus, UInt32 frames, AudioBufferList *bufs) {
    (void)ctx; (void)flags; (void)ts; (void)bus;
    for (UInt32 i = 0; i < bufs->mNumberBuffers; i++)
        if (bufs->mBuffers[i].mData) memset(bufs->mBuffers[i].mData, 0, bufs->mBuffers[i].mDataByteSize);
    if (bufs->mNumberBuffers != 1 || bufs->mBuffers[0].mNumberChannels != 4 || !bufs->mBuffers[0].mData) return noErr;
    if (bufs->mBuffers[0].mDataByteSize < frames * 4 * sizeof(float)) return noErr;
    if (pthread_mutex_trylock(&ringMutex) != 0) return noErr;
    float *out = bufs->mBuffers[0].mData;
    size_t n = count < frames ? count : frames;
    for (size_t i = 0; i < n; i++) {
        out[i*4+2] = ring[head][0]; out[i*4+3] = ring[head][1];
        head = (head + 1) % CAPACITY;
    }
    count -= n; rendered += n;
    if (n < frames) underruns++;
    pthread_mutex_unlock(&ringMutex);
    return noErr;
}
static UInt32 outputChannels(AudioDeviceID dev) {
    AudioObjectPropertyAddress a = { kAudioDevicePropertyStreamConfiguration, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMain };
    UInt32 size = 0, channels = 0;
    if (AudioObjectGetPropertyDataSize(dev, &a, 0, NULL, &size) != noErr) return 0;
    AudioBufferList *b = malloc(size);
    if (!b) return 0;
    if (AudioObjectGetPropertyData(dev, &a, 0, NULL, &size, b) == noErr)
        for (UInt32 i = 0; i < b->mNumberBuffers; i++) channels += b->mBuffers[i].mNumberChannels;
    free(b); return channels;
}
static AudioDeviceID findDevice(int list) {
    AudioObjectPropertyAddress a = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &a, 0, NULL, &size) != noErr) return 0;
    AudioDeviceID *devices = malloc(size); if (!devices) return 0;
    AudioDeviceID found = 0;
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &size, devices) != noErr) { free(devices); return 0; }
    for (UInt32 i = 0; i < size / sizeof(AudioDeviceID); i++) {
        CFStringRef name = NULL;
        AudioObjectPropertyAddress n = { kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
        UInt32 ns = sizeof(name); char text[512] = "";
        if (AudioObjectGetPropertyData(devices[i], &n, 0, NULL, &ns, &name) == noErr && name) {
            CFStringGetCString(name, text, sizeof(text), kCFStringEncodingUTF8); CFRelease(name);
        }
        UInt32 channels = outputChannels(devices[i]);
        if (list) printf("%u: %s (%u output channels)\n", devices[i], text, channels);
        /* Never send actuator audio to the default output or a similarly named virtual device. */
        AudioObjectPropertyAddress t = { kAudioDevicePropertyTransportType, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
        UInt32 transport = 0, sz = sizeof(transport);
        AudioObjectGetPropertyData(devices[i], &t, 0, NULL, &sz, &transport);
        if (!found && channels >= 4 && transport == kAudioDeviceTransportTypeUSB &&
            (strstr(text, "DualSense") || strstr(text, "Wireless Controller"))) found = devices[i];
    }
    free(devices); return found;
}
static int openAudio(AudioDeviceID dev) {
    AudioComponentDescription desc = { kAudioUnitType_Output, kAudioUnitSubType_HALOutput, kAudioUnitManufacturer_Apple, 0, 0 };
    AudioComponent component = AudioComponentFindNext(NULL, &desc);
    OSStatus status = component ? AudioComponentInstanceNew(component, &output) : -1;
    if (status != noErr) goto fail;
    UInt32 off = 0, on = 1;
    status = AudioUnitSetProperty(output, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, &off, sizeof(off));
    if (status != noErr) goto fail;
    status = AudioUnitSetProperty(output, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0, &on, sizeof(on));
    if (status != noErr) goto fail;
    status = AudioUnitSetProperty(output, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &dev, sizeof(dev));
    if (status != noErr) goto fail;
    AudioStreamBasicDescription format = { .mSampleRate = RATE, .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagsNativeFloatPacked, .mBytesPerPacket = 16, .mFramesPerPacket = 1,
        .mBytesPerFrame = 16, .mChannelsPerFrame = 4, .mBitsPerChannel = 32 };
    status = AudioUnitSetProperty(output, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &format, sizeof(format));
    if (status != noErr) goto fail;
    SInt32 map[4] = { 0, 1, 2, 3 };
    status = AudioUnitSetProperty(output, kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Output, 0, map, sizeof(map));
    if (status != noErr) goto fail;
    AURenderCallbackStruct callback = { render, NULL };
    status = AudioUnitSetProperty(output, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &callback, sizeof(callback));
    if (status != noErr) goto fail;
    status = AudioUnitInitialize(output); if (status != noErr) goto fail;
    clearRing();
    status = AudioOutputUnitStart(output); if (status != noErr) goto fail;
    currentDevice = dev; atomic_store(&outputReady, true);
    logline("CoreAudio: USB DualSense ready, 48 kHz; channels 3/4 = actuators, 1/2 = silence.");
    return 1;
fail:
    { char msg[128]; snprintf(msg, sizeof(msg), "CoreAudio open failed: OSStatus %d", (int)status); logline(msg); }
    if (output) { AudioComponentInstanceDispose(output); output = NULL; }
    return 0;
}
static void closeAudio(void) {
    closeBluetooth();
    atomic_store(&outputReady, false);
    int p = atomic_load(&peer); if (p >= 0) shutdown(p, SHUT_RDWR);
    if (output) { AudioOutputUnitStop(output); AudioUnitUninitialize(output); AudioComponentInstanceDispose(output); output = NULL; }
    if (currentDevice && usbLightbar != LIGHTBAR_INACTIVE)
        sendUsbOutput(false, LIGHTBAR_IDLE_BLUE);
    usbLightbar = LIGHTBAR_INACTIVE;
    currentDevice = 0; clearRing();
}
static long hidNumber(IOHIDDeviceRef dev, CFStringRef key) {
    CFTypeRef value = IOHIDDeviceGetProperty(dev, key); long n = 0;
    if (value && CFGetTypeID(value) == CFNumberGetTypeID()) CFNumberGetValue(value, kCFNumberLongType, &n);
    return n;
}
static void ensureHidManager(void) {
    if (hidManager) return;
    hidManager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    int vendor = 0x054c;
    CFNumberRef number = CFNumberCreate(NULL, kCFNumberIntType, &vendor);
    const void *keys[] = { CFSTR(kIOHIDVendorIDKey) }, *values[] = { number };
    CFDictionaryRef match = CFDictionaryCreate(NULL, keys, values, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    IOHIDManagerSetDeviceMatching(hidManager, match);
    CFRelease(match); CFRelease(number);
    IOHIDManagerScheduleWithRunLoop(hidManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDManagerOpen(hidManager, kIOHIDOptionsTypeNone);
}
static void resetHidManager(void) {
    /* A manager opened while USB was attached can retain a stale device set
     * after macOS replaces that service with its Bluetooth HID service. Renew
     * discovery only while no Bluetooth writer owns a device. */
    if (!hidManager) return;
    IOHIDManagerUnscheduleFromRunLoop(hidManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDManagerClose(hidManager, kIOHIDOptionsTypeNone);
    CFRelease(hidManager); hidManager = NULL;
}
static bool sendUsbOutput(bool audio, uint32_t lightbar) {
    ensureHidManager();
    CFSetRef devices = IOHIDManagerCopyDevices(hidManager); if (!devices) return false;
    CFIndex n = CFSetGetCount(devices); IOHIDDeviceRef *values = calloc(n, sizeof(*values));
    if (!values) { CFRelease(devices); return false; }
    bool sent = false;
    CFSetGetValues(devices, (const void **)values);
    for (CFIndex i = 0; i < n; i++) {
        IOHIDDeviceRef dev = values[i]; long pid = hidNumber(dev, CFSTR(kIOHIDProductIDKey));
        if (hidNumber(dev, CFSTR(kIOHIDVendorIDKey)) != 0x054c || (pid != 0x0ce6 && pid != 0x0df2)) continue;
        CFTypeRef transport = IOHIDDeviceGetProperty(dev, CFSTR(kIOHIDTransportKey));
        if (!transport || CFGetTypeID(transport) != CFStringGetTypeID() || CFStringCompare(transport, CFSTR("USB"), 0) != kCFCompareEqualTo) continue;
        uint8_t report[48] = { 0x02 };
        if (audio) {
            report[1] = 0xb0; report[2] = 0x80;
            report[5] = 0x7f; report[6] = 0xff;
        }
        if (lightbar != LIGHTBAR_INACTIVE) {
            report[2] |= 0x04;
            report[45] = lightbar >> 16; report[46] = lightbar >> 8; report[47] = lightbar;
        }
        IOReturn status = IOHIDDeviceOpen(dev, kIOHIDOptionsTypeNone);
        if (status == kIOReturnSuccess) {
            status = IOHIDDeviceSetReport(dev, kIOHIDReportTypeOutput, 0x02, report, sizeof(report));
            IOHIDDeviceClose(dev, kIOHIDOptionsTypeNone);
        }
        if (status == kIOReturnSuccess) sent = true;
        static IOReturn previous = -1;
        if (audio && status != previous) {
            char msg[128]; snprintf(msg, sizeof(msg), "HID audio enable: 0x%x%s", status,
                status == kIOReturnSuccess ? " (success)" : " (check controller access / Steam Input)");
            logline(msg); previous = status;
        }
    }
    free(values); CFRelease(devices); return sent;
}
static IOHIDDeviceRef findBluetooth(int list) {
    ensureHidManager();
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, true);
    CFSetRef devices = IOHIDManagerCopyDevices(hidManager);
    if (!devices) return NULL;
    CFIndex count = CFSetGetCount(devices);
    IOHIDDeviceRef *values = calloc(count, sizeof(*values)), found = NULL;
    if (!values) { CFRelease(devices); return NULL; }
    CFSetGetValues(devices, (const void **)values);
    for (CFIndex i = 0; i < count; i++) {
        IOHIDDeviceRef dev = values[i];
        long pid = hidNumber(dev, CFSTR(kIOHIDProductIDKey));
        if (pid != 0x0ce6 && pid != 0x0df2) continue;
        CFTypeRef transport = IOHIDDeviceGetProperty(dev, CFSTR(kIOHIDTransportKey));
        if (!transport || CFGetTypeID(transport) != CFStringGetTypeID() ||
            !CFStringHasPrefix(transport, CFSTR("Bluetooth"))) continue;
        if (list) printf("Bluetooth: %s (HID haptic streaming)\n", pid == 0x0df2 ? "DualSense Edge" : "DualSense");
        if (!found) { found = dev; CFRetain(found); }
    }
    free(values); CFRelease(devices); return found;
}
static bool sendBluetooth(const uint8_t *packet, size_t length) {
    struct timespec before, after;
    clock_gettime(CLOCK_MONOTONIC, &before);
    IOReturn status = IOHIDDeviceSetReport(bluetoothDevice, kIOHIDReportTypeOutput, packet[0], packet, length);
    clock_gettime(CLOCK_MONOTONIC, &after);
    uint64_t elapsed = (uint64_t)((after.tv_sec-before.tv_sec)*1000000000LL + after.tv_nsec-before.tv_nsec);
    if (elapsed > bluetoothMaxWriteNs) bluetoothMaxWriteNs = elapsed;
    ++bluetoothReports;
    if (status == kIOReturnSuccess) return true;
    char msg[128]; snprintf(msg, sizeof(msg), "Bluetooth HID report 0x%02x failed: 0x%x", packet[0], status);
    logline(msg); return false;
}
static void *bluetoothWriter(void *unused) {
    (void)unused;
    BtResampler filter; btFilterInit(&filter);
    uint8_t sequence = 0, counter = 0;
    uint64_t packetNumber = 0, ticks = 0, nonzeroSamples = 0;
    bool streaming = false; unsigned silentPackets = 0;
    uint8_t packet[206];
    bool lightbarSetup = false;
    uint32_t appliedLightbar = LIGHTBAR_INACTIVE;
    uint64_t nextLightbarAttempt = 0;
    mach_timebase_info_data_t tb; mach_timebase_info(&tb);
    double ticksPerSecond = 1e9 * tb.denom / tb.numer;
    uint64_t start = mach_absolute_time() + (uint64_t)(0.030 * ticksPerSecond);
    while (atomic_load(&bluetoothRunning)) {
        uint64_t due = start + (uint64_t)((ticks * (64.0 / 3000)) * ticksPerSecond);
        uint64_t now = mach_absolute_time();
        if (due > now) mach_wait_until(due);
        else if (now - due > (uint64_t)(0.050 * ticksPerSecond)) {
            /* Do not burst stale packets after system scheduling stalls. */
            start += now - due; clearRing();
        }
        if (!atomic_load(&bluetoothRunning)) break;
        float source[BT_INPUT_FRAMES * 2] = {0};
        pthread_mutex_lock(&ringMutex);
        size_t n = count < BT_INPUT_FRAMES ? count : BT_INPUT_FRAMES;
        for (size_t i = 0; i < n; ++i) {
            source[2*i] = ring[head][0]; source[2*i+1] = ring[head][1];
            head = (head + 1) % CAPACITY;
        }
        count -= n; rendered += n;
        if (n < BT_INPUT_FRAMES) ++underruns;
        pthread_mutex_unlock(&ringMutex);
        int8_t pcm[128];
        size_t frames = btResample(&filter, source, BT_INPUT_FRAMES, pcm);
        if (frames != BT_HAPTIC_FRAMES) { logline("Bluetooth resampler frame count error."); goto failed; }
        bool nonzero = false;
        for (unsigned i = 0; i < 128; ++i) if (pcm[i]) { ++nonzeroSamples; nonzero = true; }
        ++ticks;
        silentPackets = nonzero ? 0 : silentPackets + (silentPackets < 8);
        /* Idle periods send no HID reports. This leaves bandwidth to the game's
         * own controller output and avoids holding the actuators in audio mode. */
        if (nonzero || streaming) {
            bool first = !streaming;
            if (first) {
                btEncodeControl(packet, sequence++, true);
                if (!sendBluetooth(packet, 78)) goto failed;
                counter = 0; streaming = true;
            }
            size_t length = btEncodePcm(packet, first, sequence++, counter, pcm);
            if (!sendBluetooth(packet, length)) goto failed;
            counter += 2; ++packetNumber;
            if (silentPackets >= 7) {
                btEncodeControl(packet, sequence++, false);
                if (!sendBluetooth(packet, 78)) goto failed;
                streaming = false;
            } else if (packetNumber % 47 == 0) {
                btEncodeControl(packet, sequence++, true);
                if (!sendBluetooth(packet, 78)) goto failed;
            }
        }
        uint32_t desiredLightbar = atomic_load(&lightbarColor);
        if (desiredLightbar != appliedLightbar && ticks >= nextLightbarAttempt) {
            nextLightbarAttempt = ticks + 2;
            if (!lightbarSetup) {
                btEncodeLightbar(packet, sequence++, 0, true);
                lightbarSetup = sendBluetooth(packet, 78);
            }
            if (lightbarSetup) {
                uint32_t rgb = desiredLightbar == LIGHTBAR_INACTIVE ? LIGHTBAR_IDLE_BLUE : desiredLightbar;
                btEncodeLightbar(packet, sequence++, rgb, false);
                if (sendBluetooth(packet, 78)) {
                    if (appliedLightbar == LIGHTBAR_INACTIVE)
                        logline("Bluetooth: adaptive light bar active.");
                    appliedLightbar = desiredLightbar;
                } else {
                    nextLightbarAttempt = ticks + 47;
                }
            } else {
                nextLightbarAttempt = ticks + 47;
            }
        }
        if (ticks % 470 == 0) {
            pthread_mutex_lock(&ringMutex); unsigned long long lost = dropped; pthread_mutex_unlock(&ringMutex);
            char msg[240]; snprintf(msg, sizeof(msg), "Bluetooth stream: %llu packets; %llu nonzero samples; %llu queued frames dropped; max HID write %.1f ms",
                (unsigned long long)packetNumber, (unsigned long long)nonzeroSamples, lost, bluetoothMaxWriteNs/1e6);
            logline(msg);
        }
    }
    /* A short silence tail followed by a return to ordinary rumble. */
    memset(packet, 0, sizeof(packet));
    if (streaming) {
        const int8_t silent[128] = {0};
        size_t length = btEncodePcm(packet, false, sequence++, counter, silent);
        sendBluetooth(packet, length);
        usleep(30000);
        btEncodeControl(packet, sequence++, false); sendBluetooth(packet, 78);
    }
    if (appliedLightbar != LIGHTBAR_INACTIVE) {
        btEncodeLightbar(packet, sequence++, LIGHTBAR_IDLE_BLUE, false);
        sendBluetooth(packet, 78);
    }
    return NULL;
failed:
    atomic_store(&bluetoothFailed, true); atomic_store(&outputReady, false);
    int fd = atomic_load(&peer); if (fd >= 0) shutdown(fd, SHUT_RDWR);
    return NULL;
}
static int openBluetooth(IOHIDDeviceRef dev) {
    IOReturn status = IOHIDDeviceOpen(dev, kIOHIDOptionsTypeNone);
    if (status != kIOReturnSuccess) {
        char msg[128]; snprintf(msg, sizeof(msg), "Cannot open Bluetooth DualSense: 0x%x", status); logline(msg); return 0;
    }
    uint8_t calibration[41] = { 0x05 }; CFIndex length = sizeof(calibration);
    /* Switch a newly connected pad to full reports. Haptic packets use that mode. */
    IOHIDDeviceGetReport(dev, kIOHIDReportTypeFeature, 0x05, calibration, &length);
    bluetoothDevice = dev; CFRetain(dev);
    clearRing(); atomic_store(&bluetoothFailed, false); atomic_store(&bluetoothRunning, true);
    atomic_store(&outputReady, true);
    if (pthread_create(&bluetoothThread, NULL, bluetoothWriter, NULL) != 0) {
        atomic_store(&outputReady, false); atomic_store(&bluetoothRunning, false); IOHIDDeviceClose(dev, 0); CFRelease(bluetoothDevice); bluetoothDevice = NULL; return 0;
    }
    bluetoothThreadCreated = true;
    logline("Bluetooth: DualSense haptic stream ready (3 kHz signed 8-bit stereo, direct HID).");
    return 1;
}
static void closeBluetooth(void) {
    if (!bluetoothDevice) return;
    atomic_store(&outputReady, false); atomic_store(&bluetoothRunning, false);
    int fd = atomic_load(&peer); if (fd >= 0) shutdown(fd, SHUT_RDWR);
    if (bluetoothThreadCreated) { pthread_join(bluetoothThread, NULL); bluetoothThreadCreated = false; }
    IOHIDDeviceClose(bluetoothDevice, 0); CFRelease(bluetoothDevice); bluetoothDevice = NULL;
    clearRing();
}
static void refreshOutput(void) {
    AudioDeviceID usb = strcmp(transportPreference, "bluetooth") == 0 ? 0 : findDevice(0);
    if (usb) {
        if (usb != currentDevice || bluetoothDevice) {
            closeAudio(); resetHidManager(); sendUsbOutput(true, LIGHTBAR_INACTIVE); openAudio(usb);
        }
        return;
    }
    if (currentDevice) {
        closeAudio(); logline("USB output removed; looking for Bluetooth.");
    }
    if (!bluetoothDevice) resetHidManager();
    IOHIDDeviceRef bt = strcmp(transportPreference, "usb") == 0 ? NULL : findBluetooth(0);
    if (bluetoothDevice && (!bt || !CFEqual(bt, bluetoothDevice) || atomic_load(&bluetoothFailed))) {
        closeBluetooth();
        if (bt) CFRelease(bt);
        resetHidManager();
        bt = strcmp(transportPreference, "usb") == 0 ? NULL : findBluetooth(0);
    }
    if (bt) { if (!bluetoothDevice) openBluetooth(bt); CFRelease(bt); }
}
static bool readLightbarControl(const uint8_t *block) {
    /* Two quiet NaNs form the header. Valid PCM cannot contain this marker. */
    if ((block[1] != 0x4c && block[1] != 0x4d) || block[2] != 0xc0 ||
        block[3] != 0x7f || block[6] != 0xc0 || block[7] != 0x7f)
        return false;
    uint32_t rgb = ((uint32_t)block[0] << 16) | ((uint32_t)block[4] << 8) | block[5];
    atomic_store(&lightbarColor, block[1] == 0x4d ? LIGHTBAR_INACTIVE : rgb);
    return true;
}
static void *server(void *unused) {
    (void)unused;
    int listener = -1;
    while (atomic_load(&running)) {
        if (!atomic_load(&outputReady)) {
            if (listener >= 0) { close(listener); listener = -1; }
            usleep(10000); continue;
        }
        if (listener < 0) {
            listener = socket(AF_INET, SOCK_STREAM, 0); int one = 1;
            if (listener < 0) { logline("Cannot create bridge socket."); atomic_store(&running, false); break; }
            setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
            struct sockaddr_in addr = { .sin_len = sizeof(addr), .sin_family = AF_INET,
                .sin_port = htons(bridgePort), .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
            if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) || listen(listener, 1)) {
                logline("Cannot bind localhost bridge port (another bridge may be running).");
                close(listener); listener = -1; atomic_store(&running, false); break;
            }
            fcntl(listener, F_SETFL, O_NONBLOCK);
        }
        int fd = accept(listener, NULL, NULL);
        if (fd < 0) { usleep(10000); continue; }
        /* BSD accept() inherits O_NONBLOCK from the listener. recv() must block. */
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
        int noDelay = 1; setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay));
        atomic_store(&peer, fd);
        struct timeval timeout = { 5, 0 };
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        logline("Game connected."); clearRing();
        unsigned char block[480 * 2 * sizeof(float)]; size_t used = 0;
        while (atomic_load(&running) && atomic_load(&outputReady)) {
            ssize_t n = recv(fd, block + used, sizeof(block) - used, 0);
            if (n <= 0) break;
            used += n;
            if (used == sizeof(block)) {
                if (!readLightbarControl(block)) {
                    float aligned[960]; memcpy(aligned, block, sizeof(block));
                    enqueue(aligned, 480); received += 480;
                }
                used = 0;
            }
        }
        atomic_store(&lightbarColor, LIGHTBAR_INACTIVE);
        atomic_store(&peer, -1); close(fd); clearRing(); logline("Game disconnected; waiting.");
    }
    if (listener >= 0) close(listener); return NULL;
}
static int testRing(void) {
    float input[8] = { 0.25f, -0.5f, NAN, INFINITY, 2, -2, 0.75f, -0.75f };
    enqueue(input, 4); float result[24];
    AudioBufferList list = { .mNumberBuffers = 1, .mBuffers = {{4, sizeof(result), result}} };
    render(NULL, NULL, NULL, 0, 6, &list);
    assert(result[0] == 0 && result[1] == 0 && result[2] == 0.25f && result[3] == -0.5f);
    assert(result[6] == 0 && result[7] == 0 && result[10] == 1 && result[11] == -1);
    assert(result[18] == 0 && result[19] == 0 && count == 0);
    puts("PASS: actuator routing, speaker silence, clipping, nonfinite samples, underrun silence."); return 0;
}
int main(int argc, char **argv) {
    int list = 0, test = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--list")) list = 1;
        else if (!strcmp(argv[i], "--self-test")) return testRing();
        else if (!strcmp(argv[i], "--test")) test = 1;
        else if (!strcmp(argv[i], "--transport") && i+1 < argc) transportPreference = argv[++i];
        else if (!strcmp(argv[i], "--port") && i+1 < argc) bridgePort = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--parent-pid") && i+1 < argc) parentPid = atoi(argv[++i]);
        else { fprintf(stderr, "Usage: HapticsBridge [--list|--test|--self-test] [--port N] [--parent-pid PID] [--transport auto|usb|bluetooth]\n"); return 2; }
    }
    if (strcmp(transportPreference, "auto") && strcmp(transportPreference, "usb") && strcmp(transportPreference, "bluetooth")) return 2;
    if (bridgePort < 1024 || bridgePort > 65535) return 2;
    if (list) {
        AudioDeviceID d = findDevice(1); IOHIDDeviceRef bt = findBluetooth(1);
        bool found = d || bt; if (bt) CFRelease(bt);
        puts(found ? "Haptic controller found." : "No DualSense found. Connect USB or pair with macOS Bluetooth.");
        return found ? 0 : 1;
    }
    signal(SIGINT, sigHandler); signal(SIGTERM, sigHandler); signal(SIGPIPE, SIG_IGN);
    /* Single bridge per user/port. Keep lock until process exit. */
    char lockPath[160]; snprintf(lockPath, sizeof(lockPath), "/tmp/silksoftmac-%u-%d.lock", getuid(), bridgePort);
    int lock = open(lockPath, O_CREAT | O_RDWR | O_NOFOLLOW, 0600);
    if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB) != 0) { fprintf(stderr, "Bridge already running or lock unavailable.\n"); return 1; }
    logFile = fopen("HapticsBridge.log", "w");
    logline("SilkHaptic native bridge started (USB / Bluetooth DualSense).");
    refreshOutput();
    if (test) {
        if (!atomic_load(&outputReady)) { logline("No DualSense output to test. Connect USB or macOS Bluetooth."); return 1; }
        for (int b = 0; b < 100; b++) {
            float block[960];
            for (int f = 0; f < 480; f++) {
                double t = (b*480+f)/(double)RATE;
                float v = 0.20f * sin(2*M_PI*180*t) * sin(M_PI * fmin(1,t*2-floor(t*2)));
                block[f*2] = t < 0.5 ? v : 0;
                block[f*2+1] = t >= 0.5 ? v : 0;
            }
            enqueue(block, 480); usleep(10000);
        }
        usleep(100000); bool failed = atomic_load(&bluetoothFailed); closeAudio();
        logline("Actuator test complete (left, then right)."); return failed ? 1 : 0;
    }
    if (!atomic_load(&outputReady)) logline("Waiting for USB or Bluetooth DualSense; game keeps normal rumble until ready.");
    pthread_t network; if (pthread_create(&network, NULL, server, NULL)) return 1;
    unsigned ticks = 0;
    while (!interrupted && atomic_load(&running)) {
        if (parentPid > 1 && kill(parentPid, 0) != 0 && errno == ESRCH) break;
        bool refresh = ticks++ % 20 == 0;
        if (refresh) {
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, true);
            refreshOutput();
        }
        if (currentDevice) {
            uint32_t desired = atomic_load(&lightbarColor);
            if (desired != usbLightbar) {
                uint32_t rgb = desired == LIGHTBAR_INACTIVE ? LIGHTBAR_IDLE_BLUE : desired;
                if (sendUsbOutput(refresh, rgb)) {
                    if (usbLightbar == LIGHTBAR_INACTIVE)
                        logline("USB: adaptive light bar active.");
                    usbLightbar = desired;
                }
            } else if (refresh) {
                sendUsbOutput(true, desired);
            }
        }
        usleep(50000);
    }
    atomic_store(&running, false); closeAudio(); pthread_join(network, NULL);
    if (hidManager) { IOHIDManagerUnscheduleFromRunLoop(hidManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode); IOHIDManagerClose(hidManager, 0); CFRelease(hidManager); }
    char stats[200]; snprintf(stats, sizeof(stats), "Bridge stopped. Received frames: %llu; rendered frames: %llu; underrun callbacks: %llu; dropped frames: %llu", received, rendered, underruns, dropped);
    logline(stats); if (logFile) fclose(logFile); close(lock); return 0;
}
