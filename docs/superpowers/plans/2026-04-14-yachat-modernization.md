# YaChat Modernization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Modernize YaChat (Yandex XMPP client, ~2009) to build and run on modern macOS (Ventura/Sonoma/Sequoia) with Qt 5.15 LTS and modern XMPP security/features.

**Architecture:** Five sequential migration layers — macOS fixes → qmake/Qt5 build → Qt3Support removal → QCA 2.3.x security → new XMPP XEPs. Each layer must produce a working build before the next begins. Iris XMPP library and all Yandex UI code (`src/tools/yastuff/`) are preserved and extended, not replaced.

**Tech Stack:** Qt 5.15 LTS, qmake, QCA 2.3.x, OpenSSL 3.x, libsignal-protocol-c, Prosody 0.12+ (local test server), macOS 11+

---

## File Map

### Modified files (key ones; many more touched in Layer 3)
- `src/src.pro` — Qt modules, macOS deployment target, remove Sparkle/qt3support
- `iris/iris.pro` — Qt5 compat
- `iris/src/xmpp/xmpp.pro` — Qt5 compat
- `third-party/qca/qca.pro` — replace with QCA 2.3.x
- `conf.pri` — add `YANDEX_EXTENSIONS` define, update flags
- `iris/src/xmpp/xmpp-core/stream.cpp` — SASL mechanism priority, TLS min version
- `iris/src/xmpp/xmpp-core/tlshandler.cpp` — fix old-style includes
- `iris/src/xmpp/xmpp-im/xmpp_message.h/cpp` — add `replaceId` for XEP-0308
- `src/mainwin.h/cpp` — Q3MainWindow → QMainWindow
- `src/historydlg.h/cpp` — Q3ListView → QTreeView+QStandardItemModel
- `src/eventdlg.h/cpp` — Q3ListView → QTreeView
- `src/tools/growlnotifier/growlnotifier.h/cpp` — replace with QSystemTrayIcon stub
- All 52 files with Q3* usages (identified by grep below)

### New files
- `iris/src/xmpp/xmpp-im/xmpp_carbons.h/cpp` — XEP-0280 Message Carbons
- `iris/src/xmpp/xmpp-im/xmpp_mam.h/cpp` — XEP-0313 MAM
- `src/omemomanager.h/cpp` — XEP-0384 OMEMO manager
- `third-party/libsignal-protocol-c/` — Signal Protocol C library

### Deleted/stubbed
- `src/tools/sparkle/sparkle.mm/.h` — stubbed out (Sparkle framework not available)
- `src/tools/growlnotifier/growlnotifier.mm` — replaced with Qt-only stub

---

## Layer 1: macOS-Specific Fixes

### Task 1: Remove Sparkle auto-update

Sparkle requires code signing and is not available without it. The `sparkle.pri` already guards with `exists(...)`, so Sparkle will be skipped if the framework is absent. We just need to make sure any `HAVE_SPARKLE`-gated code compiles cleanly without it.

**Files:**
- Read: `src/tools/sparkle/sparkle.pri`
- Read: `src/tools/sparkle/sparkle.h`, `src/tools/sparkle/sparkle.mm`
- Modify: `src/src.pro`

- [ ] **Step 1: Verify Sparkle is already guarded**

```bash
grep -r "HAVE_SPARKLE" /Users/kweiss/Downloads/yachat/src/ --include="*.h" --include="*.cpp" --include="*.mm" -l
```

Expected: list of files that `#ifdef HAVE_SPARKLE`. These will compile safely because `HAVE_SPARKLE` won't be defined (no framework present).

- [ ] **Step 2: Remove the Sparkle include from src/src.pro**

In `src/src.pro`, find and remove this block (lines 161–163):

```qmake
# BEFORE (remove this):
mac {
    SPARKLE_PRI = $$PWD/tools/sparkle/sparkle.pri
    include($$SPARKLE_PRI)
}
```

Replace with nothing (delete those 3 lines entirely). The `CARBONCOCOA_PRI` block above it stays.

- [ ] **Step 3: Verify no unconditional Sparkle linkage remains**

```bash
grep -r "framework Sparkle\|Sparkle.framework\|#include.*sparkle" /Users/kweiss/Downloads/yachat/src/ --include="*.pro" --include="*.pri"
```

Expected: no output.

---

### Task 2: Replace Growl with QSystemTrayIcon stub

Growl is a dead project (macOS killed it). `GrowlNotifier` is used for desktop notifications. Replace its implementation with a `QSystemTrayIcon::showMessage()` wrapper that matches the existing interface exactly — callers don't change.

**Files:**
- Read: `src/tools/growlnotifier/growlnotifier.h`
- Modify: `src/tools/growlnotifier/growlnotifier.cpp` (or `.mm` — check which exists)
- Modify: `src/tools/growlnotifier/growlnotifier.pri` (if exists)

- [ ] **Step 1: Find the growl implementation file**

```bash
ls /Users/kweiss/Downloads/yachat/src/tools/growlnotifier/
```

Note the filenames — there will be a `.mm` (Objective-C++) and/or `.cpp`.

- [ ] **Step 2: Find all callers of GrowlNotifier**

```bash
grep -r "GrowlNotifier\|growlnotifier" /Users/kweiss/Downloads/yachat/src/ --include="*.h" --include="*.cpp" --include="*.mm" -l
```

Note which files call `notify()` and with what arguments.

- [ ] **Step 3: Read the existing implementation**

Read both `growlnotifier.h` and the `.mm`/`.cpp` implementation file to understand the `Private` class structure.

- [ ] **Step 4: Replace the implementation with a QSystemTrayIcon stub**

Delete or replace the existing `.mm` file content with this new `.cpp` file (rename if needed from `.mm` to `.cpp` — update the `.pri` or `src.pri` to match):

```cpp
// growlnotifier.cpp — QSystemTrayIcon-based stub replacing Growl
#include "growlnotifier.h"
#include <QSystemTrayIcon>
#include <QPixmap>
#include <QString>
#include <QStringList>

class GrowlNotifier::Private {
public:
    QString appName;
};

GrowlNotifier::GrowlNotifier(QObject* parent, const QStringList& /*notifications*/,
                              const QStringList& /*defaults*/, const QString& appName)
    : QObject(parent), d(new Private)
{
    d->appName = appName;
}

GrowlNotifier::~GrowlNotifier()
{
    delete d;
}

void GrowlNotifier::notify(const QString& /*name*/, const QString& title,
                            const QString& description, const QPixmap& /*icon*/,
                            bool /*sticky*/, const QObject* /*receiver*/,
                            const char* /*clicked_slot*/, const char* /*timeout_slot*/,
                            void* /*context*/)
{
    // Find the QSystemTrayIcon in the application and use it for notifications.
    // Falls back silently if no tray icon exists.
    foreach (QObject* obj, qApp->allObjects()) {
        if (QSystemTrayIcon* tray = qobject_cast<QSystemTrayIcon*>(obj)) {
            tray->showMessage(title, description, QSystemTrayIcon::Information, 5000);
            return;
        }
    }
}
```

- [ ] **Step 5: Update the .pri or src.pri to use .cpp instead of .mm**

Find where growlnotifier sources are listed:
```bash
grep -r "growlnotifier" /Users/kweiss/Downloads/yachat/src/ --include="*.pri" --include="*.pro"
```

Change `growlnotifier.mm` → `growlnotifier.cpp` in that file. Remove any `OBJECTIVE_SOURCES` reference to it.

- [ ] **Step 6: Remove the Growl framework linkage from conf.pri or src.pri**

```bash
grep -r "Growl\|growl" /Users/kweiss/Downloads/yachat/src/ --include="*.pri" --include="*.pro" --include="*.pri"
```

Remove any `-framework Growl` or `LIBS += ... Growl` lines.

---

### Task 3: Fix deprecated macOS APIs in mac_dock

**Files:**
- Read: `src/tools/mac_dock/` (list all files)
- Read: `src/tools/carboncocoa/carboncocoa.pri`

- [ ] **Step 1: List mac_dock files**

```bash
ls /Users/kweiss/Downloads/yachat/src/tools/mac_dock/
```

- [ ] **Step 2: Read each .mm file for deprecated APIs**

Read each `.mm` file. Look for:
- `HIToolbox` / Carbon process calls → replace with `NSApplication` equivalents
- `GetCurrentProcess` → not needed on modern macOS
- `SetFrontProcess` → use `[NSApp activateIgnoringOtherApps:YES]`
- `ProcessSerialNumber` → obsolete; remove

- [ ] **Step 3: Fix each deprecated call**

For each deprecated Carbon call found, replace with its Cocoa equivalent. Common substitutions:

```objc
// BEFORE:
ProcessSerialNumber psn = { 0, kCurrentProcess };
SetFrontProcess(&psn);

// AFTER:
[[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
```

```objc
// BEFORE:
#include <Carbon/Carbon.h>

// AFTER: (remove Carbon include, add AppKit if not present)
#include <AppKit/AppKit.h>
```

- [ ] **Step 4: Compile check**

```bash
cd /Users/kweiss/Downloads/yachat && /opt/local/libexec/qt5/bin/qmake psi.pro && make -j4 2>&1 | grep -i "error:" | head -30
```

Fix any errors from mac_dock before proceeding.

---

## Layer 2: qmake / Qt5 Build Fixes

### Task 4: Fix src/src.pro for Qt 5.15

**Files:**
- Modify: `src/src.pro`

- [ ] **Step 1: Replace QT modules line**

In `src/src.pro`, find:
```qmake
QT += xml network qt3support
```
Replace with:
```qmake
QT += xml network widgets concurrent
```

- [ ] **Step 2: Add C++17 and remove legacy CONFIG flags**

Find `CONFIG  += qt thread x11` and change to:
```qmake
CONFIG += qt thread c++17
```
(Remove `x11` — not valid on macOS with Qt5; Qt5 handles platform internally.)

- [ ] **Step 3: Fix macOS deployment target**

Find this block in `src/src.pro`:
```qmake
qc_universal {
    CONFIG += x86 x86_64
    QMAKE_MAC_SDK=/Developer/SDKs/MacOSX10.5.sdk
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.5
}
```
Replace the entire `mac { ... }` block that contains it with:
```qmake
mac {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 11.0
    QMAKE_INFO_PLIST = ../mac/Info.plist
    RC_FILE = ../mac/application.icns
    QMAKE_POST_LINK = \
        mkdir -p `dirname $(TARGET)`/../Resources/iconsets/emoticons; \
        cp -R tools/yastuff/iconsets/emoticons/* `dirname $(TARGET)`/../Resources/iconsets/emoticons; \
        cp -R ../certs ../sound `dirname $(TARGET)`/../Resources; \
        echo "APPLyach" > `dirname $(TARGET)`/../PkgInfo;

    CARBONCOCOA_PRI = $$PWD/tools/carboncocoa/carboncocoa.pri
    include($$CARBONCOCOA_PRI)
}
```
(Note: `dsa_pub.pem` copy removed — Sparkle artifact.)

- [ ] **Step 4: Remove the win32/unix non-mac blocks that won't compile**

Since target is macOS only, remove or guard these blocks:
```qmake
unix:!mac {
    QMAKE_POST_LINK = rm -f ../yachat ; ln -s src/yachat ../yachat
}
```
Wrap in `!mac:unix` (already done) — this is fine as-is since we're on mac.

Remove Windows MSVC blocks:
```qmake
win32-msvc|win32-msvc.net|win32-msvc2005|win32-msvc2008 { ... }
```
Delete entirely (not needed on macOS).

- [ ] **Step 5: Add QT += sql (needed for OMEMO in Layer 5)**

At the `QT +=` line, also add `sql`:
```qmake
QT += xml network widgets concurrent sql
```

---

### Task 5: Fix iris.pro and sub-project .pro files for Qt 5.15

**Files:**
- Modify: `iris/iris.pro`
- Modify: `iris/src/xmpp/xmpp.pro`
- Modify: `iris/src/irisnet/irisnet.pro`
- Modify: `iris/conf.pri`

- [ ] **Step 1: Find all QT += lines in iris sub-projects**

```bash
grep -r "QT +=" /Users/kweiss/Downloads/yachat/iris/ --include="*.pro" --include="*.pri" -n
```

- [ ] **Step 2: Remove qt3support from every match**

For each file that has `qt3support`:
```bash
grep -rl "qt3support" /Users/kweiss/Downloads/yachat/iris/ --include="*.pro" --include="*.pri"
```
In each file, remove `qt3support` from the `QT +=` line.

- [ ] **Step 3: Add CONFIG += c++17 to iris/iris.pro**

In `iris/iris.pro`, after `TEMPLATE = subdirs`, add:
```qmake
CONFIG += c++17
```

- [ ] **Step 4: Fix iris/conf.pri**

Read `iris/conf.pri`:
```bash
cat /Users/kweiss/Downloads/yachat/iris/conf.pri
```
It is generated by `psi.pro` at qmake time:
```
unix:system("echo \"include(../src/conf_iris.pri)\" > iris/conf.pri")
```
Read `src/conf_iris.pri` to check its contents:
```bash
cat /Users/kweiss/Downloads/yachat/src/conf_iris.pri
```
Ensure it doesn't reference qt3support.

- [ ] **Step 5: Attempt first qmake pass**

```bash
cd /Users/kweiss/Downloads/yachat
/opt/local/libexec/qt5/bin/qmake psi.pro -o Makefile
```

Expected: qmake runs without errors. Makefile generated.
If qmake is not at that path, find it: `which qmake` or `find /opt/local -name qmake -type f`.

---

### Task 6: Fix conf.pri for Qt5 and add YANDEX_EXTENSIONS define

**Files:**
- Modify: `conf.pri`

- [ ] **Step 1: Read current conf.pri**

Read `/Users/kweiss/Downloads/yachat/conf.pri`. Current content:
```
DEFINES += QCA_NO_PLUGINS OSSL_097 HAVE_OPENSSL HAVE_CONFIG
INCLUDEPATH += /usr/local/include /opt/local/include
LIBS += -lssl -lcrypto -L/opt/local/lib -lz
CONFIG += qca-static dbus release
PSI_DATADIR=/usr/local/share/yachat
```

- [ ] **Step 2: Update conf.pri**

Replace the entire file with:
```
PREFIX = /usr/local
BINDIR = /usr/local/bin
DATADIR = /usr/local/share

DEFINES += QCA_NO_PLUGINS HAVE_OPENSSL HAVE_CONFIG YANDEX_EXTENSIONS
INCLUDEPATH += /usr/local/include /opt/local/include
LIBS += -lssl -lcrypto -L/opt/local/lib -lz
CONFIG += qca-static release c++17
QMAKE_MACOSX_DEPLOYMENT_TARGET = 11.0
PSI_DATADIR=/usr/local/share/yachat
```

Changes: removed `OSSL_097` (OpenSSL 3.x doesn't need it), added `YANDEX_EXTENSIONS`, added `c++17`, added deployment target.

- [ ] **Step 3: Wrap xmpp_yalastmail.h usage in YANDEX_EXTENSIONS guard**

```bash
grep -rn "yalastmail\|YaLastMail" /Users/kweiss/Downloads/yachat/src/ /Users/kweiss/Downloads/yachat/iris/ --include="*.h" --include="*.cpp" -l
```

For each file found, wrap the include and any usage with:
```cpp
#ifdef YANDEX_EXTENSIONS
#include "xmpp_yalastmail.h"
// ... usage ...
#endif
```

---

## Layer 3: Qt3Support Removal

> This is the largest layer. 52 files have Q3* class usages. The strategy:
> 1. Bulk-fix trivial things (lowercase includes, simple collection types) with sed
> 2. Manually fix the 5 structurally complex cases (Q3MainWindow, Q3ListView×2, Q3SimpleRichText, custom paint)
> 3. Compile-iterate until clean

### Task 7: Bulk-fix lowercase includes and trivial Q3 collection aliases

**Files:** All 52 Q3* files, plus any file with lowercase Qt includes

- [ ] **Step 1: Find all lowercase Qt3-style includes**

```bash
grep -rn "#include <q[a-z]" /Users/kweiss/Downloads/yachat/src/ /Users/kweiss/Downloads/yachat/iris/ --include="*.h" --include="*.cpp" --include="*.mm" | head -50
```

- [ ] **Step 2: Bulk-replace lowercase includes**

```bash
cd /Users/kweiss/Downloads/yachat
# Fix common lowercase Qt includes to capitalized
find src iris -name "*.cpp" -o -name "*.h" -o -name "*.mm" | xargs sed -i '' \
  -e 's/#include <qtimer\.h>/#include <QTimer>/g' \
  -e 's/#include <qstring\.h>/#include <QString>/g' \
  -e 's/#include <qobject\.h>/#include <QObject>/g' \
  -e 's/#include <qapplication\.h>/#include <QApplication>/g' \
  -e 's/#include <qwidget\.h>/#include <QWidget>/g' \
  -e 's/#include <qdialog\.h>/#include <QDialog>/g' \
  -e 's/#include <qpixmap\.h>/#include <QPixmap>/g' \
  -e 's/#include <qpainter\.h>/#include <QPainter>/g' \
  -e 's/#include <qmenubar\.h>/#include <QMenuBar>/g' \
  -e 's/#include <qmenu\.h>/#include <QMenu>/g' \
  -e 's/#include <qaction\.h>/#include <QAction>/g' \
  -e 's/#include <qlabel\.h>/#include <QLabel>/g' \
  -e 's/#include <qlineedit\.h>/#include <QLineEdit>/g' \
  -e 's/#include <qcheckbox\.h>/#include <QCheckBox>/g' \
  -e 's/#include <qpushbutton\.h>/#include <QPushButton>/g' \
  -e 's/#include <qcombobox\.h>/#include <QComboBox>/g' \
  -e 's/#include <qsignalmapper\.h>/#include <QSignalMapper>/g' \
  -e 's/#include <qtextstream\.h>/#include <QTextStream>/g' \
  -e 's/#include <qpointer\.h>/#include <QPointer>/g' \
  -e 's/#include <qurl\.h>/#include <QUrl>/g' \
  -e 's/#include <qmessagebox\.h>/#include <QMessageBox>/g' \
  -e 's/#include <qicon\.h>/#include <QIcon>/g' \
  -e 's/#include <qevent\.h>/#include <QEvent>/g'
```

- [ ] **Step 3: Bulk-replace simple Q3 collection types**

```bash
find src iris -name "*.cpp" -o -name "*.h" | xargs sed -i '' \
  -e 's/Q3PtrList</QList</g' \
  -e 's/Q3ValueList</QList</g' \
  -e 's/Q3Dict</QHash<QString, /g' \
  -e 's/#include <q3ptrlist\.h>/#include <QList>/g' \
  -e 's/#include <q3valuelist\.h>/#include <QList>/g' \
  -e 's/#include <q3dict\.h>/#include <QHash>/g' \
  -e 's/#include <q3popupmenu\.h>/#include <QMenu>/g' \
  -e 's/Q3PopupMenu/QMenu/g' \
  -e 's/#include <q3textedit\.h>/#include <QTextEdit>/g' \
  -e 's/Q3TextEdit/QTextEdit/g'
```

- [ ] **Step 4: Verify no Q3PtrList/Q3Dict/Q3PopupMenu remain**

```bash
grep -rn "Q3PtrList\|Q3ValueList\|Q3Dict\|Q3PopupMenu\|Q3TextEdit" \
  /Users/kweiss/Downloads/yachat/src/ /Users/kweiss/Downloads/yachat/iris/ \
  --include="*.h" --include="*.cpp"
```

Expected: no output (or only commented-out lines). Fix any remaining ones manually.

---

### Task 8: Replace Q3MainWindow in mainwin.h/mainwin.cpp

`MainWin` inherits from `AdvancedWidget<Q3MainWindow>`. `AdvancedWidget` is a template in `advwidget.h` — we need to check if it works with `QMainWindow`.

**Files:**
- Read: `src/advwidget.h`
- Modify: `src/mainwin.h`
- Modify: `src/mainwin.cpp`

- [ ] **Step 1: Read advwidget.h to understand the template**

```bash
cat /Users/kweiss/Downloads/yachat/src/advwidget.h
```

`AdvancedWidget<T>` likely inherits from T and adds geometry save/restore. If so, switching the template parameter from `Q3MainWindow` to `QMainWindow` is all that's needed.

- [ ] **Step 2: Fix mainwin.h**

In `src/mainwin.h`:
```cpp
// BEFORE:
#include <Q3MainWindow>
// ...
class MainWin : public AdvancedWidget<Q3MainWindow>

// AFTER:
#include <QMainWindow>
// ...
class MainWin : public AdvancedWidget<QMainWindow>
```

Also remove the commented-out alternative:
```cpp
//class MainWin : public Q3MainWindow   ← delete this line
```

- [ ] **Step 3: Fix mainwin.cpp includes**

In `src/mainwin.cpp`, any `#include <Q3MainWindow>` or related Q3 includes → remove (already included via mainwin.h).

Also fix:
```cpp
// BEFORE:
#include <QMenuItem>   // Qt3 artifact, doesn't exist in Qt5

// AFTER: (remove this include entirely)
```

- [ ] **Step 4: Check mainwin.cpp for Q3-specific method calls**

```bash
grep -n "Q3\|statusBar()\|setDockEnabled\|setDockMenuEnabled\|moveToolBar\|addToolBar" \
  /Users/kweiss/Downloads/yachat/src/mainwin.cpp | head -30
```

`Q3MainWindow` had different toolbar/dockwidget APIs than `QMainWindow`. Fix each:
- `setDockEnabled(...)` → remove or replace with `setDockOptions(...)`
- Old-style `addToolBar(Qt::ToolBarDock, toolbar)` → `addToolBar(Qt::TopToolBarArea, toolbar)`

- [ ] **Step 5: Fix mainwin_p.cpp (helper file)**

```bash
grep -n "Q3MainWindow\|Q3\b" /Users/kweiss/Downloads/yachat/src/mainwin_p.cpp
```

Apply same pattern — replace Q3MainWindow with QMainWindow where found.

---

### Task 9: Replace Q3ListView in historydlg.h/cpp

`HistoryView` (a Q3ListView subclass) renders items with custom `paintCell`. The replacement is a `QTreeWidget` (simpler than full model/view since history is read-only display).

**Files:**
- Read: `src/historydlg.h`, `src/historydlg.cpp`
- Modify: both

- [ ] **Step 1: Read full historydlg.h and historydlg.cpp**

```bash
cat /Users/kweiss/Downloads/yachat/src/historydlg.h
cat /Users/kweiss/Downloads/yachat/src/historydlg.cpp
```

Note: `HistoryViewItem` uses `Q3SimpleRichText` for custom painting. `QTreeWidget` with a delegate replaces this.

- [ ] **Step 2: Replace the header**

Replace `src/historydlg.h` declarations:

```cpp
// BEFORE:
#include <q3listview.h>
class HistoryViewItem : public Q3ListViewItem { ... };
class HistoryView : public Q3ListView { ... };

// AFTER:
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QStyledItemDelegate>

class HistoryViewItem : public QTreeWidgetItem
{
public:
    HistoryViewItem(PsiEvent *, const QString &text, int id, QTreeWidget *parent);
    ~HistoryViewItem();

    QString text;
    int id;
    PsiEvent *e;
    QString eventId;
};

class HistoryView : public QTreeWidget
{
    Q_OBJECT
public:
    explicit HistoryView(QWidget *parent = nullptr);
    void doResize();
    // Keep any signals/slots from the original — read the .cpp to find them
};
```

Remove `Q3SimpleRichText *rt` — Qt5's `QTextDocument` handles rich text in delegates.

- [ ] **Step 3: Update historydlg.cpp**

For the `paintCell` override (custom item rendering), implement a `QStyledItemDelegate`:

```cpp
// In historydlg.cpp, add before HistoryView:
class HistoryDelegate : public QStyledItemDelegate {
public:
    explicit HistoryDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        // Draw background
        QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter);
        // Draw rich text via QTextDocument
        QTextDocument doc;
        doc.setHtml(index.data(Qt::DisplayRole).toString());
        painter->save();
        painter->translate(opt.rect.topLeft());
        QRect clip(0, 0, opt.rect.width(), opt.rect.height());
        doc.drawContents(painter, clip);
        painter->restore();
    }
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QTextDocument doc;
        doc.setHtml(index.data(Qt::DisplayRole).toString());
        doc.setTextWidth(option.rect.width() > 0 ? option.rect.width() : 300);
        return QSize(doc.idealWidth(), doc.size().height());
    }
};
```

Update `HistoryView` constructor to install the delegate:
```cpp
HistoryView::HistoryView(QWidget *parent) : QTreeWidget(parent) {
    setItemDelegate(new HistoryDelegate(this));
    setRootIsDecorated(false);
    setAllColumnsShowFocus(true);
}
```

Update `HistoryViewItem` constructor:
```cpp
HistoryViewItem::HistoryViewItem(PsiEvent *ev, const QString &t, int i, QTreeWidget *parent)
    : QTreeWidgetItem(parent), text(t), id(i), e(ev)
{
    setData(0, Qt::DisplayRole, t);
}
```

- [ ] **Step 4: Fix all remaining Q3ListView API calls in historydlg.cpp**

```bash
grep -n "firstChild\|nextSibling\|currentItem\|setSelected\|isSelected\|setOpen\|insertItem\|takeItem\|Q3ListView\|Q3ListViewItem" \
  /Users/kweiss/Downloads/yachat/src/historydlg.cpp
```

Replace each:
- `listView->firstChild()` → `listView->topLevelItem(0)`
- `item->nextSibling()` → `listView->itemBelow(item)`
- `listView->currentItem()` → `listView->currentItem()` (same in QTreeWidget)
- `item->setSelected(true)` → `item->setSelected(true)` (same)
- `listView->insertItem(item)` → `listView->addTopLevelItem(item)` (already handled in constructor)

---

### Task 10: Replace Q3ListView in eventdlg.h/cpp

Same pattern as historydlg but simpler — event list doesn't use custom painting.

**Files:**
- Read: `src/eventdlg.h`, `src/eventdlg.cpp`
- Modify: both

- [ ] **Step 1: Read both files**

```bash
grep -n "Q3\|q3" /Users/kweiss/Downloads/yachat/src/eventdlg.h
grep -n "Q3\|q3" /Users/kweiss/Downloads/yachat/src/eventdlg.cpp | head -40
```

- [ ] **Step 2: Replace Q3ListView with QTreeWidget**

In `src/eventdlg.h`:
```cpp
// BEFORE:
#include <Q3ListView>
class EventViewItem : public Q3ListViewItem { ... };
class EventView : public Q3ListView { ... };

// AFTER:
#include <QTreeWidget>
class EventViewItem : public QTreeWidgetItem { ... };
class EventView : public QTreeWidget { ... };
```

- [ ] **Step 3: Update eventdlg.cpp**

Apply same API replacements as in Task 9 Step 4. Key ones:
- `new EventViewItem(listView, ...)` constructor — `QTreeWidgetItem` takes `QTreeWidget*` as parent, same pattern
- Item navigation: `firstChild()` → `topLevelItem(0)`, `nextSibling()` → `itemBelow(item)`

---

### Task 11: Fix remaining Q3* usages in other files

The remaining 46 files with Q3* usage (from the grep in the file map) have lighter-touch issues. Address them file by file based on compile errors after attempting a build.

**Files:** All remaining files from the 52-file list not already handled.

- [ ] **Step 1: Attempt a build and collect errors**

```bash
cd /Users/kweiss/Downloads/yachat && make -j4 2>&1 | grep "error:" > /tmp/build_errors.txt
cat /tmp/build_errors.txt | head -50
```

- [ ] **Step 2: Fix errors in groups by type**

Work through errors top-down. Common patterns and fixes:

**Q3MainWindow toolbar APIs** (in `src/psitoolbar.cpp`, `src/tabs/tabdlg.cpp`):
```bash
grep -n "Q3MainWindow\|moveDockWindow\|lineUpDockWindows\|setDockEnabled" \
  /Users/kweiss/Downloads/yachat/src/psitoolbar.cpp \
  /Users/kweiss/Downloads/yachat/src/tabs/tabdlg.cpp
```
Replace with QMainWindow equivalents.

**Q3SocketDevice** (in iris network code):
```bash
grep -rn "Q3SocketDevice" /Users/kweiss/Downloads/yachat/iris/ --include="*.h" --include="*.cpp"
```
Replace with `QTcpSocket` / `QAbstractSocket` as appropriate.

**Q3ListViewItem in tasklist.h**:
```bash
cat /Users/kweiss/Downloads/yachat/src/tasklist.h
```
Replace any Q3ListViewItem with QTreeWidgetItem or QListWidgetItem.

**Q3*  in userlist.h**:
```bash
grep -n "Q3\b" /Users/kweiss/Downloads/yachat/src/userlist.h
```
Replace per type.

- [ ] **Step 3: Iterate until zero Q3* compile errors**

```bash
make -j4 2>&1 | grep "error:" | grep -i "q3\|qt3" | head -20
```

Repeat fixing and compiling until this produces no output.

---

### Task 12: Layer 3 checkpoint — first full compile

- [ ] **Step 1: Full clean build**

```bash
cd /Users/kweiss/Downloads/yachat && make clean && make -j4 2>&1 | tail -20
```

Expected: `make[1]: Nothing to be done for 'first'.` or similar success message. No `error:` lines.

- [ ] **Step 2: Run existing unit tests**

```bash
cd /Users/kweiss/Downloads/yachat/qa/unittest && make -j4 && ./unittest
cd /Users/kweiss/Downloads/yachat/iris/src/xmpp/jid/unittest && make -j4 && ./unittest
```

Expected: Tests pass. If any fail, fix before proceeding to Layer 4.

- [ ] **Step 3: Launch the app**

```bash
open /Users/kweiss/Downloads/yachat/src/yachat.app
```

Expected: App opens. Main window appears (may show no accounts yet — that's fine).

- [ ] **Step 4: Update CLAUDE.md**

In `CLAUDE.md`, mark Layer 1, 2, and 3 as complete:
```
- [x] Layer 1 — macOS-specific fixes
- [x] Layer 2 — qmake / Qt5 build fixes  
- [x] Layer 3 — Qt3Support removal
```

---

## Layer 4: QCA 2.3.x + TLS/SASL Security

### Task 13: Replace bundled QCA with QCA 2.3.x

QCA 2.3.x is available via MacPorts. Rather than building from source in `third-party/qca/`, we use the system-installed version.

**Files:**
- Modify: `psi.pro`
- Modify: `conf.pri`
- Modify: `iris/iris.pro` or `src/conf_iris.pri`

- [ ] **Step 1: Install QCA 2.3.x via MacPorts**

```bash
sudo port install qca +openssl
```

Verify install:
```bash
pkg-config --modversion qca2-qt5
```
Expected: `2.3.x`

Note the include path:
```bash
pkg-config --cflags qca2-qt5
pkg-config --libs qca2-qt5
```

- [ ] **Step 2: Disable bundled QCA in psi.pro**

In `psi.pro`, the bundled QCA is built when `CONFIG += qca-static`. Change `conf.pri` to disable this:

In `conf.pri`, change:
```
CONFIG += qca-static
```
to:
```
CONFIG -= qca-static
```

Then in `conf.pri`, add the system QCA paths (use output from pkg-config above):
```
INCLUDEPATH += /opt/local/include/qt5/QtCrypto
LIBS += -L/opt/local/lib -lqca-qt5
```
(Exact paths may differ — use pkg-config output.)

- [ ] **Step 3: Verify psi.pro no longer builds the bundled QCA subdir**

In `psi.pro`:
```qmake
qca-static {
    sub_qca.subdir = third-party/qca
    sub_iris.depends = sub_qca
    SUBDIRS += sub_qca
}
```
Since `qca-static` is no longer in CONFIG, this block won't execute. Confirm:
```bash
grep "qca-static" /Users/kweiss/Downloads/yachat/conf.pri
```
Expected: `CONFIG -= qca-static`

- [ ] **Step 4: Update #include paths in Iris for QCA 2.3.x**

QCA 2.3.x includes as `<QtCrypto>`. Verify current usage:
```bash
grep -rn "#include.*qca\|#include.*QtCrypto" /Users/kweiss/Downloads/yachat/iris/src/ --include="*.h" --include="*.cpp" | head -20
```
The existing `#include <QtCrypto>` usages are already correct for QCA 2.x. The old `#include "qca.h"` in `tlshandler.cpp` needs updating:

In `iris/src/xmpp/xmpp-core/tlshandler.cpp`:
```cpp
// BEFORE:
#include "qca.h"

// AFTER:
#include <QtCrypto>
```

- [ ] **Step 5: Fix QCA 2.3.x API changes**

QCA 2.3.x changed some enum values. Find and fix:
```bash
grep -rn "QCA::TLS\|QCA::SASL\|QCA::Certificate" /Users/kweiss/Downloads/yachat/iris/src/ --include="*.cpp" --include="*.h" | head -30
```

Known change: `QCA::TLS::TLSv1` is now `QCA::TLS::TLS_v1`. Check QCA 2.3.x release notes for full list. Fix each compile error as it appears.

- [ ] **Step 6: Build with system QCA**

```bash
cd /Users/kweiss/Downloads/yachat && /opt/local/libexec/qt5/bin/qmake psi.pro && make -j4 2>&1 | grep "error:" | head -20
```

Fix any QCA API errors until it compiles.

---

### Task 14: Enforce TLS 1.2+ and update SASL mechanism priority

**Files:**
- Modify: `iris/src/xmpp/xmpp-core/stream.cpp`

- [ ] **Step 1: Find where QCA::TLS is created in stream.cpp**

```bash
grep -n "new QCA::TLS\|QCA::TLS(" /Users/kweiss/Downloads/yachat/iris/src/xmpp/xmpp-core/stream.cpp
```

- [ ] **Step 2: Set minimum TLS protocol version to TLS 1.2**

After the `QCA::TLS` object is created (find the line with `new QCA::TLS`), add:

```cpp
// After: d->tls = new QCA::TLS(this);  (or wherever QCA::TLS is instantiated)
// Add:
d->tls->setConstraints(QCA::TLS::TLS_v1_2, QCA::TLS::TLS_v1_3);
```

If the TLS object is created elsewhere (e.g. in `connector.cpp`), find it:
```bash
grep -rn "new QCA::TLS\|QCA::TLS(" /Users/kweiss/Downloads/yachat/iris/src/ --include="*.cpp"
```
Apply the constraint there.

- [ ] **Step 3: Update SASL mechanism priority in stream.cpp**

Find the SASL startClient call at line ~1158:
```cpp
d->sasl->startClient("xmpp", QUrl::toAce(d->server), ml, QCA::SASL::AllowClientSendFirst);
```

Before this line, the mechanism list `ml` is built. Find that block (around line 1149–1158):
```cpp
QStringList ml;
if(!d->sasl_mech.isEmpty())
    ml += d->sasl_mech;
else
    ml = d->client.features.sasl_mechs;
```

After this block, insert mechanism prioritization:
```cpp
// Reorder ml to prefer SCRAM-SHA-256 > SCRAM-SHA-1 > PLAIN > DIGEST-MD5
// This ensures modern servers get the best mechanism first.
if (ml.isEmpty()) {
    ml = d->client.features.sasl_mechs;
}
{
    QStringList preferred = {"SCRAM-SHA-256", "SCRAM-SHA-1", "PLAIN", "DIGEST-MD5"};
    QStringList reordered;
    for (const QString &mech : preferred) {
        if (ml.contains(mech))
            reordered << mech;
    }
    // Append any remaining mechanisms not in our preferred list
    for (const QString &mech : ml) {
        if (!reordered.contains(mech))
            reordered << mech;
    }
    ml = reordered;
}
```

- [ ] **Step 4: Rebuild and test TLS/SASL**

Install Prosody 0.12 locally for testing:
```bash
sudo port install prosody
```

Configure Prosody with a self-signed cert:
```bash
sudo prosodyctl cert generate localhost
sudo prosodyctl start
sudo prosodyctl adduser test@localhost
```

Build and run yachat. Add an account pointing to `localhost`. Verify in Prosody logs:
```bash
sudo tail -f /var/log/prosody/prosody.log
```
Expected log line: `SASL: client selected SCRAM-SHA-256` and `TLS: handshake complete (TLSv1.2 or TLSv1.3)`.

- [ ] **Step 5: Update CLAUDE.md**

Mark Layer 4 complete:
```
- [x] Layer 4 — QCA 2.3.x (SCRAM-SHA-256, TLS 1.2+)
```

---

## Layer 5: Modern XMPP XEPs

### Task 15: XEP-0308 — Last Message Correction

This is the simplest XEP — just extend the existing Message class.

**Files:**
- Read: `iris/src/xmpp/xmpp-im/xmpp_message.h`, `iris/src/xmpp/xmpp-im/xmpp_message.cpp`
- Modify: both
- Modify: `src/tools/yastuff/yawidgets/yachatviewwidget.h/cpp` (add "edited" indicator)

- [ ] **Step 1: Find the Message serialization API**

```bash
grep -n "toXml\|fromXml\|parse\|fromStanza\|toStanza\|Stanza" \
  /Users/kweiss/Downloads/yachat/iris/src/xmpp/xmpp-im/xmpp_message.cpp | head -30
grep -n "fromXml\|parse\|fromStanza" \
  /Users/kweiss/Downloads/yachat/iris/src/xmpp/xmpp-im/xmpp_message.h
```

Note the exact method name and signature used to parse a `QDomElement` into a `Message`. This will be needed in Tasks 16 and 17 for parsing forwarded messages. Common patterns:
- `Message::fromXml(const QDomElement &e)` — static factory
- `msg.fromStanza(const Stanza &s)` — method on existing Message
- `msg.parseXml(const QDomElement &e)` — mutating parse

Record the actual name here before continuing.

- [ ] **Step 2: Add replaceId to xmpp_message.h**

In `iris/src/xmpp/xmpp-im/xmpp_message.h`, inside the `Message` class, add after the `XEP-0184` section:

```cpp
// XEP-0308 — Last Message Correction
QString replaceId() const;
void setReplaceId(const QString &id);
bool isCorrection() const; // returns !replaceId().isEmpty()
```

- [ ] **Step 3: Implement in xmpp_message.cpp**

In the `MessagePrivate` struct (find it: `grep -n "class MessagePrivate\|struct.*Private" xmpp_message.cpp`), add:
```cpp
QString replaceId;
```

Implement the accessors:
```cpp
QString Message::replaceId() const { return d->replaceId; }
void Message::setReplaceId(const QString &id) { d->replaceId = id; }
bool Message::isCorrection() const { return !d->replaceId.isEmpty(); }
```

- [ ] **Step 4: Serialize in toXml()**

Find the `toXml()` method (or equivalent). Before the closing `</message>`, add:
```cpp
if (!d->replaceId.isEmpty()) {
    QDomElement replace = doc.createElementNS(
        "urn:xmpp:message-correct:0", "replace");
    replace.setAttribute("id", d->replaceId);
    e.appendChild(replace);
}
```

- [ ] **Step 5: Deserialize in fromXml() / parse()**

Find where message child elements are parsed (look for `e.elementsByTagName` or iteration over child nodes). Add:
```cpp
// XEP-0308
{
    QDomElement replace = e.firstChildElement("replace");
    while (!replace.isNull()) {
        if (replace.namespaceURI() == "urn:xmpp:message-correct:0") {
            d->replaceId = replace.attribute("id");
        }
        replace = replace.nextSiblingElement("replace");
    }
}
```

- [ ] **Step 6: Add "edited" indicator in YaChatViewWidget**

```bash
grep -rn "body\|message\|setText\|addMessage" \
  /Users/kweiss/Downloads/yachat/src/tools/yastuff/yawidgets/yachatviewwidget.h \
  /Users/kweiss/Downloads/yachat/src/tools/yastuff/yawidgets/yachatviewwidget.cpp \
  | head -20
```

Find where message body text is rendered. Append ` <span style="color:gray;font-size:small">(edited)</span>` to the body HTML when `msg.isCorrection()` is true.

- [ ] **Step 7: Build and verify**

```bash
make -j4 2>&1 | grep "error:" | head -10
```

---

### Task 16: XEP-0280 — Message Carbons

Carbons lets the client receive copies of messages sent/received on other devices.

**Files:**
- Create: `iris/src/xmpp/xmpp-im/xmpp_carbons.h`
- Create: `iris/src/xmpp/xmpp-im/xmpp_carbons.cpp`
- Modify: `iris/src/xmpp/xmpp-im/xmpp.pro` (add sources)
- Modify: `src/psiaccount.cpp` (enable carbons on login, route carbon copies)

- [ ] **Step 1: Read an existing simple XEP task for the pattern to follow**

```bash
cat /Users/kweiss/Downloads/yachat/iris/src/xmpp/xmpp-im/xmpp_receipts.h
cat /Users/kweiss/Downloads/yachat/iris/src/xmpp/xmpp-im/xmpp_receipts.cpp
```

Use this as the structural template.

- [ ] **Step 2: Create xmpp_carbons.h**

```cpp
// xmpp_carbons.h — XEP-0280 Message Carbons
#ifndef XMPP_CARBONS_H
#define XMPP_CARBONS_H

#include "xmpp_task.h"
#include "xmpp_message.h"

namespace XMPP {

static const char* CARBONS_NS = "urn:xmpp:carbons:2";
static const char* FORWARD_NS  = "urn:xmpp:forward:0";

// Task: enables/disables carbons on the server.
class CarbonsEnableTask : public Task
{
    Q_OBJECT
public:
    CarbonsEnableTask(Task *parent, bool enable);
    bool take(const QDomElement &) override;
    void onGo() override;

private:
    bool enable_;
};

// Parses a forwarded message from a carbon copy stanza.
// Returns true and populates msg/isSent if the stanza is a carbon copy.
bool parseCarbonCopy(const QDomElement &stanza, Message &msg, bool &isSent);

} // namespace XMPP
#endif
```

- [ ] **Step 3: Create xmpp_carbons.cpp**

```cpp
// xmpp_carbons.cpp — XEP-0280 Message Carbons
#include "xmpp_carbons.h"
#include "xmpp_xmlcommon.h"
#include <QDomDocument>

using namespace XMPP;

CarbonsEnableTask::CarbonsEnableTask(Task *parent, bool enable)
    : Task(parent), enable_(enable)
{}

void CarbonsEnableTask::onGo()
{
    QDomDocument doc;
    QDomElement iq = createIQ(doc, "set", "", id());
    QDomElement e = doc.createElementNS(CARBONS_NS, enable_ ? "enable" : "disable");
    iq.appendChild(e);
    send(iq);
}

bool CarbonsEnableTask::take(const QDomElement &e)
{
    if (!iqVerify(e, "", id()))
        return false;
    if (e.attribute("type") == "result")
        setSuccess();
    else
        setError(e);
    return true;
}

bool XMPP::parseCarbonCopy(const QDomElement &stanza, Message &msg, bool &isSent)
{
    // A carbon copy looks like:
    // <message from="user@server" ...>
    //   <sent|received xmlns="urn:xmpp:carbons:2">
    //     <forwarded xmlns="urn:xmpp:forward:0">
    //       <message ...> (the actual message) </message>
    //     </forwarded>
    //   </sent|received>
    // </message>

    auto tryDir = [&](const QString &dir) -> bool {
        QDomElement dirElem = stanza.firstChildElement(dir);
        while (!dirElem.isNull()) {
            if (dirElem.namespaceURI() == CARBONS_NS) {
                QDomElement fwd = dirElem.firstChildElement("forwarded");
                if (!fwd.isNull() && fwd.namespaceURI() == FORWARD_NS) {
                    QDomElement inner = fwd.firstChildElement("message");
                    if (!inner.isNull()) {
                        Stanza s("message", inner);  // wrap for parsing
                        msg = Message();
                        msg.fromStanza(s);           // use existing parse
                        isSent = (dir == "sent");
                        return true;
                    }
                }
            }
            dirElem = dirElem.nextSiblingElement(dir);
        }
        return false;
    };

    return tryDir("sent") || tryDir("received");
}
```

**Note:** The exact `Message::fromStanza()` API — verify by reading `xmpp_message.cpp` to find the parse entry point, then adjust the call accordingly.

- [ ] **Step 4: Add to xmpp.pro**

```bash
grep -n "SOURCES\|HEADERS" /Users/kweiss/Downloads/yachat/iris/src/xmpp/xmpp-im/xmpp.pro | head -10
```

Add to the SOURCES and HEADERS lists:
```qmake
HEADERS += xmpp_carbons.h
SOURCES += xmpp_carbons.cpp
```

- [ ] **Step 5: Enable carbons in PsiAccount on login**

```bash
grep -n "connected\|onLogin\|loggedIn\|setStatus\|taskFinished" \
  /Users/kweiss/Downloads/yachat/src/psiaccount.cpp | head -20
```

Find the method called when login succeeds. Add carbons enable:
```cpp
// After successful login in psiaccount.cpp:
#include "xmpp_carbons.h"

// In the login success handler:
CarbonsEnableTask *carbonTask = new CarbonsEnableTask(client()->rootTask(), true);
carbonTask->go(true);
```

- [ ] **Step 6: Handle incoming carbon copies in PsiAccount**

Find where incoming stanzas/messages are dispatched in `psiaccount.cpp`:
```bash
grep -n "messageReceived\|incoming\|stanzaReceived\|Message msg" \
  /Users/kweiss/Downloads/yachat/src/psiaccount.cpp | head -20
```

In the stanza handling path, before routing a message, check if it's a carbon copy:
```cpp
#include "xmpp_carbons.h"

// In the message/stanza received handler:
XMPP::Message carbonMsg;
bool isSent = false;
if (XMPP::parseCarbonCopy(stanzaElement, carbonMsg, isSent)) {
    // Route carbon copy to the appropriate ChatDlg
    // isSent=true means we sent this on another device
    processMessage(carbonMsg, isSent);
    return;
}
```

Implement or locate `processMessage()` — it likely already exists for incoming messages.

- [ ] **Step 7: Build and smoke test**

```bash
make -j4 2>&1 | grep "error:" | head -10
```

With Prosody running and two XMPP clients connected (yachat + another client like Gajim), send a message from Gajim. Verify it appears in yachat as a carbon copy.

---

### Task 17: XEP-0313 — Message Archive Management (MAM)

MAM allows fetching message history from the server.

**Files:**
- Create: `iris/src/xmpp/xmpp-im/xmpp_mam.h`
- Create: `iris/src/xmpp/xmpp-im/xmpp_mam.cpp`
- Modify: `iris/src/xmpp/xmpp-im/xmpp.pro`
- Modify: `src/historydlg.cpp` (add MAM history toggle)
- Modify: `src/psiaccount.h/cpp` (add fetchMamHistory method)

- [ ] **Step 1: Create xmpp_mam.h**

```cpp
// xmpp_mam.h — XEP-0313 Message Archive Management
#ifndef XMPP_MAM_H
#define XMPP_MAM_H

#include "xmpp_task.h"
#include "xmpp_message.h"
#include <QList>
#include <QDateTime>

namespace XMPP {

static const char* MAM_NS = "urn:xmpp:mam:2";

struct MamResult {
    QString id;
    QDateTime timestamp;
    Message message;
};

// Fetches the last N messages from the server archive for a given JID.
class MamQueryTask : public Task
{
    Q_OBJECT
public:
    // with: the contact JID to fetch history for (empty = full archive)
    // limit: max number of messages to return (default 50)
    MamQueryTask(Task *parent, const Jid &with, int limit = 50);

    const QList<MamResult>& results() const;

    bool take(const QDomElement &) override;
    void onGo() override;

signals:
    void resultReceived(const XMPP::MamResult &result);

private:
    Jid with_;
    int limit_;
    QString queryId_;
    QList<MamResult> results_;
};

} // namespace XMPP
#endif
```

- [ ] **Step 2: Create xmpp_mam.cpp**

```cpp
// xmpp_mam.cpp — XEP-0313 Message Archive Management
#include "xmpp_mam.h"
#include "xmpp_xmlcommon.h"
#include "xmpp_carbons.h"  // for FORWARD_NS
#include <QUuid>
#include <QDomDocument>

using namespace XMPP;

MamQueryTask::MamQueryTask(Task *parent, const Jid &with, int limit)
    : Task(parent), with_(with), limit_(limit)
{
    queryId_ = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

const QList<MamResult>& MamQueryTask::results() const { return results_; }

void MamQueryTask::onGo()
{
    QDomDocument doc;
    QDomElement iq = createIQ(doc, "set", "", id());

    QDomElement query = doc.createElementNS(MAM_NS, "query");
    query.setAttribute("queryid", queryId_);

    // Filter by contact JID if specified
    if (!with_.isEmpty()) {
        QDomElement x = doc.createElementNS("jabber:x:data", "x");
        x.setAttribute("type", "submit");
        // FIELD: FORM_TYPE
        QDomElement ftField = doc.createElement("field");
        ftField.setAttribute("var", "FORM_TYPE");
        ftField.setAttribute("type", "hidden");
        QDomElement ftValue = doc.createElement("value");
        ftValue.appendChild(doc.createTextNode(MAM_NS));
        ftField.appendChild(ftValue);
        x.appendChild(ftField);
        // FIELD: with
        QDomElement withField = doc.createElement("field");
        withField.setAttribute("var", "with");
        QDomElement withValue = doc.createElement("value");
        withValue.appendChild(doc.createTextNode(with_.bare()));
        withField.appendChild(withValue);
        x.appendChild(withField);
        query.appendChild(x);
    }

    // RSM: request last `limit_` messages
    QDomElement set = doc.createElementNS("http://jabber.org/protocol/rsm", "set");
    QDomElement maxEl = doc.createElement("max");
    maxEl.appendChild(doc.createTextNode(QString::number(limit_)));
    set.appendChild(maxEl);
    QDomElement before = doc.createElement("before");
    set.appendChild(before); // empty before = last page
    query.appendChild(set);

    iq.appendChild(query);
    send(iq);
}

bool MamQueryTask::take(const QDomElement &e)
{
    // Collect <message> stanzas with our queryid (they arrive before the IQ result)
    if (e.tagName() == "message") {
        QDomElement result = e.firstChildElement("result");
        while (!result.isNull()) {
            if (result.namespaceURI() == MAM_NS &&
                result.attribute("queryid") == queryId_)
            {
                MamResult r;
                r.id = result.attribute("id");
                QDomElement fwd = result.firstChildElement("forwarded");
                if (!fwd.isNull() && fwd.namespaceURI() == FORWARD_NS) {
                    // Extract timestamp from <delay>
                    QDomElement delay = fwd.firstChildElement("delay");
                    if (!delay.isNull())
                        r.timestamp = QDateTime::fromString(
                            delay.attribute("stamp"), Qt::ISODate);
                    // Extract inner message
                    QDomElement msg = fwd.firstChildElement("message");
                    if (!msg.isNull()) {
                        Stanza s("message", msg);
                        r.message.fromStanza(s); // adjust if API differs
                        results_.append(r);
                        emit resultReceived(r);
                    }
                }
            }
            result = result.nextSiblingElement("result");
        }
        return false; // don't consume the message stanza
    }

    // The final IQ result/error
    if (!iqVerify(e, "", id()))
        return false;

    if (e.attribute("type") == "result")
        setSuccess();
    else
        setError(e);
    return true;
}
```

**Note:** Adjust `r.message.fromStanza(s)` based on the actual Message parse API found in Step 1 of Task 15.

- [ ] **Step 3: Add to xmpp.pro**

```qmake
HEADERS += xmpp_mam.h
SOURCES += xmpp_mam.cpp
```

- [ ] **Step 4: Add fetchMamHistory() to PsiAccount**

In `src/psiaccount.h`, add:
```cpp
public slots:
    void fetchMamHistory(const XMPP::Jid &with, int limit = 50);
signals:
    void mamHistoryReceived(const XMPP::Jid &with, const QList<XMPP::MamResult> &results);
```

In `src/psiaccount.cpp`:
```cpp
#include "xmpp_mam.h"

void PsiAccount::fetchMamHistory(const XMPP::Jid &with, int limit)
{
    MamQueryTask *task = new MamQueryTask(client()->rootTask(), with, limit);
    connect(task, &MamQueryTask::finished, this, [this, task, with]() {
        if (task->success())
            emit mamHistoryReceived(with, task->results());
    });
    task->go(true);
}
```

- [ ] **Step 5: Trigger MAM fetch on chat window open**

Find where `ChatDlg` or `YaChatViewWidget` is created/opened:
```bash
grep -rn "ChatDlg\|openChat\|chatWindow" /Users/kweiss/Downloads/yachat/src/psiaccount.cpp | head -10
```

In the chat open handler, after creating the chat dialog, call:
```cpp
fetchMamHistory(contact->jid(), 50);
```

Connect `mamHistoryReceived` to a slot in the chat dialog that prepends the history messages.

- [ ] **Step 6: Build and test**

Enable MAM in Prosody:
```
-- In /opt/local/etc/prosody/prosody.cfg.lua:
modules_enabled = { "mam"; }
```
Restart Prosody. Exchange a few messages. Restart yachat. Open the chat window. Verify history loads from server.

---

### Task 18: XEP-0384 — OMEMO Encryption

OMEMO is the most complex XEP. It uses the Signal Protocol for end-to-end encryption.

**Files:**
- Create: `third-party/libsignal-protocol-c/` (install via MacPorts or build from source)
- Create: `src/omemomanager.h`
- Create: `src/omemomanager.cpp`
- Modify: `src/src.pro` (add libsignal, sql)
- Modify: `src/chatdlg.h/cpp` or `src/tools/yastuff/yawidgets/yachatviewwidget.h/cpp` (OMEMO toggle button)
- Modify: `src/psiaccount.cpp` (integrate OmemoManager)

- [ ] **Step 1: Install libsignal-protocol-c**

```bash
sudo port install libsignal-protocol-c
```

Verify:
```bash
pkg-config --modversion libsignal-protocol-c
ls /opt/local/include/signal/
```

- [ ] **Step 2: Add to src/src.pro**

```qmake
QT += sql
INCLUDEPATH += /opt/local/include/signal
LIBS += -L/opt/local/lib -lsignal-protocol-c
```

- [ ] **Step 3: Create src/omemomanager.h**

```cpp
// omemomanager.h — XEP-0384 OMEMO Encryption manager
#ifndef OMEMOMANAGER_H
#define OMEMOMANAGER_H

#include <QObject>
#include <QByteArray>
#include <QString>

namespace XMPP { class Jid; class Client; }

// OmemoManager handles key generation, device list publishing (via PEP),
// session establishment, and message encrypt/decrypt for 1-on-1 chats.
// It stores all keys in a per-account SQLite database.
class OmemoManager : public QObject
{
    Q_OBJECT
public:
    explicit OmemoManager(XMPP::Client *client, const QString &accountId, QObject *parent = nullptr);
    ~OmemoManager();

    // Initialize: load or generate identity keys, publish device list via PEP.
    void initialize();

    // Returns true if an OMEMO session exists with the given JID.
    bool hasSession(const XMPP::Jid &jid) const;

    // Encrypt plaintext for recipient. Returns OMEMO <encrypted> XML element.
    // Returns null QDomElement if no session exists — caller must establish session first.
    QDomElement encrypt(const XMPP::Jid &recipient, const QString &plaintext);

    // Decrypt an OMEMO <encrypted> element. Returns plaintext, or empty string on failure.
    QString decrypt(const XMPP::Jid &sender, const QDomElement &encryptedElem);

    // Establish a session with recipient by fetching their device list and bundle.
    void establishSession(const XMPP::Jid &recipient);

signals:
    void sessionEstablished(const XMPP::Jid &jid);
    void decryptionFailed(const XMPP::Jid &sender, const QString &reason);

private:
    class Private;
    Private *d;
};
#endif
```

- [ ] **Step 4: Create src/omemomanager.cpp (skeleton)**

The Signal Protocol integration is complex. Write a working skeleton that handles the key lifecycle; full encrypt/decrypt is iterative.

```cpp
// omemomanager.cpp
#include "omemomanager.h"
#include <signal/signal_protocol.h>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include "xmpp_client.h"
#include "xmpp_jid.h"

static const char* OMEMO_NS = "eu.siacs.conversations.axolotl";

class OmemoManager::Private {
public:
    XMPP::Client *client;
    QString accountId;
    signal_context *signalCtx = nullptr;
    QSqlDatabase db;
    uint32_t deviceId = 0;

    QString dbPath() const {
        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dataDir);
        return dataDir + "/omemo_" + accountId + ".sqlite";
    }

    bool openDb() {
        db = QSqlDatabase::addDatabase("QSQLITE", "omemo_" + accountId);
        db.setDatabaseName(dbPath());
        if (!db.open()) {
            qWarning() << "OMEMO: failed to open database:" << db.lastError().text();
            return false;
        }
        QSqlQuery q(db);
        q.exec("CREATE TABLE IF NOT EXISTS identity (key TEXT PRIMARY KEY, value BLOB)");
        q.exec("CREATE TABLE IF NOT EXISTS sessions (jid TEXT, device_id INTEGER, record BLOB, PRIMARY KEY (jid, device_id))");
        return true;
    }
};

OmemoManager::OmemoManager(XMPP::Client *client, const QString &accountId, QObject *parent)
    : QObject(parent), d(new Private)
{
    d->client = client;
    d->accountId = accountId;
}

OmemoManager::~OmemoManager()
{
    if (d->signalCtx) signal_context_destroy(d->signalCtx);
    delete d;
}

void OmemoManager::initialize()
{
    if (!d->openDb()) return;

    // Initialize Signal Protocol context
    signal_context_create(&d->signalCtx, this);

    // TODO: Set up crypto provider callbacks using QCA
    // signal_context_set_crypto_provider(d->signalCtx, &provider);

    // Load or generate identity key pair
    QSqlQuery q(d->db);
    q.exec("SELECT value FROM identity WHERE key='identity_key'");
    if (!q.next()) {
        // Generate new identity key pair
        ratchet_identity_key_pair *keyPair = nullptr;
        signal_protocol_key_helper_generate_identity_key_pair(&keyPair, d->signalCtx);
        // TODO: serialize and store keyPair in db
        // TODO: generate registration ID and store as device_id
        RATCHET_UNREF(keyPair);
    }

    // Publish device list via PEP (requires PepManager integration)
    // TODO: publish device list
}

bool OmemoManager::hasSession(const XMPP::Jid &jid) const
{
    QSqlQuery q(d->db);
    q.prepare("SELECT COUNT(*) FROM sessions WHERE jid=?");
    q.addBindValue(jid.bare());
    q.exec();
    return q.next() && q.value(0).toInt() > 0;
}

QDomElement OmemoManager::encrypt(const XMPP::Jid &recipient, const QString &plaintext)
{
    Q_UNUSED(recipient); Q_UNUSED(plaintext);
    // TODO: implement Signal Protocol encrypt using stored session
    return QDomElement();
}

QString OmemoManager::decrypt(const XMPP::Jid &sender, const QDomElement &encryptedElem)
{
    Q_UNUSED(sender); Q_UNUSED(encryptedElem);
    // TODO: implement Signal Protocol decrypt
    return QString();
}

void OmemoManager::establishSession(const XMPP::Jid &recipient)
{
    Q_UNUSED(recipient);
    // TODO: fetch device list from PEP, fetch bundle for each device,
    // establish Signal session, emit sessionEstablished()
}
```

**Note on the TODO sections:** The Signal Protocol C API requires setting up several callback structs (crypto provider, store implementations for identity keys, sessions, pre-keys, and signed pre-keys). Each callback struct maps to a database table. Read `/opt/local/include/signal/signal_protocol.h` — it defines `signal_crypto_provider`, `signal_protocol_identity_key_store`, `signal_protocol_session_store`, etc. Each has a set of function pointers you implement using QSqlQuery against the SQLite tables created in `openDb()`. Implement them one at a time, verifying compile after each. The key generation call (`signal_protocol_key_helper_generate_identity_key_pair`) is the first real test of the crypto provider setup.

- [ ] **Step 5: Integrate OmemoManager into PsiAccount**

In `src/psiaccount.h`, add:
```cpp
class OmemoManager;
// ...
OmemoManager* omemoManager() const;
```

In `src/psiaccount.cpp`:
```cpp
#include "omemomanager.h"

// In PsiAccount constructor or login handler:
omemo_ = new OmemoManager(client(), jid().bare(), this);
omemo_->initialize();
```

- [ ] **Step 6: Add OMEMO toggle button to chat window**

Find the chat toolbar or button area:
```bash
grep -rn "sendButton\|toolbar\|addWidget\|QToolButton" \
  /Users/kweiss/Downloads/yachat/src/tools/yastuff/yawidgets/yachatviewwidget.cpp | head -10
```

Add a lock toggle button:
```cpp
// In chat dialog setup:
QPushButton *omemoBtn = new QPushButton(QIcon::fromTheme("security-high"), "");
omemoBtn->setCheckable(true);
omemoBtn->setToolTip("Enable OMEMO encryption");
// Add to toolbar/button layout
connect(omemoBtn, &QPushButton::toggled, this, &ChatDlg::setOmemoEnabled);
```

When OMEMO is enabled and a session exists, wrap the outgoing message body in `OmemoManager::encrypt()` and set the plaintext body to an empty string (standard OMEMO behavior).

- [ ] **Step 7: Build**

```bash
make -j4 2>&1 | grep "error:" | head -20
```

Fix any Signal Protocol API errors (the C API requires specific function signatures for callbacks).

- [ ] **Step 8: Smoke test OMEMO**

Connect two OMEMO-capable clients (yachat + Gajim with OMEMO plugin) to the local Prosody instance. Enable OMEMO in yachat for a 1-on-1 chat. Send a message. Verify Gajim decrypts it successfully.

---

### Task 19: Remove dead XMPP extensions

**Files:**
- Modify: `iris/src/xmpp/xmpp-im/xmpp_features.h/cpp`
- Modify: `iris/src/xmpp/xmpp.pro`
- Modify: any file referencing legacy groupchat or Google Voice

- [ ] **Step 1: Find legacy groupchat references**

```bash
grep -rn "jabber:iq:conference\|legacyGroupchat\|legacy.*group" \
  /Users/kweiss/Downloads/yachat/src/ /Users/kweiss/Downloads/yachat/iris/ \
  --include="*.h" --include="*.cpp" -l
```

- [ ] **Step 2: Remove legacy groupchat from features.cpp**

In `iris/src/xmpp/xmpp-im/xmpp_features.cpp` (or wherever `jabber:iq:conference` is listed), remove or comment out the feature advertisement:
```cpp
// Remove:
features << "jabber:iq:conference";
```

- [ ] **Step 3: Find and remove Google Voice extension**

```bash
grep -rn "google.*voice\|voice.*v1\|libjingle" \
  /Users/kweiss/Downloads/yachat/src/ /Users/kweiss/Downloads/yachat/iris/ \
  --include="*.h" --include="*.cpp" --include="*.pri" --include="*.pro" -l
```

Remove feature advertisement and any compile-time includes. If `third-party/libjingle.new/` is referenced in a `.pro` file, remove that subdir reference.

- [ ] **Step 4: Final build**

```bash
make -j4 2>&1 | grep "error:" | head -10
```

---

## Final Checkpoint

### Task 20: Full smoke test and CLAUDE.md update

- [ ] **Step 1: Full clean build**

```bash
cd /Users/kweiss/Downloads/yachat && make clean && make -j4 2>&1 | tail -10
```

Expected: zero errors.

- [ ] **Step 2: Launch app**

```bash
open /Users/kweiss/Downloads/yachat/src/yachat.app
```

- [ ] **Step 3: Run all 5 smoke tests**

With Prosody 0.12 running locally:
1. Add account `test@localhost`, verify login succeeds with SCRAM-SHA-256 over TLS 1.2 (check Prosody log)
2. Open chat with a second test account, send "hello" → verify received
3. Connect a second client (Gajim), send a message → verify it appears in yachat via carbons
4. Restart yachat, open the chat → verify last 50 messages load via MAM
5. Enable OMEMO, send a message to a second OMEMO-capable client → verify decrypted correctly

- [ ] **Step 4: Update CLAUDE.md**

Mark all layers complete and note any known issues:
```
- [x] Layer 1 — macOS-specific fixes
- [x] Layer 2 — qmake / Qt5 build fixes
- [x] Layer 3 — Qt3Support removal
- [x] Layer 4 — QCA 2.3.x (SCRAM-SHA-256, TLS 1.2+)
- [x] Layer 5 — New XEPs (carbons, MAM, message correction, OMEMO)
```

Add any deferred work or known issues discovered during implementation.
