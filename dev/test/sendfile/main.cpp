/*
	restinio
*/

/*!
	Echo server.
*/

#define CATCH_CONFIG_MAIN
#include <catch/catch.hpp>

#include <restinio/all.hpp>

#include <test/common/utest_logger.hpp>
#include <test/common/pub.hpp>

TEST_CASE( "simple sendfile" , "[sendfile]" )
{
	using http_server_t =
		restinio::http_server_t<
			restinio::traits_t<
				restinio::asio_timer_manager_t,
				utest_logger_t > >;

	http_server_t http_server{
		restinio::own_io_context(),
		[]( auto & settings ){
			settings
				.port( utest_default_port() )
				.address( "127.0.0.1" )
				.request_handler(
					[]( auto req ){
						if( restinio::http_method_get() == req->header().method() )
						{
							req->create_response()
								.append_header( "Server", "RESTinio utest server" )
								.append_header_date_field()
								.append_header( "Content-Type", "text/plain; charset=utf-8" )
								.set_body( restinio::sendfile( "test/sendfile/f1.txt" ) )
								.done();
							return restinio::request_accepted();
						}

						return restinio::request_rejected();
					} );
		}
	};

	other_work_thread_for_server_t<http_server_t> other_thread{ http_server };
	other_thread.run();

	const std::string request{
			"GET / HTTP/1.0\r\n"
			"From: unit-test\r\n"
			"User-Agent: unit-test\r\n"
			"Content-Type: application/x-www-form-urlencoded\r\n"
			"Connection: close\r\n"
			"\r\n"
	};
	std::string response;

	REQUIRE_NOTHROW( response = do_request( request ) );

	REQUIRE_THAT(
		response,
		Catch::Matchers::EndsWith(
			"0123456789\n"
			"FILE1\n"
			"0123456789\n" ) );

	other_thread.stop_and_join();
}

TEST_CASE( "sendfile 2 files" , "[sendfile][n-files]" )
{
	using router_t = restinio::router::express_router_t<>;

	auto router = std::make_unique< router_t >();

	const std::string dir{ "test/sendfile/" };

	router->http_get(
		"/:f1/:f2",
		[ & ]( auto req, auto params ){
			return
				req->create_response()
					.append_header( "Server", "RESTinio Benchmark" )
					.append_header_date_field()
					.append_header( "Content-Type", "text/plain; charset=utf-8" )
					.append_body(
						restinio::sendfile(
							dir + restinio::cast_to< std::string >( params[ "f1" ] ) + ".txt" ) )
					.append_body(
						restinio::sendfile(
							dir + restinio::cast_to< std::string >( params[ "f2" ] ) + ".txt" ) )
					.done();
		} );
	router->http_get(
		"/:f1/:regularBuffer/:f2",
		[ & ]( auto req, auto params ){
			return
				req->create_response()
					.append_header( "Server", "RESTinio Benchmark" )
					.append_header_date_field()
					.append_header( "Content-Type", "text/plain; charset=utf-8" )
					.append_body(
						restinio::sendfile(
							dir + restinio::cast_to< std::string >( params[ "f1" ] ) + ".txt" ) )
					.append_body(
						restinio::cast_to< std::string >( params[ "regularBuffer" ] ) )
					.append_body(
						restinio::sendfile(
							dir + restinio::cast_to< std::string >( params[ "f2" ] ) + ".txt" ) )
					.done();
		} );


	using http_server_t =
		restinio::http_server_t<
			restinio::traits_t<
				restinio::asio_timer_manager_t,
				utest_logger_t,
				router_t > >;

	http_server_t http_server{
		restinio::own_io_context(),
		[&]( auto & settings ){
			settings
				.port( utest_default_port() )
				.address( "127.0.0.1" )
				.request_handler( std::move( router ) );
		}
	};

	other_work_thread_for_server_t<http_server_t> other_thread{ http_server };
	other_thread.run();

	{
		const std::string request{
				"GET /f1/f2 HTTP/1.0\r\n"
				"From: unit-test\r\n"
				"User-Agent: unit-test\r\n"
				"Content-Type: application/x-www-form-urlencoded\r\n"
				"Connection: close\r\n"
				"\r\n"
		};
		std::string response;

		REQUIRE_NOTHROW( response = do_request( request ) );

		REQUIRE_THAT(
			response,
			Catch::Matchers::EndsWith(
				"0123456789\n"
				"FILE1\n"
				"0123456789\n"
				"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC\n" ) );
	}

	{
		const std::string request{
				"GET /f2/f1 HTTP/1.0\r\n"
				"From: unit-test\r\n"
				"User-Agent: unit-test\r\n"
				"Content-Type: application/x-www-form-urlencoded\r\n"
				"Connection: close\r\n"
				"\r\n"
		};
		std::string response;

		REQUIRE_NOTHROW( response = do_request( request ) );

		REQUIRE_THAT(
			response,
			Catch::Matchers::EndsWith(
				"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC\n"
				"0123456789\n"
				"FILE1\n"
				"0123456789\n" ) );
	}


	{
		const std::string request{
				"GET /f1/REGULARBUFFER/f2 HTTP/1.0\r\n"
				"From: unit-test\r\n"
				"User-Agent: unit-test\r\n"
				"Content-Type: application/x-www-form-urlencoded\r\n"
				"Connection: close\r\n"
				"\r\n"
		};
		std::string response;

		REQUIRE_NOTHROW( response = do_request( request ) );

		REQUIRE_THAT(
			response,
			Catch::Matchers::EndsWith(
				"0123456789\n"
				"FILE1\n"
				"0123456789\n"
				"REGULARBUFFER"
				"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC\n" ) );
	}

	other_thread.stop_and_join();
}

TEST_CASE( "sendfile offsets_and_size" , "[sendfile][offset][size]" )
{
	using router_t = restinio::router::express_router_t<>;

	auto router = std::make_unique< router_t >();

	const std::string dir{ "test/sendfile/" };

	router->http_get(
		R"(/:fname/:offset(\d+))",
		[ & ]( auto req, auto params ){
			auto fname = dir + restinio::cast_to< std::string >( params[ "fname" ] ) + ".txt";
			return
				req->create_response()
					.append_header( "Server", "RESTinio Benchmark" )
					.append_header_date_field()
					.append_header( "Content-Type", "text/plain; charset=utf-8" )
					.append_body(
						restinio::sendfile( fname )
							.offset_and_size( restinio::cast_to< restinio::file_offset_t >( params[ "offset" ] ) ) )
					.done();
		} );

	router->http_get(
		R"(/:fname/:offset(\d+)/:size(\d+))",
		[ & ]( auto req, auto params ){
			auto fname = dir + restinio::cast_to< std::string >( params[ "fname" ] ) + ".txt";
			const auto offset = restinio::cast_to< restinio::file_offset_t >( params[ "offset" ] );
			const auto size = restinio::cast_to< restinio::file_size_t >( params[ "size" ] );
			return
				req->create_response()
					.append_header( "Server", "RESTinio Benchmark" )
					.append_header_date_field()
					.append_header( "Content-Type", "text/plain; charset=utf-8" )
					.append_body(
						restinio::sendfile( fname ).offset_and_size( offset, size ) )
					.done();
		} );

	using http_server_t =
		restinio::http_server_t<
			restinio::traits_t<
				restinio::asio_timer_manager_t,
				utest_logger_t,
				router_t > >;

	http_server_t http_server{
		restinio::own_io_context(),
		[&]( auto & settings ){
			settings
				.port( utest_default_port() )
				.address( "127.0.0.1" )
				.request_handler( std::move( router ) );
		}
	};

	other_work_thread_for_server_t<http_server_t> other_thread{ http_server };
	other_thread.run();

	{
		const std::string all_file{ "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC\n" };

		for( std::size_t i = 0; i < all_file.size(); ++i )
		{
			const std::string request{
					"GET /f2/" + std::to_string( i ) + " HTTP/1.0\r\n"
					"From: unit-test\r\n"
					"User-Agent: unit-test\r\n"
					"Content-Type: application/x-www-form-urlencoded\r\n"
					"Connection: close\r\n"
					"\r\n"
			};

			std::string response;
			REQUIRE_NOTHROW( response = do_request( request ) );

			REQUIRE_THAT(
				response,
				Catch::Matchers::EndsWith(
					all_file.substr( i ) ) );
		}
	}

	{
		const std::string single_string{ "SENDILE 012345678901234567890123456789012345678901234567890123456789\n" };

		for( std::size_t n = 0; n < 3301; n+= 100 )
			for( std::size_t i = 0; i < single_string.size(); ++i )
			{
				const std::string request{
						"GET /f3/" +
							std::to_string( n * single_string.size() ) + "/" +
							std::to_string( i ) + " HTTP/1.0\r\n"
						"From: unit-test\r\n"
						"User-Agent: unit-test\r\n"
						"Content-Type: application/x-www-form-urlencoded\r\n"
						"Connection: close\r\n"
						"\r\n"
				};

				std::string response;
				REQUIRE_NOTHROW( response = do_request( request ) );

				REQUIRE_THAT(
					response,
					Catch::Matchers::EndsWith(
						single_string.substr( 0, i ) ) );
			}
	}

	other_thread.stop_and_join();
}

TEST_CASE( "sendfile chunks" , "[sendfile][chunk]" )
{
	using router_t = restinio::router::express_router_t<>;

	auto router = std::make_unique< router_t >();

	const std::string dir{ "test/sendfile/" };

	router->http_get(
		R"(/:fname/:chunk(\d+))",
		[ & ]( auto req, auto params ){
			auto fname = dir + restinio::cast_to< std::string >( params[ "fname" ] ) + ".txt";
			const auto chunk_size = restinio::cast_to< restinio::file_size_t >( params[ "chunk" ] );
			return
				req->create_response()
					.append_header( "Server", "RESTinio Benchmark" )
					.append_header_date_field()
					.append_header( "Content-Type", "text/plain; charset=utf-8" )
					.append_body(
						restinio::sendfile( fname ).chunk_size( chunk_size ) )
					.done();
		} );

	using http_server_t =
		restinio::http_server_t<
			restinio::traits_t<
				restinio::asio_timer_manager_t,
				utest_logger_t,
				router_t > >;

	http_server_t http_server{
		restinio::own_io_context(),
		[&]( auto & settings ){
			settings
				.port( utest_default_port() )
				.address( "127.0.0.1" )
				.request_handler( std::move( router ) );
		}
	};

	other_work_thread_for_server_t<http_server_t> other_thread{ http_server };
	other_thread.run();

	{
		const std::string request{
				"GET /f1/4 HTTP/1.0\r\n"
				"From: unit-test\r\n"
				"User-Agent: unit-test\r\n"
				"Content-Type: application/x-www-form-urlencoded\r\n"
				"Connection: close\r\n"
				"\r\n"
		};

		std::string response;
		REQUIRE_NOTHROW( response = do_request( request ) );

		REQUIRE_THAT(
			response,
			Catch::Matchers::EndsWith(
				"0123456789\n"
				"FILE1\n"
				"0123456789\n" ) );
	}

	{
		const std::string request{
				"GET /f2/5 HTTP/1.0\r\n"
				"From: unit-test\r\n"
				"User-Agent: unit-test\r\n"
				"Content-Type: application/x-www-form-urlencoded\r\n"
				"Connection: close\r\n"
				"\r\n"
		};

		std::string response;
		REQUIRE_NOTHROW( response = do_request( request ) );

		REQUIRE_THAT(
			response,
			Catch::Matchers::EndsWith(
				"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC\n" ) );
	}

	{
		const std::string request{
				"GET /f1/1 HTTP/1.0\r\n"
				"From: unit-test\r\n"
				"User-Agent: unit-test\r\n"
				"Content-Type: application/x-www-form-urlencoded\r\n"
				"Connection: close\r\n"
				"\r\n"
		};

		std::string response;
		REQUIRE_NOTHROW( response = do_request( request ) );

		REQUIRE_THAT(
			response,
			Catch::Matchers::EndsWith(
				"0123456789\n"
				"FILE1\n"
				"0123456789\n" ) );
	}

	{
		const std::string request{
				"GET /f2/1 HTTP/1.0\r\n"
				"From: unit-test\r\n"
				"User-Agent: unit-test\r\n"
				"Content-Type: application/x-www-form-urlencoded\r\n"
				"Connection: close\r\n"
				"\r\n"
		};

		std::string response;
		REQUIRE_NOTHROW( response = do_request( request ) );

		REQUIRE_THAT(
			response,
			Catch::Matchers::EndsWith(
				"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC\n" ) );
	}

	other_thread.stop_and_join();
}

TEST_CASE( "sendfile errors" , "[sendfile][error]" )
{
	using router_t = restinio::router::express_router_t<>;

	auto router = std::make_unique< router_t >();

	const std::string dir{ "test/sendfile/" };

	router->http_get(
		R"(/:fname/:offset(\d+))",
		[ & ]( auto req, auto params ){
			auto fname = dir + restinio::cast_to< std::string >( params[ "fname" ] ) + ".txt";
			return
				req->create_response()
					.append_header( "Server", "RESTinio Benchmark" )
					.append_header_date_field()
					.append_header( "Content-Type", "text/plain; charset=utf-8" )
					.append_body(
						restinio::sendfile( fname )
							.offset_and_size( restinio::cast_to< restinio::file_offset_t >( params[ "offset" ] ) ) )
					.done();
		} );

	router->http_get(
		R"(/:fname/:offset(\d+)/:size(\d+))",
		[ & ]( auto req, auto params ){
			auto fname = dir + restinio::cast_to< std::string >( params[ "fname" ] ) + ".txt";
			const auto offset = restinio::cast_to< restinio::file_offset_t >( params[ "offset" ] );
			const auto size = restinio::cast_to< restinio::file_size_t >( params[ "size" ] );
			return
				req->create_response()
					.append_header( "Server", "RESTinio Benchmark" )
					.append_header_date_field()
					.append_header( "Content-Type", "text/plain; charset=utf-8" )
					.append_body(
						restinio::sendfile( fname ).offset_and_size( offset, size ) )
					.done();
		} );

	using http_server_t =
		restinio::http_server_t<
			restinio::traits_t<
				restinio::asio_timer_manager_t,
				utest_logger_t,
				router_t > >;

	http_server_t http_server{
		restinio::own_io_context(),
		[&]( auto & settings ){
			settings
				.port( utest_default_port() )
				.address( "127.0.0.1" )
				.request_handler( std::move( router ) );
		}
	};

	other_work_thread_for_server_t<http_server_t> other_thread{ http_server };
	other_thread.run();

	{
		const std::string request{
				"GET /nosuchfile/0 HTTP/1.0\r\n"
				"From: unit-test\r\n"
				"User-Agent: unit-test\r\n"
				"Content-Type: application/x-www-form-urlencoded\r\n"
				"Connection: close\r\n"
				"\r\n"
		};

		std::string response;
		REQUIRE_THROWS( response = do_request( request ) );
	}

	{
		const std::string request{
				"GET /f2/1024 HTTP/1.0\r\n"
				"From: unit-test\r\n"
				"User-Agent: unit-test\r\n"
				"Content-Type: application/x-www-form-urlencoded\r\n"
				"Connection: close\r\n"
				"\r\n"
		};

		std::string response;
		REQUIRE_THROWS( response = do_request( request ) );
	}

	other_thread.stop_and_join();
}

TEST_CASE( "sendfile with invalid descriptor with " , "[sendfile][error][is_valid]" )
{
	using router_t = restinio::router::express_router_t<>;

	auto router = std::make_unique< router_t >();

	const std::string dir{ "test/sendfile/" };

	router->http_get(
		R"(/1)",
		[ & ]( auto req, auto ){
			const std::string fname = "must_not_exist_file_name";
			auto sf = restinio::sendfile( fname, restinio::open_file_errh_t::ignore_err );

			return
				req->create_response()
					.append_header( "Server", "RESTinio Benchmark" )
					.append_header_date_field()
					.append_header( "Content-Type", "text/plain; charset=utf-8" )
					.set_body( std::move( sf ) )
					.done();
		} );

	router->http_get(
		R"(/2)",
		[ & ]( auto req, auto ){
			const std::string fname = "must_not_exist_file_name";
			auto sf = restinio::sendfile( fname, restinio::open_file_errh_t::ignore_err );

			return
				req->create_response()
					.append_header( "Server", "RESTinio Benchmark" )
					.append_header_date_field()
					.append_header( "Content-Type", "text/plain; charset=utf-8" )
					.append_body( std::move( sf ) )
					.done();
		} );

	router->http_get(
		R"(/3)",
		[ & ]( restinio::request_handle_t req, auto ){
			const std::string fname = "must_not_exist_file_name";
			auto sf = restinio::sendfile( fname, restinio::open_file_errh_t::ignore_err );

			return
				req->create_response< restinio::user_controlled_output_t >()
					.append_header( "Server", "RESTinio Benchmark" )
					.append_header_date_field()
					.append_header( "Content-Type", "text/plain; charset=utf-8" )
					.set_body( std::move( sf ) )
					.set_content_length( 0 )
					.done();
		} );

	router->http_get(
		R"(/4)",
		[ & ]( restinio::request_handle_t req, auto ){
			const std::string fname = "must_not_exist_file_name";
			auto sf = restinio::sendfile( fname, restinio::open_file_errh_t::ignore_err );

			return
				req->create_response< restinio::user_controlled_output_t >()
					.append_header( "Server", "RESTinio Benchmark" )
					.append_header_date_field()
					.append_header( "Content-Type", "text/plain; charset=utf-8" )
					.append_body( std::move( sf ) )
					.set_content_length( 0 )
					.done();
		} );

	router->http_get(
		R"(/5)",
		[ & ]( restinio::request_handle_t req, auto ){
			const std::string fname = "must_not_exist_file_name";
			auto sf = restinio::sendfile( fname, restinio::open_file_errh_t::ignore_err );

			return
				req->create_response< restinio::chunked_output_t >()
					.append_header( "Server", "RESTinio Benchmark" )
					.append_header_date_field()
					.append_header( "Content-Type", "text/plain; charset=utf-8" )
					.append_chunk( std::move( sf ) )
					.done();
		} );

	using http_server_t =
		restinio::http_server_t<
			restinio::traits_t<
				restinio::asio_timer_manager_t,
				utest_logger_t,
				router_t > >;

	http_server_t http_server{
		restinio::own_io_context(),
		[&]( auto & settings ){
			settings
				.port( utest_default_port() )
				.address( "127.0.0.1" )
				.request_handler( std::move( router ) );
		}
	};

	other_work_thread_for_server_t<http_server_t> other_thread{ http_server };
	other_thread.run();

	{
		const std::string request{
				"GET /1 HTTP/1.0\r\n"
				"From: unit-test\r\n"
				"User-Agent: unit-test\r\n"
				"Content-Type: application/x-www-form-urlencoded\r\n"
				"Connection: close\r\n"
				"\r\n"
		};

		std::string response;
		REQUIRE_NOTHROW( response = do_request( request ) );

		REQUIRE_THAT(
			response,
			Catch::Matchers::Contains( "Content-Length: 0\r\n" ) );
	}

	{
		const std::string request{
				"GET /2 HTTP/1.0\r\n"
				"From: unit-test\r\n"
				"User-Agent: unit-test\r\n"
				"Content-Type: application/x-www-form-urlencoded\r\n"
				"Connection: close\r\n"
				"\r\n"
		};

		std::string response;
		REQUIRE_NOTHROW( response = do_request( request ) );

		REQUIRE_THAT(
			response,
			Catch::Matchers::Contains( "Content-Length: 0\r\n" ) );
	}

	{
		const std::string request{
				"GET /3 HTTP/1.0\r\n"
				"From: unit-test\r\n"
				"User-Agent: unit-test\r\n"
				"Content-Type: application/x-www-form-urlencoded\r\n"
				"Connection: close\r\n"
				"\r\n"
		};

		std::string response;
		REQUIRE_NOTHROW( response = do_request( request ) );

		REQUIRE_THAT(
			response,
			Catch::Matchers::Contains( "Content-Length: 0\r\n" ) );
	}

	{
		const std::string request{
				"GET /4 HTTP/1.0\r\n"
				"From: unit-test\r\n"
				"User-Agent: unit-test\r\n"
				"Content-Type: application/x-www-form-urlencoded\r\n"
				"Connection: close\r\n"
				"\r\n"
		};

		std::string response;
		REQUIRE_NOTHROW( response = do_request( request ) );

		REQUIRE_THAT(
			response,
			Catch::Matchers::Contains( "Content-Length: 0\r\n" ) );
	}

	{
		const std::string request{
				"GET /5 HTTP/1.0\r\n"
				"From: unit-test\r\n"
				"User-Agent: unit-test\r\n"
				"Content-Type: application/x-www-form-urlencoded\r\n"
				"Connection: close\r\n"
				"\r\n"
		};

		std::string response;
		REQUIRE_NOTHROW( response = do_request( request ) );

		REQUIRE_THAT(
			response,
			Catch::Matchers::EndsWith( "\r\n\r\n0\r\n\r\n" ) );
	}

	other_thread.stop_and_join();
}

TEST_CASE( "sendfile_chunk_size_guarded_value_t " , "[chunk_size_guarded_value]" )
{
	{
		restinio::sendfile_chunk_size_guarded_value_t chunk( 42 );
		REQUIRE( chunk.value() == 42 );
	}
	{
		restinio::sendfile_chunk_size_guarded_value_t chunk( 0 );
		REQUIRE( chunk.value() == restinio::sendfile_default_chunk_size );
	}
	{
		restinio::sendfile_chunk_size_guarded_value_t chunk( restinio::sendfile_max_chunk_size + 10 );
		REQUIRE( chunk.value() == restinio::sendfile_max_chunk_size );
	}
}
