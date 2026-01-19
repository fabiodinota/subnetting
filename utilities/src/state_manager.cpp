#include "state_manager.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include "vlan_manager.hpp"
#include "network.hpp"

// Helpers
static void trim(std::string &s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(*start)) {
        start++;
    }
    s.erase(s.begin(), start);
    
    auto end = s.rbegin();
    while (end != s.rend() && std::isspace(*end)) {
        end++;
    }
    s.erase(end.base(), s.end());
}

void StateManager::save(const std::vector<Device*>& devices, const std::vector<Link*>& links, const std::vector<Network*>& subnets) {
    std::ofstream file("network_save.dat");
    if (!file.is_open()) {
        std::cout << "Error: Could not open file for writing.\n";
        return;
    }

    // [DEVICES]
    file << "[DEVICES]\n";
    for (size_t i = 0; i < devices.size(); ++i) {
        Device* d = devices[i];
        std::string type_str;
        switch (d->get_type()) {
            case DeviceType::ROUTER: type_str = "ROUTER"; break;
            case DeviceType::SWITCH: type_str = "SWITCH"; break;
            case DeviceType::PC: type_str = "PC"; break;
        }
        // ID|HOSTNAME|TYPE|X|Y|ColorR|ColorG|ColorB
        file << i << "|" << d->get_hostname() << "|" << type_str << "|" 
             << d->x << "|" << d->y << "|"
             << d->color.r << "|" << d->color.g << "|" << d->color.b << "\n";
    }

    // [CONNECTIONS]
    file << "\n[CONNECTIONS]\n";
    for (auto l : links) {
        // HOST1|PORT1|HOST2|PORT2
        file << l->device1->get_hostname() << "|" << l->port1 << "|"
             << l->device2->get_hostname() << "|" << l->port2 << "\n";
    }

    // [VLANS]
    file << "\n[VLANS]\n";
    for(auto const& [id, name] : VlanManager::defined_vlans) {
        file << id << "|" << name << "\n";
    }

    // [SUBNETS]
    // # ID | Network | Slash | ParentID | Name | AssignedString | AssignedInterface | VlanID | DHCPEnabled | DHCPUpperHalf | DHCPServerID | DHCPHelperIP
    file << "\n[SUBNETS]\n";
    for(auto n : subnets) {
        std::string helper_ip = n->dhcp_helper_ip.empty() ? "NONE" : n->dhcp_helper_ip;
        file << n->id << "|" << address_to_str(n->get_address()) << "|" 
             << n->get_slash() << "|" << n->parent_id << "|"
             << (n->name.empty() ? "" : n->name) << "|"
             << n->get_assignment() << "|" << n->get_assigned_interface() 
             << "|" << n->associated_vlan_id
             << "|" << (n->dhcp_enabled ? 1 : 0)
             << "|" << (n->dhcp_upper_half_only ? 1 : 0)
             << "|" << n->dhcp_server_id
             << "|" << helper_ip << "\n";
    }

    // [DEVICE_CONFIGS]
    // DeviceID | ID | Secret | VTY | SSHUser | SSHPass | MgmtIP | UseTelnet | AllowedIP
    // Mapping ID via index for now, but safer to use Hostname to look up index? 
    // Format assumes list index consistency.
    file << "\n[DEVICE_CONFIGS]\n";
    for (size_t i = 0; i < devices.size(); ++i) {
        Device* d = devices[i];
        file << i << "|" 
             << d->enable_secret << "|" 
             << d->vty_password << "|" 
             << d->ssh_username << "|" 
             << d->ssh_password << "|" 
             << d->management_config.management_svi_ip << "|" 
             << (d->management_config.enable_telnet ? "1" : "0") << "|"
             << d->management_config.allowed_telnet_ip << "\n";
    }

    // [INTERFACE_CONFIGS]
    // DeviceID | InterfaceName | VLAN_ID | IsTrunk | NeighborPort?
    // We only care about Switch port configs really? Or Router subinterfaces?
    file << "\n[INTERFACE_CONFIGS]\n";
    for (size_t i = 0; i < devices.size(); ++i) {
        if (devices[i]->get_type() == DeviceType::SWITCH) {
             Switch* sw = dynamic_cast<Switch*>(devices[i]);
             for(auto& iface : sw->interfaces) {
                 if (iface.vlan_id > 1 || iface.is_trunk) {
                     file << i << "|" << iface.name << "|" 
                          << iface.vlan_id << "|" 
                          << (iface.is_trunk ? "1" : "0") << "\n";
                 }
             }
        }
        else if (devices[i]->get_type() == DeviceType::ROUTER) {
            Router* r = dynamic_cast<Router*>(devices[i]);
            // Save subinterfaces as pseudo-configs?
            // Format: DeviceID | Name | VLAN | 0 | SubIP | SubMask
            // Expanding format for router
            for(auto& sub : r->subinterfaces) {
                 std::string fullname = sub.interface_name.empty() ? ("g0/0/0." + std::to_string(sub.id)) : sub.interface_name;
                 file << i << "|" << fullname << "|" << sub.vlan_id << "|0|" 
                      << sub.ip_address << "|" << sub.subnet_mask << "\n";
            }
        }
    }

    // [STATIC_ROUTES]
    // RouterID|DestNet|Mask|NextHop
    file << "\n[STATIC_ROUTES]\n";
    for(const auto& r : static_routes) {
        file << r.router_id << "|" << r.dest_net << "|" << r.mask << "|" << r.next_hop << "\n";
    }

    file.close();
    std::cout << "State saved to network_save.dat.\n";
}

void StateManager::load(std::vector<Device*>& devices, std::vector<Link*>& links, std::vector<Network*>& subnets) {
    // Clear existing
    for(auto d : devices) delete d;
    devices.clear();
    for(auto l : links) delete l;
    links.clear();
    for(auto n : subnets) delete n;
    subnets.clear();
    VlanManager::defined_vlans.clear();
    VlanManager::init(); // Reset to default 1

    std::ifstream file("network_save.dat");
    if (!file.is_open()) return;

    std::cout << "Found save file. Loading topology...\n";
    
    std::string line;
    std::string current_section;

    static_routes.clear();

    while (std::getline(file, line)) {
        trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (line[0] == '[') {
            current_section = line;
            continue;
        }

        std::stringstream ss(line);
        std::string segment;
        std::vector<std::string> parts;
        while(std::getline(ss, segment, '|')) {
            parts.push_back(segment);
        }

        if (current_section == "[DEVICES]") {
            // ID|HOSTNAME|TYPE|X|Y|R|G|B
            if (parts.size() >= 3) {
                 std::string name = parts[1];
                 std::string type = parts[2];
                 Device* d = nullptr;
                 if (type == "ROUTER") d = new Router(name);
                 else if (type == "SWITCH") d = new Switch(name);
                 else if (type == "PC") d = new PC(name);
                 
                 if (d) {
                     if (parts.size() >= 8) {
                        try {
                            d->x = std::stof(parts[3]);
                            d->y = std::stof(parts[4]);
                            d->color.r = std::stof(parts[5]);
                            d->color.g = std::stof(parts[6]);
                            d->color.b = std::stof(parts[7]);
                        } catch(...) {}
                     }
                     devices.push_back(d);
                 }
            }
        }
        else if (current_section == "[CONNECTIONS]") {
            if (parts.size() >= 4) {
                 Device* d1 = Device::get_device_by_name(devices, parts[0]);
                 Device* d2 = Device::get_device_by_name(devices, parts[2]);
                 if (d1 && d2) {
                     Link* l = new Link(d1, parts[1], d2, parts[3]);
                     links.push_back(l);
                 }
            }
        }
        else if (current_section == "[VLANS]") {
            if (parts.size() >= 2) {
                int id = std::stoi(parts[0]);
                std::string name = parts[1];
                VlanManager::defined_vlans[id] = name;
            }
        }
        else if (current_section == "[SUBNETS]") {
            // ID|Net|Slash|Parent|Name|Assigned|Interface|VlanID|DHCPEnabled|DHCPUpperHalf|DHCPServerID|DHCPHelperIP
            if (parts.size() >= 7) {
                Network* n = new Network();
                n->id = std::stoi(parts[0]);
                int segments[4];
                sscanf(parts[1].c_str(), "%d.%d.%d.%d", &segments[0], &segments[1], &segments[2], &segments[3]);
                unsigned int net_int = (segments[0] << 24) | (segments[1] << 16) | (segments[2] << 8) | segments[3];
                n->set_address(net_int);
                
                n->set_slash(std::stoi(parts[2]));
                unsigned int mask = (n->get_slash() == 0) ? 0 : (~0u) << (32 - n->get_slash());
                n->set_mask(mask);
                
                n->parent_id = std::stoi(parts[3]);
                n->name = parts[4];
                n->set_assignment(parts[5]);
                n->set_assigned_interface(parts[6]);
                
                // Load associated VLAN ID if present (backward compatible)
                if (parts.size() >= 8) {
                    try { n->associated_vlan_id = std::stoi(parts[7]); } catch(...) {}
                }
                
                // Load DHCP configuration if present (backward compatible)
                if (parts.size() >= 12) {
                    try {
                        n->dhcp_enabled = (std::stoi(parts[8]) == 1);
                        n->dhcp_upper_half_only = (std::stoi(parts[9]) == 1);
                        n->dhcp_server_id = std::stoi(parts[10]);
                        std::string helper_str = parts[11];
                        n->dhcp_helper_ip = (helper_str == "NONE") ? "" : helper_str;
                    } catch(...) {
                        // Defaults already set in Network struct
                    }
                }
                
                subnets.push_back(n);
            }
        }
        else if (current_section == "[DEVICE_CONFIGS]") {
            // Index|Secret|VTY|User|Pass|MgmtIP|TelnetBool|AllowedIP
            if (parts.size() >= 8) {
                int idx = std::stoi(parts[0]);
                if (idx >= 0 && idx < (int)devices.size()) {
                    Device* d = devices[idx];
                    d->enable_secret = parts[1];
                    d->vty_password = parts[2];
                    d->ssh_username = parts[3];
                    d->ssh_password = parts[4];
                    d->management_config.management_svi_ip = parts[5];
                    d->management_config.enable_telnet = (parts[6] == "1");
                    d->management_config.allowed_telnet_ip = parts[7];
                }
            }
        }
        else if (current_section == "[INTERFACE_CONFIGS]") {
            // Index|Name|VID|Trunk|SubIP|SubMask
            if (parts.size() >= 4) {
                int idx = std::stoi(parts[0]);
                if (idx >= 0 && idx < (int)devices.size()) {
                    Device* d = devices[idx];
                    if (d->get_type() == DeviceType::SWITCH) {
                        Switch* sw = dynamic_cast<Switch*>(d);
                        std::string ifname = parts[1];
                        int vid = std::stoi(parts[2]);
                        bool trunk = (parts[3] == "1");
                        
                        // Find interface
                        // If not found, add it? (Already added by factory maybe)
                        // But switch factory adds 24 ports.
                        for(auto& iface : sw->interfaces) {
                            if(iface.name == ifname) {
                                iface.vlan_id = vid;
                                iface.is_trunk = trunk;
                            }
                        }
                    } else if (d->get_type() == DeviceType::ROUTER) {
                        // Restore subinterface
                        if (parts.size() >= 6) {
                            Router* r = dynamic_cast<Router*>(d);
                            int vid = std::stoi(parts[2]);
                            std::string ip = parts[4];
                            std::string mask = parts[5];
                            std::string name = parts[1];
                            
                            // Guess ID from name (g0/0.10 -> 10)
                            int sub_id = vid;
                            size_t dot = name.find('.');
                            if (dot != std::string::npos) {
                                try { sub_id = std::stoi(name.substr(dot+1)); } catch(...) {}
                            }
                            
                            r->configure_roas(sub_id, vid, ip, mask, name);
                        }
                    }
                }
            }
        }
        else if (current_section == "[STATIC_ROUTES]") {
            // RouterID|DestNet|Mask|NextHop
            if(parts.size() >= 4) {
                StaticRoute r;
                r.router_id = std::stoi(parts[0]);
                r.dest_net = parts[1];
                r.mask = parts[2];
                r.next_hop = parts[3];
                static_routes.push_back(r);
            }
        }
    }
    
    // Reconstruct Network hierarchy (children_ids)
    for(auto n : subnets) {
        if (n->parent_id != 0) {
            // Find parent
            for(auto p : subnets) {
                if (p->id == n->parent_id) {
                    p->children_ids.push_back(n->id);
                    p->is_split = true; // Implicitly true if children exist
                    break;
                }
            }
        }
    }
    
    file.close();
    std::cout << "Loaded full state.\n";
}

bool StateManager::load_scenario(const std::string& filename, std::vector<Device*>& devices, std::vector<Link*>& links, std::vector<Network*>& subnets) {
    // Phase 1: Clear existing state
    for(auto d : devices) delete d;
    devices.clear();
    for(auto l : links) delete l;
    links.clear();
    for(auto n : subnets) delete n;
    subnets.clear();
    VlanManager::defined_vlans.clear();
    VlanManager::init(); // Reset to default VLAN 1

    // Open scenario file
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[ERROR] Could not open scenario file: " << filename << "\n";
        return false;
    }

    std::cout << "Loading scenario from: " << filename << "...\n";
    
    std::string line;
    std::string current_section;
    
    // Temporary map for subnet hierarchy reconstruction
    std::map<int, Network*> subnet_map;

    // Phase 2: Parse sections
    while (std::getline(file, line)) {
        trim(line);
        if (line.empty() || line[0] == '#') continue;

        // Detect section headers
        if (line[0] == '[') {
            current_section = line;
            continue;
        }

        // Parse line by delimiter "|"
        std::stringstream ss(line);
        std::string segment;
        std::vector<std::string> parts;
        while(std::getline(ss, segment, '|')) {
            parts.push_back(segment);
        }

        // [DEVICES] Section
        if (current_section == "[DEVICES]") {
            // Format: ID|HOSTNAME|TYPE|X|Y|R|G|B
            if (parts.size() >= 3) {
                std::string name = parts[1];
                std::string type = parts[2];
                Device* d = nullptr;
                if (type == "ROUTER") d = new Router(name);
                else if (type == "SWITCH") d = new Switch(name);
                else if (type == "PC") d = new PC(name);
                
                if (d) {
                    if (parts.size() >= 8) {
                        try {
                            d->x = std::stof(parts[3]);
                            d->y = std::stof(parts[4]);
                            d->color.r = std::stof(parts[5]);
                            d->color.g = std::stof(parts[6]);
                            d->color.b = std::stof(parts[7]);
                        } catch(...) {}
                    }
                    devices.push_back(d);
                }
            }
        }
        // [CONNECTIONS] Section
        else if (current_section == "[CONNECTIONS]") {
            // Format: HOST1|PORT1|HOST2|PORT2
            if (parts.size() >= 4) {
                Device* d1 = Device::get_device_by_name(devices, parts[0]);
                Device* d2 = Device::get_device_by_name(devices, parts[2]);
                if (d1 && d2) {
                    Link* l = new Link(d1, parts[1], d2, parts[3]);
                    links.push_back(l);
                }
            }
        }
        // [VLANS] Section
        else if (current_section == "[VLANS]") {
            // Format: ID|Name
            if (parts.size() >= 2) {
                int id = std::stoi(parts[0]);
                std::string name = parts[1];
                VlanManager::defined_vlans[id] = name;
            }
        }
        // [SUBNETS] Section - Store in temp map for hierarchy rebuild
        else if (current_section == "[SUBNETS]") {
            // Format: ID|Network|Slash|ParentID|Name|AssignedTo|AssignedInterface|VlanID|DHCPEnabled|DHCPUpperHalf|DHCPServerID|DHCPHelperIP
            if (parts.size() >= 7) {
                Network* n = new Network();
                n->id = std::stoi(parts[0]);
                
                // Parse IP address string to integer
                int segments[4];
                sscanf(parts[1].c_str(), "%d.%d.%d.%d", &segments[0], &segments[1], &segments[2], &segments[3]);
                unsigned int net_int = (segments[0] << 24) | (segments[1] << 16) | (segments[2] << 8) | segments[3];
                n->set_address(net_int);
                
                n->set_slash(std::stoi(parts[2]));
                unsigned int mask = (n->get_slash() == 0) ? 0 : (~0u) << (32 - n->get_slash());
                n->set_mask(mask);
                
                n->parent_id = std::stoi(parts[3]);
                n->name = parts[4];
                n->set_assignment(parts[5]);
                n->set_assigned_interface(parts[6]);
                
                // Load VLAN ID if present
                if (parts.size() >= 8) {
                    try { n->associated_vlan_id = std::stoi(parts[7]); } catch(...) {}
                }
                
                // Load DHCP configuration if present (backward compatible)
                if (parts.size() >= 12) {
                    try {
                        n->dhcp_enabled = (std::stoi(parts[8]) == 1);
                        n->dhcp_upper_half_only = (std::stoi(parts[9]) == 1);
                        n->dhcp_server_id = std::stoi(parts[10]);
                        std::string helper_str = parts[11];
                        n->dhcp_helper_ip = (helper_str == "NONE") ? "" : helper_str;
                    } catch(...) {
                        // Defaults already set in Network struct
                    }
                }
                
                subnet_map[n->id] = n;
                subnets.push_back(n);
            }
        }
        // [DEVICE_CONFIGS] Section
        else if (current_section == "[DEVICE_CONFIGS]") {
            // Format: Index|Secret|VTY|User|Pass|MgmtIP|TelnetBool|AllowedIP
            if (parts.size() >= 8) {
                int idx = std::stoi(parts[0]);
                if (idx >= 0 && idx < (int)devices.size()) {
                    Device* d = devices[idx];
                    d->enable_secret = parts[1];
                    d->vty_password = parts[2];
                    d->ssh_username = parts[3];
                    d->ssh_password = parts[4];
                    d->management_config.management_svi_ip = parts[5];
                    d->management_config.enable_telnet = (parts[6] == "1");
                    d->management_config.allowed_telnet_ip = parts[7];
                }
            }
        }
        // [INTERFACE_CONFIGS] Section
        else if (current_section == "[INTERFACE_CONFIGS]") {
            // Format: Index|Name|VID|Trunk|SubIP|SubMask
            if (parts.size() >= 4) {
                int idx = std::stoi(parts[0]);
                if (idx >= 0 && idx < (int)devices.size()) {
                    Device* d = devices[idx];
                    std::string ifname = parts[1];
                    int vid = std::stoi(parts[2]);
                    bool trunk = (parts[3] == "1");
                    
                    if (d->get_type() == DeviceType::SWITCH) {
                        Switch* sw = dynamic_cast<Switch*>(d);
                        // Find interface by name (string match)
                        for(auto& iface : sw->interfaces) {
                            if(iface.name == ifname) {
                                iface.vlan_id = vid;
                                iface.is_trunk = trunk;
                                iface.vlan_name = VlanManager::get_vlan_name(vid);
                                break;
                            }
                        }
                    } else if (d->get_type() == DeviceType::ROUTER && parts.size() >= 6) {
                        // Restore subinterface
                        Router* r = dynamic_cast<Router*>(d);
                        std::string ip = parts[4];
                        std::string mask = parts[5];
                        
                        // Extract subinterface ID from name (e.g., g0/0.10 -> 10)
                        int sub_id = vid;
                        size_t dot = ifname.find('.');
                        if (dot != std::string::npos) {
                            try { sub_id = std::stoi(ifname.substr(dot+1)); } catch(...) {}
                        }
                        
                        r->configure_roas(sub_id, vid, ip, mask, ifname);
                    }
                }
            }
        }
    }
    
    // Phase 3: Reconstruct subnet hierarchy (parent-child relationships)
    for(auto n : subnets) {
        if (n->parent_id != 0) {
            auto it = subnet_map.find(n->parent_id);
            if (it != subnet_map.end()) {
                Network* parent = it->second;
                parent->children_ids.push_back(n->id);
                parent->is_split = true;
            }
        }
    }
    
    file.close();
    return true;
}
