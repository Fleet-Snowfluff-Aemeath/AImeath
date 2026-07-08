#pragma once

#include <string>

class IAgent {
public:
    virtual ~IAgent() = default;

    virtual bool openPlugin(const std::string& name, const std::string& paramsJson) = 0;
    virtual bool controlPlugin(const std::string& name, const std::string& commandJson) = 0;
    virtual bool closePlugin(const std::string& name) = 0;
    virtual void stop() = 0;

    // Chat specific
    virtual bool chatSend(const std::string& text) = 0;

    // FileManager specific
    virtual bool fileList(const std::string& path) = 0;
    virtual bool fileRead(const std::string& path) = 0;
    virtual bool fileWrite(const std::string& path, const std::string& content) = 0;
    virtual bool fileMkdir(const std::string& path) = 0;
    virtual bool fileRemove(const std::string& path) = 0;

    // Terminal specific
    virtual bool terminalExec(const std::string& command) = 0;
    virtual bool terminalStdin(const std::string& data) = 0;
};
