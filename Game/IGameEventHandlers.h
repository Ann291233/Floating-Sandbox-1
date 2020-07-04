/***************************************************************************************
* Original Author:		Gabriele Giuseppini
* Created:				2018-03-05
* Copyright:			Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#pragma once

#include "Materials.h"

#include <GameCore/GameTypes.h>

#include <optional>

/*
 * These interfaces define the methods that game event handlers must implement.
 *
 * The methods are default-implemented to facilitate implementation of handlers that
 * only care about a subset of the events.
 */

struct IRenderGameEventHandler
{
    virtual void OnEffectiveAmbientLightIntensityUpdated(float /*effectiveAmbientLightIntensity*/)
    {
        // Default-implemented
    }
};

struct ILifecycleGameEventHandler
{
    virtual void OnGameReset()
    {
        // Default-implemented
    }

    virtual void OnShipLoaded(
        unsigned int /*id*/,
        std::string const & /*name*/,
        std::optional<std::string> const & /*author*/)
    {
        // Default-implemented
    }

    virtual void OnSinkingBegin(ShipId /*shipId*/)
    {
        // Default-implemented
    }

    virtual void OnSinkingEnd(ShipId /*shipId*/)
    {
        // Default-implemented
    }

    virtual void OnShipRepaired(ShipId /*shipId*/)
    {
        // Default-implemented
    }
};

struct IStructuralGameEventHandler
{
    virtual void OnStress(
        StructuralMaterial const & /*structuralMaterial*/,
        bool /*isUnderwater*/,
        unsigned int /*size*/)
    {
        // Default-implemented
    }

    virtual void OnBreak(
        StructuralMaterial const & /*structuralMaterial*/,
        bool /*isUnderwater*/,
        unsigned int /*size*/)
    {
        // Default-implemented
    }
};

struct IWavePhenomenaGameEventHandler
{
    virtual void OnTsunami(float /*x*/)
    {
        // Default-implemented
    }

    virtual void OnTsunamiNotification(float /*x*/)
    {
        // Default-implemented
    }
};

struct ICombustionGameEventHandler
{
    virtual void OnPointCombustionBegin()
    {
        // Default-implemented
    }

    virtual void OnPointCombustionEnd()
    {
        // Default-implemented
    }

    virtual void OnCombustionSmothered()
    {
        // Default-implemented
    }

    virtual void OnCombustionExplosion(
        bool /*isUnderwater*/,
        unsigned int /*size*/)
    {
        // Default-implemented
    }
};

struct IStatisticsGameEventHandler
{
    virtual void OnFrameRateUpdated(
        float /*immediateFps*/,
        float /*averageFps*/)
    {
        // Default-implemented
    }

    virtual void OnCurrentUpdateDurationUpdated(float /*currentUpdateDuration*/)
    {
        // Default-implemented
    }
};

struct IAtmosphereGameEventHandler
{
	virtual void OnStormBegin()
	{
		// Default-implemented
	}

	virtual void OnStormEnd()
	{
		// Default-implemented
	}

	virtual void OnWindSpeedUpdated(
		float const /*zeroSpeedMagnitude*/,
		float const /*baseSpeedMagnitude*/,
		float const /*baseAndStormSpeedMagnitude*/,
		float const /*preMaxSpeedMagnitude*/,
		float const /*maxSpeedMagnitude*/,
		vec2f const& /*windSpeed*/)
	{
		// Default-implemented
	}

	virtual void OnRainUpdated(float const /*density*/)
	{
		// Default-implemented
	}

	virtual void OnThunder()
	{
		// Default-implemented
	}

	virtual void OnLightning()
	{
		// Default-implemented
	}

	virtual void OnLightningHit(StructuralMaterial const & /*structuralMaterial*/)
	{
		// Default-implemented
	}
};

struct IElectricalElementGameEventHandler
{
    virtual void OnLightFlicker(
        DurationShortLongType /*duration*/,
        bool /*isUnderwater*/,
        unsigned int /*size*/)
    {
        // Default-implemented
    }

    //
    // Announcements
    //

    virtual void OnElectricalElementAnnouncementsBegin()
    {
        // Default-implemented
    }

    virtual void OnSwitchCreated(
        ElectricalElementId /*electricalElementId*/,
        ElectricalElementInstanceIndex /*instanceIndex*/,
        SwitchType /*type*/,
        ElectricalState /*state*/,
        std::optional<ElectricalPanelElementMetadata> const & /*panelElementMetadata*/)
    {
        // Default-implemented
    }

    virtual void OnPowerProbeCreated(
        ElectricalElementId /*electricalElementId*/,
        ElectricalElementInstanceIndex /*instanceIndex*/,
        PowerProbeType /*type*/,
        ElectricalState /*state*/,
        std::optional<ElectricalPanelElementMetadata> const & /*panelElementMetadata*/)
    {
        // Default-implemented
    }

    virtual void OnEngineControllerCreated(
        ElectricalElementId /*electricalElementId*/,
        ElectricalElementInstanceIndex /*instanceIndex*/,
        std::optional<ElectricalPanelElementMetadata> const & /*panelElementMetadata*/)
    {
        // Default-implemented
    }

    virtual void OnEngineMonitorCreated(
        ElectricalElementId /*electricalElementId*/,
        ElectricalElementInstanceIndex /*instanceIndex*/,
        ElectricalMaterial const & /*electricalMaterial*/,
        float /*thrustMagnitude*/,
        float /*rpm*/,
        std::optional<ElectricalPanelElementMetadata> const & /*panelElementMetadata*/)
    {
        // Default-implemented
    }

    virtual void OnWaterPumpCreated(
        ElectricalElementId /*electricalElementId*/,
        ElectricalElementInstanceIndex /*instanceIndex*/,
        ElectricalMaterial const & /*electricalMaterial*/,
        float /*normalizedForce*/,
        std::optional<ElectricalPanelElementMetadata> const & /*panelElementMetadata*/)
    {
        // Default-implemented
    }

    virtual void OnWatertightDoorCreated(
        ElectricalElementId /*electricalElementId*/,
        ElectricalElementInstanceIndex /*instanceIndex*/,
        ElectricalMaterial const & /*electricalMaterial*/,
        bool /*isOpen*/,
        std::optional<ElectricalPanelElementMetadata> const & /*panelElementMetadata*/)
    {
        // Default-implemented
    }

    virtual void OnElectricalElementAnnouncementsEnd()
    {
        // Default-implemented
    }

    //
    // State changes
    //

    virtual void OnSwitchEnabled(
        ElectricalElementId /*electricalElementId*/,
        bool /*isEnabled*/)
    {
        // Default-implemented
    }

    virtual void OnSwitchToggled(
        ElectricalElementId /*electricalElementId*/,
        ElectricalState /*newState*/)
    {
        // Default-implemented
    }

    virtual void OnPowerProbeToggled(
        ElectricalElementId /*electricalElementId*/,
        ElectricalState /*newState*/)
    {
        // Default-implemented
    }

    virtual void OnEngineControllerEnabled(
        ElectricalElementId /*electricalElementId*/,
        bool /*isEnabled*/)
    {
        // Default-implemented
    }

    virtual void OnEngineControllerUpdated(
        ElectricalElementId /*electricalElementId*/,
        int /*telegraphValue*/)
    {
        // Default-implemented
    }

    virtual void OnEngineMonitorUpdated(
        ElectricalElementId /*electricalElementId*/,
        float /*thrustMagnitude*/,
        float /*rpm*/)
    {
        // Default-implemented
    }

    virtual void OnShipSoundUpdated(
        ElectricalElementId /*electricalElementId*/,
        ElectricalMaterial const & /*electricalMaterial*/,
        bool /*isPlaying*/,
        bool /*isUnderwater*/)
    {
        // Default-implemented
    }

    virtual void OnWaterPumpEnabled(
        ElectricalElementId /*electricalElementId*/,
        bool /*isEnabled*/)
    {
        // Default-implemented
    }

    virtual void OnWaterPumpUpdated(
        ElectricalElementId /*electricalElementId*/,
        float /*normalizedForce*/)
    {
        // Default-implemented
    }

    virtual void OnWatertightDoorEnabled(
        ElectricalElementId /*electricalElementId*/,
        bool /*isEnabled*/)
    {
        // Default-implemented
    }

    virtual void OnWatertightDoorUpdated(
        ElectricalElementId /*electricalElementId*/,
        bool /*isOpen*/)
    {
        // Default-implemented
    }
};

struct IGenericGameEventHandler
{
    virtual void OnDestroy(
        StructuralMaterial const & /*structuralMaterial*/,
        bool /*isUnderwater*/,
        unsigned int /*size*/)
    {
        // Default-implemented
    }

    virtual void OnSpringRepaired(
        StructuralMaterial const & /*structuralMaterial*/,
        bool /*isUnderwater*/,
        unsigned int /*size*/)
    {
        // Default-implemented
    }

    virtual void OnTriangleRepaired(
        StructuralMaterial const & /*structuralMaterial*/,
        bool /*isUnderwater*/,
        unsigned int /*size*/)
    {
        // Default-implemented
    }

    virtual void OnSawed(
        bool /*isMetal*/,
        unsigned int /*size*/)
    {
        // Default-implemented
    }

    virtual void OnPinToggled(
        bool /*isPinned*/,
        bool /*isUnderwater*/)
    {
        // Default-implemented
    }

    virtual void OnWaterTaken(float /*waterTaken*/)
    {
        // Default-implemented
    }

    virtual void OnWaterSplashed(float /*waterSplashed*/)
    {
        // Default-implemented
    }

    virtual void OnAirBubbleSurfaced(unsigned int /*size*/)
    {
        // Default-implemented
    }

    virtual void OnSilenceStarted()
    {
        // Default-implemented
    }

    virtual void OnSilenceLifted()
    {
        // Default-implemented
    }

    virtual void OnCustomProbe(
        std::string const & /*name*/,
        float /*value*/)
    {
        // Default-implemented
    }

    //
    // Bombs
    //

    virtual void OnBombPlaced(
        BombId /*bombId*/,
        BombType /*bombType*/,
        bool /*isUnderwater*/)
    {
        // Default-implemented
    }

    virtual void OnBombRemoved(
        BombId /*bombId*/,
        BombType /*bombType*/,
        std::optional<bool> /*isUnderwater*/)
    {
        // Default-implemented
    }

    virtual void OnBombExplosion(
        BombType /*bombType*/,
        bool /*isUnderwater*/,
        unsigned int /*size*/)
    {
        // Default-implemented
    }

    virtual void OnRCBombPing(
        bool /*isUnderwater*/,
        unsigned int /*size*/)
    {
        // Default-implemented
    }

    virtual void OnTimerBombFuse(
        BombId /*bombId*/,
        std::optional<bool> /*isFast*/)
    {
        // Default-implemented
    }

    virtual void OnTimerBombDefused(
        bool /*isUnderwater*/,
        unsigned int /*size*/)
    {
        // Default-implemented
    }

    virtual void OnAntiMatterBombContained(
        BombId /*bombId*/,
        bool /*isContained*/)
    {
        // Default-implemented
    }

    virtual void OnAntiMatterBombPreImploding()
    {
        // Default-implemented
    }

    virtual void OnAntiMatterBombImploding()
    {
        // Default-implemented
    }

    virtual void OnWatertightDoorOpened(
        bool /*isUnderwater*/,
        unsigned int /*size*/)
    {
        // Default-implemented
    }

    virtual void OnWatertightDoorClosed(
        bool /*isUnderwater*/,
        unsigned int /*size*/)
    {
        // Default-implemented
    }
};
