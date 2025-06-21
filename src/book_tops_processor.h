#ifndef BOOK_TOPS_PROCESSOR_H
#define BOOK_TOPS_PROCESSOR_H

#include <string>
#include "quoter.h"
#include "types/market_data_types.h"

class BookTopsProcessor {
public:
    BookTopsProcessor(Quoter& quoter);
    
    // Method to process the whole file
    void processBookTops(const std::string& topsFilePath);
    
    // Method to process a single book top
    void processSingleBookTop(const book_top_t& bookTop);
    
private:
    Quoter& quoter_;
};

#endif