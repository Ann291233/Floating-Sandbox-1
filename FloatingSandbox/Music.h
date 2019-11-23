/***************************************************************************************
* Original Author:		Gabriele Giuseppini
* Created:				2019-11-17
* Copyright:			Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#pragma once

#include <GameCore/GameException.h>
#include <GameCore/GameRandomEngine.h>
#include <GameCore/GameTypes.h>
#include <GameCore/GameWallClock.h>
#include <GameCore/Log.h>

#include <SFML/Audio.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <memory>
#include <limits>
#include <optional>
#include <string>
#include <vector>

/*
 * Our base wrapper for sf::Music.
 *
 * Provides volume control based on a master volume and a local volume,
 * and facilities to fade-in and fade-out.
 */
class BaseGameMusic
{
public:

    BaseGameMusic(
        float volume,
        float masterVolume,
        bool isMuted,
        std::chrono::milliseconds timeToFadeIn,
        std::chrono::milliseconds timeToFadeOut)
        : mTimeToFadeIn(timeToFadeIn)
        , mTimeToFadeOut(timeToFadeOut)
        , mMusic()
        , mVolume(volume)
        , mMasterVolume(masterVolume)
        , mFadeLevel(1.0f)
        , mIsMuted(isMuted)
        , mCurrentState(StateType::Stopped)
        , mDesiredState()
        , mFadeStateStartTimestamp(GameWallClock::time_point::min())
    {
        InternalSetVolume();
    }

    void SetVolume(float volume)
    {
        mVolume = volume;
        InternalSetVolume();
    }

    void SetMasterVolume(float masterVolume)
    {
        mMasterVolume = masterVolume;
        InternalSetVolume();
    }

    void SetMuted(bool isMuted)
    {
        mIsMuted = isMuted;
        InternalSetVolume();
    }

    void SetVolumes(
        float volume,
        float masterVolume,
        bool isMuted)
    {
        mVolume = volume;
        mMasterVolume = masterVolume;
        mIsMuted = isMuted;
        InternalSetVolume();
    }

    /*
     * NOP if already playing.
     */
    void Play()
    {
        mDesiredState.emplace(StateType::Playing);
    }

    /*
     * NOP if already playing.
     */
    void FadeToPlay()
    {
        mDesiredState.emplace(StateType::FadingIn);
    }

    /*
     * NOP if already stopped.
     */
    void Stop()
    {
        mDesiredState.emplace(StateType::Stopped);
    }

    /*
     * NOP if already stopped.
     */
    void FadeToStop()
    {
        mDesiredState.emplace(StateType::FadingOut);
    }

    /*
     * NOP if already paused.
     */
    void Pause()
    {
        mMusic.pause();
    }

    /*
     * NOP if already playing.
     */
    void Resume()
    {
        if (sf::Sound::Status::Paused == mMusic.getStatus())
        {
            mMusic.play();
        }
    }

    void Reset()
    {
        mMusic.stop();

        mCurrentState = StateType::Stopped;
        mDesiredState.reset();
    }

    void Update()
    {
        switch (mCurrentState)
        {
            case StateType::Stopped:
            {
                Update_Stopped();
                break;
            }

            case StateType::FadingIn:
            {
                Update_FadingIn();
                break;
            }

            case StateType::Playing:
            {
                Update_Playing();
                break;
            }

            case StateType::FadingOut:
            {
                Update_FadingOut();
                break;
            }
        }
    }

protected:

    virtual std::optional<std::filesystem::path> GetNextMusicFileToPlay() = 0;

    sf::Music mMusic;

private:

    void InternalSetVolume()
    {
        if (!mIsMuted)
            mMusic.setVolume(100.0f * (mVolume / 100.0f) * (mMasterVolume / 100.0f) * mFadeLevel);
        else
            mMusic.setVolume(0.0f);
    }

private:

    enum class StateType;

    inline std::optional<StateType> PopDesiredState()
    {
        if (!!mDesiredState)
        {
            auto const r = mDesiredState;
            mDesiredState.reset();
            return r;
        }
        else
        {
            return std::nullopt;
        }
    }

    inline void EnsureMusicIsPlaying()
    {
        if (sf::SoundSource::Status::Stopped == mMusic.getStatus())
        {
            auto const musicFilePath = GetNextMusicFileToPlay();
            if (!!musicFilePath)
            {
                if (!mMusic.openFromFile(musicFilePath->string()))
                {
                    throw GameException("Cannot load \"" + musicFilePath->string() + "\" music");
                }

                // Play
                mMusic.play();
            }
        }
    }

    void Update_Stopped()
    {
        //
        // Check if we need to transition
        //

        auto const desiredState = PopDesiredState();
        if (!!desiredState)
        {
            switch (*desiredState)
            {
                case StateType::FadingIn:
                {
                    LogMessage("TODOTEST:TransitionToFadingIn");
                    // Transition
                    mFadeStateStartTimestamp = GameWallClock::GetInstance().Now();
                    mCurrentState = StateType::FadingIn;

                    // Start right away
                    Update();

                    return;
                }

                case StateType::FadingOut:
                {
                    // Consume and continue
                    break;
                }

                case StateType::Playing:
                {
                    // Transition
                    TransitionToPlaying();

                    // Start right away
                    Update();

                    return;
                }

                case StateType::Stopped:
                {
                    // Consume and continue
                    break;
                }
            }
        }
    }

    void Update_FadingIn()
    {
        //
        // Check if we need to transition
        //

        auto const desiredState = PopDesiredState();
        if (!!desiredState)
        {
            switch (*desiredState)
            {
                case StateType::FadingIn:
                {
                    // Consume and continue
                    break;
                }

                case StateType::FadingOut:
                {
                    LogMessage("TODOTEST:TransitionToFadingOut");
                    // Transition
                    mFadeStateStartTimestamp = AdjustFadeStartTimestamp(mTimeToFadeIn, mTimeToFadeOut);
                    mCurrentState = StateType::FadingOut;

                    // Start right away
                    Update();

                    return;
                }

                case StateType::Playing:
                {
                    // Transition
                    TransitionToPlaying();

                    // Start right away
                    Update();

                    return;
                }

                case StateType::Stopped:
                {
                    // Transition
                    TransitionToStopped();

                    return;
                }
            }
        }

        //
        // Make sure music is playing
        //

        EnsureMusicIsPlaying();

        //
        // Update fade-in state machine
        //

        auto elapsedMillis = std::chrono::duration_cast<std::chrono::milliseconds>(
            GameWallClock::GetInstance().Elapsed(mFadeStateStartTimestamp));

        mFadeLevel = std::min(
            1.0f,
            static_cast<float>(elapsedMillis.count()) / static_cast<float>(mTimeToFadeOut.count()));

        InternalSetVolume();

        // Check if we're done
        if (elapsedMillis >= mTimeToFadeIn)
        {
            // Transition
            TransitionToPlaying();
        }
    }

    void Update_Playing()
    {
        //
        // Check if we need to transition
        //

        auto const desiredState = PopDesiredState();
        if (!!desiredState)
        {
            switch (*desiredState)
            {
                case StateType::FadingIn:
                {
                    // Consume and continue
                    break;
                }

                case StateType::FadingOut:
                {
                    LogMessage("TODOTEST:TransitionToFadingOut");
                    // Transition
                    mFadeStateStartTimestamp = GameWallClock::GetInstance().Now();
                    mCurrentState = StateType::FadingOut;

                    // Start right away
                    Update();

                    return;
                }

                case StateType::Playing:
                {
                    // Consume and continue
                    break;
                }

                case StateType::Stopped:
                {
                    // Transition
                    TransitionToStopped();

                    return;
                }
            }
        }

        //
        // Make sure music is playing
        //

        EnsureMusicIsPlaying();
    }

    void Update_FadingOut()
    {
        //
        // Check if we need to transition
        //

        auto const desiredState = PopDesiredState();
        if (!!desiredState)
        {
            switch (*desiredState)
            {
                case StateType::FadingIn:
                {
                    LogMessage("TODOTEST:TransitionToFadingIn");
                    // Transition
                    mFadeStateStartTimestamp = AdjustFadeStartTimestamp(mTimeToFadeOut, mTimeToFadeIn);
                    mCurrentState = StateType::FadingIn;

                    // Start right away
                    Update();

                    return;
                }

                case StateType::FadingOut:
                {
                    // Consume and continue
                    break;
                }

                case StateType::Playing:
                {
                    // Transition
                    TransitionToPlaying();

                    // Start right away
                    Update();

                    return;
                }

                case StateType::Stopped:
                {
                    // Transition
                    TransitionToStopped();

                    return;
                }
            }
        }

        //
        // Make sure music is playing
        //

        EnsureMusicIsPlaying();

        //
        // Update fade-out state machine
        //

        auto elapsedMillis = std::chrono::duration_cast<std::chrono::milliseconds>(
            GameWallClock::GetInstance().Elapsed(mFadeStateStartTimestamp));

        mFadeLevel = std::max(
            0.0f,
            (1.0f - static_cast<float>(elapsedMillis.count()) / static_cast<float>(mTimeToFadeOut.count())));

        InternalSetVolume();

        // Check if we're done
        if (elapsedMillis >= mTimeToFadeOut)
        {
            // Transition
            TransitionToStopped();
        }
    }

    inline void TransitionToPlaying()
    {
        LogMessage("TODOTEST:TransitionToPlaying");

        mFadeLevel = 1.0f;
        InternalSetVolume();

        mCurrentState = StateType::Playing;
    }

    inline void TransitionToStopped()
    {
        LogMessage("TODOTEST:TransitionToStopped");

        mMusic.stop();

        mCurrentState = StateType::Stopped;
    }

    inline GameWallClock::time_point AdjustFadeStartTimestamp(
        std::chrono::milliseconds oldFadeTime,
        std::chrono::milliseconds newFadeTime) const
    {
        auto const now = GameWallClock::GetInstance().Now();

        auto const elapsed = now - mFadeStateStartTimestamp;

        float const remainingFraction = std::max(0.0f,
            std::chrono::duration_cast<std::chrono::duration<float>>(oldFadeTime - elapsed)
            / std::chrono::duration_cast<std::chrono::duration<float>>(oldFadeTime));

        // Adjust start timestamp to account for remaining time
        return now - std::chrono::duration_cast<typename GameWallClock::time_point::duration>(
            remainingFraction * std::chrono::duration_cast<std::chrono::duration<float>>(newFadeTime));
    }

private:

    std::chrono::milliseconds const mTimeToFadeIn;
    std::chrono::milliseconds const mTimeToFadeOut;

    float mVolume;
    float mMasterVolume;
    float mFadeLevel;
    bool mIsMuted;

    //
    // State machine
    //

    enum class StateType
    {
        Stopped,
        FadingIn,
        Playing,
        FadingOut
    };

    // Current state
    StateType mCurrentState;

    // Desired state, queued awaiting for an Update()
    std::optional<StateType> mDesiredState;

    // Timestamp at which the current 'fade' state has started
    GameWallClock::time_point mFadeStateStartTimestamp;
};

/*
 * Background music wraps a playlist made of multiple music files, which are
 * played continuously one after each other, until the music is stopped.
 */
class BackgroundMusic : public BaseGameMusic
{
public:

    BackgroundMusic(
        float volume,
        float masterVolume,
        bool isMuted,
        std::chrono::milliseconds timeToFadeIn = std::chrono::milliseconds::zero(),
        std::chrono::milliseconds timeToFadeOut = std::chrono::milliseconds::zero())
        : BaseGameMusic(
            volume,
            masterVolume,
            isMuted,
            timeToFadeIn,
            timeToFadeOut)
        , mPlaylist()
        , mCurrentPlaylistItem(0)
    {
        mMusic.setLoop(false);
    }

    void AddToPlaylist(std::filesystem::path const & filepath)
    {
        mPlaylist.push_back(filepath);
    }

    // TODOHERE: Update() has to go and functionality logically moved up
    // TODOOLD
    ////void Update()
    ////{
    ////    // Check whether we need to start the next entry in the playlist, after
    ////    // the current entry has finished playing
    ////    if (mDesiredPlayStatus == true
    ////        && sf::SoundSource::Status::Stopped == mMusic.getStatus()
    ////        && !mPlaylist.empty())
    ////    {
    ////        LogMessage("TODOTEST: BackgroundMusic::Update: starting new music");

    ////        // Play new entry
    ////        BaseGameMusic::Play();

    ////        // Advance playlist entry
    ////        ++mCurrentPlaylistItem;
    ////        if (mCurrentPlaylistItem >= mPlaylist.size())
    ////            mCurrentPlaylistItem = 0;
    ////    }

    ////    BaseGameMusic::Update();
    ////}

protected:

    std::optional<std::filesystem::path> GetNextMusicFileToPlay() override
    {
        LogMessage("TODOTEST: GetNextMusicFileToPlay");

        if (!mPlaylist.empty())
        {
            assert(mCurrentPlaylistItem < mPlaylist.size());

            auto const musicFileToPlay = mPlaylist[mCurrentPlaylistItem];

            // Advance playlist entry
            ++mCurrentPlaylistItem;
            if (mCurrentPlaylistItem >= mPlaylist.size())
                mCurrentPlaylistItem = 0;

            return musicFileToPlay;
        }
        else
        {
            return std::nullopt;
        }
    }

private:

    std::vector<std::filesystem::path> mPlaylist;
    size_t mCurrentPlaylistItem; // The index of the playlist entry that we're playing now
};

/*
 * Game music has multiple alternatives, one of which is chosen randomly
 * when the music is started, and plays that alternative continuously,
 * until stopped.
 */
class GameMusic : public BaseGameMusic
{
public:

    GameMusic(
        float volume,
        float masterVolume,
        bool isMuted,
        std::chrono::milliseconds timeToFadeIn = std::chrono::milliseconds::zero(),
        std::chrono::milliseconds timeToFadeOut = std::chrono::milliseconds::zero())
        : BaseGameMusic(
            volume,
            masterVolume,
            isMuted,
            timeToFadeIn,
            timeToFadeOut)
        , mAlternatives()
        , mRareAlternatives()
    {
        mMusic.setLoop(true);
    }

    void AddAlternative(
		std::filesystem::path const & filepath,
		bool isRare)
    {
		if (!isRare)
			mAlternatives.push_back(filepath);
		else
			mRareAlternatives.push_back(filepath);
    }

protected:

    std::optional<std::filesystem::path> GetNextMusicFileToPlay() override
    {
        // Choose alternative
		if (mRareAlternatives.empty()
			|| GameRandomEngine::GetInstance().GenerateUniformBoolean(0.975f))
		{
            //
            // Choose normal alternative
            //

			auto alternativeToPlay = GameRandomEngine::GetInstance().Choose(mAlternatives.size());
            return mAlternatives[alternativeToPlay];
		}
        else
        {
            //
            // Choose rare alternative
            //

            auto alternativeToPlay = GameRandomEngine::GetInstance().Choose(mRareAlternatives.size());
            return mRareAlternatives[alternativeToPlay];
        }
    }

private:

    std::vector<std::filesystem::path> mAlternatives;
	std::vector<std::filesystem::path> mRareAlternatives;
};
