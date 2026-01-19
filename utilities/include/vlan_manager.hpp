#ifndef VLAN_MANAGER_HPP
#define VLAN_MANAGER_HPP

#include <string>
#include <map>
#include <vector>
#include "topology.hpp"

class VlanManager {
public:
    // Global database of VLANs: ID -> Name
    static std::map<int, std::string> defined_vlans;

    // Initialize with default VLAN 1
    static void init();

    // CRUD
    static void add_vlan(int id, const std::string& name);
    static void delete_vlan(std::vector<Device*>& devices);
    static bool vlan_exists(int id);
    static std::string get_vlan_name(int id);
    
    // UI Helpers
    static void menu_manage_vlans(std::vector<Device*>& devices);
    static void inspect_switch_ports(std::vector<Device*>& devices);
    
    // Assignment Helpers
    static void assign_vlan_to_ports(Switch* sw, const std::vector<std::string>& ports, int vlan_id, bool is_trunk);
    
    // Generic Helper
    static std::vector<std::string> parse_interface_range(const std::string& input);
};

#endif
