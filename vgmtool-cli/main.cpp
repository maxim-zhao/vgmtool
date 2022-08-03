#include "CLI11.hpp"
#include "IVGMToolCallback.h"
#include "writetotext.h"

class Callback : public IVGMToolCallback
{
public:
    bool is_verbose = false;

    void show_message(const std::string& message) const override
    {
        if (is_verbose)
        {
            printf("%s\n", message.c_str());
        }
    }

    void show_error(const std::string& message) const override
    {
        fprintf(stderr, "%s\n", message.c_str());
    }

    void show_status(const std::string& message) const override
    {
        if (is_verbose)
        {
            printf("%s\n", message.c_str());
        }
    }

    void show_conversion_progress(const std::string& message) const override
    {
        if (is_verbose)
        {
            printf("%s\n", message.c_str());
        }
    }
} callback;


int main(int argc, const char** argv)
{
    CLI::App app{"VGMTool CLI: VGM file editing utility"};

    auto* toText = app.add_subcommand("totext", "Emits a text file conversion of the VGM file")
        ->fallthrough();

    app.require_subcommand(1);

    std::vector<std::string> filenames;
    app.add_option("filename", filenames, "The file(s) to process")
       ->required()
       ->check(CLI::ExistingFile);

    app.add_flag("-v,--verbose", callback.is_verbose, "Print messages while working");

    CLI11_PARSE(app, argc, argv)

    if (toText->parsed())
    {
        for (const auto& filename : filenames)
        {
            write_to_text(filename, callback);
        }
    }

    return 0;
}
