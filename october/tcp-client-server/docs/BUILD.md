# Сборка

| [⟵ Описание](./description.md) | [Демонстрация ⟶](./demo.md) |
|--------------------------------|-----------------------------|

## Стек технологий

* Компилятор: `GCC / C 23`, `Go 1.27`
* Система сборки: `CMake`
* Сетевой стек: `Сокеты POSIX`, `epoll`, `pthreads`

## Сборка проекта

**В корне проекта**

```bash
cmake -B build
cmake --build build
```

Данные цели скомпилируются в исполняемые файлы:
* `./build/octiber/tcp-client-server/client` — Клиент
* `./build/octiber/tcp-client-server/server` — Синхронный сервер
* `./build/octiber/tcp-client-server/multiprocess-server` — Сервер на процессах
* `./build/octiber/tcp-client-server/concurrent-server` — Многопоточный сервер на epoll

## Запуск

### 1. Серверы
```bash
./build/server [<name> <number>]
./build/multiprocess-server [<name> <number>]
./build/concurrent-server [<name> <number>]
```

### 2. Клиент
```bash
./build/client [-v1|-v2] [<name> <number>]
```

### 3. Нагрузочный тест (Go)
```bash
cd load_test
go run main.go [-n requests] [-c concurrency] [-v version] [-show-errors]
```

## Примеры использования

### 1. Запуск синхронной версии
Сервер:
```bash
./build/server "Server" 50
```
Клиент:
```bash
./build/client "Client" 25
```

### 2. Остановка сервера (число вне диапазона 1-100)
```bash
./build/client "Stopper" 0
```

### 3. Стресс-тест (1000 запросов в 50 потоков)
```bash
cd load_test
go run main.go -n 1000 -c 50 -v 2
```

| [⟵ Описание](./description.md) | [Демонстрация ⟶](./demo.md) |
|--------------------------------|-----------------------------|