#ifndef COLORS_HPP
#define COLORS_HPP
#include <string>

namespace Color {
    const std::string RESET   = "\033[0m";
    const std::string BOLD    = "\033[1m";
    const std::string RED     = "\033[31m"; // Errors / Routers
    const std::string GREEN   = "\033[32m"; // Success / Switches / Free
    const std::string YELLOW  = "\033[33m"; // Warnings / VLANs / Assigned
    const std::string BLUE    = "\033[34m"; // Menus
    const std::string MAGENTA = "\033[35m"; // Headers
    const std::string CYAN    = "\033[36m"; // Info / PCs
    const std::string WHITE   = "\033[37m";
}

namespace Icon {
    const std::string ROUTER  = "🕸️ ";
    const std::string SWITCH  = "🔌 ";
    const std::string PC      = "💻 ";
    const std::string CHECK   = "✅ ";
    const std::string CROSS   = "❌ ";
    const std::string WARN    = "⚠️ ";
    const std::string LINK    = "🔗 ";
    const std::string TREE    = "├── ";
    const std::string ARROW   = "➜ ";
}
#endif
