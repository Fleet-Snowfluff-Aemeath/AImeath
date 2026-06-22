#include <iostream>
#include <string>
#include <cstdlib>
#include <thread>
#include <memory>
#include <atomic>
#include <dlfcn.h>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/json.hpp>

#include "gamemod.hpp"
#include "wsutil.hpp"

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

// ---- Game session ----

struct GameSession {
    GameModuleCache cache;
    GameModule lib{};
    GamePtr game{nullptr, GameDeleter{nullptr}};
    std::string game_type;
    std::atomic<bool> paused{false};
};

static void handleGame(websocket::stream<beast::tcp_stream>& ws)
{
    GameSession session;
    beast::flat_buffer buf;

    while (true)
    {
        buf.clear();
        ws.read(buf);
        std::string msg = beast::buffers_to_string(buf.data());

        std::string action = jsonParseStr(msg, "action");
        if (action.empty()) action = jsonParseStr(msg, "type");

        auto send = [&](const std::string& data) {
            ws.write(asio::buffer(data));
        };

        if (action == "new_game") {
            if (session.game) { send(jsonError("game already running")); continue; }
            session.game_type = jsonParseStr(msg, "game");
            if (session.game_type.empty()) { send(jsonError("missing game")); continue; }
            session.lib = session.cache.load(session.game_type);
            if (!session.lib) { send(jsonError("failed to load " + session.game_type)); continue; }
            int w = jsonParseInt(msg, "width"); if (w <= 0) w = 20;
            int h = jsonParseInt(msg, "height"); if (h <= 0) h = 20;
            session.game = session.lib.create(w, h);
            session.paused = false;
            if (session.game)
                send(session.game->getState());
        }
        else if (action == "tick" || action == "input") {
            if (!session.game) { send(jsonError("no game")); continue; }
            if (session.paused) continue;
            int val = jsonParseInt(msg, "value");
            session.game->tick(val);
            send(session.game->getState());
        }
        else if (action == "pause") {
            session.paused = true;
            send(jsonOk());
        }
        else if (action == "resume") {
            session.paused = false;
            send(jsonOk());
        }
        else if (action == "end_game") {
            session.game.reset();
            send(jsonOk());
            break;
        }
        else {
            send(jsonError("unknown action: " + action));
        }
    }
}

// ---- Chat session ----

static char* meow(const char* text)
{
    std::string s(text ? text : "");
    s += "\xe5\x96\xb5";
    char* buf = static_cast<char*>(std::malloc(s.size() + 1));
    if (buf) std::memcpy(buf, s.data(), s.size() + 1);
    return buf;
}

static void handleChat(websocket::stream<beast::tcp_stream>& ws)
{
    void* lib = dlopen("libchat.so", RTLD_NOW | RTLD_LOCAL);
    if (!lib)
    {
        boost::json::object err;
        err["type"] = "error";
        err["msg"]  = "chat module not found";
        ws.write(asio::buffer(boost::json::serialize(err)));
        return;
    }

    auto chat_new     = reinterpret_cast<void* (*)(char* (*)(const char*))>(dlsym(lib, "chat_new"));
    auto chat_free    = reinterpret_cast<void  (*)(void*)>(dlsym(lib, "chat_free"));
    auto chat_process = reinterpret_cast<char* (*)(void*, const char*)>(dlsym(lib, "chat_process"));

    void* chat = chat_new(meow);
    beast::flat_buffer buf;

    try
    {
        while (true)
        {
            buf.clear();
            ws.read(buf);
            std::string msg = beast::buffers_to_string(buf.data());

            std::string text = jsonParseStr(msg, "text");

            char* raw = chat_process(chat, text.c_str());
            std::string result(raw ? raw : "");
            std::free(raw);

            boost::json::object obj;
            obj["text"] = result;
            ws.write(asio::buffer(boost::json::serialize(obj)));
        }
    }
    catch (const boost::system::system_error&)
    {
    }

    chat_free(chat);
    dlclose(lib);
}

// ---- Router ----

static void handleClient(tcp::socket socket)
{
    try
    {
        beast::tcp_stream stream(std::move(socket));
        beast::flat_buffer buf;
        http::request<http::string_body> req;
        http::read(stream, buf, req);

        websocket::stream<beast::tcp_stream> ws(std::move(stream));
        ws.accept(req);

        if (req.target() == "/chat")
            handleChat(ws);
        else
            handleGame(ws);
    }
    catch (const boost::system::system_error&)
    {
    }
}

// ---- Main ----

int main()
{
    try
    {
        asio::io_context io;
        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 3001));
        std::cout << "Game server listening on port 3001" << std::endl;

        while (true)
        {
            tcp::socket socket(io);
            acceptor.accept(socket);
            std::thread(handleClient, std::move(socket)).detach();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
