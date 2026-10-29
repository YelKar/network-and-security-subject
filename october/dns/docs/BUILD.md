# Сборка

| [⟵ Описание](./description.md) | [Демонстрация ⟶](./demo.md) |
|--------------------------------|-----------------------------|

## Стек технологий

* Компилятор: `GCC / С++ 26`
* Система сборки: `CMake`
* Сетевой стек: `Сокеты POSIX`

## Сборка проекта

**В корне проекта**

```bash
cmake -B build
cmake --build build
```

Данное задание скомпилируется в исполняемый файл `./build/october/smtp/smtp-client`.

## Запуск

```bash
> ./build/october/dns/dns
Usage: ./build/october/dns/dns <domain> [A|AAAA] [-d]
```

## Примеры использования
### 1. Стандартный запуск (только результат)
```bash
./build/october/dns/dns ya.ru A
```
Вывод:
```bash
77.88.55.242
5.255.255.242
77.88.44.242
```
### 2. Запрос IPv6 (тип AAAA)
```bash
./build/october/dns/dns ya.ru AAAA
```
Вывод:
```bash
2a02:6b8::2:242
```

### 3. Режим отладки (-d)
```bash
./build/october/dns/dns ya.ru AAAA -d
```

<details>
<summary>
Вывод
</summary>

```bash
[DEBUG] Resolve start: ya.ru (depth: 0)
[DEBUG] Iteration 1 (candidates: 13)
[DEBUG] Trying a.root-servers.net [198.41.0.4]
[DEBUG] Sending query to 198.41.0.4:53 (UDP)...
[DEBUG] Response: RCODE=0, AA=0, TC=0, AN=0, NS=6, AR=12
[DEBUG] No answers found. Inspecting authority...
[DEBUG] Glue match: a.dns.ripn.net -> 193.232.128.6
[DEBUG] Glue match: d.dns.ripn.net -> 194.190.124.17
[DEBUG] Glue match: f.dns.ripn.net -> 193.232.156.17
[DEBUG] Glue match: b.dns.ripn.net -> 194.85.252.62
[DEBUG] Glue match: e.dns.ripn.net -> 193.232.142.17
[DEBUG] Glue match: c.tld-servers.ru -> 194.190.122.17
[DEBUG] Iteration 2 (candidates: 6)
[DEBUG] Trying a.dns.ripn.net [193.232.128.6]
[DEBUG] Sending query to 193.232.128.6:53 (UDP)...
[DEBUG] Response: RCODE=0, AA=0, TC=0, AN=0, NS=2, AR=4
[DEBUG] No answers found. Inspecting authority...
[DEBUG] Glue match: ns2.yandex.ru -> 93.158.134.1
[DEBUG] Glue match: ns1.yandex.ru -> 213.180.193.1
[DEBUG] Iteration 3 (candidates: 2)
[DEBUG] Trying ns2.yandex.ru [93.158.134.1]
[DEBUG] Sending query to 93.158.134.1:53 (UDP)...
[DEBUG] Response: RCODE=0, AA=1, TC=0, AN=1, NS=0, AR=1
[DEBUG] Answer: ya.ru -> 2a02:6b8::2:242 (TTL: 600s)
[DEBUG] Resolved successfully: 1 records
2a02:6b8::2:242
```

</details>

| [⟵ Описание](./description.md) | [Демонстрация ⟶](./demo.md) |
|--------------------------------|-----------------------------|
