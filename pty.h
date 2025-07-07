#pragma once

#include <Windows.h>
#include <functional>
#include <memory>
#include <string>

struct PtyReadBuffer {
  OVERLAPPED ovl{};
  HANDLE hPipe = INVALID_HANDLE_VALUE;
  std::unique_ptr<char[]> buffer;
  PTP_IO io = nullptr;
  PTP_TIMER timer = nullptr;
  std::function<void(const char*, DWORD)> onData;
};

constexpr auto BUFFER_SIZE = 16384;

class PseudoConsole {
 public:
  PseudoConsole(short num_rows, short num_columns);
  ~PseudoConsole();

  bool Start(std::function<void(const char*, DWORD)> onData);
  bool Send(const char* data, unsigned int length);
  void Resize(short numRows, short numColumns);
  void Close();

  static void CALLBACK ReadCompleteCallback(PTP_CALLBACK_INSTANCE,
                                          void* context,
                                          void*,
                                          ULONG ioResult,
                                          ULONG_PTR bytesTransferred,
                                          PTP_IO);

  static void CALLBACK RetryTimerCallback(PTP_CALLBACK_INSTANCE,
                                        void* context,
                                        PTP_TIMER);

 private:
  HANDLE m_hInput = INVALID_HANDLE_VALUE;
  HANDLE m_hOutput = INVALID_HANDLE_VALUE;
  HPCON m_hPseudoConsole = nullptr;
  PROCESS_INFORMATION m_processInfo{};
  std::unique_ptr<PtyReadBuffer> m_readBuffer;
  std::function<void(const char*, DWORD)> m_onData;

  short m_numRows = 24;
  short m_numColumns = 80;

  static void ScheduleRead(PtyReadBuffer* buf);
};
