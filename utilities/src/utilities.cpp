#include "utilities.hpp"
#include <string>
#include <algorithm>
#include <cctype>

std::string increment_port(std::string port_name) {
    if (port_name.empty()) return port_name;

    // Find the last number sequence in the string
    size_t i = port_name.length() - 1;
    while (i > 0 && !isdigit(port_name[i])) {
        i--;
    }
    
    // If no digit found
    if (!isdigit(port_name[i])) return port_name;

    size_t end = i;
    while (i > 0 && isdigit(port_name[i-1])) {
        i--;
    }
    size_t start = i;
    
    // Extract number
    std::string num_str = port_name.substr(start, end - start + 1);
    int num = std::stoi(num_str);
    num++;
    
    // Reconstruct
    return port_name.substr(0, start) + std::to_string(num) + port_name.substr(end + 1);
}
