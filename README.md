# yaonline-claude-revival-experiment

> Экспериментальная модернизация Ya.Online 3.0.3 — старого XMPP-клиента Яндекса (форк Psi, 2009 год) — с помощью Claude Code и ChatGPT Codex.

> **Статус: эксперимент завершён (18 апреля 2026).** Клиент **работоспособен, но далеко не идеален**: подключается к современным XMPP-серверам, SCRAM-SHA-1/256, рабочий OMEMO 1-на-1 и в MUC (в обе стороны, на нескольких устройствах), history replay, загрузка файлов (XEP-0363), все 15 XEP из Слоя 5. Базовые фичи современного XMPP-клиента работают корректно. Однако это 15-летний Qt4-проект с до-модерн-Qt паттернами во внутренностях; не ожидайте качества кода зеленого поля.

---

## О проекте

**Ya.Online** (внутреннее название **YaChat**) — это Jabber/XMPP-клиент с брендингом Яндекса, основанный на открытом клиенте [Psi](http://psi-im.org). Исходный код был опубликован 16 декабря 2009 года.

- **Руководитель проекта:** Дмитрий Матвеев
- **Ведущий разработчик:** Михаил Пищагин
- **Лицензия:** GNU General Public License v2
- **Оригинальная кодовая база:** ~900 файлов, ~189 000 строк C++/Qt

Этот репозиторий — эксперимент: можно ли оживить 15-летний Qt4-проект до рабочего состояния на современном macOS с помощью ИИ-агента (Claude+ChatGPT)?

**Ответ: да, но с оговорками.** За ~3 интенсивных дня работы (14–18 апреля 2026) клиент прошёл путь от «не компилируется ни одного файла на Qt 5.15» до «подключается, шифрует, расшифровывает, отображает файлы, запоминает пароль, работает в группах». Ценой компромиссов: ARM не протестирован, ~371 некритичное предупреждение компилятора осталось, часть внутренностей Iris всё ещё Qt4-эпохи.

---

## Что работает

| Фича | Статус | Примечания |
|---|---|---|
| Подключение к Prosody 0.12+ | ✅ | SCRAM-SHA-1/256, TLS 1.2+, запоминание пароля |
| Отправка и получение сообщений | ✅ | 1-на-1, групповые (MUC) |
| OMEMO 1-на-1 | ✅ | Шифрование ко всем своим устройствам + устройствам получателя |
| OMEMO в MUC | ✅ | Два направления, включая peer-plaintext cache для history replay |
| Загрузка файлов (XEP-0363) | ✅ | HTTP Upload slot через сервер, PUT, кликабельные ссылки в чате |
| История чатов после рестарта | ✅ | Своя отправка пишется plaintext, не `[This message is OMEMO encrypted]` |
| Yandex custom UI (frame, roster, tab bar) | ✅ | Весь `src/tools/yastuff/` (322 файла) сохранён |
| Переключение тем | ✅ | 9 скинов в `Preferences → Background`, `spring` подтверждён живым тестом |
| Автозапуск и автоподключение | ✅ | Пароль сохраняется в профиле |
| Кнопки OMEMO-замок + исправление сообщения | ✅ | Добавлены в заголовок чата и поле ввода |
| Все 15 XEP из Слоя 5 | ✅ | Carbons, MAM, Receipts, Markers, Chat states, Ping, CSI, Bookmarks, Disco, Caps, Correction, MUC, HTTP Upload, OMEMO, Direct invites |

## Что НЕ работает (известные ограничения)

- **Apple Silicon нативно** не тестировался — пути в `conf.pri` жёстко зашиты на `/usr/local/opt/qt@5`; на M1/M2/M3 нужен Rosetta Homebrew
- **Устройства пиров с опубликованным device id, но без опубликованного bundle** (поломанные клиенты) — мы не можем к ним шифровать; 5-минутный backoff на повторную попытку
- **Сообщения из прошлого, не успевшие попасть в кеш ДО апгрейда на текущую версию** — остаются `[This message is OMEMO encrypted]` навсегда (libsignal отказывается повторно расшифровывать уже-использованный ratchet-counter). Новая переписка работает чисто.
- **40+ `QXml*` вызовов** в `iris-legacy/iris/xmpp-core/parser.cpp` остались (deprecated с Qt 5.9); переход на `QXmlStreamReader` — отдельный многодневный рефакторинг
- **130 inconsistent-missing-override warnings** в каскаде `YaGroupchatContactListView` — добавление `override` к одной функции триггерит вирусное требование override ко всем
- **iris/ (non-legacy) и third-party/qca/** оставлены на диске как reference copies, но не компилируются — 9 МБ мёртвого кода в репозитории

---

## Цель модернизации (итоги)

| Цель | Статус | Как достигнуто |
|---|---|---|
| Сборка на macOS Ventura/Sonoma/Sequoia | ✅ | 0 ошибок, 371 предупреждение (было 1981) |
| Подключение к XMPP-серверу | ✅ | SCRAM-SHA-1/256 на Prosody 0.12+, QCA 2.3.7 |
| Отправка/получение сообщений | ✅ | Проверено вживую с Conversations/Cheogram/dino/yachat-самого-себя |
| Современная безопасность | ✅ | TLS 1.2+ minimum, STARTTLS injection fix (CVE-class), Carbons impersonation fix (CVE-class) |
| Современные XMPP-расширения | ✅ | Все 15 XEP из плана |

---

## Сборка (macOS)

> **Важно.** Всё собирается и тестируется только на macOS (Intel x86_64 / Apple Silicon через Rosetta). Linux/Windows сейчас вне проекта.
> Это актуальные инструкции на текущем коммите; ничего стороннего руками собирать больше не надо — `QCA 2.3.7` лежит в репозитории готовым фреймворком.

### 1. Зависимости

| Пакет | Версия | Откуда ставить | Где должно лежать |
|---|---|---|---|
| macOS SDK | 11.0+ (Big Sur или новее) | вместе с Xcode | — |
| Xcode Command Line Tools | любая свежая | `xcode-select --install` | `/Applications/Xcode.app` или CLT |
| **Qt 5.15 LTS** (не Qt 6!) | 5.15.x | **Homebrew:** `brew install qt@5` | `/usr/local/opt/qt@5` (Intel) или `/opt/homebrew/opt/qt@5` (ARM) |
| **OpenSSL 3** | 3.x | **MacPorts:** `sudo port install openssl3` | `/opt/local/lib`, `/opt/local/include/openssl` |
| **zlib** | 1.3+ | MacPorts: `sudo port install zlib` | `/opt/local/lib/libz.*` |
| **libidn2** | 2.3+ | MacPorts: `sudo port install libidn2` | `/opt/local/lib` |
| **libsignal-protocol-c** (для OMEMO) | 2.3.3 | **Homebrew:** `brew install libsignal-protocol-c` | `/usr/local/opt/libsignal-protocol-c` |

Проект одновременно использует **Homebrew** (`/usr/local`) и **MacPorts** (`/opt/local`). Это сознательное решение: Qt@5 удобнее держать через Homebrew (он поддерживает 5.15 LTS), а модульные криптобиблиотеки — через MacPorts. Оба должны быть установлены.

> **Apple Silicon (M1/M2/M3):** Homebrew на ARM ставит Qt в `/opt/homebrew/opt/qt@5`, а не в `/usr/local/opt/qt@5`. Пути в `src/src.pro`, `conf.pri` и командах ниже жёстко зашиты на `/usr/local/opt/...` — для ARM откройте терминал через Rosetta (`arch -x86_64 zsh`) и поставьте x86_64-версию Homebrew параллельно. Нативная ARM-сборка не тестировалась.

### 2. QCA 2.3.7 — уже в репозитории

В папке `third-party/qca-qt5-install/qca-qt5.framework/` лежит **готовый собранный** `qca-qt5.framework` (2.5 МБ, x86_64 Mach-O), включая все плагины (`qca-ossl`, `qca-cyrus-sasl`, `qca-gnupg`, `qca-logger`, `qca-softstore`). Это обход исторической проблемы — раньше фреймворк лежал в `/tmp/qca-install` и пропадал после каждой перезагрузки macOS. Теперь он хранится в git и переносим между машинами.

**Самому QCA собирать не нужно.** Если по какой-то причине захочется пересобрать (например, для ARM), есть первоисточник <https://userbase.kde.org/QCA> + плагин qca-ossl, положить результат в тот же путь.

### 3. Сборка

```bash
# 1. Клонировать репозиторий (с уже закоммиченным QCA)
git clone https://github.com/kira-we1ss/yaonline-claude-revival-experiment.git
cd yaonline-claude-revival-experiment

# 2. Проверить что qmake указывает на Qt 5.15, а не Qt 6
/usr/local/opt/qt@5/bin/qmake --version
# Должно вывести:
#   QMake version 3.1
#   Using Qt version 5.15.18 in /usr/local/Cellar/qt@5/5.15.18/lib

# 3. Сгенерировать Makefile из psi.pro
#    (запуск ./configure больше не нужен — conf.pri закоммичен с готовыми путями)
/usr/local/opt/qt@5/bin/qmake psi.pro

# 4. Собрать (параллельно по числу ядер)
make -j$(sysctl -n hw.ncpu)

# 5. Получить .app-bundle со всеми фреймворками
ls -lh src/yachat.app/Contents/MacOS/yachat
# -rwxr-xr-x  1 user  staff  7.7M  ...  src/yachat.app/Contents/MacOS/yachat

# 6. QCA уже скопирован внутрь бандла автоматическим post-link шагом:
ls src/yachat.app/Contents/Frameworks/qca-qt5.framework/
# Headers  Resources  Versions  qca-qt5

# 7. Запустить
open src/yachat.app
```

Успешная сборка — **~3–5 минут** на Intel i7, 8 МБ бинарник. **0 ошибок, ~371 предупреждение** (всё некритичное: deprecated-declarations в парсере XMPP и inconsistent-missing-override — отложено на будущее, не мешает работе).

### 4. Запуск и хранение данных

Запустить:
```bash
open src/yachat.app
# или
src/yachat.app/Contents/MacOS/yachat
```

Настройки, ростер, история чатов хранятся в `~/Library/Application Support/Yandex/Online/<profilename>/`. По умолчанию создаётся профиль `default`. OMEMO-ключи — в подпапке `omemo/`.

### 5. Если что-то не собирается

| Симптом | Причина | Что делать |
|---------|---------|-----------|
| `qmake: command not found` | Qt5 не в PATH, взят Qt6 или не установлен | Использовать полный путь `/usr/local/opt/qt@5/bin/qmake` |
| `Project ERROR: Unknown module(s) in QT: multimedia` | `qt@5` установлен без модулей | `brew reinstall qt@5` |
| `'openssl/evp.h' file not found` | MacPorts не установлен или без `openssl3` | `sudo port install openssl3` |
| `library 'qca-qt5' not found` | Бандлового QCA не оказалось (LFS/clone issue) | Проверить `ls third-party/qca-qt5-install/qca-qt5.framework/Versions/2/qca-qt5` — должен быть исполняемый Mach-O файл |
| `signal/signal_protocol.h' file not found` | libsignal-protocol-c не установлен | `brew install libsignal-protocol-c` |
| `ld: library not found for -lz` | Нет MacPorts zlib | `sudo port install zlib` |
| При запуске: `dyld: Library not loaded: @executable_path/.../qca-qt5` | Сломан post-link копии QCA в бандл | `make clean && make -j$(sysctl -n hw.ncpu)`, либо вручную `cp -R third-party/qca-qt5-install/qca-qt5.framework src/yachat.app/Contents/Frameworks/` |
| Крашь при открытии вкладки MUC или настроек | Старый билд до Layer 7 | `make clean && make -j$(sysctl -n hw.ncpu)` — фиксы есть в `main` с коммита `99e4c31` |

### 6. Пересборка и «чистая сборка»

```bash
# Чистая сборка с нуля (10–15 мин):
make clean
/usr/local/opt/qt@5/bin/qmake psi.pro
make -j$(sysctl -n hw.ncpu)

# Быстрая инкрементальная пересборка после правки файла:
make -j$(sysctl -n hw.ncpu)

# Пересобрать только один файл (например после правки psiaccount.cpp):
rm src/.obj/psiaccount.o && make -j4 -C src
```

### 7. Build in English (TL;DR)

For those who read English faster than Russian:

```bash
# Prereqs (Intel Mac; see detailed table above for ARM notes)
xcode-select --install
brew install qt@5 libsignal-protocol-c
sudo port install openssl3 zlib libidn2

# Build
git clone https://github.com/kira-we1ss/yaonline-claude-revival-experiment.git
cd yaonline-claude-revival-experiment
/usr/local/opt/qt@5/bin/qmake psi.pro
make -j$(sysctl -n hw.ncpu)

# Run
open src/yachat.app
```

The bundled `qca-qt5.framework` (QCA 2.3.7 + OMEMO/TLS plugins) is committed under `third-party/qca-qt5-install/` and auto-copied into the `.app` bundle during linking — no manual QCA install step is needed. `./configure` is **not** required; `conf.pri` is committed with the right paths. Expect 0 errors and ~371 deferred non-critical warnings. Build time: ~3–5 min on modern Intel i7.

 > ✅ **Все 7 слоёв завершены.** 15/15 XEP работают, UI полностью исправлен, критические баги (STARTTLS-инъекция, Carbons-подмена отправителя, уязвимости null-deref) устранены, мёртвый код удалён. Компиляция на Qt 5.15.18 / macOS 14 SDK / clang — **0 ошибок, 371 предупреждение** (было 1981, снижение на **81%**). Приложение подключается к Prosody 0.12+, аутентифицируется по SCRAM-SHA-1, загружает ростер и MUC, отправляет/принимает OMEMO-сообщения в 1×1 и многопользовательских чатах.

---

## Архитектура модернизации

Миграция выполняется послойно. Каждый слой должен дать рабочую сборку перед переходом к следующему.

```
Слой 7: Полный аудит кода, устранение ошибок, рефакторинг             ✅
Слой 6: UI-рендеринг + новые кнопки (OMEMO toggle, correction)        ✅
Слой 5: Новые XMPP XEP (15/15 — все завершены)                       ✅
Слой 4: QCA 2.3.x + SCRAM-SHA-1/256 + TLS 1.2+                       ✅
Слой 3: Удаление Qt3Support (559+ использований Q3* → Qt5)            ✅
Слой 2: Исправление системы сборки (qmake, Qt 5.15)                   ✅
Слой 1: Исправления macOS (Growl, Carbon API, Sparkle)                ✅
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

### Слой 6 — Исправления UI-рендеринга + новые кнопки ✅ *завершён 2026-04-17*

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
| I: Таббар — текст/позиция/клики | ✅ | Три связанных regression на Qt5/macOS — (1) обход `QTabBar::mousePressEvent` (его `tabAt()` не совпадал с `tabRect_[]`), (2) virtual `effectiveTabRect()` helper в `YaTabBarBase` чтобы `tabTextRect` видел переопределение `YaMultiLineTabBar::tabRect`, (3) `margin` 3→7 для 30 px bar + `SE_TabWidgetTabBar` anchor к низу виджета. Commit 3e16f46 |
| J: Собственный Yandex custom frame на macOS | ✅ | `options/macosx.xml custom-frame=false→true` — переворачиваем Qt4-era workaround чтобы нарисованная Yandex шапка + frameless roster включились на macOS |
| K: HTTP Upload (XEP-0363) слот не возвращался | ✅ | `<request>` дочерний элемент конструировался через `createElementNS(NS, ...)` и Qt `QDomElement::save()` выкидывал его при сериализации (namespace не был объявлен в корне); переключили на `createElement + setAttribute("xmlns", NS)` по паттерну Iris. Commit c44fff9 |
| L: Свои OMEMO сообщения в MUC как `[This message is OMEMO encrypted]` | ✅ | libsignal отказывается от self-session принципиально; добавили iv-keyed `mucEchoPlaintext` cache (кэш на отправке, lookup при получении self-echo), плюс персистенция в `muc_echo.json` чтобы после рестарта history replay тоже разрезолвился. Commits 3eecde1, c44fff9 |
| **Кнопка OMEMO-замок в заголовке чата** | ✅ | `QToolButton` (24×24) в правой колонке `chatTopFrame`; 🔒 зелёный=вкл, 🔓 серый=выкл; клик вызывает `OmemoManager::setEnabled()`; `PsiAccount::omemoManager()` добавлен как публичный accessor |
| **Кнопка исправления сообщения в поле ввода** | ✅ | `QToolButton` ✏ (24×20) над кнопкой «Отправить» в `YaChatEdit`; сигнал `correctionRequested()`; `YaChatDlg::onCorrectionRequested()` подставляет `~` + `lastSentBody_`; поле `ChatDlg::lastSentBody_` добавлено в `doneSend()` |
| M: Theme picker UI в Preferences | ✅ | Комбобокс уже был заведён в `yapreferences.ui` со всеми девятью скинами (`academic` / `baroque` / `glamour` / `hawaii` / `ice` / `sea` / `sky` / `spring` / `violet`), но имел `maximumSize height=16` что на Qt5/macOS делало dropdown слишком коротким и нечитаемым. Заменено на `minimumSize 140×22` — теперь темы переключаются через `Preferences → Background:`. `spring` (жёлтые цветочки) как в референсе. Commit 07faf6f |

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

### Слой 7 — Полный аудит кода ✅ *завершён 2026-04-17*

Одноразовый проход по всей активной кодовой базе. Все коммиты запушены.

| Фаза | Статус | Результат |
|------|--------|-----------|
| 1. Регрессионный аудит слоёв 1–6 | ✅ | 2 параллельных `explore`-субагента прошлись по каждому слою; Layer 3 подтверждён чистым в активной сборке |
| 2. Сканирование крашей | ✅ | 13 реальных багов найдено через warning-анализ + 3 класса крашей (proxy-model null-deref, qobject_cast в destroyed, STARTTLS-инъекция) |
| 3. Удаление мёртвого кода | ✅ | Sparkle (3 файла), Growl test (2), Carbon includes в psiapplication/common, dead qmake-ветки (`sub_iris`, `qca-static`) |
| 3b. Аудит предупреждений | ✅ | **1981 → 371** предупреждений (−81%), 0 ошибок |
| 4. Финальная верификация | ✅ | `otool -L` подтверждает линковку, 8 МБ Mach-O, QCA резолвится из bundle |

**Критические исправления (полный список см. CLAUDE.md → Layer 7 Summary):**

- **Крашь-класс:**
  - `ContactListProxyModel` + `YaRosterFilterProxyModel`: `setDynamicSortFilter(true)` в конструкторе до установки source model = null-deref (та же проблема, что починили в `MUCAffiliationsProxyModel` ранее)
  - `YaOfficeBackgroundHelper::destroyed()`: `dynamic_cast` → `static_cast` (vtable уже уничтожен к моменту сигнала destroyed)

- **Безопасность (CVE-уровень):**
  - STARTTLS data injection (RFC 6120 §5.4.3.3): после `<proceed/>` любой буфер данных перед TLS handshake скармливался в TLS-поток как application data, позволяя MITM-инъекцию. Теперь aborts с `ErrProtocol`
  - XEP-0280 §11 Carbons impersonation: `Message::fromStanza` доверял inner from/to/body без проверки что outer `<message>` от нашего bare JID. Теперь `carbonOuterFrom()` + валидация в `client_messageReceived`

- **Реальные баги через warnings:**
  - `s5b.cpp:1032` — неинициализированный `Entry *e` дереференсился как `e->query = 0` (UB)
  - `s5b.cpp:2105` — `if (port != 0 || port != 1)` тавтологически true → весь UDP receiver вываливался на каждом пакете
  - `s5b.cpp:845` — `!mode != Datagram` разбирался как `(!mode) != Datagram`, логика инвертирована
  - `mood.cpp:66` — `!type() == Unknown` та же precedence-ошибка, Unknown=0 и ветки перепутаны
  - `itunescontroller.cpp:48` — `new char[]` с `delete` без `[]` (UB)
  - `NDKeyboardLayout.m:170` — `sizeof(aBuff)` на decayed-pointer параметре, обнулялось неверное число байт
  - `JsonRpcAdaptorPrivate.cpp:211` — `arguments[9]` при массиве размера 9 → past-end write
  - `psilogger.cpp:99` — `return this && stream_` (UB, компилятор выкидывает null-check)

- **Build system:**
  - QCA 2.3.7 фреймворк перенесён из `/tmp/qca-install` (уничтожается при ребуте macOS) в `third-party/qca-qt5-install/` (закоммичен, 2.5 МБ)
  - `src/src.pro` `QMAKE_POST_LINK` теперь копирует QCA в `.app/Contents/Frameworks/` — сборка переносима между машинами
  - `psi.pro` и `src/src.pri` упрощены: удалены мёртвые ветки `sub_iris`, `qca-static`, `!iris_legacy`

**Намеренно отложено (безопасно для прод):**
- `iris/` (3.5 МБ, non-legacy ветка, не компилируется) — оставлена как reference copy
- `third-party/qca/` (5.6 МБ vendored QCA 2.0.x, не используется) — тот же принцип
- QXml* → QXmlStreamReader переписывание в `iris-legacy/iris/xmpp-core/parser.cpp` (40+ мест, отдельный многодневный проект)
- `plugins/chess`, `plugins/noughts` Qt3-чистка — исключены из сборки

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

## Post-Layer-7 OMEMO стабилизация (2026-04-17 → 2026-04-18)

После закрытия Слоя 7 выяснилось что OMEMO имеет несколько корректностных дыр при реальном использовании на нескольких устройствах и после рестарта клиента. Закрыли 14 последовательными коммитами (от `78deb73` до `b0bc4f7`):

- **Сохранение пароля на YAPSI-сборке:** `setUserAccount()` форсил `opt_pass/opt_auto/opt_reconn=true` на локальной `acc`, но `d->realAcc` снепшотился ДО форсинга — на YAPSI `userAccount()` возвращает `realAcc` для сохранения, так что `<password>` никогда не писался в config и автоподключение тихо умирало на следующем запуске. Пофиксили зеркалированием флагов на `d->realAcc` (коммит `b389480`).

- **1-на-1 OMEMO для своих других устройств:** `encrypt()` для получателя уже объединял `PEP-devicelist ∪ stored-sessions`, но для НАШИХ других устройств ходил только по `deviceLists[ourJid]` который может быть пустым/устаревшим (async PEP race, overwrite от пира который не знал про наш новый device). Отзеркалили union для своих (коммит `43c1034`).

- **Удалили сломанный `<private xmlns='urn:xmpp:carbons:2'/>` маркер** — он подавлял ВСЮ OMEMO отправку из carbon-потока, включая `<encrypted>` блок который нужен нашим другим устройствам для расшифровки. Также: на ОТПРАВЛЯЮЩЕМ клиенте дропаем sent-carbon своих же сообщений — `doneSend()` уже показал plaintext, а OMEMO-decrypt для self-JID всё равно бы провалился (коммит `e6fb0eb`).

- **Локальная история OMEMO-сообщений:** новое transient-поле `Message::localPlaintextBody()` (не сериализуется в stanza). Пайплайн отправки запоминает оригинальный plaintext, `PsiAccount::dj_sendMessage` пишет его в yahistory вместо fallback-уведомления. Раньше каждое отправленное OMEMO-сообщение сохранялось в локальную историю как `"I sent you an OMEMO encrypted message..."` и после рестарта было видно этот мусор (коммит `a5f997a`).

- **MUC OMEMO для multi-device пиров:** `ensureMucSessions()` раньше считал «если есть хоть одна session для пира — всё ок», но пиры с Conversations на телефоне + ПК имеют две разные device id и мы адресовали только одну. Пофикшено требованием сессии для КАЖДОГО анонсированного device (коммит `1680155`).

- **Peer-plaintext cache против `SG_ERR_DUPLICATE_MESSAGE`:** libsignal отказывается повторно расшифровывать уже-использованный ratchet counter. В MUC history replay на re-join это систематически выбивает, и каждое replay'енное сообщение показывается как fallback. Новый кеш `mucPeerPlaintext[bareJid, iv]` сохраняет plaintext при первом успешном decrypt; при повторных попытках отдаём его сразу без обращения к libsignal. Персистится в `muc_peer_cache.json` (коммит `de6ad65`).

- **`nickToRealJid_` теперь персистентный через leaves:** офлайн/ушедшие пиры всё ещё адресуются в исходящих MUC OMEMO сообщениях — ciphertext доходит до них через MAM/offline когда они переподключатся. Раньше мы их роняли при `presence unavailable` и для отправленного во время их отсутствия они видели fallback (коммит `cdb069f`).

- **Broken peer bundle (NecroBread's dino) throttling:** пир анонсирует device id, но bundle-нода 404-ит на PEP — мёртвый клиент или не завершённая OMEMO-инициализация. Без throttle мы долбились в 404 на каждую отправку. Добавили 5-минутный per-(jid:devId) backoff, успешный build очищает запись (коммит `78deb73`).

- **UI:** путь неудачного decrypt теперь ставит `wasEncrypted=true` на fallback-body сообщении, чтобы `ChatDlg::appendMessage` не переключал баннер «Encryption Disabled» на каждом replay'енном сообщении (коммит `4bf4601`).

- **Lock cleanup:** удалили ~50 строк/отправка диагностического лог-спама. Оставлены: startup info, публикация bundle, PEP devicelist update уведомления, строки реальных fetch'ей, per-send SUCCESS summary, per-new-session built, настоящие qWarning. Убраны: per-device encrypt/decrypt детали, per-message-arrival теги, per-presence теги, строки касающиеся plaintext (коммит `b0bc4f7`).

### OMEMO on-disk state (на аккаунт, `<profile>/omemo/<bareJid>/`)
- `store.json` — libsignal identity + sessions (ключ `jid:devId`) + PEP devicelist cache
- `muc_echo.json` — `iv → plaintext` для СВОИХ MUC-отправок (libsignal не умеет self-decrypt, резолвим через echo)
- `muc_peer_cache.json` — `(bareJid, iv) → plaintext` для peer-MUC сообщений чтобы пережить DUPLICATE_MESSAGE replay'ы

### Известные оставшиеся ограничения (приняты как есть)
- **Пир с опубликованным device id но без bundle** — мы просто пропускаем этот device. Исправление только на стороне пира.
- **`SG_ERR_DUPLICATE_MESSAGE` для replay сообщения которое мы НИКОГДА не расшифровали** (история с пиром до существования кеша, или после reset store): навсегда fallback. Новый live-трафик работает.
- **Nuke `store.json`** даёт свежий device id — пиры должны обновить наш devicelist чтобы адресоваться к новому; в промежутке их старые сообщения на нашей стороне показываются как fallback.

---

*Последнее обновление: 2026-04-18. **Эксперимент завершён.** Клиент работоспособен, но далеко не идеален. Базовые фичи современного XMPP-клиента (1-на-1 OMEMO, MUC OMEMO, HTTP upload, SCRAM auth, автоподключение, все 15 XEP) подтверждены рабочими через живые тесты с Conversations/Cheogram/dino. Все коммиты запушены в `main` на GitHub. 0 ошибок компиляции.*

*Ключевые коммиты: `8d875af` (iqVerify + publishBundle debounce), `3eecde1` (MUC iv cache), `c44fff9` (HTTP upload namespace + MUC cache persistence), `3e16f46` (Yandex custom frame + tab bar fix), `07faf6f` (theme picker combobox), `f1b0316` (STARTTLS + Carbons CVE-класс), `56ab61c` (bulk Qt5 deprecation −81%), `55872c3` → `b0bc4f7` (post-Layer-7 OMEMO стабилизация, 14 коммитов).*
