/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2021-08-28
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "Controller.h"

#include "Tools/FloodTool.h"
#include "Tools/LineTool.h"
#include "Tools/MeasuringTapeTool.h"
#include "Tools/PencilTool.h"
#include "Tools/RopeEraserTool.h"
#include "Tools/RopePencilTool.h"
#include "Tools/SamplerTool.h"
#include "Tools/SelectionTool.h"
#include "Tools/TextureEraserTool.h"
#include "Tools/TextureMagicWandTool.h"

#include <Game/ImageFileTools.h>

#include <cassert>

namespace ShipBuilder {

std::unique_ptr<Controller> Controller::CreateNew(
    std::string const & shipName,
    OpenGLManager & openGLManager,
    WorkbenchState & workbenchState,
    IUserInterface & userInterface,
    ShipTexturizer const & shipTexturizer,
    ResourceLocator const & resourceLocator)
{
    auto modelController = ModelController::CreateNew(
        workbenchState.GetNewShipSize(),
        shipName,
        shipTexturizer);

    std::unique_ptr<Controller> controller = std::unique_ptr<Controller>(
        new Controller(
            std::move(modelController),
            openGLManager,
            workbenchState,
            userInterface,
            resourceLocator));

    return controller;
}

std::unique_ptr<Controller> Controller::CreateForShip(
    ShipDefinition && shipDefinition,
    OpenGLManager & openGLManager,
    WorkbenchState & workbenchState,
    IUserInterface & userInterface,
    ShipTexturizer const & shipTexturizer,
    ResourceLocator const & resourceLocator)
{
    auto modelController = ModelController::CreateForShip(
        std::move(shipDefinition),
        shipTexturizer);

    std::unique_ptr<Controller> controller = std::unique_ptr<Controller>(
        new Controller(
            std::move(modelController),
            openGLManager,
            workbenchState,
            userInterface,
            resourceLocator));

    return controller;
}

Controller::Controller(
    std::unique_ptr<ModelController> modelController,
    OpenGLManager & openGLManager,
    WorkbenchState & workbenchState,
    IUserInterface & userInterface,
    ResourceLocator const & resourceLocator)
    : mView()
    , mModelController(std::move(modelController))
    , mUndoStack()
    , mSelectionManager(userInterface)
    , mWorkbenchState(workbenchState)
    , mUserInterface(userInterface)
    , mResourceLocator(resourceLocator)
    // State
    , mCurrentTool()
    , mLastToolTypePerLayer({ToolType::StructuralPencil, ToolType::ElectricalPencil, ToolType::RopePencil, std::nullopt})
{
    // We assume we start with at least a structural layer
    assert(mModelController->HasLayer(LayerType::Structural));

    //
    // Create view
    //

    mView = std::make_unique<View>(
        mModelController->GetShipSize(),
        mWorkbenchState.GetCanvasBackgroundColor(),
        mWorkbenchState.GetPrimaryVisualization(),
        mWorkbenchState.GetOtherVisualizationsOpacity(),
        mWorkbenchState.IsGridEnabled(),
        mUserInterface.GetDisplaySize(),
        mUserInterface.GetLogicalToPhysicalPixelFactor(),
        openGLManager,
        [this]()
        {
            mUserInterface.SwapRenderBuffers();
        },
        mResourceLocator);

    mView->UploadBackgroundTexture(
        ImageFileTools::LoadImageRgba(
            mResourceLocator.GetBitmapFilePath("shipbuilder_background")));

    // Set ideal zoom
    mView->SetZoom(mView->CalculateIdealZoom());

    //
    // Sync with UI
    //

    mUserInterface.OnViewModelChanged(mView->GetViewModel());
    mUserInterface.OnShipNameChanged(*mModelController);
    mUserInterface.OnShipScaleChanged(mModelController->GetShipMetadata().Scale);
    mUserInterface.OnShipSizeChanged(mModelController->GetShipSize());
    mUserInterface.OnLayerPresenceChanged(*mModelController);
    mUserInterface.OnModelDirtyChanged(*mModelController);
    mUserInterface.OnElectricalLayerInstancedElementSetChanged(mModelController->GetInstancedElectricalElementSet());
    mUserInterface.OnUndoStackStateChanged(mUndoStack);
    mUserInterface.OnSelectionChanged(mSelectionManager.GetSelection());

    //
    // Initialize visualization
    //

    // Switch primary viz to default if it's not compatible with current layer presence
    if (!mModelController->HasLayer(VisualizationToLayer(mWorkbenchState.GetPrimaryVisualization())))
    {
        InternalSelectPrimaryVisualization(WorkbenchState::GetDefaultPrimaryVisualization()); // Will also change tool
    }

    // Initialize layer visualizations
    InternalReconciliateTextureVisualizationMode();
    InternalUpdateModelControllerVisualizationModes();

    // Upload layers' visualizations
    mModelController->UpdateVisualizations(*mView);

    // Notify macro properties
    NotifyModelMacroPropertiesUpdated();

    // Refresh view
    mUserInterface.RefreshView();

    //
    // Create tool - iff mouse is (logically) in work canvas at this very moment
    //

    if (mUserInterface.IsMouseInWorkCanvas())
    {
        if (mWorkbenchState.GetCurrentToolType().has_value())
        {
            mCurrentTool = MakeTool(*mWorkbenchState.GetCurrentToolType());
        }
    }

    RefreshToolCoordinatesDisplay();
}

ShipDefinition Controller::MakeShipDefinition() const
{
    auto const scopedToolResumeState = SuspendTool();

    assert(mModelController);
    assert(!mModelController->IsInEphemeralVisualization());

    return mModelController->MakeShipDefinition();
}

void Controller::SetShipProperties(
    std::optional<ShipMetadata> && metadata,
    std::optional<ShipPhysicsData> && physicsData,
    std::optional<std::optional<ShipAutoTexturizationSettings>> && autoTexturizationSettings)
{
    assert(mModelController);

    // Assuming at least one of the three was changed
    assert(metadata.has_value() || physicsData.has_value() || autoTexturizationSettings.has_value());

    //
    // Prepare undo entry
    //

    auto f =
        [oldMetadata = mModelController->GetShipMetadata()
        , oldPhysicsData = mModelController->GetShipPhysicsData()
        , oldAutoTexturizationSettings = mModelController->GetShipAutoTexturizationSettings()]
        (Controller & controller) mutable
        {
            controller.RestoreShipPropertiesForUndo(
                std::move(oldMetadata),
                std::move(oldPhysicsData),
                std::move(oldAutoTexturizationSettings));
        };

    auto originalDirtyState = mModelController->GetDirtyState();

    //
    // Set new properties
    //

    InternalSetShipProperties(
        std::move(metadata),
        std::move(physicsData),
        std::move(autoTexturizationSettings));

    // At least one of the three was changed
    mUserInterface.OnModelDirtyChanged(*mModelController);

    //
    // Store undo action
    //

    mUndoStack.Push(
        _("Properties"),
        256, // Arbitrary cost
        originalDirtyState,
        std::move(f));

    mUserInterface.OnUndoStackStateChanged(mUndoStack);
}

void Controller::RestoreShipPropertiesForUndo(
    std::optional<ShipMetadata> && metadata,
    std::optional<ShipPhysicsData> && physicsData,
    std::optional<std::optional<ShipAutoTexturizationSettings>> && autoTexturizationSettings)
{
    assert(mModelController);

    InternalSetShipProperties(
        std::move(metadata),
        std::move(physicsData),
        std::move(autoTexturizationSettings));

    // At least one of the three was changed
    mUserInterface.OnModelDirtyChanged(*mModelController);
}

void Controller::SetElectricalPanelMetadata(ElectricalPanelMetadata panelMetadata)
{
    assert(mModelController);

    //
    // Prepare undo entry
    //

    auto f = [oldPanelMetadata = mModelController->GetElectricalPanelMetadata()] (Controller & controller) mutable
    {
        controller.RestoreElectricalPanelMetadataForUndo(std::move(oldPanelMetadata));
    };

    auto originalDirtyState = mModelController->GetDirtyState();

    //
    // Set new panel
    //

    InternalSetElectricalPanelMetadata(std::move(panelMetadata));

    mModelController->SetLayerDirty(LayerType::Electrical);
    mUserInterface.OnModelDirtyChanged(*mModelController);

    //
    // Store undo action
    //

    mUndoStack.Push(
        _("Electrical Panel"),
        256, // Arbitrary cost
        originalDirtyState,
        std::move(f));

    mUserInterface.OnUndoStackStateChanged(mUndoStack);
}

void Controller::RestoreElectricalPanelMetadataForUndo(ElectricalPanelMetadata && panelMetadata)
{
    assert(mModelController);

    InternalSetElectricalPanelMetadata(std::move(panelMetadata));

    mUserInterface.OnModelDirtyChanged(*mModelController);
}

void Controller::ClearModelDirty()
{
    mModelController->ClearIsDirty();
    mUserInterface.OnModelDirtyChanged(*mModelController);
}

void Controller::RestoreDirtyState(ModelDirtyState && dirtyState)
{
    // Restore dirtyness
    mModelController->RestoreDirtyState(dirtyState);
    mUserInterface.OnModelDirtyChanged(*mModelController);
}

ModelValidationResults Controller::ValidateModel()
{
    auto const scopedToolResumeState = SuspendTool();

    assert(mModelController);
    assert(!mModelController->IsInEphemeralVisualization());

    return mModelController->ValidateModel();
}

std::unique_ptr<RgbaImageData> Controller::MakePreview() const
{
    auto const scopedToolResumeState = SuspendTool();

    return mModelController->MakePreview();
}

std::optional<ShipSpaceRect> Controller::CalculateBoundingBox() const
{
    auto const scopedToolResumeState = SuspendTool();

    return mModelController->CalculateBoundingBox();
}

void Controller::NewStructuralLayer()
{
    StructuralLayerData newStructuralLayer(
        mModelController->GetShipSize(),
        StructuralElement(nullptr)); // No material

    InternalSetLayer<LayerType::Structural>(
        _("New Structural Layer"),
        std::move(newStructuralLayer));
}

void Controller::SetStructuralLayer(
    wxString actionTitle,
    StructuralLayerData && structuralLayer)
{
    InternalSetLayer<LayerType::Structural>(
        actionTitle,
        std::move(structuralLayer));
}

void Controller::RestoreStructuralLayerRegionForUndo(
    StructuralLayerData && layerRegion,
    ShipSpaceCoordinates const & origin)
{
    auto const scopedToolResumeState = SuspendTool();

    ShipSpaceRect const regionRect({ {0, 0}, layerRegion.Buffer.Size });

    mModelController->RestoreStructuralLayerRegion(
        std::move(layerRegion),
        regionRect,
        origin);

    // No need to update dirtyness, this is for undo

    // Notify macro properties
    NotifyModelMacroPropertiesUpdated();

    // Refresh model visualization
    mModelController->UpdateVisualizations(*mView);
    mUserInterface.RefreshView();
}

void Controller::RestoreStructuralLayerForUndo(StructuralLayerData && structuralLayer)
{
    auto const scopedToolResumeState = SuspendTool();

    mModelController->RestoreStructuralLayer(std::move(structuralLayer));

    // No need to update dirtyness, this is for undo

    // Notify macro properties
    NotifyModelMacroPropertiesUpdated();

    // Refresh model visualizations
    mModelController->UpdateVisualizations(*mView);
    mUserInterface.RefreshView();
}

void Controller::NewElectricalLayer()
{
    ElectricalLayerData newElectricalLayer(
        mModelController->GetShipSize(),
        ElectricalElement(nullptr, NoneElectricalElementInstanceIndex)); // No material

    InternalSetLayer<LayerType::Electrical>(
        _("New Electrical Layer"),
        std::move(newElectricalLayer));
}

void Controller::SetElectricalLayer(
    wxString actionTitle,
    ElectricalLayerData && electricalLayer)
{
    InternalSetLayer<LayerType::Electrical>(
        actionTitle,
        std::move(electricalLayer));
}

void Controller::RemoveElectricalLayer()
{
    InternalRemoveLayer<LayerType::Electrical>();
}

void Controller::RestoreElectricalLayerRegionForUndo(
    ElectricalLayerData && layerRegion,
    ShipSpaceCoordinates const & origin)
{
    auto const scopedToolResumeState = SuspendTool();

    mModelController->RestoreElectricalLayerRegion(
        std::move(layerRegion),
        { {0, 0}, layerRegion.Buffer.Size },
        origin);

    // No need to update dirtyness, this is for undo

    // Notify macro properties
    NotifyModelMacroPropertiesUpdated();

    // Notify of (possible) change in electrical panel
    mUserInterface.OnElectricalLayerInstancedElementSetChanged(mModelController->GetInstancedElectricalElementSet());

    // Refresh model visualizations
    mModelController->UpdateVisualizations(*mView);
    mUserInterface.RefreshView();
}

void Controller::RestoreElectricalLayerForUndo(std::unique_ptr<ElectricalLayerData> electricalLayer)
{
    auto const scopedToolResumeState = SuspendTool();

    WrapLikelyLayerPresenceChangingOperation<LayerType::Electrical>(
        [this, electricalLayer = std::move(electricalLayer)]() mutable
        {
            mModelController->RestoreElectricalLayer(std::move(electricalLayer));
        });

    // No need to update dirtyness, this is for undo

    // Update visualization modes
    InternalUpdateModelControllerVisualizationModes();

    // Notify macro properties
    NotifyModelMacroPropertiesUpdated();

    // Notify of (possible) change in electrical panel
    mUserInterface.OnElectricalLayerInstancedElementSetChanged(mModelController->GetInstancedElectricalElementSet());

    // Refresh model visualizations
    mModelController->UpdateVisualizations(*mView);
    mUserInterface.RefreshView();
}

void Controller::TrimElectricalParticlesWithoutSubstratum()
{
    auto const scopedToolResumeState = SuspendTool();

    // Trim
    {
        // Save state
        auto originalDirtyStateClone = mModelController->GetDirtyState();
        auto originalLayerClone = mModelController->CloneExistingLayer<LayerType::Electrical>();

        // Trim
        auto const affectedRect = mModelController->TrimElectricalParticlesWithoutSubstratum();

        if (affectedRect.has_value())
        {
            // Create undo action

            ElectricalLayerData clippedRegionClone = originalLayerClone.Clone(*affectedRect);
            auto const clipByteSize = clippedRegionClone.Buffer.GetByteSize();

            mUndoStack.Push(
                _("Trim Electrical"),
                clipByteSize,
                originalDirtyStateClone,
                [clippedRegionClone = std::move(clippedRegionClone), origin = affectedRect->origin](Controller & controller) mutable
                {
                    controller.RestoreElectricalLayerRegionForUndo(std::move(clippedRegionClone), origin);
                });

            mUserInterface.OnUndoStackStateChanged(mUndoStack);

            LayerChangeEpilog(LayerType::Electrical);
        }
    }
}

void Controller::NewRopesLayer()
{
    RopesLayerData newRopesLayer;

    InternalSetLayer<LayerType::Ropes>(
        _("New Ropes Layer"),
        std::move(newRopesLayer));
}

void Controller::SetRopesLayer(
    wxString actionTitle,
    RopesLayerData && ropesLayer)
{
    InternalSetLayer<LayerType::Ropes>(
        actionTitle,
        std::move(ropesLayer));
}

void Controller::RemoveRopesLayer()
{
    InternalRemoveLayer<LayerType::Ropes>();
}

void Controller::RestoreRopesLayerForUndo(std::unique_ptr<RopesLayerData> ropesLayer)
{
    auto const scopedToolResumeState = SuspendTool();

    WrapLikelyLayerPresenceChangingOperation<LayerType::Ropes>(
        [this, ropesLayer = std::move(ropesLayer)]() mutable
        {
            mModelController->RestoreRopesLayer(std::move(ropesLayer));
        });

    // No need to update dirtyness, this is for undo

    // Update visualization modes
    InternalUpdateModelControllerVisualizationModes();

    // Notify macro properties
    NotifyModelMacroPropertiesUpdated();

    // Refresh model visualizations
    mModelController->UpdateVisualizations(*mView);
    mUserInterface.RefreshView();
}

void Controller::SetTextureLayer(
    wxString actionTitle,
    TextureLayerData && textureLayer,
    std::optional<std::string> textureArtCredits)
{
    InternalSetLayer<LayerType::Texture>(
        actionTitle,
        std::move(textureLayer),
        std::move(textureArtCredits));
}

void Controller::RemoveTextureLayer()
{
    InternalRemoveLayer<LayerType::Texture>();
}

void Controller::RestoreTextureLayerRegionForUndo(
    TextureLayerData && layerRegion,
    ImageCoordinates const & origin)
{
    auto const scopedToolResumeState = SuspendTool();

    mModelController->RestoreTextureLayerRegion(
        std::move(layerRegion),
        { {0, 0}, layerRegion.Buffer.Size },
        origin);

    // No need to update dirtyness, this is for undo

    // Notify macro properties
    NotifyModelMacroPropertiesUpdated();

    // Refresh model visualizations
    mModelController->UpdateVisualizations(*mView);
    mUserInterface.RefreshView();
}

void Controller::RestoreTextureLayerForUndo(
    std::unique_ptr<TextureLayerData> textureLayer,
    std::optional<std::string> originalTextureArtCredits)
{
    auto const scopedToolResumeState = SuspendTool();

    WrapLikelyLayerPresenceChangingOperation<LayerType::Texture>(
        [this, textureLayer = std::move(textureLayer), originalTextureArtCredits = std::move(originalTextureArtCredits)]() mutable
        {
            mModelController->RestoreTextureLayer(
                std::move(textureLayer),
                std::move(originalTextureArtCredits));
        });

    // No need to update dirtyness, this is for undo

    // Update visualization modes
    InternalUpdateModelControllerVisualizationModes();

    // Refresh model visualizations
    mModelController->UpdateVisualizations(*mView);
    mUserInterface.RefreshView();
}

void Controller::RestoreAllLayersForUndo(
    ShipSpaceSize const & shipSize,
    StructuralLayerData && structuralLayer,
    std::unique_ptr<ElectricalLayerData> electricalLayer,
    std::unique_ptr<RopesLayerData> ropesLayer,
    std::unique_ptr<TextureLayerData> textureLayer,
    std::optional<std::string> originalTextureArtCredits)
{
    auto const scopedToolResumeState = SuspendTool();

    //
    // Model
    //

    mModelController->SetShipSize(shipSize);

    mModelController->RestoreStructuralLayer(std::move(structuralLayer));

    WrapLikelyLayerPresenceChangingOperation<LayerType::Electrical>(
        [this, electricalLayer = std::move(electricalLayer)]() mutable
        {
            mModelController->RestoreElectricalLayer(std::move(electricalLayer));
        });

    WrapLikelyLayerPresenceChangingOperation<LayerType::Ropes>(
        [this, ropesLayer = std::move(ropesLayer)]() mutable
        {
            mModelController->RestoreRopesLayer(std::move(ropesLayer));
        });

    WrapLikelyLayerPresenceChangingOperation<LayerType::Texture>(
        [this, textureLayer = std::move(textureLayer), originalTextureArtCredits = std::move(originalTextureArtCredits)]() mutable
        {
            mModelController->RestoreTextureLayer(
                std::move(textureLayer),
                std::move(originalTextureArtCredits));
        });

    //
    // Finalize
    //

    // No need to update dirtyness, this is for undo

    // Notify view of (possibly) new size
    mView->SetShipSize(shipSize);
    mUserInterface.OnViewModelChanged(mView->GetViewModel());

    // Notify UI of (possibly) new ship size
    mUserInterface.OnShipSizeChanged(shipSize);

    // Update visualization modes
    InternalUpdateModelControllerVisualizationModes();

    // Notify macro properties
    NotifyModelMacroPropertiesUpdated();

    // Refresh model visualizations
    mModelController->UpdateVisualizations(*mView);
    mUserInterface.RefreshView();
}

void Controller::AutoTrim()
{
    auto const scopedToolResumeState = SuspendTool();

    std::optional<ShipSpaceRect> const boundingRect = mModelController->CalculateBoundingBox();

    if (boundingRect.has_value())
    {
        InternalResizeShip(
            boundingRect->size,
            ShipSpaceCoordinates(
                -boundingRect->origin.x,
                -boundingRect->origin.y),
            _("Trim"));
    }
}

void Controller::Flip(DirectionType direction)
{
    auto const scopedToolResumeState = SuspendTool();

    InternalFlip<false>(direction);
}

void Controller::FlipForUndo(DirectionType direction)
{
    auto const scopedToolResumeState = SuspendTool();

    InternalFlip<true>(direction);
}

void Controller::Rotate90(RotationDirectionType direction)
{
    auto const scopedToolResumeState = SuspendTool();

    InternalRotate90<false>(direction);
}

void Controller::Rotate90ForUndo(RotationDirectionType direction)
{
    auto const scopedToolResumeState = SuspendTool();

    InternalRotate90<true>(direction);
}

void Controller::ResizeShip(
    ShipSpaceSize const & newSize,
    ShipSpaceCoordinates const & originOffset)
{
    auto const scopedToolResumeState = SuspendTool();

    InternalResizeShip(
        newSize,
        originOffset,
        _("Resize Ship"));
}

void Controller::LayerChangeEpilog(std::optional<LayerType> dirtyLayer)
{
    if (dirtyLayer.has_value())
    {
        // Mark layer as dirty
        mModelController->SetLayerDirty(*dirtyLayer);
        mUserInterface.OnModelDirtyChanged(*mModelController);

        if (*dirtyLayer == LayerType::Electrical)
        {
            // Notify of (possible) change in electrical panel
            mUserInterface.OnElectricalLayerInstancedElementSetChanged(mModelController->GetInstancedElectricalElementSet());
        }

        // Notify macro properties
        NotifyModelMacroPropertiesUpdated();
    }

    // Refresh visualization
    assert(mView);
    mModelController->UpdateVisualizations(*mView);
    mUserInterface.RefreshView();
}

void Controller::SelectPrimaryVisualization(VisualizationType primaryVisualization)
{
    if (primaryVisualization != mWorkbenchState.GetPrimaryVisualization())
    {
        {
            auto const scopedToolResumeState = SuspendTool();

            InternalSelectPrimaryVisualization(primaryVisualization);
        }

        // Refresh view
        mUserInterface.RefreshView();
    }
}

void Controller::SetGameVisualizationMode(GameVisualizationModeType mode)
{
    mWorkbenchState.SetGameVisualizationMode(mode);

    // Notify
    mUserInterface.OnGameVisualizationModeChanged(mode);

    // Update visualization modes
    InternalUpdateModelControllerVisualizationModes();

    // Refresh model visualizations
    mModelController->UpdateVisualizations(*mView);
    mUserInterface.RefreshView();
}

void Controller::SetStructuralLayerVisualizationMode(StructuralLayerVisualizationModeType mode)
{
    mWorkbenchState.SetStructuralLayerVisualizationMode(mode);

    // Notify
    mUserInterface.OnStructuralLayerVisualizationModeChanged(mode);

    // Update visualization modes
    InternalUpdateModelControllerVisualizationModes();

    // Refresh model visualizations
    mModelController->UpdateVisualizations(*mView);
    mUserInterface.RefreshView();
}

void Controller::SetElectricalLayerVisualizationMode(ElectricalLayerVisualizationModeType mode)
{
    mWorkbenchState.SetElectricalLayerVisualizationMode(mode);

    // Notify
    mUserInterface.OnElectricalLayerVisualizationModeChanged(mode);

    // Update visualization modes
    InternalUpdateModelControllerVisualizationModes();

    // Refresh model visualizations
    mModelController->UpdateVisualizations(*mView);
    mUserInterface.RefreshView();
}

void Controller::SetRopesLayerVisualizationMode(RopesLayerVisualizationModeType mode)
{
    mWorkbenchState.SetRopesLayerVisualizationMode(mode);

    // Notify
    mUserInterface.OnRopesLayerVisualizationModeChanged(mode);

    // Update visualization modes
    InternalUpdateModelControllerVisualizationModes();

    // Refresh model visualizations
    mModelController->UpdateVisualizations(*mView);
    mUserInterface.RefreshView();
}

void Controller::SetTextureLayerVisualizationMode(TextureLayerVisualizationModeType mode)
{
    mWorkbenchState.SetTextureLayerVisualizationMode(mode);

    // Notify
    mUserInterface.OnTextureLayerVisualizationModeChanged(mode);

    // Update visualization modes
    InternalUpdateModelControllerVisualizationModes();

    // Refresh model visualizations
    mModelController->UpdateVisualizations(*mView);
    mUserInterface.RefreshView();
}

void Controller::SetOtherVisualizationsOpacity(float opacity)
{
    mWorkbenchState.SetOtherVisualizationsOpacity(opacity);

    // Notify
    mUserInterface.OnOtherVisualizationsOpacityChanged(opacity);

    // Update view
    mView->SetOtherVisualizationsOpacity(opacity);
    mUserInterface.RefreshView();
}

void Controller::EnableWaterlineMarkers(bool doEnable)
{
    // Storage
    mWorkbenchState.EnableWaterlineMarkers(doEnable);

    // Notify UI
    mUserInterface.OnVisualWaterlineMarkersEnablementChanged(doEnable);

    // Upload markers
    auto const modelMacroProperties = mModelController->GetModelMacroProperties();
    if (mWorkbenchState.IsWaterlineMarkersEnabled() && modelMacroProperties.CenterOfMass.has_value())
    {
        mView->UploadWaterlineMarker(
            *modelMacroProperties.CenterOfMass,
            View::WaterlineMarkerType::CenterOfMass);
    }
    else
    {
        mView->RemoveWaterlineMarker(View::WaterlineMarkerType::CenterOfMass);
    }

    mUserInterface.RefreshView();
}

void Controller::EnableVisualGrid(bool doEnable)
{
    mWorkbenchState.EnableGrid(doEnable);

    // Notify
    mUserInterface.OnVisualGridEnablementChanged(doEnable);

    // Update view
    mView->EnableVisualGrid(doEnable);
    mUserInterface.RefreshView();
}

void Controller::TryUndoLast()
{
    if (!mUndoStack.IsEmpty())
    {
        UndoLast();
    }
}

void Controller::UndoLast()
{
    auto const scopedToolResumeState = SuspendTool();

    // Apply action
    mUndoStack.PopAndApply(*this);

    // Update undo state
    mUserInterface.OnUndoStackStateChanged(mUndoStack);
}

void Controller::UndoUntil(size_t index)
{
    auto const scopedToolResumeState = SuspendTool();

    // Apply actions
    mUndoStack.RewindAndApply(index, *this);

    // Update undo state
    mUserInterface.OnUndoStackStateChanged(mUndoStack);
}

void Controller::Render()
{
    mView->Render();
}

void Controller::AddZoom(int deltaZoom)
{
    mView->SetZoom(mView->GetZoom() + deltaZoom);

    // Tell tool about the new mouse (ship space) position, but only
    // if the mouse is in the canvas
    auto const mouseCoordinates = mUserInterface.GetMouseCoordinatesIfInWorkCanvas();
    if (mouseCoordinates && mCurrentTool)
    {
        mCurrentTool->OnMouseMove(*mouseCoordinates);
    }

    RefreshToolCoordinatesDisplay();
    mUserInterface.OnViewModelChanged(mView->GetViewModel());
    mUserInterface.RefreshView();
}

void Controller::SetCamera(int camX, int camY)
{
    mView->SetCameraShipSpacePosition(ShipSpaceCoordinates(camX, camY));

    // Tell tool about the new mouse (ship space) position, but only
    // if the mouse is in the canvas
    auto const mouseCoordinates = mUserInterface.GetMouseCoordinatesIfInWorkCanvas();
    if (mouseCoordinates
        && mCurrentTool)
    {
        mCurrentTool->OnMouseMove(*mouseCoordinates);
    }

    RefreshToolCoordinatesDisplay();
    mUserInterface.OnViewModelChanged(mView->GetViewModel());
    mUserInterface.RefreshView();
}

void Controller::ResetView()
{
    mView->SetZoom(0);
    mView->SetCameraShipSpacePosition(ShipSpaceCoordinates(0, 0));

    // Tell tool about the new mouse (ship space) position, but only
    // if the mouse is in the canvas
    auto const mouseCoordinates = mUserInterface.GetMouseCoordinatesIfInWorkCanvas();
    if (mouseCoordinates
        && mCurrentTool)
    {
        mCurrentTool->OnMouseMove(*mouseCoordinates);
    }

    RefreshToolCoordinatesDisplay();
    mUserInterface.OnViewModelChanged(mView->GetViewModel());
    mUserInterface.RefreshView();
}

void Controller::OnWorkCanvasResized(DisplayLogicalSize const & newSize)
{
    // Tell view
    mView->SetDisplayLogicalSize(newSize);

    // Tell tool about the new mouse (ship space) position, but only
    // if the mouse is in the canvas
    auto const mouseCoordinates = mUserInterface.GetMouseCoordinatesIfInWorkCanvas();
    if (mouseCoordinates
        && mCurrentTool)
    {
        mCurrentTool->OnMouseMove(*mouseCoordinates);
    }

    // Tell UI
    mUserInterface.OnViewModelChanged(mView->GetViewModel());
}

void Controller::BroadcastSampledInformationUpdatedAt(std::optional<ShipSpaceCoordinates> const & coordinates, LayerType layer) const
{
    assert(mModelController);

    std::optional<SampledInformation> sampledInformation;

    if (coordinates.has_value()
        && coordinates->IsInSize(mModelController->GetShipSize()))
    {
        sampledInformation = mModelController->SampleInformationAt(*coordinates, layer);
    }

    mUserInterface.OnSampledInformationUpdated(sampledInformation);
}

void Controller::BroadcastSampledInformationUpdatedNone() const
{
    mUserInterface.OnSampledInformationUpdated(std::nullopt);
}

void Controller::SetCurrentTool(std::optional<ToolType> tool)
{
    if (tool != mWorkbenchState.GetCurrentToolType())
    {
        bool const hadTool = (mCurrentTool != nullptr);

        // Nuke current tool
        mWorkbenchState.SetCurrentToolType(std::nullopt);
        mCurrentTool.reset();

        InternalSetCurrentTool(tool);

        // Make new tool - unless we are suspended
        assert(mWorkbenchState.GetCurrentToolType() == tool);
        if (mWorkbenchState.GetCurrentToolType().has_value()
            && hadTool)
        {
            mCurrentTool = MakeTool(*mWorkbenchState.GetCurrentToolType());
        }
    }
}

void Controller::SetNewShipSize(ShipSpaceSize size)
{
    mWorkbenchState.SetNewShipSize(size);
}

void Controller::SetCanvasBackgroundColor(rgbColor const & color)
{
    mWorkbenchState.SetCanvasBackgroundColor(color);
    mView->SetCanvasBackgroundColor(color);
    mUserInterface.RefreshView();
}

void Controller::SetStructuralMaterial(StructuralMaterial const * material, MaterialPlaneType plane)
{
    mWorkbenchState.SetStructuralMaterial(material, plane);
    mUserInterface.OnStructuralMaterialChanged(material, plane);
}

void Controller::SetElectricalMaterial(ElectricalMaterial const * material, MaterialPlaneType plane)
{
    mWorkbenchState.SetElectricalMaterial(material, plane);
    mUserInterface.OnElectricalMaterialChanged(material, plane);
}

void Controller::SetRopeMaterial(StructuralMaterial const * material, MaterialPlaneType plane)
{
    mWorkbenchState.SetRopesMaterial(material, plane);
    mUserInterface.OnRopesMaterialChanged(material, plane);
}

void Controller::OnMouseMove(DisplayLogicalCoordinates const & mouseCoordinates)
{
    // Forward to tool
    if (mCurrentTool)
    {
        mCurrentTool->OnMouseMove(mouseCoordinates);
    }

    RefreshToolCoordinatesDisplay();
}

void Controller::OnLeftMouseDown()
{
    // Forward to tool
    if (mCurrentTool)
    {
        mCurrentTool->OnLeftMouseDown();
    }
}

void Controller::OnLeftMouseUp()
{
    // Forward to tool
    if (mCurrentTool)
    {
        mCurrentTool->OnLeftMouseUp();
    }
}

void Controller::OnRightMouseDown()
{
    // Forward to tool
    if (mCurrentTool)
    {
        mCurrentTool->OnRightMouseDown();
    }
}

void Controller::OnRightMouseUp()
{
    // Forward to tool
    if (mCurrentTool)
    {
        mCurrentTool->OnRightMouseUp();
    }
}

void Controller::OnShiftKeyDown()
{
    // Forward to tool
    if (mCurrentTool)
    {
        mCurrentTool->OnShiftKeyDown();
    }
}

void Controller::OnShiftKeyUp()
{
    // Forward to tool
    if (mCurrentTool)
    {
        mCurrentTool->OnShiftKeyUp();
    }
}

void Controller::OnUncapturedMouseIn()
{
    InternalResumeTool();
}

void Controller::OnUncapturedMouseOut()
{
    InternalSuspendTool();

    // Tell UI
    mUserInterface.OnToolCoordinatesChanged(std::nullopt, mModelController->GetShipSize());
}

void Controller::OnMouseCaptureLost()
{
    // Reset tool
    InternalResetTool();
}

///////////////////////////////////////////////////////////////////////////////

template<LayerType TLayerType, typename ... TArgs>
void Controller::InternalSetLayer(wxString actionTitle, TArgs&& ... args)
{
    auto const scopedToolResumeState = SuspendTool();

    //
    // Do layer-specific work
    //

    std::optional<VisualizationType> newVisualizationType;

    if constexpr (TLayerType == LayerType::Electrical)
    {
        // Create undo action
        InternalPushUndoForWholeLayer<LayerType::Electrical>(actionTitle);

        // Switch visualization mode to this new one, if needed
        if (mWorkbenchState.GetPrimaryVisualization() != VisualizationType::ElectricalLayer)
        {
            newVisualizationType = VisualizationType::ElectricalLayer;
        }

        // Set layer
        WrapLikelyLayerPresenceChangingOperation<TLayerType>(
            [this, args = std::make_tuple(std::forward<TArgs>(args)...)]() mutable
            {
                std::apply(
                    [this](auto&& ... args)
                    {
                        mModelController->SetElectricalLayer(std::forward<TArgs>(args)...);
                    },
                    std::move(args));
            });
    }
    else if constexpr (TLayerType == LayerType::Ropes)
    {
        // Create undo action
        InternalPushUndoForWholeLayer<LayerType::Ropes>(actionTitle);

        // Switch visualization mode to this new one, if needed
        if (mWorkbenchState.GetPrimaryVisualization() != VisualizationType::RopesLayer)
        {
            newVisualizationType = VisualizationType::RopesLayer;
        }

        // Set layer
        WrapLikelyLayerPresenceChangingOperation<TLayerType>(
            [this, args = std::make_tuple(std::forward<TArgs>(args)...)]() mutable
            {
                std::apply(
                    [this](auto&& ... args)
                    {
                        mModelController->SetRopesLayer(std::forward<TArgs>(args)...);
                    },
                    std::move(args));
            });
    }
    else if constexpr (TLayerType == LayerType::Structural)
    {
        // Create undo action
        InternalPushUndoForWholeLayer<LayerType::Structural>(actionTitle);

        // Switch visualization mode to this new one, if needed
        if (mWorkbenchState.GetPrimaryVisualization() != VisualizationType::StructuralLayer)
        {
            newVisualizationType = VisualizationType::StructuralLayer;
        }

        // Set layer
        WrapLikelyLayerPresenceChangingOperation<TLayerType>(
            [this, args = std::make_tuple(std::forward<TArgs>(args)...)]() mutable
            {
                std::apply(
                    [this](auto&& ... args)
                    {
                        mModelController->SetStructuralLayer(std::forward<TArgs>(args)...);
                    },
                    std::move(args));
            });
    }
    else
    {
        static_assert(TLayerType == LayerType::Texture);

        // Create undo action
        InternalPushUndoForWholeLayer<LayerType::Texture>(actionTitle);

        // Switch visualization mode to this new one, if needed
        if (mWorkbenchState.GetPrimaryVisualization() != VisualizationType::TextureLayer)
        {
            newVisualizationType = VisualizationType::TextureLayer;
        }

        // Set layer
        WrapLikelyLayerPresenceChangingOperation<TLayerType>(
            [this, args = std::make_tuple(std::forward<TArgs>(args)...)]() mutable
            {
                std::apply(
                    [this](auto&& ... args)
                    {
                        mModelController->SetTextureLayer(std::forward<TArgs>(args)...);
                    },
                    std::move(args));
            });
    }

    // Switch primary viz
    if (newVisualizationType.has_value())
    {
        InternalSelectPrimaryVisualization(*newVisualizationType);
    }

    // Update visualization modes
    InternalUpdateModelControllerVisualizationModes();

    LayerChangeEpilog(TLayerType);
}

template<LayerType TLayerType>
void Controller::InternalRemoveLayer()
{
    auto const scopedToolResumeState = SuspendTool();

    //
    // Do layer-specific work
    //

    if constexpr (TLayerType == LayerType::Electrical)
    {
        // Create undo action
        InternalPushUndoForWholeLayer<LayerType::Electrical>(_("Remove Electrical Layer"));

        // Remove layer
        WrapLikelyLayerPresenceChangingOperation<TLayerType>(
            [this]()
            {
                mModelController->RemoveElectricalLayer();
            });
    }
    else if constexpr (TLayerType == LayerType::Ropes)
    {
        // Create undo action
        InternalPushUndoForWholeLayer<LayerType::Ropes>(_("Remove Ropes Layer"));

        // Remove layer
        WrapLikelyLayerPresenceChangingOperation<TLayerType>(
            [this]()
            {
                mModelController->RemoveRopesLayer();
            });
    }
    else
    {
        static_assert(TLayerType == LayerType::Texture); // No "remove" layer for structural

        // Create undo action
        InternalPushUndoForWholeLayer<LayerType::Texture>(_("Remove Texture Layer"));

        // Remove layer
        WrapLikelyLayerPresenceChangingOperation<TLayerType>(
            [this]()
            {
                mModelController->RemoveTextureLayer();
            });
    }

    // Update visualization modes
    InternalUpdateModelControllerVisualizationModes();

    LayerChangeEpilog(TLayerType);
}

template<LayerType TLayerType>
void Controller::InternalPushUndoForWholeLayer(wxString const & title)
{
    assert(!mCurrentTool); // Tools are suspended

    // Get dirty state snapshot
    auto originalDirtyStateClone = mModelController->GetDirtyState();

    // Create undo action
    if constexpr (TLayerType == LayerType::Electrical)
    {
        auto originalLayerClone = mModelController->CloneElectricalLayer();
        auto const cloneByteSize = originalLayerClone ? originalLayerClone->Buffer.GetByteSize() : 0;

        mUndoStack.Push(
            title,
            cloneByteSize,
            originalDirtyStateClone,
            [originalLayerClone = std::move(originalLayerClone)](Controller & controller) mutable
            {
                controller.RestoreElectricalLayerForUndo(
                    std::move(originalLayerClone));
            });
    }
    else if constexpr (TLayerType == LayerType::Ropes)
    {
        auto originalLayerClone = mModelController->CloneRopesLayer();
        auto const cloneByteSize = originalLayerClone ? originalLayerClone->Buffer.GetSize() * sizeof(RopeElement) : 0;

        // Create undo action
        mUndoStack.Push(
            title,
            cloneByteSize,
            originalDirtyStateClone,
            [originalLayerClone = std::move(originalLayerClone)](Controller & controller) mutable
            {
                controller.RestoreRopesLayerForUndo(
                    std::move(originalLayerClone));
            });
    }
    else if constexpr (TLayerType == LayerType::Structural)
    {
        auto originalLayerClone = mModelController->CloneStructuralLayer();
        auto const cloneByteSize = originalLayerClone.Buffer.GetByteSize();

        // Create undo action
        mUndoStack.Push(
            title,
            cloneByteSize,
            originalDirtyStateClone,
            [originalLayerClone = std::move(originalLayerClone)](Controller & controller) mutable
            {
                controller.RestoreStructuralLayerForUndo(
                    std::move(originalLayerClone));
            });
    }
    else
    {
        auto originalLayerClone = mModelController->CloneTextureLayer();
        auto const cloneByteSize = originalLayerClone ? originalLayerClone->Buffer.GetByteSize() : 0;
        auto originalTextureArtCredits = mModelController->GetShipMetadata().ArtCredits;

        // Create undo action
        mUndoStack.Push(
            title,
            cloneByteSize,
            originalDirtyStateClone,
            [originalLayerClone = std::move(originalLayerClone), originalTextureArtCredits = std::move(originalTextureArtCredits)](Controller & controller) mutable
            {
                controller.RestoreTextureLayerForUndo(
                    std::move(originalLayerClone),
                    std::move(originalTextureArtCredits));
            });
    }

    // Notify undo stack
    mUserInterface.OnUndoStackStateChanged(mUndoStack);
}

template<LayerType TLayerType, typename TFunctor>
void Controller::WrapLikelyLayerPresenceChangingOperation(TFunctor operation)
{
    assert(!mCurrentTool); // Tools are suspended

    bool const oldIsLayerPresent = mModelController->HasLayer(TLayerType);

    operation();

    bool const newIsLayerPresent = mModelController->HasLayer(TLayerType);

    if (oldIsLayerPresent != newIsLayerPresent)
    {
        // Notify layer presence changed
        mUserInterface.OnLayerPresenceChanged(*mModelController);

        if constexpr (TLayerType == LayerType::Texture)
        {
            // Make sure current game viz mode is consistent with presence of texture layer
            InternalReconciliateTextureVisualizationMode();
        }

        if (!newIsLayerPresent)
        {
            //
            // Deal with layer removal - need to ensure consistency
            //

            // Switch primary viz to default if it was about this layer
            if (VisualizationToLayer(mWorkbenchState.GetPrimaryVisualization()) == TLayerType)
            {
                InternalSelectPrimaryVisualization(WorkbenchState::GetDefaultPrimaryVisualization()); // Will also change tool
            }

            if constexpr (TLayerType == LayerType::Texture)
            {
                // Change texture visualization mode if it's currently "None", so that next time a texture
                // layer is present, we don't start in "none" mode
                if (mWorkbenchState.GetTextureLayerVisualizationMode() == TextureLayerVisualizationModeType::None)
                {
                    mWorkbenchState.SetTextureLayerVisualizationMode(TextureLayerVisualizationModeType::MatteMode); // New default for next layer
                    mUserInterface.OnTextureLayerVisualizationModeChanged(TextureLayerVisualizationModeType::MatteMode);
                }
            }
        }
        else
        {
            // Note: we do nothing if, instead, we've just *added* the layer - we let the caller decide
            // what to do on that, as it's not about consistency
        }
    }
}

void Controller::InternalSetShipProperties(
    std::optional<ShipMetadata> && metadata,
    std::optional<ShipPhysicsData> && physicsData,
    std::optional<std::optional<ShipAutoTexturizationSettings>> && autoTexturizationSettings)
{
    assert(mModelController);

    if (metadata.has_value())
    {
        bool const hasShipNameChanged =
            mModelController->GetShipMetadata().ShipName != metadata->ShipName;

        bool const hasShipScaleChanged =
            mModelController->GetShipMetadata().Scale != metadata->Scale;

        mModelController->SetShipMetadata(std::move(*metadata));

        if (hasShipNameChanged)
        {
            mUserInterface.OnShipNameChanged(*mModelController);
        }

        if (hasShipScaleChanged)
        {
            mUserInterface.OnShipScaleChanged(mModelController->GetShipMetadata().Scale);
        }
    }

    if (physicsData.has_value())
    {
        mModelController->SetShipPhysicsData(std::move(*physicsData));
    }

    if (autoTexturizationSettings.has_value())
    {
        mModelController->SetShipAutoTexturizationSettings(std::move(*autoTexturizationSettings));

        if (mWorkbenchState.GetGameVisualizationMode() == GameVisualizationModeType::AutoTexturizationMode)
        {
            // Redo game viz
            mModelController->ForceWholeGameVisualizationRefresh();

            // Refresh model visualizations
            mModelController->UpdateVisualizations(*mView);
            mUserInterface.RefreshView();
        }
    }
}

void Controller::InternalSetElectricalPanelMetadata(ElectricalPanelMetadata && panelMetadata)
{
    assert(mModelController);

    mModelController->SetElectricalPanelMetadata(std::move(panelMetadata));
}

void Controller::InternalSelectPrimaryVisualization(VisualizationType primaryVisualization)
{
    //
    // No tool destroy/create
    // No visualization changes
    //

    assert(!mCurrentTool);
    assert(mWorkbenchState.GetPrimaryVisualization() != primaryVisualization);

    // Store new primary visualization
    mWorkbenchState.SetPrimaryVisualization(primaryVisualization);

    // Notify
    mUserInterface.OnPrimaryVisualizationChanged(primaryVisualization);

    // Select the last tool we have used for this new viz's layer
    // Note: stores new tool for new layer
    InternalSetCurrentTool(mLastToolTypePerLayer[static_cast<size_t>(VisualizationToLayer(primaryVisualization))]);

    // Tell view
    mView->SetPrimaryVisualization(primaryVisualization);
}

void Controller::InternalReconciliateTextureVisualizationMode()
{
    if (!mModelController->HasLayer(LayerType::Texture))
    {
        // If game visualization mode is the one only allowed with texture,
        // change it to auto-texturization
        if (mWorkbenchState.GetGameVisualizationMode() == GameVisualizationModeType::TextureMode)
        {
            mWorkbenchState.SetGameVisualizationMode(GameVisualizationModeType::AutoTexturizationMode);
            mUserInterface.OnGameVisualizationModeChanged(GameVisualizationModeType::AutoTexturizationMode);
        }
    }
    else
    {
        // If game visualization mode is the one only allowed without texture,
        // change it to texture
        if (mWorkbenchState.GetGameVisualizationMode() == GameVisualizationModeType::AutoTexturizationMode)
        {
            mWorkbenchState.SetGameVisualizationMode(GameVisualizationModeType::TextureMode);
            mUserInterface.OnGameVisualizationModeChanged(GameVisualizationModeType::TextureMode);
        }
    }
}

void Controller::InternalUpdateModelControllerVisualizationModes()
{
    //
    // Here we orchestrate the viz modes that we want for the ModelController
    //

    // Game

    mModelController->SetGameVisualizationMode(mWorkbenchState.GetGameVisualizationMode());

    // Structural

    assert(mModelController->HasLayer(LayerType::Structural));

    mModelController->SetStructuralLayerVisualizationMode(mWorkbenchState.GetStructuralLayerVisualizationMode());

    // Electrical

    if (mModelController->HasLayer(LayerType::Electrical))
    {
        mModelController->SetElectricalLayerVisualizationMode(mWorkbenchState.GetElectricalLayerVisualizationMode());
    }
    else
    {
        mModelController->SetElectricalLayerVisualizationMode(ElectricalLayerVisualizationModeType::None);
    }

    // Ropes

    if (mModelController->HasLayer(LayerType::Ropes))
    {
        mModelController->SetRopesLayerVisualizationMode(mWorkbenchState.GetRopesLayerVisualizationMode());
    }
    else
    {
        mModelController->SetRopesLayerVisualizationMode(RopesLayerVisualizationModeType::None);
    }

    // Texture

    if (mModelController->HasLayer(LayerType::Texture))
    {
        mModelController->SetTextureLayerVisualizationMode(mWorkbenchState.GetTextureLayerVisualizationMode());
    }
    else
    {
        mModelController->SetTextureLayerVisualizationMode(TextureLayerVisualizationModeType::None);
    }
}

void Controller::InternalSetCurrentTool(std::optional<ToolType> toolType)
{
    //
    // No tool destroy/create
    // No visualization changes
    //

    assert(!mCurrentTool);

    // Set current tool
    mWorkbenchState.SetCurrentToolType(toolType);

    // Notify
    mUserInterface.OnCurrentToolChanged(toolType);

    if (!toolType.has_value())
    {
        // Relinquish cursor
        mUserInterface.ResetToolCursor();
    }

    // Remember new tool as the last tool of this primary visualization's layer
    mLastToolTypePerLayer[static_cast<size_t>(VisualizationToLayer(mWorkbenchState.GetPrimaryVisualization()))] = toolType;
}

Controller::ScopedToolResumeState Controller::SuspendTool() const
{
    return ScopedToolResumeState(
        *this,
        (const_cast<Controller *>(this))->InternalSuspendTool());
}

bool Controller::InternalSuspendTool()
{
    bool const doResume = (mCurrentTool != nullptr);

    mCurrentTool.reset();
    // Leave mCurrentToolType as-is

    return doResume;
}

void Controller::InternalResumeTool()
{
    mCurrentTool.reset();

    if (mWorkbenchState.GetCurrentToolType().has_value())
    {
        mCurrentTool = MakeTool(*mWorkbenchState.GetCurrentToolType());
    }
}

void Controller::InternalResetTool()
{
    InternalSuspendTool();
    InternalResumeTool();
}

std::unique_ptr<Tool> Controller::MakeTool(ToolType toolType)
{
    switch (toolType)
    {
        case ToolType::ElectricalEraser:
        {
            return std::make_unique<ElectricalEraserTool>(
                *this,
                mResourceLocator);
        }

        case ToolType::ElectricalLine:
        {
            return std::make_unique<ElectricalLineTool>(
                *this,
                mResourceLocator);
        }

        case ToolType::ElectricalPencil:
        {
            return std::make_unique<ElectricalPencilTool>(
                *this,
                mResourceLocator);
        }

        case ToolType::ElectricalSampler:
        {
            return std::make_unique<ElectricalSamplerTool>(
                *this,
                mResourceLocator);
        }

        case ToolType::StructuralEraser:
        {
            return std::make_unique<StructuralEraserTool>(
                *this,
                mResourceLocator);
        }

        case ToolType::StructuralFlood:
        {
            return std::make_unique<StructuralFloodTool>(
                *this,
                mResourceLocator);
        }

        case ToolType::StructuralLine:
        {
            return std::make_unique<StructuralLineTool>(
                *this,
                mResourceLocator);
        }

        case ToolType::StructuralMeasuringTapeTool:
        {
            return std::make_unique<MeasuringTapeTool>(
                *this,
                mResourceLocator);
        }

        case ToolType::StructuralPencil:
        {
            return std::make_unique<StructuralPencilTool>(
                *this,
                mResourceLocator);
        }

        case ToolType::StructuralSampler:
        {
            return std::make_unique<StructuralSamplerTool>(
                *this,
                mResourceLocator);
        }

        case ToolType::RopePencil:
        {
            return std::make_unique<RopePencilTool>(
                *this,
                mResourceLocator);
        }

        case ToolType::RopeEraser:
        {
            return std::make_unique<RopeEraserTool>(
                *this,
                mResourceLocator);
        }

        case ToolType::RopeSampler:
        {
            return std::make_unique<RopeSamplerTool>(
                *this,
                mResourceLocator);
        }

        case ToolType::TextureEraser:
        {
            return std::make_unique<TextureEraserTool>(
                *this,
                mResourceLocator);
        }

        case ToolType::TextureMagicWand:
        {
            return std::make_unique<TextureMagicWandTool>(
                *this,
                mResourceLocator);
        }

        case ToolType::StructuralSelection:
        {
            return std::make_unique<StructuralSelectionTool>(
                *this,
                mSelectionManager,
                mResourceLocator);
        }

        case ToolType::ElectricalSelection:
        {
            return std::make_unique<ElectricalSelectionTool>(
                *this,
                mSelectionManager,
                mResourceLocator);
        }

        case ToolType::RopeSelection:
        {
            return std::make_unique<RopeSelectionTool>(
                *this,
                mSelectionManager,
                mResourceLocator);
        }

        case ToolType::TextureSelection:
        {
            return std::make_unique<TextureSelectionTool>(
                *this,
                mSelectionManager,
                mResourceLocator);
        }
    }

    assert(false);
    return nullptr;
}

void Controller::InternalResizeShip(
    ShipSpaceSize const & newSize,
    ShipSpaceCoordinates const & originOffset,
    wxString const & actionName)
{
    // Store undo
    {
        // Get dirty state
        ModelDirtyState const originalDirtyState = mModelController->GetDirtyState();

        // Clone all layers
        auto structuralLayerClone = mModelController->CloneStructuralLayer();
        auto electricalLayerClone = mModelController->CloneElectricalLayer();
        auto ropesLayerClone = mModelController->CloneRopesLayer();
        auto textureLayerClone = mModelController->CloneTextureLayer();
        auto textureArtCreditsClone = mModelController->GetShipMetadata().ArtCredits;

        // Calculate cost
        size_t const totalCost =
            structuralLayerClone.Buffer.GetByteSize()
            + (electricalLayerClone ? electricalLayerClone->Buffer.GetByteSize() : 0)
            + (ropesLayerClone ? ropesLayerClone->Buffer.GetSize() * sizeof(RopeElement) : 0)
            + (textureLayerClone ? textureLayerClone->Buffer.GetByteSize() : 0);

        // Create undo

        mUndoStack.Push(
            actionName,
            totalCost,
            originalDirtyState,
            [shipSize = mModelController->GetShipSize()
            , structuralLayerClone = std::move(structuralLayerClone)
            , electricalLayerClone = std::move(electricalLayerClone)
            , ropesLayerClone = std::move(ropesLayerClone)
            , textureLayerClone = std::move(textureLayerClone)
            , textureArtCreditsClone = std::move(textureArtCreditsClone)](Controller & controller) mutable
            {
                controller.RestoreAllLayersForUndo(
                    shipSize,
                    std::move(structuralLayerClone),
                    std::move(electricalLayerClone),
                    std::move(ropesLayerClone),
                    std::move(textureLayerClone),
                    std::move(textureArtCreditsClone));
            });

        mUserInterface.OnUndoStackStateChanged(mUndoStack);
    }

    // Resize
    mModelController->ResizeShip(newSize, originOffset);

    // Update dirtyness
    mModelController->SetAllPresentLayersDirty();
    mUserInterface.OnModelDirtyChanged(*mModelController);

    // Notify view of new size
    mView->SetShipSize(newSize);
    mUserInterface.OnViewModelChanged(mView->GetViewModel());

    // Notify UI of new ship size
    mUserInterface.OnShipSizeChanged(newSize);

    // Notify macro properties
    NotifyModelMacroPropertiesUpdated();

    // Refresh model visualizations
    mModelController->UpdateVisualizations(*mView);
    mUserInterface.RefreshView();
}

template<bool IsForUndo>
void Controller::InternalFlip(DirectionType direction)
{
    if constexpr (!IsForUndo)
    {
        // Get dirty state
        ModelDirtyState const originalDirtyState = mModelController->GetDirtyState();

        // Calculate undo title
        wxString undoTitle;
        if (direction == DirectionType::Horizontal)
            undoTitle = _("Flip H");
        else if (direction == DirectionType::Vertical)
            undoTitle = _("Flip V");
        else if (direction == (DirectionType::Horizontal | DirectionType::Vertical))
            undoTitle = _("Flip H+V");
        else
            assert(false);

        // Create undo

        mUndoStack.Push(
            undoTitle,
            1, // Arbitrary
            originalDirtyState,
            [direction](Controller & controller) mutable
            {
                controller.FlipForUndo(direction);
            });

        mUserInterface.OnUndoStackStateChanged(mUndoStack);
    }

    // Flip
    mModelController->Flip(direction);

    if constexpr (!IsForUndo)
    {
        // Update dirtyness
        mModelController->SetAllPresentLayersDirty();
        mUserInterface.OnModelDirtyChanged(*mModelController);
    }

    // Update macro properties
    NotifyModelMacroPropertiesUpdated();

    // Refresh model visualizations
    mModelController->UpdateVisualizations(*mView);
    mUserInterface.RefreshView();
}

template<bool IsForUndo>
void Controller::InternalRotate90(RotationDirectionType direction)
{
    if constexpr (!IsForUndo)
    {
        // Get dirty state
        ModelDirtyState const originalDirtyState = mModelController->GetDirtyState();

        // Calculate undo title and anti-rotation
        wxString undoTitle;
        RotationDirectionType antiRotation;
        if (direction == RotationDirectionType::Clockwise)
        {
            undoTitle = _("Rotate CW");
            antiRotation = RotationDirectionType::CounterClockwise;
        }
        else
        {
            assert(direction == RotationDirectionType::CounterClockwise);

            undoTitle = _("Rotate CCW");
            antiRotation = RotationDirectionType::Clockwise;
        }

        // Create undo

        mUndoStack.Push(
            undoTitle,
            1, // Arbitrary
            originalDirtyState,
            [antiRotation](Controller & controller) mutable
            {
                controller.Rotate90ForUndo(antiRotation);
            });

        mUserInterface.OnUndoStackStateChanged(mUndoStack);
    }

    // Rotate
    mModelController->Rotate90(direction);

    if constexpr (!IsForUndo)
    {
        // Update dirtyness
        mModelController->SetAllPresentLayersDirty();
        mUserInterface.OnModelDirtyChanged(*mModelController);
    }

    // Notify view of new size
    mView->SetShipSize(mModelController->GetShipSize());
    mUserInterface.OnViewModelChanged(mView->GetViewModel());

    // Notify UI of new ship size
    mUserInterface.OnShipSizeChanged(mModelController->GetShipSize());

    // Update macro properties
    NotifyModelMacroPropertiesUpdated();

    // Refresh model visualizations
    mModelController->UpdateVisualizations(*mView);
    mUserInterface.RefreshView();
}

void Controller::NotifyModelMacroPropertiesUpdated()
{
    auto const & modelMacroProperties = mModelController->GetModelMacroProperties();

    // Notify UI
    mUserInterface.OnModelMacroPropertiesUpdated(modelMacroProperties);

    // Upload marker - if applicable
    if (mWorkbenchState.IsWaterlineMarkersEnabled())
    {
        if (modelMacroProperties.CenterOfMass.has_value())
        {
            mView->UploadWaterlineMarker(
                *modelMacroProperties.CenterOfMass,
                View::WaterlineMarkerType::CenterOfMass);
        }
        else
        {
            mView->RemoveWaterlineMarker(View::WaterlineMarkerType::CenterOfMass);
        }
    }
}

void Controller::RefreshToolCoordinatesDisplay()
{
    // Calculate ship coordinates
    ShipSpaceCoordinates mouseShipSpaceCoordinates = mView->ScreenToShipSpace(mUserInterface.GetMouseCoordinates());

    // Check if within ship canvas
    auto const & shipSize = mModelController->GetShipSize();
    if (mouseShipSpaceCoordinates.IsInSize(shipSize))
    {
        mUserInterface.OnToolCoordinatesChanged(mouseShipSpaceCoordinates, shipSize);
    }
    else
    {
        mUserInterface.OnToolCoordinatesChanged(std::nullopt, shipSize);
    }
}

}