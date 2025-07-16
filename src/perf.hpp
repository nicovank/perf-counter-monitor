#ifndef PERF_COUNTER_MONITOR_PERF_HPP
#define PERF_COUNTER_MONITOR_PERF_HPP

#include <cstdint>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace perf {
std::string toString(std::uint32_t type, std::uint64_t config);
std::string toString(std::pair<std::uint32_t, std::uint64_t> event);

struct SystemWideGroup {
  SystemWideGroup(
      const std::vector<std::pair<std::uint32_t, std::uint64_t>> &events,
      int nCpus = std::thread::hardware_concurrency());
  ~SystemWideGroup();

  SystemWideGroup(const SystemWideGroup &) = delete;
  SystemWideGroup &operator=(const SystemWideGroup &) = delete;

  SystemWideGroup(SystemWideGroup &&) = default;
  SystemWideGroup &operator=(SystemWideGroup &&) = default;

  void enable();
  void disable();
  void reset();

  [[nodiscard]] bool isEnabled() const;

  [[nodiscard]] std::vector<std::uint64_t> read() const;

private:
  std::size_t nEvents;
  std::vector<int> descriptors;
  bool enabled = false;
};
} // namespace perf

#endif
