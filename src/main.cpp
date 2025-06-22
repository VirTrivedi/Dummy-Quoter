#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>
#include <sys/stat.h>
#include <queue>
#include <memory>
#include "quoter.h"
#include "book_tops_processor.h"
#include "book_fills_processor.h"
#include "types/market_data_types.h"

// Helper function to check if file exists
bool file_exists(const std::string& filename) {
    struct stat buffer;
    return (stat(filename.c_str(), &buffer) == 0);
}

// Helper function to create directories
bool create_directories(const std::string& path) {
    std::string command = "mkdir -p " + path;
    return (system(command.c_str()) == 0);
}

// Helper function to get directory from path
std::string get_directory(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) {
        return "";
    }
    return path.substr(0, pos);
}

// Store market data events for sorting
enum class EventType { BOOK_TOP, BOOK_FILL };

struct MarketDataEvent {
    uint64_t timestamp;
    EventType type;
    book_top_t book_top;
    book_fill_snapshot_t book_fill;

    // Constructor for book top and fill
    MarketDataEvent(uint64_t ts, const book_top_t& top)
        : timestamp(ts), type(EventType::BOOK_TOP), book_top(top) {}

    MarketDataEvent(uint64_t ts, const book_fill_snapshot_t& fill)
        : timestamp(ts), type(EventType::BOOK_FILL), book_fill(fill) {}
};

// Comparison operator for priority queue
struct EventComparator {
    bool operator()(const std::shared_ptr<MarketDataEvent>& a, const std::shared_ptr<MarketDataEvent>& b) {
        return a->timestamp > b->timestamp;
    }
};

int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <book_tops_file> <book_fills_file> <output_orders_file>" << std::endl;
        return 1;
    }

    std::string book_tops_file = argv[1];
    std::string book_fills_file = argv[2];
    std::string output_orders_file = argv[3];

    // Check if input files exist
    if (!file_exists(book_tops_file)) {
        std::cerr << "Error: Book tops file does not exist: " << book_tops_file << std::endl;
        return 1;
    }

    if (!file_exists(book_fills_file)) {
        std::cerr << "Error: Book fills file does not exist: " << book_fills_file << std::endl;
        return 1;
    }

    // Check if output directory exists and create if needed
    std::string output_dir = get_directory(output_orders_file);
    if (!output_dir.empty()) {
        std::cout << "Creating output directory: " << output_dir << std::endl;
        if (!create_directories(output_dir)) {
            std::cerr << "Error creating output directory: " << output_dir << std::endl;
            return 1;
        }
    }

    try {
        // Create quoter with the output file path
        Quoter quoter(output_orders_file);

        // Open the tops file and read header
        std::ifstream tops_file(book_tops_file, std::ios::binary);
        if (!tops_file.is_open()) {
            throw std::runtime_error("Failed to open book tops file: " + book_tops_file);
        }
        
        book_tops_file_hdr_t tops_header;
        tops_file.read(reinterpret_cast<char*>(&tops_header), sizeof(book_tops_file_hdr_t));
        if (!tops_file) {
            throw std::runtime_error("Failed to read header from book tops file");
        }
        
        // Open the fills file and read header
        std::ifstream fills_file(book_fills_file, std::ios::binary);
        if (!fills_file.is_open()) {
            throw std::runtime_error("Failed to open book fills file: " + book_fills_file);
        }
        
        book_fills_file_hdr_t fills_header;
        fills_file.read(reinterpret_cast<char*>(&fills_header), sizeof(book_fills_file_hdr_t));
        if (!fills_file) {
            throw std::runtime_error("Failed to read header from book fills file");
        }

        // Set the symbol ID in quoter
        quoter.setSymbolId(tops_header.symbol_idx);
        
        std::cout << "Processing " << tops_header.number_of_tops << " tops and " 
                  << fills_header.number_of_fills << " fills for symbol " 
                  << tops_header.symbol_idx << std::endl;

        // Create processors
        BookTopsProcessor tops_processor(quoter);
        BookFillsProcessor fills_processor(quoter);

        std::priority_queue<
            std::shared_ptr<MarketDataEvent>, 
            std::vector<std::shared_ptr<MarketDataEvent>>, 
            EventComparator
        > event_queue;

        // Read first book top and book fill
        book_top_t top;
        book_fill_snapshot_t fill;

        tops_file.read(reinterpret_cast<char*>(&top), sizeof(book_top_t));
        bool has_more_tops = tops_file.good() && !tops_file.eof();

        fills_file.read(reinterpret_cast<char*>(&fill), sizeof(book_fill_snapshot_t));
        bool has_more_fills = fills_file.good() && !fills_file.eof();

        if (has_more_tops) {
            event_queue.push(std::make_shared<MarketDataEvent>(top.ts, top));
        }
        
        if (has_more_fills) {
            event_queue.push(std::make_shared<MarketDataEvent>(fill.ts, fill));
        }

        uint64_t processed_tops = 0;
        uint64_t processed_fills = 0;
        uint64_t last_ts = 0;

        // Add a variable to track the most recent valid book top
        book_top_t most_recent_book_top;
        bool have_valid_book_top = false;

        // Process events in timestamp order
        while (!event_queue.empty()) {
            auto event = event_queue.top();
            event_queue.pop();
            
            // Process based on event type
            if (event->type == EventType::BOOK_TOP) {
                if (event->book_top.top_level.ask_nanos != INT64_MAX &&
                    event->book_top.top_level.bid_nanos != INT64_MAX) {

                    tops_processor.processSingleBookTop(event->book_top);
                    processed_tops++;
                
                    most_recent_book_top = event->book_top;
                    have_valid_book_top = true;
                } else {
                    std::cerr << "Warning: Invalid book top detected, skipping..." << std::endl;
                }
                
                if (tops_file.read(reinterpret_cast<char*>(&top), sizeof(book_top_t))) {
                    event_queue.push(std::make_shared<MarketDataEvent>(top.ts, top));
                }
            }
            else {
                fills_processor.processSingleBookFill(event->book_fill);
                processed_fills++;
                
                if (fills_file.read(reinterpret_cast<char*>(&fill), sizeof(book_fill_snapshot_t))) {
                    event_queue.push(std::make_shared<MarketDataEvent>(fill.ts, fill));
                }
            }

            // Print progress every 10,000 events
            if ((processed_tops + processed_fills) % 10000 == 0) {
                std::cout << "Processed " << processed_tops << " tops and " 
                          << processed_fills << " fills..." << std::endl;
            }
            
            if (event->timestamp < last_ts) {
                std::cerr << "Warning: Non-monotonic timestamp detected: " 
                          << event->timestamp << " after " << last_ts << std::endl;
            }
            last_ts = event->timestamp;
        }

        std::cout << "Processing complete. Processed " << processed_tops 
                  << " tops and " << processed_fills << " fills." << std::endl;
        std::cout << "Orders written to: " << output_orders_file << std::endl;
        
        // Calculate end-of-day PnL using the last book top
        if (have_valid_book_top) {            
            quoter.calculateEndOfDayPnL(most_recent_book_top);
        } else {
            std::cerr << "Error: No book tops were processed, cannot calculate PnL" << std::endl;
        }        
        // Close the files
        tops_file.close();
        fills_file.close();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}