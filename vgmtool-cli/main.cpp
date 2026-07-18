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
        auto* s = outputFilename.empty()
            ? &std::cout
            : new std::ofstream(outputFilename);

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

        // - Then take verbs for actions on it.
        // These must have storage for the parameters that lasts for as long as the time to the callback,
        // which means they clutter the scope, so we put them in local structs.
        struct
        {
            std::wstring titleEn;
            std::wstring titleJa;
            std::wstring gameEn;
            std::wstring gameJa;
            std::wstring systemEn;
            std::wstring systemJa;
            std::wstring authorEn;
            std::wstring authorJa;
            std::wstring releaseDate;
            std::wstring creator;
            std::wstring notes;
        } gd3Args;
        auto* pGd3Verb = app.add_subcommand("gd3")
            ->description("Set GD3 tag fields on a VGM file")
            ->callback([&]
            {
                f.gd3().set_text(Gd3Tag::Key::TitleEn, gd3Args.titleEn);
                f.gd3().set_text(Gd3Tag::Key::TitleJa, gd3Args.titleJa);
                f.gd3().set_text(Gd3Tag::Key::GameEn, gd3Args.gameEn);
                f.gd3().set_text(Gd3Tag::Key::GameJa, gd3Args.gameJa);
                f.gd3().set_text(Gd3Tag::Key::SystemEn, gd3Args.systemEn);
                f.gd3().set_text(Gd3Tag::Key::SystemJa, gd3Args.systemJa);
                f.gd3().set_text(Gd3Tag::Key::AuthorEn, gd3Args.authorEn);
                f.gd3().set_text(Gd3Tag::Key::AuthorJa, gd3Args.authorJa);
                f.gd3().set_text(Gd3Tag::Key::ReleaseDate, gd3Args.releaseDate);
                f.gd3().set_text(Gd3Tag::Key::Creator, gd3Args.creator);
                f.gd3().set_text(Gd3Tag::Key::Notes, gd3Args.notes);
            });
        pGd3Verb->add_option("--title-en", gd3Args.titleEn)->description("Title (EN)");
        pGd3Verb->add_option("--title-ja", gd3Args.titleJa)->description("Title (JA)");
        pGd3Verb->add_option("--game-en", gd3Args.gameEn)->description("Game (EN)");
        pGd3Verb->add_option("--game-ja", gd3Args.gameJa)->description("Game (JA)");
        pGd3Verb->add_option("--system-en", gd3Args.systemEn)->description("System (EN)");
        pGd3Verb->add_option("--system-ja", gd3Args.systemJa)->description("System (JA)");
        pGd3Verb->add_option("--author-en", gd3Args.authorEn)->description("Author (EN)");
        pGd3Verb->add_option("--author-ja", gd3Args.authorJa)->description("Author (JA)");
        pGd3Verb->add_option("--release-date", gd3Args.releaseDate)->description("Release date");
        pGd3Verb->add_option("--creator", gd3Args.creator)->description("Creator");
        pGd3Verb->add_option("--notes", gd3Args.notes)->description("Notes");

        struct
        {
            std::string output{};
            bool forTextFile{};
            bool gd3Only{};
        } toTextArgs;
        auto* toTextVerb = app.add_subcommand("totext")
            ->description("Emit a text file conversion of the VGM file")
            ->callback([&]
            {
                write_to_text(f, toTextArgs.output, toTextArgs.gd3Only, toTextArgs.forTextFile);
            });
        toTextVerb->add_option("--output", toTextArgs.output)
            ->description("Filename to output to. If not specified, output to stdout.");
        toTextVerb->add_flag("--fortxt", toTextArgs.forTextFile)
            ->description("Emit only the title and times for use in generating a description text file");
        toTextVerb->add_flag("--gd3", toTextArgs.gd3Only)
            ->description("Emit only the GD3 tag");

        struct
        {
            int start = 0;
            int loop = -1;
            int end;
            std::string logTrim;
        } trimArgs;
        auto* trimVerb = app.add_subcommand("trim")
            ->description("Trim the file")
            ->callback([&]
            {
                trim_vgm_file(f, trimArgs.start, trimArgs.loop, trimArgs.end, callback);
            });
        trimVerb->add_option("--start", trimArgs.start)
            ->description("Trim start point in samples")
            ->default_val(0)
            ->check(CLI::NonNegativeNumber);
        trimVerb->add_option("--loop", trimArgs.loop)
            ->description("Trim loop point in samples. Omit or set to negative value to disable.");
        trimVerb->add_option("--end", trimArgs.end)
            ->description("Trim end point in samples")
            ->required()
            ->check(CLI::NonNegativeNumber);
        //trimVerb->add_option("--log", trimArgs.logTrim)
        //    ->description("Log trim points to this file");

        app.add_subcommand("check")
            ->description("Check the VGM file for errors")
            ->callback([&]
            {
                f.check_header(false); // Will throw if it's wrong, and be logged at the end
            });

        struct
        {
            // The last verb is save, which saves the file back to disk
            std::string saveFilename;
            int compression = 15;
        } saveArgs;
        auto* saveVerb = app.add_subcommand("save")
            ->description("Save the modified VGM file")
            ->callback([&]
            {
                if (saveArgs.saveFilename.empty())
                {
                    saveArgs.saveFilename = filename;
                }
                f.save_file(saveArgs.saveFilename, callback, callback.is_verbose, saveArgs.compression);
            });
        saveVerb->add_option("--as", saveArgs.saveFilename)
            ->description("The output file. If not set, the original file will be overwritten.");
        saveVerb->add_option("--compression", saveArgs.compression)
            ->description("Compress the output file. Higher values result in smaller files but take longer to compress. Use 0 for no compression. A value of 15 is a good middle ground.")
            ->default_val(saveArgs.compression)
            ->check(CLI::Range(0, 1000));

        // TODO: warn, or something else, if we don't save and something happened to the VGM
        // Tricky to do - what if they did a no-op to it?
        // We could deep-copy the whole thing at the start and compare at the end, but that seems like a lot of overhead.

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
