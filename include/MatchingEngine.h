#pragma once

#include <utility>
#include <functional>
#include <algorithm>
#include "BookSide.h"
#include "Trade.h"
#include <vector>
#include <optional>
#include "TradeListener.h"

class MatchingEngine {
private:
    BookSide<std::greater<long long>> bids;
    BookSide<std::less<long long>> asks;
    std::vector<Trade> trades;
    TradeListener* listener = nullptr;

public:
    void reserveTrades(std::size_t n) {
        trades.reserve(n);
    } 

    std::vector<Trade> addOrder(Order incomingOrder) {

        std::vector<Trade> reports;
        
        if (incomingOrder.side == Side::BUY) {

            while (incomingOrder.quantity > 0 && !asks.empty()) {

                Order& bestAsk = asks.bestOrder();

                if (incomingOrder.price < bestAsk.price) {
                    break;
                }
 
                Quantity tradeQty = std::min(incomingOrder.quantity, bestAsk.quantity);

                incomingOrder.quantity -= tradeQty;
                bestAsk.quantity -= tradeQty;

                //create trade
                Trade trade{
                    incomingOrder.orderId,
                    bestAsk.orderId,
                    bestAsk.price,
                    tradeQty
                };

                trades.push_back(trade); 
                reports.push_back(trade);
                if (listener != nullptr) listener->onTrade(trade);


                if (bestAsk.quantity == 0) asks.removeBestOrder();

            }

            if (incomingOrder.quantity > 0) {
                bids.addOrder(std::move(incomingOrder));
            }
        }

        else if (incomingOrder.side == Side::SELL) {

            while (incomingOrder.quantity > 0 && !bids.empty()) {

                Order& bestBid = bids.bestOrder();

                if (incomingOrder.price > bestBid.price) {
                    break;
                }

                // compute quantity Trade
                Quantity tradeQty = std::min(incomingOrder.quantity, bestBid.quantity);
                incomingOrder.quantity -= tradeQty;
                bestBid.quantity -= tradeQty;

                Trade trade{
                    bestBid.orderId,
                    incomingOrder.orderId,
                    bestBid.price,
                    tradeQty
                };

                trades.push_back(trade);
                reports.push_back(trade);
                if (listener != nullptr) listener->onTrade(trade);

                if(bestBid.quantity == 0) bids.removeBestOrder();
            }

            if (incomingOrder.quantity > 0) {
                asks.addOrder(std::move(incomingOrder));
            }

        }
        return reports;
    }

    bool cancelOrder(OrderId orderId) {
        if (bids.cancelOrder(orderId)) {
            return true;
        }

        return asks.cancelOrder(orderId);
    }

    std::optional<Order> findOrder(OrderId orderId) const {

        auto result = bids.findOrder(orderId);

        if (result.has_value()) {
            return result;
        }

        return asks.findOrder(orderId);
    }

    const std::vector<Trade>& getTrades() const {
        return trades;
    }

    

    const BookSide<std::greater<long long>>& getBids() const {
        return bids;
    }

    const BookSide<std::less<long long>>& getAsks() const {
        return asks;
    }

    void setTradeListener(TradeListener* newListener) {
        listener = newListener;
    }
};
