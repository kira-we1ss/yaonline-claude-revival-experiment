# YaChat Modernization — Claude Instructions

## Project Identity
This is **YaChat**, a Yandex-branded XMPP client (~2009) forked from Psi. Located at `/Users/kweiss/Downloads/yachat`.
~900 source files, ~189K lines of C++/Qt code. Not a git repo.

## Session Recovery
If the session resets, read these files in order to recover full context:
1. This file (`CLAUDE.md`)
2. `docs/superpowers/specs/2026-04-14-yachat-modernization-design.md` — full approved design
3. `docs/superpowers/plans/2026-04-14-yachat-modernization.md` — implementation plan (20 tasks, 5 layers)

**Always update this file** when something significant is learned, completed, or changed.

## Goal
Modernize YaChat to build and run on modern macOS (Ventura/Sonoma/Sequoia). Success = connects to XMPP server, sends and receives messages.

## Constraints
- **macOS only** — no Linux/Windows work
- **Qt 5.15 LTS** — not Qt 6
- **Keep qmake** — no CMake migration
- **Keep all Yandex UI** — `src/tools/yastuff/` (322 files) stays intact
- **Keep Iris** — extend it, don't replace it

## Migration Layers (execute in order)
1. **Layer 1** — macOS-specific fixes (Growl → QSystemTrayIcon, deprecated Carbon/Cocoa APIs, remove Sparkle)
2. **Layer 2** — qmake fixes for Qt 5.15 (remove `qt3support`, add `c++17`, fix `QMAKE_MACOSX_DEPLOYMENT_TARGET = 11.0`)
3. **Layer 3** — Qt3Support removal (559+ Q3* class usages → Qt5 equivalents)
4. **Layer 4** — QCA 2.3.x (SCRAM-SHA-256, TLS 1.2+ minimum)
5. **Layer 5** — New XEPs: carbons (XEP-0280), MAM (XEP-0313), message correction (XEP-0308), OMEMO (XEP-0384, 1-on-1 only)

Each layer must produce a working build before starting the next.

## Key Files & Directories
| Path | Purpose |
|------|---------|
| `src/src.pro` | Main app qmake project |
| `iris/iris.pro` | Iris XMPP library project |
| `third-party/qca/` | QCA crypto library (needs update to 2.3.x) |
| `iris/src/xmpp/xmpp-im/` | XMPP extension implementations |
| `iris/src/xmpp/xmpp-core/xmpp_clientstream.cpp` | TLS/SASL stream handling |
| `src/tools/yastuff/` | Yandex UI layer — do not break |
| `src/tools/growlnotifier/` | Replace with QSystemTrayIcon |
| `src/tools/mac_dock/` | macOS dock integration — update APIs |
| `conf.pri` | Build config (defines, includes, libs) |
| `docs/superpowers/specs/2026-04-14-yachat-modernization-design.md` | Full design doc |

## Qt3 → Qt5 Class Replacement Map
| Old | New |
|-----|-----|
| `Q3MainWindow` | `QMainWindow` |
| `Q3ListView` / `Q3ListViewItem` | `QTreeView` + `QStandardItemModel` |
| `Q3TextEdit` | `QTextEdit` |
| `Q3PopupMenu` | `QMenu` |
| `Q3PtrList<T>` | `QList<T*>` |
| `Q3Dict<T>` | `QHash<QString, T*>` |
| `Q3SocketDevice` | `QTcpSocket` |
| `#include <qtimer.h>` | `#include <QTimer>` |

## New Files to Create (Layer 5)
| File | Purpose |
|------|---------|
| `iris/src/xmpp/xmpp-im/xmpp_carbons.h/cpp` | XEP-0280 Message Carbons |
| `iris/src/xmpp/xmpp-im/xmpp_mam.h/cpp` | XEP-0313 MAM |
| `src/omemomanager.h/cpp` | XEP-0384 OMEMO manager |
| `third-party/libsignal-protocol-c/` | Signal Protocol C library for OMEMO |

## Dead Code to Remove
- Legacy groupchat (`jabber:iq:conference`) support
- Google Voice extension (`http://www.google.com/xmpp/protocol/voice/v1`)
- Sparkle auto-update integration
- Growl notifier (`src/tools/growlnotifier/`)

## Things to Preserve / Be Careful With
- `xmpp_yalastmail.h` — wrap in `#ifdef YANDEX_EXTENSIONS` (defined in `conf.pri` by default), don't delete
- All of `src/tools/yastuff/` — Yandex UI, user wants it kept
- Existing unit tests in `qa/unittest/` and `iris/src/xmpp/*/unittest/` — fix failures, don't delete

## Error Handling Rules
- TLS/auth failures → clear error dialog via existing `AccountModifyDlg` error path
- OMEMO key conflicts → prompt user, never silently fail
- Carbons/MAM unavailable → silent degradation, debug log only

## Out of Scope (do not do these)
- CMake migration
- MUC OMEMO (XEP-0384 §8)
- Linux or Windows support
- Code signing / notarization
- New unit tests
- Sparkle / auto-update

## Instructions for Claude
- **Update this file** whenever a layer is completed, a key decision is made, or something unexpected is discovered
- **Update this file** when the implementation plan is written (add its path)
- **Update this file** when layers are completed (mark them done with a checkmark)
- Prefer editing existing files over creating new ones
- Do not add features beyond what's listed above
- Do not refactor code that isn't broken by the Qt5 migration
- Run compile checks after every layer before proceeding
- Test against Prosody 0.12+ locally for each checkpoint
