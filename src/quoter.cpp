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

void Quoter::trackFill(uint64_t orderId, int64_t price_nanos, uint32_t quantity, bool isBid) {
    // Find the order in tracking map
    auto orderIt = allOrders_.find(orderId);
    if (orderIt == allOrders_.end()) {
        std::cerr << "Warning: Received fill for unknown order ID: " << orderId << std::endl;
        return;
    }
    
    // Update the order's fill information
    OrderInfo& order = orderIt->second;
    order.filled_quantity += quantity;
    
    std::cout << "Fill tracked: " << (isBid ? "BUY" : "SELL") << " " << quantity 
              << " @ " << static_cast<double>(price_nanos) / 1e9 << std::endl;
}

void Quoter::processFill(const book_fill_snapshot_t& fill) {
    if (fill.resting_side_is_bid) {
        auto it = bidOrders_.find(fill.resting_side_price);
        if (it != bidOrders_.end() && it->second == fill.resting_order_id) {
            bidOrders_.erase(it);
            
            trackFill(fill.resting_order_id, fill.trade_price, fill.trade_qty, true);
        }
    } else {
        auto it = askOrders_.find(fill.resting_side_price);
        if (it != askOrders_.end() && it->second == fill.resting_order_id) {
            askOrders_.erase(it);
            
            trackFill(fill.resting_order_id, fill.trade_price, fill.trade_qty, false);
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
    
    if (priceNanos <= 0) {
        std::cerr << "Warning: Ignoring order with non-positive price: " << priceNanos << std::endl;
        return;
    }

    // Write order to binary file
    outputFile_.write(reinterpret_cast<const char*>(&order), sizeof(Order));
    
    // Check if write was successful
    if (!outputFile_) {
        std::cerr << "Error: Failed to write order to file" << std::endl;
    }
    
    // Store order info for PnL calculation
    OrderInfo orderInfo;
    orderInfo.orderId = orderId;
    orderInfo.price_nanos = priceNanos;
    orderInfo.quantity = quantity;
    orderInfo.filled_quantity = 0;
    orderInfo.is_bid = isBid;
    allOrders_[orderId] = orderInfo;
    
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
    
    // Check and place bid order for the top level
    if (bidPrices[0] > 0 && bidQtys[0] > 0 && bidOrders_.find(bidPrices[0]) == bidOrders_.end()) {
        placeOrder(nextOrderId_++, symbolId, currentTime, 
                  bookTop.ts, bidPrices[0], bidQtys[0], true);
    }
    
    // Check and place ask order for the top level
    if (askPrices[0] > 0 && askQtys[0] > 0 && askOrders_.find(askPrices[0]) == askOrders_.end()) {
        placeOrder(nextOrderId_++, symbolId, currentTime, 
                  bookTop.ts, askPrices[0], askQtys[0], false);
    }
    
    // Remove orders at price levels that are no longer in top level
    auto bidIt = bidOrders_.begin();
    while (bidIt != bidOrders_.end()) {
        if (bidIt->first == bidPrices[0]) {
            ++bidIt;
        } else {
            bidIt = bidOrders_.erase(bidIt);
        }
    }
    
    auto askIt = askOrders_.begin();
    while (askIt != askOrders_.end()) {
        if (askIt->first == askPrices[0]) {
            ++askIt;
        } else {
            askIt = askOrders_.erase(askIt);
        }
    }
}

void Quoter::calculateEndOfDayPnL(const book_top_t& finalBookTop) {
    int64_t finalBid = finalBookTop.top_level.bid_nanos;
    int64_t finalAsk = finalBookTop.top_level.ask_nanos;
    int64_t finalMidPrice = (finalBid + finalAsk) / 2;

    if (finalBid <= 0 || finalAsk <= 0) {
        std::cout << "WARNING: Invalid final prices detected! Bid: " << finalBid 
                << ", Ask: " << finalAsk << std::endl;
        // Use the last valid price instead, or a reasonable default
        finalBid = std::max(finalBid, static_cast<int64_t>(1000000000)); // $1.00
        finalAsk = std::max(finalAsk, static_cast<int64_t>(1000000000)); // $1.00
        finalMidPrice = (finalBid + finalAsk) / 2;
    }
    
    // Calculate PnL assuming all orders were filled
    int64_t simulatedPosition = 0;
    int64_t simulatedCash = 0;
    
    double simulatedBuyVolume = 0;
    double simulatedSellVolume = 0;
    double simulatedBuyCost = 0;
    double simulatedSellProceeds = 0;
    
    std::cout << "\nCalculating PnL assuming all orders were filled..." << std::endl;
    
    // Loop through all orders and assume they were filled
    for (const auto& orderPair : allOrders_) {
        const OrderInfo& order = orderPair.second;
        
        uint32_t fillQty = order.quantity;
        int64_t fillPrice = order.price_nanos;
        
        if (order.is_bid) {
            simulatedPosition += fillQty;
            simulatedCash -= fillPrice * fillQty;
            
            simulatedBuyVolume += fillQty;
            simulatedBuyCost += (fillPrice * fillQty) / 1e9;
        } else {
            simulatedPosition -= fillQty;
            simulatedCash += fillPrice * fillQty;
            
            simulatedSellVolume += fillQty;
            simulatedSellProceeds += (fillPrice * fillQty) / 1e9;
        }
    }
    
    // Close any remaining positions at the final mid price
    int64_t closingPnL = 0;
    if (simulatedPosition != 0) {
        std::cout << "End of day: Closing remaining position of " << simulatedPosition 
                  << " shares at mid price " << static_cast<double>(finalMidPrice) / 1e9 << " USD" << std::endl;
                  
        closingPnL = simulatedPosition * finalMidPrice;
        simulatedCash += closingPnL;
    }
    
    // Calculate the final PnL
    double simulatedPnL = static_cast<double>(simulatedCash) / 1e9;
    
    // Print PnL report
    std::cout << "\n========= SIMULATED FILLS PNL REPORT =========\n";
    std::cout << "Symbol ID: " << symbolId_ << std::endl;
    std::cout << "Total Orders Placed: " << allOrders_.size() << std::endl;
    std::cout << "Total Buys: " << simulatedBuyVolume << " shares for $" << simulatedBuyCost << std::endl;
    std::cout << "Total Sells: " << simulatedSellVolume << " shares for $" << simulatedSellProceeds << std::endl;
    
    std::cout << "Final Mid Price: $" << static_cast<double>(finalMidPrice) / 1e9 << std::endl;
    
    if (simulatedPosition != 0) {
        std::cout << "End Position: " << simulatedPosition << " shares" << std::endl;
        std::cout << "Closing Position Value: $" << static_cast<double>(closingPnL) / 1e9 << std::endl;
    }
    
    std::cout << "Final P&L: $" << simulatedPnL << std::endl;
    
    if (simulatedPnL > 0) {
        std::cout << "Trading result: PROFIT" << std::endl;
    } else if (simulatedPnL < 0) {
        std::cout << "Trading result: LOSS" << std::endl;
    } else {
        std::cout << "Trading result: BREAKEVEN" << std::endl;
    }
    std::cout << "=============================================\n" << std::endl;
}