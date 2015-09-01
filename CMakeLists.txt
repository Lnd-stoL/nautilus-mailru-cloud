cmake_minimum_required(VERSION 3.2)
project(nautilus_mailru_cloud)

find_package(PkgConfig)
pkg_check_modules(N_EXT REQUIRED libnautilus-extension)

find_package(Boost 1.54 COMPONENTS thread date_time filesystem system program_options REQUIRED)
find_package(Threads)

include_directories(N_EXT_INCLUDE_DIRS)

#don't know why this is needed
include_directories(
/usr/include/glib-2.0
/usr/lib/x86_64-linux-gnu/glib-2.0/include
/usr/include/gtk-3.0 /usr/include/atk-1.0
/usr/include/at-spi2-atk/2.0
/usr/include/pango-1.0
/usr/include/gio-unix-2.0/
/usr/include/cairo
/usr/include/gdk-pixbuf-2.0
/usr/include/harfbuzz
/usr/include/freetype2
/usr/include/pixman-1
/usr/include/libpng12
/usr/include/nautilus
/usr/include
./dep/cpp-netlib-0.9.4/include)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES NautilusExtension.cpp GUIProvider.cpp GUIProvider.hpp)
add_executable(nautilus_mailru_cloud ${SOURCE_FILES} boost_headers.hpp extension_info.hpp CloudMailRuExtension.cpp CloudMailRuExtension.hpp MenuItem.cpp MenuItem.hpp FileInfo.hpp Clipboard.cpp Clipboard.hpp Test.cpp CloudMailRuRestAPI.cpp CloudMailRuRestAPI.hpp)
target_link_libraries(nautilus_mailru_cloud $(pkg-config libnautilus-extension --libs) -lboost_filesystem -lboost_system -lboost_thread -lcppnetlib-client-connections -lcppnetlib-server-parsers -lcppnetlib-uri)
