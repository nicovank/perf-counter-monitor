#include "perf.hpp"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

namespace {
int perf_event_open(struct perf_event_attr* attr, pid_t pid, int cpu,
                    int group_fd, unsigned long flags) {
    return static_cast<int>(
        syscall(SYS_perf_event_open, attr, pid, cpu, group_fd, flags));
}
} // namespace

std::string perf::toString(std::uint32_t type, std::uint64_t config) {
    if (type == PERF_TYPE_HARDWARE) {
        switch (config) { // NOLINT(bugprone-switch-missing-default-case)
            case PERF_COUNT_HW_CPU_CYCLES:
                return "PERF_COUNT_HW_CPU_CYCLES";
            case PERF_COUNT_HW_INSTRUCTIONS:
                return "PERF_COUNT_HW_INSTRUCTIONS";
            case PERF_COUNT_HW_CACHE_REFERENCES:
                return "PERF_COUNT_HW_CACHE_REFERENCES";
            case PERF_COUNT_HW_CACHE_MISSES:
                return "PERF_COUNT_HW_CACHE_MISSES";
            case PERF_COUNT_HW_BRANCH_INSTRUCTIONS:
                return "PERF_COUNT_HW_BRANCH_INSTRUCTIONS";
            case PERF_COUNT_HW_BRANCH_MISSES:
                return "PERF_COUNT_HW_BRANCH_MISSES";
            case PERF_COUNT_HW_BUS_CYCLES:
                return "PERF_COUNT_HW_BUS_CYCLES";
            case PERF_COUNT_HW_STALLED_CYCLES_FRONTEND:
                return "PERF_COUNT_HW_STALLED_CYCLES_FRONTEND";
            case PERF_COUNT_HW_STALLED_CYCLES_BACKEND:
                return "PERF_COUNT_HW_STALLED_CYCLES_BACKEND";
            case PERF_COUNT_HW_REF_CPU_CYCLES:
                return "PERF_COUNT_HW_REF_CPU_CYCLES";
        }
    } else if (type == PERF_TYPE_SOFTWARE) {
        switch (config) { // NOLINT(bugprone-switch-missing-default-case)
            case PERF_COUNT_SW_CPU_CLOCK:
                return "PERF_COUNT_SW_CPU_CLOCK";
            case PERF_COUNT_SW_TASK_CLOCK:
                return "PERF_COUNT_SW_TASK_CLOCK";
            case PERF_COUNT_SW_PAGE_FAULTS:
                return "PERF_COUNT_SW_PAGE_FAULTS";
            case PERF_COUNT_SW_CONTEXT_SWITCHES:
                return "PERF_COUNT_SW_CONTEXT_SWITCHES";
            case PERF_COUNT_SW_CPU_MIGRATIONS:
                return "PERF_COUNT_SW_CPU_MIGRATIONS";
            case PERF_COUNT_SW_PAGE_FAULTS_MIN:
                return "PERF_COUNT_SW_PAGE_FAULTS_MIN";
            case PERF_COUNT_SW_PAGE_FAULTS_MAJ:
                return "PERF_COUNT_SW_PAGE_FAULTS_MAJ";
            case PERF_COUNT_SW_ALIGNMENT_FAULTS:
                return "PERF_COUNT_SW_ALIGNMENT_FAULTS";
            case PERF_COUNT_SW_EMULATION_FAULTS:
                return "PERF_COUNT_SW_EMULATION_FAULTS";
            case PERF_COUNT_SW_DUMMY:
                return "PERF_COUNT_SW_DUMMY";
            case PERF_COUNT_SW_BPF_OUTPUT:
                return "PERF_COUNT_SW_BPF_OUTPUT";
            case PERF_COUNT_SW_CGROUP_SWITCHES:
                return "PERF_COUNT_SW_CGROUP_SWITCHES";
        }
    } else if (type == PERF_TYPE_HW_CACHE) {
        std::string representation;

        switch (config & 0xFF) { // NOLINT(bugprone-switch-missing-default-case)
            case PERF_COUNT_HW_CACHE_L1D:
                representation = "PERF_COUNT_HW_CACHE_L1D";
                break;
            case PERF_COUNT_HW_CACHE_L1I:
                representation = "PERF_COUNT_HW_CACHE_L1I";
                break;
            case PERF_COUNT_HW_CACHE_LL:
                representation = "PERF_COUNT_HW_CACHE_LL";
                break;
            case PERF_COUNT_HW_CACHE_DTLB:
                representation = "PERF_COUNT_HW_CACHE_DTLB";
                break;
            case PERF_COUNT_HW_CACHE_ITLB:
                representation = "PERF_COUNT_HW_CACHE_ITLB";
                break;
            case PERF_COUNT_HW_CACHE_BPU:
                representation = "PERF_COUNT_HW_CACHE_BPU";
                break;
            case PERF_COUNT_HW_CACHE_NODE:
                representation = "PERF_COUNT_HW_CACHE_NODE";
                break;
        }

        switch ((config >> 8) // NOLINT(bugprone-switch-missing-default-case)
                & 0xFF) {
            case PERF_COUNT_HW_CACHE_OP_READ:
                representation += " | PERF_COUNT_HW_CACHE_OP_READ";
                break;
            case PERF_COUNT_HW_CACHE_OP_WRITE:
                representation += " | PERF_COUNT_HW_CACHE_OP_WRITE";
                break;
            case PERF_COUNT_HW_CACHE_OP_PREFETCH:
                representation += " | PERF_COUNT_HW_CACHE_OP_PREFETCH";
                break;
        }

        switch ((config >> 16) // NOLINT(bugprone-switch-missing-default-case)
                & 0xFF) {
            case PERF_COUNT_HW_CACHE_RESULT_ACCESS:
                representation += " | PERF_COUNT_HW_CACHE_RESULT_ACCESS";
                break;
            case PERF_COUNT_HW_CACHE_RESULT_MISS:
                representation += " | PERF_COUNT_HW_CACHE_RESULT_MISS";
                break;
        }

        return representation;
    }

    return "[unknown]";
}

std::string perf::toString(std::pair<std::uint32_t, std::uint64_t> event) {
    return toString(event.first, event.second);
}

perf::SystemWideGroup::SystemWideGroup(
    const std::vector<std::pair<std::uint32_t, std::uint64_t>>& events,
    int nCpus)
    : nEvents(events.size()) {
    assert(!events.empty());

    for (int i = 0; i < nCpus; ++i) {
        for (const auto& event : events) {
            struct perf_event_attr pe;
            std::memset(&pe, 0, sizeof(struct perf_event_attr));
            pe.size = sizeof(struct perf_event_attr);
            pe.type = event.first;
            pe.config = event.second;

            const auto fd = perf_event_open(&pe, -1, i, -1, 0);
            if (fd == -1) {
                std::cerr << "perf_event_open failed for event "
                          << perf::toString(pe.type, pe.config) << " on core "
                          << i << std::endl;
                std::exit(EXIT_FAILURE);
            }
            descriptors.push_back(fd);
        }
    }
}

perf::SystemWideGroup::~SystemWideGroup() {
    for (const auto fd : descriptors) {
        close(fd);
    }
}

void perf::SystemWideGroup::enable() {
    for (const auto fd : descriptors) {
        if (ioctl(fd, PERF_EVENT_IOC_ENABLE, 0) != 0) {
            std::cerr << "ioctl(ENABLE) failed" << std::endl;
            std::exit(EXIT_FAILURE);
        }
    }
    enabled = true;
}

void perf::SystemWideGroup::disable() {
    for (const auto fd : descriptors) {
        if (ioctl(fd, PERF_EVENT_IOC_DISABLE, 0) != 0) {
            std::cerr << "ioctl(DISABLE) failed" << std::endl;
            std::exit(EXIT_FAILURE);
        }
    }
    enabled = false;
}

void perf::SystemWideGroup::reset() {
    for (const auto fd : descriptors) {
        if (ioctl(fd, PERF_EVENT_IOC_RESET, 0) != 0) {
            std::cerr << "ioctl(RESET) failed" << std::endl;
            std::exit(EXIT_FAILURE);
        }
    }
}

bool perf::SystemWideGroup::isEnabled() const {
    return enabled;
}

std::vector<std::uint64_t> perf::SystemWideGroup::read() const {
    // Note: The reads in this function are not atomic. This is fine because we
    // are just trying to get a general idea of what's going on, not taking
    // fine-grained measurements.
    std::vector<std::uint64_t> values(nEvents, 0);

    for (std::size_t i = 0; i < descriptors.size(); ++i) {
        std::uint64_t value;
        if (::read(descriptors[i], &value, sizeof(std::uint64_t))
            != sizeof(std::uint64_t)) {
            std::cerr << "read failed" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        values[i % nEvents] += value;
    }

    return values;
}
