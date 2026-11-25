#pragma once

#include "Order.h"

struct Trade {
    OrderId buyOrderId;
    OrderId sellOrderId;
    long long price;
    Quantity quantity;
};
