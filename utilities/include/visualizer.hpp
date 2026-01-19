#ifndef VISUALIZER_HPP
#define VISUALIZER_HPP

#include <vector>
#include <string>
#include <set>
#include "topology.hpp"
#include "network.hpp"

class Visualizer {
public:
    static void draw(const std::vector<Device*>& devices, const std::vector<Link*>& links, const std::vector<Network*>& subnets);

private:
    static void print_node(Device* dev, std::string prefix, bool is_last, std::set<std::string>& visited, 
                           const std::vector<Link*>& links, const std::vector<Network*>& subnets);
};

#endif
