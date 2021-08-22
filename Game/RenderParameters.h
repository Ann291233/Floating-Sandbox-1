/***************************************************************************************
* Original Author:		Gabriele Giuseppini
* Created:				2020-07-12
* Copyright:			Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#pragma once

#include "ViewModel.h"

#include <GameCore/Colors.h>
#include <GameCore/GameTypes.h>
#include <GameCore/ImageSize.h>

#include <cstdint>

namespace Render {

/*
 * The entire set of user-controllable settings or calculated parameters
 * that are direct input to the rendering process (i.e. which are accessed
 *  by rendering code)
 */
struct RenderParameters
{
    ViewModel View;
    float EffectiveAmbientLightIntensity; // Calculated

    //
    // World
    //

    rgbColor FlatSkyColor;
    float OceanTransparency;
    float OceanDarkeningRate;
    OceanRenderModeType OceanRenderMode;
    rgbColor DepthOceanColorStart;
    rgbColor DepthOceanColorEnd;
    rgbColor FlatOceanColor;
    size_t OceanTextureIndex;
    OceanRenderDetailType OceanRenderDetail;
    bool ShowShipThroughOcean;
    LandRenderModeType LandRenderMode;
    rgbColor FlatLandColor;
    size_t LandTextureIndex;

    //
    // Ship
    //

    float ShipAmbientLightSensitivity;
    rgbColor FlatLampLightColor;
    bool DrawExplosions;
    bool DrawFlames;
    bool ShowStressedSprings;
    bool ShowFrontiers;
    bool ShowAABBs;
    vec3f ShipWaterColor; // Calculated
    float ShipWaterContrast;
    float ShipWaterLevelOfDetail;
    HeatRenderModeType HeatRenderMode;
    float HeatSensitivity;
    DebugShipRenderModeType DebugShipRenderMode;

    //
    // Misc
    //

    UnitsSystem DisplayUnitsSystem;

    //
    // Dirty flags
    //

    // World
    bool IsViewDirty;
    bool IsCanvasSizeDirty;
    bool IsEffectiveAmbientLightIntensityDirty;
    bool IsFlatSkyColorDirty;
    bool IsOceanDarkeningRateDirty;
    bool AreOceanRenderModeParametersDirty; // Tracks various render mode parameters as a whole, for convenience
    bool IsOceanTextureIndexDirty;
    bool AreLandRenderParametersDirty; // Tracks various land render mode parameters as a whole, for convenience
    bool IsLandTextureIndexDirty;
    // Ship
    bool IsShipAmbientLightSensitivityDirty;
    bool IsFlatLampLightColorDirty;
    bool IsShipWaterColorDirty;
    bool IsShipWaterContrastDirty;
    bool IsShipWaterLevelOfDetailDirty;
    bool IsHeatSensitivityDirty;
    bool AreShipStructureRenderModeSelectorsDirty; // For all those parameters that require changing ship shaders
    // Misc
    bool IsDisplayUnitsSystemDirty;

    RenderParameters(
        LogicalPixelSize const & initialCanvasSize,
        int logicalToPhysicalPixelFactor);

    RenderParameters TakeSnapshotAndClear();
};

}