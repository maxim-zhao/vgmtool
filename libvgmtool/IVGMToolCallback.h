#pragma once
#include <string>

class IVGMToolCallback
{
public:
    virtual ~IVGMToolCallback() = default;

    // Something like a popup info messagebox
    virtual void show_message(const std::string& message) const = 0;
    // Something like a popup error messagebox. Implies failure.
    virtual void show_error(const std::string& message) const = 0;
    // Something like a status bar, or verbose output.
    virtual void show_status(const std::string& message) const = 0;
    // Specific to conversion, should go away...
    virtual void show_conversion_progress(const std::string& message) const = 0;
};
