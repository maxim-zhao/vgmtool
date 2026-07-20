#pragma once
#include <string>

class IStatusCallback
{
public:
    virtual ~IStatusCallback() = default;

    // Something like a popup info messagebox
    virtual void message(const std::string& message) const = 0;
    // Something like a popup error messagebox. Implies failure.
    virtual void error(const std::string& message) const = 0;
    // Something like a status bar, or verbose output.
    virtual void verbose_message(const std::string& message) const = 0;
};
