package main

/*
#cgo CFLAGS: -I..
#include "client_core.c"
#include "message.c"
#include "util.c"
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
	flag.Parse()

	hostC := C.CString("127.0.0.1")
	defer C.free(unsafe.Pointer(hostC))

	fmt.Printf("=========================================\n")
	fmt.Printf("Стресс-тест Go -> C client_core\n")
	fmt.Printf("Запросов: %d | Горутин: %d | Версия: v%d\n", *requests, *concurrency, *version)
	fmt.Printf("=========================================\n")

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

				status := C.RunClientSession(hostC, C.uint16_t(5001), &cfg, false)
				C.free(unsafe.Pointer(cName))

				if status == 0 {
					successCount.Add(1)
				} else {
					errorCount.Add(1)
				}
			}
		}(w)
	}

	wg.Wait()
	elapsed := time.Since(startTime)
	rps := float64(successCount.Load()) / elapsed.Seconds()

	fmt.Printf("\n--- РЕЗУЛЬТАТЫ ---\n")
	fmt.Printf("Успешно:    %d / %d\n", successCount.Load(), *requests)
	fmt.Printf("Ошибок:     %d\n", errorCount.Load())
	fmt.Printf("Время:      %.3f сек\n", elapsed.Seconds())
	fmt.Printf("Пропускная: %.2f req/sec (RPS)\n", rps)
	fmt.Printf("-------------------\n")
}
