#pragma once

#include <functional>
#include <memory>

namespace Mterm {

class PseudoConsole {
 public:
  PseudoConsole();
  ~PseudoConsole();

  bool Start(short num_rows,
             short num_columns,
             std::function<void(const char*, unsigned int)> on_data_callback);
  bool Send(const char* data, unsigned int length);
  void Resize(short num_rows, short num_columns);
  void Close();

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace Mterm
