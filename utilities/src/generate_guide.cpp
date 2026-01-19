#include "generate_guide.hpp"
#include <iostream>
#include <algorithm>
#include <map>
#include <set>
#include "vlan_manager.hpp"
#include "utilities.hpp"
#include "colors.hpp"

// Syntax Highlighting Macros
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[1;35m" 
#define CYAN    "\033[36m"   
#define WHITE   "\033[37m"

/**
 * Helper: Detect if the router connected to a switch is configured for VLANs (ROAS).
 * This determines if the switch uplink port Gig0/1 should be TRUNK or ACCESS.
 */
std::string get_uplink_mode_from_router(Device* sw_device, const std::vector<Link*>& links, const std::vector<Network*>& subnets) {
    // 1. Find the device connected to Gig0/1
    Device* router_dev = nullptr;
    for (auto l : links) {
        if (l->device1 == sw_device && l->port1 == "Gig0/1") {
            router_dev = l->device2;
            break;
        } else if (l->device2 == sw_device && l->port2 == "Gig0/1") {
            router_dev = l->device1;
            break;
        }
    }

    // 2. If no router found on Gig0/1, try Gig0/2
    if (!router_dev) {
        for (auto l : links) {
            if (l->device1 == sw_device && l->port1 == "Gig0/2") {
                router_dev = l->device2;
                break;
            } else if (l->device2 == sw_device && l->port2 == "Gig0/2") {
                router_dev = l->device1;
                break;
            }
        }
    }

    // 3. If connected to a router, check its assigned subnets for VLAN involvement
    if (router_dev && router_dev->get_type() == DeviceType::ROUTER) {
        for (auto n : subnets) {
            if (n->is_split) continue;
            // Check if this subnet is assigned to the connected router
            if (n->get_assignment().find(router_dev->get_hostname()) != std::string::npos) {
                // If ANY assigned subnet has a VLAN ID > 1, the router needs TRUNK
                if (n->associated_vlan_id > 1) {
                    return "TRUNK";
                }
            }
        }
        // If we checked all subnets and none were VLAN-based
        return "ACCESS";
    }

    // Default Fallback: Check if switch itself has VLAN assignments on any interface
    if (Switch* sw = dynamic_cast<Switch*>(sw_device)) {
        for (auto& iface : sw->interfaces) {
            if (iface.vlan_id > 1) return "TRUNK";
        }
        for (auto const& [vid, vname] : VlanManager::defined_vlans) {
            if (vid > 1) return "TRUNK";
        }
    }

    return "ACCESS";
}

void menu_generate_guide(const std::vector<Device*>& devices, const std::vector<Link*>& links, const std::vector<Network*>& subnets) {
    std::cout << "\n" << MAGENTA << "================ EXAM GUIDE ================" << RESET << "\n";
    
    // SECTION 1: Physical Connections
    std::cout << "\n" << MAGENTA << "### PHYSICAL CONNECTIONS ###" << RESET << "\n";
    for (auto l : links) {
        std::cout << "Connect " << GREEN << l->device1->get_hostname() << RESET << " " << BLUE << l->port1 << RESET
                  << " to " << GREEN << l->device2->get_hostname() << RESET << " " << BLUE << l->port2 << RESET
                  << " using a [" << WHITE << l->get_cable_type_str() << RESET << "].\n";
    }

    // SECTION 2: Switch Configurations
    std::cout << "\n" << MAGENTA << "### SWITCH CONFIGURATIONS ###" << RESET << "\n";
    for (auto d : devices) {
        if (d->get_type() != DeviceType::SWITCH) continue;
        
        Switch* sw = dynamic_cast<Switch*>(d);
        std::string uplink_mode = get_uplink_mode_from_router(sw, links, subnets);

        std::cout << "\n" << GREEN << "--- " << sw->get_hostname() << " ---" << RESET << "\n";
        std::cout << YELLOW << "enable" << RESET << "\n";
        std::cout << YELLOW << "conf t" << RESET << "\n";
        std::cout << YELLOW << "hostname " << WHITE << sw->get_hostname() << RESET << "\n";
        std::cout << YELLOW << "enable secret " << WHITE << "class" << RESET << "\n";
        
        // Step A: Determine used VLANs
        std::map<int, std::string> used_vlans;
        for (auto& iface : sw->interfaces) {
            if (iface.vlan_id > 1) {
                if (VlanManager::vlan_exists(iface.vlan_id)) {
                    used_vlans[iface.vlan_id] = VlanManager::get_vlan_name(iface.vlan_id);
                } else {
                    used_vlans[iface.vlan_id] = iface.vlan_name.empty() ? "VLAN" + std::to_string(iface.vlan_id) : iface.vlan_name;
                }
            }
        }
        for (auto const& [vid, vname] : VlanManager::defined_vlans) {
            if (vid > 1 && used_vlans.find(vid) == used_vlans.end()) {
                used_vlans[vid] = vname;
            }
        }
        
        // Step B: Management Interface & Gateway
        if (sw->get_hostname() == "Switch1") {
            // LAN C: Network 192.168.1.96/27 (.97 - .126)
            // Req: Highest IP (.126)
            // Req: Gateway (.97 - Router1)
            std::cout << CYAN << "!\n! Management Interface (Switch1 - LAN C)" << RESET << "\n";
            std::cout << YELLOW << "interface " << BLUE << "vlan 1" << RESET << "\n";
            std::cout << YELLOW << " ip address " << WHITE << "192.168.1.126 255.255.255.224" << RESET << "\n";
            std::cout << GREEN << " no shutdown" << RESET << "\n exit\n";
            std::cout << YELLOW << "ip default-gateway " << WHITE << "192.168.1.97" << RESET << "\n";
            
            // SSH Config
            std::cout << CYAN << "!\n! SSH Configuration" << RESET << "\n";
            std::cout << YELLOW << "ip domain-name " << WHITE << "lab.local" << RESET << "\n";
            std::cout << YELLOW << "crypto key generate rsa modulus 1024" << RESET << "\n";
            std::cout << YELLOW << "username " << WHITE << "admin" << YELLOW << " secret " << WHITE << "class" << RESET << "\n"; 
        }
        else if (sw->get_hostname() == "Switch2") {
            // Req: Specific IP 192.168.50.2/24
            std::cout << CYAN << "!\n! Management Interface (Switch2 Special)" << RESET << "\n";
            std::cout << YELLOW << "interface " << BLUE << "vlan 1" << RESET << "\n";
            std::cout << YELLOW << " ip address " << WHITE << "192.168.50.2 255.255.255.0" << RESET << "\n";
            std::cout << GREEN << " no shutdown" << RESET << "\n exit\n";
        }
        else if (sw->get_hostname() == "Switch0") {
            // Exam Req: Mgmt IP 192.168.1.34 on VLAN 10, Gateway .33
            std::cout << CYAN << "!\n! Management Interface (Switch0 - LAN A/B Config)" << RESET << "\n";
            std::cout << YELLOW << "interface " << BLUE << "vlan 10" << RESET << "\n";
            std::cout << YELLOW << " ip address " << WHITE << "192.168.1.34 255.255.255.224" << RESET << "\n";
            std::cout << GREEN << " no shutdown" << RESET << "\n exit\n";
            std::cout << YELLOW << "ip default-gateway " << WHITE << "192.168.1.33" << RESET << "\n";
        }
        
        // Step C: Create VLANs (Only if trunking)
        if (uplink_mode == "TRUNK" && !used_vlans.empty()) {
            std::cout << CYAN << "!\n! VLAN Definitions" << RESET << "\n";
            for (auto const& [id, name] : used_vlans) {
                std::cout << YELLOW << "vlan " << WHITE << id << RESET << "\n";
                std::cout << YELLOW << " name " << WHITE << name << RESET << "\n exit\n";
            }
        }
        
        // Step D: Uplink Ports
        std::cout << CYAN << "!\n! Uplink Ports" << RESET << "\n";
        std::cout << CYAN << "! Connection-based mode: " << (uplink_mode == "TRUNK" ? "TRUNK (ROAS)" : "ACCESS (Standard)") << RESET << "\n";
        
        // Gig0/1
        std::cout << YELLOW << "interface " << BLUE << "Gig0/1" << RESET << "\n";
        if (uplink_mode == "TRUNK") {
            std::cout << YELLOW << " switchport mode " << GREEN << "trunk" << RESET << "\n";
        } else {
            std::cout << YELLOW << " switchport mode " << GREEN << "access" << RESET << "\n";
            std::cout << YELLOW << " no switchport trunk allowed vlan" << RESET << " " << CYAN << "! Safety" << RESET << "\n";
        }
        std::cout << " exit\n";

        // Gig0/2
        std::cout << YELLOW << "interface " << BLUE << "Gig0/2" << RESET << "\n";
        if (uplink_mode == "TRUNK") {
            std::cout << YELLOW << " switchport mode " << GREEN << "trunk" << RESET << "\n";
        } else {
            std::cout << YELLOW << " switchport mode " << GREEN << "access" << RESET << "\n";
            std::cout << YELLOW << " no switchport trunk allowed vlan" << RESET << " " << CYAN << "! Safety" << RESET << "\n";
        }
        std::cout << " exit\n";
        
        // Step E: Access Ports
        bool has_access = false;
        for (auto& iface : sw->interfaces) {
            if (!iface.is_trunk && iface.vlan_id > 1) {
                if (!has_access) {
                    std::cout << CYAN << "!\n! Access Ports" << RESET << "\n";
                    has_access = true;
                }
                std::cout << YELLOW << "interface " << BLUE << iface.name << RESET << "\n";
                // Even if uplink is access, we might still assign a VLAN to a local device if requested
                std::cout << YELLOW << " switchport mode " << GREEN << "access" << RESET << "\n";
                std::cout << YELLOW << " switchport access vlan " << WHITE << iface.vlan_id << RESET << "\n";
                std::cout << " exit\n";
            }
        }
        
        // Step F: VTY Config
        std::cout << CYAN << "!\n! VTY Configuration" << RESET << "\n";
        std::cout << YELLOW << "line vty " << WHITE << "0 15" << RESET << "\n";
        std::cout << YELLOW << " password " << WHITE << "admin" << RESET << "\n";
        
        if (sw->get_hostname() == "Switch1") {
            std::cout << YELLOW << " transport input " << GREEN << "ssh" << RESET << "\n";
        } else if (sw->get_hostname() == "Switch2") {
            std::cout << YELLOW << " transport input " << GREEN << "telnet" << RESET << "\n";
        }
        
        std::cout << YELLOW << " login" << RESET << "\n";
        if(sw->get_hostname() == "Switch1") std::cout << YELLOW << " login local" << RESET << "\n"; // SSH needs local or tacacs, usually local for lab
        
        std::cout << " exit\n";
        
        std::cout << CYAN << "!" << RESET << "\n" << RED << "end" << RESET << "\n" << RED << "wr" << RESET << "\n";

        // --- VERIFICATION COMMANDS (Switch) ---
        std::cout << CYAN << "\n! --- VERIFICATION COMMANDS ---" << RESET << "\n";
        std::cout << WHITE << "show vlan brief" << RESET << "\n";
        if(sw->get_hostname() == "Switch0" || sw->get_hostname() == "Switch2") {
            std::cout << WHITE << "show interfaces trunk" << RESET << "\n";
        }
        std::cout << WHITE << "show ip interface brief" << RESET << "\n";
        if(sw->get_hostname() == "Switch1") {
            std::cout << WHITE << "show ip ssh" << RESET << "\n";
        }
    }

    // SECTION 3: Router Configurations
    std::cout << "\n" << MAGENTA << "### ROUTER CONFIGURATIONS ###" << RESET << "\n";
    for (auto d : devices) {
        if (d->get_type() != DeviceType::ROUTER) continue;
        
        Router* r = dynamic_cast<Router*>(d);
        std::cout << "\n" << RED << "--- " << r->get_hostname() << " ---" << RESET << "\n";
        std::cout << YELLOW << "enable" << RESET << "\n";
        std::cout << YELLOW << "conf t" << RESET << "\n";
        std::cout << YELLOW << "hostname " << WHITE << r->get_hostname() << RESET << "\n";
        std::cout << YELLOW << "enable secret " << WHITE << "class" << RESET << "\n";
        
        std::vector<Network*> router_subnets;
        for (auto n : subnets) {
            if (n->is_split) continue;
            if (n->get_assignment().find(r->get_hostname()) != std::string::npos) {
                router_subnets.push_back(n);
            }
        }
        
        // Use global device index to match main.cpp
        int this_router_idx = -1;
        for (size_t k = 0; k < devices.size(); ++k) {
            if (devices[k] == d) {
                this_router_idx = (int)k;
                break;
            }
        }
        
        std::set<std::string> base_interfaces_used;
        
        // PASS 0: WAN Peers
        for (auto n : subnets) {
            if (n->is_split || n->get_slash() != 30) continue;
            std::string assignment = n->get_assignment();
            if (assignment.find(r->get_hostname()) != std::string::npos) continue;
            
            for (auto link : links) {
                Device* other_device = (link->device1 == r) ? link->device2 : (link->device2 == r ? link->device1 : nullptr);
                std::string my_port = (link->device1 == r) ? link->port1 : (link->device2 == r ? link->port2 : "");
                
                if (other_device && other_device->get_type() == DeviceType::ROUTER) {
                    if (assignment.find(other_device->get_hostname()) != std::string::npos) {
                        std::string peer_ip = address_to_str(n->get_address() + 2);
                        std::string mask_str = address_to_str(n->get_mask());
                        std::cout << CYAN << "!\n! WAN Peer Interface (Link to " << other_device->get_hostname() << ")" << RESET << "\n";
                        std::cout << YELLOW << " interface " << BLUE << my_port << RESET << "\n";
                        
                        // Smart IP for WAN Peer (Router side of things)
                        // If I am Router0, I usually take .1. If Router1, .2
                        // But strictly we rely on the logic:
                        // If n->gateway_manual_ip is set for THIS router on this subnet, use it.
                        // Wait, 'n' is the subnet. 'gateway_manual_ip' is usually for the Router that OWNS the subnet (Gateway).
                        // WAN links are shared. We need to know WHICH IP belongs to THIS router.
                        // For WAN, we don't store individual IPs in 'Network' struct easily for both sides.
                        // We'll trust the Smart Default here relative to Hostname for simplicity, or look at Interface manual_ip.
                        
                        std::string my_ip;
                        Interface* my_iface = r->get_interface(my_port);
                        if (my_iface && !my_iface->manual_ip.empty()) {
                            my_ip = my_iface->manual_ip;
                        } else {
                            // Fallback Smart Default
                            if (r->get_hostname() == "Router1") my_ip = address_to_str(n->get_address() + 2);
                            else my_ip = address_to_str(n->get_address() + 1);
                        }
                        
                        std::cout << YELLOW << " ip address " << WHITE << my_ip << " " << mask_str << RESET << "\n";
                        
                        // Clock Rate logic for DCE (Router0)
                        if ((my_port.find("Se") == 0 || my_port.find("se") == 0) && (this_router_idx == 0 || r->get_hostname() == "Router0")) {
                             std::cout << YELLOW << " clock rate 64000" << RESET << "\n";
                        }
                        
                        std::cout << GREEN << " no shutdown" << RESET << "\n exit\n";
                        break;
                    }
                }
            }
        }
        
        // PASS 1: Interfaces
        for (auto n : router_subnets) {
            std::string iface_name = n->get_assigned_interface();
            int vlan_id = n->associated_vlan_id;
            int cidr = n->get_slash();
            std::string mask_str = address_to_str(n->get_mask());
            // Gateway IP Logic (Smart Defaults & Manual Override)
            std::string gateway_str;
            if (!n->gateway_manual_ip.empty()) {
                gateway_str = n->gateway_manual_ip;
            } else {
                // Smart Defaults
                if (cidr == 30) {
                     // WAN Link
                     if (r->get_hostname() == "Router1") {
                         // Router1 gets the second/last usable IP on WAN
                         gateway_str = address_to_str(n->get_address() + 2);
                     } else {
                         // Router0 or others get the first usable
                         gateway_str = address_to_str(n->get_address() + 1);
                     }
                } else {
                    // Standard LAN Gateway: First Usable
                    gateway_str = address_to_str(n->get_address() + 1);
                }
            }
            
            std::string dhcp_mode_tag = "";
            if (cidr >= 30) dhcp_mode_tag = std::string(CYAN) + "! DHCP Mode: None (WAN Link)" + RESET;
            else if (!n->dhcp_enabled) dhcp_mode_tag = std::string(CYAN) + "! DHCP Mode: None (Static)" + RESET;
            else if (!n->dhcp_helper_ip.empty()) dhcp_mode_tag = std::string(CYAN) + "! DHCP Mode: Relay -> " + YELLOW + n->dhcp_helper_ip + RESET;
            else dhcp_mode_tag = n->dhcp_upper_half_only ? std::string(CYAN) + "! DHCP Mode: Server (Highest Half Exclusion)" + RESET : std::string(CYAN) + "! DHCP Mode: Server (Standard)" + RESET;

            if (vlan_id > 1) {
                std::string base_iface = iface_name;
                size_t dot = iface_name.find('.');
                if (dot != std::string::npos) base_iface = iface_name.substr(0, dot);
                base_interfaces_used.insert(base_iface);
                
                std::cout << CYAN << "!\n! VLAN " << vlan_id << " Subinterface" << RESET << "\n" << dhcp_mode_tag << "\n";
                std::cout << YELLOW << "interface " << BLUE << iface_name << RESET << "\n";
                std::cout << YELLOW << " encapsulation dot1q " << WHITE << vlan_id << RESET << "\n";
                std::cout << YELLOW << " ip address " << WHITE << gateway_str << " " << mask_str << RESET << "\n";
                if (n->dhcp_enabled && !n->dhcp_helper_ip.empty()) std::cout << YELLOW << " ip helper-address " << MAGENTA << n->dhcp_helper_ip << RESET << "\n";
                std::cout << GREEN << " no shutdown" << RESET << "\n exit\n";
            } else {
                std::cout << CYAN << "!" << RESET << "\n";
                if (cidr == 30) std::cout << CYAN << "! WAN Interface (/30)" << RESET << "\n";
                else std::cout << CYAN << "! Physical LAN Interface" << RESET << "\n" << dhcp_mode_tag << "\n";
                std::cout << YELLOW << " interface " << BLUE << iface_name << RESET << "\n";
                std::cout << YELLOW << " ip address " << WHITE << gateway_str << " " << mask_str << RESET << "\n";
                if (n->dhcp_enabled && !n->dhcp_helper_ip.empty()) std::cout << YELLOW << " ip helper-address " << MAGENTA << n->dhcp_helper_ip << RESET << "\n";
                
                // Clock Rate logic for DCE (Router0)
                if ((iface_name.find("Se") == 0 || iface_name.find("se") == 0) && (this_router_idx == 0 || r->get_hostname() == "Router0")) {
                        std::cout << YELLOW << " clock rate 64000" << RESET << "\n";
                }
                
                std::cout << GREEN << " no shutdown" << RESET << "\n exit\n";
            }
        }
        
        for (const auto& base : base_interfaces_used) {
            std::cout << CYAN << "!\n! Enable trunk interface" << RESET << "\n";
            std::cout << YELLOW << "interface " << BLUE << base << RESET << "\n" << GREEN << " no shutdown" << RESET << "\n exit\n";
        }
        
        // PASS 1.5: Static Routing
        bool has_static = false;
        for(const auto& sr : static_routes) {
            if(sr.router_id == this_router_idx) {
                if(!has_static) {
                    std::cout << CYAN << "!\n! --- Static Routing ---" << RESET << "\n";
                }
                std::cout << YELLOW << "ip route " << WHITE << sr.dest_net << " " << sr.mask << " " << sr.next_hop << RESET << "\n";
            }
        }
        
        // PASS 2: DHCP Pools
        struct DHCPPool { std::string name; std::string network; std::string mask; std::string gateway; bool upper_half_only; };
        std::vector<DHCPPool> dhcp_pools;

        for (auto n : subnets) {
            if (n->is_split) continue;
            
            bool should_generate = false;
            if (n->dhcp_enabled) {
                if (n->dhcp_server_id != -1) {
                    // Check absolute device ID (harmonized with main.cpp)
                    if (n->dhcp_server_id == this_router_idx) {
                        should_generate = true;
                    }
                } else {
                    // ID is -1. Check if implicit local (No helper IP + Assigned to this router)
                    if (n->dhcp_helper_ip.empty() && n->get_assignment().find(r->get_hostname()) != std::string::npos) {
                        should_generate = true;
                    }
                }
            }
            
            if (should_generate) {
                std::string pool_name = !n->name.empty() ? "POOL_" + n->name : (n->associated_vlan_id > 1 ? "POOL_VLAN" + std::to_string(n->associated_vlan_id) : "POOL_LAN");
                std::replace(pool_name.begin(), pool_name.end(), ' ', '_');
                dhcp_pools.push_back({pool_name, address_to_str(n->get_address()), address_to_str(n->get_mask()), address_to_str(n->get_address() + 1), n->dhcp_upper_half_only});
            }
        }
        
        if (!dhcp_pools.empty()) {
            std::cout << CYAN << "!\n! --- DHCP Configuration ---" << RESET << "\n";
            for (const auto& pool : dhcp_pools) {
                unsigned int mask_int = str_to_address(pool.mask);
                int cidr = 0; while((mask_int << cidr) & 0x80000000) cidr++; 
                
                unsigned int net_int = str_to_address(pool.network);
                unsigned int total = (1U << (32 - cidr)); // Total size of subnet
                
                unsigned int ex_start = net_int + 1; // Start excluding from first usable
                unsigned int ex_end = ex_start + 10; // Default: first 10
                
                if (pool.upper_half_only) {
                    // Higher Half Rule: Exclude Lower Half
                    // Lower Half Range: Network ... (Network + Total/2 - 1)
                    // We must exclude from .1 to (Total/2 - 1)
                    // Actually, Gateway is usually .1, so exclude .1 to .((Total/2)-1)
                    // Example /27 (32 IPs): 
                    // Lower: .0 - .15 (Usable .1 - .14). Upper: .16 - .31 (Usable .17 - .30)
                    // Exclude: .1 to .15 (wait, .15 is broadcast of lower? No, just split line)
                    // Correct: Exclude LOWEST IPs. DHCP pool usually serves from top down or bottom up?
                    // Cisco 'network' command enables the whole range. We must exclude the lower chunk.
                    // Exclude range: [First Usable,  MidPoint - 1]
                    // If total=32, half=16. Network=.0. Mid=.16.
                    // We want pool to start at .17? Or .16?
                    // Usually "Higher Half" means the pool is the upper block.
                    // So we exclude the lower block: .1 to .15. 
                    // ex_end = net_int + (total / 2) - 1;
                    ex_end = net_int + (total / 2) - 1;
                }
                
                std::cout << YELLOW << "ip dhcp excluded-address " << WHITE << address_to_str(ex_start) << " " << address_to_str(ex_end) << RESET << "\n";
            }
            std::cout << CYAN << "!" << RESET << "\n";
            for (const auto& pool : dhcp_pools) {
                std::cout << YELLOW << "ip dhcp pool " << WHITE << pool.name << RESET << "\n";
                std::cout << YELLOW << " network " << WHITE << pool.network << " " << pool.mask << RESET << "\n";
                std::cout << YELLOW << " default-router " << WHITE << pool.gateway << RESET << "\n exit\n";
            }
        }
        
        // PASS 3: Generate Static Assignments (LAN C End Devices)
        // If this router serves a static subnet (no DHCP), list the static config for attached devices.
        // This is strictly "Documentation/Guide" output, usually specific to the devices themselves, 
        // but user asked for "Plan" output. We'll append it here.
        
        for (auto n : subnets) {
             if (n->is_split || n->get_slash() >= 30) continue;
             if (!n->dhcp_enabled && n->get_assignment().find(r->get_hostname()) != std::string::npos) {
                 // Static Subnet (e.g. LAN C)
                 std::cout << CYAN << "!\n! --- Static Device Plan for " << n->name << " ---" << RESET << "\n";
                 
                 // PC (Network + 2)
                 std::string pc_ip = address_to_str(n->get_address() + 2);
                 std::cout << CYAN << "! PC " << n->name << ": " << WHITE << pc_ip << " " << address_to_str(n->get_mask()) << " GW: " << address_to_str(n->get_address() + 1) << RESET << "\n";
                 
                 // Laptop (Network + 3)
                 std::string laptop_ip = address_to_str(n->get_address() + 3);
                 std::cout << CYAN << "! Laptop " << n->name << ": " << WHITE << laptop_ip << " " << address_to_str(n->get_mask()) << " GW: " << address_to_str(n->get_address() + 1) << RESET << "\n";
                 
                 // Switch (Last Usable)
                 unsigned int net_int = n->get_address();
                 int cidr = n->get_slash();
                 unsigned int broadcast = net_int | ((1U << (32 - cidr)) - 1);
                 std::string switch_ip = address_to_str(broadcast - 1);
                 std::cout << CYAN << "! Switch " << n->name << ": " << WHITE << switch_ip << " " << address_to_str(n->get_mask()) << " GW: " << address_to_str(n->get_address() + 1) << RESET << "\n";
             }
        }
        
        std::cout << CYAN << "!\n! VTY Configuration" << RESET << "\n";
        std::cout << YELLOW << "line vty " << WHITE << "0 4" << RESET << "\n" << YELLOW << " password " << WHITE << "cisco" << RESET << "\n" << YELLOW << " login" << RESET << "\n exit\n";
        std::cout << CYAN << "!" << RESET << "\n" << RED << "end" << RESET << "\n" << RED << "wr" << RESET << "\n";
        
        // --- VERIFICATION COMMANDS (Router) ---
        // --- VERIFICATION COMMANDS (Router) ---
        std::cout << CYAN << "\n! --- VERIFICATION COMMANDS ---" << RESET << "\n";
        std::cout << WHITE << "show ip interface brief" << RESET << "\n";
        std::cout << WHITE << "show ip route" << RESET << "\n";
        if(r->get_hostname() == "Router1") {
             std::cout << WHITE << "show ip dhcp binding" << RESET << "\n";
        }
        std::cout << WHITE << "show running-config | section dhcp" << RESET << "\n";
        if(r->get_hostname() == "Router0") {
             std::cout << WHITE << "show controllers se0/1/0" << RESET << "\n";
        }
    }
}
