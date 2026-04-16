# yaonline-claude-revival-experiment

> Экспериментальная модернизация Ya.Online 3.0.3 — старого XMPP-клиента Яндекса (форк Psi, 2009 год) — с помощью Claude Code.

---

## О проекте

**Ya.Online** (внутреннее название **YaChat**) — это Jabber/XMPP-клиент с брендингом Яндекса, основанный на открытом клиенте [Psi](http://psi-im.org). Исходный код был опубликован 16 декабря 2009 года.

- **Руководитель проекта:** Дмитрий Матвеев
- **Ведущий разработчик:** Михаил Пищагин
- **Лицензия:** GNU General Public License v2
- **Оригинальная кодовая база:** ~900 файлов, ~189 000 строк C++/Qt

Этот репозиторий — эксперимент: можно ли оживить 15-летний Qt4-проект до рабочего состояния на современном macOS с помощью ИИ-агента (Claude)?

---

## Цель модернизации

Собрать и запустить клиент на современном macOS (Ventura / Sonoma / Sequoia):
- подключение к XMPP-серверу
- отправка и получение сообщений
- современная безопасность (SCRAM-SHA-256, TLS 1.2+)
- актуальные XMPP-расширения (карбонные копии, MAM, OMEMO)

---

## Сборка (macOS, текущее состояние)

### Требования

| Зависимость | Версия | Способ установки |
|---|---|---|
| macOS | 11.0+ | — |
| Xcode Command Line Tools | последняя | `xcode-select --install` |
| Qt 5.15 LTS | 5.15.x | `brew install qt@5` |
| OpenSSL | 3.x | `brew install openssl` или MacPorts |
| zlib | системная | встроена в macOS |
| QCA 2.3.x | 2.3+ | `sudo port install qca +openssl` |

### Шаги сборки

```bash
# 1. Клонировать репозиторий
git clone https://github.com/kira-we1ss/yaonline-claude-revival-experiment.git
cd yaonline-claude-revival-experiment

# 2. Запустить qmake (Qt5 из Homebrew)
/usr/local/opt/qt@5/bin/qmake psi.pro

# 3. Собрать
make -j$(sysctl -n hw.ncpu)

# 4. Запустить
open src/yachat.app
```

> ⚠️ **Проект находится в активной разработке.** Сборка на данный момент не гарантируется — Layer 3 (удаление Qt3Support) всё ещё в процессе. В `contactview` уже перенесён drop-side drag/decode слой с `Q3UriDrag`/`Q3TextDrag` на `QMimeData`, а source-side text drag больше не зависит от `Q3TextDrag` и формируется как явный `text/plain` payload; при этом сам виджет всё ещё держится на `Q3ListView`/`Q3DragObject`, и впереди остаются `gcuserview` и `accountmanagedlg`.

---

## Архитектура модернизации

Миграция выполняется послойно. Каждый слой должен дать рабочую сборку перед переходом к следующему.

```
Слой 5: Новые XMPP XEP (карбоны, MAM, исправление сообщений, OMEMO)
Слой 4: QCA 2.3.x (SCRAM-SHA-256, минимум TLS 1.2)
Слой 3: Удаление Qt3Support (559+ использований Q3* → Qt5)
Слой 2: Исправление системы сборки (qmake, Qt 5.15)
Слой 1: Исправления macOS (Growl, Carbon API, Sparkle)
```

---

## Журнал прогресса

### Слой 1 — Исправления macOS ✅ *завершён 2026-04-14*

| Задача | Статус | Описание |
|---|---|---|
| Удаление Sparkle | ✅ | Убран мёртвый фреймворк автообновления (требует подписи кода) |
| Замена Growl | ✅ | Growl → заглушка на `QSystemTrayIcon` (Growl мёртв с macOS 10.9+) |
| Carbon API | ✅ | `ProcessSerialNumber`/`SetFrontProcess` → Cocoa, `__bridge_transfer` → `CFBridgingRelease()` |

### Слой 2 — Система сборки ✅ *завершён 2026-04-14*

| Задача | Статус | Описание |
|---|---|---|
| `src/src.pro` | ✅ | `qt3support` удалён, добавлены `widgets`/`concurrent`/`sql`, `c++17`, `macOS 11.0` |
| `iris/iris.pro` | ✅ | Добавлен `c++17`, Qt3 зависимостей не было |
| `conf.pri` | ✅ | `OSSL_097` удалён, добавлены `YANDEX_EXTENSIONS`/`c++17`/`DEPLOYMENT_TARGET` |
| Расширения Яндекса | ✅ | Весь код `xmpp_yalastmail` обёрнут в `#ifdef YANDEX_EXTENSIONS` |

### Слой 3 — Удаление Qt3Support 🔄 *в процессе*

| Задача | Статус | Описание |
|---|---|---|
| Массовая замена (Задача 7) | ✅ | 422 строки `#include <qfoo.h>` → `<QFoo>`, `Q3PtrList`/`Q3PopupMenu`/`Q3TextEdit` заменены, 63 итератора `Q3PtrListIterator` → range-for |
| `Q3MainWindow` (Задача 8) | ✅ | `mainwin.h/cpp` → `QMainWindow`, `mainwin_p.cpp` Q3ToolBar-зависимости убраны |
| `Q3ListView` в HistoryDlg (Задача 9) | ✅ | `historydlg.h/cpp` → `QTreeWidget` + `QStyledItemDelegate` |
| `Q3ListView` в EventDlg (Задача 10) | ✅ | `eventdlg.h/cpp` → `QTreeWidget` + `QTreeWidgetItem` |
| Остальные Q3* (Задача 11) | 🔄 | `searchdlg`, `discodlg`, `pgpkeydlg`, `proxy`, `iconset`, `tabdlg` завершены; в `contactview` переведён drop-side drag/decode слой на `QMimeData`, source-side text drag больше не использует `Q3TextDrag`, а рядом подтянуты локальные Qt5-cleanup швы (`std::sort`, new-style timer/icon connections), но `Q3ListView`/`Q3ListViewItem`/`Q3DragObject` и связанная архитектура ещё остаются; `gcuserview` и `accountmanagedlg` — всё ещё впереди |
| Контрольная сборка (Задача 12) | 🔲 | Первая полная сборка под Qt 5.15 |

### Слой 4 — QCA 2.3.x + безопасность 🔲 *не начат*

| Задача | Статус | Описание |
|---|---|---|
| Замена QCA | 🔲 | Системная QCA 2.3.x вместо встроенной |
| TLS 1.2+ / SCRAM-SHA-256 | 🔲 | Минимальная версия TLS, приоритет SCRAM в SASL |

### Слой 5 — Новые XMPP XEP 🔲 *не начат*

| Задача | Статус | Описание |
|---|---|---|
| XEP-0308 | 🔲 | Исправление последнего сообщения |
| XEP-0280 | 🔲 | Карбонные копии сообщений |
| XEP-0313 | 🔲 | Архив сообщений (MAM) |
| XEP-0384 | 🔲 | OMEMO шифрование (только личные чаты) |
| Удаление мёртвых XEP | 🔲 | Групповые чаты legacy, Google Voice |

---

## Что осталось от Яндекса

Весь брендинг и кастомный UI Яндекса **сохранён** (`src/tools/yastuff/`, ~322 файла):
- `YaWindow` — кастомные окна
- `YaChatViewWidget` — отображение чата
- `YaRosterWidget` — список контактов
- `YaMucManager` — групповые чаты
- Интеграция с сервисами Яндекса (за флагом `YANDEX_EXTENSIONS`)

---

## Технический стек

- **Qt:** 5.15 LTS (Homebrew)
- **Компилятор:** Apple Clang (Xcode), стандарт C++17
- **Криптография:** QCA 2.3.x + OpenSSL 3.x
- **XMPP:** встроенная библиотека Iris (расширяется, не заменяется)
- **Платформа:** macOS 11.0+
- **Система сборки:** qmake

---

*Последнее обновление: 2026-04-16 (Layer 3 всё ещё в работе; в `contactview` обновлён drag/drop batch: drop-side уже на `QMimeData`, source-side text drag больше не использует `Q3TextDrag`, но базовый `Q3ListView` seam ещё не снят)*
