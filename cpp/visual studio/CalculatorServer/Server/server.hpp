#pragma once

#include <string>


#include "cpprest/json.h"
#include "cpprest/http_listener.h"

#include "../reader/reader_statistics.hpp"

using namespace web;
using namespace http;
using namespace utility;
using namespace http::experimental::listener;

class server
{
public:
	server(bool verbose);
	server(bool verbose, std::wstring window_regex, utility::string_t url);

	pplx::task<void> open() { return m_listener.open(); }
	pplx::task<void> close() { return m_listener.close(); }

private:
	void read_anno_population(web::json::value& result);
	void read_buildings_count(web::json::value& result);
	void read_productivity_statistics(web::json::value& result, bool optimalProductivity);

	void handle_get(http_request message);

	reader::image_recognition recog;
	reader::statistics stats;
	http_listener m_listener;
	std::mutex mutex_;
};