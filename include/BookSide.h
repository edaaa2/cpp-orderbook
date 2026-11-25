#pragma once

#include <iostream>
#include <map>
#include <list>
#include <functional>
#include <unordered_map>
#include <iterator>
#include <optional>
#include <utility>


#include "Order.h"

template <typename Compare>
class BookSide {
private:
    std::map<long long, std::list<Order>, Compare> orders;

    std::unordered_map<OrderId, std::list<Order>::iterator> orderIndex;

public:
    void addOrder(Order order){
        if (orderIndex.find(order.orderId) != orderIndex.end()) return;  

        auto& level = orders[order.price];   //orders[price] -> list<Order>
        
        // level.push_back(order);
        level.emplace_back(std::move(order));
        
        auto orderIt = std::prev(level.end());
        orderIndex.emplace(orderIt->orderId, orderIt);
    }

    Order& bestOrder(){
        auto it = orders.begin();
        return it->second.front();   //price and time priority
    }
    const Order& bestOrder() const {
        auto it = orders.begin();
        return it->second.front();
    }

    bool empty() const {
        return orders.empty();  //calls std::map empty function
    }

    // Sum all resting orders at the best price. Requires a nonempty side.
    Quantity bestLevelQuantity() const {
        Quantity total = 0;
        for (const auto& order : orders.begin()->second) {
            total += order.quantity;
        }
        return total;
    }

    template <typename Visitor>
    void visitBestLevel(Visitor visit) const {
        for (const auto& order : orders.begin()->second) {
            visit(order);
        }
    }

    void removeBestOrder() {
        auto it = orders.begin();

        OrderId orderId = it->second.front().orderId;
        orderIndex.erase(orderId);

        it->second.pop_front();   // calls std::list member function
        if (it->second.empty()) {
            orders.erase(it);    //delete map member
        }
    }


    bool cancelOrder(OrderId orderId) {
        auto indexIt = orderIndex.find(orderId); //unordered_map key and value

        if (indexIt == orderIndex.end()) {
            return false;
        }
        auto orderIt = indexIt->second;   //list<Order> iteration

        auto levelIt = orders.find(orderIt->price); 
        
        levelIt->second.erase(orderIt);   //delete map 
        
        orderIndex.erase(indexIt);  //delete iterator
        if (levelIt->second.empty()) {
            orders.erase(levelIt);
        }
        return true;
    }

    std::optional<Order> findOrder(OrderId orderId) const {
        auto it = orderIndex.find(orderId);

        if (it == orderIndex.end()) {
            return std::nullopt;
        }
        return *(it->second);
    }

    // Apply a quantity reduction from an external event without matching.
    bool reduceOrder(OrderId orderId, Quantity quantity) {
        auto indexIt = orderIndex.find(orderId);
        if (indexIt == orderIndex.end() || quantity <= 0 ||
            quantity > indexIt->second->quantity) {
            return false;
        }

        auto orderIt = indexIt->second;
        if (quantity == orderIt->quantity) {
            return cancelOrder(orderId);
        }

        orderIt->quantity -= quantity;
        return true;
    }

    
    void print() const {
        for (const auto& [price, level] : orders) {
            std::cout << "Price: " << price << '\n';

            for (const auto& order : level) {
                std::cout << "Order ID: " << order.orderId
                        << ", Quantity: " << order.quantity << '\n';
            }
        }
    }

   
};
