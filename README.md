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
- подключение к XMPP-серверу ✅
- отправка и получение сообщений ✅
- современная безопасность (SCRAM-SHA-1/256, TLS 1.2+) ✅
- актуальные XMPP-расширения (карбоны, MAM, OMEMO, XEP-0045 и др.) 🔄

---

## Сборка (macOS, текущее состояние)

### Требования

| Зависимость | Версия | Способ установки |
|---|---|---|
| macOS | 11.0+ | — |
| Xcode Command Line Tools | последняя | `xcode-select --install` |
| Qt 5.15 LTS | 5.15.x | `brew install qt@5` |
| OpenSSL | 3.x | MacPorts (`/opt/local`) |
| QCA 2.3.7 | 2.3.7 | Собирается из исходников (см. conf.pri) |

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

> ✅ **Слои 1–5 почти завершены (14/15 XEP).** Приложение подключается к Prosody 0.12+, аутентифицируется по SCRAM-SHA-1, загружает ростер и MUC. Слой 6 (UI + новые кнопки) в процессе.

---

## Архитектура модернизации

Миграция выполняется послойно. Каждый слой должен дать рабочую сборку перед переходом к следующему.

```
Слой 7: Полный аудит кода, устранение ошибок, рефакторинг (после 4–6)
Слой 6: UI-рендеринг + новые кнопки (OMEMO toggle, correction)     🔄
Слой 5: Новые XMPP XEP (15/15 — все завершены)                    ✅
Слой 4: QCA 2.3.x + SCRAM-SHA-1/256 + TLS 1.2+                    ✅
Слой 3: Удаление Qt3Support (559+ использований Q3* → Qt5)         ✅
Слой 2: Исправление системы сборки (qmake, Qt 5.15)                ✅
Слой 1: Исправления macOS (Growl, Carbon API, Sparkle)             ✅
```

---

## Журнал прогресса

### Слой 1 — Исправления macOS ✅ *завершён 2026-04-14*

| Задача | Статус | Описание |
|---|---|---|
| Удаление Sparkle | ✅ | Убран мёртвый фреймворк автообновления |
| Замена Growl | ✅ | Growl → заглушка на `QSystemTrayIcon` |
| Carbon API | ✅ | `ProcessSerialNumber`/`SetFrontProcess` → Cocoa |

### Слой 2 — Система сборки ✅ *завершён 2026-04-14*

| Задача | Статус | Описание |
|---|---|---|
| `src/src.pro` | ✅ | `qt3support` удалён, `c++17`, `macOS 11.0` |
| `iris/iris.pro` | ✅ | Добавлен `c++17` |
| `conf.pri` | ✅ | QCA 2.3.7, `YANDEX_EXTENSIONS`, `DEPLOYMENT_TARGET` |

### Слой 3 — Удаление Qt3Support ✅ *завершён 2026-04-16*

| Задача | Статус | Описание |
|---|---|---|
| Массовая замена Q3* | ✅ | 559+ замен: `Q3PtrList`, `Q3PopupMenu`, `Q3TextEdit`, `Q3ListView` и др. |
| `Q3MainWindow` | ✅ | `mainwin.h/cpp` → `QMainWindow` |
| `Q3ListView` в ContactView | ✅ | Полный порт: `QTreeWidget` + `QTreeWidgetItem` + `QDrag/QMimeData` |
| Qt4 API (misc) | ✅ | `QHttp`→`QNetworkAccessManager`, Qt::escape→toHtmlEscaped и др. |
| yastuff (322 файла) | ✅ | `QTextControl` shim, `Q_WS_*`→`Q_OS_*`, Qt5 флаги окон |
| Контрольная сборка | ✅ | **`yachat.app` собран (8.8 МБ)** — 16 апреля 2026 |

### Слой 4 — QCA 2.3.x + современная аутентификация ✅ *завершён 2026-04-17*

| # | Задача | Статус | Описание |
|---|--------|--------|----------|
| 1 | QCA 2.3.7 | ✅ | Собран из исходников с ossl-плагином; встроенная 2.0.1 отключена |
| 1а–г | Цепочка крашей | ✅ | QCA double-load, SecureStream SIGSEGV, бесконечный reconnect — устранены |
| 2 | TLS 1.2+ минимум | ✅ | `QSsl::TlsV1_2OrLater` в `tlshandler.cpp` |
| 3 | SCRAM-SHA-256 | ✅ | Реализован в `simplesasl.cpp` через QCA 2.3.7 (PBKDF2 + HMAC) |
| 4 | SCRAM-SHA-1 | ✅ | Fallback; **подтверждён на Prosody 0.12+** |
| 5 | Удаление `jabber:iq:auth` | ✅ | XEP-0078 legacy заменён на немедленный ErrProtocol |
| 6 | Удаление DIGEST-MD5 | ✅ | Убран из simplesasl; SCRAM покрывает всё |
| 7 | PLAIN только поверх TLS | ✅ | `AllowPlain` → `AllowPlainOverTLS` в psiaccount.cpp |
| 8 | Тест на Prosody 0.12+ | ✅ | SCRAM-SHA-1 подтверждён, ростер и MUC загружены |

### Слой 6 — Исправления UI-рендеринга + новые кнопки 🔄 *в процессе*

| Баг / Задача | Статус | Описание |
|-----|--------|----------|
| A: Чёрный текст в чате | ✅ | `YaChatView` viewport + QTextControl palette: Base/Window=white |
| B: Чёрный прямоугольник в контактах | ✅ | `YaToolBox::normalPage_` + `YaWindowTheme::bottomColor` → white |
| C: `NSRequiresAquaSystemAppearance` | ✅ | Info.plist: принудительный Aqua (light) режим |
| D: Глобальная светлая палитра | ✅ | `app.setPalette(lightPal)` в main.cpp |
| E: Таббар — серый фон | ✅ | `YaTabBar::paintEvent` заполняет #CCCCCC перед вкладками |
| F: Поле ввода | ✅ | `YaChatEdit::setAutoFillBackground(true)` + белая палитра |
| G: Крашь при закрытии MUC вкладки | ✅ | `TabManager::tabDestroyed`: `qobject_cast` → `static_cast` |
| H: Крашь настроек чата (MUCAffiliations) | ✅ | `QSignalBlocker` + `proxy->invalidate()` в `getItemsByAffiliation_success` |
| I: Таббар — текст/позиция | 🔄 | Вкладки визуально смещены; в работе |
| **F: Кнопка OMEMO-замок в заголовке чата** | 🔲 | `QToolButton` в `YaChatHeader`/`YaChatToolBar`; зелёный=вкл, серый=выкл; клик переключает OMEMO |
| **G: Кнопка исправления сообщения в поле ввода** | 🔲 | Кнопка-карандаш рядом с «Отправить»; клик подставляет последнее сообщение с `~`; метка "(изменено)" |

### Слой 5 — Новые XMPP XEP ✅ *завершён 2026-04-17*

Реализация 15 расширений, поддерживаемых современными десктопными клиентами (Gajim, Dino, Psi+):

| Приоритет | XEP | Название | Статус |
|-----------|-----|----------|--------|
| 1 | XEP-0280 | Message Carbons | ✅ |
| 2 | XEP-0313 | MAM (полная синхронизация с сервером) | ✅ |
| 3 | XEP-0308 | Исправление последнего сообщения | ✅ |
| 4 | XEP-0384 | OMEMO (чтение + отправка, переключатель) | ✅ |
| 5 | XEP-0045 | MUC современный (замена `jabber:iq:conference`) | ✅ |
| 6 | XEP-0363 | HTTP File Upload | ✅ |
| 7 | XEP-0184 | Message Delivery Receipts | ✅ |
| 8 | XEP-0333 | Chat Markers | ✅ |
| 9 | XEP-0085 | Chat State Notifications | ✅ |
| 10 | XEP-0048 | Bookmarks (v1 + автоподключение) | ✅ |
| 11 | XEP-0199 | XMPP Ping | ✅ |
| 12 | XEP-0352 | Client State Indication | ✅ |
| 13 | XEP-0249 | Direct MUC Invitations | ✅ |
| 14 | XEP-0030 | Service Discovery (расширен) | ✅ |
| 15 | XEP-0115 | Entity Capabilities (SHA-1 хэш) | ✅ |

**Детали реализации:**
- XEP-0280: парсинг `<sent>`/`<received>` обёрток, включение после входа, подавление уведомлений для carbon-копий
- XEP-0313: новый Task `JT_MAMQuery` с RSM-пагинацией; синхронизация 7 дней после входа, до 200 сообщений
- XEP-0308: поле `replaceId` в Message; `~` в начале строки = исправление последнего сообщения
- XEP-0184: исправлен критический баг — `<received>` теперь корректно содержит атрибут `id=`
- XEP-0333: `<markable/>` в исходящих, `<displayed>` при фокусировке окна; маркеры обновляют статус доставки
- XEP-0085: добавлен таймер 2 мин → StateInactive; отправка StateActive при фокусировке; текстовая метка "печатает..."
- XEP-0363: новый Task `JT_HttpUploadSlot`; замена YaNarodDiskManager на XEP-0363 slot+PUT
- XEP-0363: кнопка "Отправить файл" уже существовала; теперь работает через стандартный протокол
- XEP-0048: убрана YAPSI-блокировка автоподключения к закладкам
- XEP-0030/0115: `computeVerHash()` (SHA-1), `ServerInfoManager` расширен (MAM, carbons, HTTP upload)
- XEP-0249: прямые приглашения теперь создают `GroupchatInviteEvent` (ранее игнорировались)
- XEP-0045: `http://jabber.org/protocol/muc` приоритизирован над `jabber:iq:conference`
- XEP-0384: `OmemoManager` — libsignal-protocol-c 2.3.3 (X3DH + Double Ratchet); AES-128-GCM через OpenSSL; TOFU доверие; `xmpp_omemo.h/cpp` в обеих копиях Iris; хук расшифровки в `psiaccount.cpp`

### Слой 7 — Полный аудит кода 🔲 *не начат (после слоёв 4–6)*

Одноразовый проход по всей кодовой базе (~900 файлов):

- **Фаза 1:** Аудит всех правок слоёв 1–6 с помощью `code-review` навыка
- **Фаза 2:** Полное сканирование — null proxy models, `qobject_cast` в `destroyed()`, `QList::first()` на пустых списках, deprecated Qt5 API, `parent=0` виджеты без палитры
- **Фаза 3:** Проверка компилятора — сборка с `-Wall -Wextra`, цель: ноль предупреждений и ошибок
- **Фаза 4:** Рефакторинг — удаление мёртвого кода (`YAPSI_ACTIVEX_SERVER`, `jabber:iq:auth` остатки, `DIGEST-MD5` скелет, неиспользуемая QCA 2.0.x)
- **Фаза 5:** Финальная верификация — все известные краши воспроизведены и подтверждены исправленными

---

## Что осталось от Яндекса

Весь брендинг и кастомный UI Яндекса **сохранён** (`src/tools/yastuff/`, ~322 файла):
- `YaWindow` — кастомные окна с жёлтым градиентным заголовком
- `YaChatViewWidget` — отображение чата
- `YaRosterWidget` — список контактов с аватарами
- `YaMucManager` — групповые чаты
- Интеграция с сервисами Яндекса (за флагом `YANDEX_EXTENSIONS`)

---

## Технический стек

- **Qt:** 5.15.18 LTS (Homebrew)
- **Компилятор:** Apple Clang (Xcode), стандарт C++17
- **Криптография:** QCA 2.3.7 (собран из исходников) + OpenSSL 3.x (MacPorts)
- **SASL:** SimpleSASL с SCRAM-SHA-256/SHA-1 (RFC 5802, реализован через QCA PBKDF2/HMAC)
- **XMPP:** встроенная библиотека Iris (расширяется, не заменяется)
- **Платформа:** macOS 11.0+
- **Система сборки:** qmake

---

*Последнее обновление: 2026-04-17 (Claude: **Слой 5 завершён (15/15 XEP).** XEP-0384 OMEMO реализован: `OmemoManager` на libsignal-protocol-c 2.3.3, X3DH + Double Ratchet, AES-128-GCM, TOFU доверие, хук в `psiaccount.cpp`. Слой 6 в процессе: UI-кнопки OMEMO, исправление сообщения.)*
