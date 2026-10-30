# Сборка

| [↑](..) | [⟵ Описание](./description.md) | [Демонстрация ⟶](./demo.md) |
|---------|--------------------------------|-----------------------------|

## Стек технологий

* Компилятор: `GCC / Clang (C++20 / C++26)`
* Система сборки: `CMake`
* Сетевой стек: `Сокеты POSIX`, `std::jthread`

## Сборка проекта

**В корне проекта:**

```bash
cmake -B build
cmake --build build
```

Исполняемый файл компилируется по пути `./build/october/web-server/web-server`.

## Запуск

```bash
> ./build/october/webserver/webserver --help
Usage: ../../build/october/webserver/webserver [options]

Options:
  -p, --port <port>   TCP port to listen on (1..65535), default: 8080
  -d, --dir <path>    document root directory, default: ./public
  -h, --host <ip>     interface address, default: 0.0.0.0
      --help          show this help message

Examples:
  ../../build/october/webserver/webserver -p 8080 -d ./public
  sudo ../../build/october/webserver/webserver -p 80 -d /var/www/html
```

## Примеры использования

### 1. Стандартный запуск (порт 8080)
```bash
./build/october/webserver/webserver -p 8080 -d ./path/to/public
```

Вывод:
```text
[INFO] Server configured: host=0.0.0.0, port=8080, public_dir="./public"
[INFO] Listening socket created and bound to port 8080.
[INFO] HTTP Web Server is running. Press Ctrl+C to stop.
```

### 2. Запуск на стандартном HTTP-порту 80
Сервер поддерживает запуск на привилегированных портах без искусственных ограничений (требуются права суперпользоваля):
```bash
sudo ./build/october/webserver/webserver -p 80 -d ./public
```

По умолчанию запускается на порту 8080
```bash
./build/october/webserver/webserver -d ./public
```

### 3. Проверка

**Успешный запрос (200 OK):**
```bash
curl -i http://localhost:8080/
```
```text
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: 426
Connection: close

<!DOCTYPE html>
<html>
...
```

**Запрос несуществующего файла (404 Not Found):**
```bash
curl -i http://localhost:8080/unknown.html
```
```text
HTTP/1.1 404 Not Found
Content-Type: text/plain
Content-Length: 14
Connection: close

File Not Found
```

| [⟵ Описание](./description.md) | [Демонстрация ⟶](./demo.md) |
|--------------------------------|-----------------------------|
