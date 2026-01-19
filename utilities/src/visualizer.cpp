#include "visualizer.hpp"
#include <iostream>
#include <algorithm>
#include "colors.hpp"
#include "network.hpp"
#include "vlan_manager.hpp"

// Forward declaration
void print_subtree(Device* dev, std::string prefix, std::set<std::string>& visited, 
                   const std::vector<Link*>& links, const std::vector<Network*>& subnets);

// Helper: Get subnet info string for a given VLAN ID (with DHCP status)
std::string get_subnet_info_for_vlan(int vlan_id, const std::vector<Network*>& subnets) {
    if (vlan_id <= 1) return "";
    
    for (auto n : subnets) {
        if (n->associated_vlan_id == vlan_id && !n->is_split) {
            std::string net_str = address_to_str(n->get_address());
            std::string name = n->name.empty() ? VlanManager::get_vlan_name(vlan_id) : n->name;
            std::string base = "🌐 " + name + " (" + net_str + "/" + std::to_string(n->get_slash()) + ")";
            
            // Append DHCP Status Tag
            std::string dhcp_tag = "";
            if (!n->dhcp_enabled) {
                dhcp_tag = std::string(Color::CYAN) + " [Static]" + Color::RESET;
            } else if (!n->dhcp_helper_ip.empty()) {
                dhcp_tag = std::string(Color::YELLOW) + " [DHCP: Relay -> " + n->dhcp_helper_ip + "]" + Color::RESET;
            } else {
                dhcp_tag = std::string(Color::GREEN) + " [DHCP: Server]" + Color::RESET;
            }
            
            return base + dhcp_tag;
        }
    }
    return "";
}

// Helper: Get all subnets assigned to a router
std::vector<std::pair<std::string, std::string>> get_router_subnets(Device* router, const std::vector<Network*>& subnets) {
    std::vector<std::pair<std::string, std::string>> results;
    
    for (auto n : subnets) {
        if (n->is_split) continue;
        
        // Check if this subnet's assigned interface mentions this router
        std::string assignment = n->get_assignment();
        if (assignment.find(router->get_hostname()) != std::string::npos) {
            // Extract gateway IP (first usable)
            unsigned int net_addr = n->get_address();
            unsigned int gateway = net_addr + 1;
            std::string gw_ip = address_to_str(gateway);
            
            std::string name = n->name.empty() ? ("Subnet " + std::to_string(n->id)) : n->name;
            std::string iface = n->get_assigned_interface();
            
            results.push_back({gw_ip, name + " via " + iface});
        }
    }
    
    return results;
}

// Helper: Get DHCP pools hosted by this router (by router index)
std::vector<std::pair<std::string, std::string>> get_router_dhcp_pools(Device* router, int router_idx, const std::vector<Network*>& subnets) {
    std::vector<std::pair<std::string, std::string>> pools;
    
    for (auto n : subnets) {
        if (n->is_split) continue;
        if (!n->dhcp_enabled) continue;
        
        // Check if this router is the designated DHCP server
        bool is_server = false;
        
        // Case 1: dhcp_server_id matches this router's index
        if (n->dhcp_server_id == router_idx) {
            is_server = true;
        }
        
        // Case 2: Local DHCP (server_id == -1, no helper) and subnet is assigned to this router
        if (n->dhcp_server_id == -1 && n->dhcp_helper_ip.empty()) {
            std::string assignment = n->get_assignment();
            if (assignment.find(router->get_hostname()) != std::string::npos) {
                is_server = true;
            }
        }
        
        if (is_server) {
            std::string net_str = address_to_str(n->get_address());
            std::string name = n->name.empty() ? ("Pool " + std::to_string(n->id)) : n->name;
            pools.push_back({name, net_str + "/" + std::to_string(n->get_slash())});
        }
    }
    
    return pools;
}

void Visualizer::draw(const std::vector<Device*>& devices, const std::vector<Link*>& links, const std::vector<Network*>& subnets) {
    if (devices.empty()) {
        std::cout << "No devices to visualize.\n";
        return;
    }

    std::cout << "\n" << Color::MAGENTA << Color::BOLD << "=== Network Topology Deep Inspection ===" << Color::RESET << "\n\n";

    // 1. Identify all Routers as independent roots
    std::vector<Device*> routers;
    for (auto d : devices) {
        if (d->get_type() == DeviceType::ROUTER) {
            routers.push_back(d);
        }
    }

    if (routers.empty()) {
        std::cout << Color::YELLOW << "No Routers found. Showing entire shared topology..." << Color::RESET << "\n";
        std::set<std::string> visited;
        if (!devices.empty()) print_node(devices[0], "", true, visited, links, subnets);
        return;
    }

    // 2. Draw a tree for EACH Router
    int router_idx = 0;
    for (auto router : routers) {
        std::cout << Color::BOLD << "Topology for Router: " << router->get_hostname() << Color::RESET << "\n";
        
        // Print root router
        std::cout << Color::RED << Icon::ROUTER << router->get_hostname() << Color::RESET << "\n";
        
        // Print router's IP assignments (gateway IPs)
        auto router_ips = get_router_subnets(router, subnets);
        for (const auto& [ip, desc] : router_ips) {
            std::cout << "   " << Color::BLUE << "ipv4: " << ip << " (" << desc << ")" << Color::RESET << "\n";
        }
        
        // Print DHCP Pools hosted by this router
        auto dhcp_pools = get_router_dhcp_pools(router, router_idx, subnets);
        if (!dhcp_pools.empty()) {
            for (const auto& [name, range] : dhcp_pools) {
                std::cout << "   " << Color::GREEN << "💧 Hosting Pool: " << name << " (" << range << ")" << Color::RESET << "\n";
            }
        }
        
        // Now print subtree
        std::set<std::string> visited;
        visited.insert(router->get_hostname());
        print_subtree(router, "", visited, links, subnets);
        
        std::cout << "\n" << Color::WHITE << "──────────────────────────────────────────" << Color::RESET << "\n\n";
        router_idx++;
    }
}

void Visualizer::print_node(Device* dev, std::string prefix, bool is_last, std::set<std::string>& visited, 
                            const std::vector<Link*>& links, const std::vector<Network*>& subnets) {
    std::string type_str;
    std::string color_code;
    std::string icon;
    
    switch (dev->get_type()) {
        case DeviceType::ROUTER: 
            type_str = "ROUTER";
            color_code = Color::RED;
            icon = Icon::ROUTER;
            break;
        case DeviceType::SWITCH: 
            type_str = "SWITCH"; 
            color_code = Color::GREEN;
            icon = Icon::SWITCH;
            break;
        case DeviceType::PC: 
            type_str = "PC"; 
            color_code = Color::CYAN;
            icon = Icon::PC;
            break;
    }

    std::string marker = is_last ? "└── " : "├── ";
    if (prefix.empty()) marker = "";

    std::cout << prefix << marker << color_code << icon << dev->get_hostname() << Color::RESET << "\n";

    visited.insert(dev->get_hostname());
    print_subtree(dev, prefix, visited, links, subnets);
}

void print_subtree(Device* dev, std::string prefix, std::set<std::string>& visited, 
                   const std::vector<Link*>& links, const std::vector<Network*>& subnets) {
    // Find children
    struct Connection {
        Link* link;
        Device* neighbor;
        std::string my_port;
        std::string cable_str;
    };
    std::vector<Connection> connections;

    for (const auto& l : links) {
        if (l->device1 == dev) {
             connections.push_back({l, l->device2, l->port1, l->get_cable_type_str()});
        } else if (l->device2 == dev) {
             connections.push_back({l, l->device1, l->port2, l->get_cable_type_str()});
        }
    }

    std::vector<Connection> valid_children;
    for (auto& c : connections) {
        if (visited.find(c.neighbor->get_hostname()) == visited.end()) {
            valid_children.push_back(c);
        }
    }

    for (size_t i = 0; i < valid_children.size(); ++i) {
        auto& c = valid_children[i];
        bool is_last_child = (i == valid_children.size() - 1);
        
        std::string marker = is_last_child ? "└── " : "├── ";
        std::string cont_line = is_last_child ? "    " : "│   ";
        
        // Get VLAN info from interface
        Interface* face = dev->get_interface(c.my_port);
        int vlan_id = (face && face->vlan_id > 0) ? face->vlan_id : 1;
        
        std::cout << prefix << marker << Color::WHITE << c.my_port << Color::RESET << " -- ";
        
        bool is_router_neighbor = false;
        std::string n_color = Color::RESET;
        std::string n_icon = "";
        std::string n_type = "";
        bool is_pc = false;

        switch (c.neighbor->get_type()) {
            case DeviceType::ROUTER: 
                n_color = Color::RED; n_icon = Icon::ROUTER; n_type = "ROUTER";
                is_router_neighbor = true;
                break;
            case DeviceType::SWITCH: 
                n_color = Color::GREEN; n_icon = Icon::SWITCH; n_type = "SWITCH";
                break;
            case DeviceType::PC: 
                n_color = Color::CYAN; n_icon = Icon::PC; n_type = "PC";
                is_pc = true;
                break;
        }

        std::cout << n_color << n_icon << c.neighbor->get_hostname() << Color::RESET;

        // Show VLAN for non-default
        if (vlan_id > 1) {
             std::cout << Color::YELLOW << " [VLAN " << vlan_id << "]" << Color::RESET;
        }

        if (is_router_neighbor) {
            std::cout << Color::MAGENTA << " (WAN Link)" << Color::RESET << "\n";
            visited.insert(c.neighbor->get_hostname());
            continue; 
        }

        std::cout << "\n";
        
        // NEW: If it's a PC/Laptop, show subnet info on next line (with DHCP status)
        if (is_pc && vlan_id > 1) {
            std::string subnet_info = get_subnet_info_for_vlan(vlan_id, subnets);
            if (!subnet_info.empty()) {
                // Calculate proper indentation
                std::string info_prefix = prefix + cont_line;
                // Add spacing to align under the device name
                size_t port_width = c.my_port.length() + 4; // " -- " is 4 chars
                std::string spacing(port_width, ' ');
                std::cout << info_prefix << spacing << "└── " << subnet_info << "\n";
            }
        }
        
        visited.insert(c.neighbor->get_hostname());

        std::string next_prefix = prefix + cont_line;
        print_subtree(c.neighbor, next_prefix, visited, links, subnets);
    }
}
