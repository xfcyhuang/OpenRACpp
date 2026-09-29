/*
 * c23_probe.c — OpenRAC C23 基线探针(PORTING_PLAN.md §Phase 0 / §4.4)
 *
 * 验证两件事:
 *  1. 本项目依赖的全部 C23 语言特性在锁定工具链(clang/llvm-mingw, -std=gnu23)上可用;
 *  2. 确定性前提成立:整数回绕(-fwrapv)、算术右移、除法向零截断。
 * 该文件同时充当"特性白名单"的活文档:移植代码允许使用的特性以此处探测通过的为准。
 */
#include <stdio.h>
#include <stdint.h>
#include <limits.h>

/*
 * 线程后端探测(计划 §4.5):本工具链(llvm-mingw/UCRT)不含 <threads.h>,
 * Windows 上 ora_thread 封装 Win32(SRWLOCK/CONDITION_VARIABLE),POSIX 上用 pthread。
 */
#if __has_include(<threads.h>)
  #include <threads.h>
  #define PROBE_THREADS_C11 1
#elif defined(_WIN32)
  #include <windows.h>
  #define PROBE_THREADS_WIN32 1
#endif

/* 1. enum 指定底层类型 —— Order 线格式要求精确宽度(见计划 §4.1) */
enum ord_tag : uint8_t { ORD_ACK = 0x10, ORD_FIELDS = 0xFF };

/* 2. constexpr —— 代替 C# const / static readonly */
constexpr uint32_t kCellUnits = 1024; /* 1 cell = 1024 世界单位(Map.cs:976) */

/* 3. 标准属性 */
[[deprecated("probe only")]] static int legacy_probe(void) { return 1; }

/* 4. 位精度整数 —— 备用(如需 24bit 定点);白名单内但按需使用 */
static _BitInt(24) wide24(void) { return 1 << 20; }

/* 5. #embed —— 二进制资产嵌入(替代资源脚本) */
static const unsigned char blob[] = {
    #embed "embed_asset.bin"
};

/* 6. typeof / auto / 匿名结构体联合 / 指定初始化器 / 复合字面量 */
typedef struct { union { struct { int x, y; }; int v[2]; }; } cell_t;

static int failures = 0;
#define CHECK(cond) \
    do { if (!(cond)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

#if defined(PROBE_THREADS_C11)
static int shift_thread(void *arg) { (void)arg; return -1 >> 1; }
#elif defined(PROBE_THREADS_WIN32)
static DWORD WINAPI shift_thread(LPVOID arg) { (void)arg; return (DWORD)(-1 >> 1); }
#endif

int main(void) {
    /* —— 语言特性 —— */
    CHECK(sizeof(enum ord_tag) == 1);
    static_assert(kCellUnits == 1024, "constexpr 可用于 static_assert");
    CHECK(kCellUnits == 1024);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations" /* 该警告本身即 [[deprecated]] 生效的证据 */
    CHECK(legacy_probe() == 1);
#pragma clang diagnostic pop
    auto f = 2.5; typeof(f) g = f;
    CHECK(g == 2.5);
    CHECK((int)wide24() == (1 << 20));
    CHECK(sizeof(blob) == 4 && blob[0] == 0xDE && blob[2] == 0xBE && blob[3] == 0x78);

    cell_t c = { .v = { 7, 9 } };
    CHECK(c.x == 7 && c.y == 9);
    int *p = (int[]){ 4, 5 }; /* 复合字面量 */
    CHECK(p[1] == 5);

    /* —— 确定性前提(计划 §4.4,对应 C# unchecked 语义)—— */
    int32_t i = INT32_MIN;
    CHECK((int32_t)(i - 1) == INT32_MAX);   /* 有符号溢出必须回绕(-fwrapv) */
    CHECK((-1 >> 1) == -1);                 /* 算术右移(实现定义,需自检) */
    CHECK((-7 / 2) == -3 && (-7 % 2) == -1);/* 除法/取模向零截断(与 C# 一致) */

    /* —— 线程库(替代 Task/Thread,计划 §4.5;ora_thread 封装层的基础)—— */
#if defined(PROBE_THREADS_C11)
    thrd_t t;
    int r = thrd_create(&t, shift_thread, NULL);
    CHECK(r == thrd_success);
    if (r == thrd_success) { int res = 0; thrd_join(t, &res); CHECK(res == -1); }
    printf("thread backend: <threads.h>\n");
#elif defined(PROBE_THREADS_WIN32)
    HANDLE h = CreateThread(NULL, 0, shift_thread, NULL, 0, NULL);
    CHECK(h != NULL);
    if (h != NULL) { DWORD res = 0; WaitForSingleObject(h, INFINITE); GetExitCodeThread(h, &res); CloseHandle(h); CHECK((int)res == -1); }
    printf("thread backend: Win32 (CreateThread) — <threads.h> 不可用,ora_thread 走 Win32 路径\n");
#else
    printf("thread backend: 无(探测失败)\n");
    CHECK(0 && "no thread backend");
#endif

    if (failures == 0)
        printf("C23 baseline probe: ALL OK  (clang %s, __STDC_VERSION__ = %ldL)\n",
               __clang_version__, (long)__STDC_VERSION__);
    return failures;
}
