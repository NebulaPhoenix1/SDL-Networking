cmake_minimum_required(VERSION 3.15)
project(DualSocketGame CXX)


set(CMAKE_BUILD_RPATH "/usr/local/lib;/opt/homebrew/lib")
set(CMAKE_INSTALL_RPATH "/usr/local/lib;/opt/homebrew/lib")

# Use C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the SDL3 libraries installed on your system
find_package(SDL3 REQUIRED)
# Helps M-Series Macs find Homebrew headers
include_directories(/opt/homebrew/include) 
# Helps Intel Macs find global headers
include_directories(/usr/local/include)

# Tell CMake where to look for Shared.h
include_directories(SharedCode)

# --- BUILD SERVER ---
# Point to the Game_Server.cpp inside its folder
add_executable(Server SDL3_Networking_Game_Server/Game_Server.cpp) 
target_link_libraries(Server PRIVATE SDL3::SDL3)
target_link_libraries(Server PRIVATE SDL3_net) 

# --- BUILD CLIENT ---
# Point to the Game_Client.cpp inside its folder
add_executable(Client SDL3_Networking_Game_Client/Game_Client.cpp)
target_link_libraries(Client PRIVATE SDL3::SDL3)
target_link_libraries(Client PRIVATE SDL3_net)