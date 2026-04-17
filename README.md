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

*Последнее обновление: 2026-04-17 (Claude: **Layer 7 полностью закрыт** — полный аудит активной кодовой базы с множественными parallel-субагентами, 13 реальных багов из warning-анализа устранены, 2 CVE-уровня уязвимости (STARTTLS-инъекция + XEP-0280 Carbons impersonation) исправлены, QCA фреймворк перенесён из `/tmp` в стабильную локацию в репозитории, dead-code (Sparkle, Growl test, Carbon includes, dead qmake-ветки) удалён, build warnings **1981 → 371 (−81%)**. Все коммиты запушены. Приложение остаётся полностью функциональным; 0 ошибок компиляции.

**Предыдущая большая пачка того же дня — закрыт весь оставшийся Layer 6 UI + вывезли HTTP Upload + OMEMO MUC self-echo.**

**OMEMO multi-device:** свои сообщения из yachat видны на Android-телефоне (Conversations), обратно тоже. Последний пробел закрыли через пять связанных правок: (а) `iris-legacy/iris/xmpp-im/xmpp_task.cpp iqVerify()` молча отбрасывал IQ-ответы на self-PEP запросы с пустым `from` — по RFC 6120 §10.3.3 сервер вправе опустить `from` когда отвечает от имени собственного JID пользователя, так что приняли пустой `from` если `to` совпадает с нашим локальным JID; (б) добавили 10-секундный debounce в `publishBundle()` плюс убрали auto-republish branch из глобального PEP слушателя — без этого мы зацикливались на 131 417 строк лога за 40 секунд когда другой клиент переоверрайтил наш devicelist; (в) `fetchContactBundles()` пропускает наш собственный device id, libsignal не умеет шифровать сам себе; (г) для MUC (где libsignal отказывается в self-session принципиально) сохраняем {iv → plaintext} в памяти при отправке, при приходе self-echo отдаём cached plaintext вместо fallback текста `[This message is OMEMO encrypted]`; (д) этот же кеш персистится в `muc_echo.json` чтобы после рестарта history replay тоже разрезолвил свои шифрованные сообщения. Результат: own messages в комнатах показываются как обычный текст, на телефоне тоже все сообщения расшифровываются, `encrypt: SUCCESS — produced 6 <key> entries` на каждое отправленное сообщение.

**HTTP Upload (XEP-0363):** загрузка файлов заработала. Баг был в том что `<request xmlns='urn:xmpp:http:upload:0'/>` элемент конструировался через `doc()->createElementNS(UPLOAD_NS, "request")`, а Qt `QDomElement::save()` молча выбрасывает такой namespace-declared элемент при сериализации если сам namespace не объявлен в корне документа — Prosody видел IQ без дочернего элемента и отвечал `<undefined-condition/>`, UI показывал «Could not get upload slot from server». Переключили на паттерн Iris (`createElement + setAttribute("xmlns", NS)`) как в `xmpp_discoinfotask.cpp`. Парсинг ответа тоже переключили с `elementsByTagNameNS` на ручной match по tagName + xmlns-атрибуту/`namespaceURI` чтобы понимать оба стиля namespace-declarations которые используют разные серверы. Сейчас слот возвращается, PUT проходит, URL прилетает в чат как кликабельная ссылка.

**Yandex custom frame restored + рабочий tab bar на Qt5/macOS:** `options/macosx.xml` держал `custom-frame=false` ещё с Qt4/10.5 эпохи, вырубая весь нарисованный Yandex chrome (жёлтую цветочную шапку, рисованные traffic lights, бесфреймовый roster). Перевернули в `true`. Сразу вылезли три латентных regression в tab bar: (1) переключение вкладок мёртвое потому что `YaMultiLineTabBar::mousePressEvent` цеплялся через `QTabBar::mousePressEvent`, а его native tab-hit-test использует `tabAt()` на Qt-internal layout который на Qt5/macOS не совпадает с нашим `tabRect_[]` вообще — исправили полным обходом Qt-press/release, dispatch делаем сами по `tabRect_[]` через `setCurrentIndex`; (2) текст вкладок рендерился с y=-6 (вне верхнего края таба!) потому что `QTabBar::tabRect` не virtual и `YaTabBarBase::tabTextRect` вызывая `this->tabRect(index)` отдавал dispatch в Qt default (width=barWidth, height=0) вместо override в `YaMultiLineTabBar` — добавили virtual helper `effectiveTabRect()` в базу, переписали математику textRect с нуля через явные left/right/top/height (без `setLeft/setRight/moveCenter` которые могут swap edges на negative-width intermediate states); (3) сам tab bar был 22 px высоты что слишком мало для Arial 12 в vertical metrics Qt5/macOS — подняли `margin()` 3→7, bar теперь 30 px с аккуратным 7 px padding, `SE_TabWidgetTabBar` в стиле теперь anchor'ит bar к низу виджета, `SE_TabWidgetTabContents` заканчивается строго на верху bar'а.

**Theme picker теперь кликабельный:** комбобокс выбора скина в Preferences был уже полностью заведён (девять тем — `academic`, `baroque`, `glamour`, `hawaii`, `ice`, `sea`, `sky`, `spring`, `violet` — подгружались из `YaWindowTheme::funnyThemes()`, сохранение тоже работало), но имел в `.ui` `maximumSize height=16` что на Qt5/macOS делало dropdown слишком коротким чтобы прочитать элементы. Заменил на `minimumSize 140×22`. Теперь через `Preferences → Background:` переключаются темы как в референсе; `spring` подтверждён живым тестом.

Коммиты: 8d875af (iqVerify + debounce), 3eecde1 (MUC iv cache), c44fff9 (HTTP upload namespace + MUC cache persistence), 3e16f46 (custom frame + tab bar), 07faf6f (theme picker combobox).)*
