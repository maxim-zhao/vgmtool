#include "CLI11.hpp"
#include "libvgmtool/IVGMToolCallback.h"
#include <libvgmtool/trim.h>

#include "libvgmtool/convert.h"
#include "libvgmtool/utils.h"
#include "libvgmtool/vgm.h"
#include "libvgmtool/VgmFile.h"

#include <libpu8/libpu8/libpu8.h>

namespace
{
    class Callback final : public IVGMToolCallback
    {
    public:
        bool is_verbose = false;

        void show_message(const std::string& message) const override
        {
            printf("%s\n", message.c_str());
        }

        void show_error(const std::string& message) const override
        {
            fprintf(stderr, "%s\n", message.c_str()); // NOLINT(cert-err33-c)
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

    void write_to_text(const std::string& filename, const std::string& outputFilename, bool gd3Only, bool forTextFile)
    {
        // Read in file
        VgmFile f(filename);
        // Write to stdout if no filename is given
        auto* s = outputFilename.empty() ? &std::cout : new std::ofstream(outputFilename);

        if (gd3Only)
        {
            if (f.gd3().empty())
            {
                *s << "No GD3 tag";
            }
            else
            {
                *s << f.gd3().write_to_text();
            }
        }
        else if (forTextFile)
        {
            const auto& length = Utils::samples_to_display_text(f.header().sample_count(), false);
            *s << std::format(
                "{: <{}} {}   {}",
                u8narrow(f.gd3().get_text(Gd3Tag::Key::TitleEn)),
                39 - length.length(),
                length,
                Utils::samples_to_display_text(f.header().loop_sample_count(), false));
        }
        else
        {
            f.write_to_text(*s, callback);
        }

        if (!outputFilename.empty())
        {
            delete s;
        }
    }
}

int main_utf8(int argc, char** argv)
{
    try
    {
        CLI::App app{"VGMTool CLI: VGM file editing utility"};
        app.require_subcommand()
           ->fallthrough()
           ->allow_windows_style_options()
           ->set_help_all_flag("--help-all", "Show all subcommands help");
        app.add_flag("-v, --verbose", callback.is_verbose)
           ->description("Print messages while working");

        std::vector<std::string> filenames;
        app.add_option("filename", filenames)
           ->description("The file(s) to process")
           ->required()
           ->check(CLI::ExistingFile);

        // TODO I'd like to give the vars scoped storage but the lambda also needs to be able to see them
        auto* toTextVerb = app.add_subcommand("totext")
                              ->description("Emits a text file conversion of the VGM file");
        std::string outputFilename;
        toTextVerb->add_option("--output", outputFilename)
                  ->description("Filename to output to. If not specified, output to stdout.");
        bool forTextFile = false;
        toTextVerb->add_flag("--fortxt", forTextFile)
                  ->description("Emit only the title and times for use in generating a description text file");
        bool gd3Only = false;
        toTextVerb->add_flag("--gd3", gd3Only)
                  ->description("Emit only the GD3 tag");
        toTextVerb->callback([&]
        {
            for (const auto& filename : filenames)
            {
                write_to_text(filename, outputFilename, gd3Only, forTextFile);
            }
        });

        auto* trimVerb = app.add_subcommand("trim", "Trim the file");
        int start;
        trimVerb->add_option("--start", start)
                ->description("Trim start point in samples")
                ->required()
                ->check(CLI::NonNegativeNumber);
        int loop;
        trimVerb->add_option("--loop", loop)
                ->description("Trim loop point in samples")
                ->check(CLI::NonNegativeNumber);
        int end;
        trimVerb->add_option("--end", end)
                ->description("Trim end point in samples")
                ->required()
                ->check(CLI::NonNegativeNumber);
        bool logTrim;
        trimVerb->add_flag("--log", logTrim)
                ->description("Log trim points to editpoints.txt");
        trimVerb->callback([&]
        {
            for (const auto& filename : filenames)
            {
                trim(filename, start, loop, end, false, logTrim, callback);
            }
        });

        app.add_subcommand("check")
           ->description("Check the VGM file(s) for errors")
           ->callback([&]
           {
               for (const auto& filename : filenames)
               {
                   check_lengths(filename, true, callback);
               }
           });

        auto* compressVerb = app.add_subcommand("compress", "Compress VGM file(s)");
        int iterations;
        compressVerb->add_option("--iterations", iterations)
                    ->description("Zopfli compression iterations")
                    ->default_val(15);
        compressVerb->callback([&]
        {
            for (const auto& filename : filenames)
            {
                Utils::compress(filename, callback, iterations);
            }
        });

        app.add_subcommand("decompress")
           ->description("Decompress VGM file(s)")
           ->callback([&]
           {
               for (const auto& filename : filenames)
               {
                   Utils::decompress(filename);
               }
           });

        app.add_subcommand("convert")
           ->description("Convert GYM, CYM, SSL files to VGM")
           ->callback([&]
           {
               for (const auto& filename : filenames)
               {
                   Convert::to_vgm(filename, callback);
               }
           });

        try
        {
            app.parse(argc, argv);
        }
        catch (const CLI::ParseError& e)
        {
            return app.exit(e);
        }

        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        fprintf(stderr, "Fatal error: %s", e.what()); // NOLINT(cert-err33-c)
        return EXIT_FAILURE;
    }
}
