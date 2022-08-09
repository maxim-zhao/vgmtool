#include "CLI11.hpp"
#include "IVGMToolCallback.h"
#include <trim.h>
#include <writetotext.h>

#include "convert.h"
#include "utils.h"
#include "vgm.h"

class Callback : public IVGMToolCallback
{
public:
    bool is_verbose = false;

    void show_message(const std::string& message) const override
    {
        printf("%s\n", message.c_str());
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
    try
    {
        CLI::App app{"VGMTool CLI: VGM file editing utility"};
        app.require_subcommand()
           ->fallthrough()
           ->set_help_all_flag("--help-all", "Show all subcommands help");
        app.add_flag("-v, --verbose", callback.is_verbose, "Print messages while working");

        const auto* toTextVerb = app.add_subcommand("totext", "Emits a text file conversion of the VGM file");

        auto* trimVerb = app.add_subcommand("trim", "Trim the file");
        int start;
        int loop = -1;
        int end;
        bool logTrim;
        trimVerb->add_option("--start", start, "Trim start point in samples")->required()->
                  check(CLI::NonNegativeNumber);
        trimVerb->add_option("--loop", loop, "Trim loop point in samples")->check(CLI::NonNegativeNumber);
        trimVerb->add_option("--end", end, "Trim end point in samples")->required()->check(CLI::NonNegativeNumber);
        trimVerb->add_flag("--log", logTrim, "Log trim points to editpoints.txt");

        const auto* checkVerb = app.add_subcommand("check", "Check the VGM file(s) for errors");

        auto* compressVerb = app.add_subcommand("compress", "Compress VGM file(s)");
        int compressionIterations;
        compressVerb->add_option("--iterations", compressionIterations, "Zopfli compression iterations")
                    ->default_val(15);

        const auto* convertVerb = app.add_subcommand("convert", "Convert GYM, CYM, SSL files to VGM");

        std::vector<std::string> filenames;
        app.add_option("filename", filenames, "The file(s) to process")
           ->required()
           ->check(CLI::ExistingFile);

        CLI11_PARSE(app, argc, argv)

        for (const auto& filename : filenames)
        {
            if (toTextVerb->parsed())
            {
                write_to_text(filename, callback);
            }

            if (trimVerb->parsed())
            {
                trim(filename, start, loop, end, false, logTrim, callback);
            }

            if (checkVerb->parsed())
            {
                check_lengths(filename, true, callback);
            }

            if (compressVerb->parsed())
            {
                Utils::compress(filename, compressionIterations);
            }

            if (convertVerb->parsed())
            {
                Convert::to_vgm(filename, callback);
            }
        }

        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        fprintf(stderr, "Fatal error: %s", e.what());
        return EXIT_FAILURE;
    }
}
