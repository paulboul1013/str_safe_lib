# String safe library 安全測試報告

測試日期：2026-10-16

## 結論

本輪修正後，repo 的 9 個 runtime API 與 3 個 array-sized wrapper 在目前測試範圍內通過 GCC、Clang、ASan、UBSan、pointer sanitizer、Valgrind、guard page 與 deterministic property fuzz。測試過程實際找出並修正 4 個可造成越界或未定義行為的缺陷。

這仍不是與 Linux kernel 同級的安全 string layer，也不適合作為 libc memory/string primitives 的 drop-in replacement。raw size API 仍相信 caller 提供的長度；library 沒有 kernel FORTIFY/object-size runtime checks、string/non-string 類型系統或舊 API 禁用政策；自行輸出的 `memcpy`／`memmove`／`memset` 也明顯慢於 glibc。

Linux API 與安全模型的逐項一手來源比較見 [LINUX_API_COMPARISON.md](LINUX_API_COMPARISON.md)。

## 找到並修正的缺陷

| 嚴重度 | 缺陷 | 修正與證據 |
| --- | --- | --- |
| Critical | `strscpy_auto(pointer, src)` 把 `sizeof(pointer)` 當容量，4-byte heap buffer 可被當成 8 bytes 寫入。 | `STRSAFE_MUST_BE_ARRAY` 已接入 `strscpy_auto`；`strtomem_auto` 與 `strtomem_pad_auto` 也採同一防護。GCC/Clang 共 6 個 compile-fail cases 證明 pointer 會被拒絕。 |
| High | GCC 13.3 在 `-O2` 將 replacement `memset` 迴圈改寫成 `memset` 呼叫，符號解析回自身，測試以 exit 139 segfault。 | 對 GCC 關閉該函式的 loop-distribute-pattern rewrite，對 Clang 使用 per-function `no_builtin`。一般 `-O2`、`-O3 -flto` 與 hardened build 均通過。 |
| High | 舊 `memmove` 對不同物件的 pointer 做關係比較，屬 ISO C undefined behavior；反向 loop 還把 `size_t` 轉成 `ssize_t`。 | 使用 `uintptr_t` unsigned address difference 選方向，反向複製改成先遞減 `size_t`。pointer sanitizer 與隨機 overlap model 通過。這個位址模型適用本專案鎖定的 Linux/GCC/Clang 平台，並非純 ISO C 可攜保證。 |
| High | `memmove` 在 destination 位於 source 前方且重疊時呼叫 `memcpy`；結果看似正確，但違反 `memcpy` non-overlap contract。Clang ASan 報 `memcpy-param-overlap`。 | 該路徑改由 `memmove` 自己向前複製；GCC/Clang sanitizer fuzz 均通過。 |
| Medium | `E2BIG` 被硬編碼成 7，helper 使用保留的雙底線名稱，另有 signed/unsigned conversion warnings。 | 改從 `<errno.h>` 取得 `E2BIG`、helper 改用 `STRSAFE_` prefix，並消除嚴格警告。 |

## 驗證矩陣

環境為 Linux 6.18 WSL2 x86-64、AMD Ryzen 7 PRO 4750G、GCC 13.3、Clang 18.1.3、Valgrind 3.22。

| 驗證 | 規模 | 結果 |
| --- | --- | --- |
| deterministic unit/edge tests | 9 runtime APIs、3 wrappers、68 checks；zero/one/exact/truncate/pad/overlap/canary/return values | GCC、Clang PASS |
| property fuzz | 每個 compiler 100,000 iterations，固定 seed `0x6a09e667f3bcc909` | GCC、Clang PASS |
| ASan + UBSan + pointer checks | 每個 compiler 20,000 fuzz iterations | GCC、Clang PASS |
| Valgrind | 5,000 fuzz iterations；full leak/origin checks | PASS，0 error |
| guard pages | source/destination 貼齊 `PROT_NONE` page；含 zero-size inaccessible pointers | GCC、Clang PASS |
| compile-fail | 3 個 auto wrappers × GCC/Clang | 6/6 如預期拒絕 pointer |
| optimization matrix | GCC `-O0`、GCC `-O3 -flto`、Clang `-O3` | 全部 68 checks PASS |
| hardened build | GCC `-D_FORTIFY_SOURCE=3 -fstack-protector-strong -fPIE -pie` | 68 checks PASS；這不代表自訂 API 自動取得 kernel-style FORTIFY |
| strict warnings | `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -Werror` | library、tests、demo PASS |

重跑方式：

```sh
make check
make benchmark
```

LeakSanitizer 在此受 ptrace 管理的環境不能執行，因此 ASan 設定 `detect_leaks=0`；Valgrind full leak check 補上 leak 驗證。

## 效率結果

benchmark 為 warm-cache、每項取 3 次最佳值。它適合看數量級，不是跨機器的穩定承諾。表中 ratio 為 project/libc；`1.0x` 才代表同速。

| API | 32 B | 256 B | 4 KiB | 64 KiB |
| --- | ---: | ---: | ---: | ---: |
| `memcpy` | 0.193x | 0.042x | 0.031x | 0.050x |
| `memmove` overlap +1 | 0.428x | 0.138x | 0.037x | 0.054x |
| `memset` | 0.241x | 0.034x | 0.031x | 0.031x |
| `strnlen` | 0.243x | 0.068x | 0.022x | 0.031x |

大區塊的自訂 memory primitives 約只有 glibc 的 2.2%–5.5% 吞吐，約慢 18–45 倍。repo-specific API 與同樣逐 byte 的透明 C model 大致相近，但這不等於與 Linux kernel 的 word-at-a-time 或架構最佳化實作同速。

## 安全邊界與剩餘工作

- `strscpy(dst, src, size)` 等 raw API 無法知道實際 object size。若 caller 傳入過大的 `size`，C 函式本身不能阻止越界。
- 來源必須在宣告可讀範圍內含 NUL，或至少有 API 允許讀取的 bytes；來源與目的端不得重疊，只有 `memmove` 允許重疊。
- auto wrappers 只驗證 destination 是陣列，未驗證 C-string、byte array 或 `__nonstring` 意圖，也未限制 source object size。
- 標準名稱的 memory primitives 可能 interpose libc、影響整個 process，也使 compiler builtin 行為變複雜。production user-space 版本應 namespace，並使用 libc primitives。
- 若要達到 kernel 類似治理，需要 object-size fortified wrappers、目的類型契約、`memtostr*` 對稱 API，以及在 CI/靜態分析層禁止新增 `strcpy`／`strncpy`／`strlcpy` call sites。
