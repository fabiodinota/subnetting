#ifndef GUI_LAYER_HPP
#define GUI_LAYER_HPP

#include "topology.hpp"
#include <vector>

// Forward declarations if we can't include imgui directly yet
// ideally we would have 'imgui.h' logic here if available.
// For now, we stub the run function.

class GuiLayer {
public:
    static void run(std::vector<Device*>& devices, std::vector<Link*>& links);
};

#endif
