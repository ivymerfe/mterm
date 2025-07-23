#include "PseudoConsole.h"

#define WIN32_LEAN_AND_MEAN
#include "Windows.h"

#include <assert.h>
#include <exception>
#include <memory>
#include <string>
#include <vector>

namespace MTerm {

class PseudoConsole::Impl {
 private:
  HANDLE m_hInput = INVALID_HANDLE_VALUE;
  HANDLE m_hOutput = INVALID_HANDLE_VALUE;
  HPCON m_hPseudoConsole = nullptr;
  PROCESS_INFORMATION m_processInfo{};

  struct PtyReadBuffer {
    OVERLAPPED ovl{};
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    std::unique_ptr<char[]> buffer;
    PTP_IO io = nullptr;
    PTP_TIMER timer = nullptr;
    std::function<void(const char*, DWORD)> onData;
  };

  std::unique_ptr<PtyReadBuffer> m_readBuffer = nullptr;

  short m_numRows = 24;
  short m_numColumns = 80;
  std::function<void(const char*, unsigned int)> m_onData;

 public:
  ~Impl() { Close(); }

  bool Start(short num_rows,
             short num_columns,
             std::function<void(const char*, unsigned int)> on_data_callback) {
    m_numRows = num_rows;
    m_numColumns = num_columns;
    m_onData = std::move(on_data_callback);

    // --- Создаём именованный пайп для вывода PTY (асинхронный) ---
    wchar_t pipeName[64];
    swprintf_s(pipeName, L"\\\\.\\pipe\\pty_out_%p", this);

    // Чтение из PTY (наш процесс читает)
    HANDLE hPipePTYOutRead =
        CreateNamedPipeW(pipeName, PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                         PIPE_TYPE_BYTE | PIPE_WAIT, 1, PTY_BUFFER_SIZE,
                         PTY_BUFFER_SIZE, 0, nullptr);
    assert(hPipePTYOutRead != INVALID_HANDLE_VALUE);

    // Запись в PTY (ConPTY пишет)
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE hPipePTYOutWrite =
        CreateFileW(pipeName, GENERIC_WRITE, 0, &sa, OPEN_EXISTING, 0, nullptr);
    assert(hPipePTYOutWrite != INVALID_HANDLE_VALUE);

    // --- Создаём обычный пайп для ввода PTY (можно без OVERLAPPED) ---
    HANDLE hPipePTYInRead = nullptr, hPipePTYInWrite = nullptr;
    BOOL ok = CreatePipe(&hPipePTYInRead, &hPipePTYInWrite, &sa, 0);
    assert(ok);

    HRESULT hr = CreatePseudoConsole({m_numColumns, m_numRows}, hPipePTYInRead,
                                     hPipePTYOutWrite, 0, &m_hPseudoConsole);
    assert(SUCCEEDED(hr));

    CloseHandle(hPipePTYInRead);
    CloseHandle(hPipePTYOutWrite);

    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(si);
    SIZE_T attrListSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrListSize);
    std::vector<char> attrList(attrListSize);
    si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)attrList.data();
    InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attrListSize);
    UpdateProcThreadAttribute(
        si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
        m_hPseudoConsole, sizeof(m_hPseudoConsole), nullptr, nullptr);

    std::wstring cmd = L"pwsh.exe -NoLogo";
    ok = CreateProcessW(
        nullptr, &cmd[0], nullptr, nullptr, FALSE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT, nullptr,
        nullptr, &si.StartupInfo, &m_processInfo);
    assert(ok);

    DeleteProcThreadAttributeList(si.lpAttributeList);

    m_hInput = hPipePTYInWrite;
    m_hOutput = hPipePTYOutRead;

    // --- Асинхронное чтение из PTY ---
    m_readBuffer = std::make_unique<PtyReadBuffer>();
    InitPtyRead(m_readBuffer.get(), m_hOutput, m_onData);
    ScheduleRead(m_readBuffer.get());

    return true;
  }

  bool Send(const char* data, unsigned int length) {
    DWORD written;
    return WriteFile(m_hInput, data, length, &written, nullptr);
  }

  void Resize(short num_rows, short num_columns) {
    if (num_rows != m_numRows || num_columns != m_numColumns) {
      if (FAILED(
              ResizePseudoConsole(m_hPseudoConsole, {num_columns, num_rows}))) {
        throw new std::exception();
      }
      m_numRows = num_rows;
      m_numColumns = num_columns;
    }
  }

  void Close() {
    if (m_readBuffer) {
      /*if (m_readBuffer->io)
        CloseThreadpoolIo(m_readBuffer->io);
      if (m_readBuffer->timer)
        CloseThreadpoolTimer(m_readBuffer->timer);*/
    }
    if (m_hInput != INVALID_HANDLE_VALUE)
      CloseHandle(m_hInput);
    if (m_hOutput != INVALID_HANDLE_VALUE)
      CloseHandle(m_hOutput);
    if (m_hPseudoConsole)
      ClosePseudoConsole(m_hPseudoConsole);
    if (m_processInfo.hProcess) {
      TerminateProcess(m_processInfo.hProcess, 0);
      CloseHandle(m_processInfo.hProcess);
    }
    if (m_processInfo.hThread)
      CloseHandle(m_processInfo.hThread);
  }

  static void CALLBACK ReadCompleteCallback(PTP_CALLBACK_INSTANCE,
                                            void* context,
                                            void* /*overlapped*/,
                                            ULONG ioResult,
                                            ULONG_PTR bytesTransferred,
                                            PTP_IO) {
    auto buf = static_cast<PtyReadBuffer*>(context);
    if (ioResult == ERROR_OPERATION_ABORTED) {
      CloseThreadpoolTimer(buf->timer);
      CloseThreadpoolIo(buf->io);
      return;
    }
    bool eof = (ioResult == ERROR_HANDLE_EOF || ioResult == ERROR_BROKEN_PIPE);
    if (bytesTransferred > 0) {
      buf->onData(buf->buffer.get(), (DWORD)bytesTransferred);
    }
    if (eof) {
      CloseThreadpoolTimer(buf->timer);
      CloseThreadpoolIo(buf->io);
    } else {
      ScheduleRead(buf);
    }
  }

  static void CALLBACK RetryTimerCallback(PTP_CALLBACK_INSTANCE,
                                          void* context,
                                          PTP_TIMER) {
    ScheduleRead(static_cast<PtyReadBuffer*>(context));
  }

  static void ScheduleRead(PtyReadBuffer* buf) {
    while (true) {
      StartThreadpoolIo(buf->io);
      BOOL ok = ReadFile(buf->hPipe, buf->buffer.get(), PTY_BUFFER_SIZE,
                         nullptr, &buf->ovl);
      if (ok || GetLastError() == ERROR_IO_PENDING) {
        // Успешно инициировано асинхронное чтение
        return;
      }
      DWORD err = GetLastError();
      CancelThreadpoolIo(buf->io);
      if (err != ERROR_INVALID_USER_BUFFER) {
        // Неизвестная ошибка — не повторяем
        return;
      }
      // Если буфер пользователя недоступен — повторяем попытку немедленно
      // (цикл while продолжится)
    }
  }

  static void InitPtyRead(PtyReadBuffer* buf,
                          HANDLE hPipe,
                          std::function<void(const char*, DWORD)> cb) {
    buf->hPipe = hPipe;
    buf->buffer = std::make_unique<char[]>(PTY_BUFFER_SIZE);
    buf->onData = std::move(cb);
    buf->io = CreateThreadpoolIo(hPipe, ReadCompleteCallback, buf, nullptr);
    buf->timer = CreateThreadpoolTimer(RetryTimerCallback, buf, nullptr);
  }
};

PseudoConsole::PseudoConsole() : m_impl(std::make_unique<Impl>()) {}

PseudoConsole::~PseudoConsole() {
}

bool PseudoConsole::Start(
    short num_rows,
    short num_columns,
    std::function<void(const char*, unsigned int)> on_data_callback) {
  return m_impl->Start(num_rows, num_columns, on_data_callback);
}

bool PseudoConsole::Send(const char* data, unsigned int length) {
  return m_impl->Send(data, length);
}

void PseudoConsole::Resize(short num_rows, short num_columns) {
  m_impl->Resize(num_rows, num_columns);
}

void PseudoConsole::Close() {
  m_impl->Close();
}

}  // namespace MTerm
