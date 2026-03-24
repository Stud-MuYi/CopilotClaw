#include <chrono>
#include <cctype>
#include <string>

#if !defined(_WIN32) || !defined(_WIN64)
#error "CopilotClaw supports only Windows 11 x86_64."
#endif

#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <ws2tcpip.h>

namespace {

constexpr unsigned short kGatewayPort = 18789;
constexpr DWORD kStatusCommandTimeoutMs = 2500;
constexpr DWORD kGatewayStartTimeoutMs = 8000;
constexpr DWORD kGatewayStopTimeoutMs = 5000;
constexpr auto kGatewayPollInterval = std::chrono::milliseconds(150);
constexpr wchar_t kInstanceMutexName[] = L"Local\\CopilotClaw.OpenClawToggle";
constexpr wchar_t kGatewayControlUrl[] = L"http://127.0.0.1:18789/";
constexpr wchar_t kGatewayStatusCommand[] =
    L"cmd.exe /d /s /c \"openclaw gateway status --no-color\"";
constexpr wchar_t kGatewayStartCommand[] =
    L"cmd.exe /d /s /c \"openclaw gateway start\"";
constexpr wchar_t kGatewayStopCommand[] =
    L"cmd.exe /d /s /c \"openclaw gateway stop\"";
constexpr wchar_t kGatewayStatusFallbackCommand[] =
    L"powershell.exe -NonInteractive -WindowStyle Hidden -Command \"openclaw gateway status --no-color\"";
constexpr wchar_t kGatewayStartFallbackCommand[] =
    L"powershell.exe -NonInteractive -WindowStyle Hidden -Command \"openclaw gateway start\"";
constexpr wchar_t kGatewayStopFallbackCommand[] =
    L"powershell.exe -NonInteractive -WindowStyle Hidden -Command \"openclaw gateway stop\"";

class WinsockRuntime {
public:
    WinsockRuntime() = default;

    ~WinsockRuntime() {
        if (initialized_) {
            WSACleanup();
        }
    }

    WinsockRuntime(const WinsockRuntime&) = delete;
    WinsockRuntime& operator=(const WinsockRuntime&) = delete;

    auto Initialize() -> bool {
        if (initialized_) {
            return true;
        }

        WSADATA data{};
        initialized_ = WSAStartup(MAKEWORD(2, 2), &data) == 0;
        return initialized_;
    }

private:
    bool initialized_ = false;
};

class ScopedSocket {
public:
    explicit ScopedSocket(SOCKET socket) : socket_(socket) {}

    ~ScopedSocket() {
        if (socket_ != INVALID_SOCKET) {
            closesocket(socket_);
        }
    }

    ScopedSocket(const ScopedSocket&) = delete;
    ScopedSocket& operator=(const ScopedSocket&) = delete;

    auto Get() const -> SOCKET {
        return socket_;
    }

private:
    SOCKET socket_ = INVALID_SOCKET;
};

class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE handle = nullptr) : handle_(handle) {}

    ~ScopedHandle() {
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
    }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    auto Get() const -> HANDLE {
        return handle_;
    }

    auto Reset(HANDLE handle = nullptr) -> void {
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
        handle_ = handle;
    }

private:
    HANDLE handle_ = nullptr;
};

auto IsWindows11OrLater() -> bool {
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);

    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr) {
        return false;
    }

    const auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
    if (rtlGetVersion == nullptr) {
        return false;
    }

    OSVERSIONINFOW versionInfo{};
    versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
    if (rtlGetVersion(&versionInfo) != 0) {
        return false;
    }

    return versionInfo.dwMajorVersion > 10 ||
           (versionInfo.dwMajorVersion == 10 && versionInfo.dwBuildNumber >= 22000);
}

auto IsGatewayResponsive() -> bool {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(kGatewayPort);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    ScopedSocket socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (socket.Get() == INVALID_SOCKET) {
        return false;
    }

    u_long nonBlocking = 1;
    if (ioctlsocket(socket.Get(), FIONBIO, &nonBlocking) != 0) {
        return false;
    }

    const int connectResult = connect(
        socket.Get(),
        reinterpret_cast<const sockaddr*>(&address),
        static_cast<int>(sizeof(address))
    );
    if (connectResult == 0) {
        return true;
    }

    const int lastError = WSAGetLastError();
    if (lastError != WSAEWOULDBLOCK && lastError != WSAEINPROGRESS && lastError != WSAEINVAL) {
        return false;
    }

    fd_set writeSet;
    FD_ZERO(&writeSet);
    FD_SET(socket.Get(), &writeSet);

    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 250000;

    const int selectResult = select(0, nullptr, &writeSet, nullptr, &timeout);
    if (selectResult <= 0) {
        return false;
    }

    int socketError = 0;
    int optionLength = sizeof(socketError);
    if (getsockopt(socket.Get(), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&socketError), &optionLength) != 0) {
        return false;
    }

    return socketError == 0;
}

auto WaitForGatewayState(bool shouldBeResponsive, DWORD timeoutMs) -> bool {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (IsGatewayResponsive() == shouldBeResponsive) {
            return true;
        }
        Sleep(static_cast<DWORD>(kGatewayPollInterval.count()));
    }
    return IsGatewayResponsive() == shouldBeResponsive;
}

auto ToLowerAscii(std::string text) -> std::string {
    for (char& ch : text) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return text;
}

struct ProcessRunResult {
    bool launched = false;
    bool exited = false;
    DWORD exitCode = 1;
    std::string output;
};

auto ReadPipeToString(HANDLE pipeHandle) -> std::string {
    std::string output;
    char buffer[4096];
    DWORD bytesRead = 0;
    while (ReadFile(pipeHandle, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
        output.append(buffer, buffer + bytesRead);
    }
    return output;
}

auto RunHiddenProcess(const wchar_t* commandLineText, DWORD timeoutMs, bool captureOutput) -> ProcessRunResult {
    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION processInfo{};
    std::wstring commandLine(commandLineText);

    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;

    ScopedHandle readPipe;
    ScopedHandle writePipe;
    ScopedHandle nullInput;

    if (captureOutput) {
        HANDLE readHandle = nullptr;
        HANDLE writeHandle = nullptr;
        if (!CreatePipe(&readHandle, &writeHandle, &securityAttributes, 0)) {
            return {};
        }

        readPipe.Reset(readHandle);
        writePipe.Reset(writeHandle);

        const HANDLE nullHandle = CreateFileW(
            L"NUL",
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            &securityAttributes,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        if (nullHandle == INVALID_HANDLE_VALUE) {
            return {};
        }

        nullInput.Reset(nullHandle);
        SetHandleInformation(readPipe.Get(), HANDLE_FLAG_INHERIT, 0);
        startupInfo.dwFlags |= STARTF_USESTDHANDLES;
        startupInfo.hStdOutput = writePipe.Get();
        startupInfo.hStdError = writePipe.Get();
        startupInfo.hStdInput = nullInput.Get();
    }

    const BOOL created = CreateProcessW(
        nullptr,
        commandLine.data(),
        nullptr,
        nullptr,
        captureOutput ? TRUE : FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo
    );

    if (!created) {
        return {};
    }

    ScopedHandle processHandle(processInfo.hProcess);
    ScopedHandle threadHandle(processInfo.hThread);

    if (captureOutput) {
        writePipe.Reset();
    }

    ProcessRunResult result;
    result.launched = true;

    const DWORD waitResult = WaitForSingleObject(processHandle.Get(), timeoutMs);
    if (waitResult == WAIT_OBJECT_0) {
        result.exited = true;
    } else {
        TerminateProcess(processHandle.Get(), 1);
        WaitForSingleObject(processHandle.Get(), 1000);
    }

    if (captureOutput && readPipe.Get() != nullptr) {
        result.output = ReadPipeToString(readPipe.Get());
    }

    GetExitCodeProcess(processHandle.Get(), &result.exitCode);
    return result;
}

auto RunHiddenProcessAndWait(const wchar_t* commandLineText, DWORD timeoutMs) -> bool {
    const auto result = RunHiddenProcess(commandLineText, timeoutMs, false);
    return result.launched && result.exited && result.exitCode == 0;
}

auto RunGatewayStatusCommand() -> ProcessRunResult {
    auto result = RunHiddenProcess(kGatewayStatusCommand, kStatusCommandTimeoutMs, true);
    if (result.launched && result.exited && result.exitCode == 0) {
        return result;
    }

    return RunHiddenProcess(kGatewayStatusFallbackCommand, kStatusCommandTimeoutMs, true);
}

auto RunGatewayCommandAndWait(
    const wchar_t* commandLineText,
    const wchar_t* fallbackCommandLineText,
    DWORD timeoutMs
) -> bool {
    if (RunHiddenProcessAndWait(commandLineText, timeoutMs)) {
        return true;
    }

    return RunHiddenProcessAndWait(fallbackCommandLineText, timeoutMs);
}

auto OpenGatewayControlPage() -> void {
    const auto openResult = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"open", kGatewayControlUrl, nullptr, nullptr, SW_SHOWNORMAL)
    );
    (void)openResult;
}

struct GatewayState {
    bool runtimeRunning = false;
    bool rpcProbeOk = false;
    bool listeningHint = false;
    bool portResponsive = false;
};

auto QueryGatewayState() -> GatewayState {
    GatewayState state;
    const auto statusResult = RunGatewayStatusCommand();
    if (statusResult.launched) {
        const std::string normalized = ToLowerAscii(statusResult.output);
        state.runtimeRunning = normalized.find("runtime: running") != std::string::npos;
        state.rpcProbeOk = normalized.find("rpc probe: ok") != std::string::npos;
        state.listeningHint =
            normalized.find("listening:") != std::string::npos && normalized.find("18789") != std::string::npos;
    }

    state.portResponsive = IsGatewayResponsive();
    return state;
}

auto ShouldStopGateway() -> bool {
    const auto state = QueryGatewayState();
    return state.portResponsive || state.runtimeRunning || state.rpcProbeOk || state.listeningHint;
}

auto TryEnterSingleInstance() -> bool {
    const HANDLE mutex = CreateMutexW(nullptr, TRUE, kInstanceMutexName);
    if (mutex == nullptr) {
        return false;
    }

    static ScopedHandle instanceMutex(mutex);
    return GetLastError() != ERROR_ALREADY_EXISTS;
}

auto ToggleGateway() -> int {
    if (ShouldStopGateway()) {
        if (!RunGatewayCommandAndWait(kGatewayStopCommand, kGatewayStopFallbackCommand, kGatewayStopTimeoutMs)) {
            return 1;
        }

        return WaitForGatewayState(false, kGatewayStopTimeoutMs) ? 0 : 1;
    }

    if (!RunGatewayCommandAndWait(kGatewayStartCommand, kGatewayStartFallbackCommand, kGatewayStartTimeoutMs)) {
        return 1;
    }

    if (!WaitForGatewayState(true, kGatewayStartTimeoutMs)) {
        return 1;
    }

    OpenGatewayControlPage();
    return 0;
}

}  // namespace

auto WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) -> int {
    if (!IsWindows11OrLater()) {
        return 1;
    }

    if (!TryEnterSingleInstance()) {
        return 0;
    }

    WinsockRuntime winsockRuntime;
    if (!winsockRuntime.Initialize()) {
        return 1;
    }

    return ToggleGateway();
}
