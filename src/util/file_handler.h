#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include <string>
#include <vector>
#include "order.h"

// Function to read orders from a file
std::vector<Order> readOrdersFromFile(const std::string& filePath);

// Function to write orders to a file
void writeOrdersToFile(const std::string& filePath, const std::vector<Order>& orders);

#endif