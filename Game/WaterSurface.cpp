﻿/***************************************************************************************
* Original Author:		Gabriele Giuseppini
* Created:				2018-04-14
* Copyright:			Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "Physics.h"

namespace Physics {

WaterSurface::WaterSurface()
    : mSamples(new Sample[SamplesCount])
{
}

void WaterSurface::Update(
    float currentSimulationTime,
    Wind const & wind,
    GameParameters const & gameParameters)
{
    // Waves

    float const waveSpeed = gameParameters.WindSpeedBase / 6.0f; // Water moves slower than wind
    float const waveTheta = currentSimulationTime * (0.5f + waveSpeed) / 3.0f;
    float const waveHeight = gameParameters.WaveHeight;

    // Ripples

    float const windSpeedAbsoluteMagnitude = wind.GetCurrentWindSpeed().length();
    float const windSpeedGustRelativeAmplitude = wind.GetMaxSpeedMagnitude() - wind.GetBaseSpeedMagnitude();
    float const rawWindNormalizedIncisiveness = (windSpeedGustRelativeAmplitude == 0.0f)
        ? 0.0f
        : std::max(0.0f, windSpeedAbsoluteMagnitude - abs(wind.GetBaseSpeedMagnitude()))
          / abs(windSpeedGustRelativeAmplitude);

    float const windRipplesTimeFrequency = (gameParameters.WindSpeedBase >= 0.0f)
        ? 128.0f
        : -128.0f;

    float const smoothedWindNormalizedIncisiveness = mWindIncisivenessRunningAverage.Update(rawWindNormalizedIncisiveness);
    float const windRipplesWaveHeight = 0.7f * smoothedWindNormalizedIncisiveness;

    // sample index = 0
    float previousSampleValue;
    {
        float const c1 = sinf(waveTheta) * 0.5f;
        float const c2 = sinf(-waveTheta * 1.1f) * 0.3f;
        float const c3 = sinf(-currentSimulationTime * windRipplesTimeFrequency);
        previousSampleValue = (c1 + c2) * waveHeight + c3 * windRipplesWaveHeight;
        mSamples[0].SampleValue = previousSampleValue;
    }

    // sample index = 1...SamplesCount - 1
    float x = Dx;
    for (int64_t i = 1; i < SamplesCount; i++, x += Dx)
    {
        float const c1 = sinf(x * SpatialFrequency1 + waveTheta) * 0.5f;
        float const c2 = sinf(x * SpatialFrequency2 - waveTheta * 1.1f) * 0.3f;
        float const c3 = sinf(x * SpatialFrequency3 - currentSimulationTime * windRipplesTimeFrequency);
        float const sampleValue = (c1 + c2) * waveHeight + c3 * windRipplesWaveHeight;
        mSamples[i].SampleValue = sampleValue;
        mSamples[i - 1].SampleValuePlusOneMinusSampleValue = sampleValue - previousSampleValue;

        previousSampleValue = sampleValue;
    }

    // sample index = SamplesCount
    mSamples[SamplesCount - 1].SampleValuePlusOneMinusSampleValue = mSamples[0].SampleValue - previousSampleValue;
}

void WaterSurface::Upload(
    GameParameters const & gameParameters,
    Render::RenderContext & renderContext) const
{
    size_t constexpr SlicesCount = 500;

    float const visibleWorldWidth = renderContext.GetVisibleWorldWidth();
    float const sliceWidth = visibleWorldWidth / static_cast<float>(SlicesCount);
    float sliceX = renderContext.GetCameraWorldPosition().x - (visibleWorldWidth / 2.0f);

    renderContext.UploadOceanStart(SlicesCount);

    // We do one extra iteration as the number of slices is the number of quads, and the last vertical
    // quad side must be at the end of the width
    for (size_t i = 0; i <= SlicesCount; ++i, sliceX += sliceWidth)
    {
        renderContext.UploadOcean(
            sliceX,
            GetWaterHeightAt(sliceX),
            gameParameters.SeaDepth);
    }

    renderContext.UploadOceanEnd();
}

}