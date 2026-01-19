#include "documentation.hpp"
#include <iostream>
#include <limits>
#include <string>

// ANSI escape codes for formatting
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define CYAN    "\033[36m"

namespace {
    void wait_for_enter() {
        std::cout << "\n" << YELLOW << "Press Enter to continue..." << RESET;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        // If buffer was empty, ignore might wait for input, or if there was a leftover newline it consumes it.
        // A safer way often depends on previous input state. 
        // Since this is called after a menu selection which consumes int, usually there's a newline left.
        // We might just use cin.get() but let's stick to a robust clear.
        // Actually, main menu uses cin >> option. The utility clear_input() is used there.
        // So here we likely just need one reading.
        std::cin.get(); 
    }

    void show_basic_config() {
        std::cout << "\n" << BOLD << "=== Module 1: Basic Device Configuration ===" << RESET << "\n";
        std::cout << "- Set Hostname:      " << CYAN << "hostname <name>" << RESET << "\n";
        std::cout << "- DNS Lookup:        " << CYAN << "no ip domain-lookup" << RESET << " (Crucial for exams)\n";
        std::cout << "- Password Security: " << CYAN << "service password-encryption" << RESET << "\n";
        std::cout << "- Enable Secret:     " << CYAN << "enable secret <password>" << RESET << "\n";
        std::cout << "- SSH Access:\n";
        std::cout << "    " << CYAN << "line vty 0 4" << RESET << "\n";
        std::cout << "    " << CYAN << "transport input ssh" << RESET << "\n";
        std::cout << "    " << CYAN << "login local" << RESET << "\n";
        wait_for_enter();
    }

    void show_vlans() {
        std::cout << "\n" << BOLD << "=== Module 3: VLANs & Trunking ===" << RESET << "\n";
        std::cout << "- Create VLAN:       " << CYAN << "vlan 10, name STUDENT" << RESET << "\n";
        std::cout << "- Access Ports:      " << CYAN << "switchport mode access, switchport access vlan 10" << RESET << "\n";
        std::cout << "- Trunk Ports:       " << CYAN << "switchport mode trunk, switchport trunk native vlan 99" << RESET << "\n";
        std::cout << "- Disable DTP:       " << CYAN << "switchport nonegotiate" << RESET << "\n";
        std::cout << "- Verification:      " << CYAN << "show vlan brief, show interfaces trunk" << RESET << "\n";
        wait_for_enter();
    }

    void show_inter_vlan() {
        std::cout << "\n" << BOLD << "=== Module 4: Inter-VLAN Routing (ROAS) ===" << RESET << "\n";
        std::cout << "- Physical Interface:" << CYAN << "interface g0/0/1, no shutdown" << RESET << " (NO IP here!)\n";
        std::cout << "- Subinterface:      " << CYAN << "interface g0/0/1.10" << RESET << "\n";
        std::cout << "- Encapsulation:     " << CYAN << "encapsulation dot1q 10" << RESET << " (Must be first)\n";
        std::cout << "- IP Address:        " << CYAN << "ip address 192.168.10.1 255.255.255.0" << RESET << "\n";
        wait_for_enter();
    }

    void show_dhcp() {
        std::cout << "\n" << BOLD << "=== Module 7: DHCPv4 Configuration ===" << RESET << "\n";
        std::cout << "- Exclude Addr:      " << CYAN << "ip dhcp excluded-address x.x.x.x x.x.x.x" << RESET << "\n";
        std::cout << "- Create Pool:       " << CYAN << "ip dhcp pool NAME" << RESET << "\n";
        std::cout << "- Network:           " << CYAN << "network 192.168.10.0 255.255.255.0" << RESET << "\n";
        std::cout << "- Default Gateway:   " << CYAN << "default-router 192.168.10.1" << RESET << "\n";
        std::cout << "- Relay Agent:       " << CYAN << "ip helper-address <server_ip>" << RESET << " (on Router interfaces)\n";
        wait_for_enter();
    }

    void show_routing() {
        std::cout << "\n" << BOLD << "=== Module 14-16: Static & Default Routing ===" << RESET << "\n";
        std::cout << "- Standard Static:   " << CYAN << "ip route <dest_net> <mask> <next_hop_ip>" << RESET << "\n";
        std::cout << "- Default Route:     " << CYAN << "ip route 0.0.0.0 0.0.0.0 <next_hop_ip>" << RESET << "\n";
        std::cout << "- Floating Static:   Add distance at end (e.g., 200)\n";
        std::cout << "- Verification:      " << CYAN << "show ip route" << RESET << "\n";
        wait_for_enter();
    }

    void show_nat() {
        std::cout << "\n" << BOLD << "=== Module 6: NAT / PAT (ENSA) ===" << RESET << "\n";
        std::cout << "- Define Interfaces: " << CYAN << "ip nat inside, ip nat outside" << RESET << "\n";
        std::cout << "- PAT (Overload):    " << CYAN << "ip nat inside source list 1 interface g0/0/1 overload" << RESET << "\n";
        std::cout << "- Static NAT:        " << CYAN << "ip nat inside source static <local_ip> <global_ip>" << RESET << "\n";
        wait_for_enter();
    }
}

void Documentation::show_main_menu() {
    int choice = -1;
    while (choice != 0) {
        std::cout << "\n" << BOLD << "=== Knowledge Base ===" << RESET << "\n";
        std::cout << "1. Basic Device Config (Module 1)\n";
        std::cout << "2. VLANs & Trunking (Module 3)\n";
        std::cout << "3. Inter-VLAN Routing (ROAS & SVI) (Module 4)\n";
        std::cout << "4. DHCPv4 Configuration (Module 7)\n";
        std::cout << "5. Static & Default Routing (Module 14-16)\n";
        std::cout << "6. NAT / PAT (Module 6)\n";
        std::cout << "0. Back to Main Menu\n";
        std::cout << "Select topic: ";
        
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        // Consume newline left by cin >> choice so wait_for_enter works cleanly
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        switch(choice) {
            case 1: show_basic_config(); break;
            case 2: show_vlans(); break;
            case 3: show_inter_vlan(); break;
            case 4: show_dhcp(); break;
            case 5: show_routing(); break;
            case 6: show_nat(); break;
            case 0: break;
            default: std::cout << "Invalid option.\n";
        }
    }
}
