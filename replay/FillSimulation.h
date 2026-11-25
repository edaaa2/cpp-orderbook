#pragma once

#include <algorithm>
#include <iostream>
#include <limits>
#include <optional>
#include <unordered_map>

#include "BookSide.h"

class FillSimulation {
    struct Fill {
        Quantity shares = 0;
        long long firstWait = 0;

        void apply(Quantity volume, Quantity size, long long wait) {
            if (shares == 0) firstWait = wait;
            shares += std::min(volume, size - shares);
        }
    };

    struct Trial {
        long long expires;
        long long arrival;
        long long price;
        std::unordered_map<OrderId, Quantity> ahead;
        Fill samePrice;
        Fill fifo;
    };

    long long latency;
    long long lifetime;
    Quantity size;
    std::optional<long long> pending;
    std::optional<Trial> active;

    static long long after(long long timestamp, long long delay) {
        return timestamp + std::min(delay, std::numeric_limits<long long>::max() - timestamp);
    }

public:
    FillSimulation(long long latency, long long lifetime, Quantity size)
        : latency(latency), lifetime(lifetime), size(size) {
        std::cout << "arrival_us,price,size,observed_us,status,same_price_shares,"
                     "same_price_first_wait_us,fifo_shares,fifo_first_wait_us\n";
    }

    void advance(long long timestamp, const BookSide<std::greater<long long>>& bids,
                 const BookSide<std::less<long long>>& asks) {
        if (active && active->expires <= timestamp) {
            report(active->expires, "expired");
        }
        if (!pending || *pending > timestamp) return;

        const auto arrival = *pending;
        pending.reset();
        if (bids.empty() || asks.empty() || bids.bestOrder().price >= asks.bestOrder().price) return;

        active.emplace(Trial{after(arrival, lifetime), arrival, bids.bestOrder().price, {}, {}, {}});
        bids.visitBestLevel([&](const Order& order) {
            active->ahead.emplace(order.orderId, order.quantity);
        });
        if (active->expires <= timestamp) report(active->expires, "expired");
    }

    void onQuote(long long timestamp, const BookSide<std::greater<long long>>& bids,
                 const BookSide<std::less<long long>>& asks) {
        if (active || pending || bids.empty() || asks.empty() ||
            bids.bestOrder().price >= asks.bestOrder().price) return;

        const auto bidSize = static_cast<long double>(bids.bestLevelQuantity());
        const auto askSize = static_cast<long double>(asks.bestLevelQuantity());
        // Imbalance >= 0.2 is equivalent to twice the bid size >= three times the ask size.
        if (2 * bidSize < 3 * askSize) return;
        pending = after(timestamp, latency);
        if (latency == 0) advance(timestamp, bids, asks);
    }

    void onReduction(long long timestamp, char action, const Order& resting, Quantity quantity) {
        if (!active || resting.side != Side::BUY || resting.price != active->price) return;

        auto& trial = *active;
        const auto wait = timestamp - trial.arrival;
        if (action == 'E') trial.samePrice.apply(quantity, size, wait);

        auto ahead = trial.ahead.find(resting.orderId);
        if (ahead != trial.ahead.end()) {
            ahead->second -= quantity;
            if (ahead->second == 0) trial.ahead.erase(ahead);
        } else if (action == 'E' && trial.ahead.empty()) {
            trial.fifo.apply(quantity, size, wait);
        }
    }

    void finish(long long timestamp) {
        if (active) report(timestamp, "end_of_feed");
    }

private:
    void report(long long timestamp, const char* status) {
        const auto& trial = *active;
        std::cout << trial.arrival << ',' << trial.price << ',' << size << ','
                  << timestamp - trial.arrival << ',' << status;
        for (const auto* fill : {&trial.samePrice, &trial.fifo}) {
            std::cout << ',' << fill->shares << ',';
            if (fill->shares > 0) std::cout << fill->firstWait;
        }
        std::cout << '\n';
        active.reset();
    }
};
