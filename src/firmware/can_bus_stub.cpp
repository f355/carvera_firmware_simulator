/* Host-side transport for the Z1 LPC CANopen module. */
#include "CANBus.h"

CANBus::CANBus(PinName rd, PinName td, RxFilter filter, void* context)
    : can(rd, td),
      controller(nullptr),
      rx_filter(filter),
      rx_filter_context(context),
      rx_head(0),
      rx_tail(0),
      rx_count(0),
      rx_overflow_count(0),
      tx_count(0),
      tx_failed_count(0),
      tx_timeout_count(0),
      ready(false) {}

CANBus::~CANBus() { stop(); }
bool CANBus::start(uint32_t bitrate) {
  ready = bitrate > 0 && bitrate <= 1000000;
  return ready;
}
void CANBus::stop() { ready = false; }
bool CANBus::send(const mbed::CANMessage& message, uint32_t) {
  if (!ready || message.len > 8) {
    ++tx_failed_count;
    return false;
  }
  ++tx_count;
  return true;
}
bool CANBus::read(mbed::CANMessage&) { return false; }
CANBus::Statistics CANBus::statistics() {
  return {tx_count, rx_count, rx_overflow_count, tx_failed_count, tx_timeout_count, 0, 0};
}
void CANBus::on_receive() {}
bool CANBus::abort_tx(uint32_t) { return true; }
bool CANBus::wait_for_tx_idle(uint32_t) const { return true; }
bool CANBus::wait_for_tx_complete(uint32_t, bool& completed) const {
  completed = true;
  return true;
}
