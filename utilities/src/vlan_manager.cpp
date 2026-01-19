#include "vlan_manager.hpp"
#include <iostream>
#include <limits>
#include <sstream>
#include <algorithm>
#include "colors.hpp"

std::map<int, std::string> VlanManager::defined_vlans;

void VlanManager::init() {
    if (defined_vlans.find(1) == defined_vlans.end()) {
        defined_vlans[1] = "default";
    }
}

void VlanManager::add_vlan(int id, const std::string& name) {
    defined_vlans[id] = name;
    std::cout << Color::GREEN << Icon::CHECK << " VLAN " << id << " (" << name << ") defined." << Color::RESET << "\n";
}

bool VlanManager::vlan_exists(int id) {
    return defined_vlans.find(id) != defined_vlans.end();
}

std::string VlanManager::get_vlan_name(int id) {
    if (defined_vlans.find(id) != defined_vlans.end()) {
        return defined_vlans[id];
    }
    return "unknown";
}

// Helper to clear input buffer
static void clear_in() {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

// Helper: Check if a string contains any letters
static bool contains_letters(const std::string& s) {
    for (char c : s) {
        if (std::isalpha(static_cast<unsigned char>(c))) return true;
    }
    return false;
}

// Helper: Case-insensitive substring match
static bool icontains(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    if (haystack.size() < needle.size()) return false;
    
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); }
    );
    return it != haystack.end();
}

// Helper: Case-insensitive equality
static bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> VlanManager::parse_interface_range(const std::string& input) {
    // NEW SMART PARSING LOGIC:
    // 1. If input contains letters (e.g. "Gig0/1", "Fa0/5") -> treat as EXACT NAME
    // 2. If input is pure numbers/dashes/commas (e.g. "1-5", "1,2,3") -> treat as RANGE MODE
    
    std::vector<std::string> result;
    std::stringstream ss(input);
    std::string segment;
    
    while(std::getline(ss, segment, ',')) {
        // Trim whitespace
        segment.erase(0, segment.find_first_not_of(" \t\n\r"));
        segment.erase(segment.find_last_not_of(" \t\n\r") + 1);
        
        if (segment.empty()) continue;
        
        // Check if this token contains letters -> EXACT NAME MODE
        if (contains_letters(segment)) {
            // This is a named interface like "Gig0/1" or "Fa0/5"
            // Just pass it through for exact matching in assign_vlan_to_ports
            result.push_back(segment);
        } else {
            // RANGE MODE: Pure numbers (possibly with dash)
            size_t dash = segment.find('-');
            if (dash != std::string::npos && dash > 0 && dash < segment.length() - 1) {
                // Range like "1-5"
                try {
                    int start = std::stoi(segment.substr(0, dash));
                    int end = std::stoi(segment.substr(dash + 1));
                    if (start > end) std::swap(start, end);
                    
                    for(int i = start; i <= end; ++i) {
                        result.push_back("__RANGE__:" + std::to_string(i)); // Mark as range-generated
                    }
                } catch (...) {
                    std::cout << Color::YELLOW << Icon::WARN << " [WARN] Invalid range format: " << segment << Color::RESET << "\n";
                }
            } else {
                // Single number like "5"
                try {
                    int id = std::stoi(segment);
                    result.push_back("__RANGE__:" + std::to_string(id)); // Mark as range-generated
                } catch (...) {
                    // Not a number, treat as name
                    result.push_back(segment);
                }
            }
        }
    }
    return result;
}

void VlanManager::assign_vlan_to_ports(Switch* sw, const std::vector<std::string>& ports, int vlan_id, bool is_trunk) {
    std::string vname = get_vlan_name(vlan_id);
    
    // 1. Cleanup Phantom Interfaces (e.g. remove 'f0/1' if 'Fa0/1' exists)
    auto& ifaces = sw->interfaces;
    for (auto it = ifaces.begin(); it != ifaces.end(); ) {
        std::string n = it->name;
        if (n.rfind("f0/", 0) == 0) { 
            std::string suffix = n.substr(2);
            std::string better = "Fa0" + suffix;
            
            bool found_better = false;
            for(const auto& check : ifaces) {
                if (check.name == better) { found_better = true; break; }
            }
            
            if (found_better) {
                it = ifaces.erase(it);
                continue; 
            }
        }
        ++it;
    }

    // 2. Assign Ports with SMART MATCHING
    for(const auto& pname : ports) {
        Interface* target_iface = nullptr;
        
        // Check if this is a range-generated port (starts with __RANGE__:)
        if (pname.rfind("__RANGE__:", 0) == 0) {
            // RANGE MODE: Extract number and find interface ending with /N
            std::string num_str = pname.substr(10); // After "__RANGE__:"
            std::string suffix = "/" + num_str;
            
            // Search for existing interface ending with suffix (e.g. "/1")
            for (auto& iface : sw->interfaces) {
                if (iface.name.length() >= suffix.length()) {
                    if (iface.name.compare(iface.name.length() - suffix.length(), suffix.length(), suffix) == 0) {
                        target_iface = &iface;
                        break;
                    }
                }
            }
            
            // If not found, create new with "Fa0" prefix
            if (!target_iface) {
                std::string new_name = "Fa0" + suffix;
                sw->add_interface(new_name);
                target_iface = sw->get_interface(new_name);
            }
        } else {
            // EXACT NAME MODE: Find interface by name match
            // First try exact match (case-insensitive)
            for (auto& iface : sw->interfaces) {
                if (iequals(iface.name, pname)) {
                    target_iface = &iface;
                    break;
                }
            }
            
            // If no exact match, try substring match (case-insensitive)
            // e.g. "Gig0/1" should match "GigabitEthernet0/1"
            if (!target_iface) {
                for (auto& iface : sw->interfaces) {
                    if (icontains(iface.name, pname)) {
                        target_iface = &iface;
                        break;
                    }
                }
            }
            
            // If still not found, print error (don't auto-create for named interfaces)
            if (!target_iface) {
                std::cout << Color::RED << Icon::CROSS << " [ERROR] Interface '" << pname << "' not found on " << sw->get_hostname() << "." << Color::RESET << "\n";
                continue; // Skip this port
            }
        }

        // Apply Config
        if(target_iface) {
            target_iface->vlan_id = vlan_id;
            target_iface->is_trunk = is_trunk;
            target_iface->vlan_name = vname;
            std::cout << Color::GREEN << Icon::CHECK << " Configured " << target_iface->name << " -> " 
                      << (is_trunk ? "TRUNK" : ("VLAN " + std::to_string(vlan_id))) << Color::RESET << "\n";
        }
    }
}

void VlanManager::delete_vlan(std::vector<Device*>& devices) {
    std::cout << "\n" << Color::MAGENTA << "[VLAN Database]" << Color::RESET << "\n";
    std::cout << "ID\tName\n";
    for(auto const& [id, name] : defined_vlans) {
        std::cout << id << "\t" << name << "\n";
    }

    std::cout << "Enter VLAN ID to delete: ";
    int id;
    if (!(std::cin >> id)) {
        clear_in();
        return;
    }
    clear_in();

    if (id == 1) {
        std::cout << Color::RED << Icon::CROSS << " [ERROR] Cannot delete the Default VLAN." << Color::RESET << "\n";
        return;
    }
    if (defined_vlans.find(id) == defined_vlans.end()) {
        std::cout << Color::RED << Icon::CROSS << " [ERROR] VLAN not found." << Color::RESET << "\n";
        return;
    }

    // Safety Reset
    for (auto* dev : devices) {
        for (auto& iface : dev->interfaces) {
            if (iface.vlan_id == id) {
                iface.vlan_id = 1;
                iface.vlan_name = "default";
                iface.is_trunk = false; // Reset to safe state (access vlan 1)
                std::cout << Color::YELLOW << "[INFO] Reset Interface " << iface.name << " on " << dev->get_hostname() << " to VLAN 1." << Color::RESET << "\n";
            }
        }
    }

    defined_vlans.erase(id);
    std::cout << Color::GREEN << Icon::CHECK << " VLAN " << id << " deleted." << Color::RESET << "\n";
}

void VlanManager::menu_manage_vlans(std::vector<Device*>& devices) {
    init(); // Ensure default exists
    
    while(true) {
        std::cout << "\n" << Color::MAGENTA << "--- VLAN Manager ---" << Color::RESET << "\n";
        std::cout << Color::BLUE << "1. " << Color::RESET << "Define VLANs\n";
        std::cout << Color::BLUE << "2. " << Color::RESET << "View VLAN Database\n";
        std::cout << Color::BLUE << "3. " << Color::RESET << "Assign Ports (Batch)\n";
        std::cout << Color::BLUE << "4. " << Color::RESET << "Delete VLAN\n";
        std::cout << Color::BLUE << "5. " << Color::RESET << "Inspect & Reset Switch Ports\n";
        std::cout << Color::BLUE << "0. " << Color::RESET << "Back\n";
        std::cout << "Select: ";
        
        int opt;
        if (!(std::cin >> opt)) { clear_in(); continue; }
        clear_in();
        
        if (opt == 0) return;
        
        if (opt == 1) {
            std::cout << "Enter VLAN ID: ";
            int id; std::cin >> id; clear_in();
            std::cout << "Enter VLAN Name: ";
            std::string name; std::getline(std::cin, name);
            add_vlan(id, name);
        }
        else if (opt == 2) {
            std::cout << "\n" << Color::MAGENTA << "[VLAN Database]" << Color::RESET << "\n";
            std::cout << "ID\tName\n";
            for(auto const& [id, name] : defined_vlans) {
                std::cout << id << "\t" << name << "\n";
            }
        }
        else if (opt == 3) {
            // Select Switch
            std::cout << "Select Switch:\n";
            std::vector<Switch*> switches;
            int idx = 0;
            for(auto d : devices) {
                if (d->get_type() == DeviceType::SWITCH) {
                    std::cout << "[" << idx << "] " << d->get_hostname() << "\n";
                    switches.push_back(dynamic_cast<Switch*>(d));
                    idx++;
                }
            }
            
            if(switches.empty()) {
                std::cout << Color::YELLOW << "No switches available." << Color::RESET << "\n";
                continue;
            }
            
            std::cout << "Select Switch ID: ";
            int s_id;
            if(!(std::cin >> s_id) || s_id < 0 || s_id >= (int)switches.size()) {
                clear_in(); std::cout << Color::RED << "Invalid Switch." << Color::RESET << "\n"; continue;
            }
            clear_in();
            
            Switch* target_sw = switches[s_id];
            
            // Show Status
            std::cout << "\nCurrent Port Status:\n";
            for(const auto& iface : target_sw->interfaces) {
                // Filter meaningful ports (e.g. starting with f or g)
                std::cout << iface.name << ": " 
                          << (iface.is_trunk ? (Color::MAGENTA + "TRUNK" + Color::RESET) : (Color::CYAN + "VLAN " + std::to_string(iface.vlan_id) + " (" + iface.vlan_name + ")" + Color::RESET)) 
                          << "\n";
            }
            
            // Parse Range
            std::cout << "Enter Port Range (e.g. '1-10', '1,2,5'): ";
            std::string range_input;
            std::getline(std::cin, range_input);
            std::vector<std::string> ports = parse_interface_range(range_input);
            std::cout << "Selected " << ports.size() << " ports.\n";
            
            // VLAN ID
            std::cout << "Enter VLAN ID to assign (or 't' for Trunk): ";
            std::string v_input;
            std::cin >> v_input; clear_in();
            
            bool is_trunk = false;
            int v_id = 1;
            
            if (v_input == "t" || v_input == "T") {
                is_trunk = true;
            } else {
                try {
                    v_id = std::stoi(v_input);
                } catch(...) {
                    std::cout << Color::RED << "Invalid ID." << Color::RESET << "\n";
                    continue;
                }
                
                if (!vlan_exists(v_id)) {
                    std::cout << "VLAN " << v_id << " not defined. Create it? (y/n): ";
                    char c; std::cin >> c; clear_in();
                    if (c == 'y' || c == 'Y') {
                        std::cout << "Name for VLAN " << v_id << ": ";
                        std::string vname; std::getline(std::cin, vname);
                        add_vlan(v_id, vname);
                    } else {
                        std::cout << "Cancelled.\n";
                        continue;
                    }
                }
            }
            
            assign_vlan_to_ports(target_sw, ports, v_id, is_trunk);
            std::cout << Color::GREEN << Icon::CHECK << " Ports updated successfully." << Color::RESET << "\n";
        }
        else if (opt == 4) {
            delete_vlan(devices);
        }
        else if (opt == 5) {
            inspect_switch_ports(devices);
        }
    }
}

void VlanManager::inspect_switch_ports(std::vector<Device*>& devices) {
    // 1. Select Switch
    std::cout << "\n" << Color::MAGENTA << "--- Port Inspector ---" << Color::RESET << "\n";
    std::cout << "Select Switch:\n";
    
    std::vector<Switch*> switches;
    int idx = 0;
    for(auto d : devices) {
        if (d->get_type() == DeviceType::SWITCH) {
            std::cout << "[" << idx << "] " << d->get_hostname() << "\n";
            switches.push_back(dynamic_cast<Switch*>(d));
            idx++;
        }
    }
    
    if(switches.empty()) {
        std::cout << Color::YELLOW << Icon::WARN << " No switches available." << Color::RESET << "\n";
        return;
    }
    
    std::cout << "Select Switch ID: ";
    int s_id;
    if(!(std::cin >> s_id) || s_id < 0 || s_id >= (int)switches.size()) {
        clear_in();
        std::cout << Color::RED << Icon::CROSS << " Invalid Switch." << Color::RESET << "\n";
        return;
    }
    clear_in();
    
    Switch* sw = switches[s_id];
    
    // 2. Display Table
    std::cout << "\n" << Color::CYAN << "[PORT INSPECTOR: " << sw->get_hostname() << "]" << Color::RESET << "\n";
    printf("%-12s %-10s %-15s %-12s\n", "Interface", "VLAN ID", "VLAN Name", "Status");
    printf("------------|----------|---------------|------------\n");
    
    for(const auto& iface : sw->interfaces) {
        std::string status;
        std::string row_color;
        
        if (iface.is_trunk) {
            status = "TRUNK";
            row_color = Color::MAGENTA;
        } else if (iface.vlan_id == 1) {
            status = "Default";
            row_color = Color::WHITE;
        } else {
            status = "Assigned";
            row_color = Color::GREEN;
        }
        
        printf("%s%-12s %-10d %-15s [%s]%s\n",
               row_color.c_str(),
               iface.name.c_str(),
               iface.vlan_id,
               iface.vlan_name.empty() ? get_vlan_name(iface.vlan_id).c_str() : iface.vlan_name.c_str(),
               status.c_str(),
               Color::RESET.c_str());
    }
    
    // 3. Action Prompt
    std::cout << "\nEnter Port Name to Reset to VLAN 1 (or 'all' to reset entire switch, 'q' to quit): ";
    std::string input;
    std::getline(std::cin, input);
    
    // Trim
    input.erase(0, input.find_first_not_of(" \t\n\r"));
    input.erase(input.find_last_not_of(" \t\n\r") + 1);
    
    if (input.empty() || input == "q" || input == "Q") {
        return;
    }
    
    // 4. Logic
    if (input == "all" || input == "ALL" || input == "All") {
        // Reset all ports
        for (auto& iface : sw->interfaces) {
            iface.vlan_id = 1;
            iface.vlan_name = "default";
            iface.is_trunk = false;
        }
        std::cout << Color::GREEN << Icon::CHECK << " [SUCCESS] All ports on " << sw->get_hostname() << " reset to default." << Color::RESET << "\n";
    } else {
        // Find specific interface (case-insensitive)
        Interface* target = nullptr;
        for (auto& iface : sw->interfaces) {
            // Case-insensitive comparison
            std::string iface_lower = iface.name;
            std::string input_lower = input;
            std::transform(iface_lower.begin(), iface_lower.end(), iface_lower.begin(), ::tolower);
            std::transform(input_lower.begin(), input_lower.end(), input_lower.begin(), ::tolower);
            
            if (iface_lower == input_lower || iface.name == input) {
                target = &iface;
                break;
            }
        }
        
        if (target) {
            target->vlan_id = 1;
            target->vlan_name = "default";
            target->is_trunk = false;
            std::cout << Color::GREEN << Icon::CHECK << " [SUCCESS] " << target->name << " reset to default." << Color::RESET << "\n";
        } else {
            std::cout << Color::RED << Icon::CROSS << " [ERROR] Interface '" << input << "' not found on " << sw->get_hostname() << "." << Color::RESET << "\n";
        }
    }
}
