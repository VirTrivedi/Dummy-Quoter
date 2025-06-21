#ifndef QUOTER_H
#define QUOTER_H

#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <map>
#include "types/market_data_types.h"

class Quoter {
public:
    Quoter(const std::string& outputFilePath);
    ~Quoter();

    void updateStrategy(const book_top_t& bookTop);
    void processFill(const book_fill_snapshot_t& fill);
    void processBookTops(const std::vector<book_top_t>& bookTops);
    void placeOrder(uint64_t orderId, uint32_t symbolId, uint64_t sentTs, 
                    uint64_t mdTs, int64_t priceNanos, uint32_t quantity, 
                    bool isBid, bool isPostOnly = true);
    void setSymbolId(uint64_t symbolId);

private:
    std::string outputFilePath_;
    std::ofstream outputFile_;
    uint64_t nextOrderId_;
    uint64_t symbolId_;
    
    // Track current orders at each price level
    std::map<int64_t, uint64_t> bidOrders_;
    std::map<int64_t, uint64_t> askOrders_;
    
    void updateQuotes(const book_top_t& bookTop);
};

#endif