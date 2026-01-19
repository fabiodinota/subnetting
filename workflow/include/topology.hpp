#ifndef TOPOLOGY_HPP
#define TOPOLOGY_HPP

#include <string>
#include <vector>
#include <iostream>
#include <memory>

enum class DeviceType {
    ROUTER,
    SWITCH,
    PC
};

enum class CableType {
    CROSSOVER,
    STRAIGHT_THROUGH,
    SERIAL,
    UNKNOWN
};

class Device; 

struct Interface {
    std::string name;
    bool is_connected = false;
    Device* neighbor = nullptr;
    std::string neighbor_port;
    
    // VLAN Configuration
    int vlan_id = 1;
    bool is_trunk = false;
    std::string vlan_name = "default";
    
    // Manual IP Override
    std::string manual_ip = ""; // If set, overrides default/DHCP assigned IP
};

class Device {
protected:
    std::string hostname;
    DeviceType type;
    std::string model;

public:
    std::vector<Interface> interfaces;

    Device(std::string name, DeviceType t, std::string m);
    virtual ~Device() = default;

    struct ManagementConfig {
        std::string management_svi_ip;
        std::string management_gateway; 
        std::string allowed_telnet_ip;
        // If true, use telnet. If false, use ssh.
        bool enable_telnet = true; 
    };
    ManagementConfig management_config;

    std::string get_hostname() const { return hostname; }
    DeviceType get_type() const { return type; }
    std::string get_model() const { return model; }

    // Password Management
    std::string enable_secret = "";
    std::string vty_password = "";
    std::string ssh_username = "";
    std::string ssh_password = "";

    // GUI Visuals
    float x = 0.0f;
    float y = 0.0f;
    struct { float r=1, g=1, b=1, a=1; } color; // Simple color struct to avoid ImGui dependency here

    void add_interface(const std::string& name);
    Interface* get_interface(const std::string& name);
    std::vector<std::string> get_available_ports() const;

    static Device* get_device_by_name(const std::vector<Device*>& devices, const std::string& name);

    void connect(const std::string& my_port_name, Device* other_dev, const std::string& other_port_name);
    
    // Disconnects all interfaces on this device
    // Disconnects all interfaces on this device
    void disconnect_all_interfaces();

    // Remove references to a specific neighbor (used when deleting a device)
    void remove_neighbor_references(Device* target);
};

class Router : public Device {
public:
    Router(std::string name);

    struct SubInterface {
        int id;
        int vlan_id;
        std::string ip_address;
        std::string subnet_mask;
        std::string interface_name; // Full name e.g. g0/1.10
    };

    struct DHCPPool {
        std::string name;
        std::string network; // Network address
        std::string mask;
        std::string default_router;
    };

    std::vector<SubInterface> subinterfaces;
    std::vector<DHCPPool> dhcp_pools;

    void configure_roas(int sub_id, int vlan_id, std::string ip, std::string mask, std::string iface_name = "");
    void add_dhcp_pool(std::string name, std::string net, std::string mask, std::string gateway);
};

class Switch : public Device {
public:
    Switch(std::string name);

    struct Vlan {
        int id;
        std::string name;
    };

    enum class PortMode {
        ACCESS,
        TRUNK,
        DYNAMIC_AUTO
    };

    struct PortConfig {
        std::string interface_name;
        PortMode mode;
        int vlan_id; // For access ports
        bool nonegotiate;
    };

    std::vector<Vlan> vlans;
    std::vector<PortConfig> port_configs;

    void add_vlan(int id, std::string name);
    void configure_access_port(std::string interface, int vlan_id);
    void configure_trunk_port(std::string interface);
};

class PC : public Device {
public:
    PC(std::string name);
};

class Link {
public:
    Device* device1;
    std::string port1;
    Device* device2;
    std::string port2;
    CableType type;

    Link(Device* d1, std::string p1, Device* d2, std::string p2);

    std::string get_cable_type_str() const;

private:
    void determine_cable_type();
};

struct StaticRoute {
    int router_id;
    std::string dest_net;
    std::string mask;
    std::string next_hop;
};

extern std::vector<StaticRoute> static_routes;

#endif
