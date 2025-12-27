# SRTC Provider (SRT Caller/Pull)

The SRTC provider enables OvenMediaEngine to actively pull streams from external SRT sources operating in **listener mode**. This is the opposite of the standard SRT provider which listens for incoming connections.

## Use Case

```
┌─────────────────┐         SRT Caller         ┌─────────────────┐
│  External SRT   │ ◀─────────────────────────│  OvenMediaEngine │
│  Source         │        (SRTC pulls)        │  (SRTC Provider) │
│  (Listener)     │                            │                  │
│  :9000          │                            │  → LL-HLS :8888  │
└─────────────────┘                            │  → WebRTC :3333  │
                                               └─────────────────┘
```

**Typical sources:**
- Hardware encoders (Teradek, Haivision)
- FFmpeg in listener mode
- Other streaming servers with SRT output

## Configuration

### Server.xml - Bind Section

```xml
<Bind>
    <Providers>
        <SRTC>
            <WorkerCount>1</WorkerCount>
        </SRTC>
    </Providers>
</Bind>
```

### Server.xml - Application Providers

```xml
<Providers>
    <SRTC />
</Providers>
```

### Server.xml - Origin Configuration

The SRTC provider uses the Origin mechanism for on-demand stream pulling:

```xml
<Origins>
    <Origin>
        <Location>/app/stream-name</Location>
        <Pass>
            <Scheme>srt</Scheme>
            <Urls>
                <Url>srt-source.example.com:9000?passphrase=secret&amp;latency=200</Url>
            </Urls>
        </Pass>
    </Origin>
</Origins>
```

### URL Parameters

| Parameter | Description | Default |
|-----------|-------------|---------|
| `passphrase` | SRT encryption passphrase (10-79 chars) | none |
| `latency` | SRT latency in milliseconds | 120 |
| `mode` | Connection mode (always `caller` for SRTC) | caller |
| `streamid` | SRT Stream ID for multiplexed sources | none |

## Complete Example

```xml
<?xml version="1.0" encoding="UTF-8"?>
<Server version="8">
    <Name>SRTC-Example</Name>
    <Type>origin</Type>
    <IP>*</IP>

    <Bind>
        <Providers>
            <SRTC><WorkerCount>2</WorkerCount></SRTC>
        </Providers>
        <Publishers>
            <LLHLS><Port>8888</Port></LLHLS>
            <WebRTC>
                <Signalling><Port>3333</Port></Signalling>
                <IceCandidates>
                    <IceCandidate>*:10000-10004/udp</IceCandidate>
                </IceCandidates>
            </WebRTC>
        </Publishers>
    </Bind>

    <VirtualHosts>
        <VirtualHost>
            <Name>default</Name>
            <Host><Names><Name>*</Name></Names></Host>

            <Origins>
                <!-- Pull from hardware encoder -->
                <Origin>
                    <Location>/app/encoder1</Location>
                    <Pass>
                        <Scheme>srt</Scheme>
                        <Urls>
                            <Url>192.168.1.100:9000?passphrase=mykey123&amp;latency=200</Url>
                        </Urls>
                    </Pass>
                </Origin>

                <!-- Pull from FFmpeg source -->
                <Origin>
                    <Location>/app/ffmpeg-feed</Location>
                    <Pass>
                        <Scheme>srt</Scheme>
                        <Urls>
                            <Url>stream-server.local:9876?latency=120</Url>
                        </Urls>
                    </Pass>
                </Origin>
            </Origins>

            <Applications>
                <Application>
                    <Name>app</Name>
                    <Type>live</Type>
                    <OutputProfiles>
                        <OutputProfile>
                            <Name>bypass</Name>
                            <OutputStreamName>${OriginStreamName}</OutputStreamName>
                            <Encodes>
                                <Video><Bypass>true</Bypass></Video>
                                <Audio><Bypass>true</Bypass></Audio>
                            </Encodes>
                        </OutputProfile>
                    </OutputProfiles>
                    <Providers>
                        <SRTC />
                    </Providers>
                    <Publishers>
                        <LLHLS />
                        <WebRTC />
                    </Publishers>
                </Application>
            </Applications>
        </VirtualHost>
    </VirtualHosts>
</Server>
```

## Testing with FFmpeg

Start FFmpeg as an SRT listener (source):

```bash
ffmpeg -re -f lavfi -i testsrc=size=1280x720:rate=30 \
       -f lavfi -i sine=frequency=1000:sample_rate=48000 \
       -c:v libx264 -preset ultrafast -tune zerolatency -b:v 2000k \
       -c:a aac -b:a 128k \
       -f mpegts "srt://0.0.0.0:9000?mode=listener&latency=120&passphrase=testkey123"
```

Then request the stream from OME:

```bash
# LL-HLS
curl http://localhost:8888/app/stream-name/llhls.m3u8

# Play with ffplay
ffplay http://localhost:8888/app/stream-name/llhls.m3u8
```

## How It Works

### Deferred Track Discovery

Unlike RTSP or RTMP where track information is available during connection setup, SRT with MPEG-TS transport discovers tracks **asynchronously** from the data stream:

1. **Connection**: SRTC connects to the SRT source
2. **Data Reception**: MPEG-TS packets start arriving
3. **Track Discovery**: PAT/PMT tables reveal video/audio tracks
4. **Stream Update**: Transcoder creates output streams when tracks are known

This is handled automatically by the provider.

### Connection Flow

```
┌──────────────────────────────────────────────────────────────────┐
│ 1. HTTP Request arrives (e.g., /app/stream/llhls.m3u8)           │
│                              ↓                                    │
│ 2. Origin map lookup → finds SRTC origin configuration           │
│                              ↓                                    │
│ 3. SRTC Provider creates stream, connects to SRT source          │
│                              ↓                                    │
│ 4. Stream created with placeholder "Data" track                  │
│                              ↓                                    │
│ 5. MPEG-TS depacketizer discovers Video/Audio tracks             │
│                              ↓                                    │
│ 6. UpdateStream() notifies MediaRouter                           │
│                              ↓                                    │
│ 7. Transcoder creates output streams (deferred creation)         │
│                              ↓                                    │
│ 8. Publishers (LLHLS, WebRTC) receive packets                    │
└──────────────────────────────────────────────────────────────────┘
```

## Supported Codecs

The SRTC provider supports any codec that can be transported via MPEG-TS:

| Type | Codecs |
|------|--------|
| Video | H.264 (AVC), H.265 (HEVC) |
| Audio | AAC, MP3, AC3 |

## Reconnection

The provider automatically attempts to reconnect if the connection is lost:

- **Max Attempts**: 3 (configurable via `MAX_RECONNECT_ATTEMPTS`)
- **Behavior**: Exponential backoff between attempts
- **On Failure**: Stream enters ERROR state

## Differences from SRT Provider

| Feature | SRT (Listener) | SRTC (Caller) |
|---------|----------------|---------------|
| Connection | Waits for incoming | Connects outbound |
| Port Binding | Required | Not required |
| Use Case | Receive from encoders | Pull from servers |
| Configuration | Bind section | Origin section |

## Troubleshooting

### "Connection timed out"

- Verify the SRT source is running and accessible
- Check firewall rules allow outbound connections
- Verify the passphrase matches

### "No supported codec"

- Ensure the source sends H.264/H.265 video and AAC audio
- Check MPEG-TS stream is properly formatted

### First request returns 404

This is expected behavior. The first request triggers the Origin pull, which takes time to:
1. Connect to SRT source
2. Receive enough data to discover tracks
3. Create output streams

Retry the request after 1-2 seconds.

## Building

The SRTC provider is built automatically with OvenMediaEngine:

```bash
cd src
make release
```

No additional dependencies required (uses existing libsrt).
