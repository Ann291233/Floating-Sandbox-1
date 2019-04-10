/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2018-10-13
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#pragma once

#include "RenderContext.h"

#include <GameCore/GameTypes.h>

#include <chrono>
#include <string>
#include <vector>

class TextLayer
{
public:

    TextLayer(
        bool isStatusTextEnabled,
        bool isExtendedStatusTextEnabled);

    void SetStatusTextEnabled(bool isEnabled);

    void SetExtendedStatusTextEnabled(bool isEnabled);

    void SetStatusText(
        float immediateFps,
        float averageFps,
        std::chrono::duration<float> elapsedGameSeconds,
        bool isPaused,
        float zoom,
        vec2f const & camera,
        float totalUpdateToRenderDurationRatio,
        float lastUpdateToRenderDurationRatio,
        Render::RenderStatistics const & renderStatistics);

    void Update();

    void Render(Render::RenderContext & renderContext);

private:

    //
    // Current state
    //

    bool mIsStatusTextEnabled;
    bool mIsExtendedStatusTextEnabled;
    std::vector<std::string> mStatusTextLines;
    RenderedTextHandle mStatusTextHandle;
    bool mIsStatusTextDirty;
};

