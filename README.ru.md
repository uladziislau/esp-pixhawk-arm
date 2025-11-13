# ESP Pixhawk Arm (ESP32/ESP8266)

Минималистичный веб‑интерфейс и HTTP‑API для ARM/DISARM автопилота Pixhawk по MAVLink на базе ESP32 или ESP8266.

- Единый скетч для обеих платформ (условная компиляция)
- Асинхронный веб‑стек с Server‑Sent Events для мгновенных обновлений статуса
- Настройка Wi‑Fi через WiFiManager, mDNS и OTA‑обновления
- Управление режимами полета (RTL, Stabilize, Alt Hold)
- Обратная связь по подтверждению MAVLink команд

## Возможности
- Веб‑страница (из флеша) с кнопками ARM/DISARM и управления режимами полета
- API:
  - `GET /status` → `{ "armed": bool, "mode": number }`
  - `POST /arm` → `{ "ok": true }`
  - `POST /disarm` → `{ "ok": true }`
  - `POST /mode/rtl` → `{ "ok": true }` (Возврат к точке запуска)
  - `POST /mode/stabilize` → `{ "ok": true }` (Стабилизированный режим)
  - `POST /mode/alt_hold` → `{ "ok": true }` (Удержание высоты)
  - SSE `GET /events` → события `status` и `ack` с обратной связью
- Чтение состояния ARM из MAVLink `HEARTBEAT` (без оптимистичных переключений)
- Отображение подтверждений MAVLink команд в UI
- Поддержка ESP32 (HardwareSerial2) и ESP8266 (SoftwareSerial)
- Опциональный AP-only режим с фиксированным IP (172.217.28.1)

## Аппаратная часть
- Подключение телеметрии Pixhawk к UART ESP
- Пины по умолчанию:
  - ESP32: `RX=GPIO16`, `TX=GPIO17` (UART2)
  - ESP8266: `RX=GPIO5`, `TX=GPIO4` (SoftwareSerial)
- Переопределение на этапе компиляции:
  - `-DMAV_RX_PIN=<gpio> -DMAV_TX_PIN=<gpio>`

## Зависимости
Установите через Arduino Library Manager или PlatformIO:
- `ESPAsyncWebServer`
- `AsyncTCP` (ESP32) или `ESPAsyncTCP` (ESP8266)
- `WiFiManager` (tzapu)
- `ArduinoOTA`
- `mDNS` (`ESPmDNS` на ESP32, `ESP8266mDNS` на ESP8266)
- `mavlink` (Arduino‑заголовки MAVLink)

## Сборка
- Откройте `esp-pixhawk-arm.ino` в Arduino IDE
- Выберите плату (ESP32 или ESP8266)
- Убедитесь, что зависимости установлены
- Прошейте; при первом запуске появится портал WiFiManager (SSID вида `esp32-pixhawk` или `esp8266-pixhawk`)

### AP-only режим (опционально)
Раскомментируйте `#define AP_ONLY_MODE` в скетче для включения режима только точка доступа:
- Фиксированный IP: `172.217.28.1`
- SSID точки доступа: `pixhawk32-ap` или `pixhawk8266-ap`
- Пароль: `12345678`

## Использование
- Откройте устройство в браузере:
  - ESP32: `http://pixhawk32.local/` (или `http://172.217.28.1/` в AP-only режиме)
  - ESP8266: `http://pixhawk8266.local/` (или `http://172.217.28.1/` в AP-only режиме)
- Используйте кнопки UI для ARM/DISARM или изменения режимов полета
- Подтверждения команд отображаются временными сообщениями в UI
- Все изменения режимов и команды ARM/DISARM подтверждаются через MAVLink

## Безопасность
- По умолчанию эндпоинты открыты. Для продакшена добавьте базовую авторизацию или PIN для `POST /arm` и `POST /disarm`.

## Заметки
- На ESP8266 используется SoftwareSerial; для более надёжной телеметрии предпочтительнее ESP32 с HardwareSerial.
- UI вшит в прошивку и отдаётся из памяти; файловая система не требуется.

## Лицензия
Проект распространяется по лицензии MIT (см. `LICENSE`).
