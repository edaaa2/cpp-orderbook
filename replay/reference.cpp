#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

struct Order {
    char side;
    long long price;
    long long quantity;
};

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: replay events.txt\n";
        return 1;
    }
    std::ifstream input(argv[1]);
    if (!input) {
        std::cerr << "Cannot open input file\n";
        return 1;
    }

    std::map<long long, long long> bids, asks;
    std::unordered_map<long long, Order> orders;
    long long last_timestamp = -1;
    std::string line;
    int line_number = 0;
    std::cout << "timestamp_us,bid,bid_qty,ask,ask_qty\n";

    try {
        while (std::getline(input, line)) {
            ++line_number;
            if (line.empty() || line.front() == '#') continue;

            std::istringstream event(line);
            long long timestamp, id;
            char action;
            if (!(event >> timestamp >> action >> id) ||
                timestamp < 0 || timestamp < last_timestamp || id <= 0) {
                throw std::runtime_error("invalid event header");
            }
            last_timestamp = timestamp;

            if (action == 'A') {
                Order order{};
                if (!(event >> order.side >> order.price >> order.quantity) ||
                    (order.side != 'B' && order.side != 'S') ||
                    order.price <= 0 || order.quantity <= 0 ||
                    !orders.emplace(id, order).second) {
                    throw std::runtime_error("invalid add");
                }
                auto& levels = order.side == 'B' ? bids : asks;
                levels[order.price] += order.quantity;
            } else if (action == 'C' || action == 'E') {
                // Both events subtract visible quantity from an existing order.
                long long quantity;
                auto found = orders.find(id);
                if (!(event >> quantity) || found == orders.end() ||
                    quantity <= 0 || quantity > found->second.quantity) {
                    throw std::runtime_error("invalid cancel or execution");
                }
                auto& order = found->second;
                auto& levels = order.side == 'B' ? bids : asks;
                auto level = levels.find(order.price);
                level->second -= quantity;
                if (level->second == 0) levels.erase(level);
                order.quantity -= quantity;
                if (order.quantity == 0) orders.erase(found);
            } else {
                throw std::runtime_error("unknown event type");
            }

            std::string extra;
            if (event >> extra) throw std::runtime_error("unexpected field");
            std::cout << timestamp << ',';
            if (bids.empty()) std::cout << ',';
            else std::cout << bids.rbegin()->first << ',' << bids.rbegin()->second;
            std::cout << ',';
            if (asks.empty()) std::cout << ',';
            else std::cout << asks.begin()->first << ',' << asks.begin()->second;
            std::cout << '\n';
        }
    } catch (const std::runtime_error& error) {
        std::cerr << "Line " << line_number << ": " << error.what() << '\n';
        return 1;
    }
}
