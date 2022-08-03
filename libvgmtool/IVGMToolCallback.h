#pragma once
#include <string>

class IVGMToolCallback
{
public:
    virtual ~IVGMToolCallback() = default;

    virtual void show_message(const std::string& message) const = 0;
    virtual void show_error(const std::string& message) const;
    virtual void show_status(const std::string& message) const = 0;
    virtual void show_conversion_progress(const std::string& message) const = 0;
};
