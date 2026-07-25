#include "trim.h"

#include <filesystem>
#include <ranges>

#include "IStatusCallback.h"
#include "SN76489State.h"
#include "YM2413State.h"
#include "VgmFile.h"
#include "VgmCommands.h"

namespace
{
    class ChipStatesTracker
    {
        std::shared_ptr<IChipState> _current;
        std::shared_ptr<IChipState> _lastWritten;
        std::shared_ptr<IChipState> _start;
        std::shared_ptr<IChipState> _loop;
    public:
        ChipStatesTracker() = default; // Default one is empty!
        explicit ChipStatesTracker(const IChipState& base)
            : _current(base.clone()),
        _lastWritten(base.clone()),
        _start(nullptr),
        _loop(nullptr)
        {
        }

        void emit_delta(CommandStream& commandStream) const
        {
            _current->copy_to_command_stream(commandStream, _lastWritten, IChipState::WriteTypes::automatic);
        }

        void snapshot_start()
        {
            _start = _current->clone();
            *_lastWritten = *_current;
        }

        void snapshot_loop()
        {
            _loop = _current->clone();
        }

        void emit_loop_delta(CommandStream& commandStream) const
        {
            // We might not have one, in which case this is a no-op
            if (_loop)
            {
                _loop->copy_to_command_stream(commandStream, _current, IChipState::WriteTypes::force_delta);
            }
        }

        void insert_start_state(VgmFile& vgmFile) const
        {
            CommandStream startState;
            _start->copy_to_command_stream(startState, _start->clone(), IChipState::WriteTypes::force_full_image);
            vgmFile.data_before_loop().commands().insert(
                vgmFile.data_before_loop().commands().begin(),
                std::make_move_iterator(startState.commands().begin()),
                std::make_move_iterator(startState.commands().end())
            );

        }

        void add(const std::shared_ptr<const VgmCommands::ICommand>& command) const
        {
            _current->add(command);
        }
    };

    std::shared_ptr<ChipStatesTracker> getTracker(std::unordered_map<Chip, std::shared_ptr<ChipStatesTracker>>& map, const Chip chip, const VgmHeader& header)
    {
        // If we have it in the map, return it, else create it
        if (const auto it = map.find(chip); it != map.end())
        {
            return it->second;
        }
        switch (chip)
        {
        case Chip::SN76489:
            return map.emplace(chip, std::make_shared<ChipStatesTracker>(SN76489State(header))).first->second;
        case Chip::YM2413:
            return map.emplace(chip, std::make_shared<ChipStatesTracker>(YM2413State(header))).first->second;
        default:
            // Return nothing by default
            return {};
        }
    }
}
void trim_vgm_file(VgmFile& vgmFile, int start, int loop, int end, const IStatusCallback& callback)
{
    callback.verbose_message(std::format("Trimming VGM file: start {}, loop {}, end {}", start, loop, end));

    auto& header = vgmFile.header();

    // Validate edit points
    if ((start > end) || (loop > end) || ((loop > -1) && (loop < start)))
    {
        callback.error(std::format("Impossible edit points: start={}, loop={}, end={}", start, loop, end));
        return;
    }

    if (std::cmp_greater(end, header.sample_count()))
    {
        callback.message(std::format(
            "End point ({} samples) beyond end of file!\nUsing maximum value of {} samples instead",
            end,
            header.sample_count()));
        end = static_cast<int>(header.sample_count());
    }

    callback.verbose_message("Trimming VGM data...");

    // Copy all the commands from the VGM file into one big stream
    CommandStream allCommands;
    std::ranges::copy(vgmFile.data_before_loop().commands(), std::back_inserter(allCommands.commands()));
    std::ranges::copy(vgmFile.data_with_loop().commands(), std::back_inserter(allCommands.commands()));

    // Initialize tracking state(s)
    std::unordered_map<Chip, std::shared_ptr<ChipStatesTracker>> chipStateTrackers;

    CommandStream* currentStream = nullptr;
    vgmFile.data_before_loop().commands().clear();
    vgmFile.data_with_loop().commands().clear();

    // I want to walk through all the data...
    // While time < start, we just track chip state.
    // When time passes start, we emit a full image and any remaining wait until either loop or end.
    // While time < end, we track chip state and emit waits and deltas.
    // When time passes loop, we capture the chip state for later.
    // When time passes end, we emit the delta to get back to the loop state.
    int time = 0; // TODO extend times to 64 bit? 2^32 samples is 2^32 / 44100 = 27 hours, so probably not needed for now
    for (const auto& command : allCommands.commands())
    {
        // Every command maybe has some time, and maybe changes the chip state(s).
        auto pendingTime = 0;
        if (const auto wait = std::dynamic_pointer_cast<const VgmCommands::Wait>(command))
        {
            pendingTime += wait->duration();
        }
        switch (command->chip())  // NOLINT(clang-diagnostic-switch-enum)
        {
        case Chip::Nothing:
            break;
        case Chip::SN76489:
            if (const auto ggStereo = std::dynamic_pointer_cast<const VgmCommands::GGStereo>(command))
            {
                getTracker(chipStateTrackers, Chip::SN76489, header)->add(ggStereo);
            }
            else if (const auto sn76489 = std::dynamic_pointer_cast<const VgmCommands::SN76489>(command))
            {
                getTracker(chipStateTrackers, Chip::SN76489, header)->add(sn76489);
            }
            break;
            // TODO lots more chips to handle
        default:
            // Pass through commands for other chips, if we are past the start.
            if (time > start)
            {
                currentStream->commands().push_back(command);
            }
            break;
        }

        if (pendingTime > 0)
        {
            if (currentStream != nullptr)
            {
                // Emit the deltas for each chip that's active...
                for (const auto & tracker : chipStateTrackers | std::views::values)
                {
                    tracker->emit_delta(*currentStream);
                }
            }
            // Add it on... but maybe not all of it.
            const auto timeBefore = time;
            const auto timeAfter = time + pendingTime;
            // ---|-------|-----|----
            //    start   loop  end
            // Start and loop can be the same.
            // We might have moved past one or more of these, and we want to take action on each.
            // Note that we want to trigger the start and loop when we have pending time
            // moving us *after* the point in question, so that we capture the state
            // just before the time is added.
            if (timeBefore <= start && timeAfter > start)
            {
                // We are passing the start point
                currentStream = &vgmFile.data_before_loop();
                // Remember the state
                for (const auto & tracker : chipStateTrackers | std::views::values)
                {
                    tracker->snapshot_start();
                }
                // Subtract any time before the start point from the pending time
                pendingTime -= (start - timeBefore);
                // Emit as much time as we need to get to the loop point, or end, or just all the pending time
                auto timeToEmit = pendingTime;
                if (loop > -1 && timeAfter >= loop)
                {
                    // We are also passing the loop point. Only emit time up to there (might be 0).
                    timeToEmit = loop - start;
                }
                else if (timeAfter >= end)
                {
                    // We are also passing the end point. Only emit time up to there.
                    timeToEmit = end - start;
                }
                currentStream->add_pause(timeToEmit);
                pendingTime -= timeToEmit;
                time += timeToEmit;
            }
            if (timeBefore <= loop && timeAfter > loop)
            {
                // We are passing the loop point
                // Capture the current state for later
                for (const auto & tracker : chipStateTrackers | std::views::values)
                {
                    tracker->snapshot_loop();
                }
                // Switch to the second stream
                currentStream = &vgmFile.data_with_loop();
                // And as much time as we need to get to the end point, or just all the pending time
                const auto timeToEmit = std::min(pendingTime, end - loop);
                currentStream->add_pause(timeToEmit);
                pendingTime -= timeToEmit;
                time += timeToEmit;
            }
            if (timeBefore < end && timeAfter >= end)
            {
                // We are reaching or passing the end point
                // Emit as much time as we need to get to the end point, or just all the pending time
                const auto timeToEmit = std::min(pendingTime, end - time);
                currentStream->add_pause(timeToEmit);
                // Emit a delta to get back to the loop state
                for (const auto& tracker : chipStateTrackers | std::views::values)
                {
                    tracker->emit_loop_delta(*currentStream);
                }
                // Add an end marker
                currentStream->commands().push_back(std::make_shared<VgmCommands::End>());
                // And then we are done. Break the outer loop.
                break;
            }
            if (pendingTime > 0 && currentStream != nullptr)
            {
                // We are not passing any edit points, so just emit all the pending time
                currentStream->add_pause(pendingTime);
            }
            // Finally, remember the new time
            time = timeAfter;
        }
    }

    // Inject the start state at the beginning
    for (const auto& tracker : chipStateTrackers | std::views::values)
    {
        tracker->insert_start_state(vgmFile);
    }

    // We emitted the pauses as-is. Now we optimise them.
    vgmFile.data_before_loop().optimise_pauses();
    vgmFile.data_with_loop().optimise_pauses();

    // Update header
    header.set_sample_count(end - start);
    if (loop > -1)
    {
        header.set_loop_sample_count(end - loop);
    }
    else
    {
        header.set_loop_sample_count(0);
    }

    // TODO report here on the timings in m:ss.fff
    callback.verbose_message(std::format(
        "Trimming complete: {} commands -> {} + {}",
        allCommands.commands().size(),
        vgmFile.data_before_loop().commands().size(),
        vgmFile.data_with_loop().commands().size()));
}
