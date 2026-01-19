#ifndef UTILITIES_HPP
#define UTILITIES_HPP

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

// Helper to increment port number in a string (e.g. "fa0/1" -> "fa0/2")
std::string increment_port(std::string port_name);

#endif // UTILITIES_HPP
