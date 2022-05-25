/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2018-05-13
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "Physics.h"

#include <GameCore/GameGeometry.h>
#include <GameCore/GameRandomEngine.h>

#include <cmath>
#include <queue>

namespace Physics {

void ElectricalElements::Add(
    ElementIndex pointElementIndex,
    ElectricalElementInstanceIndex instanceIndex,
    std::optional<ElectricalPanelElementMetadata> const & panelElementMetadata,
    ElectricalMaterial const & electricalMaterial,
    Points const & points)
{
    ElementIndex const elementIndex = static_cast<ElementIndex>(mIsDeletedBuffer.GetCurrentPopulatedSize());

    mIsDeletedBuffer.emplace_back(false);
    mPointIndexBuffer.emplace_back(pointElementIndex);
    mMaterialBuffer.emplace_back(&electricalMaterial);
    mMaterialTypeBuffer.emplace_back(electricalMaterial.ElectricalType);
    mConductivityBuffer.emplace_back(electricalMaterial.ConductsElectricity);
    mMaterialHeatGeneratedBuffer.emplace_back(electricalMaterial.HeatGenerated);
    mMaterialOperatingTemperaturesBuffer.emplace_back(
        electricalMaterial.MinimumOperatingTemperature,
        electricalMaterial.MaximumOperatingTemperature);
    mMaterialLuminiscenceBuffer.emplace_back(electricalMaterial.Luminiscence);
    mMaterialLightColorBuffer.emplace_back(electricalMaterial.LightColor);
    mMaterialLightSpreadBuffer.emplace_back(electricalMaterial.LightSpread);
    mConnectedElectricalElementsBuffer.emplace_back(); // Will be populated later
    mConductingConnectedElectricalElementsBuffer.emplace_back(); // Will be populated later
    mAvailableLightBuffer.emplace_back(0.f);

    //
    // Per-type initialization
    //

    switch (electricalMaterial.ElectricalType)
    {
        case ElectricalMaterial::ElectricalElementType::Cable:
        {
            // State
            mElementStateBuffer.emplace_back(ElementState::CableState());

            break;
        }

        case ElectricalMaterial::ElectricalElementType::Engine:
        {
            // State
            mElementStateBuffer.emplace_back(
                ElementState::EngineState(
                    electricalMaterial.EnginePower * 746.0f, // HP => N*m/s (which we use as N)
                    electricalMaterial.EngineResponsiveness));

            // Indices
            mEngines.emplace_back(elementIndex);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::EngineController:
        {
            // State
            mElementStateBuffer.emplace_back(ElementState::EngineControllerState(0, false));

            // Indices
            mSinks.emplace_back(elementIndex);
            mEngineControllers.emplace_back(elementIndex);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::EngineTransmission:
        {
            // State
            mElementStateBuffer.emplace_back(ElementState::EngineTransmissionState());

            break;
        }

        case ElectricalMaterial::ElectricalElementType::Generator:
        {
            // State
            mElementStateBuffer.emplace_back(ElementState::GeneratorState(true));

            // Indices
            mSources.emplace_back(elementIndex);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::Lamp:
        {
            // State
            mElementStateBuffer.emplace_back(
                ElementState::LampState(
                    electricalMaterial.IsSelfPowered,
                    electricalMaterial.WetFailureRate));

            // Indices
            mSinks.emplace_back(elementIndex);
            mLamps.emplace_back(elementIndex);

            // Lighting

            float const lampLightSpreadMaxDistance = CalculateLampLightSpreadMaxDistance(
                electricalMaterial.LightSpread,
                mCurrentLightSpreadAdjustment);

            mLampRawDistanceCoefficientBuffer.emplace_back(
                CalculateLampRawDistanceCoefficient(
                    electricalMaterial.Luminiscence,
                    mCurrentLuminiscenceAdjustment,
                    lampLightSpreadMaxDistance));

            mLampLightSpreadMaxDistanceBuffer.emplace_back(lampLightSpreadMaxDistance);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::OtherSink:
        {
            // State
            mElementStateBuffer.emplace_back(ElementState::OtherSinkState(false));

            // Indices
            mSinks.emplace_back(elementIndex);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::PowerMonitor:
        {
            // State
            mElementStateBuffer.emplace_back(ElementState::PowerMonitorState(false));

            // Indices
            mSinks.emplace_back(elementIndex);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::ShipSound:
        {
            // State
            mElementStateBuffer.emplace_back(ElementState::ShipSoundState(electricalMaterial.IsSelfPowered, false));

            // Indices
            mSinks.emplace_back(elementIndex);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::SmokeEmitter:
        {
            // State
            mElementStateBuffer.emplace_back(ElementState::SmokeEmitterState(electricalMaterial.ParticleEmissionRate, false));

            // Indices
            mSinks.emplace_back(elementIndex);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::WaterPump:
        {
            // State
            mElementStateBuffer.emplace_back(ElementState::WaterPumpState(electricalMaterial.WaterPumpNominalForce));

            // Indices
            mSinks.emplace_back(elementIndex);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::WaterSensingSwitch:
        {
            // State
            mElementStateBuffer.emplace_back(ElementState::WaterSensingSwitchState());

            // Indices
            mAutomaticConductivityTogglingElements.emplace_back(elementIndex);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::WatertightDoor:
        {
            // State
            mElementStateBuffer.emplace_back(
                ElementState::WatertightDoorState(
                    false, // IsActive
                    !points.GetStructuralMaterial(pointElementIndex).IsHull)); // DefaultIsOpen: open <=> material open (==not hull)

            // Indices
            mSinks.emplace_back(elementIndex);

            break;
        }

        default:
        {
            // State - dummy
            mElementStateBuffer.emplace_back(ElementState::DummyState());

            break;
        }
    }

    mCurrentConnectivityVisitSequenceNumberBuffer.emplace_back();

    mInstanceInfos.emplace_back(instanceIndex, panelElementMetadata);
}


void ElectricalElements::AnnounceInstancedElements()
{
    mGameEventHandler->OnElectricalElementAnnouncementsBegin();

    for (auto elementIndex : *this)
    {
        assert(elementIndex < mInstanceInfos.size());

        switch (GetMaterialType(elementIndex))
        {
            case ElectricalMaterial::ElectricalElementType::Engine:
            {
                // Announce engine as EngineMonitor
                mGameEventHandler->OnEngineMonitorCreated(
                    ElectricalElementId(mShipId, elementIndex),
                    mInstanceInfos[elementIndex].InstanceIndex,
                    mElementStateBuffer[elementIndex].Engine.CurrentThrustMagnitude,
                    mElementStateBuffer[elementIndex].Engine.CurrentRpm,
                    *mMaterialBuffer[elementIndex],
                    mInstanceInfos[elementIndex].PanelElementMetadata);

                break;
            }

            case ElectricalMaterial::ElectricalElementType::EngineController:
            {
                mGameEventHandler->OnEngineControllerCreated(
                    ElectricalElementId(mShipId, elementIndex),
                    mInstanceInfos[elementIndex].InstanceIndex,
                    *mMaterialBuffer[elementIndex],
                    mInstanceInfos[elementIndex].PanelElementMetadata);

                break;
            }

            case ElectricalMaterial::ElectricalElementType::Generator:
            {
                // Announce generators that are instanced as power probes
                if (mInstanceInfos[elementIndex].InstanceIndex != NoneElectricalElementInstanceIndex)
                {
                    mGameEventHandler->OnPowerProbeCreated(
                        ElectricalElementId(mShipId, elementIndex),
                        mInstanceInfos[elementIndex].InstanceIndex,
                        PowerProbeType::Generator,
                        static_cast<ElectricalState>(mElementStateBuffer[elementIndex].Generator.IsProducingCurrent),
                        *mMaterialBuffer[elementIndex],
                        mInstanceInfos[elementIndex].PanelElementMetadata);
                }

                break;
            }

            case ElectricalMaterial::ElectricalElementType::InteractiveSwitch:
            {
                SwitchType switchType = SwitchType::AutomaticSwitch; // Arbitrary, for MSVC
                switch (mMaterialBuffer[elementIndex]->InteractiveSwitchType)
                {
                    case ElectricalMaterial::InteractiveSwitchElementType::Push:
                    {
                        switchType = SwitchType::InteractivePushSwitch;
                        break;
                    }

                    case ElectricalMaterial::InteractiveSwitchElementType::Toggle:
                    {
                        switchType = SwitchType::InteractiveToggleSwitch;
                        break;
                    }
                }

                mGameEventHandler->OnSwitchCreated(
                    ElectricalElementId(mShipId, elementIndex),
                    mInstanceInfos[elementIndex].InstanceIndex,
                    switchType,
                    static_cast<ElectricalState>(mConductivityBuffer[elementIndex].ConductsElectricity),
                    *mMaterialBuffer[elementIndex],
                    mInstanceInfos[elementIndex].PanelElementMetadata);

                break;
            }

            case ElectricalMaterial::ElectricalElementType::PowerMonitor:
            {
                mGameEventHandler->OnPowerProbeCreated(
                    ElectricalElementId(mShipId, elementIndex),
                    mInstanceInfos[elementIndex].InstanceIndex,
                    PowerProbeType::PowerMonitor,
                    static_cast<ElectricalState>(mElementStateBuffer[elementIndex].PowerMonitor.IsPowered),
                    *mMaterialBuffer[elementIndex],
                    mInstanceInfos[elementIndex].PanelElementMetadata);

                break;
            }

            case ElectricalMaterial::ElectricalElementType::ShipSound:
            {
                // Ships sounds announce themselves as switches
                mGameEventHandler->OnSwitchCreated(
                    ElectricalElementId(mShipId, elementIndex),
                    mInstanceInfos[elementIndex].InstanceIndex,
                    SwitchType::ShipSoundSwitch,
                    static_cast<ElectricalState>(mConductivityBuffer[elementIndex].ConductsElectricity),
                    *mMaterialBuffer[elementIndex],
                    mInstanceInfos[elementIndex].PanelElementMetadata);

                break;
            }

            case ElectricalMaterial::ElectricalElementType::WaterPump:
            {
                mGameEventHandler->OnWaterPumpCreated(
                    ElectricalElementId(mShipId, elementIndex),
                    mInstanceInfos[elementIndex].InstanceIndex,
                    mElementStateBuffer[elementIndex].WaterPump.CurrentNormalizedForce,
                    * mMaterialBuffer[elementIndex],
                    mInstanceInfos[elementIndex].PanelElementMetadata);

                break;
            }

            case ElectricalMaterial::ElectricalElementType::WaterSensingSwitch:
            {
                // Announce water-sensing switches that are instanced as switches
                if (mInstanceInfos[elementIndex].InstanceIndex != NoneElectricalElementInstanceIndex)
                {
                    mGameEventHandler->OnSwitchCreated(
                        ElectricalElementId(mShipId, elementIndex),
                        mInstanceInfos[elementIndex].InstanceIndex,
                        SwitchType::AutomaticSwitch,
                        static_cast<ElectricalState>(mConductivityBuffer[elementIndex].ConductsElectricity),
                        *mMaterialBuffer[elementIndex],
                        mInstanceInfos[elementIndex].PanelElementMetadata);
                }

                break;
            }

            case ElectricalMaterial::ElectricalElementType::WatertightDoor:
            {
                assert(!mElementStateBuffer[elementIndex].WatertightDoor.IsActivated);

                mGameEventHandler->OnWatertightDoorCreated(
                    ElectricalElementId(mShipId, elementIndex),
                    mInstanceInfos[elementIndex].InstanceIndex,
                    mElementStateBuffer[elementIndex].WatertightDoor.DefaultIsOpen,
                    *mMaterialBuffer[elementIndex],
                    mInstanceInfos[elementIndex].PanelElementMetadata);

                break;
            }

            case ElectricalMaterial::ElectricalElementType::Cable:
            case ElectricalMaterial::ElectricalElementType::EngineTransmission:
            case ElectricalMaterial::ElectricalElementType::Lamp:
            case ElectricalMaterial::ElectricalElementType::OtherSink:
            case ElectricalMaterial::ElectricalElementType::SmokeEmitter:
            {
                // Nothing to announce for these
                break;
            }
        }
    }

    mGameEventHandler->OnElectricalElementAnnouncementsEnd();
}

void ElectricalElements::HighlightElectricalElement(
    ElementIndex elementIndex,
    Points & points)
{
    rgbColor constexpr EngineOnHighlightColor = rgbColor(0xfc, 0xff, 0xa6);
    rgbColor constexpr EngineOffHighlightColor = rgbColor(0xc4, 0xb7, 0x02);

    rgbColor constexpr PowerOnHighlightColor = rgbColor(0x02, 0x5e, 0x1e);
    rgbColor constexpr PowerOffHighlightColor = rgbColor(0x91, 0x00, 0x00);

    rgbColor constexpr SoundOnHighlightColor = rgbColor(0xe0, 0xe0, 0xe0);
    rgbColor constexpr SoundOffHighlightColor = rgbColor(0x75, 0x75, 0x75);

    rgbColor constexpr SwitchOnHighlightColor = rgbColor(0x00, 0xab, 0x00);
    rgbColor constexpr SwitchOffHighlightColor = rgbColor(0xde, 0x00, 0x00);

    rgbColor constexpr WaterPumpOnHighlightColor = rgbColor(0x47, 0x60, 0xff);
    rgbColor constexpr WaterPumpOffHighlightColor = rgbColor(0x1b, 0x28, 0x80);

    rgbColor constexpr WatertightDoorOpenHighlightColor = rgbColor(0x9e, 0xff, 0xf2);
    rgbColor constexpr WatertightDoorClosedHighlightColor = rgbColor(0x80, 0xb0, 0xaa);

    // Switch state as appropriate
    switch (GetMaterialType(elementIndex))
    {
        case ElectricalMaterial::ElectricalElementType::Engine:
        {
            points.StartElectricalElementHighlight(
                GetPointIndex(elementIndex),
                mElementStateBuffer[elementIndex].Engine.LastHighlightedRpm != 0.0f? EngineOnHighlightColor : EngineOffHighlightColor,
                GameWallClock::GetInstance().NowAsFloat());

            break;
        }

        case ElectricalMaterial::ElectricalElementType::Generator:
        {
            points.StartElectricalElementHighlight(
                GetPointIndex(elementIndex),
                mElementStateBuffer[elementIndex].Generator.IsProducingCurrent ? PowerOnHighlightColor : PowerOffHighlightColor,
                GameWallClock::GetInstance().NowAsFloat());

            break;
        }

        case ElectricalMaterial::ElectricalElementType::InteractiveSwitch:
        case ElectricalMaterial::ElectricalElementType::WaterSensingSwitch:
        {
            points.StartElectricalElementHighlight(
                GetPointIndex(elementIndex),
                mConductivityBuffer[elementIndex].ConductsElectricity ? SwitchOnHighlightColor : SwitchOffHighlightColor,
                GameWallClock::GetInstance().NowAsFloat());

            break;
        }

        case ElectricalMaterial::ElectricalElementType::PowerMonitor:
        {
            points.StartElectricalElementHighlight(
                GetPointIndex(elementIndex),
                mElementStateBuffer[elementIndex].PowerMonitor.IsPowered ? PowerOnHighlightColor : PowerOffHighlightColor,
                GameWallClock::GetInstance().NowAsFloat());

            break;
        }

        case ElectricalMaterial::ElectricalElementType::ShipSound:
        {
            points.StartElectricalElementHighlight(
                GetPointIndex(elementIndex),
                mElementStateBuffer[elementIndex].ShipSound.IsPlaying  ? SoundOnHighlightColor : SoundOffHighlightColor,
                GameWallClock::GetInstance().NowAsFloat());

            break;
        }

        case ElectricalMaterial::ElectricalElementType::WaterPump:
        {
            points.StartElectricalElementHighlight(
                GetPointIndex(elementIndex),
                mElementStateBuffer[elementIndex].WaterPump.TargetNormalizedForce != 0.0f ? WaterPumpOnHighlightColor : WaterPumpOffHighlightColor,
                GameWallClock::GetInstance().NowAsFloat());

            break;
        }

        case ElectricalMaterial::ElectricalElementType::WatertightDoor:
        {
            points.StartElectricalElementHighlight(
                GetPointIndex(elementIndex),
                mElementStateBuffer[elementIndex].WatertightDoor.IsOpen() ? WatertightDoorOpenHighlightColor : WatertightDoorClosedHighlightColor,
                GameWallClock::GetInstance().NowAsFloat());

            break;
        }

        default:
        {
            // Shouldn't be invoked for non-highlightable elements
            assert(false);
        }
    }
}

void ElectricalElements::Query(ElementIndex elementIndex) const
{
    LogMessage("ElectricalElementIndex: ", elementIndex, (nullptr != mMaterialBuffer[elementIndex]) ? (" (" + mMaterialBuffer[elementIndex]->Name) + ")" : "");
    if (mMaterialTypeBuffer[elementIndex] == ElectricalMaterial::ElectricalElementType::Engine)
    {
        LogMessage("EngineGroup=", mElementStateBuffer[elementIndex].Engine.EngineGroup);
    }
    else if (mMaterialTypeBuffer[elementIndex] == ElectricalMaterial::ElectricalElementType::EngineController)
    {
        LogMessage("EngineGroup=", mElementStateBuffer[elementIndex].EngineController.EngineGroup);
    }
}

void ElectricalElements::SetSwitchState(
    ElectricalElementId electricalElementId,
    ElectricalState switchState,
    Points & points,
    GameParameters const & gameParameters)
{
    assert(electricalElementId.GetShipId() == mShipId);

    InternalSetSwitchState(
        electricalElementId.GetLocalObjectId(),
        switchState,
        points,
        gameParameters);
}

void ElectricalElements::SetEngineControllerState(
    ElectricalElementId electricalElementId,
    int telegraphValue,
    GameParameters const & /*gameParameters*/)
{
    assert(electricalElementId.GetShipId() == mShipId);
    auto const elementIndex = electricalElementId.GetLocalObjectId();

    assert(GetMaterialType(elementIndex) == ElectricalMaterial::ElectricalElementType::EngineController);
    auto & state = mElementStateBuffer[elementIndex].EngineController;

    assert(telegraphValue >= -static_cast<int>(GameParameters::EngineTelegraphDegreesOfFreedom / 2)
        && telegraphValue <= static_cast<int>(GameParameters::EngineTelegraphDegreesOfFreedom / 2));

    // Make sure it's a state change
    if (telegraphValue != state.CurrentTelegraphValue)
    {
        // Change current value
        state.CurrentTelegraphValue = telegraphValue;

        // Notify
        mGameEventHandler->OnEngineControllerUpdated(
            electricalElementId,
            telegraphValue);
    }
}

void ElectricalElements::Destroy(ElementIndex electricalElementIndex)
{
    // Connectivity is taken care by ship destroy handler, as usual

    assert(!IsDeleted(electricalElementIndex));

    // Zero out our light
    mAvailableLightBuffer[electricalElementIndex] = 0.0f;

    // Switch state as appropriate
    switch (GetMaterialType(electricalElementIndex))
    {
        case ElectricalMaterial::ElectricalElementType::Engine:
        {
            // Publish state change, if necessary
            if (mElementStateBuffer[electricalElementIndex].Engine.LastPublishedRpm != 0.0f
                || mElementStateBuffer[electricalElementIndex].Engine.LastPublishedThrustMagnitude != 0.0f)
            {
                mGameEventHandler->OnEngineMonitorUpdated(
                    ElectricalElementId(mShipId, electricalElementIndex),
                    0.0f,
                    0.0f);
            }

            break;
        }

        case ElectricalMaterial::ElectricalElementType::EngineController:
        {
            mElementStateBuffer[electricalElementIndex].EngineController.IsPowered = false;

            // Publish disable
            mGameEventHandler->OnEngineControllerEnabled(
                ElectricalElementId(
                    mShipId,
                    electricalElementIndex),
                false);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::Generator:
        {
            // See if state change
            if (mElementStateBuffer[electricalElementIndex].Generator.IsProducingCurrent)
            {
                mElementStateBuffer[electricalElementIndex].Generator.IsProducingCurrent = false;

                // See whether we need to publish a power probe change
                if (mInstanceInfos[electricalElementIndex].InstanceIndex != NoneElectricalElementInstanceIndex)
                {
                    mGameEventHandler->OnPowerProbeToggled(
                        ElectricalElementId(mShipId, electricalElementIndex),
                        ElectricalState::Off);
                }
            }

            break;
        }

        case ElectricalMaterial::ElectricalElementType::InteractiveSwitch:
        {
            // Publish disable
            mGameEventHandler->OnSwitchEnabled(
                ElectricalElementId(
                    mShipId,
                    electricalElementIndex),
                false);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::PowerMonitor:
        {
            // Publish state change, if necessary
            if (mElementStateBuffer[electricalElementIndex].PowerMonitor.IsPowered)
            {
                mElementStateBuffer[electricalElementIndex].PowerMonitor.IsPowered = false;

                mGameEventHandler->OnPowerProbeToggled(
                    ElectricalElementId(mShipId, electricalElementIndex),
                    ElectricalState::Off);
            }

            break;
        }

        case ElectricalMaterial::ElectricalElementType::ShipSound:
        {
            // Publish state change, if necessary
            if (mElementStateBuffer[electricalElementIndex].ShipSound.IsPlaying)
            {
                mElementStateBuffer[electricalElementIndex].ShipSound.IsPlaying = false;

                // Publish state change
                mGameEventHandler->OnShipSoundUpdated(
                    ElectricalElementId(mShipId, electricalElementIndex),
                    *mMaterialBuffer[electricalElementIndex],
                    false,
                    false); // Irrelevant
            }

            // Publish disable
            mGameEventHandler->OnSwitchEnabled(ElectricalElementId(mShipId, electricalElementIndex), false);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::WaterPump:
        {
            mElementStateBuffer[electricalElementIndex].WaterPump.TargetNormalizedForce = 0.0f;

            // At UpdateSinks() we'll smooth towards new TargetNormalizedForce and eventually
            // publish an electrical element state update

            // Publish disable
            mGameEventHandler->OnWaterPumpEnabled(
                ElectricalElementId(
                    mShipId,
                    electricalElementIndex),
                false);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::WaterSensingSwitch:
        {
            // See whether we need to publish a disable
            if (mInstanceInfos[electricalElementIndex].InstanceIndex != NoneElectricalElementInstanceIndex)
            {
                // Publish disable
                mGameEventHandler->OnSwitchEnabled(
                    ElectricalElementId(
                        mShipId,
                        electricalElementIndex),
                    false);
            }

            break;
        }

        case ElectricalMaterial::ElectricalElementType::WatertightDoor:
        {
            auto & watertightDoorState = mElementStateBuffer[electricalElementIndex].WatertightDoor;

            // Publish state change, if necessary
            if (watertightDoorState.IsActivated)
            {
                watertightDoorState.IsActivated = false;

                // Propagate structural effect
                assert(nullptr != mShipPhysicsHandler);
                mShipPhysicsHandler->HandleWatertightDoorUpdated(GetPointIndex(electricalElementIndex), watertightDoorState.IsOpen());

                // Publish state change
                mGameEventHandler->OnWatertightDoorUpdated(
                    ElectricalElementId(mShipId, electricalElementIndex),
                    watertightDoorState.IsOpen());
            }

            // Publish disable
            mGameEventHandler->OnWatertightDoorEnabled(ElectricalElementId(mShipId, electricalElementIndex), false);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::Cable:
        case ElectricalMaterial::ElectricalElementType::EngineTransmission:
        case ElectricalMaterial::ElectricalElementType::Lamp:
        case ElectricalMaterial::ElectricalElementType::OtherSink:
        case ElectricalMaterial::ElectricalElementType::SmokeEmitter:
        {
            break;
        }
    }

    // Invoke destroy handler
    assert(nullptr != mShipPhysicsHandler);
    mShipPhysicsHandler->HandleElectricalElementDestroy(electricalElementIndex);

    // Remember that connectivity structure has changed during this step
    mHasConnectivityStructureChangedInCurrentStep = true;

    // Remember that power has been severed during this step
    mHasPowerBeenSeveredInCurrentStep = true;

    // Flag ourselves as deleted
    mIsDeletedBuffer[electricalElementIndex] = true;
}

void ElectricalElements::Restore(ElementIndex electricalElementIndex)
{
    // Connectivity is taken care by ship destroy handler, as usual

    assert(IsDeleted(electricalElementIndex));

    // Clear the deleted flag
    mIsDeletedBuffer[electricalElementIndex] = false;

    // Switch state as appropriate
    switch (GetMaterialType(electricalElementIndex))
    {
        case ElectricalMaterial::ElectricalElementType::Engine:
        {
            mElementStateBuffer[electricalElementIndex].Engine.Reset();

            break;
        }

        case ElectricalMaterial::ElectricalElementType::EngineController:
        {
            // Notify enabling
            mGameEventHandler->OnEngineControllerEnabled(ElectricalElementId(mShipId, electricalElementIndex), true);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::Generator:
        {
            // Nothing to do: at the next UpdateSources() that makes this generator work, the generator will start
            // producing current again and it will announce it

            assert(!mElementStateBuffer[electricalElementIndex].Generator.IsProducingCurrent);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::Lamp:
        {
            mElementStateBuffer[electricalElementIndex].Lamp.Reset();

            break;
        }

        case ElectricalMaterial::ElectricalElementType::InteractiveSwitch:
        {
            // Notify enabling
            mGameEventHandler->OnSwitchEnabled(ElectricalElementId(mShipId, electricalElementIndex), true);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::PowerMonitor:
        {
            // Nothing to do: at the next UpdateSources() that makes this monitor work, there will be a state change
            // and the monitor will announce it

            assert(!mElementStateBuffer[electricalElementIndex].PowerMonitor.IsPowered);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::ShipSound:
        {
            // Notify enabling
            mGameEventHandler->OnSwitchEnabled(ElectricalElementId(mShipId, electricalElementIndex), true);

            // Nothing else to do: at the next UpdateSinks() that makes this sound work, there will be a state change

            assert(!mElementStateBuffer[electricalElementIndex].ShipSound.IsPlaying);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::WaterPump:
        {
            // Notify enabling
            mGameEventHandler->OnWaterPumpEnabled(ElectricalElementId(mShipId, electricalElementIndex), true);

            // Nothing else to do: at the next UpdateSinks() that makes this pump work, there will be a state change

            assert(mElementStateBuffer[electricalElementIndex].WaterPump.TargetNormalizedForce == 0.0f);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::WaterSensingSwitch:
        {
            // See whether we need to publish an enable
            if (mInstanceInfos[electricalElementIndex].InstanceIndex != NoneElectricalElementInstanceIndex)
            {
                // Publish disable
                mGameEventHandler->OnSwitchEnabled(
                    ElectricalElementId(
                        mShipId,
                        electricalElementIndex),
                    true);
            }

            break;
        }

        case ElectricalMaterial::ElectricalElementType::WatertightDoor:
        {
            // Notify enabling
            mGameEventHandler->OnWatertightDoorEnabled(ElectricalElementId(mShipId, electricalElementIndex), true);

            // Nothing else to do: the last status we've announced is for !Activated (we did at Destroy);
            // at the next UpdateSinks() that makes this door work, there will be a state change

            assert(!mElementStateBuffer[electricalElementIndex].WatertightDoor.IsActivated);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::Cable:
        case ElectricalMaterial::ElectricalElementType::EngineTransmission:
        case ElectricalMaterial::ElectricalElementType::OtherSink:
        case ElectricalMaterial::ElectricalElementType::SmokeEmitter:
        {
            // These types do not have a state machine that needs to be reset
            break;
        }
    }

    // Invoke restore handler
    assert(nullptr != mShipPhysicsHandler);
    mShipPhysicsHandler->HandleElectricalElementRestore(electricalElementIndex);

    // Remember that connectivity structure has changed during this step
    mHasConnectivityStructureChangedInCurrentStep = true;
}

void ElectricalElements::OnElectricSpark(
    ElementIndex electricalElementIndex,
    float currentSimulationTime)
{
    // Disable as appropriate
    switch (GetMaterialType(electricalElementIndex))
    {
        case ElectricalMaterial::ElectricalElementType::Engine:
        {
            mElementStateBuffer[electricalElementIndex].Engine.SuperElectrificationSimulationTimestampEnd =
                currentSimulationTime + GameRandomEngine::GetInstance().GenerateUniformReal(7.0f, 15.0f);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::Generator:
        {
            mElementStateBuffer[electricalElementIndex].Generator.DisabledSimulationTimestampEnd =
                currentSimulationTime + GameRandomEngine::GetInstance().GenerateUniformReal(15.0f, 30.0f);

            break;
        }

        case ElectricalMaterial::ElectricalElementType::Lamp:
        {
            mElementStateBuffer[electricalElementIndex].Lamp.DisabledSimulationTimestampEnd =
                currentSimulationTime + GameRandomEngine::GetInstance().GenerateUniformReal(4.0f, 8.0f);

            mHasPowerBeenSeveredInCurrentStep = true;

            break;
        }

        default:
        {
            // We don't disable anything else, we rely on generators going off
            break;
        }
    }
}

void ElectricalElements::UpdateForGameParameters(GameParameters const & gameParameters)
{
    //
    // Recalculate lamp coefficients, if needed
    //

    if (gameParameters.LightSpreadAdjustment != mCurrentLightSpreadAdjustment
        || gameParameters.LuminiscenceAdjustment != mCurrentLuminiscenceAdjustment)
    {
        for (size_t l = 0; l < mLamps.size(); ++l)
        {
            auto const lampElementIndex = mLamps[l];

            float const lampLightSpreadMaxDistance = CalculateLampLightSpreadMaxDistance(
                mMaterialLightSpreadBuffer[lampElementIndex],
                gameParameters.LightSpreadAdjustment);

            mLampRawDistanceCoefficientBuffer[l] = CalculateLampRawDistanceCoefficient(
                mMaterialLuminiscenceBuffer[lampElementIndex],
                gameParameters.LuminiscenceAdjustment,
                lampLightSpreadMaxDistance);

            mLampLightSpreadMaxDistanceBuffer[l] = lampLightSpreadMaxDistance;
        }

        // Remember new parameters
        mCurrentLightSpreadAdjustment = gameParameters.LightSpreadAdjustment;
        mCurrentLuminiscenceAdjustment = gameParameters.LuminiscenceAdjustment;
    }
}

void ElectricalElements::Update(
    GameWallClock::time_point currentWallClockTime,
    float currentSimulationTime,
    SequenceNumber newConnectivityVisitSequenceNumber,
    Points & points,
    Springs const & springs,
    Storm::Parameters const & stormParameters,
    GameParameters const & gameParameters)
{
    //
    // 1. Update engine conductivity
    //

    if (mHasConnectivityStructureChangedInCurrentStep)
    {
        UpdateEngineConductivity(
            newConnectivityVisitSequenceNumber,
            points,
            springs);
    }

    //
    // 2. Update automatic conductivity toggles (e.g. water-sensing switches)
    //

    UpdateAutomaticConductivityToggles(
        currentSimulationTime,
        points,
        gameParameters);

    //
    // 3. Update sources and connectivity
    //
    // We do this regardless of dirty elements, as elements might have changed their state autonomously
    // (e.g. generators might have become wet, switches might have been toggled, etc.)
    //

    UpdateSourcesAndPropagation(
        currentSimulationTime,
        newConnectivityVisitSequenceNumber,
        points,
        gameParameters);

    //
    // 4. Update sinks
    //
    // - Applies static forces, will be integrated at next loop
    //

    UpdateSinks(
        currentWallClockTime,
        currentSimulationTime,
        newConnectivityVisitSequenceNumber,
        points,
        stormParameters,
        gameParameters);

    mHasConnectivityStructureChangedInCurrentStep = false;
}

void ElectricalElements::AddFactoryConnectedElectricalElement(
    ElementIndex electricalElementIndex,
    ElementIndex connectedElectricalElementIndex)
{
    // Add element
    AddConnectedElectricalElement(
        electricalElementIndex,
        connectedElectricalElementIndex);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void ElectricalElements::InternalSetSwitchState(
    ElementIndex elementIndex,
    ElectricalState switchState,
    Points & points,
    GameParameters const & gameParameters)
{
    // Make sure it's a state change
    if (static_cast<bool>(switchState) != mConductivityBuffer[elementIndex].ConductsElectricity)
    {
        // Update conductivity graph (circuit)
        InternalChangeConductivity(elementIndex, static_cast<bool>(switchState));

        // Notify switch toggled
        if (mInstanceInfos[elementIndex].InstanceIndex != NoneElectricalElementInstanceIndex)
        {
            mGameEventHandler->OnSwitchToggled(
                ElectricalElementId(mShipId, elementIndex),
                switchState);
        }

        // Show notifications - for some types only
        if (gameParameters.DoShowElectricalNotifications
            && (mMaterialTypeBuffer[elementIndex] == ElectricalMaterial::ElectricalElementType::InteractiveSwitch
                || mMaterialTypeBuffer[elementIndex] == ElectricalMaterial::ElectricalElementType::WaterSensingSwitch))
        {
            HighlightElectricalElement(elementIndex, points);
        }
    }
}

void ElectricalElements::InternalChangeConductivity(
    ElementIndex elementIndex,
    bool value)
{
    // Update conductive connectivity
    if (mConductivityBuffer[elementIndex].ConductsElectricity == false && value == true)
    {
        // OFF->ON

        // For each electrical element connected to this one: if both elements conduct electricity,
        // conduct-connect elements to each other
        for (auto const & otherElementIndex : mConnectedElectricalElementsBuffer[elementIndex])
        {
            assert(!mConductingConnectedElectricalElementsBuffer[elementIndex].contains(otherElementIndex));
            assert(!mConductingConnectedElectricalElementsBuffer[otherElementIndex].contains(elementIndex));
            if (mConductivityBuffer[otherElementIndex].ConductsElectricity)
            {
                mConductingConnectedElectricalElementsBuffer[elementIndex].push_back(otherElementIndex);
                mConductingConnectedElectricalElementsBuffer[otherElementIndex].push_back(elementIndex);
            }
        }
    }
    else if (mConductivityBuffer[elementIndex].ConductsElectricity == true && value == false)
    {
        // ON->OFF

        // For each electrical element connected to this one: if the other element conducts electricity,
        // sever conduct-connection to each other
        for (auto const & otherElementIndex : mConnectedElectricalElementsBuffer[elementIndex])
        {
            if (mConductivityBuffer[otherElementIndex].ConductsElectricity)
            {
                assert(mConductingConnectedElectricalElementsBuffer[elementIndex].contains(otherElementIndex));
                assert(mConductingConnectedElectricalElementsBuffer[otherElementIndex].contains(elementIndex));

                mConductingConnectedElectricalElementsBuffer[elementIndex].erase_first(otherElementIndex);
                mConductingConnectedElectricalElementsBuffer[otherElementIndex].erase_first(elementIndex);
            }
            else
            {
                assert(!mConductingConnectedElectricalElementsBuffer[elementIndex].contains(otherElementIndex));
                assert(!mConductingConnectedElectricalElementsBuffer[otherElementIndex].contains(elementIndex));
            }
        }
    }

    // Change current value
    mConductivityBuffer[elementIndex].ConductsElectricity = value;
}

void ElectricalElements::UpdateEngineConductivity(
    SequenceNumber newConnectivityVisitSequenceNumber,
    Points const & points,
    Springs const & springs)
{
    //
    // Starting from all engine controller, we follow engine-transmitting elements
    // and create "engine groups" (connected components), each of which is
    // assigned an Engine Group ID.
    //
    // This graph visit could leave out disconnected Engine's, hence
    // we initialize all Engines' group ID's to the "None" group ID.
    //

    // Clear out engines - set their engine groups to group zero
    for (ElementIndex const engineElementIndex : mEngines)
    {
        mElementStateBuffer[engineElementIndex].Engine.EngineGroup = 0;
    }

    // Visit non-deleted engine controllers
    std::queue<ElementIndex> elementsToVisit;
    EngineGroupIndex nextEngineGroupIndex = 1; // "Zero" group stays untouched
    for (auto const engineControllerElementIndex : mEngineControllers)
    {
        if (!IsDeleted(engineControllerElementIndex))
        {
            // Check whether we've already visited this controller
            if (mElementStateBuffer[engineControllerElementIndex].EngineController.EngineConnectivityVisitSequenceNumber != newConnectivityVisitSequenceNumber)
            {
                //
                // Build engine group flooding graph from this engine controller
                //

                EngineGroupIndex const engineGroupIndex = nextEngineGroupIndex++;

                // Visit controller
                mElementStateBuffer[engineControllerElementIndex].EngineController.EngineGroup = engineGroupIndex;
                mElementStateBuffer[engineControllerElementIndex].EngineController.EngineConnectivityVisitSequenceNumber = newConnectivityVisitSequenceNumber;

                // Add to queue
                assert(elementsToVisit.empty());
                elementsToVisit.push(engineControllerElementIndex);
                
                while (!elementsToVisit.empty())
                {
                    auto const e = elementsToVisit.front();
                    elementsToVisit.pop();

                    // Already marked as visited

                    for (auto const ce : mConnectedElectricalElementsBuffer[e])
                    {
                        assert(!IsDeleted(ce));

                        switch (mMaterialTypeBuffer[ce])
                        {
                            case ElectricalMaterial::ElectricalElementType::Engine:
                            {
                                // Make sure not visited already
                                ElementState::EngineState & engineState = mElementStateBuffer[ce].Engine;
                                if (engineState.EngineConnectivityVisitSequenceNumber != newConnectivityVisitSequenceNumber)
                                {
                                    assert(engineState.EngineGroup == 0);

                                    // Visit element
                                    engineState.EngineGroup = engineGroupIndex;
                                    engineState.EngineConnectivityVisitSequenceNumber = newConnectivityVisitSequenceNumber;

                                    // Store reference point - we take use arbitrarily this point (e) as it's "incoming" to the engine,
                                    // and we know for a fact that it's not deleted
                                    {
                                        ElementIndex const enginePointIndex = GetPointIndex(ce);
                                        ElementIndex const referencePointIndex = GetPointIndex(e);

                                        // Find the spring connecting this engine and the incoming point
                                        ElementIndex springIndex = NoneElementIndex;
                                        for (auto const & cs : points.GetConnectedSprings(enginePointIndex).ConnectedSprings)
                                        {
                                            if (cs.OtherEndpointIndex == referencePointIndex)
                                            {
                                                springIndex = cs.SpringIndex;
                                                break;
                                            }
                                        }

                                        assert(springIndex != NoneElementIndex);

                                        // Get the octant of the e->ref_point spring wrt ref_point
                                        Octant const incomingOctant = springs.GetFactoryEndpointOctant(springIndex, referencePointIndex);

                                        // Calculate angle : CW angle between engine direction and engine->reference_point vector
                                        float engineCWAngle =
                                            (2.0f * Pi<float> - mMaterialBuffer[ce]->EngineCCWDirection)
                                            - OctantToCWAngle(OppositeOctant(incomingOctant));

                                        // Normalize
                                        if (engineCWAngle < 0.0f)
                                            engineCWAngle += 2.0f * Pi<float>;

                                        // Store in engine state
                                        engineState.ReferencePointIndex = referencePointIndex;
                                        engineState.ReferencePointCWAngleCos = std::cos(engineCWAngle);
                                        engineState.ReferencePointCWAngleSin = std::sin(engineCWAngle);
                                    }

                                    // Add to queue
                                    elementsToVisit.push(ce);
                                }

                                break;
                            }

                            case ElectricalMaterial::ElectricalElementType::EngineController:
                            {
                                // Make sure not visited already
                                if (mElementStateBuffer[ce].EngineController.EngineConnectivityVisitSequenceNumber != newConnectivityVisitSequenceNumber)
                                {
                                    // Visit element
                                    mElementStateBuffer[ce].EngineController.EngineGroup = engineGroupIndex;
                                    mElementStateBuffer[ce].EngineController.EngineConnectivityVisitSequenceNumber = newConnectivityVisitSequenceNumber;

                                    // Add to queue
                                    elementsToVisit.push(ce);
                                }

                                break;
                            }

                            case ElectricalMaterial::ElectricalElementType::EngineTransmission:
                            {
                                // Make sure not visited already
                                if (mElementStateBuffer[ce].EngineTransmission.EngineConnectivityVisitSequenceNumber != newConnectivityVisitSequenceNumber)
                                {
                                    // Visit element
                                    mElementStateBuffer[ce].EngineTransmission.EngineConnectivityVisitSequenceNumber = newConnectivityVisitSequenceNumber;

                                    // Add to queue
                                    elementsToVisit.push(ce);
                                }

                                break;
                            }

                            default:
                            {
                                // Nothing to do
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    //
    // Now we know how many engine groups we have - resize buffer for their state
    //

    size_t const engineGroupCount = static_cast<size_t>(nextEngineGroupIndex);
    
    mEngineGroupStates.resize(engineGroupCount);
}

void ElectricalElements::UpdateAutomaticConductivityToggles(
    float currentSimulationTime,
    Points & points,
    GameParameters const & gameParameters)
{
    //
    // Visit all non-deleted elements that change their conductivity automatically,
    // and eventually change their conductivity
    //

    for (auto const elementIndex : mAutomaticConductivityTogglingElements)
    {
        // Do not visit deleted elements
        if (!IsDeleted(elementIndex))
        {
            switch (GetMaterialType(elementIndex))
            {
            case ElectricalMaterial::ElectricalElementType::WaterSensingSwitch:
            {
                auto & waterSensingSwitchState = mElementStateBuffer[elementIndex].WaterSensingSwitch;

                // No transitions if in grace period
                if (currentSimulationTime >= waterSensingSwitchState.GracePeriodEndSimulationTime)
                {
                    // When higher than watermark: conductivity state toggles to opposite than material's
                    // When lower than watermark: conductivity state toggles to same as material's

                    float constexpr WaterLowWatermark = 0.05f;
                    float constexpr WaterHighWatermark = 0.45f;

                    float constexpr GracePeriodInterval = 3.0f;

                    if (mConductivityBuffer[elementIndex].ConductsElectricity == mConductivityBuffer[elementIndex].MaterialConductsElectricity
                        && points.GetWater(GetPointIndex(elementIndex)) >= WaterHighWatermark)
                    {
                        // Toggle to opposite of material
                        InternalSetSwitchState(
                            elementIndex,
                            static_cast<ElectricalState>(!mConductivityBuffer[elementIndex].MaterialConductsElectricity),
                            points,
                            gameParameters);

                        // Start grace period
                        waterSensingSwitchState.GracePeriodEndSimulationTime = currentSimulationTime + GracePeriodInterval;
                    }
                    else if (mConductivityBuffer[elementIndex].ConductsElectricity != mConductivityBuffer[elementIndex].MaterialConductsElectricity
                        && points.GetWater(GetPointIndex(elementIndex)) <= WaterLowWatermark)
                    {
                        // Toggle to material's
                        InternalSetSwitchState(
                            elementIndex,
                            static_cast<ElectricalState>(mConductivityBuffer[elementIndex].MaterialConductsElectricity),
                            points,
                            gameParameters);

                        // Start grace period
                        waterSensingSwitchState.GracePeriodEndSimulationTime = currentSimulationTime + GracePeriodInterval;
                    }
                }

                break;
            }

            default:
            {
                // Shouldn't be here - all automatically-toggling elements should have been handled
                assert(false);
            }
            }
        }
    }
}

void ElectricalElements::UpdateSourcesAndPropagation(
    float currentSimulationTime,
    SequenceNumber newConnectivityVisitSequenceNumber,
    Points & points,
    GameParameters const & gameParameters)
{
    //
    // Visit electrical graph starting from sources, and propagate connectivity state
    // by means of visit sequence number
    //

    std::queue<ElementIndex> electricalElementsToVisit;

    for (auto const sourceElementIndex : mSources)
    {
        // Do not visit deleted sources
        if (!IsDeleted(sourceElementIndex))
        {
            //
            // Check pre-conditions that need to be satisfied before visiting the connectivity graph
            //

            auto const sourcePointIndex = GetPointIndex(sourceElementIndex);

            bool preconditionsSatisfied = false;

            switch (GetMaterialType(sourceElementIndex))
            {
                case ElectricalMaterial::ElectricalElementType::Generator:
                {
                    //
                    // Preconditions to produce current:
                    // - Not too wet
                    // - Temperature within operating temperature
                    // - Not disabled
                    //

                    auto & generatorState = mElementStateBuffer[sourceElementIndex].Generator;

                    // Check if disable interval has elapsed
                    if (generatorState.DisabledSimulationTimestampEnd.has_value()
                        && currentSimulationTime >= *generatorState.DisabledSimulationTimestampEnd)
                    {
                        generatorState.DisabledSimulationTimestampEnd.reset();
                    }

                    bool isProducingCurrent;
                    if (generatorState.IsProducingCurrent)
                    {
                        if (points.IsWet(sourcePointIndex, 0.55f)
                            || !mMaterialOperatingTemperaturesBuffer[sourceElementIndex].IsInRange(points.GetTemperature(sourcePointIndex))
                            || generatorState.DisabledSimulationTimestampEnd.has_value())
                        {
                            isProducingCurrent = false;
                        }
                        else
                        {
                            isProducingCurrent = true;
                        }
                    }
                    else
                    {
                        if (!points.IsWet(sourcePointIndex, 0.15f)
                            && mMaterialOperatingTemperaturesBuffer[sourceElementIndex].IsBackInRange(points.GetTemperature(sourcePointIndex))
                            && !generatorState.DisabledSimulationTimestampEnd.has_value())
                        {
                            isProducingCurrent = true;
                        }
                        else
                        {
                            isProducingCurrent = false;
                        }
                    }

                    preconditionsSatisfied = isProducingCurrent;

                    //
                    // Check if it's a state change
                    //

                    if (mElementStateBuffer[sourceElementIndex].Generator.IsProducingCurrent != isProducingCurrent)
                    {
                        // Change state
                        mElementStateBuffer[sourceElementIndex].Generator.IsProducingCurrent = isProducingCurrent;

                        // See whether we need to publish a power probe change
                        if (mInstanceInfos[sourceElementIndex].InstanceIndex != NoneElectricalElementInstanceIndex)
                        {
                            // Notify
                            mGameEventHandler->OnPowerProbeToggled(
                                ElectricalElementId(mShipId, sourceElementIndex),
                                static_cast<ElectricalState>(isProducingCurrent));

                            // Show notifications
                            if (gameParameters.DoShowElectricalNotifications)
                            {
                                HighlightElectricalElement(sourceElementIndex, points);
                            }
                        }

                        // Remember that power has been severed, in case we're turning off
                        if (!isProducingCurrent)
                        {
                            mHasPowerBeenSeveredInCurrentStep = true;
                        }
                    }

                    break;
                }

                default:
                {
                    assert(false); // At the moment our only sources are generators
                    break;
                }
            }

            if (preconditionsSatisfied
                // Make sure we haven't visited it already
                && newConnectivityVisitSequenceNumber != mCurrentConnectivityVisitSequenceNumberBuffer[sourceElementIndex])
            {
                //
                // Flood graph
                //

                // Mark starting point as visited
                mCurrentConnectivityVisitSequenceNumberBuffer[sourceElementIndex] = newConnectivityVisitSequenceNumber;

                // Add source to queue
                assert(electricalElementsToVisit.empty());
                electricalElementsToVisit.push(sourceElementIndex);

                // Visit all electrical elements electrically reachable from this source
                while (!electricalElementsToVisit.empty())
                {
                    auto const e = electricalElementsToVisit.front();
                    electricalElementsToVisit.pop();

                    // Already marked as visited
                    assert(newConnectivityVisitSequenceNumber == mCurrentConnectivityVisitSequenceNumberBuffer[e]);

                    for (auto const cce : mConductingConnectedElectricalElementsBuffer[e])
                    {
                        assert(!IsDeleted(cce));

                        // Make sure not visited already
                        if (newConnectivityVisitSequenceNumber != mCurrentConnectivityVisitSequenceNumberBuffer[cce])
                        {
                            // Mark it as visited
                            mCurrentConnectivityVisitSequenceNumberBuffer[cce] = newConnectivityVisitSequenceNumber;

                            // Add to queue
                            electricalElementsToVisit.push(cce);
                        }
                    }
                }

                //
                // Generate heat
                //

                points.AddHeat(sourcePointIndex,
                    mMaterialHeatGeneratedBuffer[sourceElementIndex]
                    * gameParameters.ElectricalElementHeatProducedAdjustment
                    * GameParameters::SimulationStepTimeDuration<float>);
            }
        }
    }
}

void ElectricalElements::UpdateSinks(
    GameWallClock::time_point currentWallClockTime,
    float currentSimulationTime,
    SequenceNumber currentConnectivityVisitSequenceNumber,
    Points & points,
    Storm::Parameters const & stormParameters,
    GameParameters const & gameParameters)
{
    //
    // Visit all sinks and run their state machine
    //
    // Also visit deleted elements, as some types have
    // post-deletion wind-down state machines
    //

    // Reset engine group states
    std::fill(
        std::next(mEngineGroupStates.begin()), // No need to reset group "zero"
        mEngineGroupStates.end(),
        EngineGroupState());

    // For smoke
    float const effectiveAugmentedAirTemperature =
        gameParameters.AirTemperature
        + stormParameters.AirTemperatureDelta
        + 200.0f; // To ensure buoyancy

    for (auto const sinkElementIndex : mSinks)
    {
        //
        // Update state machine
        //

        bool const isConnectedToPower =
            (mCurrentConnectivityVisitSequenceNumberBuffer[sinkElementIndex] == currentConnectivityVisitSequenceNumber);

        bool isProducingHeat = false;

        switch (GetMaterialType(sinkElementIndex))
        {
            case ElectricalMaterial::ElectricalElementType::EngineController:
            {
                if (!IsDeleted(sinkElementIndex))
                {
                    auto & controllerState = mElementStateBuffer[sinkElementIndex].EngineController;

                    // Check whether it's powered
                    bool isPowered = false;
                    if (isConnectedToPower)
                    {
                        if (controllerState.IsPowered
                            && mMaterialOperatingTemperaturesBuffer[sinkElementIndex].IsInRange(points.GetTemperature(GetPointIndex(sinkElementIndex))))
                        {
                            isPowered = true;
                        }
                        else if (!controllerState.IsPowered
                            && mMaterialOperatingTemperaturesBuffer[sinkElementIndex].IsBackInRange(points.GetTemperature(GetPointIndex(sinkElementIndex))))
                        {
                            isPowered = true;
                        }
                    }

                    if (isPowered)
                    {
                        //
                        // Update engine group for this controller
                        //                        

                        assert(controllerState.EngineGroup != 0);

                        // RPM: 0, 1/N, 1/N->1

                        float constexpr TelegraphCoeff =
                            1.0f
                            / static_cast<float>(GameParameters::EngineTelegraphDegreesOfFreedom / 2 - 1);

                        int const absTelegraphValue = std::abs(controllerState.CurrentTelegraphValue);

                        float controllerRpm;
                        if (absTelegraphValue == 0)
                            controllerRpm = 0.0f;
                        else if (absTelegraphValue == 1)
                            controllerRpm = TelegraphCoeff;
                        else
                            controllerRpm = static_cast<float>(absTelegraphValue - 1) * TelegraphCoeff;

                        // Group RPM = max
                        mEngineGroupStates[controllerState.EngineGroup].GroupRpm = std::max(mEngineGroupStates[controllerState.EngineGroup].GroupRpm, controllerRpm);

                        // Thrust magnitude: 0, 0, 1/N->1

                        float controllerThrustMagnitude = 0.0f;
                        if (controllerState.CurrentTelegraphValue > 1)
                        {
                            controllerThrustMagnitude = static_cast<float>(controllerState.CurrentTelegraphValue - 1) * TelegraphCoeff;
                        }
                        else if (controllerState.CurrentTelegraphValue < -1)
                        {
                            controllerThrustMagnitude = static_cast<float>(controllerState.CurrentTelegraphValue + 1) * TelegraphCoeff;
                        }

                        // Group thrust magnitude = sum
                        mEngineGroupStates[controllerState.EngineGroup].GroupThrustMagnitude += controllerThrustMagnitude;
                    }

                    // Remember controller state
                    mElementStateBuffer[sinkElementIndex].EngineController.IsPowered = isPowered;
                }

                break;
            }

            case ElectricalMaterial::ElectricalElementType::Lamp:
            {
                if (!IsDeleted(sinkElementIndex))
                {
                    // Update state machine
                    RunLampStateMachine(
                        isConnectedToPower,
                        sinkElementIndex,
                        currentWallClockTime,
                        currentSimulationTime,
                        points,
                        gameParameters);

                    isProducingHeat = (GetAvailableLight(sinkElementIndex) > 0.0f);
                }

                break;
            }

            case ElectricalMaterial::ElectricalElementType::OtherSink:
            {
                if (!IsDeleted(sinkElementIndex))
                {
                    // Update state machine
                    if (mElementStateBuffer[sinkElementIndex].OtherSink.IsPowered)
                    {
                        if (!isConnectedToPower
                            || !mMaterialOperatingTemperaturesBuffer[sinkElementIndex].IsInRange(points.GetTemperature(GetPointIndex(sinkElementIndex))))
                        {
                            mElementStateBuffer[sinkElementIndex].OtherSink.IsPowered = false;
                        }
                    }
                    else
                    {
                        if (isConnectedToPower
                            && mMaterialOperatingTemperaturesBuffer[sinkElementIndex].IsBackInRange(points.GetTemperature(GetPointIndex(sinkElementIndex))))
                        {
                            mElementStateBuffer[sinkElementIndex].OtherSink.IsPowered = true;
                        }
                    }

                    isProducingHeat = mElementStateBuffer[sinkElementIndex].OtherSink.IsPowered;
                }

                break;
            }

            case ElectricalMaterial::ElectricalElementType::PowerMonitor:
            {
                if (!IsDeleted(sinkElementIndex))
                {
                    // Update state machine
                    if (mElementStateBuffer[sinkElementIndex].PowerMonitor.IsPowered)
                    {
                        if (!isConnectedToPower)
                        {
                            //
                            // Toggle state ON->OFF
                            //

                            mElementStateBuffer[sinkElementIndex].PowerMonitor.IsPowered = false;

                            // Notify
                            mGameEventHandler->OnPowerProbeToggled(
                                ElectricalElementId(mShipId, sinkElementIndex),
                                ElectricalState::Off);

                            // Show notifications
                            if (gameParameters.DoShowElectricalNotifications)
                            {
                                HighlightElectricalElement(sinkElementIndex, points);
                            }
                        }
                    }
                    else
                    {
                        if (isConnectedToPower)
                        {
                            //
                            // Toggle state OFF->ON
                            //

                            mElementStateBuffer[sinkElementIndex].PowerMonitor.IsPowered = true;

                            // Notify
                            mGameEventHandler->OnPowerProbeToggled(
                                ElectricalElementId(mShipId, sinkElementIndex),
                                ElectricalState::On);

                            // Show notifications
                            if (gameParameters.DoShowElectricalNotifications)
                            {
                                HighlightElectricalElement(sinkElementIndex, points);
                            }
                        }
                    }
                }

                break;
            }

            case ElectricalMaterial::ElectricalElementType::ShipSound:
            {
                if (!IsDeleted(sinkElementIndex))
                {
                    auto & state = mElementStateBuffer[sinkElementIndex].ShipSound;

                    // Update state machine
                    if (state.IsPlaying)
                    {
                        if ((!state.IsSelfPowered && !isConnectedToPower)
                            || !mConductivityBuffer[sinkElementIndex].ConductsElectricity)
                        {
                            //
                            // Toggle state ON->OFF
                            //

                            state.IsPlaying = false;

                            // Notify sound
                            mGameEventHandler->OnShipSoundUpdated(
                                ElectricalElementId(mShipId, sinkElementIndex),
                                *(mMaterialBuffer[sinkElementIndex]),
                                false,
                                false); // Irrelevant

                            // Show notifications
                            if (gameParameters.DoShowElectricalNotifications)
                            {
                                HighlightElectricalElement(sinkElementIndex, points);
                            }
                        }
                    }
                    else
                    {
                        if ((state.IsSelfPowered || isConnectedToPower)
                            && mConductivityBuffer[sinkElementIndex].ConductsElectricity)
                        {
                            //
                            // Toggle state OFF->ON
                            //

                            state.IsPlaying = true;

                            // Notify sound
                            mGameEventHandler->OnShipSoundUpdated(
                                ElectricalElementId(mShipId, sinkElementIndex),
                                *(mMaterialBuffer[sinkElementIndex]),
                                true,
                                points.IsCachedUnderwater(GetPointIndex(sinkElementIndex)));

                            // Disturb ocean, with delays depending on sound
                            switch (mMaterialBuffer[sinkElementIndex]->ShipSoundType)
                            {
                            case ElectricalMaterial::ShipSoundElementType::QueenMaryHorn:
                            {
                                mParentWorld.DisturbOcean(std::chrono::milliseconds(250));
                                break;
                            }

                            case ElectricalMaterial::ShipSoundElementType::FourFunnelLinerWhistle:
                            {
                                mParentWorld.DisturbOcean(std::chrono::milliseconds(600));
                                break;
                            }

                            case ElectricalMaterial::ShipSoundElementType::TripodHorn:
                            {
                                mParentWorld.DisturbOcean(std::chrono::milliseconds(500));
                                break;
                            }

                            case ElectricalMaterial::ShipSoundElementType::LakeFreighterHorn:
                            {
                                mParentWorld.DisturbOcean(std::chrono::milliseconds(150));
                                break;
                            }

                            case ElectricalMaterial::ShipSoundElementType::ShieldhallSteamSiren:
                            {
                                mParentWorld.DisturbOcean(std::chrono::milliseconds(550));
                                break;
                            }

                            case ElectricalMaterial::ShipSoundElementType::QueenElizabeth2Horn:
                            {
                                mParentWorld.DisturbOcean(std::chrono::milliseconds(250));
                                break;
                            }

                            case ElectricalMaterial::ShipSoundElementType::SSRexWhistle:
                            {
                                mParentWorld.DisturbOcean(std::chrono::milliseconds(250));
                                break;
                            }

                            case ElectricalMaterial::ShipSoundElementType::Klaxon1:
                            {
                                mParentWorld.DisturbOcean(std::chrono::milliseconds(100));
                                break;
                            }

                            case ElectricalMaterial::ShipSoundElementType::NuclearAlarm1:
                            {
                                mParentWorld.DisturbOcean(std::chrono::milliseconds(500));
                                break;
                            }

                            case ElectricalMaterial::ShipSoundElementType::EvacuationAlarm1:
                            {
                                mParentWorld.DisturbOcean(std::chrono::milliseconds(100));
                                break;
                            }

                            case ElectricalMaterial::ShipSoundElementType::EvacuationAlarm2:
                            {
                                mParentWorld.DisturbOcean(std::chrono::milliseconds(100));
                                break;
                            }

                            default:
                            {
                                // Do not disturb
                                break;
                            }
                            }

                            // Show notifications
                            if (gameParameters.DoShowElectricalNotifications)
                            {
                                HighlightElectricalElement(sinkElementIndex, points);
                            }
                        }
                    }
                }

                break;
            }

            case ElectricalMaterial::ElectricalElementType::SmokeEmitter:
            {
                auto const emitterPointIndex = GetPointIndex(sinkElementIndex);
                float const emitterDepth = points.GetCachedDepth(emitterPointIndex);

                if (!IsDeleted(sinkElementIndex))
                {
                    // Update state machine
                    if (mElementStateBuffer[sinkElementIndex].SmokeEmitter.IsOperating)
                    {
                        if (!isConnectedToPower
                            || emitterDepth > 0.0f)
                        {
                            // Stop operating
                            mElementStateBuffer[sinkElementIndex].SmokeEmitter.IsOperating = false;
                        }
                    }
                    else
                    {
                        if (isConnectedToPower
                            && emitterDepth <= 0.0f)
                        {
                            // Start operating
                            mElementStateBuffer[sinkElementIndex].SmokeEmitter.IsOperating = true;

                            // Make sure we calculate the next emission timestamp
                            mElementStateBuffer[sinkElementIndex].SmokeEmitter.NextEmissionSimulationTimestamp = 0.0f;
                        }
                    }

                    if (mElementStateBuffer[sinkElementIndex].SmokeEmitter.IsOperating)
                    {
                        // See if we need to calculate the next emission timestamp
                        if (mElementStateBuffer[sinkElementIndex].SmokeEmitter.NextEmissionSimulationTimestamp == 0.0f)
                        {
                            mElementStateBuffer[sinkElementIndex].SmokeEmitter.NextEmissionSimulationTimestamp =
                                currentSimulationTime
                                + GameRandomEngine::GetInstance().GenerateExponentialReal(
                                gameParameters.SmokeEmissionDensityAdjustment
                                / mElementStateBuffer[sinkElementIndex].SmokeEmitter.EmissionRate);
                        }

                        // See if it's time to emit smoke
                        if (currentSimulationTime >= mElementStateBuffer[sinkElementIndex].SmokeEmitter.NextEmissionSimulationTimestamp)
                        {
                            //
                            // Emit smoke
                            //

                            // Choose temperature: highest of emitter's and current air + something (to ensure buoyancy)
                            float const smokeTemperature = std::max(
                                points.GetTemperature(emitterPointIndex),
                                effectiveAugmentedAirTemperature);

                            // Generate particle
                            points.CreateEphemeralParticleLightSmoke(
                                points.GetPosition(emitterPointIndex),
                                emitterDepth,
                                smokeTemperature,
                                currentSimulationTime,
                                points.GetPlaneId(emitterPointIndex),
                                gameParameters);

                            // Make sure we re-calculate the next emission timestamp
                            mElementStateBuffer[sinkElementIndex].SmokeEmitter.NextEmissionSimulationTimestamp = 0.0f;
                        }
                    }
                }

                break;
            }

            case ElectricalMaterial::ElectricalElementType::WaterPump:
            {
                auto const pointIndex = GetPointIndex(sinkElementIndex);

                auto & waterPumpState = mElementStateBuffer[sinkElementIndex].WaterPump;

                //
                // 1) If not deleted, run operating state machine (connectivity, operating temperature)
                //    in order to come up with TargetForce
                //

                if (!IsDeleted(sinkElementIndex))
                {
                    if (waterPumpState.TargetNormalizedForce != 0.0f)
                    {
                        // Currently it's powered...
                        // ...see if it stops being powered
                        if (!isConnectedToPower
                            || !mMaterialOperatingTemperaturesBuffer[sinkElementIndex].IsInRange(points.GetTemperature(pointIndex)))
                        {
                            // State change: stop operating
                            waterPumpState.TargetNormalizedForce = 0.0f;

                            // Show notifications
                            if (gameParameters.DoShowElectricalNotifications)
                            {
                                HighlightElectricalElement(sinkElementIndex, points);
                            }
                        }
                        else
                        {
                            // Operating, thus producing heat
                            isProducingHeat = true;
                        }
                    }
                    else
                    {
                        // Currently it's not powered...
                        // ...see if it becomes powered
                        if (isConnectedToPower
                            && mMaterialOperatingTemperaturesBuffer[sinkElementIndex].IsBackInRange(points.GetTemperature(pointIndex)))
                        {
                            // State change: start operating
                            waterPumpState.TargetNormalizedForce = 1.0f;

                            // Operating, thus producing heat
                            isProducingHeat = true;

                            // Show notifications
                            if (gameParameters.DoShowElectricalNotifications)
                            {
                                HighlightElectricalElement(sinkElementIndex, points);
                            }
                        }
                    }
                }

                //
                // 2) Converge CurrentForce towards TargetForce and eventually act on particle
                //
                // We run this also when deleted, as it's part of our wind-down state machine
                //

                // Converge current force
                waterPumpState.CurrentNormalizedForce +=
                    (waterPumpState.TargetNormalizedForce - waterPumpState.CurrentNormalizedForce)
                    * 0.03f; // Convergence rate, magic number
                if (std::abs(waterPumpState.CurrentNormalizedForce - waterPumpState.TargetNormalizedForce) < 0.001f)
                {
                    waterPumpState.CurrentNormalizedForce = waterPumpState.TargetNormalizedForce;
                }

                // Calculate force
                float waterPumpForce = waterPumpState.CurrentNormalizedForce * waterPumpState.NominalForce;
                if (waterPumpForce == 0.0f) // Ensure -0.0 is +0.0, or else CompositeIsLeaking's union trick won't work
                {
                    waterPumpForce = 0.0f;
                }

                // Apply force to point
                points.GetLeakingComposite(pointIndex).LeakingSources.WaterPumpForce = waterPumpForce;

                // Eventually publish force change notification
                if (waterPumpState.CurrentNormalizedForce != waterPumpState.LastPublishedNormalizedForce)
                {
                    // Notify
                    mGameEventHandler->OnWaterPumpUpdated(
                        ElectricalElementId(mShipId, sinkElementIndex),
                        waterPumpState.CurrentNormalizedForce);

                    // Remember last-published value
                    waterPumpState.LastPublishedNormalizedForce = waterPumpState.CurrentNormalizedForce;
                }

                break;
            }

            case ElectricalMaterial::ElectricalElementType::WatertightDoor:
            {
                //
                // Run operating state machine (connectivity, operating temperature)
                //

                if (!IsDeleted(sinkElementIndex))
                {
                    auto const pointIndex = GetPointIndex(sinkElementIndex);

                    auto & watertightDoorState = mElementStateBuffer[sinkElementIndex].WatertightDoor;

                    bool hasStateChanged = false;
                    if (watertightDoorState.IsActivated)
                    {
                        // Currently it's activated...
                        // ...see if it stops being activated
                        if (!isConnectedToPower
                            || !mMaterialOperatingTemperaturesBuffer[sinkElementIndex].IsInRange(points.GetTemperature(pointIndex)))
                        {
                            //
                            // State change: stop operating
                            //

                            watertightDoorState.IsActivated = false;

                            hasStateChanged = true;
                        }
                    }
                    else
                    {
                        // Currently it's not activated...
                        // ...see if it becomes activated
                        if (isConnectedToPower
                            && mMaterialOperatingTemperaturesBuffer[sinkElementIndex].IsBackInRange(points.GetTemperature(pointIndex)))
                        {
                            //
                            // State change: start operating
                            //

                            watertightDoorState.IsActivated = true;

                            hasStateChanged = true;
                        }
                    }

                    if (hasStateChanged)
                    {
                        // Propagate structural effect
                        assert(nullptr != mShipPhysicsHandler);
                        mShipPhysicsHandler->HandleWatertightDoorUpdated(pointIndex, watertightDoorState.IsOpen());

                        // Publish state change
                        mGameEventHandler->OnWatertightDoorUpdated(
                            ElectricalElementId(mShipId, sinkElementIndex),
                            watertightDoorState.IsOpen());

                        // Show notifications
                        if (gameParameters.DoShowElectricalNotifications)
                        {
                            HighlightElectricalElement(sinkElementIndex, points);
                        }
                    }
                }

                break;
            }

            default:
            {
                assert(false);
                break;
            }
        }

        //
        // Generate heat if sink is working
        //

        if (isProducingHeat)
        {
            points.AddHeat(GetPointIndex(sinkElementIndex),
                mMaterialHeatGeneratedBuffer[sinkElementIndex]
                * gameParameters.ElectricalElementHeatProducedAdjustment
                * GameParameters::SimulationStepTimeDuration<float>);
        }
    }

    //
    // Visit all engines and run their state machine
    //

    for (auto const engineSinkElementIndex : mEngines)
    {
        if (!IsDeleted(engineSinkElementIndex))
        {
            assert(GetMaterialType(engineSinkElementIndex) == ElectricalMaterial::ElectricalElementType::Engine);
            auto & engineState = mElementStateBuffer[engineSinkElementIndex].Engine;

            auto const enginePointIndex = GetPointIndex(engineSinkElementIndex);

            // Check first of all if super-electrification interval has elapsed
            if (engineState.SuperElectrificationSimulationTimestampEnd.has_value()
                && currentSimulationTime >= *engineState.SuperElectrificationSimulationTimestampEnd)
            {
                // Elapsed
                engineState.SuperElectrificationSimulationTimestampEnd.reset();
            }

            //
            // Calculate thrust direction based off reference point
            //

            vec2f const engineToReferencePointDir =
                (points.GetPosition(engineState.ReferencePointIndex) - points.GetPosition(enginePointIndex))
                .normalise();

            vec2f const thrustDir = vec2f(
                engineState.ReferencePointCWAngleCos * engineToReferencePointDir.x + engineState.ReferencePointCWAngleSin * engineToReferencePointDir.y,
                -engineState.ReferencePointCWAngleSin * engineToReferencePointDir.x + engineState.ReferencePointCWAngleCos * engineToReferencePointDir.y);

            //
            // Calculate target RPM and thrust magnitude
            //

            // Adjust targets based off super-electrification
            float powerMultiplier = engineState.SuperElectrificationSimulationTimestampEnd.has_value()
                ? 4.0f
                : 1.0f;

            // Adjust targets based off point's water
            auto const engineWater = points.GetWater(enginePointIndex);
            if (engineWater != 0.0f)
            {
                //  e^(-0.5*x + 5)/(5 + e^(-0.5*x + 5))
                float const expCoeff = std::exp(-engineWater * 0.5f + 5.0f);
                powerMultiplier *= expCoeff / (5.0f + expCoeff);
            }

            // Update current RPM to match group target (via responsiveness)
            float const targetRpm = mEngineGroupStates[engineState.EngineGroup].GroupRpm;
            {
                float const effectiveTargetRpm = targetRpm * powerMultiplier;

                engineState.CurrentRpm =
                    engineState.CurrentRpm
                    + (effectiveTargetRpm - engineState.CurrentRpm) * engineState.Responsiveness;

                if (std::abs(effectiveTargetRpm - engineState.CurrentRpm) < 0.001f)
                {
                    engineState.CurrentRpm = effectiveTargetRpm;
                }
            }

            // Update current thrust magnitude to match group target (via responsiveness)
            float const targetThrustMagnitude = mEngineGroupStates[engineState.EngineGroup].GroupThrustMagnitude;
            {
                float const effectiveTargetThrustMagnitude = targetThrustMagnitude * powerMultiplier;

                engineState.CurrentThrustMagnitude =
                    engineState.CurrentThrustMagnitude
                    + (effectiveTargetThrustMagnitude - engineState.CurrentThrustMagnitude) * engineState.Responsiveness;

                if (std::abs(effectiveTargetThrustMagnitude - engineState.CurrentThrustMagnitude) < 0.001f)
                {
                    engineState.CurrentThrustMagnitude = effectiveTargetThrustMagnitude;
                }
            }

            //
            // Apply engine thrust
            //

            // Calculate force vector
            vec2f const thrustForce =
                thrustDir
                * engineState.CurrentThrustMagnitude
                * engineState.ThrustCapacity
                * gameParameters.EngineThrustAdjustment;

            // Apply force to point
            points.AddStaticForce(enginePointIndex, thrustForce);

            //
            // Publish
            //

            // Eventually publish power change notification
            if (engineState.CurrentThrustMagnitude != engineState.LastPublishedThrustMagnitude
                || engineState.CurrentRpm != engineState.LastPublishedRpm)
            {
                // Notify
                mGameEventHandler->OnEngineMonitorUpdated(
                    ElectricalElementId(mShipId, engineSinkElementIndex),
                    engineState.CurrentThrustMagnitude,
                    engineState.CurrentRpm);

                // Remember last-published values
                engineState.LastPublishedThrustMagnitude = engineState.CurrentThrustMagnitude;
                engineState.LastPublishedRpm = engineState.CurrentRpm;
            }

            // Eventually show notifications - only if moving between zero and non-zero RPM
            if (gameParameters.DoShowElectricalNotifications)
            {
                if ((targetRpm != 0.0f && engineState.LastHighlightedRpm == 0.0)
                    || (targetRpm == 0.0f && engineState.LastHighlightedRpm != 0.0))
                {
                    engineState.LastHighlightedRpm = targetRpm;

                    HighlightElectricalElement(engineSinkElementIndex, points);
                }
            }

            //
            // Generate heat if running
            //

            points.AddHeat(enginePointIndex,
                mMaterialHeatGeneratedBuffer[engineSinkElementIndex]
                * engineState.CurrentRpm
                * gameParameters.ElectricalElementHeatProducedAdjustment
                * GameParameters::SimulationStepTimeDuration<float>);

            //
            // Generate wake - if running and underwater
            //

            {
                vec2f const enginePosition = points.GetPosition(enginePointIndex);

                // Depth of engine, positive = underwater
                float const engineDepth = points.GetCachedDepth(enginePointIndex);

                float const absThrustMagnitude = std::abs(engineState.CurrentThrustMagnitude);

                if (absThrustMagnitude > 0.1f // Magic number
                    && engineDepth > 0.0f)
                {
                    // Generate wake particles
                    if (gameParameters.DoGenerateEngineWakeParticles)
                    {
                        auto const planeId = points.GetPlaneId(enginePointIndex);

                        for (int i = 0; i < std::round(absThrustMagnitude * 4.0f); ++i)
                        {
                            // Choose random angle for this particle
                            float constexpr HalfFanOutAngle = Pi<float> / 14.0f; // Magic number
                            float const angle = Clamp(
                                0.15f * GameRandomEngine::GetInstance().GenerateNormalizedNormalReal(),
                                -HalfFanOutAngle,
                                HalfFanOutAngle);

                            // Calculate velocity
                            vec2f const wakeVelocity =
                                -thrustDir.rotate(angle)
                                * (engineState.CurrentThrustMagnitude < 0.0f ? -1.0f : 1.0f)
                                * 20.0f; // Magic number

                            // Create particle
                            points.CreateEphemeralParticleWakeBubble(
                                enginePosition,
                                wakeVelocity,
                                engineDepth,
                                currentSimulationTime,
                                planeId,
                                gameParameters);
                        }
                    }

                    // Displace ocean surface
                    if (gameParameters.DoDisplaceWater)
                    {
                        // Offset from engine due to thrust - along the thrust direction
                        vec2f const engineOffset =
                            -thrustForce
                            * GameParameters::SimulationStepTimeDuration<float> *GameParameters::SimulationStepTimeDuration<float>
                            *0.025f;

                        vec2f const engineOffsetedPosition = enginePosition + engineOffset;

                        // New depth at offset
                        float const offsetedEngineDepth = mParentWorld.GetOceanSurface().GetDepth(engineOffsetedPosition);

                        // Sine perturbation - to make sure that water displacement keeps moving,
                        // otherwise big waves build up
                        float const sinePerturbation = std::sin(currentSimulationTime * 2.5f);

                        // Displacement amount - goes to zero after a certain depth threshold
                        float constexpr MaxDepth = 10.0f;
                        float const displacementAmount =
                            4.0f * absThrustMagnitude
                            * (1.0f + sinePerturbation) / 2.0f
                            * (1.0f - std::min(2.0f * SmoothStep(0.0f, 2.0f * MaxDepth, offsetedEngineDepth), 1.0f));

                        mParentWorld.DisplaceOceanSurfaceAt(
                            engineOffsetedPosition.x,
                            displacementAmount);
                    }
                }
            }

            //
            // Update engine conductivity
            //

            InternalChangeConductivity(
                engineSinkElementIndex,
                (engineState.CurrentRpm > 0.15f)); // Magic number
        }
    }

    //
    // Clear "power severed" dirtyness
    //

    mHasPowerBeenSeveredInCurrentStep = false;
}

void ElectricalElements::RunLampStateMachine(
    bool isConnectedToPower,
    ElementIndex elementLampIndex,
    GameWallClock::time_point currentWallClockTime,
    float currentSimulationTime,
    Points & points,
    GameParameters const & /*gameParameters*/)
{
    float constexpr LampWetFailureWaterHighWatermark = 0.1f;
    float constexpr LampWetFailureWaterLowWatermark = 0.055f;

    //
    // Lamp is only on if visited or self-powered, and within operating temperature and
    // not disabled; actual light depends on flicker state machine
    //

    auto const pointIndex = GetPointIndex(elementLampIndex);

    assert(GetMaterialType(elementLampIndex) == ElectricalMaterial::ElectricalElementType::Lamp);
    auto & lamp = mElementStateBuffer[elementLampIndex].Lamp;

    // First of all, check if disable interval has elapsed
    if (lamp.DisabledSimulationTimestampEnd.has_value()
        && currentSimulationTime >= *lamp.DisabledSimulationTimestampEnd)
    {
        lamp.DisabledSimulationTimestampEnd.reset();
    }

    // Now run state machine
    switch (lamp.State)
    {
        case ElementState::LampState::StateType::Initial:
        {
            // Transition to ON - if we have current or if we're self-powered AND if within operating temperature
            if ((isConnectedToPower || lamp.IsSelfPowered)
                && mMaterialOperatingTemperaturesBuffer[elementLampIndex].IsInRange(points.GetTemperature(pointIndex))
                && !lamp.DisabledSimulationTimestampEnd.has_value())
            {
                // Transition to ON
                mAvailableLightBuffer[elementLampIndex] = 1.f;
                lamp.State = ElementState::LampState::StateType::LightOn;
                lamp.NextWetFailureCheckTimePoint = currentWallClockTime + std::chrono::seconds(1);
            }
            else
            {
                // Transition to OFF
                mAvailableLightBuffer[elementLampIndex] = 0.f;
                lamp.State = ElementState::LampState::StateType::LightOff;
            }

            break;
        }

        case ElementState::LampState::StateType::LightOn:
        {
            // Check whether we still have current, or we're wet and it's time to fail,
            // or whether we are outside of the operating temperature range
            if ((   !isConnectedToPower
                    && !lamp.IsSelfPowered
                ) ||
                (   points.IsWet(GetPointIndex(elementLampIndex), LampWetFailureWaterHighWatermark)
                    && CheckWetFailureTime(lamp, currentWallClockTime)
                ) ||
                (
                    !mMaterialOperatingTemperaturesBuffer[elementLampIndex].IsInRange(points.GetTemperature(pointIndex))
                ) ||
                (
                    lamp.DisabledSimulationTimestampEnd.has_value()
                ))
            {
                //
                // Turn off
                //

                mAvailableLightBuffer[elementLampIndex] = 0.0f;

                // Check whether we need to flicker or just turn off suddenly
                if (mHasPowerBeenSeveredInCurrentStep)
                {
                    //
                    // Start flicker state machine
                    //

                    // Transition state, choose whether to A or B
                    lamp.FlickerCounter = 0u;
                    lamp.NextStateTransitionTimePoint = currentWallClockTime + ElementState::LampState::FlickerStartInterval;
                    if (GameRandomEngine::GetInstance().Choose(2) == 0)
                        lamp.State = ElementState::LampState::StateType::FlickerA;
                    else
                        lamp.State = ElementState::LampState::StateType::FlickerB;
                }
                else
                {
                    //
                    // Turn off immediately
                    //

                    lamp.State = ElementState::LampState::StateType::LightOff;
                }
            }

            break;
        }

        case ElementState::LampState::StateType::FlickerA:
        {
            // 0-1-0-1-Off

            // Check if we should become ON again
            if ((isConnectedToPower || lamp.IsSelfPowered)
                && !points.IsWet(GetPointIndex(elementLampIndex), LampWetFailureWaterLowWatermark)
                && mMaterialOperatingTemperaturesBuffer[elementLampIndex].IsBackInRange(points.GetTemperature(pointIndex))
                && !lamp.DisabledSimulationTimestampEnd.has_value())
            {
                mAvailableLightBuffer[elementLampIndex] = 1.f;

                // Transition state
                lamp.State = ElementState::LampState::StateType::LightOn;
            }
            else if (currentWallClockTime > lamp.NextStateTransitionTimePoint)
            {
                ++lamp.FlickerCounter;

                if (1 == lamp.FlickerCounter
                    || 3 == lamp.FlickerCounter)
                {
                    // Flicker to on, for a short time

                    mAvailableLightBuffer[elementLampIndex] = 1.f;

                    mGameEventHandler->OnLightFlicker(
                        DurationShortLongType::Short,
                        points.IsCachedUnderwater(pointIndex),
                        1);

                    lamp.NextStateTransitionTimePoint = currentWallClockTime + ElementState::LampState::FlickerAInterval;
                }
                else if (2 == lamp.FlickerCounter)
                {
                    // Flicker to off, for a short time

                    mAvailableLightBuffer[elementLampIndex] = 0.f;

                    lamp.NextStateTransitionTimePoint = currentWallClockTime + ElementState::LampState::FlickerAInterval;
                }
                else
                {
                    assert(4 == lamp.FlickerCounter);

                    // Transition to off for good
                    mAvailableLightBuffer[elementLampIndex] = 0.f;
                    lamp.State = ElementState::LampState::StateType::LightOff;
                }
            }

            break;
        }

        case ElementState::LampState::StateType::FlickerB:
        {
            // 0-1-0-1--0-1-Off

            // Check if we should become ON again
            if ((isConnectedToPower || lamp.IsSelfPowered)
                && !points.IsWet(GetPointIndex(elementLampIndex), LampWetFailureWaterLowWatermark)
                && mMaterialOperatingTemperaturesBuffer[elementLampIndex].IsBackInRange(points.GetTemperature(pointIndex))
                && !lamp.DisabledSimulationTimestampEnd.has_value())
            {
                mAvailableLightBuffer[elementLampIndex] = 1.f;

                // Transition state
                lamp.State = ElementState::LampState::StateType::LightOn;
            }
            else if (currentWallClockTime > lamp.NextStateTransitionTimePoint)
            {
                ++lamp.FlickerCounter;

                if (1 == lamp.FlickerCounter
                    || 5 == lamp.FlickerCounter)
                {
                    // Flicker to on, for a short time

                    mAvailableLightBuffer[elementLampIndex] = 1.f;

                    mGameEventHandler->OnLightFlicker(
                        DurationShortLongType::Short,
                        points.IsCachedUnderwater(pointIndex),
                        1);

                    lamp.NextStateTransitionTimePoint = currentWallClockTime + ElementState::LampState::FlickerBInterval;
                }
                else if (2 == lamp.FlickerCounter
                        || 4 == lamp.FlickerCounter)
                {
                    // Flicker to off, for a short time

                    mAvailableLightBuffer[elementLampIndex] = 0.f;

                    lamp.NextStateTransitionTimePoint = currentWallClockTime + ElementState::LampState::FlickerBInterval;
                }
                else if (3 == lamp.FlickerCounter)
                {
                    // Flicker to on, for a longer time

                    mAvailableLightBuffer[elementLampIndex] = 1.f;

                    mGameEventHandler->OnLightFlicker(
                        DurationShortLongType::Long,
                        points.IsCachedUnderwater(pointIndex),
                        1);

                    lamp.NextStateTransitionTimePoint = currentWallClockTime + 2 * ElementState::LampState::FlickerBInterval;
                }
                else
                {
                    assert(6 == lamp.FlickerCounter);

                    // Transition to off for good
                    mAvailableLightBuffer[elementLampIndex] = 0.f;
                    lamp.State = ElementState::LampState::StateType::LightOff;
                }
            }

            break;
        }

        case ElementState::LampState::StateType::LightOff:
        {
            assert(mAvailableLightBuffer[elementLampIndex] == 0.f);

            // Check if we should become ON again
            if ((isConnectedToPower || lamp.IsSelfPowered)
                && !points.IsWet(GetPointIndex(elementLampIndex), LampWetFailureWaterLowWatermark)
                && mMaterialOperatingTemperaturesBuffer[elementLampIndex].IsBackInRange(points.GetTemperature(pointIndex))
                && !lamp.DisabledSimulationTimestampEnd.has_value())
            {
                mAvailableLightBuffer[elementLampIndex] = 1.f;

                // Notify flicker event, so we play light-on sound
                mGameEventHandler->OnLightFlicker(
                    DurationShortLongType::Short,
                    points.IsCachedUnderwater(pointIndex),
                    1);

                // Transition state
                lamp.State = ElementState::LampState::StateType::LightOn;
            }

            break;
        }
    }
}

bool ElectricalElements::CheckWetFailureTime(
    ElementState::LampState & lamp,
    GameWallClock::time_point currentWallClockTime)
{
    bool isFailure = false;

    if (currentWallClockTime >= lamp.NextWetFailureCheckTimePoint)
    {
        // Sample the CDF
       isFailure =
            GameRandomEngine::GetInstance().GenerateNormalizedUniformReal()
            < lamp.WetFailureRateCdf;

        // Schedule next check
        lamp.NextWetFailureCheckTimePoint = currentWallClockTime + std::chrono::seconds(1);
    }

    return isFailure;
}

}