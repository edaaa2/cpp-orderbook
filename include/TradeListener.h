#pragma once

#include<iostream>
#include "Trade.h"

class TradeListener {
public:
    virtual void onTrade(const Trade& trade) = 0;

    virtual ~TradeListener() = default;
};

class ConsoleTradeListener : public TradeListener {
public:
    void onTrade(const Trade& trade) override {
        std::cout << "Trade executed: "
                  << "Buy ID: " << trade.buyOrderId
                  << ", Sell ID: " << trade.sellOrderId
                  << ", Price: " << trade.price
                  << ", Quantity: " << trade.quantity
                  << '\n';
    }
};