#include "CLI11.hpp"
#include "libvgmtool/IVGMToolCallback.h"
#include <libvgmtool/trim.h>

#include "libvgmtool/convert.h"
#include "libvgmtool/utils.h"
#include "libvgmtool/vgm.h"
#include "libvgmtool/VgmFile.h"

#include <libpu8/libpu8/libpu8.h>

#ifdef WIN32
#include <Windows.h>
#endif

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

int main_utf8(int argc, char** argv)
{
    try
    {
        CLI::App app{"VGMTool CLI: VGM file editing utility"};
        app.require_subcommand()
           ->fallthrough()
           ->set_help_all_flag("--help-all", "Show all subcommands help");
        app.add_flag("-v, --verbose", callback.is_verbose, "Print messages while working");

        auto* toTextVerb = app.add_subcommand("totext", "Emits a text file conversion of the VGM file");
        toTextVerb->add_option("--output", "Filename to output to. If not specified, output to stdout.");
        toTextVerb->add_flag("--fortxt", "Emit only the title and times for use in generating a description text file");
        toTextVerb->add_flag("--gd3", "Emit only the GD3 tag");

        auto* trimVerb = app.add_subcommand("trim", "Trim the file");
        trimVerb->add_option("--start", "Trim start point in samples")->required()->
                  check(CLI::NonNegativeNumber);
        trimVerb->add_option("--loop", "Trim loop point in samples")->check(CLI::NonNegativeNumber);
        trimVerb->add_option("--end", "Trim end point in samples")->required()->check(CLI::NonNegativeNumber);
        trimVerb->add_flag("--log", "Log trim points to editpoints.txt");

        const auto* checkVerb = app.add_subcommand("check", "Check the VGM file(s) for errors");

        auto* compressVerb = app.add_subcommand("compress", "Compress VGM file(s)");
        compressVerb->add_option("--iterations", "Zopfli compression iterations")
                    ->default_val(15);

        auto* decompressVerb = app.add_subcommand("decompress", "Decompress VGM file(s)");

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
                const auto* output = toTextVerb->get_option("--output");
                write_to_text(
                    filename,
                    output->as<std::string>(),
                    toTextVerb->get_option("--gd3")->as<bool>(),
                    toTextVerb->get_option("--fortxt")->as<bool>());
            }

            if (trimVerb->parsed())
            {
                trim(
                    filename,
                    trimVerb->get_option("--start")->as<int>(),
                    trimVerb->get_option("--loop")->as<int>(),
                    trimVerb->get_option("--end")->as<int>(),
                    false,
                    trimVerb->get_option("--logTrim")->as<bool>(),
                    callback);
            }

            if (checkVerb->parsed())
            {
                check_lengths(filename, true, callback);
            }

            if (compressVerb->parsed())
            {
                Utils::compress(filename, compressVerb->get_option("--iterations")->as<int>());
            }

            if (decompressVerb->parsed())
            {
                Utils::decompress(filename);
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
        fprintf(stderr, "Fatal error: %s", e.what()); // NOLINT(cert-err33-c)
        return EXIT_FAILURE;
    }
}
