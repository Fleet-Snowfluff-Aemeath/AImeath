#include <iostream>
#include <string>
#include <cstdlib>
#include <thread>
#include <memory>
#include <atomic>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include "gamemod.hpp"
#include "wsutil.hpp"

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

struct GameSession {
    GameModuleCache cache;
    GameModule lib{};
    GamePtr game{nullptr, GameDeleter{nullptr}};
    std::string game_type;
    std::atomic<bool> paused{false};
};

static void handleClient(tcp::socket socket)
{
    try
    {
        websocket::stream<beast::tcp_stream> ws(
            beast::tcp_stream(std::move(socket)));
        ws.accept();

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
                if (session.game) {
                    send(session.game->getState());
                }
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
    catch (const boost::system::system_error&)
    {
    }
}

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
