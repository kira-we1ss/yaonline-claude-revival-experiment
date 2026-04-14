# YaChat Modernization Design

**Date:** 2026-04-14  
**Project:** YaChat (Yandex XMPP client, fork of Psi, ~2009)  
**Target:** macOS only, Qt 5.15 LTS, qmake build system  
**Success criteria:** App builds and runs on modern macOS (Ventura/Sonoma/Sequoia), connects to an XMPP server, sends and receives messages

---

## 1. Architecture Overview

The migration follows a strict bottom-up layer order. Each layer must compile and produce a working build before the next layer begins.

```
Layer 5: New XMPP XEPs (carbons, MAM, OMEMO, message correction)
Layer 4: QCA 2.3.x (SCRAM-SHA-256, TLS 1.2+ enforcement)
Layer 3: Qt3Support removal (Q3* → Qt5 equivalents)
Layer 2: qmake / build system fixes for Qt 5.15
Layer 1: macOS-specific fixes (Growl, deprecated OS APIs)
```

**What stays untouched:**
- The Yandex UI layer (`src/tools/yastuff/`) — all 322 files preserved
- The Iris XMPP library structure — extended, not replaced
- The qmake build system — fixed and modernized, not replaced with CMake

**Checkpoints** (each must produce a working build before proceeding):
1. App compiles against Qt 5.15 with no Qt3 classes
2. QCA connects with SCRAM-SHA-256 to a test server over TLS 1.2+
3. Carbons and MAM work in a live chat session
4. OMEMO encrypts a message end-to-end in a 1-on-1 chat

---

## 2. Build System & Qt5 Migration

### 2.1 qmake changes (Layer 2)

Affected files: `src/src.pro`, `iris/iris.pro`, `third-party/qca/qca.pro`, and all sub-project `.pro` files.

- Remove `QT += qt3support` everywhere
- Replace `QT += xml network` with `QT += xml network widgets concurrent`
- Add `CONFIG += c++17`
- Set `QMAKE_MACOSX_DEPLOYMENT_TARGET = 11.0` (drop macOS 10.x)
- Fix stale `QMAKE_MAC_SDK` references to current macOS SDK

### 2.2 Qt3Support removal (Layer 3)

559+ instances across the codebase. Qt 5 removed the qt3support module entirely — all Q3* classes must be replaced inline.

| Old (Qt3) | Replacement (Qt5) |
|---|---|
| `Q3MainWindow` | `QMainWindow` |
| `Q3ListView` / `Q3ListViewItem` | `QTreeView` + `QStandardItemModel` |
| `Q3TextEdit` | `QTextEdit` |
| `Q3PopupMenu` | `QMenu` |
| `Q3PtrList<T>` | `QList<T*>` |
| `Q3Dict<T>` | `QHash<QString, T*>` |
| `Q3SocketDevice` | `QTcpSocket` |
| `#include <qtimer.h>` | `#include <QTimer>` |

All other lowercase Qt3-style includes (`#include <qstring.h>`) updated to capitalized Qt4/5 style.

### 2.3 macOS-specific fixes (Layer 1)

- **Growl** (`src/tools/growlnotifier/`) → replaced with `QSystemTrayIcon::showMessage()` for basic notifications. `UNUserNotificationCenter` via an Objective-C++ bridge only if richer notifications are needed.
- **Sparkle** auto-update integration — removed entirely (requires code signing; out of scope)
- **Carbon/Cocoa API calls** in `src/tools/mac_dock/` and system tray code — audited and updated to current macOS SDK equivalents
- **Objective-C++ files** (`.mm`) — confirmed `OBJECTIVE_HEADERS` / `OBJECTIVE_SOURCES` are set correctly in qmake for Qt 5.15

Scope of this layer: compile errors and crashes only. No logic or visual changes.

---

## 3. QCA & XMPP Security

### 3.1 QCA update (Layer 4)

Replace `third-party/qca/` with QCA 2.3.x (current stable).

New capabilities:
- SCRAM-SHA-256 and SCRAM-SHA-512 SASL mechanisms
- TLS 1.2 as the minimum enforced version (SSLv3, TLS 1.0, TLS 1.1 disabled)
- Modern cipher suite selection (RC4, 3DES, and export ciphers disabled)

### 3.2 Iris stream changes

Files: `iris/src/xmpp/xmpp-core/xmpp_clientstream.cpp`

- Enforce `QCA::TLS::ProtocolVersion::TLS_v1_2` as minimum in `QCA::TLSContext` configuration
- Update SASL mechanism priority list: SCRAM-SHA-256 > SCRAM-SHA-1 > PLAIN > DIGEST-MD5
- Keep `digestmd5response.h/cpp` — some older servers still offer DIGEST-MD5; it remains available but deprioritized

### 3.3 Certificate handling

- Replace manual `QSSLCert` handling with `QCA::Certificate` + macOS system keychain validation
- Add per-account certificate pinning option stored in account config (not hard-coded)

---

## 4. Modern XMPP XEPs

All new XEP implementations live in `iris/src/xmpp/xmpp-im/`, following the existing `Task` subclass pattern used throughout Iris.

### XEP-0280 — Message Carbons

- New files: `xmpp_carbons.h`, `xmpp_carbons.cpp`
- Enables carbons on login; handles incoming `<forwarded>` stanzas
- `PsiAccount` subscribes to carbon copies and routes them to the correct `ChatDlg`
- Degrades silently if server does not advertise carbons support

### XEP-0313 — Message Archive Management (MAM)

- New files: `xmpp_mam.h`, `xmpp_mam.cpp`
- Queries server archive with RSM (XEP-0059) pagination
- `HistoryDlg` gains a toggle: local history vs. server MAM history
- Fetches last 50 messages (configurable) on chat window open
- Degrades silently if server does not support MAM

### XEP-0308 — Last Message Correction

- Extend `xmpp_message.h/cpp`: add `replaceId` field, serialize/deserialize `<replace id="..."/>` element
- `YaChatViewWidget` renders corrected messages with a subtle "edited" label inline

### XEP-0384 — OMEMO Encryption

- New dependency: `libsignal-protocol-c` (Signal Protocol C library), added to `third-party/`
- New manager: `OmemoManager` (`src/omemomanager.h/cpp`) — handles key generation, device list publishing via PEP (XEP-0163), and session establishment
- Key storage: per-account SQLite file via `QSqlDatabase` (adds `QT += sql` to `src/src.pro`)
- `ChatDlg` / `YaChatViewWidget` get an OMEMO lock toggle button
- **Scope limit:** 1-on-1 chats only. MUC OMEMO (XEP-0384 §8) is out of scope for this pass.

### Removed dead extensions

| Extension | Reason |
|---|---|
| Legacy groupchat (`jabber:iq:conference`) | Superseded by MUC everywhere; removed |
| Google Voice (`http://www.google.com/xmpp/protocol/voice/v1`) | Server shut down; removed |
| Yandex custom (`xmpp_yalastmail.h`) | Kept but isolated behind `#ifdef YANDEX_EXTENSIONS`, defined in `conf.pri` by default |

---

## 5. Error Handling & Testing

### Error handling

- **TLS/auth failures** → surface a clear error dialog showing the failure reason, routed through the existing `AccountModifyDlg` error path. No generic "connection failed" messages.
- **OMEMO key conflicts** (device list mismatch) → prompt user to re-verify; never silently fail or silently decrypt.
- **MAM/carbons unavailable** → silent degradation. Log to debug output only; no user-visible error.
- **Qt3→Qt5 regressions** → crashes and data loss are blockers. Visual regressions are not.

### Testing

- **Compile check at each layer** — zero-warning builds (`CONFIG += warn_on`; `-Werror` where feasible)
- **Manual smoke test** against a local Prosody 0.12+ instance at each checkpoint:
  1. Login with SCRAM-SHA-256 over TLS 1.2
  2. Send and receive a plain text message
  3. Message appears on a second device via carbons (XEP-0280)
  4. MAM loads last 50 messages on chat window open
  5. OMEMO encrypts and decrypts a 1-on-1 message
- **Existing unit tests** (`qa/unittest/`, `iris/src/xmpp/*/unittest/`) — run at each layer, fix failures, do not delete tests
- **Out of scope:** New unit tests, CI setup, code signing, notarization

---

## 6. Out of Scope

- CMake migration
- MUC OMEMO (XEP-0384 §8)
- Sparkle / auto-update
- Linux or Windows support
- Code signing or notarization
- New unit tests beyond fixing existing ones
- XMPP extensions not listed above
