# yaonline-claude-revival-experiment

> Экспериментальная модернизация Ya.Online 3.0.3 — старого XMPP-клиента Яндекса (форк Psi, 2009 год) — с помощью Claude Code и ChatGPT Codex.

---

## О проекте

**Ya.Online** (внутреннее название **YaChat**) — это Jabber/XMPP-клиент с брендингом Яндекса, основанный на открытом клиенте [Psi](http://psi-im.org). Исходный код был опубликован 16 декабря 2009 года.

- **Руководитель проекта:** Дмитрий Матвеев
- **Ведущий разработчик:** Михаил Пищагин
- **Лицензия:** GNU General Public License v2
- **Оригинальная кодовая база:** ~900 файлов, ~189 000 строк C++/Qt

Этот репозиторий — эксперимент: можно ли оживить 15-летний Qt4-проект до рабочего состояния на современном macOS с помощью ИИ-агента (Claude+ChatGPT)?

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

> ✅ **Layer 3 завершён.** Все исходные файлы компилируются без ошибок. Бинарник `yachat.app` (8.8 МБ) успешно собран. Переходим к Layer 4 (QCA 2.3.x, TLS 1.2+, SCRAM-SHA-256).

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

### Слой 3 — Удаление Qt3Support ✅ *завершён 2026-04-16*

| Задача | Статус | Описание |
|---|---|---|
| Массовая замена (Задача 7) | ✅ | `#include <qfoo.h>`→`<QFoo>`, `Q3PtrList`/`Q3PopupMenu`/`Q3TextEdit` заменены, итераторы → range-for |
| `Q3MainWindow` (Задача 8) | ✅ | `mainwin.h/cpp` → `QMainWindow`, `mainwin_p.cpp` Q3ToolBar убраны |
| `Q3ListView` в HistoryDlg / EventDlg (Задачи 9–10) | ✅ | `QTreeWidget` + `QStyledItemDelegate`/`QTreeWidgetItem` |
| **`Q3ListView` в ContactView** | ✅ | Полный порт: `Q3ListView`→`QTreeWidget`, `Q3ListViewItem`→`QTreeWidgetItem`, `Q3DragObject`→`QDrag/QMimeData`, `ContactViewDelegate` |
| Qt4 API: mainwin/infodlg/psiaccount/psicon | ✅ | `QMenuBar::insertItem`→`addMenu`, `QUrl::queryItems`→`QUrlQuery`, `className()`→`metaObject()->className()`, `removeRef`→`removeAll` и др. |
| Qt4 API: misc (eventdb, vcardfactory, serverlistquerier, rc, xdata_widget и др.) | ✅ | `QHttp`→`QNetworkAccessManager`, `setEncoding`→`setCodec`, `setOn`→`setChecked`, `QStyleOptionViewItemV4`, JsonQt C++17 throw-спеки |
| yastuff (все 322 файла) | ✅ | `QTextControl`→`QWidgetTextControl` (через shim), `Qt::escape`→`toHtmlEscaped()`, `WStyle_*`→Qt5 флаги, `QMenuItem` удалён, `Q_WS_MAC`→`Q_OS_MAC` |
| Статические плагины | ✅ | `Q_IMPORT_PLUGIN` исправлен на правильные имена классов (`ITunesPlugin`, `PsiFilePlugin`) |
| **Контрольная сборка (Задача 12)** | ✅ | **`yachat.app` собран успешно (8.8 МБ)** — 16 апреля 2026 |

### Слой 4 — QCA 2.3.x + современная аутентификация 🔄 *следующий*

| Задача | Статус | Описание |
|---|---|---|
| Замена QCA | 🔲 | Системная QCA 2.3.x вместо встроенной 2.0.x |
| TLS 1.2+ минимум | 🔲 | `QSsl::TlsV1_2OrLater` в `xmpp_clientstream.cpp` |
| SCRAM-SHA-256 | 🔲 | Добавить через QCA 2.3.x SASL-провайдер |
| SCRAM-SHA-1 fallback | 🔲 | Резервный механизм, пока SCRAM-256 не подтверждён везде |
| Удалить `jabber:iq:auth` | 🔲 | XEP-0078, устарел; `protocol.cpp` строки ~1209–1541 |
| Удалить DIGEST-MD5 | 🔲 | RFC 6331 deprecated 2011; Prosody 0.12 его не поддерживает |
| PLAIN только через TLS | 🔲 | Запретить PLAIN без проверенного TLS-соединения |
| Тест на Prosody 0.12+ | 🔲 | Подключение, аутентификация, обмен сообщениями |

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

*Последнее обновление: 2026-04-16 (Claude: **Layer 3 полностью завершён** — `yachat.app` 8.8 МБ собран без ошибок. Все Qt3/Qt4 API портированы: contactview→QTreeWidget, yastuff Qt::escape/QTextControl/QMenuItem, JsonQt C++17, macspellchecker ARC, статические плагины, ресурсы psi.qrc. Следующий шаг: Layer 4 — QCA 2.3.x, TLS 1.2+, SCRAM-SHA-256)*
