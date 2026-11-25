#pragma once

#include <cstdint>
#include "Side.h"

using OrderId = std::int64_t;
using Quantity = std::int64_t;

struct Order {
    OrderId orderId;
    long long price;
    Quantity quantity;
    Side side;
};
