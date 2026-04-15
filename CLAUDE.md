# OvenMediaEngine — Emberware Fork

Fork von [OvenMediaEngine](https://github.com/AirenSoft/OvenMediaEngine) (Sub-Second Latency Streaming Server). Wird in emberware-v2 als Origin-Server für RTSP/SRT/WebRTC Streaming eingesetzt.

## Tech Stack

- **Sprache**: C++ (CMake), zusätzlich Python für Build-Tools
- **Build**: Docker (verschiedene Targets — Standard, CUDA, local dev)
- **Streaming**: RTMP/SRT/WebRTC/LLHLS/HLS
- **API**: REST + WebSocket
- **Apps**: defined in `Server.xml`

## Build-Targets

| Dockerfile | Zweck |
|------------|-------|
| `Dockerfile` | Standard Production |
| `Dockerfile.cuda` | NVIDIA HW-Accel |
| `Dockerfile.cuda.local` | CUDA local dev |
| `Dockerfile.local` | Local dev (Standard) |
| `Dockerfile.local.dev` | Local dev mit debug-symbols |

## Critical Rules

- **Keine AI-Attribution** in Commits
- **Upstream sync**: Vor jedem feature-add prüfen ob upstream bereits eine Lösung hat
- **Server.xml**: Schema validieren vor commit — kaputte XML killt OME beim Start
- **Docker builds**: Nutze immer `--platform linux/amd64` (Apple Silicon Cross-build)
- **Performance**: keine Locks im Hot-Path (RTP/RTCP processing)

## MCP Server

Nur globale MCPs aktiv (kein `.mcp.json`). Verfügbare MCPs prüfen: `claude mcp list`.

| MCP | Trigger |
|-----|---------|
| `context7` | C++/CMake, Streaming-Protokolle (WebRTC, SRT, HLS spec) |
| `sequential-thinking` | Pipeline-Architektur, Latenz-Optimierung, Buffering |
| `memory` | Patches gegen upstream, Build-Quirks, OME-Quirks |
| `github` | PRs, Issues, Sync mit AirenSoft/OvenMediaEngine |
| `firecrawl` | OME Docs, Streaming RFCs |

**Hinweis**: aseOFstreams `.mcp.json` hat zusätzlich `azure` + `docker` MCPs — wenn du in beiden gleichzeitig arbeitest, in das aseOFstreams Verzeichnis wechseln um die Tools zu nutzen.
