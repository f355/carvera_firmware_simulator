/*
 * This file is part of the Carvera Firmware Simulator.
 *
 * Copyright (c) 2026 Konstantin Tcepliaev <f355@f355.org>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "InterruptIn.h"
#include "PwmOut.h"
#include "sim/machine_simulator.hpp"
#include "us_ticker_api.h"

namespace {

unsigned int fired_id = 0;

void ticker_callback(uint32_t id) { fired_id = id; }

struct EdgeProbe {
  void on_rise() { ++rises; }

  int rises{0};
};

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

template <typename Predicate>
bool eventually(Predicate&& predicate, std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return predicate();
}

}  // namespace

int main() {
  sim::MachineSimulator simulator;

  simulator.advance_us(25);
  require(simulator.time_us() == 25, "advance_us() should move virtual time forward");
  require(us_ticker_read() == 25, "us_ticker_read() should read simulator virtual time");

  us_ticker_set_handler(ticker_callback);
  ticker_event_t event{};
  us_ticker_insert_event(&event, 30, 99);
  simulator.advance_us(4);
  require(fired_id == 0, "ticker event should not fire before its timestamp");
  simulator.advance_us(1);
  require(fired_id == 99, "ticker event should fire once due");

  simulator.set_gpio_input({1, 18}, true);
  require(simulator.gpio_level({1, 18}), "set_gpio_input() should drive the observed pin level");

  EdgeProbe edge_probe;
  mbed::InterruptIn irq(P2_6);
  irq.rise(&edge_probe, &EdgeProbe::on_rise);
  simulator.trigger_interrupt_rise({2, 6});
  require(edge_probe.rises == 1, "trigger_interrupt_rise() should call the registered InterruptIn rise callback");

  simulator.reset();
  require(simulator.time_us() == 0, "reset() should reset virtual time");
  require(!simulator.gpio_level({1, 18}), "reset() should clear GPIO state");
  simulator.trigger_interrupt_rise({2, 6});
  require(edge_probe.rises == 1, "reset() should clear InterruptIn callbacks");

  simulator.start_realtime();
  require(simulator.is_realtime(), "start_realtime() should enable wall-clock time");
  const auto realtime_start = simulator.time_us();
  require(eventually([&] { return simulator.time_us() > realtime_start; }, std::chrono::milliseconds(100)),
          "realtime mode should advance without manual ticks");

  fired_id = 0;
  us_ticker_set_handler(ticker_callback);
  ticker_event_t realtime_event{};
  us_ticker_insert_event(&realtime_event, us_ticker_read() + 1000, 123);
  const bool realtime_event_fired = eventually(
      [&] {
        simulator.poll();
        return fired_id == 123;
      },
      std::chrono::milliseconds(100));
  require(realtime_event_fired, "poll() should dispatch due realtime ticker events");

  simulator.pause_realtime();
  require(!simulator.is_realtime(), "pause_realtime() should return to manual mode");
  const auto paused_time = simulator.time_us();
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  require(simulator.time_us() == paused_time, "paused realtime clock should stop advancing");

  {
    sim::MachineSimulator first;
    first.advance_us(42);
    first.set_gpio_input({1, 18}, true);
    first.set_adc_channel_raw(2, 1234);
    first.add_step_dir_axis({2, 0}, {2, 1});
    mbed::PwmOut first_pwm(P2_5);
    first_pwm = 0.75F;
    EdgeProbe first_irq_probe;
    mbed::InterruptIn first_irq(P2_7);
    first_irq.rise(&first_irq_probe, &EdgeProbe::on_rise);

    {
      sim::MachineSimulator second;
      require(second.time_us() == 0, "nested simulator should start with its own clock");
      require(!second.gpio_level({1, 18}), "nested simulator should not inherit GPIO state");
      require(second.adc_channel_raw(2) == 0, "nested simulator should not inherit ADC samples");
      require(!mbed::PwmOut::state(P2_5).configured, "nested simulator should not inherit PWM state");
      require(sim::stepper_axes::count() == 0, "active stepper registry should belong to the nested simulator");
      mbed::InterruptIn::simulate_rise(P2_7);
      require(first_irq_probe.rises == 0, "nested simulator should not inherit InterruptIn callbacks");
      second.advance_us(7);
    }

    require(first.time_us() == 42, "destroying nested simulator should restore the previous active context");
    require(first.gpio_level({1, 18}), "previous active context should keep its GPIO state");
    require(first.adc_channel_raw(2) == 1234, "previous active context should keep ADC samples");
    require(mbed::PwmOut::state(P2_5).duty == 0.75F, "previous active context should keep PWM state");
    require(sim::stepper_axes::count() == 1, "previous active context should keep its stepper registry");
    mbed::InterruptIn::simulate_rise(P2_7);
    require(first_irq_probe.rises == 1, "previous active context should keep InterruptIn callbacks");
  }

  return 0;
}
