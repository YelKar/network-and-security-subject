#include <tcp-server/util.h>
#include <stdio.h>

void PrintResult(const char* myName, const char* otherName, int myNumber, int otherNumber)
{
	printf("\033[1m|---------------------\033[0m\n");
	printf("\033[1m| My name: \033[0;3;33m%s\033[0m\n", myName);
	printf("\033[1m| Other name: \033[0;3;33m%s\033[0m\n", otherName);
	printf("\033[1m| My number: \033[34m%d\033[0m\n", myNumber);
	printf("\033[1m| Other number: \033[34m%d\033[0m\n", otherNumber);
	printf("\033[1m|---------------------\033[0m\n");
	printf("\033[1m| Sum: \033[31m%d\033[0m\n", otherNumber + myNumber);
	printf("\033[1m|---------------------\033[0m\n");
}
