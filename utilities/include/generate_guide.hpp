#ifndef GENERATE_GUIDE_HPP
#define GENERATE_GUIDE_HPP

#include <vector>
#include <string>
#include "topology.hpp"
#include "network.hpp"

// Prototypes for guide generation
void menu_generate_guide(const std::vector<Device*>& devices, const std::vector<Link*>& links, const std::vector<Network*>& subnets);

#endif
