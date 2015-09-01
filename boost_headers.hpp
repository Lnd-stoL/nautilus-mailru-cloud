
#ifndef BOOST_HEADERS_HPP
#define BOOST_HEADERS_HPP

//----------------------------------------------------------------------------------------------------------------------

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ini_parser.hpp>

#define BOOST_NETWORK_ENABLE_HTTPS
#include <boost/network/uri.hpp>
#include <boost/network/protocol/http.hpp>

namespace b_network = boost::network;
namespace b_fs      = boost::filesystem;
namespace b_sys     = boost::system;
namespace b_http    = boost::network::http;
namespace b_uri     = b_network::uri;
namespace b_pt      = boost::property_tree;

//----------------------------------------------------------------------------------------------------------------------

#endif
