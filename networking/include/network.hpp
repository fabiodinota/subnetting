#ifndef NETWORK_HPP
#define NETWORK_HPP
#include <string>
#include <vector>
#define IPV4_NET_BITS 32

class Network
{
private:
    int address;
    int mask;
    int slash;
    int broadcast;

public:
    Network();
    void print_details();

    int get_address();
    int get_mask();
    int get_slash();
    int get_broadcast();

    void set_address(int address);
    void set_mask(int mask);
    void set_slash(int slash);
    void set_broadcast(int broadcast);

    // Assignment Tag for UI
    std::string assignment_tag = "Free";
    std::string assigned_interface = "";
    
    void set_assignment(std::string tag) { assignment_tag = tag; }
    std::string get_assignment() { return assignment_tag; }
    
    void set_assigned_interface(std::string iface) { assigned_interface = iface; }
    std::string get_assigned_interface() { return assigned_interface; }

    bool is_split = false;
    std::string name = "";
    std::string vlan_name = "default";
    
    // Manual IP Override
    std::string manual_ip = ""; // If set, overrides default/DHCP assigned IP


    // VLAN Association
    int associated_vlan_id = 0; // 0 = Physical/No VLAN

    // DHCP Control Flags
    bool dhcp_enabled = false;          // Whether to generate DHCP pool
    bool dhcp_upper_half_only = false;  // Exam mode: exclude lower half
    int dhcp_server_id = -1;            // ID of router serving DHCP (-1 = local/current)
    std::string dhcp_helper_ip = "";    // IP address of remote DHCP server (for ip helper-address)
    
    // Manual IP Override
    std::string gateway_manual_ip = ""; // If set, overrides automatic gateway calculation


    // Hierarchy
    int id = 0;
    int parent_id = 0; // 0 means root
    std::vector<int> children_ids;
};

std::string address_to_str(int address);
unsigned int str_to_address(const std::string& ip);

#endif