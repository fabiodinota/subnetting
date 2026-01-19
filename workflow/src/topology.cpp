#include <topology.hpp>
#include <iostream>
#include <algorithm>

// --- Device ---
Device::Device(std::string name, DeviceType t, std::string m)
    : hostname(name), type(t), model(m) {}

void Device::add_interface(const std::string& name) {
    interfaces.push_back({name, false});
}

Interface* Device::get_interface(const std::string& name) {
    for (auto& iface : interfaces) {
        if (iface.name == name) {
            return &iface;
        }
    }
    return nullptr;
}

std::vector<std::string> Device::get_available_ports() const {
    std::vector<std::string> avail;
    for (const auto& iface : interfaces) {
        if (!iface.is_connected) {
            avail.push_back(iface.name);
        }
    }
    return avail;
}

Device* Device::get_device_by_name(const std::vector<Device*>& devices, const std::string& name) {
    for (auto d : devices) {
        if (d->get_hostname() == name) {
            return d;
        }
    }
    return nullptr;
}

void Device::connect(const std::string& my_port_name, Device* other_dev, const std::string& other_port_name) {
    // Bug 1 Fix: Check if interfaces exist; if not, create them dynamically.
    // Also ensuring no duplicates if it already exists.
    
    Interface* my_iface = get_interface(my_port_name);
    if (!my_iface) {
        add_interface(my_port_name);
        my_iface = get_interface(my_port_name);
    }

    Interface* other_iface = other_dev->get_interface(other_port_name);
    if (!other_iface) {
        other_dev->add_interface(other_port_name);
        other_iface = other_dev->get_interface(other_port_name);
    }

    if (my_iface->is_connected || other_iface->is_connected) {
        std::cerr << "Error: One of the interfaces is already connected.\n";
        return; 
    }

    my_iface->is_connected = true;
    my_iface->neighbor = other_dev;
    my_iface->neighbor_port = other_port_name;

    other_iface->is_connected = true;
    other_iface->neighbor = this;
    other_iface->neighbor_port = my_port_name;
}

// Disconnects all interfaces on this device
void Device::disconnect_all_interfaces() {
    for (auto& iface : interfaces) {
        iface.is_connected = false;
        iface.neighbor = nullptr;
        iface.neighbor_port = "";
    }
}

void Device::remove_neighbor_references(Device* target) {
    for (auto& iface : interfaces) {
        if (iface.is_connected && iface.neighbor == target) {
            iface.is_connected = false;
            iface.neighbor = nullptr;
            iface.neighbor_port = "";
        }
    }
}

// --- Router ---
Router::Router(std::string name) : Device(name, DeviceType::ROUTER, "ISR4331") {
    // Default interfaces for a standard router (Gig0/X and Se0/X/X)
    add_interface("Gig0/0");
    add_interface("Gig0/1");
    add_interface("Gig0/2");
    add_interface("Se0/1/0");
    add_interface("Se0/1/1");
}

void Router::configure_roas(int sub_id, int vlan_id, std::string ip, std::string mask, std::string iface_name) {
    if (iface_name.empty()) {
        // Fallback default logic if not provided
        // But better to store empty and handle display logic later, or build it here.
        // Let's store whatever is passed.
    }
    subinterfaces.push_back({sub_id, vlan_id, ip, mask, iface_name});
}

void Router::add_dhcp_pool(std::string name, std::string net, std::string mask, std::string gateway) {
    dhcp_pools.push_back({name, net, mask, gateway});
}

// --- Switch ---
Switch::Switch(std::string name) : Device(name, DeviceType::SWITCH, "2960") {
    // Default interfaces for a standard switch (24 FE + 2 GE)
    // Fa0/X and Gig0/X
    for (int i = 1; i <= 24; ++i) {
        add_interface("Fa0/" + std::to_string(i));
    }
    add_interface("Gig0/1");
    add_interface("Gig0/2");
}

void Switch::add_vlan(int id, std::string name) {
    vlans.push_back({id, name});
}

void Switch::configure_access_port(std::string interface, int vlan_id) {
    // Check if config exists, update or add
    bool found = false;
    for (auto& cfg : port_configs) {
        if (cfg.interface_name == interface) {
            cfg.mode = PortMode::ACCESS;
            cfg.vlan_id = vlan_id;
            found = true;
            break;
        }
    }
    if (!found) {
        port_configs.push_back({interface, PortMode::ACCESS, vlan_id, false});
    }
}

void Switch::configure_trunk_port(std::string interface) {
    bool found = false;
    for (auto& cfg : port_configs) {
        if (cfg.interface_name == interface) {
            cfg.mode = PortMode::TRUNK;
            cfg.nonegotiate = true; // Defaulting based on requirements
            found = true;
            break;
        }
    }
    if (!found) {
        port_configs.push_back({interface, PortMode::TRUNK, 0, true});
    }
}

// --- PC ---
PC::PC(std::string name) : Device(name, DeviceType::PC, "Generic") {
    add_interface("Fa0");
}

// --- Link ---
Link::Link(Device* d1, std::string p1, Device* d2, std::string p2)
    : device1(d1), port1(p1), device2(d2), port2(p2) {
    
    // Use the bidirectional connect method
    d1->connect(p1, d2, p2);

    determine_cable_type();
}

void Link::determine_cable_type() {
    // Logic: 
    // Like Devices (R-R, S-S, PC-PC) -> Crossover
    // R-PC -> Crossover (Technically they are 'Like' in terms of MDI)
    // S-R, S-PC -> Straight
    
    // Group 1: Router, PC
    // Group 2: Switch
    
    auto get_group = [](DeviceType t) {
        if (t == DeviceType::SWITCH) return 2;
        return 1; // ROUTER or PC
    };

    int g1 = get_group(device1->get_type());
    int g2 = get_group(device2->get_type());

    // Bug 2 Fix: Serial Cable Detection
    // Check interfaces first
    auto is_serial = [](const std::string& p) {
        // Check for 's', 'S', 'se', 'Se', 'Ser', etc.
        // Simplest: first char is 's' or 'S'
        if (p.empty()) return false;
        char c = std::tolower(p[0]);
        return c == 's';
    };

    if (is_serial(port1) || is_serial(port2)) {
        type = CableType::SERIAL;
        return; 
    }

    if (g1 == g2) {
        type = CableType::CROSSOVER;
    } else {
        type = CableType::STRAIGHT_THROUGH;
    }
}

std::string Link::get_cable_type_str() const {
    switch (type) {
        case CableType::CROSSOVER: return "Crossover Cable";
        case CableType::STRAIGHT_THROUGH: return "Copper Straight-Through";
        case CableType::SERIAL: return "Serial Cable";
        default: return "Unknown Cable";
    }
}
