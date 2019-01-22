#include <mymq/configuration.hpp>

#include <gtest/gtest.h>

#include <sstream>

using flowmq::ServerConfiguration;
using flowmq::ServerConfigurationLoader;

TEST(TestLoadConfig, SimpleConfig){

    std::stringstream ss;

    ss << 
        "current_node = 3 \n"
        "id = 0 \n" 
        "host = localhost   \n"       
        "cluster_port = 12    \n"      
        "\n"
        "# comment\n"
        "client_port = 9000 \n"
        "id = 1 \n" 
        "host = localhost   \n"       
        "cluster_port = 13    \n"      
        "client_port = 9001 \n"
        "\n"
        "\n"
        ;
    auto config = ServerConfigurationLoader::load_config(ss);

    EXPECT_EQ(config.current_node, 3);
    ASSERT_EQ(config.server_nodes.size(), 2);
    EXPECT_EQ(config.server_nodes[0], std::make_tuple(0, "localhost", "12", "9000"));
    EXPECT_EQ(config.server_nodes[1], std::make_tuple(1, "localhost", "13", "9001"));

}
