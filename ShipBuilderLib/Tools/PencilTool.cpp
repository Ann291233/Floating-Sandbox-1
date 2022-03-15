/***************************************************************************************
* Original Author:		Gabriele Giuseppini
* Created:				2021-09-12
* Copyright:			Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "PencilTool.h"

#include "Controller.h"

#include <GameCore/GameGeometry.h>

#include <UILib/WxHelpers.h>

#include <type_traits>

namespace ShipBuilder {

StructuralPencilTool::StructuralPencilTool(
    Controller & controller,
    ResourceLocator const & resourceLocator)
    : PencilTool(
        ToolType::StructuralPencil,
        controller,
        resourceLocator)
{}

ElectricalPencilTool::ElectricalPencilTool(
    Controller & controller,
    ResourceLocator const & resourceLocator)
    : PencilTool(
        ToolType::ElectricalPencil,
        controller,
        resourceLocator)
{}

StructuralEraserTool::StructuralEraserTool(
    Controller & controller,
    ResourceLocator const & resourceLocator)
    : PencilTool(
        ToolType::StructuralEraser,
        controller,
        resourceLocator)
{}

ElectricalEraserTool::ElectricalEraserTool(
    Controller & controller,
    ResourceLocator const & resourceLocator)
    : PencilTool(
        ToolType::ElectricalEraser,
        controller,
        resourceLocator)
{}

template<LayerType TLayer, bool IsEraser>
PencilTool<TLayer, IsEraser>::PencilTool(
    ToolType toolType,
    Controller & controller,
    ResourceLocator const & resourceLocator)
    : Tool(
        toolType,
        controller)
    , mOriginalLayerClone(mController.GetModelController().CloneExistingLayer<TLayer>())
    , mTempVisualizationDirtyShipRegion()
    , mEngagementData()
    , mIsShiftDown(false)
{
    wxImage cursorImage;
    if constexpr (IsEraser)
    {
        cursorImage = WxHelpers::LoadCursorImage("eraser_cursor", 8, 27, resourceLocator);
    }
    else
    {
        cursorImage = WxHelpers::LoadCursorImage("pencil_cursor", 2, 22, resourceLocator);
    }

    SetCursor(cursorImage);

    // Check if we need to immediately do a visualization
    auto const mouseCoordinates = GetMouseCoordinatesIfInWorkCanvas();
    if (mouseCoordinates)
    {
        // Calculate affected rect
        std::optional<ShipSpaceRect> const affectedRect = CalculateApplicableRect(ScreenToShipSpace(*mouseCoordinates));

        // Apply (temporary) change
        if (affectedRect)
        {
            DoTempVisualization(*affectedRect);

            assert(mTempVisualizationDirtyShipRegion);

            mController.LayerChangeEpilog();
        }
    }
}

template<LayerType TLayer, bool IsEraser>
PencilTool<TLayer, IsEraser>::~PencilTool()
{
    // Mend our temporary visualization, if any
    if (mTempVisualizationDirtyShipRegion)
    {
        MendTempVisualization();

        assert(!mTempVisualizationDirtyShipRegion);

        mController.LayerChangeEpilog();
    }
}

template<LayerType TLayer, bool IsEraser>
void PencilTool<TLayer, IsEraser>::OnMouseMove(DisplayLogicalCoordinates const & mouseCoordinates)
{
    // Assuming L/R button transitions already communicated

    auto const mouseShipSpaceCoords = ScreenToShipSpace(mouseCoordinates);

    if (!mEngagementData)
    {
        //
        // Temp visualization
        //

        // Calculate affected rect
        std::optional<ShipSpaceRect> const affectedRect = CalculateApplicableRect(mouseShipSpaceCoords);

        if (affectedRect != mTempVisualizationDirtyShipRegion)
        {
            // Restore previous temp visualization
            if (mTempVisualizationDirtyShipRegion)
            {
                MendTempVisualization();

                assert(!mTempVisualizationDirtyShipRegion);
            }

            // Apply (temporary) change
            if (affectedRect)
            {
                DoTempVisualization(*affectedRect);

                assert(mTempVisualizationDirtyShipRegion);
            }

            mController.LayerChangeEpilog();
        }
    }
    else
    {
        DoEdit(mouseShipSpaceCoords);
    }
}

template<LayerType TLayer, bool IsEraser>
void PencilTool<TLayer, IsEraser>::OnLeftMouseDown()
{
    // Restore temp visualization
    if (mTempVisualizationDirtyShipRegion)
    {
        MendTempVisualization();

        assert(!mTempVisualizationDirtyShipRegion);
    }

    auto const mouseShipSpaceCoords = GetCurrentMouseCoordinatesInShipSpace();

    if (!mEngagementData)
    {
        StartEngagement(mouseShipSpaceCoords, StrongTypedFalse<IsRightMouseButton>);

        assert(mEngagementData);
    }

    DoEdit(mouseShipSpaceCoords);
}

template<LayerType TLayer, bool IsEraser>
void PencilTool<TLayer, IsEraser>::OnLeftMouseUp()
{
    if (mEngagementData)
    {
        EndEngagement();

        assert(!mEngagementData);
    }

    // Note: we don't start temp visualization, as the current mouse position
    // already has the edit (as permanent)
}

template<LayerType TLayer, bool IsEraser>
void PencilTool<TLayer, IsEraser>::OnRightMouseDown()
{
    // Restore temp visualization
    if (mTempVisualizationDirtyShipRegion)
    {
        MendTempVisualization();

        assert(!mTempVisualizationDirtyShipRegion);
    }

    auto const mouseShipSpaceCoords = GetCurrentMouseCoordinatesInShipSpace();

    if (!mEngagementData)
    {
        StartEngagement(mouseShipSpaceCoords, StrongTypedTrue<IsRightMouseButton>);

        assert(mEngagementData);
    }

    DoEdit(mouseShipSpaceCoords);
}

template<LayerType TLayer, bool IsEraser>
void PencilTool<TLayer, IsEraser>::OnRightMouseUp()
{
    if (mEngagementData)
    {
        EndEngagement();

        assert(!mEngagementData);
    }

    // Note: we don't start temp visualization, as the current mouse position
    // already has the edit (as permanent)
}

template<LayerType TLayer, bool IsEraser>
void PencilTool<TLayer, IsEraser>::OnShiftKeyDown()
{
    mIsShiftDown = true;

    if (mEngagementData)
    {
        // Remember initial engagement
        assert(!mEngagementData->ShiftLockInitialPosition.has_value());
        mEngagementData->ShiftLockInitialPosition = GetCurrentMouseCoordinatesInShipSpace();
    }
}

template<LayerType TLayer, bool IsEraser>
void PencilTool<TLayer, IsEraser>::OnShiftKeyUp()
{
    mIsShiftDown = false;

    if (mEngagementData)
    {
        // Forget engagement
        assert(mEngagementData->ShiftLockInitialPosition.has_value());
        mEngagementData->ShiftLockInitialPosition.reset();
        mEngagementData->ShiftLockIsVertical.reset();
    }
}

//////////////////////////////////////////////////////////////////////////////

template<LayerType TLayer, bool IsEraser>
void PencilTool<TLayer, IsEraser>::StartEngagement(
    ShipSpaceCoordinates const & mouseCoordinates,
    StrongTypedBool<struct IsRightMouseButton> isRightButton)
{
    assert(!mEngagementData);

    mEngagementData.emplace(
        isRightButton ? MaterialPlaneType::Background : MaterialPlaneType::Foreground,
        mController.GetModelController().GetDirtyState(),
        mIsShiftDown ? mouseCoordinates : std::optional<ShipSpaceCoordinates>());
}

template<LayerType TLayer, bool IsEraser>
void PencilTool<TLayer, IsEraser>::DoEdit(ShipSpaceCoordinates const & mouseCoordinates)
{
    assert(mEngagementData);

    LayerMaterialType const * const fillMaterial = GetFillMaterial(mEngagementData->Plane);

    bool hasEdited = false;

    // Calculate SHIFT lock direction, if needed
    if (mEngagementData->ShiftLockInitialPosition.has_value() && !mEngagementData->ShiftLockIsVertical.has_value())
    {
        if (mouseCoordinates != *(mEngagementData->ShiftLockInitialPosition))
        {
            // Constrain to either horizontally or vertically, wrt SHIFT lock's initial position
            if (std::abs(mouseCoordinates.x - mEngagementData->ShiftLockInitialPosition->x) > std::abs(mouseCoordinates.y - mEngagementData->ShiftLockInitialPosition->y))
            {
                // X is larger
                mEngagementData->ShiftLockIsVertical = false;
            }
            else
            {
                // Y is larger
                mEngagementData->ShiftLockIsVertical = true;
            }
        }
    }

    // Calculate actual mouse coordinates - adjusted for SHIFT lock
    ShipSpaceCoordinates actualMouseCoordinates = mouseCoordinates;
    if (mEngagementData->ShiftLockIsVertical.has_value())
    {
        assert(mEngagementData->ShiftLockInitialPosition.has_value());

        if (*(mEngagementData->ShiftLockIsVertical))
        {
            actualMouseCoordinates.x = mEngagementData->ShiftLockInitialPosition->x;
        }
        else
        {
            actualMouseCoordinates.y = mEngagementData->ShiftLockInitialPosition->y;
        }
    }

    // Calculate start point
    ShipSpaceCoordinates const startPoint = (TLayer == LayerType::Structural && mEngagementData->PreviousEngagementPosition.has_value()) // // Pencil wakes exist only in structural layer, not in the others
        ? *mEngagementData->PreviousEngagementPosition
        : actualMouseCoordinates;

    // Calculate end point
    ShipSpaceCoordinates const endPoint = actualMouseCoordinates;

    // Generate line
    GenerateIntegralLinePath<IntegralLineType::Minimal>(
        startPoint,
        endPoint,
        [&](ShipSpaceCoordinates const & pos)
        {
            // Calc applicable rect intersecting pencil with workspace size
            auto const applicableRect = CalculateApplicableRect(pos);
            if (applicableRect)
            {
                bool isAllowed;

                if constexpr (TLayer == LayerType::Structural)
                {
                    isAllowed = true;

                    mController.GetModelController().StructuralRegionFill(
                        *applicableRect,
                        fillMaterial);
                }
                else
                {
                    static_assert(TLayer == LayerType::Electrical);

                    assert(applicableRect->size == ShipSpaceSize(1, 1));

                    if constexpr (IsEraser)
                    {
                        isAllowed = true;
                    }
                    else
                    {
                        isAllowed = mController.GetModelController().IsElectricalParticleAllowedAt(applicableRect->origin);
                    }

                    if (isAllowed)
                    {
                        mController.GetModelController().ElectricalRegionFill(
                            *applicableRect,
                            fillMaterial);
                    }
                }

                if (isAllowed)
                {
                    // Update edit region
                    if (!mEngagementData->EditRegion)
                    {
                        mEngagementData->EditRegion = *applicableRect;
                    }
                    else
                    {
                        mEngagementData->EditRegion->UnionWith(*applicableRect);
                    }

                    hasEdited = true;
                }
            }
        });

    // Update previous engagement
    mEngagementData->PreviousEngagementPosition = endPoint;

    // Epilog
    mController.LayerChangeEpilog(hasEdited ? TLayer : std::optional<LayerType>());
}

template<LayerType TLayer, bool IsEraser>
void PencilTool<TLayer, IsEraser>::EndEngagement()
{
    assert(mEngagementData);

    if (mEngagementData->EditRegion)
    {
        //
        // Create undo action
        //

        auto clippedLayerClone = mOriginalLayerClone.Clone(*mEngagementData->EditRegion);

        mController.StoreUndoAction(
            IsEraser
            ? (TLayer == LayerType::Structural ? _("Eraser Structural") : _("Eraser Electrical"))
            : (TLayer == LayerType::Structural ? _("Pencil Structural") : _("Pencil Electrical")),
            clippedLayerClone.Buffer.GetByteSize(),
            mEngagementData->OriginalDirtyState,
            [clippedLayerClone = std::move(clippedLayerClone), origin = mEngagementData->EditRegion->origin](Controller & controller) mutable
            {
                if constexpr(TLayer == LayerType::Structural)
                {
                    controller.RestoreStructuralLayerRegionForUndo(std::move(clippedLayerClone), origin);
                }
                else
                {
                    static_assert(TLayer == LayerType::Electrical);

                    controller.RestoreElectricalLayerRegionForUndo(std::move(clippedLayerClone), origin);
                }
            });
    }

    //
    // Reset engagement
    //

    mEngagementData.reset();

    //
    // Restart temp visualization
    //

    assert(!mTempVisualizationDirtyShipRegion);

    // Re-take original layer clone
    mOriginalLayerClone = mController.GetModelController().CloneExistingLayer<TLayer>();
}

template<LayerType TLayer, bool IsEraser>
void PencilTool<TLayer, IsEraser>::DoTempVisualization(ShipSpaceRect const & affectedRect)
{
    // No mouse button information, hence choosing foreground plane arbitrarily
    LayerMaterialType const * fillMaterial = GetFillMaterial(MaterialPlaneType::Foreground);

    View::OverlayMode overlayMode = View::OverlayMode::Default;

    if constexpr (TLayer == LayerType::Structural)
    {
        mController.GetModelController().StructuralRegionFillForEphemeralVisualization(
            affectedRect,
            fillMaterial);
    }
    else
    {
        static_assert(TLayer == LayerType::Electrical);

        assert(affectedRect.size == ShipSpaceSize(1, 1));
        if (!IsEraser
            && !mController.GetModelController().IsElectricalParticleAllowedAt(affectedRect.origin))
        {
            overlayMode = View::OverlayMode::Error;
        }

        mController.GetModelController().ElectricalRegionFillForEphemeralVisualization(
            affectedRect,
            fillMaterial);
    }

    mController.GetView().UploadRectOverlay(
        affectedRect,
        overlayMode);

    mTempVisualizationDirtyShipRegion = affectedRect;
}

template<LayerType TLayer, bool IsEraser>
void PencilTool<TLayer, IsEraser>::MendTempVisualization()
{
    assert(mTempVisualizationDirtyShipRegion);

    if constexpr (TLayer == LayerType::Structural)
    {
        mController.GetModelController().RestoreStructuralLayerRegionForEphemeralVisualization(
            mOriginalLayerClone,
            *mTempVisualizationDirtyShipRegion,
            mTempVisualizationDirtyShipRegion->origin);
    }
    else
    {
        static_assert(TLayer == LayerType::Electrical);

        mController.GetModelController().RestoreElectricalLayerRegionForEphemeralVisualization(
            mOriginalLayerClone,
            *mTempVisualizationDirtyShipRegion,
            mTempVisualizationDirtyShipRegion->origin);
    }

    mController.GetView().RemoveRectOverlay();

    mTempVisualizationDirtyShipRegion.reset();
}

template<LayerType TLayer, bool IsEraser>
std::optional<ShipSpaceRect> PencilTool<TLayer, IsEraser>::CalculateApplicableRect(ShipSpaceCoordinates const & coords) const
{
    // Anchor in the middle, and vertically from top

    int const pencilSize = GetPencilSize();
    int const topLeftPencilSize =  (pencilSize - 1) / 2;

    ShipSpaceCoordinates const origin = ShipSpaceCoordinates(coords.x, coords.y - (pencilSize - 1));

    return ShipSpaceRect(origin - ShipSpaceSize(topLeftPencilSize, -topLeftPencilSize), { pencilSize, pencilSize })
        .MakeIntersectionWith({ { 0, 0 }, mController.GetModelController().GetShipSize() });
}

template<LayerType TLayer, bool IsEraser>
int PencilTool<TLayer, IsEraser>::GetPencilSize() const
{
    if constexpr (!IsEraser)
    {
        if constexpr (TLayer == LayerType::Structural)
        {
            return static_cast<int>(mController.GetWorkbenchState().GetStructuralPencilToolSize());
        }
        else
        {
            static_assert(TLayer == LayerType::Electrical);

            return 1;
        }
    }
    else
    {
        if constexpr (TLayer == LayerType::Structural)
        {
            return static_cast<int>(mController.GetWorkbenchState().GetStructuralEraserToolSize());
        }
        else
        {
            static_assert(TLayer == LayerType::Electrical);

            return 1;
        }
    }
}

template<LayerType TLayer, bool IsEraser>
typename PencilTool<TLayer, IsEraser>::LayerMaterialType const * PencilTool<TLayer, IsEraser>::GetFillMaterial(MaterialPlaneType plane) const
{
    if constexpr (!IsEraser)
    {
        if constexpr (TLayer == LayerType::Structural)
        {
            if (plane == MaterialPlaneType::Foreground)
            {
                return mController.GetWorkbenchState().GetStructuralForegroundMaterial();
            }
            else
            {
                assert(plane == MaterialPlaneType::Background);

                return mController.GetWorkbenchState().GetStructuralBackgroundMaterial();
            }

        }
        else
        {
            static_assert(TLayer == LayerType::Electrical);

            if (plane == MaterialPlaneType::Foreground)
            {
                return mController.GetWorkbenchState().GetElectricalForegroundMaterial();
            }
            else
            {
                assert(plane == MaterialPlaneType::Background);

                return mController.GetWorkbenchState().GetElectricalBackgroundMaterial();
            }
        }
    }
    else
    {
        return nullptr;
    }
}

}