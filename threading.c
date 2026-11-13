#include <conio.h>
#include <windows.h>
#include <stdio.h>

volatile LONG g_state = 1;

DWORD WINAPI PrintThread(LPVOID lpParam) {
    while (1) {
        char ch = _getch();
        if (ch == 'a')
            InterlockedExchange(&g_state, 0);
    }
    return 0;
}

int main(void) {
    HANDLE thread = CreateThread(NULL, 0, PrintThread, NULL, 0, NULL);
    if (thread == NULL) {
        return 1;
    }

    while (1) {
        if (InterlockedCompareExchange(&g_state, 0, 0) == 1) {
            printf("1");
        } else {
            printf("0");
        }
        Sleep(1000);
    }

    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);

    return 0;
}
