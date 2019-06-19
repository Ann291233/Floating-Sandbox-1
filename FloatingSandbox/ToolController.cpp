/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2018-06-16
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "ToolController.h"

#include <GameCore/Vectors.h>

#include <cassert>

ToolController::ToolController(
    ToolType initialToolType,
    wxFrame * parentFrame,
    std::shared_ptr<IGameController> gameController,
    std::shared_ptr<SoundController> soundController,
    ResourceLoader & resourceLoader)
    : mInputState()
    , mCurrentTool(nullptr)
    , mAllTools()
    , mParentFrame(parentFrame)
    , mPanCursor()
    , mGameController(gameController)
    , mSoundController(soundController)
{
    //
    // Initialize all tools
    //

    mAllTools.emplace_back(
        std::make_unique<MoveTool>(
            parentFrame,
            gameController,
            soundController,
            resourceLoader));

    mAllTools.emplace_back(
        std::make_unique<MoveAllTool>(
            parentFrame,
            gameController,
            soundController,
            resourceLoader));

    mAllTools.emplace_back(
        std::make_unique<SmashTool>(
            parentFrame,
            gameController,
            soundController,
            resourceLoader));

    mAllTools.emplace_back(
        std::make_unique<SawTool>(
            parentFrame,
            gameController,
            soundController,
            resourceLoader));

    mAllTools.emplace_back(
        std::make_unique<FlameThrowerTool>(
            parentFrame,
            gameController,
            soundController,
            resourceLoader));

    mAllTools.emplace_back(
        std::make_unique<GrabTool>(
            parentFrame,
            gameController,
            soundController,
            resourceLoader));

    mAllTools.emplace_back(
        std::make_unique<SwirlTool>(
            parentFrame,
            gameController,
            soundController,
            resourceLoader));

    mAllTools.emplace_back(
        std::make_unique<PinTool>(
            parentFrame,
            gameController,
            soundController,
            resourceLoader));

    mAllTools.emplace_back(
        std::make_unique<InjectAirBubblesTool>(
            parentFrame,
            gameController,
            soundController,
            resourceLoader));

    mAllTools.emplace_back(
        std::make_unique<FloodHoseTool>(
            parentFrame,
            gameController,
            soundController,
            resourceLoader));

    mAllTools.emplace_back(
        std::make_unique<AntiMatterBombTool>(
            parentFrame,
            gameController,
            soundController,
            resourceLoader));

    mAllTools.emplace_back(
        std::make_unique<ImpactBombTool>(
            parentFrame,
            gameController,
            soundController,
            resourceLoader));

    mAllTools.emplace_back(
        std::make_unique<RCBombTool>(
            parentFrame,
            gameController,
            soundController,
            resourceLoader));

    mAllTools.emplace_back(
        std::make_unique<TimerBombTool>(
            parentFrame,
            gameController,
            soundController,
            resourceLoader));

    mAllTools.emplace_back(
        std::make_unique<WaveMakerTool>(
            parentFrame,
            gameController,
            soundController,
            resourceLoader));

    mAllTools.emplace_back(
        std::make_unique<TerrainAdjustTool>(
            parentFrame,
            gameController,
            soundController,
            resourceLoader));

    mAllTools.emplace_back(
        std::make_unique<ScrubTool>(
            parentFrame,
            gameController,
            soundController,
            resourceLoader));

    mAllTools.emplace_back(
        std::make_unique<RepairStructureTool>(
            parentFrame,
            gameController,
            soundController,
            resourceLoader));

    // Prepare own cursor(s)
    mPanCursor = MakeCursor("pan_cursor", 15, 15, resourceLoader);

    // Set current tool
    this->SetTool(initialToolType);
}

void ToolController::OnMouseMove(
    int x,
    int y)
{
    // Update input state
    mInputState.PreviousMousePosition = mInputState.MousePosition;
    mInputState.MousePosition = vec2f(x, y);

    // Perform action
    if (mInputState.IsRightMouseDown)
    {
        // Perform our pan tool

        // Pan (opposite direction)
        vec2f screenOffset = mInputState.PreviousMousePosition - mInputState.MousePosition;
        mGameController->PanImmediate(screenOffset);
    }
    else
    {
        // Perform current tool's action
        if (nullptr != mCurrentTool)
        {
            mCurrentTool->OnMouseMove(mInputState);
        }
    }
}

void ToolController::OnLeftMouseDown()
{
    // Update input state
    mInputState.IsLeftMouseDown = true;

    // Perform current tool's action
    if (nullptr != mCurrentTool)
    {
        mCurrentTool->OnLeftMouseDown(mInputState);
    }
}

void ToolController::OnLeftMouseUp()
{
    // Update input state
    mInputState.IsLeftMouseDown = false;

    // Perform current tool's action
    if (nullptr != mCurrentTool)
    {
        mCurrentTool->OnLeftMouseUp(mInputState);
    }
}

void ToolController::OnRightMouseDown()
{
    // Update input state
    mInputState.IsRightMouseDown = true;

    // Show our pan cursor
    mParentFrame->SetCursor(*mPanCursor);
}

void ToolController::OnRightMouseUp()
{
    // Update input state
    mInputState.IsRightMouseDown = false;

    if (nullptr != mCurrentTool)
    {
        // Show tool's cursor again, since we moved out of Pan
        mCurrentTool->ShowCurrentCursor();
    }
}

void ToolController::OnShiftKeyDown()
{
    // Update input state
    mInputState.IsShiftKeyDown = true;

    // Perform current tool's action
    if (nullptr != mCurrentTool)
    {
        mCurrentTool->OnShiftKeyDown(mInputState);
    }
}

void ToolController::OnShiftKeyUp()
{
    // Update input state
    mInputState.IsShiftKeyDown = false;

    // Perform current tool's action
    if (nullptr != mCurrentTool)
    {
        mCurrentTool->OnShiftKeyUp(mInputState);
    }
}