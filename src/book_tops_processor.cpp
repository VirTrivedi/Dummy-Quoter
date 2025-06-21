#include "book_tops_processor.h"
#include "types/market_data_types.h"
#include <iostream>
#include <fstream>

BookTopsProcessor::BookTopsProcessor(Quoter& quoter) : quoter_(quoter) {}

void BookTopsProcessor::processSingleBookTop(const book_top_t& bookTop) {
    quoter_.updateStrategy(bookTop);
}

void BookTopsProcessor::processBookTops(const std::string& topsFilePath) {
    std::ifstream topsFile(topsFilePath, std::ios::binary);
    if (!topsFile.is_open()) {
        std::cerr << "Error: Could not open book tops file: " << topsFilePath << std::endl;
        throw std::runtime_error("Failed to open book tops file");
    }
    
    // Read header first
    book_tops_file_hdr_t header;
    topsFile.read(reinterpret_cast<char*>(&header), sizeof(book_tops_file_hdr_t));
    
    if (!topsFile) {
        std::cerr << "Error: Could not read header from book tops file" << std::endl;
        topsFile.close();
        throw std::runtime_error("Failed to read header from book tops file");
    }
    
    std::cout << "Processing " << header.number_of_tops << " top book entries for symbol index " 
              << header.symbol_idx << std::endl;
    
    // Set the symbol ID in the quoter
    quoter_.setSymbolId(header.symbol_idx);
    
    // Read and process each book top
    uint32_t count = 0;
    book_top_t bookTop;
    while (topsFile.read(reinterpret_cast<char*>(&bookTop), sizeof(book_top_t))) {
        // Process each book top
        processSingleBookTop(bookTop);
        count++;
        
        if (count % 10000 == 0) {
            std::cout << "Processed " << count << " book tops..." << std::endl;
        }
    }
    
    std::cout << "Finished processing " << count << " book tops" << std::endl;
    topsFile.close();
}