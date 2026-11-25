#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "BookSide.h"
#include "FillSimulation.h"

using Bids = BookSide<std::greater<long long>>;
using Asks = BookSide<std::less<long long>>;

struct Event {
    long long timestamp{};
    char action{};
    Order order{};
};

struct Quote {
    long long price{};
    Quantity size{};
};

template <typename Compare>
Quote bestQuote(const BookSide<Compare>& side) {
    if (side.empty()) return {};
    return {side.bestOrder().price, side.bestLevelQuantity()};
}

void writeQuote(Quote quote) {
    if (quote.price == 0) std::cout << ',';
    else std::cout << quote.price << ',' << quote.size;
}

Event parseEvent(const std::string& line, long long lastTimestamp) {
    std::istringstream input(line);
    Event event{};
    auto& order = event.order;
    if (!(input >> event.timestamp >> event.action >> order.orderId) ||
        event.timestamp < 0 || event.timestamp < lastTimestamp || order.orderId <= 0) {
        throw std::runtime_error("invalid event header");
    }

    if (event.action == 'A') {
        char side;
        if (!(input >> side >> order.price >> order.quantity) ||
            (side != 'B' && side != 'S') || order.price <= 0 || order.quantity <= 0) {
            throw std::runtime_error("invalid add");
        }
        order.side = side == 'B' ? Side::BUY : Side::SELL;
    } else if (event.action == 'C' || event.action == 'E') {
        if (!(input >> order.quantity) || order.quantity <= 0) {
            throw std::runtime_error("invalid cancel or execution");
        }
    } else {
        throw std::runtime_error("unknown event type");
    }

    std::string extra;
    if (input >> extra) throw std::runtime_error("unexpected field");
    return event;
}

void applyEvent(const Event& event, Bids& bids, Asks& asks, FillSimulation* simulation = nullptr) {
    const auto& order = event.order;
    if (event.action == 'A') {
        if (bids.findOrder(order.orderId) || asks.findOrder(order.orderId)) {
            throw std::runtime_error("invalid add");
        }
        if (order.side == Side::BUY) bids.addOrder(order);
        else asks.addOrder(order);
        return;
    }
    auto resting = bids.findOrder(order.orderId);
    if (!resting) resting = asks.findOrder(order.orderId);
    if (!resting || order.quantity > resting->quantity) {
        throw std::runtime_error("invalid cancel or execution");
    }
    if (resting->side == Side::BUY) bids.reduceOrder(order.orderId, order.quantity);
    else asks.reduceOrder(order.orderId, order.quantity);
    if (simulation) simulation->onReduction(event.timestamp, event.action, *resting, order.quantity);
}

void benchmark(const std::vector<Event>& events) {
    if (events.empty()) throw std::runtime_error("benchmark needs at least one event");

    constexpr int trials = 201;
    std::vector<double> durations;
    durations.reserve(trials);
    std::vector<std::pair<Quote, Quote>> quotes(events.size());
    std::uint64_t checksum = 0;
    for (int trial = -1; trial < trials; ++trial) {
        Bids bids;
        Asks asks;
        const auto start = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < events.size(); ++i) {
            applyEvent(events[i], bids, asks);
            quotes[i] = {bestQuote(bids), bestQuote(asks)};
        }
        const auto stop = std::chrono::steady_clock::now();
        if (trial >= 0) durations.push_back(std::chrono::duration<double>(stop - start).count());

        for (const auto& [bid, ask] : quotes) {
            for (auto value : {static_cast<std::uint64_t>(bid.price), static_cast<std::uint64_t>(bid.size),
                               static_cast<std::uint64_t>(ask.price), static_cast<std::uint64_t>(ask.size)}) {
                checksum = checksum * 31 + value;
            }
        }
    }

    std::sort(durations.begin(), durations.end());
    const double median = durations[durations.size() / 2];
    std::cout << "events,trials,median_us,events_per_second,checksum\n"
              << events.size() << ',' << trials << ',' << std::fixed << std::setprecision(2)
              << median * 1e6 << ',' << events.size() / median << ',' << checksum << '\n';
}

int main(int argc, char** argv) {
    const bool bench = argc == 3 && std::string(argv[2]) == "--benchmark";
    if (argc != 2 && !bench && (argc != 6 || std::string(argv[2]) != "--simulate")) {
        std::cerr << "Usage: MarketReplay events.txt [--benchmark | --simulate latency_us lifetime_us shares]\n";
        return 1;
    }
    std::ifstream input(argv[1]);
    if (!input) {
        std::cerr << "Cannot open input file\n";
        return 1;
    }

    Bids bids;
    Asks asks;
    std::vector<Event> events;
    long long last_timestamp = -1;
    std::string line;
    int line_number = 0;
    std::optional<FillSimulation> simulation;
    if (argc == 6) {
        long long settings[3]{};
        for (int i = 0; i < 3; ++i) {
            const std::string arg = argv[i + 3];
            const auto end = arg.data() + arg.size();
            const auto result = std::from_chars(arg.data(), end, settings[i]);
            if (result.ec != std::errc{} || result.ptr != end || settings[i] < 0 ||
                (i != 0 && settings[i] == 0)) {
                std::cerr << "Expected nonnegative latency, positive lifetime, and positive shares\n";
                return 1;
            }
        }
        simulation.emplace(settings[0], settings[1], settings[2]);
    } else if (!bench) {
        std::cout << "timestamp_us,bid,bid_qty,ask,ask_qty\n";
    }

    try {
        while (std::getline(input, line)) {
            ++line_number;
            if (line.empty() || line.front() == '#') continue;

            const auto event = parseEvent(line, last_timestamp);
            last_timestamp = event.timestamp;
            if (bench) {
                events.push_back(event);
                continue;
            }

            const auto timestamp = event.timestamp;
            if (simulation) simulation->advance(timestamp, bids, asks);
            applyEvent(event, bids, asks, simulation ? &*simulation : nullptr);
            if (simulation) {
                simulation->onQuote(timestamp, bids, asks);
                continue;
            }
            std::cout << timestamp << ',';
            writeQuote(bestQuote(bids));
            std::cout << ',';
            writeQuote(bestQuote(asks));
            std::cout << '\n';
        }
        if (simulation) simulation->finish(last_timestamp);
        if (bench) benchmark(events);
    } catch (const std::runtime_error& error) {
        std::cerr << "Line " << line_number << ": " << error.what() << '\n';
        return 1;
    }
}
