#ifndef STATE_MANAGER_HPP
#define STATE_MANAGER_HPP

#include <vector>
#include <string>
#include <string>
#include "topology.hpp"

class Network;

class StateManager {
public:
    static void save(const std::vector<Device*>& devices, const std::vector<Link*>& links, const std::vector<Network*>& subnets);
    static void load(std::vector<Device*>& devices, std::vector<Link*>& links, std::vector<Network*>& subnets);
    static bool load_scenario(const std::string& filename, std::vector<Device*>& devices, std::vector<Link*>& links, std::vector<Network*>& subnets);
};

#endif
