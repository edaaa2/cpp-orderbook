#pragma once

#include "Side.h"

struct Order {
    int orderId;
    long long price;
    int quantity;
    Side side;
};