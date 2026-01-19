#include <iostream>
#include <vector>
#include <string>
#include <limits>
#include <algorithm>
#include <memory>
#include <sstream>
#include <set>

#include "topology.hpp"
#include "calculator.hpp"
#include "netparser.hpp"
#include "network.hpp"
#include "logging.hpp"  // Keeping existing logging if needed, though prompt implies new CLI
#include "documentation.hpp"
#include "state_manager.hpp"
#include "visualizer.hpp"
#include "utilities.hpp"
#include "vlan_manager.hpp"
#include "presenter.hpp"    // For exam guide
#include "generate_guide.hpp"
#include "gui_layer.hpp"
#include "colors.hpp"

// Global lists to hold our topology
// Global lists to hold our topology
std::vector<Device*> devices;
std::vector<Link*> links;
std::vector<Network*> subnets;

// Helper to find device by name
Device* find_device(const std::string& name) {
    for (auto d : devices) {
        if (d->get_hostname() == name) return d;
    }
    return nullptr;
}

// Helper: Integer to IP String (assuming network.hpp helpers or manual)
// Actually address_to_str is generic in network.hpp.
// We might need to implement get_first_usable if not present in Network class.
std::string get_first_usable_ip(Network* net) {
    int addr = net->get_address();
    return address_to_str(addr + 1);
}

std::string get_mask_str(Network* net) {
    return address_to_str(net->get_mask());
}

std::string get_net_str(Network* net) {
    return address_to_str(net->get_address());
}

void clear_input() {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void trim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

void print_menu() {
    std::cout << Color::MAGENTA << Color::BOLD << "\n--- Cisco Packet Tracer Assistant ---\n" << Color::RESET;
    std::cout << Color::BLUE << "1. " << Color::RESET << Icon::PC << " Add Device\n";
    std::cout << Color::BLUE << "2. " << Color::RESET << Icon::LINK << " Connect Devices\n";
    std::cout << Color::BLUE << "3. " << Color::RESET << "Configure Subnets (VLSM)\n";
    std::cout << Color::BLUE << "4. " << Color::RESET << "Generate Exam Guide\n";
    std::cout << Color::BLUE << "5. " << Color::RESET << "Visualize Topology\n";
    std::cout << Color::BLUE << "6. " << Color::RESET << "Configure Device Security\n";
    std::cout << Color::BLUE << "7. " << Color::RESET << Icon::SWITCH << " VLAN Manager\n";
    std::cout << Color::BLUE << "8. " << Color::RESET << "Load Exam Scenario\n";
    std::cout << Color::BLUE << "9. " << Color::RESET << "Knowledge Base\n";
    std::cout << Color::BLUE << "10. " << Color::RESET << "Save & Exit\n";
    std::cout << Color::BLUE << "11. " << Color::RESET << "Disconnect All\n";
    std::cout << Color::BLUE << "12. " << Color::RESET << "Delete Device\n";
    std::cout << Color::BLUE << "13. " << Color::RESET << "Delete Connection\n";
    std::cout << Color::BLUE << "14. " << Color::RESET << "Password Manager\n";
    std::cout << Color::BLUE << "15. " << Color::RESET << Color::YELLOW << "Factory Reset (Exam Template)" << Color::RESET << "\n";
    std::cout << Color::BLUE << "16. " << Color::RESET << "Network Overview (Subnet-VLAN Map)\n";
    std::cout << Color::BLUE << "17. " << Color::RESET << Color::RED << "🗑️  Nuclear Wipe (Delete Everything)" << Color::RESET << "\n";
    std::cout << Color::BLUE << "18. " << Color::RESET << "🛣️  Configure Static Routes\n";
    std::cout << Color::BLUE << "0. " << Color::RESET << "Exit\n";
    std::cout << "Select: ";
}

void menu_add_device() {
    std::cout << "\n--- Add Device ---\n";
    std::cout << "Type (1=Router, 2=Switch, 3=PC): ";
    int t_int;
    if (!(std::cin >> t_int)) {
        clear_input();
        std::cout << "Invalid input.\n";
        return;
    }
    clear_input();

    std::string input_line;
    std::cout << "Hostnames (comma separated): ";
    std::getline(std::cin, input_line);

    std::stringstream ss(input_line);
    std::string name;
    
    int added_count = 0;
    while (std::getline(ss, name, ',')) {
        trim(name);
        if (name.empty()) continue;

        if (find_device(name)) {
            std::cout << Color::RED << Icon::CROSS << " Error: Device '" << name << "' already exists! Skipping." << Color::RESET << "\n";
            continue;
        }

        Device* new_dev = nullptr;
        if (t_int == 1) new_dev = new Router(name);
        else if (t_int == 2) new_dev = new Switch(name);
        else if (t_int == 3) new_dev = new PC(name);
        else {
            std::cout << "Invalid type selected.\n";
            return;
        }

        devices.push_back(new_dev);
        std::cout << Color::GREEN << Icon::CHECK << " Added " << name << "." << Color::RESET << "\n";
        added_count++;
    }
    
    if (added_count == 0 && t_int >= 1 && t_int <= 3) {
         std::cout << "No valid devices added.\n";
    }
}

void menu_connect_devices() {
    std::cout << "\n--- Connect Devices (Batch) ---\n";
    if (devices.size() < 2) {
        std::cout << "Need at least 2 devices.\n";
        return;
    }

    // 1. Show Devices
    std::cout << "Available Devices:\n";
    for (size_t i = 0; i < devices.size(); ++i) {
        std::string type_str;
        switch (devices[i]->get_type()) {
            case DeviceType::ROUTER: type_str = "ROUTER"; break;
            case DeviceType::SWITCH: type_str = "SWITCH"; break;
            case DeviceType::PC: type_str = "PC"; break;
        }
        std::cout << "[" << i << "] " << devices[i]->get_hostname() << " (" << type_str << ")\n";
    }

    // 2. Select Sources
    std::cout << "Enter Source Device IDs (space separated): ";
    std::string line;
    std::getline(std::cin, line);
    std::stringstream ss(line);
    int id;
    std::vector<int> source_ids;
    while (ss >> id) {
        if (id >= 0 && id < (int)devices.size()) {
            source_ids.push_back(id);
        } else {
            std::cout << "Warning: Invalid ID " << id << " ignored.\n";
        }
    }

    if (source_ids.empty()) {
        std::cout << "No valid source IDs entered.\n";
        return;
    }

    // 3. Select Target
    std::cout << "Enter Target Device ID: ";
    int target_id;
    if (!(std::cin >> target_id)) {
        clear_input();
        std::cout << "Invalid input for target.\n";
        return;
    }
    clear_input(); // consume newline

    if (target_id < 0 || target_id >= (int)devices.size()) {
        std::cout << "Invalid target ID.\n";
        return;
    }

    Device* target_dev = devices[target_id];
    std::cout << "Connecting " << source_ids.size() << " device(s) to " << target_dev->get_hostname() << "...\n";

    // Ask for start port
    std::cout << "Enter starting port on " << target_dev->get_hostname() << " (or press ENTER for auto-assign): ";
    std::string current_target_port;
    std::getline(std::cin, current_target_port);
    trim(current_target_port);

    // Ask for preferred source port
    std::cout << "Enter preferred port for Source (or ENTER for auto): ";
    std::string preferred_source_port;
    std::getline(std::cin, preferred_source_port);
    trim(preferred_source_port);

    // 4. Connection Loop
    for (int src_id : source_ids) {
        if (src_id == target_id) {
            std::cout << "Skipping connection: Cannot connect device to itself (ID " << src_id << ").\n";
            continue;
        }

        Device* source_dev = devices[src_id];

        // Find available ports logic for SOURCE
        // Helper to get first available port for source
        // Helper to get first available port for source
        auto get_source_port_auto = [](Device* d) -> std::string {
            std::vector<std::string> ports = d->get_available_ports();
            if (ports.empty()) return "";
            if (d->get_type() == DeviceType::PC) {
                for (const auto& p : ports) {
                    if (p == "Fa0") return p;
                }
            }
            return ports[0];
        };

        std::string p_source;
        if (preferred_source_port.empty()) {
             p_source = get_source_port_auto(source_dev);
        } else {
             p_source = preferred_source_port;
             // Check if valid/free
             Interface* iff = source_dev->get_interface(p_source);
             if (!iff) {
                 std::cout << Color::RED << Icon::CROSS << " [ERROR] Port " << p_source << " not found on " << source_dev->get_hostname() << Color::RESET << "\n";
                 continue;
             }
             if (iff->is_connected) {
                 std::cout << Color::YELLOW << Icon::WARN << " [ERROR] Port " << p_source << " on " << source_dev->get_hostname() << " is busy." << Color::RESET << "\n";
                 continue;
             }
        }

        if (p_source.empty()) {
            std::cout << "[ERROR] No free ports on " << source_dev->get_hostname() << ". Skipping.\n";
            continue;
        }

        // Determine TARGET port
        std::string p_target;
        if (current_target_port.empty()) {
            // Auto-assign existing logic
            auto get_auto_port = [](Device* d) -> std::string {
                std::vector<std::string> ports = d->get_available_ports();
                if (ports.empty()) return "";
                return ports[0];
            };
            p_target = get_auto_port(target_dev);
        } else {
            // Use specific port
            p_target = current_target_port;
            
            // Validate existence and availability
            Interface* iface = target_dev->get_interface(p_target);
            if (!iface) {
                std::cout << Color::RED << Icon::CROSS << " [ERROR] Port " << p_target << " does not exist on " << target_dev->get_hostname() << "." << Color::RESET << "\n";
                break; // Stop batch
            }
            if (iface->is_connected) {
                std::cout << Color::YELLOW << Icon::WARN << " [ERROR] Port " << p_target << " is already in use." << Color::RESET << "\n";
                // Try to find next AVAILABLE one? Or just hard stop as per "Sequential Strategy"?
                // Prompt implies: increment it. If incremented doesn't exist, stop.
                // If it exists but is busy? Usually stop or skip. Let's stop to be safe.
                break; 
            }
        }

        if (p_target.empty()) {
            std::cout << "[ERROR] " << target_dev->get_hostname() << " is full. Stopping.\n";
            break; 
        }

        // Perform connection
        Link* l = new Link(source_dev, p_source, target_dev, p_target);
        links.push_back(l);

        std::cout << Color::GREEN << Icon::LINK << " [SUCCESS] Connected " << source_dev->get_hostname() << " (" << p_source << ") <--> " 
                  << target_dev->get_hostname() << " (" << p_target << ")" << Color::RESET << "\n";
        
        // Prepare next target port if using Manual Mode
        if (!current_target_port.empty()) {
             current_target_port = increment_port(current_target_port);
        }
    }
}

// Helper to get number of hosts supported by a network mask
int get_hosts_capacity(Network* net) {
    if (!net) return 0;
    int slash = net->get_slash();
    // 32 - slash = host bits. Hosts = 2^bits - 2.
    int host_bits = 32 - slash;
    if (host_bits <= 0) return 0;
    unsigned long long max_hosts = (1ULL << host_bits) - 2;
    return (int)max_hosts;
}

// Helper to find network by ID
Network* get_net_by_id(const std::vector<Network*>& subnets, int id) {
    for (auto n : subnets) {
        if (n->id == id) return n;
    }
    return nullptr;
}

void print_subnet_recursive(const std::vector<Network*>& subnets, Network* current, std::string prefix) {
    std::string s_net = get_net_str(current);
    int slash = current->get_slash();
    int caps = get_hosts_capacity(current);
    std::string net_display = s_net + "/" + std::to_string(slash);
    
    std::string status = current->get_assignment();
    if (current->is_split) status = "SPLIT";
    
    std::string n = current->name.empty() ? "-" : current->name;
    
    // Determine connector
    std::string connector = (current->parent_id == 0) ? "" : "└── ";
    
    // DHCP Status Tag
    std::string dhcp_tag = "";
    if (!current->is_split && slash < 30) {  // Only for LANs, not WANs or splits
        if (current->dhcp_enabled) {
            if (current->dhcp_helper_ip.empty()) {
                dhcp_tag = std::string(Color::GREEN) + "[DHCP: Server]" + Color::RESET;
            } else {
                dhcp_tag = std::string(Color::YELLOW) + "[DHCP: Relay]" + Color::RESET;
            }
        } else if (!status.empty() && status != "Free") {
            dhcp_tag = std::string(Color::CYAN) + "[Static]" + Color::RESET;
        }
    }

    printf("%s%s[%d]  %-18s (%-6d) %-15s [%s] %s\n", 
           prefix.c_str(), connector.c_str(), current->id, net_display.c_str(), caps, n.c_str(), status.c_str(), dhcp_tag.c_str());

    // Recurse
    std::string next_prefix = (current->parent_id == 0) ? prefix : prefix + "    ";
    for (int cid : current->children_ids) {
        Network* child = get_net_by_id(subnets, cid);
        if (child) {
            print_subnet_recursive(subnets, child, next_prefix);
        }
    }
}

void menu_configure_subnets(); 

std::string find_server_ip_for_relay(int server_router_id) {
    if (server_router_id < 0 || server_router_id >= (int)devices.size()) return "";
    Device* server_dev = devices[server_router_id];
    if (server_dev->get_type() != DeviceType::ROUTER) return "";

    // Priority 1: Check if server is reachable via WAN (/30)
    for (auto n : subnets) {
        if (n->is_split) continue;
        if (n->get_slash() == 30) {
            std::string assign = n->get_assignment();
            // A. Server is the owner of this /30
            if (assign.find(server_dev->get_hostname()) != std::string::npos) {
                return address_to_str(n->get_address() + 1);
            }
            // B. Server is the peer (connected to owner)
            for(auto dev : devices) {
                if(dev->get_type() == DeviceType::ROUTER && dev != server_dev) {
                    if(assign.find(dev->get_hostname()) != std::string::npos) {
                        // This dev owns the subnet. Is it connected to server?
                        for(auto l : links) {
                            if((l->device1 == server_dev && l->device2 == dev) || 
                               (l->device1 == dev && l->device2 == server_dev)) {
                                return address_to_str(n->get_address() + 2);
                            }
                        }
                    }
                }
            }
        }
    }

    // Priority 2: Return any valid interface IP on the server
    for (auto n : subnets) {
        if (n->is_split) continue;
        if (n->get_assignment().find(server_dev->get_hostname()) != std::string::npos) {
            return address_to_str(n->get_address() + 1);
        }
    }

    return "";
}

void menu_configure_subnets() {
    std::cout << "\n--- Configure Subnets ---\n";
    
    int next_id_counter = 1;
    
    // Check if we have existing subnets
    if (!subnets.empty()) {
        std::cout << Color::YELLOW << "[INFO] Existing subnet configuration found (" << subnets.size() << " subnets)." << Color::RESET << "\n";
        std::cout << "(E)dit existing or (N)ew configuration? (e/n): ";
        char choice;
        std::cin >> choice;
        clear_input();
        
        if (choice == 'E' || choice == 'e') {
            // Resume editing - skip base network wizard
            std::cout << Color::GREEN << Icon::CHECK << " Resuming existing configuration..." << Color::RESET << "\n";
            
            // Find the highest existing ID for the counter
            for (auto n : subnets) {
                if (n->id >= next_id_counter) {
                    next_id_counter = n->id + 1;
                }
            }
            // Jump directly to the interactive loop (handled below)
        } else {
            // Start new configuration
            std::cout << Color::YELLOW << Icon::WARN << " Starting new configuration. Clearing existing data..." << Color::RESET << "\n";
            
            // Clear existing subnets
            for (auto n : subnets) {
                delete n;
            }
            subnets.clear();
            
            // Continue to base network wizard below
            goto new_config;
        }
    } else {
        // No existing subnets, run the wizard
        new_config:
        
        std::cout << "Enter Base Network (e.g. 192.168.1.0/24): ";
        std::string net_str;
        std::getline(std::cin, net_str);

        Network* base_net = nullptr;
        
        try {
            NetParser parser(net_str);
            base_net = parser.get_network();
        
            if (!base_net) {
                std::cout << "Invalid network pointer returned.\n";
                return;
            }

            Calculator calc(base_net);
            std::cout << "Mode (N for Number of Subnets, H for Hosts per Subnet): ";
            char mode;
            std::cin >> mode; 
            clear_input();
            
            int req;
            if (mode == 'H' || mode == 'h') {
                 std::cout << "Enter required hosts per subnet: ";
                 std::cin >> req; clear_input();
                 subnets = calc.subnet_by_hosts(req);
            } else {
                 std::cout << "Enter required number of subnets: ";
                 std::cin >> req; clear_input();
                 subnets = calc.subnet_by_networks(req);
            }
            
            // Init IDs
            for(auto n : subnets) {
                n->id = next_id_counter++;
                n->parent_id = 0;
                n->children_ids.clear();
            }
            
            std::cout << "Generated " << subnets.size() << " subnets.\n";
            
        } catch (const std::exception& e) {
            std::cout << "[ERROR] " << e.what() << "\n";
            return;
        }
    }

    // Interactive Loop
    while (true) {
        std::cout << "\n--- Generated Subnets (Tree View) ---\n";
        printf("%-4s %-18s %-8s %-15s %s\n", "ID", "Network", "Hosts", "Name", "Status");
        
        // Print Tree
        for (auto n : subnets) {
            if (n->parent_id == 0) {
                print_subnet_recursive(subnets, n, "");
            }
        }
        
        std::cout << "\nEnter Subnet ID to manage (or 0 to finish): ";
        int choice_id;
        if (!(std::cin >> choice_id)) {
            clear_input(); continue;
        }
        clear_input();
        
        if (choice_id == 0) break;
        
        Network* selected_net = get_net_by_id(subnets, choice_id);
        if (!selected_net) {
            std::cout << "Invalid ID.\n";
            continue;
        }
        
        if (selected_net->is_split) {
            std::cout << "[ERROR] This subnet has been split. Manage its children instead.\n";
            continue;
        }

        std::string s_net = get_net_str(selected_net);
        std::string s_mask = get_mask_str(selected_net);
        
        std::cout << "Selected: " << s_net << "/" << selected_net->get_slash() << "\n";
        std::cout << "Action: (A)ssign, (S)plit, (R)ename, (C)ancel: ";
        char action;
        std::cin >> action; clear_input();

        if (action == 'R' || action == 'r') {
             std::cout << "Enter name for " << s_net << " (e.g. LAN A): ";
             std::string new_name;
             std::getline(std::cin, new_name);
             selected_net->name = new_name;
             continue;
        }
        else if (action == 'S' || action == 's') {
            // VLSM Logic
            std::cout << "--- VLSM Split ---\n";
            std::cout << "Enter new host requirement (e.g. 2 for WAN links): ";
            int new_hosts;
            if (!(std::cin >> new_hosts)) { clear_input(); continue; }
            clear_input();

            try {
                Calculator sub_calc(selected_net);
                std::vector<Network*> new_children = sub_calc.subnet_by_hosts(new_hosts);
                
                // Assign IDs and Link to Parent
                for(auto child : new_children) {
                    child->id = next_id_counter++;
                    child->parent_id = selected_net->id;
                    child->children_ids.clear();
                    
                    selected_net->children_ids.push_back(child->id);
                }
                
                // Add new children to the main list
                subnets.insert(subnets.end(), new_children.begin(), new_children.end());
                
                // Mark parent as split
                selected_net->is_split = true;
                selected_net->set_assignment("Split (VLSM Parent)");
                
                std::cout << "Successfully split into " << new_children.size() << " new subnets.\n";
                
                std::cout << "Do you want to name these new subnets now? (y/n): ";
                char c; std::cin >> c; clear_input();
                if(c == 'y' || c == 'Y') {
                    for(size_t k = 0; k < new_children.size(); ++k) {
                        std::string temp_net = get_net_str(new_children[k]);
                        std::cout << "Name for " << temp_net << "/" << new_children[k]->get_slash() << ": ";
                        std::string nm; std::getline(std::cin, nm);
                        if(!nm.empty()) new_children[k]->name = nm; 
                    }
                }
                
            } catch (const std::exception& e) {
                std::cout << "[ERROR] Splitting failed: " << e.what() << "\n";
            }
            continue;
        } 
        else if (action == 'C' || action == 'c') {
            continue;
        }
        else if (action == 'A' || action == 'a') {
            // Assignment Logic
            std::cout << "Assign to:\n";
            std::cout << "(R)outer Interface\n(S)witch VLAN\n(C)ancel\nChoice: ";
            char type;
            std::cin >> type; clear_input();
            
            if (type == 'R' || type == 'r') {
                std::cout << "Which Router? ";
                std::string rname; std::cin >> rname; clear_input();
                Device* d = find_device(rname);
                if (d && d->get_type() == DeviceType::ROUTER) {
                    Router* r = dynamic_cast<Router*>(d);
                    
                    // Get current router's index
                    int current_router_idx = -1;
                    {
                        int idx = 0;
                        for (auto dev : devices) {
                            if (dev->get_type() == DeviceType::ROUTER) {
                                if (dev == d) {
                                    current_router_idx = idx;
                                    break;
                                }
                                idx++;
                            }
                        }
                    }
                    
                    // INTERFACE PROMPT - Store exactly as typed
                    std::cout << "Enter Router Interface (e.g. Gig0/1, Se0/1/0): ";
                    std::string iface_str; std::cin >> iface_str; clear_input();
                    
                    // VLAN Association
                    std::cout << "Associate with VLAN ID? (Enter 0 for physical/none): ";
                    int vlan_assoc = 0;
                    if (!(std::cin >> vlan_assoc)) { clear_input(); vlan_assoc = 0; }
                    clear_input();
                    
                    selected_net->associated_vlan_id = vlan_assoc;
                    
                    // Auto-naming from VLAN
                    if (vlan_assoc > 0 && selected_net->name.empty()) {
                        if (VlanManager::vlan_exists(vlan_assoc)) {
                            selected_net->name = VlanManager::get_vlan_name(vlan_assoc);
                            std::cout << Color::GREEN << Icon::CHECK << " Auto-named: " << selected_net->name << Color::RESET << "\n";
                        }
                    }
                    
                    // Build final interface name (user's base + VLAN subinterface if applicable)
                    std::string final_iface = iface_str;
                    if (vlan_assoc > 0) {
                        // Strip any existing .X suffix and add correct VLAN suffix
                        size_t dot_pos = iface_str.find('.');
                        if (dot_pos != std::string::npos) {
                            iface_str = iface_str.substr(0, dot_pos);
                        }
                        final_iface = iface_str + "." + std::to_string(vlan_assoc);
                    }
                    
                    // Store assignment
                    selected_net->set_assigned_interface(final_iface);
                    selected_net->set_assignment("Assigned: " + rname + " - " + final_iface);
                    
                    // DHCP Configuration (only for non-WAN subnets)
                    int cidr = selected_net->get_slash();
                    if (cidr < 30) {
                        std::cout << "\nEnable DHCP for this subnet? (y/n): ";
                        char dhcp_choice;
                        std::cin >> dhcp_choice; clear_input();
                        
                        if (dhcp_choice == 'y' || dhcp_choice == 'Y') {
                            selected_net->dhcp_enabled = true;
                            std::cout << Color::GREEN << Icon::CHECK << " DHCP enabled." << Color::RESET << "\n";
                            
                            // LOCAL or REMOTE DHCP Server
                            std::cout << "Is the DHCP Server Local (this router) or Remote? (L/R): ";
                            char lr_choice;
                            std::cin >> lr_choice; clear_input();
                            
                            if (lr_choice == 'R' || lr_choice == 'r') {
                                // REMOTE DHCP Server
                                std::cout << "\nAvailable Remote Routers:\n";
                                int router_idx = 0;
                                for (auto dev : devices) {
                                    if (dev->get_type() == DeviceType::ROUTER && dev != d) {
                                        std::cout << "  [" << router_idx << "] " << dev->get_hostname() << "\n";
                                    }
                                    if (dev->get_type() == DeviceType::ROUTER) router_idx++;
                                }
                                
                                std::cout << "Enter DHCP Server Router ID (or -1 for manual IP): ";
                                int remote_id = -1;
                                if (!(std::cin >> remote_id)) { clear_input(); remote_id = -1; }
                                clear_input();
                                
                                if (remote_id == -1) {
                                    std::cout << "Enter IP of Remote DHCP Server (for ip helper-address): ";
                                    std::string helper_ip;
                                    std::cin >> helper_ip; clear_input();
                                    selected_net->dhcp_server_id = -1; 
                                    selected_net->dhcp_helper_ip = helper_ip;
                                } else {
                                     if(remote_id >= 0 && remote_id < (int)devices.size() && devices[remote_id]->get_type() == DeviceType::ROUTER) {
                                         selected_net->dhcp_server_id = remote_id;
                                         std::string auto_ip = find_server_ip_for_relay(remote_id);
                                         
                                         if(!auto_ip.empty()) {
                                             selected_net->dhcp_helper_ip = auto_ip;
                                             std::cout << Color::GREEN << "✅ Auto-Resolved Server IP: " << auto_ip << Color::RESET << "\n";
                                         } else {
                                             std::cout << Color::RED << "❌ Could not find an IP for Router " << remote_id << ". Please enter manually." << Color::RESET << "\n";
                                             std::cout << "Enter IP: ";
                                             std::string helper_ip;
                                             std::cin >> helper_ip; clear_input();
                                             selected_net->dhcp_helper_ip = helper_ip;
                                         }
                                         
                                          std::cout << Color::YELLOW << Icon::WARN << " Centralized DHCP configured. " 
                                              << "Helper-address: " << selected_net->dhcp_helper_ip << Color::RESET << "\n";
                                     } else {
                                         std::cout << "Invalid Router ID. Using local config logic or disabling.\n";
                                         // Fallback if invalid
                                         selected_net->dhcp_server_id = current_router_idx;
                                         selected_net->dhcp_helper_ip = "";
                                     }
                                }
                            } else {
                                // LOCAL DHCP Server
                                selected_net->dhcp_server_id = current_router_idx;
                                selected_net->dhcp_helper_ip = "";
                                std::cout << Color::GREEN << Icon::CHECK << " DHCP served locally by " << rname << "." << Color::RESET << "\n";
                            }
                            
                            // Exam Mode (Upper Half Only)
                            std::cout << "Restrict pool to 'Highest Half' of addresses (Exam Mode)? (y/n): ";
                            char half_choice;
                            std::cin >> half_choice; clear_input();
                            
                            selected_net->dhcp_upper_half_only = (half_choice == 'y' || half_choice == 'Y');
                            if (selected_net->dhcp_upper_half_only) {
                                std::cout << Color::YELLOW << Icon::WARN << " Exam Mode: Lower half excluded." << Color::RESET << "\n";
                            }
                        } else {
                            // DHCP Disabled
                            selected_net->dhcp_enabled = false;
                            selected_net->dhcp_server_id = -1;
                            selected_net->dhcp_helper_ip = "";
                            selected_net->dhcp_upper_half_only = false;
                            std::cout << "DHCP disabled (Static IPs).\n";
                        }
                    } else {
                        // WAN links - no DHCP
                        selected_net->dhcp_enabled = false;
                        selected_net->dhcp_server_id = -1;
                        selected_net->dhcp_helper_ip = "";
                        selected_net->dhcp_upper_half_only = false;
                    }
                    
                    // Confirmation
                    std::cout << Color::GREEN << Icon::CHECK << " Assigned " << selected_net->name 
                              << " to " << rname << " " << final_iface << "." << Color::RESET << "\n";
                    
                } else {
                    std::cout << Color::RED << Icon::CROSS << " Router not found." << Color::RESET << "\n";
                }
            } else if (type == 'S' || type == 's') {
                std::cout << "Which Switch? ";
                std::string sname; std::cin >> sname; clear_input();
                Device* d = find_device(sname);
                if (d && d->get_type() == DeviceType::SWITCH) {
                    Switch* sw = dynamic_cast<Switch*>(d);
                    std::cout << "Enter VLAN ID (e.g. 10): ";
                    std::string v_input; std::cin >> v_input; clear_input();
                    
                    int vid = 1; 
                    try { vid = std::stoi(v_input); } catch(...) {}
                    
                    // Default name to subnet name if available
                    std::cout << "VLAN Name (e.g. DATA) [Default: " << (selected_net->name.empty() ? "VLAN"+v_input : selected_net->name) << "]: ";
                    std::string vname; std::getline(std::cin, vname); // Use getline for empty check
                    if (vname.empty()) vname = (selected_net->name.empty() ? "VLAN"+v_input : selected_net->name);
                    
                    sw->add_vlan(vid, vname);
                    
                    // We store "VLAN " + input as the interface string representation
                    selected_net->set_assigned_interface("VLAN " + v_input);
                    selected_net->set_assignment("Assigned: " + sname + " - VLAN " + v_input);
                    std::cout << "Assigned!\n";
                } else {
                    std::cout << "Switch not found.\n";
                }
            }
        }
    }
}

// Helper: Find first matching physical interface on device
std::string normalize_interface(Device* dev, const std::string& assigned_iface) {
    // Strip any subinterface suffix first (e.g., "g0/0.10" -> "g0/0")
    std::string base_iface = assigned_iface;
    size_t dot_pos = base_iface.find('.');
    if (dot_pos != std::string::npos) {
        base_iface = base_iface.substr(0, dot_pos);
    }
    
    // Try exact match first
    if (dev->get_interface(base_iface)) {
        return base_iface;
    }
    
    // Normalize common prefixes
    std::string prefix_lower = base_iface;
    std::transform(prefix_lower.begin(), prefix_lower.end(), prefix_lower.begin(), ::tolower);
    
    // Build prefix to match
    std::string target_prefix;
    if (prefix_lower.find("gig") == 0 || prefix_lower.find("g0") == 0 || prefix_lower.find("gi") == 0) {
        target_prefix = "gig";
    } else if (prefix_lower.find("fa") == 0 || prefix_lower.find("f0") == 0) {
        target_prefix = "fa";
    } else if (prefix_lower.find("se") == 0 || prefix_lower.find("s0") == 0) {
        target_prefix = "se";
    }
    
    // Search for matching interface on device
    for (auto& iface : dev->interfaces) {
        std::string iface_lower = iface.name;
        std::transform(iface_lower.begin(), iface_lower.end(), iface_lower.begin(), ::tolower);
        
        if (!target_prefix.empty() && iface_lower.find(target_prefix) == 0) {
            return iface.name; // Return actual interface name
        }
    }
    
    // Fallback: return original (might need manual correction)
    return base_iface;
}

// Helper: Calculate upper-half exclusion range
void print_dhcp_exclusions(const std::string& network_ip, const std::string& mask_str) {
    // Parse mask to CIDR
    int cidr = 0;
    unsigned int mask_int = 0;
    {
        int segments[4];
        sscanf(mask_str.c_str(), "%d.%d.%d.%d", &segments[0], &segments[1], &segments[2], &segments[3]);
        mask_int = (segments[0] << 24) | (segments[1] << 16) | (segments[2] << 8) | segments[3];
    }
    for (int i = 0; i < 32; ++i) {
        if ((mask_int >> (31 - i)) & 1) cidr++;
        else break;
    }
    
    // Parse network to int
    unsigned int net_int = 0;
    {
        int segments[4];
        sscanf(network_ip.c_str(), "%d.%d.%d.%d", &segments[0], &segments[1], &segments[2], &segments[3]);
        net_int = (segments[0] << 24) | (segments[1] << 16) | (segments[2] << 8) | segments[3];
    }
    
    // Calculate midpoint
    unsigned long long total_addresses = (1ULL << (32 - cidr));
    unsigned int midpoint = total_addresses / 2;
    unsigned int start_ex = net_int + 1;
    unsigned int end_ex = net_int + midpoint - 1;
    
    std::cout << "ip dhcp excluded-address " << address_to_str(start_ex) << " " << address_to_str(end_ex) << "\n";
}


void menu_configure_security() {
    std::cout << "\n--- Configure Device Security ---\n";
    std::cout << "Select Device to configure:\n";
    if (devices.empty()) {
        std::cout << "No devices available.\n";
        return;
    }
    for (size_t i = 0; i < devices.size(); ++i) {
        std::cout << "[" << i << "] " << devices[i]->get_hostname() << "\n";
    }
    int id;
    std::cout << "Device ID: ";
    if (!(std::cin >> id) || id < 0 || id >= (int)devices.size()) {
        clear_input();
        std::cout << "Invalid ID.\n";
        return;
    }
    clear_input();

    Device* d = devices[id];
    std::cout << "Configuring " << d->get_hostname() << "...\n";

    // 1. Management IP
    std::cout << "Enter Management VLAN IP (Leave empty to skip): ";
    std::string mgmt_ip;
    std::getline(std::cin, mgmt_ip);
    trim(mgmt_ip);
    if (!mgmt_ip.empty()) {
        d->management_config.management_svi_ip = mgmt_ip;
    }

    // 2. Default Gateway
    std::cout << "Enter Default Gateway (Leave empty to skip): ";
    std::string gw;
    std::getline(std::cin, gw);
    trim(gw);
    if (!gw.empty()) {
        d->management_config.management_gateway = gw;
    }

    // 3. Remote Access Restriction
    std::cout << "Restrict Remote Access to specific IP? (Enter IP or press ENTER for any): ";
    std::string remote_ip;
    std::getline(std::cin, remote_ip);
    trim(remote_ip);
    d->management_config.allowed_telnet_ip = remote_ip; // Empty means "any" equivalent (no ACL restriction)

    // 4. Transport Type
    std::cout << "Use Telnet or SSH? (t/s): ";
    std::string type;
    std::getline(std::cin, type);
    if (type == "s" || type == "S" || type == "ssh") {
        d->management_config.enable_telnet = false;
    } else {
        d->management_config.enable_telnet = true; // Default to telnet
    }
    
    std::cout << "Security configuration saved for " << d->get_hostname() << ".\n";
}

void load_exam_scenario() {
    std::cout << Color::MAGENTA << "\n--- Load Exam Scenario ---" << Color::RESET << "\n";
    std::cout << "Enter scenario file to load (default: network_save.dat): ";
    
    std::string filename;
    std::getline(std::cin, filename);
    trim(filename);
    
    if (filename.empty()) {
        filename = "network_save.dat";
    }
    
    bool success = StateManager::load_scenario(filename, devices, links, subnets);
    
    if (success) {
        std::cout << Color::GREEN << Icon::CHECK << " [SUCCESS] Topology, VLANs, and Subnets restored." << Color::RESET << "\n";
        std::cout << "  " << Icon::ROUTER << " Devices: " << devices.size() << "\n";
        std::cout << "  " << Icon::LINK << " Connections: " << links.size() << "\n";
        std::cout << "  VLANs: " << VlanManager::defined_vlans.size() << "\n";
        std::cout << "  Subnets: " << subnets.size() << "\n";
    } else {
        std::cout << Color::RED << Icon::CROSS << " [ERROR] Failed to load scenario." << Color::RESET << "\n";
    }
}

void load_exam_template() {
    std::cout << Color::MAGENTA << "\n--- Load Golden Exam Scenario ---" << Color::RESET << "\n";
    std::cout << Color::YELLOW << Icon::WARN << " This will ERASE all current data and load the exam subnets!" << Color::RESET << "\n";
    std::cout << "Continue? (y/n): ";
    
    char c;
    if (!(std::cin >> c)) {
        clear_input();
        return;
    }
    clear_input();
    
    if (c != 'y' && c != 'Y') {
        std::cout << "Operation cancelled.\n";
        return;
    }
    
    // 1. Clear Existing Data
    for (auto d : devices) delete d;
    devices.clear();
    for (auto l : links) delete l;
    links.clear();
    for (auto n : subnets) delete n;
    subnets.clear();
    VlanManager::defined_vlans.clear();
    VlanManager::init();

    // 2. Create Topology Devices
    Router* router0 = new Router("Router0");
    Router* router1 = new Router("Router1");
    devices.push_back(router0);
    devices.push_back(router1);
    
    Switch* switch0 = new Switch("Switch0");
    Switch* switch1 = new Switch("Switch1");
    Switch* switch2 = new Switch("Switch2");
    devices.push_back(switch0);
    devices.push_back(switch1);
    devices.push_back(switch2);
    
    PC* pc0 = new PC("PC0");
    PC* laptop0 = new PC("Laptop0");
    PC* pc1 = new PC("PC1");
    PC* laptop1 = new PC("Laptop1");
    PC* pc2 = new PC("PC2");
    PC* laptop2 = new PC("Laptop2");
    devices.push_back(pc0);
    devices.push_back(laptop0);
    devices.push_back(pc1);
    devices.push_back(laptop1);
    devices.push_back(pc2);
    devices.push_back(laptop2);
    
    // 3. Create Links (Optimized for Exam Guide)
    links.push_back(new Link(router0, "Gig0/1", switch0, "Gig0/1"));
    links.push_back(new Link(router1, "Gig0/1", switch1, "Gig0/1"));
    links.push_back(new Link(switch0, "Gig0/2", switch2, "Gig0/2"));
    links.push_back(new Link(router0, "Se0/1/0", router1, "Se0/1/0"));
    
    links.push_back(new Link(pc0, "Fa0", switch0, "Fa0/1"));
    links.push_back(new Link(laptop0, "Fa0", switch0, "Fa0/2"));
    links.push_back(new Link(pc1, "Fa0", switch1, "Fa0/1"));
    links.push_back(new Link(laptop1, "Fa0", switch1, "Fa0/2"));
    links.push_back(new Link(pc2, "Fa0", switch2, "Fa0/1"));
    links.push_back(new Link(laptop2, "Fa0", switch2, "Fa0/2"));

    // 4. Define Golden VLANs
    VlanManager::add_vlan(10, "LAN_A");
    VlanManager::add_vlan(20, "LAN_B");

    // 5. Define Golden Subnets
    
    // LAN A: 192.168.1.32/27, VLAN 10, DHCP via Router1
    Network* lanA = new Network();
    lanA->id = 1; lanA->name = "LAN A";
    lanA->set_address(str_to_address("192.168.1.32"));
    lanA->set_slash(27);
    lanA->set_mask((~0u) << (32 - 27));
    lanA->set_assignment("Router0");
    lanA->set_assigned_interface("Gig0/1.10");
    lanA->associated_vlan_id = 10;
    lanA->dhcp_enabled = true;
    lanA->dhcp_server_id = 1; // Served by Router1
    lanA->dhcp_helper_ip = "192.168.1.130";
    lanA->dhcp_upper_half_only = true;
    subnets.push_back(lanA);

    // LAN B: 192.168.1.64/27, VLAN 20, DHCP via Router1
    Network* lanB = new Network();
    lanB->id = 2; lanB->name = "LAN B";
    lanB->set_address(str_to_address("192.168.1.64"));
    lanB->set_slash(27);
    lanB->set_mask((~0u) << (32 - 27));
    lanB->set_assignment("Router0");
    lanB->set_assigned_interface("Gig0/1.20");
    lanB->associated_vlan_id = 20;
    lanB->dhcp_enabled = true;
    lanB->dhcp_server_id = 1; // Served by Router1
    lanB->dhcp_helper_ip = "192.168.1.130";
    lanB->dhcp_upper_half_only = true;
    subnets.push_back(lanB);

    // LAN C: 192.168.1.96/27, Physical LAN on Router1
    Network* lanC = new Network();
    lanC->id = 3; lanC->name = "LAN C";
    lanC->set_address(str_to_address("192.168.1.96"));
    lanC->set_slash(27);
    lanC->set_mask((~0u) << (32 - 27));
    lanC->set_assignment("Router1");
    lanC->set_assigned_interface("Gig0/1");
    lanC->associated_vlan_id = 1; // Physical
    lanC->dhcp_enabled = false; // Static
    subnets.push_back(lanC);

    // LAN D (WAN): 192.168.1.128/30
    Network* lanD = new Network();
    lanD->id = 4; lanD->name = "LAN D";
    lanD->set_address(str_to_address("192.168.1.128"));
    lanD->set_slash(30);
    lanD->set_mask((~0u) << (32 - 30));
    lanD->set_assignment("Router0");
    lanD->set_assigned_interface("Se0/1/0");
    lanD->associated_vlan_id = 0;
    lanD->dhcp_enabled = false;
    subnets.push_back(lanD);

    // 6. Configure VLAN Port Assignments
    // Switch0 (Connects to Router0)
    VlanManager::assign_vlan_to_ports(switch0, {"Fa0/1"}, 10, false);
    VlanManager::assign_vlan_to_ports(switch0, {"Fa0/2"}, 20, false);

    // Switch2 (Connects to Switch0)
    VlanManager::assign_vlan_to_ports(switch2, {"Fa0/1"}, 10, false);
    VlanManager::assign_vlan_to_ports(switch2, {"Fa0/2"}, 20, false);

    // Switch1 (Connects to Router1)
    // Physical network (Default VLAN 1)

    // 7. Feedback
    std::cout << "\n" << Color::GREEN << "✅ Exam Template & VLANs Loaded Successfully!" << Color::RESET << "\n";
    std::cout << Color::YELLOW << "Note: Devices have been reset to the specific exam scenario." << Color::RESET << "\n";
    std::cout << "  - Routers: 2, Switches: 3, PCs: 6\n";
    std::cout << "  - Subnets: 4 (LAN A, B, C, D)\n";
    std::cout << "  - VLAN Assignments: Switch0/Switch2 Ports configured for VLAN 10/20\n";
}

void show_network_overview() {
    std::cout << "\n" << Color::MAGENTA << Color::BOLD << "=== Network Overview ===" << Color::RESET << "\n\n";
    
    if (subnets.empty()) {
        std::cout << Color::YELLOW << Icon::WARN << " No subnets configured yet." << Color::RESET << "\n";
        std::cout << "Use option 3 (Configure Subnets) to create your addressing scheme.\n";
        return;
    }
    
    // Header with DHCP Config column
    printf("%s%-4s | %-12s | %-18s | %-5s | %-10s | %-15s | %-18s%s\n", 
           Color::CYAN.c_str(), "ID", "Subnet Name", "IP Range", "VLAN", "Interface", "Gateway", "DHCP Config", Color::RESET.c_str());
    printf("-----|--------------|--------------------|----- -|------------|-----------------|-------------------\n");
    
    for (auto n : subnets) {
        // Skip parent nodes that have been split
        if (n->is_split) continue;
        
        std::string ip_range = address_to_str(n->get_address()) + "/" + std::to_string(n->get_slash());
        std::string vlan_str = n->associated_vlan_id > 0 ? std::to_string(n->associated_vlan_id) : "-";
        std::string iface_str = n->get_assigned_interface().empty() ? "-" : n->get_assigned_interface();
        std::string gateway = address_to_str(n->get_address() + 1);
        
        // DHCP Config column
        std::string dhcp_cfg = "";
        if (n->get_slash() >= 30) {
            dhcp_cfg = "-";  // WAN links
        } else if (!n->dhcp_enabled) {
            dhcp_cfg = "Static";
        } else if (n->dhcp_helper_ip.empty()) {
            dhcp_cfg = "Server (Local)";
        } else {
            dhcp_cfg = "Relay -> " + n->dhcp_helper_ip;
        }
        
        // Color based on assignment status
        std::string row_color = n->assignment_tag == "Free" ? Color::WHITE : Color::GREEN;
        
        printf("%s%-4d | %-12s | %-18s | %-5s | %-10s | %-15s | %-18s%s\n",
               row_color.c_str(),
               n->id,
               n->name.empty() ? "(unnamed)" : n->name.substr(0, 12).c_str(),
               ip_range.c_str(),
               vlan_str.c_str(),
               iface_str.substr(0, 10).c_str(),
               gateway.c_str(),
               dhcp_cfg.c_str(),
               Color::RESET.c_str());
    }
    
    // Summary
    std::cout << "\n" << Color::CYAN << "Legend:" << Color::RESET << " ";
    std::cout << Color::GREEN << "Green" << Color::RESET << " = Assigned, ";
    std::cout << Color::WHITE << "White" << Color::RESET << " = Free\n";
    std::cout << Color::CYAN << "DHCP:" << Color::RESET << " Server (Local) = This router serves DHCP, ";
    std::cout << "Relay = Uses ip helper-address\n";
}


void disconnect_all() {
    std::cout << "Are you sure you want to unplug ALL cables? (y/n): ";
    char c;
    std::cin >> c;
    clear_input();
    
    if (c == 'y' || c == 'Y') {
        for (auto d : devices) {
            d->disconnect_all_interfaces();
        }
        // Also clear Links vector as they are objects holding state
        for (auto l : links) delete l;
        links.clear();
        
        std::cout << "[SUCCESS] All devices are now disconnected.\n";
    } else {
        std::cout << "Operation cancelled.\n";
    }
}

void menu_delete_device() {
    std::cout << "\n--- Delete Device ---\n";
    if (devices.empty()) {
        std::cout << "No devices to delete.\n";
        return;
    }

    // 1. List Devices
    for (size_t i = 0; i < devices.size(); ++i) {
        std::cout << "[" << i << "] " << devices[i]->get_hostname() << "\n";
    }

    // 2. Select ID
    std::cout << "Select Device ID to delete: ";
    int id;
    if (!(std::cin >> id)) {
        clear_input();
        std::cout << "Invalid input.\n";
        return;
    }
    clear_input();

    if (id < 0 || id >= (int)devices.size()) {
        std::cout << "Invalid ID.\n";
        return;
    }

    Device* target = devices[id];
    std::string name = target->get_hostname();
    
    // 3. Safety Cleanup: Check neighbors
    // Iterate all OTHER devices and remove references to target
    int cables_unplugged = 0;
    for (size_t i = 0; i < devices.size(); ++i) {
        if ((int)i == id) continue; // Skip self
        
        // We need to count how many interfaces were connected to 'target' before clearing
        // But the helper just clears them. 
        // We can inspect strictly for feedback?
        // Let's just run strict cleanup.
        devices[i]->remove_neighbor_references(target);
    }

    // Also remove from links vector
    auto it = links.begin();
    while (it != links.end()) {
        if ((*it)->device1 == target || (*it)->device2 == target) {
            delete *it;
            it = links.erase(it);
            cables_unplugged++;
        } else {
            ++it;
        }
    }

    // 4. Erase Device
    delete target;
    devices.erase(devices.begin() + id);

    std::cout << "Successfully deleted " << name << " and unplugged " << cables_unplugged << " cables.\n";
}

void menu_delete_connection() {
    std::cout << "\n--- Delete Connection ---\n";
    if (links.empty()) {
        std::cout << "No active connections.\n";
        return;
    }

    // 1. Scan/List Links
    for (size_t i = 0; i < links.size(); ++i) {
        Link* l = links[i];
        std::cout << "[" << i << "] " 
                  << l->device1->get_hostname() << " (" << l->port1 << ") <--> "
                  << l->device2->get_hostname() << " (" << l->port2 << ")\n";
    }

    // 2. Select ID
    std::cout << "Select Link ID to disconnect: ";
    int id;
    if (!(std::cin >> id)) {
        clear_input();
        std::cout << "Invalid input.\n";
        return;
    }
    clear_input();

    if (id < 0 || id >= (int)links.size()) {
        std::cout << "Invalid ID.\n";
        return;
    }

    Link* target_link = links[id];
    
    // 3. Action: Disconnect ports on both sides
    Device* d1 = target_link->device1;
    Device* d2 = target_link->device2;
    // We need to find the specific interfaces and disconnect them.
    // The device pointers are valid. 
    // We must find the interface on d1 named port1, and d2 named port2.
    
    Interface* if1 = d1->get_interface(target_link->port1);
    if (if1) {
        if1->is_connected = false;
        if1->neighbor = nullptr;
        if1->neighbor_port = "";
    }
    
    Interface* if2 = d2->get_interface(target_link->port2);
    if (if2) {
        if2->is_connected = false;
        if2->neighbor = nullptr;
        if2->neighbor_port = "";
    }

    // 4. Remove Link object
    delete target_link;
    links.erase(links.begin() + id);

    std::cout << "Disconnected Link #" << id << ".\n";
}

void menu_password_manager() {
    while (true) {
        std::cout << "\n--- Password Manager ---\n";
        std::cout << "1. Apply Exam Defaults (Global)\n";
        std::cout << "2. Set Custom Global Passwords\n";
        std::cout << "3. Configure Specific Device\n";
        std::cout << "0. Back\n";
        std::cout << "Select: ";
        
        int opt;
        if (!(std::cin >> opt)) { clear_input(); continue; }
        clear_input();
        
        if (opt == 0) return;
        
        if (opt == 1) {
            for(auto d : devices) {
                if (d->get_type() == DeviceType::ROUTER || d->get_type() == DeviceType::SWITCH) {
                    d->enable_secret = "class";
                    d->vty_password = "admin";
                    // For exam defaults, maybe no username? Prompt didn't specify username default.
                    // "Automatically sets enable_secret='class' ... vty_password='admin'"
                }
            }
            std::cout << "[SUCCESS] Applied Exam Defaults to all devices.\n";
        }
        else if (opt == 2) {
             std::cout << "Enter Enable Secret: ";
             std::string secret; std::getline(std::cin, secret); trim(secret);
             
             std::cout << "Enter VTY Password: ";
             std::string vty; std::getline(std::cin, vty); trim(vty);
             
             for(auto d : devices) {
                if (d->get_type() == DeviceType::ROUTER || d->get_type() == DeviceType::SWITCH) {
                    if(!secret.empty()) d->enable_secret = secret;
                    if(!vty.empty()) d->vty_password = vty;
                }
            }
            std::cout << "[SUCCESS] Applied Custom Globals.\n";
        }
        else if (opt == 3) {
             std::cout << "Select Device:\n";
              for (size_t i = 0; i < devices.size(); ++i) {
                std::cout << "[" << i << "] " << devices[i]->get_hostname() << "\n";
            }
            int id;
            if (!(std::cin >> id) || id < 0 || id >= (int)devices.size()) {
                clear_input(); std::cout << "Invalid ID.\n"; continue;
            }
            clear_input();
            
            Device* d = devices[id];
            
            std::cout << "Configuring " << d->get_hostname() << "\n";
            std::cout << "Enter Enable Secret (current: " << d->enable_secret << "): ";
            std::string secret; std::getline(std::cin, secret); trim(secret);
            if (!secret.empty()) d->enable_secret = secret;
            
            std::cout << "Enter VTY Password (current: " << d->vty_password << "): ";
            std::string vty; std::getline(std::cin, vty); trim(vty);
            if (!vty.empty()) d->vty_password = vty;
            
            std::cout << "Create Local User for SSH? (y/n): ";
            char c; std::cin >> c; clear_input();
            if (c == 'y' || c == 'Y') {
                 std::cout << "Username: ";
                 std::string u; std::getline(std::cin, u); trim(u);
                 d->ssh_username = u;
                 
                 std::cout << "Password: ";
                 std::string p; std::getline(std::cin, p); trim(p);
                 d->ssh_password = p;
            } else {
                 // Clear if they say no? Or keep existing? 
                 // Usually means "don't change" or "disable".
                 // Let's ask if they want to clear.
                 if (!d->ssh_username.empty()) {
                     std::cout << "Clear existing SSH user? (y/n): ";
                     char c2; std::cin >> c2; clear_input();
                     if (c2 == 'y' || c2 == 'Y') {
                         d->ssh_username = "";
                         d->ssh_password = "";
                     }
                 }
            }
            std::cout << "[SUCCESS] Updated passwords for " << d->get_hostname() << ".\n";
        }
    }
}

void run_cli_mode() {
    // Initialize Logger to prevent segfaults in helper classes
    activate_logging(spdlog::level::info);

    StateManager::load(devices, links, subnets);

    // Basic loop
    while(true) {
        print_menu();
        
        int opt;
        if (!(std::cin >> opt)) {
            clear_input();
            continue;
        }
        clear_input();

        switch(opt) {
            case 1: menu_add_device(); break;
            case 2: menu_connect_devices(); break;
            case 3: menu_configure_subnets(); break;
            case 4: menu_generate_guide(devices, links, subnets); break;
            case 5: Visualizer::draw(devices, links, subnets); break;
            case 6: menu_configure_security(); break;
            case 7: VlanManager::menu_manage_vlans(devices); break;
            case 8: load_exam_scenario(); break;
            case 9: Documentation::show_main_menu(); break;
            case 10: StateManager::save(devices, links, subnets); exit(0);
            case 11: disconnect_all(); break;
            case 12: menu_delete_device(); break;
            case 13: menu_delete_connection(); break;
            case 14: menu_password_manager(); break;
            case 15: load_exam_template(); break;
            case 16: show_network_overview(); break;
            case 17: {
                std::cout << Color::RED << "Are you sure you want to delete EVERYTHING? (y/n): " << Color::RESET;
                char c;
                if (std::cin >> c) {
                    clear_input();
                    if (c == 'y' || c == 'Y') {
                        for (auto d : devices) delete d;
                        devices.clear();
                        for (auto l : links) delete l;
                        links.clear();
                        for (auto n : subnets) delete n;
                        subnets.clear();
                        VlanManager::defined_vlans.clear();
                        VlanManager::init();
                        static_routes.clear();
                        std::cout << Color::RED << "\n💥 All data has been incinerated." << Color::RESET << "\n";
                    } else {
                        std::cout << "Operation cancelled.\n";
                    }
                }
                break;
            }
            case 18: {
                while(true) {
                    std::cout << "\n--- Static Route Manager ---\n";
                    std::cout << "1. Add Route\n";
                    std::cout << "2. View/Delete Routes\n";
                    std::cout << "0. Back\n";
                    std::cout << "Select: ";
                    int sopt;
                    if(!(std::cin >> sopt)) { clear_input(); continue; }
                    clear_input();
                    
                    if (sopt == 0) break;
                    
                    if (sopt == 1) {
                         std::cout << "Available Routers:\n";
                         std::vector<int> router_ids;
                         for(size_t i=0; i<devices.size(); ++i) {
                             if(devices[i]->get_type() == DeviceType::ROUTER) {
                                 std::cout << "[" << i << "] " << devices[i]->get_hostname() << "\n";
                                 router_ids.push_back((int)i);
                             }
                         }
                         if(router_ids.empty()) { std::cout << "No routers found.\n"; continue; }
                         
                         std::cout << "Select Router ID: ";
                         int rid; 
                         if(!(std::cin >> rid)) { clear_input(); continue; }
                         clear_input();
                         
                         bool valid = false;
                         for(int id : router_ids) if(id == rid) valid = true;
                         if(!valid) { std::cout << "Invalid Router ID.\n"; continue; }
                         
                         std::string dest, mask, hop;
                         std::cout << "Destination Network (0.0.0.0 for Default): ";
                         std::getline(std::cin, dest); trim(dest);
                         
                         std::cout << "Subnet Mask (0.0.0.0 for Default): ";
                         std::getline(std::cin, mask); trim(mask);
                         
                         std::cout << "Next Hop IP Address: ";
                         std::getline(std::cin, hop); trim(hop);
                         
                         static_routes.push_back({rid, dest, mask, hop});
                         std::cout << Color::GREEN << "✅ Route Added." << Color::RESET << "\n";
                    }
                    else if (sopt == 2) {
                        if(static_routes.empty()) { std::cout << "No routes defined.\n"; continue; }
                        
                        std::cout << "\nCannot undo deletions!\n";
                        for(size_t i=0; i<static_routes.size(); ++i) {
                            StaticRoute& r = static_routes[i];
                            std::string hostname = "Unknown";
                            if(r.router_id >= 0 && r.router_id < (int)devices.size()) 
                                hostname = devices[r.router_id]->get_hostname();
                            
                            std::cout << "[" << i+1 << "] " << hostname << ": ip route " 
                                      << r.dest_net << " " << r.mask << " " << r.next_hop << "\n";
                        }
                        
                        std::cout << "Enter Route ID to delete (or 0 to cancel): ";
                        int did;
                        if(!(std::cin >> did)) { clear_input(); continue; }
                        clear_input();
                        
                        if(did > 0 && did <= (int)static_routes.size()) {
                            static_routes.erase(static_routes.begin() + (did - 1));
                            std::cout << Color::RED << "Route deleted." << Color::RESET << "\n";
                        }
                    }
                }
                break;
            }
            case 0: exit(0);
            default: std::cout << "Invalid option.\n";
        }
    }
}

int main(int argc, char* argv[]) {
    // Check arguments
    bool use_gui = false;
    for(int i=1; i<argc; ++i) {
        std::string arg = argv[i];
        if(arg == "--gui") {
            use_gui = true;
        }
    }

    if (use_gui) {
        std::cout << "Launching GUI Mode...\n";
        // Load state for GUI too?
        StateManager::load(devices, links, subnets); // Pass subnets
        GuiLayer::run(devices, links);
    } else {
        run_cli_mode();
    }
    
    return 0;
}