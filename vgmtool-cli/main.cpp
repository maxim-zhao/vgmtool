#include "CLI11.hpp"
#include "libvgmtool/IVGMToolCallback.h"
#include <libvgmtool/trim.h>
#include <libvgmtool/writetotext.h>

#include "libvgmtool/convert.h"
#include "libvgmtool/Gd3Tag.h"
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

        auto* toTextVerb = app.add_subcommand("totext", "Emits a text file conversion of the VGM file");
        toTextVerb->add_option("--output", "Filename to output to. If not specified, output to stdout.")->check(CLI::NonexistentPath);
        toTextVerb->add_flag("--fortxt", "Emit only the title and times for use in generating a description text file");

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
                const auto* output = toTextVerb->get_option("--output");
                write_to_text(filename, callback, output->empty(), output->as<std::string>());
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
