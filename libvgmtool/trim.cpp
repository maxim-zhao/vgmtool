#include <cstdlib>
#include "trim.h"

#include <filesystem>
#include "IStatusCallback.h"
#include "SN76489State.h"
#include "vgm.h"
#include "VgmFile.h"
#include "VgmCommands.h"

static void add_pause(std::vector<std::shared_ptr<VgmCommands::ICommand>>& stream, int pauseLength)
{
    if (pauseLength == 0)
    {
        return;
    }

    // This is not quite optimal - it depends upon what the length modulo 0xffff is.
    // If it is not any of the <3 byte options, we would be better off emitting a
    // 16-bit wait that makes it so, if possible. This is unlikely to happen
    // very often.

    while (pauseLength > 0xffff)
    {
        const auto wait = std::make_shared<VgmCommands::Wait16bit>();
        wait->set_duration(0xffff);
        stream.push_back(wait);
        pauseLength -= 0xffff;
    }

    // Two one-byte commands are more efficient than a three-byte wait
    if (pauseLength == LEN60TH * 2)
    {
        stream.push_back(std::make_shared<VgmCommands::Wait60th>());
        stream.push_back(std::make_shared<VgmCommands::Wait60th>());
    }
    else if (pauseLength == LEN60TH)
    {
        stream.push_back(std::make_shared<VgmCommands::Wait60th>());
    }
    else if (pauseLength == LEN50TH * 2)
    {
        stream.push_back(std::make_shared<VgmCommands::Wait50th>());
        stream.push_back(std::make_shared<VgmCommands::Wait50th>());
    }
    else if (pauseLength == LEN50TH)
    {
        stream.push_back(std::make_shared<VgmCommands::Wait50th>());
    }
    else if (pauseLength <= 16)
    {
        const auto wait = std::make_shared<VgmCommands::Wait4bit>();
        wait->set_duration(pauseLength);
        stream.push_back(wait);
    }
    else
    {
        const auto wait = std::make_shared<VgmCommands::Wait16bit>();
        wait->set_duration(static_cast<uint16_t>(pauseLength));
        stream.push_back(wait);
    }
}

static void optimise_pauses(CommandStream& commandStream)
{
    // We walk the command stream, merging any consecutive pure pauses
    auto currentPauseLength = 0;
    std::vector<std::shared_ptr<VgmCommands::ICommand>> output;
    for (const auto& command : commandStream.commands())
    {
        if (const auto& pause = std::dynamic_pointer_cast<VgmCommands::Wait>(command);
            pause && command->chip() == VgmHeader::Chip::Nothing)
        {
            // It's a pause. Add to the running total.
            currentPauseLength += pause->duration();
        }
        else
        {
            // Emit any pending pause
            if (currentPauseLength > 0)
            {
                add_pause(output, currentPauseLength);
                currentPauseLength = 0;
            }
            output.push_back(command);
        }
    }
    // And any trailing pause
    if (currentPauseLength > 0)
    {
        add_pause(output, currentPauseLength);
    }
    // Finally, swap it in
    commandStream.commands().swap(output);
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
    // TODO: make this extensible to more chips
    SN76489State currentPsgState(header);
    SN76489State lastWrittenPsgState(header);
    SN76489State loopPsgState(header);
    SN76489State startPsgState(header);
    //uint8_t ym2413Regs[YM2413NumRegs]{};
    //uint8_t lastWrittenYM2413Regs[YM2413NumRegs]{};

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
        if (const auto wait = std::dynamic_pointer_cast<VgmCommands::Wait>(command))
        {
            pendingTime += wait->duration();
        }
        switch (command->chip())  // NOLINT(clang-diagnostic-switch-enum)
        {
        case VgmHeader::Chip::Nothing:
            break;
        case VgmHeader::Chip::SN76489:
            if (const auto ggStereo = std::dynamic_pointer_cast<VgmCommands::GGStereo>(command))
            {
                currentPsgState.add(ggStereo);
            }
            else if (const auto sn76489 = std::dynamic_pointer_cast<VgmCommands::SN76489>(command))
            {
                currentPsgState.add(sn76489);
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
                // Emit the delta...
                currentPsgState.copy_to_command_stream(*currentStream, lastWrittenPsgState, false);
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
                startPsgState = currentPsgState;
                lastWrittenPsgState = currentPsgState;
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
                add_pause(currentStream->commands(), timeToEmit);
                pendingTime -= timeToEmit;
                time += timeToEmit;
            }
            if (timeBefore <= loop && timeAfter > loop)
            {
                // We are passing the loop point
                // Capture the current state for later
                loopPsgState = currentPsgState;
                // Switch to the second stream
                currentStream = &vgmFile.data_with_loop();
                // And as much time as we need to get to the end point, or just all the pending time
                const auto timeToEmit = std::min(pendingTime, end - loop);
                add_pause(currentStream->commands(), timeToEmit);
                pendingTime -= timeToEmit;
                time += timeToEmit;
            }
            if (timeBefore < end && timeAfter >= end)
            {
                // We are reaching or passing the end point
                // Emit as much time as we need to get to the end point, or just all the pending time
                const auto timeToEmit = std::min(pendingTime, end - time);
                add_pause(currentStream->commands(), timeToEmit);
                // Emit a delta to get back to the loop state
                loopPsgState.copy_to_command_stream(*currentStream, currentPsgState, false);
                // Add an end marker
                currentStream->commands().push_back(std::make_shared<VgmCommands::End>());
                // And then we are done. Break the outer loop.
                break;
            }
            if (pendingTime > 0 && currentStream != nullptr)
            {
                // We are not passing any edit points, so just emit all the pending time
                add_pause(currentStream->commands(), pendingTime);
            }
            // Finally, remember the new time
            time = timeAfter;
        }
    }

    // Inject the start state at the beginning
    // TODO make this conditional on it being needed for each chip
    CommandStream startState;
    currentPsgState.copy_to_command_stream(startState, startPsgState, true);
    vgmFile.data_before_loop().commands().insert(
        vgmFile.data_before_loop().commands().begin(),
        std::make_move_iterator(startState.commands().begin()),
        std::make_move_iterator(startState.commands().end())
    );

    // We emitted the pauses as-is. Now we optimise them.
    optimise_pauses(vgmFile.data_before_loop());
    optimise_pauses(vgmFile.data_with_loop());

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
