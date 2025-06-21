#ifndef BOOK_FILLS_PROCESSOR_H
#define BOOK_FILLS_PROCESSOR_H

#include <string>
#include "quoter.h"
#include "types/market_data_types.h"

class BookFillsProcessor {
public:
    BookFillsProcessor(Quoter& quoter);
    
    // Method to process the whole file
    void processBookFills(const std::string& fillsFilePath);
    
    // Method to process a single book fill
    void processSingleBookFill(const book_fill_snapshot_t& fill);
    
private:
    Quoter& quoter_;
};

#endif