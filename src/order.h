#ifndef ORDER_H
#define ORDER_H

#include <cstdint>

struct Order {
    uint64_t orderid;
    uint32_t symbolid;
    uint64_t sent_ts;
    uint64_t md_ts;
    int64_t price_nanos;
    uint32_t quantity;
    bool is_bid;
    bool is_post_only;

    Order() : orderid(0), symbolid(0), sent_ts(0), md_ts(0), 
              price_nanos(0), quantity(0), is_bid(false), is_post_only(true) {}
};

#endif