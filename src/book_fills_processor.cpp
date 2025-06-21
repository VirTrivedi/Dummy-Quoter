#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include "book_fills_processor.h"
#include "quoter.h"
#include "types/market_data_types.h"

BookFillsProcessor::BookFillsProcessor(Quoter& quoter) : quoter_(quoter) {}

void BookFillsProcessor::processSingleBookFill(const book_fill_snapshot_t& fill) {
    std::cout << "Processing fill at time " << fill.ts << ": " 
              << fill.trade_qty << " @ " << static_cast<double>(fill.trade_price)/1e9 
              << " (resting side is " << (fill.resting_side_is_bid ? "bid" : "ask") << ")" << std::endl;
    quoter_.processFill(fill);
}

void BookFillsProcessor::processBookFills(const std::string& fillsFilePath) {
    std::ifstream fillsFile(fillsFilePath, std::ios::binary);
    if (!fillsFile.is_open()) {
        std::cerr << "Error: Could not open fills file: " << fillsFilePath << std::endl;
        return;
    }

    // Read the header first
    book_fills_file_hdr_t header;
    fillsFile.read(reinterpret_cast<char*>(&header), sizeof(book_fills_file_hdr_t));
    
    if (!fillsFile) {
        std::cerr << "Error: Could not read header from fills file" << std::endl;
        fillsFile.close();
        return;
    }
    
    std::cout << "Processing " << header.number_of_fills << " fills for symbol index " 
              << header.symbol_idx << std::endl;

    // Read each fill snapshot
    uint32_t count = 0;
    for (uint32_t i = 0; i < header.number_of_fills; ++i) {
        book_fill_snapshot_t fill;
        fillsFile.read(reinterpret_cast<char*>(&fill), sizeof(book_fill_snapshot_t));
        
        if (!fillsFile) {
            std::cerr << "Error: Failed to read fill snapshot #" << i << std::endl;
            break;
        }
        
        // Process the fill
        processSingleBookFill(fill);
        count++;
        
        if (count % 10000 == 0) {
            std::cout << "Processed " << count << " fills..." << std::endl;
        }
    }
    
    std::cout << "Finished processing " << count << " fills" << std::endl;
    fillsFile.close();
}