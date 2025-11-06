cmake_minimum_required(VERSION 3.10)
project(Connect4_SQL_SFML VERSION 1.0 LANGUAGES CXX)

# ===============================
# C++ Setup
# ===============================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ===============================
# SFML 2.5.1 setup
# ===============================
set(SFML_ROOT "C:/SFML-2.5.1")
set(SFML_DIR "${SFML_ROOT}/lib/cmake/SFML")
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)
include_directories(${SFML_ROOT}/include)

# ===============================
# MySQL Connector setup
# ===============================
set(MYSQL_ROOT "C:/mysql-connector-c++-9.5.0-winx64")
include_directories(${MYSQL_ROOT}/include ${MYSQL_ROOT}/include/jdbc)
link_directories(${MYSQL_ROOT}/lib64/vs14)

# ===============================
# Executable
# ===============================
add_executable(Connect4Game main.cpp)

# ===============================
# Link libraries
# ===============================
target_link_libraries(Connect4Game PRIVATE
    sfml-graphics
    sfml-window
    sfml-system
    "C:/mysql-connector-c++-9.5.0-winx64/lib64/vs14/mysqlcppconn.lib"
)

# ===============================
# Post-build DLL copy (for runtime)
# ===============================
add_custom_command(TARGET Connect4Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SFML_ROOT}/bin/sfml-graphics-2.dll"
        "${SFML_ROOT}/bin/sfml-window-2.dll"
        "${SFML_ROOT}/bin/sfml-system-2.dll"
        "${MYSQL_ROOT}/lib64/mysqlcppconn-10-vs14.dll"
        "${MYSQL_ROOT}/lib64/libssl-3-x64.dll"
        "${MYSQL_ROOT}/lib64/libcrypto-3-x64.dll"
        $<TARGET_FILE_DIR:Connect4Game>
)
