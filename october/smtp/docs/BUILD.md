# Сборка

| [⟵ Описание](./description.md) | [Демонстрация ⟶](./demo.md) |
|--------------------------------|-----------------------------|

## Стек технологий

* Компилятор: `GCC С++ 26`
* Система сборки: `CMake`
* Python 3.14 + Библиотека AIOSMTPD

## Сборка проекта

**В корне проекта**

```bash
cmake -B build
cmake --build build
```

Данное задание скомпилируется в исполняемый файл `./build/october/smtp/smtp-client`.

## Запуск

```bash
>  ./build/october/smtp/smtp-client -h
Usage: ./build/october/smtp/smtp-client <host|provider> -t <recipient> [options]

Required Parameters:
  <host|provider>            Target server or provider alias (e.g. yandex, mailru)
  -t, --to <email>           Recipient email address
  -s, --subject <text>       Email subject (required unless specified in message file)
  -b, --body <text>          Email body text
      OR
  -m, --message-file <path>  Load entire email from file

Optional Parameters:
  [port]                     Port number (default: 587)
  -f, --from <email>         Sender email address
  -k, --key-file <path>      Path to credentials file
  -h, --help                 Show this help message
```

### Локально

Для локального запуска, необходимо:

#### 1. Запуск сервера

Если не установлена библиотека aiosmtpd выполняем

```bash
> python -m venv .venv
> echo '.venv' >> .gitignore
> source ./.venv/bin/activate
> pip install aiosmtpd
```

И запускаем сервер

```bash
./.venv/bin/aiosmtpd -n -l 127.0.0.1:1025
```

#### 2. Отправка письма

После этого необходимо открыть другой терминал
и в параметрах команды
просто указать ip и порт сервера и составляющие письма.

```bash
./build/october/smtp/smtp-client 127.0.0.1 1025 -f source@test.local -t dest@test.local -s "Test" -b "Hello Dest"
```

В результате получаем следующий вывод:

```bash
--- SMTP Client Launching ---
Target:  127.0.0.1:1025                                                                                                                                                                                                             
From:    source@test.local                                                                                                                                                                                                          
To:      dest@test.local                                                                                                                                                                                                            
Subject: Test                                                                                                                                                                                                                       

[1] Connecting...
S: 220 PCName Python SMTP 1.4.6

[4] Sending email...
C: HELO localhost
S: 250 PCName
C: MAIL FROM:<source@test.local>
S: 250 OK
C: RCPT TO:<dest@test.local>
S: 250 OK
C: DATA
S: 354 End data with <CR><LF>.<CR><LF>
[CLIENT DATA SENDING] (142 bytes)...
S: 250 OK

[5] Disconnecting...
C: QUIT
S: 221 Bye

[SUCCESS] Email was sent successfully!
```

После чего можно перейти обратно в терминал с сервером, где мы увидим следующий вывод:

```bash
---------- MESSAGE FOLLOWS ----------
From: <source@test.local>
To: <dest@test.local>
Subject: Test
MIME-Version: 1.0
Content-Type: text/plain; charset=UTF-8
X-Peer: ('127.0.0.1', 54492)

Hello Dest
------------ END MESSAGE ------------
```

### Глобально

Чтобы отправить письмо, например,
на адрес yelka05@inbox.ru (mail.ru)
с адреса yel@akaram.ru (yandex.ru),
необходимо:

1. Получить ключ яндекса на сайте id.yandex.ru (для mail.ru в настройках ящика в разделе «Пароли приложений»). Например,
   `AAaa1122BBbb3344`
2. Положить его в файл (.yandex.key) в формате:
   ```
   yel@akaram.ru
   AAaa1122BBbb3344
   ```
   или
   ```
   yel@akaram.ru:AAaa1122BBbb3344
   ```
3. После чего выполняем команду, аналогичную локальному запуску.

   **Важно: необходимо либо находиться в одной папке с файлом `.yandex.key`, либо явно указать путь к нему в параметре
   `-k, --key-file <path>`.**

Выполняем:

```bash
./build/october/smtp/smtp-client yandex -t yelka05@inbox.ru -s "Lab 7" -b "Hello! What's up?" -k october/smtp/.yandex.key
```

<details>
<summary>
Получаем
</summary>

```bash
[INFO] Loaded credentials from file: october/smtp/.yandex.key
--- SMTP Client Launching ---
Target:  smtp.yandex.ru:587
From:    yel@akaram.ru
To:      yelka05@inbox.ru
Subject: Lab 7

[1] Connecting...
S: 220 mail-nwsmtp-smtp-production-main-64 Ok 1793300501-f1pS5mFpjKo0

[2] Initializing STARTTLS...
C: EHLO localhost
S: 250-mail-nwsmtp-smtp-production-main-64
S: 250-8BITMIME
S: 250-PIPELINING
S: 250-SIZE 53477376
S: 250-STARTTLS
S: 250-AUTH LOGIN PLAIN XOAUTH2
S: 250-DSN
S: 250 ENHANCEDSTATUSCODES
C: STARTTLS
S: 220 Go ahead
[TLS Handshake starting...]
[TLS Handshake complete: TLS_AES_256_GCM_SHA384]
C: EHLO localhost
S: 250-mail-nwsmtp-smtp-production-main-64
S: 250-8BITMIME
S: 250-PIPELINING
S: 250-SIZE 53477376
S: 250-STARTTLS
S: 250-AUTH LOGIN PLAIN XOAUTH2
S: 250-DSN
S: 250 ENHANCEDSTATUSCODES

[3] Authenticating...
C: AUTH LOGIN
S: 334 VXNlcm5hbWU6
C: eWVsQGFrYXJhbS5ydQ==
S: 334 UGFzc3dvcmQ6
C: eXZuc3FwbnRmbWRzbG11dQ==
S: 235 2.7.0 Authentication successful. 1793300502-f1pS5mFpjKo0
[Authentication successful]

[4] Sending email...
C: MAIL FROM:<yel@akaram.ru>
S: 250 2.1.0 <yel@akaram.ru> ok 1793300503-f1pS5mFpjKo0-GTscCGNa
C: RCPT TO:<yelka05@inbox.ru>
S: 250 2.1.5 <yelka05@inbox.ru> recipient ok 1793300503-f1pS5mFpjKo0-GTscCGNa
C: DATA
S: 354 Start mail input, end with <CRLF>.<CRLF>
[CLIENT DATA SENDING] (147 bytes)...
S: 250 2.0.0 Ok: queued on mail-nwsmtp-smtp-production-main-64 1793300503-f1pS5mFpjKo0-GTscCGNa

[5] Disconnecting...
C: QUIT
S: 221 2.0.0 Closing connecton

[SUCCESS] Email was sent successfully!
```

</details>

| [⟵ Описание](./description.md) | [Демонстрация ⟶](./demo.md) |
|--------------------------------|-----------------------------|
