/* Host-side mbed CAN surface used by the Z1 LPC firmware build. */
#pragma once

#include <cstdint>
#include <deque>
#include <functional>

#include "PinNames.h"

typedef struct {
  volatile std::uint32_t CMR{0};
  volatile std::uint32_t SR{0};
} LPC_CAN_TypeDef;

enum CANType { CANData = 0, CANRemote = 1 };
enum CANFormat { CANStandard = 0, CANExtended = 1 };

namespace mbed {

struct CANMessage {
  unsigned int id{0};
  unsigned char data[8]{};
  unsigned char len{8};
  CANType type{CANData};
  CANFormat format{CANStandard};

  CANMessage() = default;
  CANMessage(unsigned int message_id, const char* bytes, unsigned char length = 8, CANType message_type = CANData,
             CANFormat message_format = CANStandard)
      : id(message_id), len(length), type(message_type), format(message_format) {
    for (unsigned char i = 0; i < len && i < 8; ++i) data[i] = static_cast<unsigned char>(bytes[i]);
  }
};

class CAN {
 public:
  CAN(PinName, PinName) {}
  void reset() { rx_.clear(); }
  int frequency(int) { return 1; }
  int write(const CANMessage&) { return 1; }
  int read(CANMessage& message) {
    if (rx_.empty()) return 0;
    message = rx_.front();
    rx_.pop_front();
    return 1;
  }
  void attach(void (*callback)(void)) { callback_ = callback == nullptr ? std::function<void()>{} : callback; }
  template <typename T>
  void attach(T* object, void (T::*method)()) {
    callback_ = object == nullptr || method == nullptr ? std::function<void()>{}
                                                       : std::function<void()>{[object, method] { (object->*method)(); }};
  }
  unsigned char tderror() const { return 0; }
  unsigned char rderror() const { return 0; }

 private:
  std::deque<CANMessage> rx_;
  std::function<void()> callback_;
};

}  // namespace mbed
