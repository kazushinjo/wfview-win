#include <QtGlobal>
#include "crashhandler.h"

#if defined(Q_OS_WIN) && defined(_M_X64)

#include <windows.h>
#include <dbghelp.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

namespace {

wchar_t logPathW[MAX_PATH];
wchar_t dumpPathW[MAX_PATH];
volatile LONG handling = 0;

// Plain Win32 file append: the handler may run while Qt's log mutex is held.
void writeLog(const char *text)
{
    HANDLE h = CreateFileW(logPathW, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return;
    DWORD written = 0;
    WriteFile(h, text, DWORD(strlen(text)), &written, nullptr);
    CloseHandle(h);
}

void writeDump(EXCEPTION_POINTERS *ep)
{
    HANDLE f = CreateFileW(dumpPathW, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return;
    MINIDUMP_EXCEPTION_INFORMATION mei;
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = ep;
    mei.ClientPointers = FALSE;
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), f,
                      MINIDUMP_TYPE(MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory),
                      ep ? &mei : nullptr, nullptr, nullptr);
    CloseHandle(f);
}

void writeStack(CONTEXT ctx)
{
    HANDLE proc = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    SymInitialize(proc, nullptr, TRUE);

    STACKFRAME64 sf = {};
    sf.AddrPC.Offset = ctx.Rip;
    sf.AddrPC.Mode = AddrModeFlat;
    sf.AddrFrame.Offset = ctx.Rbp;
    sf.AddrFrame.Mode = AddrModeFlat;
    sf.AddrStack.Offset = ctx.Rsp;
    sf.AddrStack.Mode = AddrModeFlat;

    alignas(SYMBOL_INFO) char symBuf[sizeof(SYMBOL_INFO) + 512];
    char line[1200];
    for (int i = 0; i < 64; ++i)
    {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, GetCurrentThread(), &sf, &ctx, nullptr,
                         SymFunctionTableAccess64, SymGetModuleBase64, nullptr) || sf.AddrPC.Offset == 0)
            break;

        const DWORD64 pc = sf.AddrPC.Offset;
        char module[MAX_PATH] = "?";
        const DWORD64 base = SymGetModuleBase64(proc, pc);
        if (base)
        {
            char full[MAX_PATH];
            if (GetModuleFileNameA(HMODULE(base), full, MAX_PATH))
            {
                const char *slash = strrchr(full, '\\');
                strncpy_s(module, slash ? slash + 1 : full, _TRUNCATE);
            }
        }

        auto *sym = reinterpret_cast<SYMBOL_INFO *>(symBuf);
        memset(symBuf, 0, sizeof(symBuf));
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 511;
        DWORD64 symDisp = 0;
        const bool haveSym = SymFromAddr(proc, pc, &symDisp, sym);

        IMAGEHLP_LINE64 src = {};
        src.SizeOfStruct = sizeof(src);
        DWORD lineDisp = 0;
        const bool haveLine = SymGetLineFromAddr64(proc, pc, &lineDisp, &src);

        if (haveSym && haveLine)
            snprintf(line, sizeof(line), "    #%02d %s!%s+0x%llx  (%s:%lu)\n", i, module, sym->Name,
                     (unsigned long long)symDisp, src.FileName, src.LineNumber);
        else if (haveSym)
            snprintf(line, sizeof(line), "    #%02d %s!%s+0x%llx\n", i, module, sym->Name, (unsigned long long)symDisp);
        else
            snprintf(line, sizeof(line), "    #%02d %s+0x%llx\n", i, module, (unsigned long long)(pc - base));
        writeLog(line);
    }
    SymCleanup(proc);
}

void report(const char *reason, EXCEPTION_POINTERS *ep)
{
    if (InterlockedExchange(&handling, 1))
        return;

    SYSTEMTIME t;
    GetLocalTime(&t);
    char head[512];
    int n = snprintf(head, sizeof(head), "%04d-%02d-%02d %02d:%02d:%02d.%03d FTL crash: %s, thread 0x%lx",
                     t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,
                     reason, GetCurrentThreadId());
    if (ep && n > 0 && n < int(sizeof(head)))
        n += snprintf(head + n, sizeof(head) - n, ", exception 0x%08lx at %p",
                      ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress);
    if (n > 0 && n < int(sizeof(head)))
        snprintf(head + n, sizeof(head) - n, "\n");
    writeLog(head);

    CONTEXT ctx;
    if (ep)
        ctx = *ep->ContextRecord;
    else
        RtlCaptureContext(&ctx);
    writeStack(ctx);

    writeDump(ep);
    char tail[MAX_PATH + 32];
    snprintf(tail, sizeof(tail), "    minidump: %ls\n", dumpPathW);
    writeLog(tail);
}

LONG WINAPI onUnhandledException(EXCEPTION_POINTERS *ep)
{
    report("unhandled exception", ep);
    return EXCEPTION_CONTINUE_SEARCH;
}

void onAbort(int)
{
    report("abort() called", nullptr);
}

void onInvalidParameter(const wchar_t *, const wchar_t *, const wchar_t *, unsigned int, uintptr_t)
{
    report("CRT invalid parameter", nullptr);
    std::abort();
}

void onPureCall()
{
    report("pure virtual function call", nullptr);
    std::abort();
}

} // namespace

void installCrashHandler(const wchar_t *logPath)
{
    wcsncpy_s(logPathW, logPath, _TRUNCATE);
    wcsncpy_s(dumpPathW, logPath, _TRUNCATE);
    wchar_t *dot = wcsrchr(dumpPathW, L'.');
    if (dot && size_t(dot - dumpPathW) + 5 <= MAX_PATH)
        wcscpy_s(dot, MAX_PATH - (dot - dumpPathW), L".dmp");

    SetUnhandledExceptionFilter(onUnhandledException);
    signal(SIGABRT, onAbort);
    _set_invalid_parameter_handler(onInvalidParameter);
    _set_purecall_handler(onPureCall);
}

#elif defined(Q_OS_WIN)

void installCrashHandler(const wchar_t *) {}

#endif
