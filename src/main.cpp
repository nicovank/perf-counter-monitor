#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <argparse/argparse.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "perf.hpp"

namespace {
std::string humanReadable(std::uint64_t value) {
    if (value < 1'000) {
        return std::to_string(value);
    }

    if (value < 1'000'000) {
        return std::to_string(value / 1'000) + "K";
    }

    if (value < 1'000'000'000) {
        return std::to_string(value / 1'000'000) + "M";
    }

    if (value < 1'000'000'000'000) {
        return std::to_string(value / 1'000'000'000) + "G";
    }

    return std::to_string(value / 1'000'000'000'000) + "T";
}

std::uint64_t sum_over_dims(const std::vector<std::uint64_t>& data,
                            std::initializer_list<std::size_t> dims) {
    return std::accumulate(dims.begin(), dims.end(), std::uint64_t(0),
                           [&data](std::uint64_t sum, std::size_t dim) {
                               return sum + data[dim];
                           });
}

ftxui::Element
plot_single_value(const std::deque<std::vector<std::uint64_t>>& data,
                  std::initializer_list<std::size_t> dims,
                  std::uint64_t period_ms) {
    auto c = ftxui::Canvas(150, 100);

    const int mx = 10; // Start plotting from this x value.
    const int my = 8;  // Start plotting from this y value.

    const std::uint64_t max_value = std::accumulate(
        data.begin(), data.end(), std::uint64_t(0),
        [dims](std::uint64_t a, const std::vector<std::uint64_t>& b) {
            return std::max(a, sum_over_dims(b, dims));
        });

    std::vector<int> xs;
    std::vector<int> ys;

    for (std::size_t i = 0; i < data.size(); ++i) {
        const auto value = sum_over_dims(data[i], dims);

        const auto x = static_cast<int>(static_cast<double>(i)
                                            / static_cast<double>(data.size())
                                            * (c.width() - mx)
                                        + mx);
        const auto y = static_cast<int>(
            c.height()
            - (static_cast<double>(value) / static_cast<double>(max_value)
                   * (c.height() - my)
               + my));

        xs.push_back(x);
        ys.push_back(y);
    }

    c.DrawPointLine(xs[0], ys[0], xs[1], ys[1], ftxui::Color::Green);
    for (int i = 1; i < xs.size() - 1; ++i) {
        c.DrawPointLine(xs[i], ys[i], xs[i + 1], ys[i + 1],
                        ftxui::Color::Green);
    }

    c.DrawText(0, 0, humanReadable(max_value));
    c.DrawText(0, 95, "0");
    c.DrawText(10, 99,
               "-" + std::to_string(data.size() * period_ms / 1'000) + "s");
    c.DrawText(149, 99, "0");
    c.DrawBlockLine(10, 0, 10, 95, ftxui::Color::GrayDark);
    c.DrawBlockLine(10, 95, 150, 95, ftxui::Color::GrayDark);

    return canvas(std::move(c));
}

ftxui::Element plot_ratio(const std::deque<std::vector<std::uint64_t>>& data,
                          std::initializer_list<std::size_t> xDims,
                          std::initializer_list<std::size_t> yDims,
                          std::uint64_t period_ms) {
    auto c = ftxui::Canvas(150, 100);

    const int mx = 10; // Start plotting from this x value.
    const int my = 8;  // Start plotting from this y value.

    std::vector<int> xs;
    std::vector<int> ys;

    for (std::size_t i = 0; i < data.size(); ++i) {
        const auto numerator
            = static_cast<double>(sum_over_dims(data[i], xDims));
        const auto denominator
            = static_cast<double>(sum_over_dims(data[i], yDims));

        const auto value
            = denominator == 0 ? 0 : (numerator / denominator * 100.0);

        const auto x = static_cast<int>(static_cast<double>(i)
                                            / static_cast<double>(data.size())
                                            * (c.width() - mx)
                                        + mx);
        const auto y = static_cast<int>(
            c.height()
            - (static_cast<double>(value) / 100.0 * (c.height() - my) + my));

        xs.push_back(x);
        ys.push_back(y);
    }

    c.DrawPointLine(xs[0], ys[0], xs[1], ys[1], ftxui::Color::Green);
    for (int i = 1; i < xs.size() - 1; ++i) {
        c.DrawPointLine(xs[i], ys[i], xs[i + 1], ys[i + 1],
                        ftxui::Color::Green);
    }

    c.DrawText(0, 0, "100%");
    c.DrawText(0, 95, "0%");
    c.DrawText(10, 99,
               "-" + std::to_string(data.size() * period_ms / 1'000) + "s");
    c.DrawText(149, 99, "0");
    c.DrawBlockLine(10, 0, 10, 95, ftxui::Color::GrayDark);
    c.DrawBlockLine(10, 95, 150, 95, ftxui::Color::GrayDark);

    return canvas(std::move(c));
}

ftxui::Element plot(const std::string& event,
                    const std::deque<std::vector<std::uint64_t>>& data,
                    std::uint64_t period_ms) {
    if (event == "cpu-cycles") {
        return plot_single_value(data, {0}, period_ms);
    }

    if (event == "dtlb-miss-rate") {
        return ftxui::vbox({
            plot_ratio(data, {0, 1}, {2, 3}, period_ms) | ftxui::border,
            plot_single_value(data, {2, 3}, period_ms) | ftxui::border,
        });
    }

    std::unreachable();
}

int perf_event_open(struct perf_event_attr* attr, pid_t pid, int cpu,
                    int group_fd, unsigned long flags) {
    return static_cast<int>(
        syscall(SYS_perf_event_open, attr, pid, cpu, group_fd, flags));
}
} // namespace

int main(int argc, char** argv) {
    // Disable --version argument.
    auto program = argparse::ArgumentParser("monitor", "",
                                            argparse::default_arguments::help);

    program.add_description(
        "Monitor a given performance counter over time, machine-wide.\n"
        "Available arguments for the --event/-e options are:\n"
        " 1. cpu-cycles\n"
        " 2. dtlb-miss-rate");

    program.add_argument("-e", "--event")
        .required()
        .help(
            "the name of the performance event to monitor, see -h for options")
        .metavar("EVENT")
        .choices("cpu-cycles", "dtlb-miss-rate");

    program.add_argument("-p", "--period-ms")
        .default_value(std::uint64_t(100))
        .help("period at which to sample the event, in milliseconds")
        .metavar("N")
        .scan<'u', std::uint64_t>();
    program.add_argument("--buffer-size")
        .default_value(std::uint64_t(100))
        .help("size of the event history buffer")
        .metavar("N")
        .scan<'u', std::uint64_t>();

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(EXIT_FAILURE);
    }

    const auto event = program.get<std::string>("event");
    const auto period_ms = program.get<std::uint64_t>("period-ms");
    const auto bufferSize = program.get<std::uint64_t>("buffer-size");

    std::vector<std::pair<std::uint32_t, std::uint64_t>> perfEventDescriptors;
    if (event == "cpu-cycles") {
        perfEventDescriptors.emplace_back(PERF_TYPE_HARDWARE,
                                          PERF_COUNT_HW_CPU_CYCLES);
    } else if (event == "dtlb-miss-rate") {
        perfEventDescriptors.insert(
            perfEventDescriptors.end(),
            {{PERF_TYPE_HW_CACHE,
              PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_READ << 8)
                  | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16)},
             {PERF_TYPE_HW_CACHE,
              PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_WRITE << 8)
                  | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16)},
             {PERF_TYPE_HW_CACHE,
              PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_READ << 8)
                  | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16)},
             {PERF_TYPE_HW_CACHE,
              PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_WRITE << 8)
                  | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16)},
             {PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES},
             {PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES}});
    }

    perf::SystemWideGroup group(perfEventDescriptors);

    const auto nameByEvent = std::unordered_map<std::string, std::string>{
        {"cpu-cycles", "CPU cycles"},
        {"dtlb-miss-rate", "dTLB miss rate"},
    };

    std::cout << "Monitoring " << nameByEvent.at(event) << " every "
              << period_ms << " milliseconds. Press Ctrl+C or 'q' to stop."
              << std::endl;

    std::atomic_bool running = true;
    auto screen = ftxui::ScreenInteractive::FitComponent();
    auto history = std::deque<std::vector<std::uint64_t>>(
        bufferSize, std::vector<std::uint64_t>(perfEventDescriptors.size(), 0));
    ftxui::Component component = ftxui::Renderer(
        [&] { return plot(event, history, period_ms) | ftxui::border; });

    std::thread ui_thread([&]() {
        screen.Loop(component
                    | ftxui::CatchEvent([&](const ftxui::Event& event) {
                          if (event == ftxui::Event::CtrlC
                              || event == ftxui::Event::Character('q')) {
                              running = false;
                              return true;
                          }
                          return false;
                      }));
    });

    group.reset();
    group.enable();

    auto lastValues
        = std::vector<std::uint64_t>(perfEventDescriptors.size(), 0);
    auto deltas = std::vector<std::uint64_t>(perfEventDescriptors.size(), 0);
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(period_ms));

        auto values = group.read();
        for (std::size_t i = 0; i < values.size(); ++i) {
            deltas[i] = values[i] - lastValues[i];
            lastValues[i] = values[i];
        }

        history.pop_front();
        history.push_back(deltas);
        screen.PostEvent(ftxui::Event::Custom);
    }

    screen.Exit();
    ui_thread.join();
}
