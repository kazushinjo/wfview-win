#ifndef CRASHHANDLER_H
#define CRASHHANDLER_H

#ifdef Q_OS_WIN
// On abort(), an unhandled exception, a CRT invalid parameter or a pure virtual
// call, appends a symbolised stack trace to logPath and writes a minidump next
// to it (same name, .dmp). Needs the .pdb beside the exe for function names.
void installCrashHandler(const wchar_t *logPath);
#endif

#endif // CRASHHANDLER_H
