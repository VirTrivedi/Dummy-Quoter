#include "file_handler.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include "order.h"

// Function to write orders to a file
void writeOrdersToFile(const std::string& filename, const std::vector<Order>& orders) {
    std::ofstream output_file(filename, std::ios::binary | std::ios::trunc);
    if (!output_file.is_open()) {
        throw std::runtime_error("Error: Could not open output file: " + filename);
    }

    for (const auto& order : orders) {
        output_file.write(reinterpret_cast<const char*>(&order), sizeof(Order));
        if (!output_file) {
            throw std::runtime_error("Error: Failed to write order to output file.");
        }
    }

    output_file.close();
}

// Function to read orders from a file
std::vector<Order> readOrdersFromFile(const std::string& filename) {
    std::ifstream input_file(filename, std::ios::binary);
    if (!input_file.is_open()) {
        throw std::runtime_error("Error: Could not open input file: " + filename);
    }

    std::vector<Order> orders;
    Order order;

    while (input_file.read(reinterpret_cast<char*>(&order), sizeof(Order))) {
        orders.push_back(order);
    }

    input_file.close();
    return orders;
}