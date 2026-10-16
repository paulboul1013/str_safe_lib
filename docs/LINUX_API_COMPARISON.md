# 與 Linux kernel 字串安全 API 的比較

## 範圍與基準

本文件比較本 repo 的 [`String.h`](../String.h)／[`String.c`](../String.c) 與 Linux kernel 的字串複製 API、安全契約及 FORTIFY 防護。Linux 來源固定在 2026-10-16 查得的 `torvalds/linux` HEAD：[`2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e`](https://github.com/torvalds/linux/tree/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e)，避免 `master` 後續變動使結論失去對應版本。

這裡的「Linux 已刪除舊 API」只指 kernel 自己的程式碼與內部 API，不表示 glibc 或其他 user-space C library 已移除同名函式。Linux 官方 deprecated 文件也採逐個 API 處理，而不是移除整個傳統 string library。

## Linux 目前的 deprecated／removed 狀態

| API | kernel 現況 | 官方理由與替代方案 |
| --- | --- | --- |
| `strcpy()` | **仍保留，但明確要求不要新增使用。** `include/linux/string.h` 仍宣告它；FORTIFY 也仍提供包裝。 | 沒有目的端界限。官方建議改用 `strscpy()`，並注意回傳值由目的端指標改成已複製長度／負 errno。[deprecated.rst L122-L132](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/Documentation/process/deprecated.rst#L122-L132)、[string.h L67-L70](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/include/linux/string.h#L67-L70)、[fortify-string.h L719-L755](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/include/linux/fortify-string.h#L719-L755) |
| `strncpy()` | **已從 kernel 移除。** | 可能不終止目的字串；固定填滿 NUL 又會造成不必要的成本；同一 API 混合多種意圖。替代 API 依 C-string／fixed-width non-string、是否 padding 分流。[deprecated.rst L134-L159](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/Documentation/process/deprecated.rst#L134-L159) |
| `strlcpy()` | **列為 deprecated；在此快照的核心 `include/linux/string.h` 與 `lib/string.c` 已無宣告／定義。** | 為了回傳完整來源長度會讀完整來源，既低效，也可能對未終止來源產生線性越界讀取；改用 `strscpy()`。[deprecated.rst L161-L168](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/Documentation/process/deprecated.rst#L161-L168) |

因此，正確說法不是「Linux 已刪除所有舊的不安全字串 API」。`strncpy()` 已移除，`strlcpy()` 已退出核心 API，但 `strcpy()` 仍為相容性／編譯器降級等情況保留，政策上禁止新用法，並以 FORTIFY 降低部分風險。kernel 甚至仍保留 `stpcpy()` 定義，以處理編譯器可能產生的 libcall，同時把它標為不安全且不建議使用。[lib/string.c L175-L197](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/lib/string.c#L175-L197)

## Linux 替代 API 的精確契約

### `strscpy()`

目前公開介面是宏，底層函式為 `sized_strscpy()`：

- 目的端是可推導大小的陣列時可用二參數形式 `strscpy(dst, src)`；目的端是指標，或只想使用陣列的一部分時，必須傳第三個 `size`。
- 二參數形式用 `sizeof(dst) + __must_be_array(dst)`，因此把指標誤傳成陣列會在編譯期失敗，而不是把 `sizeof(pointer)` 當容量。
- `dst` 與 `src` 都必須是 C-string 類型；重疊時行為未定義。
- `size != 0` 時一定 NUL 終止目的端。
- 成功回傳不含尾端 NUL 的複製字元數；`size == 0` 或截斷時回傳 `-E2BIG`。
- 不做尾端 padding，因此短字串的成本只與實際字串長度相關。

以上契約與宏檢查見 [string.h L72-L111](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/include/linux/string.h#L72-L111)。底層實作也拒絕 `count == 0` 或 `count > INT_MAX`，截斷時強制寫入 NUL；一般組態會採 word-at-a-time 掃描／複製，而不是逐 byte 迴圈。[lib/string.c L100-L171](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/lib/string.c#L100-L171)

### `strscpy_pad()`

`strscpy_pad()` 的複製、重疊、終止及回傳契約與 `strscpy()` 相同。只有在來源完整放入時，它才把尾端剩餘空間填成 NUL；截斷時不再做額外 padding。[string.h L113-L146](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/include/linux/string.h#L113-L146)

padding 適合 fixed-width 欄位或會跨權限邊界而必須清除殘留資料的結構。一般 C-string 若不需要這個保證，使用 `strscpy()` 可避免不必要的全緩衝區寫入。

### `strtomem()`／`strtomem_pad()`

這兩個 API 的目的端語意與 `strscpy()` 不同：目的端是**固定寬度、刻意不 NUL 終止的 byte array**。

- kernel 不接受 `dest_len` 參數；目的陣列大小必須能由編譯器推導。
- 目的端必須是 byte array，並標記 `__nonstring`；來源必須是 C-string。
- `strtomem()` 複製最多 `sizeof(dest)` 個非 NUL byte，不把 NUL 複製進目的端，也不填滿剩餘空間。
- `strtomem_pad()` 做同樣的複製，再以呼叫者指定的 byte 填滿目的端。
- kernel 同時取得來源的 `__builtin_object_size(src, 1)`，以 `min(source_object_size, dest_size)` 限制 `strnlen()` 的讀取上界。

實際宏及契約見 [string.h L390-L443](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/include/linux/string.h#L390-L443)。陣列／byte array／C-string／`__nonstring` 的型別檢查定義見 [compiler.h L197-L230](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/include/linux/compiler.h#L197-L230)。

這也是 kernel 不用一個 `strncpy()` 處理所有情況的核心：

| 資料流目的 | 不需 padding | 需 padding |
| --- | --- | --- |
| C-string → C-string | `strscpy()` | `strscpy_pad()` |
| fixed-width non-string → C-string | `memtostr()` | `memtostr_pad()` |
| C-string → fixed-width non-string | `strtomem()` | `strtomem_pad()` |
| runtime 大小、來源可能未終止 | 依明確長度使用 memory API | `memcpy_and_pad()` |

完整替代表來自 [deprecated.rst L146-L159](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/Documentation/process/deprecated.rst#L146-L159)。

## FORTIFY 與 object-size 防護

Linux 的安全性不只來自「換一個函式名稱」，而是由 API 契約、編譯器可見的物件大小和執行期檢查共同構成。

1. `__struct_size(p)` 優先使用 `__builtin_dynamic_object_size(p, 0)`，`__member_size(p)` 使用 type 1；編譯器不支援 dynamic object size 時退回 `__builtin_object_size()`。[compiler_types.h L515-L530](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/include/linux/compiler_types.h#L515-L530)
2. `CONFIG_FORTIFY_SOURCE`、最佳化開啟且未定義 `__NO_FORTIFY` 時，`linux/string.h` 載入 fortified wrappers。[string.h L382-L388](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/include/linux/string.h#L382-L388)
3. fortified `sized_strscpy()` 在 size 與目的端 member size 可於編譯期比較時產生 write-overflow 編譯錯誤；其他已知大小案例先用 bounded `strnlen()` 防止來源越界讀，再於執行期檢查目的端大小；安全的常數字串路徑可直接降成 `memcpy()`。[fortify-string.h L225-L280](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/include/linux/fortify-string.h#L225-L280)
4. fortified `memcpy()`／`memmove()` 同時攜帶 enclosing object 與 individual member 的大小。常數越界可成為編譯錯誤或 field-spanning 警告；已知 object size 的執行期越界會進入 `fortify_panic()`。[fortify-string.h L497-L567](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/include/linux/fortify-string.h#L497-L567)、[fortify-string.h L575-L647](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/include/linux/fortify-string.h#L575-L647)

這套防護不是萬能。若編譯器無法知道物件大小，size 會是 `SIZE_MAX`，wrapper 只能退回原始函式；kernel 原始碼也明列 flexible array、某些結構尾端陣列等偵測限制。[fortify-string.h L598-L634](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/include/linux/fortify-string.h#L598-L634) 因此 FORTIFY 是 defense in depth，不能把錯誤長度或錯誤資料模型自動變安全。

## 本 repo 的逐項比較

| 項目 | 本 repo | 與 kernel 的差距／風險 |
| --- | --- | --- |
| `strscpy()` 基本語意 | [`String.c` L41-L60](../String.c#L41-L60) 對 `size == 0`／`size > INT_MAX` 回 `-E2BIG`；成功回長度；截斷時寫 NUL。 | 一般可讀 C-string 與有效目的範圍下，結果大致符合 kernel。overlap 仍需視為 undefined；沒有 object-size read/write 防護。 |
| `strscpy_pad()` | [`String.c` L62-L75](../String.c#L62-L75) 只在成功時填滿尾端 NUL。 | 基本結果符合 kernel；安全差距承襲 `strscpy()` 與自訂 `memset()`。 |
| 自動大小 API | [`String.h` L38-L46](../String.h#L38-L46) 的三個 `*_auto` 宏使用 `sizeof(dst) + STRSAFE_MUST_BE_ARRAY(dst)`。 | 本輪已修正原本的 pointer-size 漏洞；GCC/Clang compile-fail 測試證明 pointer 會被拒絕。仍沒有 kernel 的 C-string／byte-array／`__nonstring` 類型區分，也沒有來源 object-size 防護。 |
| `strtomem*()` | [`String.c` L126-L138](../String.c#L126-L138) 的底層函式仍接受 `void *dest` 與 runtime `dest_len`；新增的 `strtomem_auto`／`strtomem_pad_auto` 可為固定陣列推導大小。 | auto 形式避免把 pointer 的 `sizeof` 當容量，但整體契約仍比 kernel 弱：沒有 `__nonstring`，也未以來源 object size 限制掃描。raw 形式若收到錯誤 `dest_len`，仍可能目的端 overflow 或來源越界讀。 |
| `memcpy_pad()` | [`String.c` L112-L124](../String.c#L112-L124) 複製 `min(src_len, dest_len)` 後填滿目的端。 | 行為接近 kernel `memcpy_and_pad()` 的核心工作，但不同名；對來源與目的端傳入長度沒有 object-size 驗證。 |
| `memcpy()`／`memmove()`／`memset()` | repo 自行輸出標準名稱並逐 byte 實作。[`String.c` L19-L29](../String.c#L19-L29)、[`String.c` L77-L110](../String.c#L77-L110) | 本輪已修正 GCC `-O2` 將 `memset` 改寫成自我呼叫、跨物件 pointer relational UB、signed reverse index，以及 `memmove` 將重疊區域交給 `memcpy` 的問題。仍沒有 FORTIFY；覆蓋／interpose libc 標準符號增加整合風險，效率也遠低於 libc。 |
| 編譯期型別檢查 | `*_auto` 會拒絕 pointer destination；沒有 byte-array、C-string、`__nonstring` 檢查。 | 已封住最直接的 array/pointer 誤用，但仍無法完整表達或強制 kernel 的資料模型。raw runtime-size API 仍由 caller 保證真實容量。 |
| object-size／FORTIFY | 無。 | 不能在常數越界時讓編譯失敗，也不能在編譯器知道物件大小時攔截動態越界。 |
| 錯誤碼與命名 | [`String.h` L4-L25](../String.h#L4-L25) 現從 `<errno.h>` 取得 `E2BIG`，helper 宏也已改用 `STRSAFE_` prefix。 | 原本的硬編碼與保留識別字問題已修正；header 仍依賴 GCC/Clang type builtins 與 POSIX `ssize_t`，不是純 ISO C／MSVC 可攜介面。 |
| 效率 | 所有核心 primitive 與 `strscpy()` 都是逐 byte 迴圈。 | kernel `sized_strscpy()` 可 word-at-a-time；正式 user-space libc 的 `memcpy`／`memmove`／`memset` 通常還有架構專用、向量化與 size-specific 路徑。這份實作不能視為同等效率的替代。 |

### 目前安全等級的結論

本 repo 經本輪修正後，已能正確處理受測語意、編譯期拒絕 auto API 的 pointer destination，並通過雙編譯器、sanitizer、guard-page、Valgrind 與 property fuzz。不過它 **仍未達到 Linux kernel 淘汰 `strncpy()` 後的完整安全模型，也不適合作為 drop-in 的完整安全 string library**。它尚未做到：

- 在編譯期區分錯誤的 string/non-string 類型；
- 用 compiler object size 驗證讀寫界限；
- 在已知大小但動態長度越界時 fail closed；
- 從專案層級禁止呼叫者繼續使用 libc 的 `strcpy()`／`strncpy()`／`strlcpy()`；
- 涵蓋 `memtostr*()` 等相反資料流，讓每個 API 只代表單一意圖；
- 提供與 kernel 或成熟 libc 相當的架構最佳化、sanitizer 整合與長期測試面。

單純不在 `String.h` 宣告舊 API 並不會刪除它們：呼叫者仍可包含系統 `<string.h>`、自行宣告，或直接連結 libc。若目標是讓某個專案像 kernel 一樣完成 migration，需要建置規則、靜態分析或 compile-time error/deprecated attribute 配合，並把所有 call site 移到目的明確的新 API。

## 建議的達標順序

1. **已完成：修正自動大小介面與核心 UB。** 三個 auto API 現只接受真正的陣列；`memset` 最佳化遞迴、`memmove` 指標比較／反向 index／錯用 `memcpy` 也已由回歸測試固定。
2. **不要在安全層自行替代標準 memory primitives。** 將 library API namespaced，底層使用 toolchain/libc 的 `memcpy`、`memmove`、`memset`，保留 sanitizer、`_FORTIFY_SOURCE` 與平台最佳化；若目標是 freestanding 環境，則把該限制明確寫進規格並另做實作。
3. **建立 object-size wrapper。** 對 GCC/Clang 使用 `__builtin_dynamic_object_size`／`__builtin_object_size`；常數越界觸發 compile-time error，已知物件大小的動態越界走明確的錯誤或 trap 路徑。未知大小不能宣稱已被保護。
4. **把 string 與 fixed-width non-string 分型。** `strtomem*()` 應要求 non-string destination；另補 `memtostr*()`。若保留 runtime 長度版本，應使用不同名稱，避免與 kernel 同名但契約不同。
5. **建立移除舊 API 的 enforcement。** 對受控程式碼開啟禁止／棄用診斷、CI source scan 與 migration 測試；這才等同 kernel 的「所有 caller 已遷移」，而非只有替代函式存在。
6. **持續執行現有驗證矩陣。** `make test` 覆蓋雙編譯器、guard page、property fuzz 與 compile-fail；`make memory` 覆蓋 ASan／UBSan／pointer checks 與 Valgrind；`make benchmark` 追蹤效能。

## 一手來源

- Linux kernel deprecated interfaces: [`Documentation/process/deprecated.rst`](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/Documentation/process/deprecated.rst)
- Linux string API declarations and intent-specific macros: [`include/linux/string.h`](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/include/linux/string.h)
- Linux generic string implementation: [`lib/string.c`](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/lib/string.c)
- Linux fortified wrappers: [`include/linux/fortify-string.h`](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/include/linux/fortify-string.h)
- Compiler object-size selection: [`include/linux/compiler_types.h`](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/include/linux/compiler_types.h)
- Compile-time array/string-kind checks: [`include/linux/compiler.h`](https://github.com/torvalds/linux/blob/2f2f7eeef8b0a1a9b9ca998231419873ccae8d4e/include/linux/compiler.h)
