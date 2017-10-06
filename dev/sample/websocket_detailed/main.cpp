#include <type_traits>
#include <iostream>
#include <chrono>
#include <memory>

#include <asio.hpp>
#include <asio/ip/tcp.hpp>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <restinio/all.hpp>
#include <restinio/websocket/websocket.hpp>

namespace rr = restinio::router;
using router_t = rr::express_router_t;

namespace rws = restinio::websocket;

using traits_t =
	restinio::traits_t<
		restinio::asio_timer_factory_t,
		restinio::single_threaded_ostream_logger_t,
		router_t >;

using http_server_t = restinio::http_server_t< traits_t >;

// void
// print_ws_header( const rws::ws_message_header_t & header )
// {
// 	std::cout <<
// 		"final: " << header.m_is_final <<
// 		", opcode: " << static_cast<int>(header.m_opcode) <<
// 		", payload_len: " << header.m_payload_len <<
// 		", masking_key: " << header.m_masking_key
// 		<< std::endl;
// }

// void
// print_ws_message( const rws::message_t & msg )
// {
// 	std::cout << "header: {";

// 	print_ws_header(msg.header());

// 	std::cout << "}, payload: '" << msg.payload() << "' (";

// 	for( const auto ch : msg.payload() )
// 	{
// 		std::cout << std::hex << (static_cast< int >(ch) & 0xFF) << " ";
// 	}

// 	std::cout << ")" << std::endl;
// }

auto server_handler( rws::ws_handle_t & websocket )
{
	auto router = std::make_unique< router_t >();

	router->http_get(
		"/",
		[&]( auto req, auto ){

			if( restinio::http_connection_header_t::upgrade == req->header().connection() )
			{
				websocket =
					rws::upgrade< traits_t >(
						*req,
						rws::activation_t::immediate,
						[]( auto wsh, auto m ){
							// print_ws_message( *m );

							if( m->opcode() == rws::opcode_t::continuation_frame )
 							{
 							}
							else if( m->opcode() == rws::opcode_t::ping_frame )
 							{
								if( m->payload().size() > 125)
								{
									// TODO: if this is error so why send anything?
									wsh->kill();
								}

								auto pong = *m;
								pong.set_opcode( rws::opcode_t::pong_frame );
								wsh->send_message( pong );
							}
							else if( m->opcode() == rws::opcode_t::pong_frame )
							{
							}
							else if( m->opcode() == rws::opcode_t::connection_close_frame )
							{
								// TODO: send response if code not 1006.
								wsh->send_message( *m );
								wsh->shutdown();
							}
							else
							{
								wsh->send_message( *m );
							}
						} );

				return restinio::request_accepted();
			}

			return restinio::request_rejected();
		} );

	return router;
}

int main()
{
	using namespace std::chrono;

	try
	{
		rws::ws_handle_t websocket;

		run( restinio::on_this_thread<traits_t>()
					.address( "localhost" )
					.port( 9001 )
					.request_handler( server_handler( websocket ) )
					.read_next_http_message_timelimit( 10s )
					.write_http_response_timelimit( 1s )
					.handle_request_timeout( 1s ) );

	}
	catch( const std::exception & ex )
	{
		std::cerr << "Error: " << ex.what() << std::endl;
		return 1;
	}

	return 0;
}
