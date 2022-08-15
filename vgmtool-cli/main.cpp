#include "CLI11.hpp"
#include "libvgmtool/IVGMToolCallback.h"
#include <libvgmtool/trim.h>
#include <libvgmtool/writetotext.h>

#include "libvgmtool/BinaryData.h"
#include "libvgmtool/convert.h"
#include "libvgmtool/Gd3Tag.h"
#include "libvgmtool/utils.h"
#include "libvgmtool/vgm.h"

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


void print_tag(const std::string& description, const Gd3Tag& tag, Gd3Tag::Key key)
{
    const auto& text = u8narrow(tag.get_text(key));
    if (text.empty())
    {
        return;
    }
    if (text.find('\n') != std::string::npos)
    {
        // Text with line breaks
        printf("%s:\n========================\n%s\n========================\n", description.c_str(), text.c_str());
    }
    else
    {
        printf("%s:\t%s\n", description.c_str(), text.c_str());
    }
}

void show_gd3(const std::string& filename)
{
    VgmFile file(filename);
    if (file.header().gd3_offset() == 0)
    {
        printf("No GD3 tag");
        return;
    }

    const auto& tag = file.gd3();
    print_tag("Title (EN)", tag, Gd3Tag::Key::TitleEn);
    print_tag("Title (JP)", tag, Gd3Tag::Key::TitleJp);
    print_tag("Author (EN)", tag, Gd3Tag::Key::AuthorEn);
    print_tag("Author (EN)", tag, Gd3Tag::Key::AuthorJp);
    print_tag("Game (EN)", tag, Gd3Tag::Key::GameEn);
    print_tag("Game (JP)", tag, Gd3Tag::Key::GameJp);
    print_tag("System (EN)", tag, Gd3Tag::Key::SystemEn);
    print_tag("System (JP)", tag, Gd3Tag::Key::SystemJp);
    print_tag("Release date", tag, Gd3Tag::Key::ReleaseDate);
    print_tag("Creator", tag, Gd3Tag::Key::Creator);
    print_tag("Notes", tag, Gd3Tag::Key::Notes);
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

        auto* decompressVerb = app.add_subcommand("decompress", "Decompress VGM file(s)");

        const auto* convertVerb = app.add_subcommand("convert", "Convert GYM, CYM, SSL files to VGM");

        const auto* showGd3Verb = app.add_subcommand("showgd3", "Show the GD3 tag");

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

            if (decompressVerb->parsed())
            {
                Utils::decompress(filename);
            }

            if (convertVerb->parsed())
            {
                Convert::to_vgm(filename, callback);
            }

            if (showGd3Verb->parsed())
            {
                show_gd3(filename);
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
