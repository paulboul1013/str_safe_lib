# str_safe_lib

這是一個學習用途的 C string/memory safety library，實作 Linux kernel 風格的 `strscpy`、`strscpy_pad`、`strtomem` 與相關 byte operations。

目前提供：

- `strscpy`、`strscpy_pad`
- `strtomem`、`strtomem_pad`
- `memcpy_pad`
- `memset`、`memcpy`、`memmove`、`strnlen`
- `strscpy_auto`、`strtomem_auto`、`strtomem_pad_auto`：只接受真正的 destination array，pointer 會在編譯期被拒絕

## 簡單使用方式

下面的案例示範安全複製 C string，以及把較短的 byte array 複製到固定大小欄位並用 `0` 補滿：

```c
#include <stdio.h>

#include "String.h"

int main(void)
{
    char message[8];
    const unsigned char source[] = {1, 2, 3};
    unsigned char destination[5];
    size_t i;

    if (strscpy_auto(message, "hello") < 0) {
        fprintf(stderr, "message was truncated\n");
        return 1;
    }

    memcpy_pad(destination, sizeof(destination),
               source, sizeof(source), 0);

    printf("%s\n", message);
    for (i = 0; i < sizeof(destination); ++i) {
        printf("%u%s", (unsigned int)destination[i],
               i + 1U == sizeof(destination) ? "\n" : " ");
    }

    return 0;
}
```

將程式存成 `example.c`，在 repo 根目錄編譯並執行：

```sh
gcc -std=c11 -O2 -Wall -Wextra -Werror String.c example.c -o example
./example
```

預期輸出：

```text
hello
1 2 3 0 0
```

`strscpy_auto` 只能接受真正的陣列；若 destination 是 pointer，請改用 `strscpy(pointer, source, actual_capacity)`，並確保 `actual_capacity` 是實際可寫空間。

## 測試

```sh
make test       # GCC/Clang unit、guard page、100k property fuzz、compile-fail
make memory     # ASan/UBSan/pointer sanitizer、Valgrind
make check      # test + memory + optimization/hardening matrix
make benchmark
```

完整結果與已修正缺陷見 [安全測試報告](docs/SECURITY_TEST_REPORT.md)；與 Linux kernel 當前 API、FORTIFY 與 deprecated policy 的一手來源比較見 [Linux API 對照](docs/LINUX_API_COMPARISON.md)。

## 目前限制

這份 library 已通過現有測試，但仍不是 Linux kernel 同級的安全層，也不能取代成熟 libc。raw size API 仍依賴 caller 傳入真實容量；尚無 kernel-style object-size/FORTIFY、string/non-string type contract 或舊 API 禁用政策。自訂 memory primitives 在本機 benchmark 的大區塊吞吐約為 glibc 的 2.2%–5.5%，production user-space 應優先使用 libc 並為專案 API 加 namespace。

Linux kernel 移除的是 kernel 內的 `strncpy()`，並淘汰 `strlcpy()`；這不表示 user-space `<string.h>` 已刪除這些函式。kernel 目前仍保留 `strcpy()` 相容介面，但明確禁止新增使用。
