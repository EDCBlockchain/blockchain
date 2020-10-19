#include <boost/test/unit_test.hpp>

#include <fc/network/http/websocket.hpp>

#include <iostream>
#include <fc/log/logger.hpp>
#include <fc/log/console_appender.hpp>

BOOST_AUTO_TEST_SUITE(fc_network)

BOOST_AUTO_TEST_CASE(websocket_test)
{ 
    // set up logging
    fc::appender::ptr ca(new fc::console_appender);
    fc::logger l = fc::logger::get("rpc");
    l.add_appender( ca );
    
    fc::http::websocket_client client;
    fc::http::websocket_connection_ptr s_conn, c_conn;
    int port;
    {
        // even with this key set, if the remote does not provide it, it will revert to
        // the remote endpoint
        fc::http::websocket_server server("MyProxyHeaderKey");
        server.on_connection([&]( const fc::http::websocket_connection_ptr& c ){
                s_conn = c;
                c->on_message_handler([&](const std::string& s){
                    c->send_message("echo: " + s);
                });
                c->on_http_handler([&](const std::string& s){
                    fc::http::reply reply;
                    reply.body_as_string = "http-echo: " + s;
                    return reply;
                });
            });

        server.listen( 0 );
        port = server.get_listening_port();

        server.start_accept();

        std::string echo;
        c_conn = client.connect( "ws://localhost:" + fc::to_string(port) );
        c_conn->on_message_handler([&](const std::string& s){
                    echo = s;
                });
        c_conn->send_message( "hello world" );
        fc::usleep( fc::milliseconds(100) );
        BOOST_CHECK_EQUAL("echo: hello world", echo);
        c_conn->send_message( "again" );
        fc::usleep( fc::milliseconds(100) );
        BOOST_CHECK_EQUAL("echo: again", echo);

        s_conn->close(0, "test");
        fc::usleep( fc::milliseconds(100) );
        BOOST_CHECK_THROW(c_conn->send_message( "again" ), fc::exception);

        c_conn = client.connect( "ws://localhost:" + fc::to_string(port) );
        c_conn->on_message_handler([&](const std::string& s){
                    echo = s;
                });
        c_conn->send_message( "hello world" );
        fc::usleep( fc::milliseconds(100) );
        BOOST_CHECK_EQUAL("echo: hello world", echo);

        // Testing on_http
        std::string server_endpoint_string = "127.0.0.1:" + fc::to_string(port);
        fc::ip::endpoint server_endpoint = fc::ip::endpoint::from_string( server_endpoint_string );
        fc::http::connection c_http_conn;
        c_http_conn.connect_to( server_endpoint );

        std::string url = "http://localhost:" + fc::to_string(port);
        fc::http::reply reply = c_http_conn.request( "POST", url, "hello world again" );
        std::string reply_body( reply.body.begin(), reply.body.end() );
        BOOST_CHECK_EQUAL(200, reply.status);
        BOOST_CHECK_EQUAL("http-echo: hello world again", reply_body );
    }

    BOOST_CHECK_THROW(c_conn->send_message( "again" ), fc::assert_exception);
    BOOST_CHECK_THROW(client.connect( "ws://localhost:" + fc::to_string(port) ), fc::exception);
    l.remove_appender(ca);
}

BOOST_AUTO_TEST_CASE(websocket_test_with_proxy_header)
{ 
    // set up logging
    fc::appender::ptr ca(new fc::console_appender);
    fc::logger l = fc::logger::get("rpc");
    auto old_log_level = l.get_log_level();
    l.set_log_level( fc::log_level::info );
    l.add_appender( ca );
    
    fc::http::websocket_client client;
    // add the proxy header element
    client.append_header("MyProxyHeaderKey", "MyServer:8080");

    fc::http::websocket_connection_ptr s_conn, c_conn;
    int port;
    {
        // the server will be on the lookout for the key in the header
        fc::http::websocket_server server("MyProxyHeaderKey");
        server.on_connection([&]( const fc::http::websocket_connection_ptr& c ){
                s_conn = c;
                c->on_message_handler([&](const std::string& s){
                    c->send_message("echo: " + s);
                });
                c->on_http_handler([&](const std::string& s){
                    fc::http::reply reply;
                    reply.body_as_string = "http-echo: " + s;
                    return reply;
                });
            });

        server.listen( 0 );
        port = server.get_listening_port();

        server.start_accept();

        std::string echo;
        c_conn = client.connect( "ws://localhost:" + fc::to_string(port) );
        c_conn->on_message_handler([&](const std::string& s){
                    echo = s;
                });
        c_conn->send_message( "hello world" );
        fc::usleep( fc::milliseconds(100) );
        BOOST_CHECK_EQUAL("echo: hello world", echo);
        c_conn->send_message( "again" );
        fc::usleep( fc::milliseconds(100) );
        BOOST_CHECK_EQUAL("echo: again", echo);

        s_conn->close(0, "test");
        fc::usleep( fc::milliseconds(100) );
        BOOST_CHECK_THROW(c_conn->send_message( "again" ), fc::exception);

        c_conn = client.connect( "ws://localhost:" + fc::to_string(port) );
        c_conn->on_message_handler([&](const std::string& s){
                    echo = s;
                });
        c_conn->send_message( "hello world" );
        fc::usleep( fc::milliseconds(100) );
        BOOST_CHECK_EQUAL("echo: hello world", echo);

        // Testing on_http
        std::string server_endpoint_string = "127.0.0.1:" + fc::to_string(port);
        fc::ip::endpoint server_endpoint = fc::ip::endpoint::from_string( server_endpoint_string );
        fc::http::connection c_http_conn;
        c_http_conn.connect_to( server_endpoint );

        std::string url = "http://localhost:" + fc::to_string(port);
        fc::http::header fwd("MyProxyHeaderKey", "MyServer:8080");
        fc::http::headers headers {fwd};
        fc::http::reply reply = c_http_conn.request( "POST", url, "hello world again", headers );
        std::string reply_body( reply.body.begin(), reply.body.end() );
        BOOST_CHECK_EQUAL(200, reply.status);
        BOOST_CHECK_EQUAL("http-echo: hello world again", reply_body );
    }

    BOOST_CHECK_THROW(c_conn->send_message( "again" ), fc::assert_exception);
    BOOST_CHECK_THROW(client.connect( "ws://localhost:" + fc::to_string(port) ), fc::exception);

    l.remove_appender(ca);
    l.set_log_level(old_log_level);
}

BOOST_AUTO_TEST_SUITE_END()
