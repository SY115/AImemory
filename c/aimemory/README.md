# AImemory - AI Anti-Hallucination Memory Engine

**Author:** Jiayi Sun (SY115)  
**License:** Free use with attribution. Commercial use over $1M revenue: 5% fee.

## What is this?

AImemory is a third-party memory plugin that sits between you and your AI. It reduces AI hallucinations by giving AI persistent memory about who you are, what mistakes it has made before, and what rules it should follow — then enforces those rules before every response.

### The Problem

AI hallucinates because:
1. **It doesn't know what it doesn't know** — no self-awareness of its own limitations
2. **Users can lead AI astray** — AI doesn't push back on incorrect premises
3. **AI answers first, verifies never** — it generates confident responses even when guessing

### The Solution

A six-layer memory system stored **locally on your machine**, with automatic send interception and context injection:

| Layer | Purpose |
|-------|---------|
| **Identity** | Who is the user, skills, preferences |
| **Work** | Project context, technical details |
| **Chat** | Conversation history |
| **Correction** | Mistakes AI has made before |
| **Self** | AI's capability boundaries |
| **Verify** | Rules and patterns to check against |

When you send a message to any AI, AImemory:
1. **Intercepts** the message before it's sent
2. **Searches** all six memory layers for relevant context
3. **Detects** your intent (asking info vs requesting action)
4. **Injects** relevant memories and rules into the message
5. AI receives your message **with context** and responds accordingly
6. **Verifies** AI's response against memory, flags hallucination risk
7. **Auto-corrects** if AI ignores injected rules
8. **Suggests new conversation** if repeated hallucinations detected

## Architecture

```
User types message
        ↓
[AImemory Extension intercepts]
        ↓
[Search 6-layer memory] ← Local engine (C, localhost:7890)
        ↓
[Inject context into message]
        ↓
AI receives message + context → responds
        ↓
[Verify response against memory]
        ↓
[Show warning badge if risk detected]
```

- **Core Engine:** C (fast, lightweight, cross-platform)
- **HTTP Server:** C (localhost:7890, REST API)
- **Browser Extension:** JavaScript (Chrome, works with ChatGPT/Claude/Gemini)
- **Data Storage:** Local flat files (.mem), **never sent to cloud**
- **Cross-language:** Chinese ↔ English synonym matching

## Build

### Linux / macOS
```bash
cd aimemory
make
```

### Windows (with w64devkit or MinGW)
```bash
gcc -Wall -O2 -std=c11 -o aimemory.exe src/engine.c src/main.c -lws2_32
gcc -Wall -O2 -std=c11 -o aimemory-server.exe src/engine.c src/server.c -lws2_32
```

## Usage

### 1. Start the engine
```bash
# Load demo data (first time)
echo demo | ./aimemory

# Start the server
./aimemory-server
```

### 2. Install browser extension
1. Open Chrome → `chrome://extensions`
2. Enable **Developer mode**
3. Click **Load unpacked** → select `extension/` folder
4. Grant permission when prompted

### 3. Use any AI normally
Open ChatGPT, Claude, or Gemini in Chrome. AImemory works silently in the background. You'll see:
- **Green indicator** (bottom-right): engine connected
- **Warning badges** on AI responses when hallucination risk is detected
- **Context injection** happens automatically before your messages are sent

### API Endpoints
| Method | Path | Description |
|--------|------|-------------|
| GET | `/health` | Health check |
| GET | `/stats` | Memory statistics |
| GET | `/search?q=...` | Search all layers |
| POST | `/store` | Store memory entry |
| POST | `/intent` | Detect user intent |
| POST | `/verify` | Verify + ask-don't-guess |
| POST | `/extract` | Auto-learn from conversation |

## Privacy

🔒 **All data is stored locally on your machine** in the `data/` directory. Nothing is ever sent to any cloud service or third party. You own your data completely.

## Features (v0.5)

- [x] Six-layer memory storage with unified search
- [x] Cross-language matching (Chinese ↔ English synonym table)
- [x] Intent detection (asking info vs requesting action)
- [x] Intent-aware risk scoring (questions don't trigger false alerts)
- [x] Send interception with memory context injection
- [x] Full rule content injection (AI understands WHY, not just keywords)
- [x] AI response verification with warning badges
- [x] Auto-correction on hallucination detection
- [x] New conversation recovery with context copy
- [x] Permission request popup (privacy-first UX)
- [x] Deduplication (no repeated warnings)
- [x] Auto-learning from conversations
- [x] HTTP REST API server
- [x] Chrome extension (ChatGPT, Claude, Gemini)
- [x] Windows + Linux cross-platform
- [ ] Desktop app integration (Claude/GPT native apps)
- [ ] Encrypted local storage
- [ ] Custom rule editor UI
- [ ] Firefox extension

## How It Works (Real Example)

Without AImemory:
```
User: "Send the full PoC to SSD"
AI: "Sure, here's a draft email with all the details attached..."
→ User loses negotiation leverage by giving away vulnerability details for free
```

With AImemory:
```
User: "Send the full PoC to SSD"
[AImemory injects: PAST MISTAKE: AI suggested sending full PoC before receiving offer.
 RULE: Never send vulnerability details before formal offer.]
AI: "Hold on — you haven't received an offer from SSD yet. 
     Sending the full PoC now would hurt your negotiation position.
     Have they responded to your April 6 summary email?"
→ AI actively protects user's interests based on injected memory
```

## License

Free to use with attribution. If your commercial product using AImemory generates over $1M USD in annual revenue, a 5% licensing fee applies. See LICENSE for details.

## Author

**Jiayi Sun (SY115)** — Independent Security Researcher & Developer

GitHub: [@SY115](https://github.com/SY115)
