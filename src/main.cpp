#include <iostream>
#include <functional>
#include <cassert>
#include <memory>
#include <utility>
#include <chrono>
#include <random>
#include <vector>
#include <string>

#include "Side.h"
#include "Order.h"
#include "Trade.h"
#include "BookSide.h"
#include "MatchingEngine.h"
#include "TradeListener.h"

void testDoubleCancel() {
    MatchingEngine engine;

    engine.addOrder(Order{201, 100, 30, Side::BUY});

    // 第一次撤单：应该成功
    assert(engine.cancelOrder(201) == true);

    // 第二次撤单：订单已经不存在
    assert(engine.cancelOrder(201) == false);

    // 查询应该找不到
    assert(!engine.findOrder(201).has_value());

    std::cout << "Double Cancel Test Passed!\n";
}

void testCancelMiddleOrder() {
    MatchingEngine engine;

    engine.addOrder(Order{301, 100, 10, Side::BUY});
    engine.addOrder(Order{302, 100, 20, Side::BUY});
    engine.addOrder(Order{303, 100, 30, Side::BUY});

    // 撤销中间订单
    assert(engine.cancelOrder(302));

    // 302 已经不存在
    assert(!engine.findOrder(302).has_value());

    // 301 和 303 仍然存在
    assert(engine.findOrder(301).has_value());
    assert(engine.findOrder(303).has_value());

    // 301 仍然排在最前面
    assert(engine.getBids().bestOrder().orderId == 301);

    // 用卖单验证剩余订单的撮合顺序
    auto reports = engine.addOrder(
        Order{304, 100, 40, Side::SELL}
    );

    assert(reports.size() == 2);
    assert(reports[0].buyOrderId == 301);
    assert(reports[0].quantity == 10);
    assert(reports[1].buyOrderId == 303);
    assert(reports[1].quantity == 30);

    std::cout << "Cancel Middle Order Test Passed!\n";
}

void testPartialFillRemaining() {
    MatchingEngine engine;

    engine.addOrder(Order{401, 100, 50, Side::BUY});

    auto reports = engine.addOrder(
        Order{402, 100, 20, Side::SELL}
    );

    // 只产生一笔成交
    assert(reports.size() == 1);
    assert(reports[0].quantity == 20);
    assert(reports[0].price == 100);

    // 买单剩余 30 股
    auto buy = engine.findOrder(401);
    assert(buy.has_value());
    assert(buy->quantity == 30);

    // 卖单完全成交，不应该挂单
    assert(!engine.findOrder(402).has_value());

    // 订单簿状态
    assert(!engine.getBids().empty());
    assert(engine.getAsks().empty());

    std::cout << "Partial Fill Remaining Test Passed!\n";
}

void testMultipleFills() {
    MatchingEngine engine;

    engine.addOrder(Order{501, 100, 10, Side::SELL});
    engine.addOrder(Order{502, 101, 20, Side::SELL});
    engine.addOrder(Order{503, 102, 30, Side::SELL});

    auto reports = engine.addOrder(
        Order{504, 105, 45, Side::BUY}
    );

    // 应该产生三笔成交
    assert(reports.size() == 3);

    // 第一笔
    assert(reports[0].sellOrderId == 501);
    assert(reports[0].price == 100);
    assert(reports[0].quantity == 10);

    // 第二笔
    assert(reports[1].sellOrderId == 502);
    assert(reports[1].price == 101);
    assert(reports[1].quantity == 20);

    // 第三笔
    assert(reports[2].sellOrderId == 503);
    assert(reports[2].price == 102);
    assert(reports[2].quantity == 15);

    // 前两笔卖单已完全成交
    assert(!engine.findOrder(501).has_value());
    assert(!engine.findOrder(502).has_value());

    // 第三笔卖单剩余 15 股
    auto remaining = engine.findOrder(503);
    assert(remaining.has_value());
    assert(remaining->quantity == 15);

    // 买单 504 已完全成交，不应留在订单簿
    assert(!engine.findOrder(504).has_value());

    std::cout << "Multiple Fills Test Passed!\n";
}

int main(int argc, char* argv[]) {

    if (argc > 1 && std::string(argv[1]) == "test") {
        testDoubleCancel();
        testCancelMiddleOrder();
        testPartialFillRemaining();
        testMultipleFills();

        std::cout << "All tests passed!\n";
        return 0;
    }


    const int N = 1'000'000;

    std::vector<Order> orders;
    orders.reserve(N);

    std::mt19937 rng(42);

    std::uniform_int_distribution<int> priceDist(95, 105);
    std::uniform_int_distribution<int> qtyDist(1, 100);
    std::uniform_int_distribution<int> sideDist(0, 1);

    // for (int i = 0; i < N; i++) {
    //     int price = priceDist(rng);
    //     int quantity = qtyDist(rng);

    //     Side side = (sideDist(rng) == 0) ? Side::BUY : Side::SELL;

    //     orders.push_back({i + 1, price, quantity, side});
    // }

    for (int i = 0; i < N; i++) {
    Side side = (i % 2 == 0) ? Side::BUY : Side::SELL;

    orders.push_back({
        i + 1,
        100,
        10,
        side
    });
}
    MatchingEngine engine;
    engine.reserveTrades(500000);

    auto start = std::chrono::steady_clock::now();
    for (const auto& order : orders) {
        engine.addOrder(order);
    }
    auto end = std::chrono::steady_clock::now();
    std::cout << "Total Trades: "
          << engine.getTrades().size()
          << '\n';
    std::cout << "Trade Capacity: "
          << engine.getTrades().capacity()
          << '\n';
    std::chrono::duration<double> elapsed = end - start;
    double throughput = orders.size() / elapsed.count();

    std::cout << "Total Orders: " << orders.size() << '\n';

    std::cout << "Elapsed Time: " << elapsed.count() << " seconds\n";

    std::cout << "Throughput: " << throughput << " orders/sec\n";
    
    return 0;
   
}