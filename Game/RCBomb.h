/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2018-05-23
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#pragma once

#include "Physics.h"

#include <chrono>
#include <cstdint>

using namespace std::chrono_literals;

namespace Physics
{

/*
 * Bomb specialization for bombs that explode when a remote control is triggered.
 */
class RCBomb final : public Bomb
{
public:

    RCBomb(
        BombId id,
        ElementIndex springIndex,
        World & parentWorld,
        std::shared_ptr<IGameEventHandler> gameEventHandler,
        IPhysicsHandler & physicsHandler,
        Points & shipPoints,
        Springs & shipSprings);

    virtual bool Update(
        GameWallClock::time_point currentWallClockTime,
        GameParameters const & gameParameters) override;

    virtual bool MayBeRemoved() const override
    {
        // We can always be removed
        return true;
    }

    virtual void OnBombRemoved() override
    {
        // Notify removal
        mGameEventHandler->OnBombRemoved(
            mId,
            BombType::RCBomb,
            mParentWorld.IsUnderwater(
                GetPosition()));

        // Detach ourselves, if we're attached
        DetachIfAttached();
    }

    virtual void OnNeighborhoodDisturbed() override
    {
        Detonate();
    }

    virtual void Upload(
        ShipId shipId,
        Render::RenderContext & renderContext) const override;

    void Detonate();

private:

    ///////////////////////////////////////////////////////
    // State machine
    ///////////////////////////////////////////////////////

    enum class State
    {
        // In these states we wait for remote detonation or disturbance,
        // and ping regularly at long intervals, transitioning between
        // on and off
        IdlePingOff,
        IdlePingOn,

        // In this state we are about to explode; we wait a little time
        // before exploding, and ping regularly at short intervals
        DetonationLeadIn,

        // In this state we are exploding, and increment our counter to
        // match the explosion animation until the animation is over
        Exploding,

        // This is the final state; once this state is reached, we're expired
        Expired
    };

    static constexpr auto SlowPingOffInterval = 750ms;
    static constexpr auto SlowPingOnInterval = 250ms;
    static constexpr auto FastPingInterval = 100ms;
    static constexpr auto DetonationLeadInToExplosionInterval = 1500ms;
    static constexpr auto ExplosionProgressInterval = 20ms;
    static constexpr uint8_t ExplosionStepsCount = 9;
    static constexpr int PingFramesCount = 4;

    inline void TransitionToDetonationLeadIn(GameWallClock::time_point currentWallClockTime)
    {
        mState = State::DetonationLeadIn;

        ++mPingOnStepCounter;

        mGameEventHandler->OnRCBombPing(
            mParentWorld.IsUnderwater(GetPosition()),
            1);

        // Schedule next transition
        mNextStateTransitionTimePoint = currentWallClockTime + FastPingInterval;
    }

    inline void TransitionToExploding(
        GameWallClock::time_point currentWallClockTime,
        GameParameters const & gameParameters)
    {
        mState = State::Exploding;

        assert(mExplodingStepCounter < ExplosionStepsCount);

        // Check whether we're done
        if (mExplodingStepCounter == ExplosionStepsCount - 1)
        {
            // Transition to expired
            mState = State::Expired;
        }
        else
        {
            // Invoke blast handler
            mPhysicsHandler.DoBombExplosion(
                GetPosition(),
                static_cast<float>(mExplodingStepCounter) / static_cast<float>(ExplosionStepsCount - 1),
                gameParameters);

            // Increment counter
            ++mExplodingStepCounter;

            // Schedule next transition
            mNextStateTransitionTimePoint = currentWallClockTime + ExplosionProgressInterval;
        }
    }

    State mState;

    // The next timestamp at which we'll automatically transition state
    GameWallClock::time_point mNextStateTransitionTimePoint;

    // The timestamp at which we'll explode while in detonation lead-in
    GameWallClock::time_point mExplosionTimePoint;

    // The counters for the various states. Fine to rollover!
    uint8_t mPingOnStepCounter;     // Set to one upon entering
    uint8_t mExplodingStepCounter;  // Set to zero upon entering
};

}
