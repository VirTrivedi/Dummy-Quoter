#include "quoter.h"
#include <iostream>
#include <chrono>
#include <algorithm>

Quoter::Quoter(const std::string& outputFilePath) 
    : outputFilePath_(outputFilePath), nextOrderId_(1), symbolId_(0) {
    
    // Open output file in binary mode
    outputFile_.open(outputFilePath_, std::ios::binary | std::ios::trunc);
    if (!outputFile_.is_open()) {
        std::cerr << "Error: Unable to open output file: " << outputFilePath_ << std::endl;
        throw std::runtime_error("Failed to open output file");
    }
}

Quoter::~Quoter() {
    if (outputFile_.is_open()) {
        outputFile_.close();
    }
}

void Quoter::setSymbolId(uint64_t symbolId) {
    symbolId_ = symbolId;
    std::cout << "Symbol ID set to: " << symbolId_ << std::endl;
}

void Quoter::updateStrategy(const book_top_t& bookTop) {
    updateQuotes(bookTop);
}

void Quoter::processBookTops(const std::vector<book_top_t>& bookTops) {
    for (const auto& bookTop : bookTops) {
        updateStrategy(bookTop);
    }
}

void Quoter::processFill(const book_fill_snapshot_t& fill) {
    if (fill.resting_side_is_bid) {
        auto it = bidOrders_.find(fill.resting_side_price);
        if (it != bidOrders_.end() && it->second == fill.resting_order_id) {
            bidOrders_.erase(it);
        }
    } else {
        auto it = askOrders_.find(fill.resting_side_price);
        if (it != askOrders_.end() && it->second == fill.resting_order_id) {
            askOrders_.erase(it);
        }
    }
}

void Quoter::placeOrder(uint64_t orderId, uint32_t symbolId, uint64_t sentTs, 
                        uint64_t mdTs, int64_t priceNanos, uint32_t quantity, 
                        bool isBid, bool isPostOnly) {
    
    Order order;
    order.orderid = orderId;
    order.symbolid = symbolId;
    order.sent_ts = sentTs;
    order.md_ts = mdTs;
    order.price_nanos = priceNanos;
    order.quantity = quantity;
    order.is_bid = isBid;
    order.is_post_only = isPostOnly;
    
    // Write order to binary file
    outputFile_.write(reinterpret_cast<const char*>(&order), sizeof(Order));
    
    // Check if write was successful
    if (!outputFile_) {
        std::cerr << "Error: Failed to write order to file" << std::endl;
    }
    
    // Update our tracking maps
    if (isBid) {
        bidOrders_[priceNanos] = orderId;
    } else {
        askOrders_[priceNanos] = orderId;
    }
}

void Quoter::updateQuotes(const book_top_t& bookTop) {
    uint64_t currentTime = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    uint32_t symbolId = static_cast<uint32_t>(symbolId_); 
    
    // Process top 3 bid levels
    int64_t bidPrices[3] = {
        bookTop.top_level.bid_nanos,
        bookTop.second_level.bid_nanos,
        bookTop.third_level.bid_nanos
    };
    
    uint32_t bidQtys[3] = {
        bookTop.top_level.bid_qty,
        bookTop.second_level.bid_qty,
        bookTop.third_level.bid_qty
    };
    
    // Process top 3 ask levels
    int64_t askPrices[3] = {
        bookTop.top_level.ask_nanos,
        bookTop.second_level.ask_nanos,
        bookTop.third_level.ask_nanos
    };
    
    uint32_t askQtys[3] = {
        bookTop.top_level.ask_qty,
        bookTop.second_level.ask_qty,
        bookTop.third_level.ask_qty
    };
    
    // Check and place bid orders for each price level
    for (int i = 0; i < 3; ++i) {
        if (bidPrices[i] > 0 && bidQtys[i] > 0 && bidOrders_.find(bidPrices[i]) == bidOrders_.end()) {
            placeOrder(nextOrderId_++, symbolId, currentTime, 
                      bookTop.ts, bidPrices[i], bidQtys[i], true);
        }
    }
    
    // Check and place ask orders for each price level
    for (int i = 0; i < 3; ++i) {
        if (askPrices[i] > 0 && askQtys[i] > 0 && askOrders_.find(askPrices[i]) == askOrders_.end()) {
            placeOrder(nextOrderId_++, symbolId, currentTime, 
                      bookTop.ts, askPrices[i], askQtys[i], false);
        }
    }
    
    // Remove orders at price levels that are no longer in top 3
    auto bidIt = bidOrders_.begin();
    while (bidIt != bidOrders_.end()) {
        bool priceInTop3 = false;
        for (int i = 0; i < 3; ++i) {
            if (bidIt->first == bidPrices[i]) {
                priceInTop3 = true;
                break;
            }
        }
        
        if (!priceInTop3) {
            bidIt = bidOrders_.erase(bidIt);
        } else {
            ++bidIt;
        }
    }
    
    auto askIt = askOrders_.begin();
    while (askIt != askOrders_.end()) {
        bool priceInTop3 = false;
        for (int i = 0; i < 3; ++i) {
            if (askIt->first == askPrices[i]) {
                priceInTop3 = true;
                break;
            }
        }
        
        if (!priceInTop3) {
            askIt = askOrders_.erase(askIt);
        } else {
            ++askIt;
        }
    }
}