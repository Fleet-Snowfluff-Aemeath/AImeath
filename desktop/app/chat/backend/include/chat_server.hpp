#pragma once

#include <string>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include "app_api.hpp"

#ifdef __cplusplus
extern "C" {
#endif

int  app_queue_size(void* p);
int  app_streaming(void* p);
void app_test_set_streaming(void* p, int val);
void app_test_drain_queue(void* p);

#ifdef __cplusplus
}

void chatServe(boost::beast::websocket::stream<boost::beast::tcp_stream>& ws,
               const std::string& first_msg);
#endif
