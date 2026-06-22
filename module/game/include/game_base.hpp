#pragma once

#include <string>
#include <boost/noncopyable.hpp>

class Game : private boost::noncopyable
{
public:
    virtual ~Game() = default;
    virtual void tick(int action) = 0;
    virtual bool isOver() const = 0;
    virtual int score() const = 0;
    virtual std::string getState() const = 0;
};