#include "libvgmtool/IVGMToolCallback.h"
#include <libvgmtool/trim.h>

#include "libvgmtool/convert.h"
#include "libvgmtool/utils.h"
#include "libvgmtool/vgm.h"
#include "libvgmtool/VgmFile.h"

#include <libpu8/libpu8/libpu8.h>
#include "CLI11.hpp"

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

    void write_to_text(const VgmFile& f, const std::string& outputFilename, bool gd3Only, bool forTextFile)
    {
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
        /*

        {
            auto verb = app.add_subcommand("convert")
                           ->description("Convert GYM, CYM, SSL files to VGM");
            std::vector<std::string> filenames;
            verb->add_option("filename", filenames)
                ->description("The file(s) to process")
                ->required()
                ->check(CLI::ExistingFile);
            verb->callback([&]
            {
                for (const auto& filename : filenames)
                {
                    Convert::to_vgm(filename, callback);
                }
            });
        }

        */

        // New way of working...
        // - Always take a file
        std::string filename;
        app.add_option("filename", filename)
           ->description("The input file")
           ->required()
           ->check(CLI::ExistingFile);

        // We want to load the file first...
        VgmFile f;
        app.parse_complete_callback([&]
        {
            f.load_file(filename);
        });

        // - Then take verbs for actions on it
        {
            auto verb = app.add_subcommand("gd3")
                           ->description("Set GD3 tag fields on a VGM file");
            std::wstring titleEn, titleJa, gameEn, gameJa, systemEn, systemJa, authorEn, authorJa, releaseDate, creator,
                         notes;

            verb->add_option("--title-en", titleEn)->description("Title (EN)");
            verb->add_option("--title-ja", titleJa)->description("Title (JA)");
            verb->add_option("--game-en", gameEn)->description("Game (EN)");
            verb->add_option("--game-ja", gameJa)->description("Game (JA)");
            verb->add_option("--system-en", systemEn)->description("System (EN)");
            verb->add_option("--system-ja", systemJa)->description("System (JA)");
            verb->add_option("--author-en", authorEn)->description("Author (EN)");
            verb->add_option("--author-ja", authorJa)->description("Author (JA)");
            verb->add_option("--release-date", releaseDate)->description("Release date");
            verb->add_option("--creator", creator)->description("Creator");
            verb->add_option("--notes", notes)->description("Notes");

            verb->callback([&]
            {
                f.gd3().set_text(Gd3Tag::Key::TitleEn, titleEn);
                f.gd3().set_text(Gd3Tag::Key::TitleJa, titleJa);
                f.gd3().set_text(Gd3Tag::Key::GameEn, gameEn);
                f.gd3().set_text(Gd3Tag::Key::GameJa, gameJa);
                f.gd3().set_text(Gd3Tag::Key::SystemEn, systemEn);
                f.gd3().set_text(Gd3Tag::Key::SystemJa, systemJa);
                f.gd3().set_text(Gd3Tag::Key::AuthorEn, authorEn);
                f.gd3().set_text(Gd3Tag::Key::AuthorJa, authorJa);
                f.gd3().set_text(Gd3Tag::Key::ReleaseDate, releaseDate);
                f.gd3().set_text(Gd3Tag::Key::Creator, creator);
                f.gd3().set_text(Gd3Tag::Key::Notes, notes);
            });
        }

        {
            auto* verb = app
                .add_subcommand("totext")
                ->description("Emits a text file conversion of the VGM file");
            auto output = verb
                ->add_option("--output")
                ->type_name("TEXT")
                ->description("Filename to output to. If not specified, output to stdout.");
            auto forTextFile = verb
                ->add_flag("--fortxt")
                ->description("Emit only the title and times for use in generating a description text file");
            auto gd3Only = verb
                ->add_flag("--gd3")
                ->description("Emit only the GD3 tag");
            verb->callback([&]
            {
                write_to_text(
                    f, 
                    output->as<std::string>(), 
                    gd3Only->as<bool>(), 
                    forTextFile->as<bool>());
            });
        }

        {
            auto* trimVerb = app.add_subcommand("trim", "Trim the file");
            int start = 0;
            trimVerb->add_option("--start", start)
                    ->description("Trim start point in samples")
                    ->default_val(0)
                    ->check(CLI::NonNegativeNumber);
            int loop = -1;
            trimVerb->add_option("--loop", loop)
                    ->description("Trim loop point in samples. Omit or set to negative value to disable.")
                    ->default_val(-1)
                    ->check(CLI::Number);
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
                trim_vgm_file(f, start, loop, end, callback);
            });
        }

        {
            const auto verb = app.add_subcommand("check")
                                 ->description("Check the VGM file(s) for errors");
            verb->callback([&]
            {
                f.check_header(false); // Will throw if it's wrong, and be logged at the end
            });
        }

        {
            // The last verb is save, which saves the file back to disk
            auto verb = app.add_subcommand("save")
                           ->description("Save the modified VGM file");
            std::string saveFilename;
            verb->add_option("--as", saveFilename)
                ->description("The output file. If not set, the original file will be overwritten.");
            int compression = 0;
            verb->add_option("--compression", compression)
                ->description("Compress the output file. Higher values result in smaller files but take longer to compress. Omit or use 0 for no compression.")
                ->check(CLI::Range(0, 100));
            verb->callback([&]
            {
                if (saveFilename.empty())
                {
                    saveFilename = filename;
                }
                f.save_file(saveFilename, callback, compression);
            });
        }

        // TODO: warn, or something else, if we don't save and something happened to the VGM

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
