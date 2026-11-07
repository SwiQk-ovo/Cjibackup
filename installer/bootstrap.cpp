// 安装包引导程序（纯 Win32，不依赖 Qt）：
// 把「安装器 + Qt 运行库 + 载荷」解到临时目录再启动安装器。
// 单文件安装包 = bootstrap.exe + boot.pack + footer{uint64 size, "CJIBOOT1"}
// boot.pack    = "CJIBT001" + uint32 count + 条目（交错布局）
// 条目         = uint16 pathLen + path(utf8) + uint64 size + 原始数据
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct Entry
{
    std::wstring path;
    unsigned long long size = 0;
    unsigned long long offset = 0;
};


// 解包过程的诊断日志：%TEMP%\CjibackupSetup.log（追加写，UTF-8）
void logLine(const std::wstring &text)
{
    wchar_t tempPath[MAX_PATH] = {0};
    GetTempPathW(MAX_PATH, tempPath);
    const std::wstring path = std::wstring(tempPath) + L"CjibackupSetup.log";

    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;

    SYSTEMTIME t = {};
    GetLocalTime(&t);
    wchar_t stamp[64] = {0};
    swprintf(stamp, 64, L"[%04d-%02d-%02d %02d:%02d:%02d] ", t.wYear, t.wMonth, t.wDay,
             t.wHour, t.wMinute, t.wSecond);

    const std::wstring line = std::wstring(stamp) + text + L"\r\n";
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, line.c_str(), int(line.size()),
                                          nullptr, 0, nullptr, nullptr);
    if (bytes > 0) {
        std::string utf8(size_t(bytes), char(0));
        WideCharToMultiByte(CP_UTF8, 0, line.c_str(), int(line.size()), &utf8[0], bytes,
                            nullptr, nullptr);
        DWORD written = 0;
        WriteFile(file, utf8.data(), DWORD(utf8.size()), &written, nullptr);
    }
    CloseHandle(file);
}

HINSTANCE g_instance = nullptr;
HWND g_window = nullptr;
int g_percent = 0;
const wchar_t *g_status = L"正在准备安装程序…";

HFONT makeFont(int size, bool bold)
{
    LOGFONTW lf = {};
    lf.lfHeight = -size;
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lstrcpynW(lf.lfFaceName, L"Microsoft YaHei UI", 32);
    return CreateFontIndirectW(&lf);
}

void paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);

    RECT client;
    GetClientRect(hwnd, &client);

    HBRUSH background = CreateSolidBrush(RGB(0x16, 0x1A, 0x23));
    FillRect(dc, &client, background);
    DeleteObject(background);

    RECT accent = {0, 0, client.right, 3};
    HBRUSH accentBrush = CreateSolidBrush(RGB(0x3B, 0x82, 0xF6));
    FillRect(dc, &accent, accentBrush);
    DeleteObject(accentBrush);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    HFONT titleFont = makeFont(20, true);
    HGDIOBJ old = SelectObject(dc, titleFont);
    RECT titleRect = {28, 28, client.right - 28, 62};
    DrawTextW(dc, L"拷备", -1, &titleRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SelectObject(dc, old);
    DeleteObject(titleFont);

    SetTextColor(dc, RGB(0x9A, 0xA6, 0xB8));
    HFONT bodyFont = makeFont(13, false);
    old = SelectObject(dc, bodyFont);
    RECT statusRect = {28, 70, client.right - 28, 96};
    DrawTextW(dc, g_status, -1, &statusRect,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, old);
    DeleteObject(bodyFont);

    RECT track = {28, 130, client.right - 28, 136};
    HBRUSH trackBrush = CreateSolidBrush(RGB(0x2A, 0x2F, 0x3A));
    FillRect(dc, &track, trackBrush);
    DeleteObject(trackBrush);

    RECT fill = track;
    fill.right = track.left + (track.right - track.left) * g_percent / 100;
    HBRUSH fillBrush = CreateSolidBrush(RGB(0x3B, 0x82, 0xF6));
    FillRect(dc, &fill, fillBrush);
    DeleteObject(fillBrush);

    SetTextColor(dc, RGB(0x6C, 0x76, 0x88));
    HFONT smallFont = makeFont(12, false);
    old = SelectObject(dc, smallFont);
    wchar_t buffer[64];
    swprintf(buffer, 64, L"%d%%", g_percent);
    RECT percentRect = {28, 146, client.right - 28, 168};
    DrawTextW(dc, buffer, -1, &percentRect, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    SelectObject(dc, old);
    DeleteObject(smallFont);

    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_PAINT:
        paint(hwnd);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void pumpMessages()
{
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void updateProgress(int percent, const wchar_t *status)
{
    g_percent = percent;
    if (status)
        g_status = status;
    if (g_window) {
        InvalidateRect(g_window, nullptr, FALSE);
        UpdateWindow(g_window);
    }
    pumpMessages();
}

std::wstring modulePath()
{
    wchar_t buffer[MAX_PATH * 2] = {0};
    GetModuleFileNameW(nullptr, buffer, MAX_PATH * 2);
    return buffer;
}

bool ensureDirectory(const std::wstring &path)
{
    if (path.empty())
        return true;
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY))
        return true;

    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos && slash > 2)
        ensureDirectory(path.substr(0, slash));

    return CreateDirectoryW(path.c_str(), nullptr) != FALSE
           || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool copyRange(HANDLE source, unsigned long long offset, unsigned long long size,
               const std::wstring &target, DWORD *errorOut)
{
    const size_t slash = target.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
        ensureDirectory(target.substr(0, slash));

    // 杀毒软件 / 系统索引偶尔会短暂占用刚创建的文件，重试几次基本能过
    for (int attempt = 0; attempt < 4; ++attempt) {
        HANDLE out = CreateFileW(target.c_str(), GENERIC_WRITE,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
        if (out == INVALID_HANDLE_VALUE) {
            const DWORD err = GetLastError();
            if (errorOut)
                *errorOut = err;
            logLine(L"打开目标失败 attempt=" + std::to_wstring(attempt) + L" err="
                    + std::to_wstring(err) + L" " + target);
            Sleep(150);
            continue;
        }

        LARGE_INTEGER position;
        position.QuadPart = static_cast<LONGLONG>(offset);
        SetFilePointerEx(source, position, nullptr, FILE_BEGIN);

        const DWORD kChunk = 1 << 20;
        std::vector<char> buffer(kChunk);
        unsigned long long remaining = size;
        DWORD savedError = 0;

        while (remaining > 0) {
            const DWORD toRead = static_cast<DWORD>(remaining < kChunk ? remaining : kChunk);
            DWORD got = 0;
            if (!ReadFile(source, buffer.data(), toRead, &got, nullptr) || got == 0) {
                savedError = GetLastError();
                break;
            }
            DWORD written = 0;
            if (!WriteFile(out, buffer.data(), got, &written, nullptr) || written != got) {
                savedError = GetLastError();
                break;
            }
            remaining -= got;
        }

        CloseHandle(out);

        if (remaining == 0)
            return true;

        if (errorOut)
            *errorOut = savedError;
        logLine(L"写文件失败 err=" + std::to_wstring(savedError) + L" " + target);
        DeleteFileW(target.c_str());
        Sleep(150);
    }
    return false;
}

void deleteTree(const std::wstring &dir)
{
    WIN32_FIND_DATAW data;
    const std::wstring pattern = dir + L"\\*";
    HANDLE finder = FindFirstFileW(pattern.c_str(), &data);
    if (finder != INVALID_HANDLE_VALUE) {
        do {
            const std::wstring name = data.cFileName;
            if (name == L"." || name == L"..")
                continue;
            const std::wstring full = dir + L"\\" + name;
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                deleteTree(full);
            else
                DeleteFileW(full.c_str());
        } while (FindNextFileW(finder, &data));
        FindClose(finder);
    }
    RemoveDirectoryW(dir.c_str());
}

std::wstring utf8ToWide(const std::vector<char> &bytes)
{
    if (bytes.empty())
        return std::wstring();
    const int wideLen = MultiByteToWideChar(CP_UTF8, 0, bytes.data(), int(bytes.size()),
                                            nullptr, 0);
    std::wstring result(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, bytes.data(), int(bytes.size()), &result[0], wideLen);
    return result;
}

std::wstring argumentSuffix()
{
    const std::wstring raw = GetCommandLineW();
    size_t pos = 0;
    if (!raw.empty() && raw[0] == L'"') {
        pos = raw.find(L'"', 1);
        pos = (pos == std::wstring::npos) ? raw.size() : pos + 1;
    } else {
        pos = raw.find(L' ');
        if (pos == std::wstring::npos)
            pos = raw.size();
    }
    return pos < raw.size() ? raw.substr(pos) : std::wstring();
}

} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int)
{
    g_instance = instance;

    const std::wstring exePath = modulePath();
    HANDLE self = CreateFileW(exePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, 0, nullptr);
    if (self == INVALID_HANDLE_VALUE) {
        MessageBoxW(nullptr, L"无法读取安装包文件。", L"拷备", MB_ICONERROR);
        return 1;
    }

    LARGE_INTEGER fileSize;
    GetFileSizeEx(self, &fileSize);
    if (fileSize.QuadPart < 32) {
        MessageBoxW(nullptr, L"安装包不完整，请重新下载。", L"拷备", MB_ICONERROR);
        CloseHandle(self);
        return 1;
    }

    unsigned long long payloadSize = 0;
    char magic[8] = {0};
    DWORD read = 0;
    LARGE_INTEGER position;
    position.QuadPart = fileSize.QuadPart - 16;
    SetFilePointerEx(self, position, nullptr, FILE_BEGIN);
    ReadFile(self, &payloadSize, 8, &read, nullptr);
    ReadFile(self, magic, 8, &read, nullptr);

    if (std::memcmp(magic, "CJIBOOT1", 8) != 0 || payloadSize == 0) {
        MessageBoxW(nullptr, L"安装包数据校验失败，请重新下载。", L"拷备", MB_ICONERROR);
        CloseHandle(self);
        return 1;
    }

    const unsigned long long payloadOffset = fileSize.QuadPart - 16 - payloadSize;

    char payloadMagic[8] = {0};
    position.QuadPart = static_cast<LONGLONG>(payloadOffset);
    SetFilePointerEx(self, position, nullptr, FILE_BEGIN);
    ReadFile(self, payloadMagic, 8, &read, nullptr);

    if (std::memcmp(payloadMagic, "CJIBT001", 8) != 0) {
        MessageBoxW(nullptr, L"安装包数据损坏，请重新下载。", L"拷备", MB_ICONERROR);
        CloseHandle(self);
        return 1;
    }

    unsigned int count = 0;
    ReadFile(self, &count, 4, &read, nullptr);
    if (count == 0 || count > 4096) {
        MessageBoxW(nullptr, L"安装包数据异常。", L"拷备", MB_ICONERROR);
        CloseHandle(self);
        return 1;
    }

    std::vector<Entry> entries(count);
    for (unsigned int i = 0; i < count; ++i) {
        unsigned short pathLen = 0;
        ReadFile(self, &pathLen, 2, &read, nullptr);
        if (pathLen == 0 || pathLen > 4096) {
            MessageBoxW(nullptr, L"安装包数据异常。", L"拷备", MB_ICONERROR);
            CloseHandle(self);
            return 1;
        }

        std::vector<char> utf8(pathLen);
        ReadFile(self, utf8.data(), pathLen, &read, nullptr);

        unsigned long long size = 0;
        ReadFile(self, &size, 8, &read, nullptr);

        LARGE_INTEGER cursor;
        LARGE_INTEGER zero;
        zero.QuadPart = 0;
        SetFilePointerEx(self, zero, &cursor, FILE_CURRENT);

        entries[i].path = utf8ToWide(utf8);
        entries[i].size = size;
        entries[i].offset = static_cast<unsigned long long>(cursor.QuadPart);

        LARGE_INTEGER skip;
        skip.QuadPart = static_cast<LONGLONG>(size);
        SetFilePointerEx(self, skip, nullptr, FILE_CURRENT);
    }

    // 引导窗口
    WNDCLASSW wc = {};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"CjibackupBootstrapWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    const int width = 460;
    const int height = 190;
    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);

    g_window = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"拷备 安装程序",
                               WS_POPUP, (screenW - width) / 2, (screenH - height) / 2,
                               width, height, nullptr, nullptr, instance, nullptr);
    ShowWindow(g_window, SW_SHOW);
    UpdateWindow(g_window);

    // 解包到临时目录：主目录不行就依次换（杀软拦截某个目录时还有退路）
    std::vector<std::wstring> tempRoots;
    {
        wchar_t sysTemp[MAX_PATH] = {0};
        GetTempPathW(MAX_PATH, sysTemp);
        tempRoots.push_back(sysTemp);

        wchar_t localAppData[MAX_PATH] = {0};
        if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH) > 0)
            tempRoots.push_back(std::wstring(localAppData) + L"\\Temp\\");

        const std::wstring exeDir = exePath.substr(0, exePath.find_last_of(L"\\/"));
        if (!exeDir.empty())
            tempRoots.push_back(exeDir + L"\\");
    }

    wchar_t status[512] = {0};
    std::wstring tempDir;
    std::wstring failedEntry;
    DWORD failedError = 0;
    bool ok = false;

    for (const std::wstring &root : tempRoots) {
        wchar_t candidate[MAX_PATH] = {0};
        swprintf(candidate, MAX_PATH, L"%lsCjibackupSetup_%u", root.c_str(),
                 (unsigned)GetCurrentProcessId());
        if (!ensureDirectory(candidate)) {
            logLine(L"建目录失败 " + std::wstring(candidate));
            continue;
        }

        const std::wstring dir = candidate;
        bool thisRootOk = true;
        for (unsigned int i = 0; i < count; ++i) {
            swprintf(status, 512, L"正在解压：%ls", entries[i].path.c_str());
            updateProgress(int((i + 1) * 95 / (count + 1)), status);

            const std::wstring target = dir + L"\\" + entries[i].path;
            DWORD err = 0;
            if (!copyRange(self, entries[i].offset, entries[i].size, target, &err)) {
                thisRootOk = false;
                failedEntry = entries[i].path;
                failedError = err;
                break;
            }
        }

        if (thisRootOk) {
            tempDir = dir;
            ok = true;
            break;
        }

        logLine(L"解包失败，换下一个临时目录 " + dir);
        deleteTree(candidate);
    }

    CloseHandle(self);

    if (!ok) {
        wchar_t detail[1024] = {0};
        wchar_t logPath[MAX_PATH] = {0};
        wchar_t tempPath[MAX_PATH] = {0};
        GetTempPathW(MAX_PATH, tempPath);
        lstrcpynW(logPath, (std::wstring(tempPath) + L"CjibackupSetup.log").c_str(), MAX_PATH);

        swprintf(detail, 1024,
                 L"解压安装文件失败（%ls，错误码 %lu）。\n\n"
                 L"可以把下面的日志发给作者，或者直接下载免安装版解压使用：\n%ls",
                 failedEntry.empty() ? L"未知文件" : failedEntry.c_str(),
                 (unsigned long)failedError, logPath);
        logLine(L"解压失败 entry=" + failedEntry + L" err=" + std::to_wstring(failedError));
        MessageBoxW(g_window, detail, L"拷备", MB_ICONERROR);
        deleteTree(tempDir);
        DestroyWindow(g_window);
        return 1;
    }

    updateProgress(97, L"正在启动安装程序…");

    const std::wstring installerPath = tempDir + L"\\CjibackupSetup.exe";
    std::wstring commandLine = L"\"" + installerPath + L"\"" + argumentSuffix();

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    if (!CreateProcessW(installerPath.c_str(), &commandLine[0], nullptr, nullptr, FALSE, 0,
                        nullptr, tempDir.c_str(), &si, &pi)) {
        wchar_t message[512];
        swprintf(message, 512, L"无法启动安装程序（错误码 %lu）。", GetLastError());
        MessageBoxW(g_window, message, L"拷备", MB_ICONERROR);
        deleteTree(tempDir);
        DestroyWindow(g_window);
        return 1;
    }

    ShowWindow(g_window, SW_HIDE);
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    // 调试用：CJI_BOOT_KEEP=1 时保留解包目录，方便排查
    if (GetEnvironmentVariableW(L"CJI_BOOT_KEEP", nullptr, 0) == 0)
        deleteTree(tempDir);
    DestroyWindow(g_window);
    return int(exitCode);
}
