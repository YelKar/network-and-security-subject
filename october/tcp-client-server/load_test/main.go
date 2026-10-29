package main

/*
#cgo CFLAGS: -I../include -I../include/tcp-server
#include "../src/client_core.c"
#include "../src/message.c"
#include "../src/util.c"
#include <stdlib.h>
*/
import "C"

import (
	"flag"
	"fmt"
	"sync"
	"sync/atomic"
	"time"
	"unsafe"
)

func main() {
	requests := flag.Int("n", 1000, "Общее количество запросов")
	concurrency := flag.Int("c", 50, "Количество параллельных горутин")
	version := flag.Int("v", 2, "Версия протокола (1 или 2)")
	showErrors := flag.Bool("show-errors", false, "Выводить подробности об ошибках запросов")
	flag.Parse()

	hostC := C.CString("127.0.0.1")
	defer C.free(unsafe.Pointer(hostC))

	fmt.Printf("\033[1;33m--- Стресс-тест ---\033[0m\n")
	fmt.Printf(
		"Запросов: \033[1;34m%d\033[0m\n"+
			"Горутин:  \033[1;34m%d\033[0m\n"+
			"Версия:   \033[1;32mv%d\033[0m\n",
		*requests,
		*concurrency,
		*version,
	)
	fmt.Printf("\033[1;33m-------------------\033[0m\n\n")

	var successCount atomic.Uint64
	var errorCount atomic.Uint64

	jobs := make(chan int, *requests)
	for i := 0; i < *requests; i++ {
		jobs <- i
	}
	close(jobs)

	var wg sync.WaitGroup
	startTime := time.Now()

	for w := 0; w < *concurrency; w++ {
		wg.Add(1)
		go func(workerID int) {
			defer wg.Done()
			for reqID := range jobs {
				nameStr := fmt.Sprintf("GoBot_%d", reqID)
				cName := C.CString(nameStr)

				cfg := C.struct_Config{
					name:    cName,
					number:  C.int32_t(42),
					version: C.uint8_t(*version),
				}

				status, err := C.RunClientSession(hostC, C.uint16_t(5001), &cfg, false)
				C.free(unsafe.Pointer(cName))

				if status == 0 {
					successCount.Add(1)
				} else {
					errorCount.Add(1)

					if *showErrors {
						errMsg := "соединение разорвано или сервер прислал некорректный ответ"
						if err != nil && err.Error() != "errno 0" && err.Error() != "success" {
							errMsg = err.Error()
						}
						fmt.Printf("[Worker #%d | Req #%d] Ошибка (code=%d): %s\n", workerID, reqID, status, errMsg)
					}
				}
			}
		}(w)
	}

	wg.Wait()
	elapsed := time.Since(startTime)
	rps := float64(successCount.Load()) / elapsed.Seconds()

	fmt.Printf("\033[33;1m----------- РЕЗУЛЬТАТЫ -----------\033[0m\n")
	fmt.Printf("Успешно:      \033[1;32m%d / %d\033[0m\n", successCount.Load(), *requests)
	fmt.Printf("Ошибок:       \033[1;31m%d\033[0m\n", errorCount.Load())
	fmt.Printf("Время:        \033[1;34m%.3f\033[0m сек\n", elapsed.Seconds())
	fmt.Printf("Проп. сп-сть: \033[1;34m%.2f\033[0m req/sec (RPS)\n", rps)
	fmt.Printf("\033[33;1m-----------------------------------\033[0m\n")
}
