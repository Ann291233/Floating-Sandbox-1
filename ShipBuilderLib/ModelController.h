/***************************************************************************************
* Original Author:		Gabriele Giuseppini
* Created:				2021-08-28
* Copyright:			Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#pragma once

#include "IModelObservable.h"
#include "InstancedElectricalElementSet.h"
#include "Model.h"
#include "ModelValidationSession.h"
#include "ShipBuilderTypes.h"
#include "View.h"

#include <Game/Layers.h>
#include <Game/Materials.h>
#include <Game/ShipDefinition.h>
#include <Game/ShipTexturizer.h>

#include <GameCore/Finalizer.h>
#include <GameCore/GameTypes.h>
#include <GameCore/ImageData.h>

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace ShipBuilder {

/*
 * This class implements operations on the model.
 *
 * The ModelController does not maintain the model's dirtyness; that is managed by the Controller.
 */
class ModelController : public IModelObservable
{
public:

    static std::unique_ptr<ModelController> CreateNew(
        ShipSpaceSize const & shipSpaceSize,
        std::string const & shipName,
        ShipTexturizer const & shipTexturizer);

    static std::unique_ptr<ModelController> CreateForShip(
        ShipDefinition && shipDefinition,
        ShipTexturizer const & shipTexturizer);

    ShipDefinition MakeShipDefinition() const;

    ShipSpaceSize const & GetShipSize() const override
    {
        return mModel.GetShipSize();
    }

    void SetShipSize(ShipSpaceSize const & shipSize)
    {
        mModel.SetShipSize(shipSize);
    }

    ModelMacroProperties GetModelMacroProperties() const override
    {
        assert(mMassParticleCount == 0 || mTotalMass != 0.0f);

        return ModelMacroProperties(
            mMassParticleCount,
            mTotalMass,
            mMassParticleCount != 0 ? mCenterOfMassSum / mTotalMass : std::optional<vec2f>());
    }

    std::unique_ptr<RgbaImageData> MakePreview() const;

    std::optional<ShipSpaceRect> CalculateBoundingBox() const;

    bool HasLayer(LayerType layer) const override
    {
        return mModel.HasLayer(layer);
    }

    bool IsDirty() const override
    {
        return mModel.GetIsDirty();
    }

    bool IsLayerDirty(LayerType layer) const override
    {
        return mModel.GetIsDirty(layer);
    }

    ModelDirtyState GetDirtyState() const
    {
        return mModel.GetDirtyState();
    }

    void SetLayerDirty(LayerType layer)
    {
        mModel.SetIsDirty(layer);
    }

    void SetAllPresentLayersDirty()
    {
        mModel.SetAllPresentLayersDirty();
    }

    void RestoreDirtyState(ModelDirtyState const & dirtyState)
    {
        mModel.SetDirtyState(dirtyState);
    }

    void ClearIsDirty()
    {
        mModel.ClearIsDirty();
    }

    ModelValidationSession StartValidation(Finalizer && finalizer) const;

#ifdef _DEBUG
    bool IsInEphemeralVisualization() const
    {
        return mIsStructuralLayerInEphemeralVisualization
            || mIsElectricalLayerInEphemeralVisualization
            || mIsRopesLayerInEphemeralVisualization
            || mIsTextureLayerInEphemeralVisualization;
    }
#endif

    ShipMetadata const & GetShipMetadata() const override
    {
        return mModel.GetShipMetadata();
    }

    void SetShipMetadata(ShipMetadata && shipMetadata)
    {
        mModel.SetShipMetadata(std::move(shipMetadata));
    }

    ShipPhysicsData const & GetShipPhysicsData() const override
    {
        return mModel.GetShipPhysicsData();
    }

    void SetShipPhysicsData(ShipPhysicsData && shipPhysicsData)
    {
        mModel.SetShipPhysicsData(std::move(shipPhysicsData));
    }

    std::optional<ShipAutoTexturizationSettings> const & GetShipAutoTexturizationSettings() const override
    {
        return mModel.GetShipAutoTexturizationSettings();
    }

    void SetShipAutoTexturizationSettings(std::optional<ShipAutoTexturizationSettings> && shipAutoTexturizationSettings)
    {
        mModel.SetShipAutoTexturizationSettings(std::move(shipAutoTexturizationSettings));
    }

    InstancedElectricalElementSet const & GetInstancedElectricalElementSet() const
    {
        return mInstancedElectricalElementSet;
    }

    void SetElectricalPanelMetadata(ElectricalPanelMetadata && panelMetadata)
    {
        mModel.SetElectricalPanelMetadata(std::move(panelMetadata));
    }

    std::optional<SampledInformation> SampleInformationAt(ShipSpaceCoordinates const & coordinates, LayerType layer) const;

    void Flip(DirectionType direction);

    void Rotate90(RotationDirectionType direction);

    void ResizeShip(
        ShipSpaceSize const & newSize,
        ShipSpaceCoordinates const & originOffset);

    template<LayerType TLayer>
    typename LayerTypeTraits<TLayer>::layer_data_type CloneExistingLayer() const
    {
        switch (TLayer)
        {
            case LayerType::Electrical:
            {
                assert(!mIsElectricalLayerInEphemeralVisualization);
                break;
            }

            case LayerType::Ropes:
            {
                assert(!mIsRopesLayerInEphemeralVisualization);
                break;
            }

            case LayerType::Structural:
            {
                assert(!mIsStructuralLayerInEphemeralVisualization);
                break;
            }

            case LayerType::Texture:
            {
                assert(!mIsTextureLayerInEphemeralVisualization);
                break;
            }
        }

        return mModel.CloneExistingLayer<TLayer>();
    }

    std::unique_ptr<ShipLayers> CloneRegion(
        ShipSpaceRect const & region,
        std::optional<LayerType> layerSelection) const
    {
        return mModel.CloneRegion(region, layerSelection);
    }

    //
    // Structural
    //

    StructuralLayerData const & GetStructuralLayer() const override;

    void SetStructuralLayer(StructuralLayerData && structuralLayer);

    StructuralLayerData CloneStructuralLayer() const;

    StructuralMaterial const * SampleStructuralMaterialAt(ShipSpaceCoordinates const & coords) const;

    void StructuralRegionFill(
        ShipSpaceRect const & region,
        StructuralMaterial const * material);

    std::optional<ShipSpaceRect> StructuralFlood(
        ShipSpaceCoordinates const & start,
        StructuralMaterial const * material,
        bool doContiguousOnly);

    void RestoreStructuralLayerRegion(
        StructuralLayerData && sourceLayerRegion,
        ShipSpaceRect const & sourceRegion,
        ShipSpaceCoordinates const & targetOrigin);

    void RestoreStructuralLayer(StructuralLayerData && sourceLayer);

    void StructuralRegionFillForEphemeralVisualization(
        ShipSpaceRect const & region,
        StructuralMaterial const * material);

    void RestoreStructuralLayerRegionForEphemeralVisualization(
        StructuralLayerData const & sourceLayerRegion,
        ShipSpaceRect const & sourceRegion,
        ShipSpaceCoordinates const & targetOrigin);

    //
    // Electrical
    //

    ElectricalPanelMetadata const & GetElectricalPanelMetadata() const;

    void SetElectricalLayer(ElectricalLayerData && electricalLayer);

    void RemoveElectricalLayer();

    std::unique_ptr<ElectricalLayerData> CloneElectricalLayer() const;

    ElectricalMaterial const * SampleElectricalMaterialAt(ShipSpaceCoordinates const & coords) const;

    bool IsElectricalParticleAllowedAt(ShipSpaceCoordinates const & coords) const;

    std::optional<ShipSpaceRect> TrimElectricalParticlesWithoutSubstratum();

    void ElectricalRegionFill(
        ShipSpaceRect const & region,
        ElectricalMaterial const * material);

    void RestoreElectricalLayerRegion(
        ElectricalLayerData && sourceLayerRegion,
        ShipSpaceRect const & sourceRegion,
        ShipSpaceCoordinates const & targetOrigin);

    void RestoreElectricalLayer(std::unique_ptr<ElectricalLayerData> sourceLayer);

    void ElectricalRegionFillForEphemeralVisualization(
        ShipSpaceRect const & region,
        ElectricalMaterial const * material);

    void RestoreElectricalLayerRegionForEphemeralVisualization(
        ElectricalLayerData const & sourceLayerRegion,
        ShipSpaceRect const & sourceRegion,
        ShipSpaceCoordinates const & targetOrigin);

    //
    // Ropes
    //

    void SetRopesLayer(RopesLayerData && ropesLayer);

    void RemoveRopesLayer();

    std::unique_ptr<RopesLayerData> CloneRopesLayer() const;

    StructuralMaterial const * SampleRopesMaterialAt(ShipSpaceCoordinates const & coords) const;

    std::optional<size_t> GetRopeElementIndexAt(ShipSpaceCoordinates const & coords) const;

    void AddRope(
        ShipSpaceCoordinates const & startCoords,
        ShipSpaceCoordinates const & endCoords,
        StructuralMaterial const * material);

    void MoveRopeEndpoint(
        size_t ropeElementIndex,
        ShipSpaceCoordinates const & oldCoords,
        ShipSpaceCoordinates const & newCoords);

    bool EraseRopeAt(ShipSpaceCoordinates const & coords);

    void RestoreRopesLayer(std::unique_ptr<RopesLayerData> sourceLayer);

    void AddRopeForEphemeralVisualization(
        ShipSpaceCoordinates const & startCoords,
        ShipSpaceCoordinates const & endCoords,
        StructuralMaterial const * material);

    void MoveRopeEndpointForEphemeralVisualization(
        size_t ropeElementIndex,
        ShipSpaceCoordinates const & oldCoords,
        ShipSpaceCoordinates const & newCoords);

    void RestoreRopesLayerForEphemeralVisualization(RopesLayerData const & sourceLayer);

    //
    // Texture
    //

    TextureLayerData const & GetTextureLayer() const;

    ImageSize const & GetTextureSize() const
    {
        assert(mModel.HasLayer(LayerType::Texture));

        return mModel.GetTextureLayer().Buffer.Size;
    }

    void SetTextureLayer(
        TextureLayerData && textureLayer,
        std::optional<std::string> originalTextureArtCredits);
    void RemoveTextureLayer();

    std::unique_ptr<TextureLayerData> CloneTextureLayer() const;

    void TextureRegionErase(ImageRect const & region);

    std::optional<ImageRect> TextureMagicWandEraseBackground(
        ImageCoordinates const & start,
        unsigned int tolerance,
        bool isAntiAlias,
        bool doContiguousOnly);

    void RestoreTextureLayerRegion(
        TextureLayerData && sourceLayerRegion,
        ImageRect const & sourceRegion,
        ImageCoordinates const & targetOrigin);

    void RestoreTextureLayer(
        std::unique_ptr<TextureLayerData> textureLayer,
        std::optional<std::string> originalTextureArtCredits);

    void TextureRegionEraseForEphemeralVisualization(ImageRect const & region);

    void RestoreTextureLayerRegionForEphemeralVisualization(
        TextureLayerData const & sourceLayerRegion,
        ImageRect const & sourceRegion,
        ImageCoordinates const & targetOrigin);


    //
    // Visualizations
    //

    void SetGameVisualizationMode(GameVisualizationModeType mode);
    void ForceWholeGameVisualizationRefresh();

    void SetStructuralLayerVisualizationMode(StructuralLayerVisualizationModeType mode);

    void SetElectricalLayerVisualizationMode(ElectricalLayerVisualizationModeType mode);

    void SetRopesLayerVisualizationMode(RopesLayerVisualizationModeType mode);

    void SetTextureLayerVisualizationMode(TextureLayerVisualizationModeType mode);

    void UpdateVisualizations(View & view);

private:

    ModelController(
        Model && model,
        ShipTexturizer const & shipTexturizer);

    inline ShipSpaceRect GetWholeShipRect() const
    {
        return ShipSpaceRect(mModel.GetShipSize());
    }

    inline ImageRect GetWholeTextureRect() const
    {
        assert(mModel.HasLayer(LayerType::Texture));
            
        return ImageRect(GetTextureSize());
    }

    void InitializeStructuralLayerAnalysis();

    void InitializeElectricalLayerAnalysis();

    void InitializeRopesLayerAnalysis();

    inline void WriteParticle(
        ShipSpaceCoordinates const & coords,
        StructuralMaterial const * material);

    inline void WriteParticle(
        ShipSpaceCoordinates const & coords,
        ElectricalMaterial const * material);

    inline void AppendRope(
        ShipSpaceCoordinates const & startCoords,
        ShipSpaceCoordinates const & endCoords,
        StructuralMaterial const * material);

    void MoveRopeEndpoint(
        RopeElement & ropeElement,
        ShipSpaceCoordinates const & oldCoords,
        ShipSpaceCoordinates const & newCoords);

    bool InternalEraseRopeAt(ShipSpaceCoordinates const & coords);

    template<LayerType TLayer>
    std::optional<ShipSpaceRect> DoFlood(
        ShipSpaceCoordinates const & start,
        typename LayerTypeTraits<TLayer>::material_type const * material,
        bool doContiguousOnly,
        typename LayerTypeTraits<TLayer>::layer_data_type const & layer);

    std::optional<ImageRect> DoTextureMagicWandEraseBackground(
        ImageCoordinates const & start,
        unsigned int tolerance,
        bool isAntiAlias,
        bool doContiguousOnly,
        TextureLayerData & layer);

    // Viz

    template<VisualizationType TVisualization, typename TRect>
    void RegisterDirtyVisualization(TRect const & region);

    ImageRect UpdateGameVisualization(ShipSpaceRect const & region);

    ImageRect UpdateStructuralLayerVisualization(ShipSpaceRect const & region);

    void RenderStructureInto(
        ShipSpaceRect const & structureRegion,
        RgbaImageData & texture) const;

    void UpdateElectricalLayerVisualization(ShipSpaceRect const & region);

    void UpdateRopesLayerVisualization();

    void UpdateTextureLayerVisualization();

private:

    Model mModel;

    ShipTexturizer const & mShipTexturizer;

    //
    // Auxiliary layers' members
    //

    size_t mMassParticleCount;
    float mTotalMass;
    vec2f mCenterOfMassSum;

    InstancedElectricalElementSet mInstancedElectricalElementSet;
    size_t mElectricalParticleCount;

    //
    // Visualizations
    //

    GameVisualizationModeType mGameVisualizationMode;
    std::unique_ptr<RgbaImageData> mGameVisualizationAutoTexturizationTexture;
    std::unique_ptr<RgbaImageData> mGameVisualizationTexture;
    int mGameVisualizationTextureMagnificationFactor;

    StructuralLayerVisualizationModeType mStructuralLayerVisualizationMode;
    std::unique_ptr<RgbaImageData> mStructuralLayerVisualizationTexture;

    ElectricalLayerVisualizationModeType mElectricalLayerVisualizationMode;
    std::unique_ptr<RgbaImageData> mElectricalLayerVisualizationTexture;

    RopesLayerVisualizationModeType mRopesLayerVisualizationMode;

    TextureLayerVisualizationModeType mTextureLayerVisualizationMode;

    // Regions whose visualization needs to be *updated* and uploaded
    std::optional<ShipSpaceRect> mDirtyGameVisualizationRegion;
    std::optional<ShipSpaceRect> mDirtyStructuralLayerVisualizationRegion;
    std::optional<ShipSpaceRect> mDirtyElectricalLayerVisualizationRegion;
    std::optional<ShipSpaceRect> mDirtyRopesLayerVisualizationRegion;
    std::optional<ImageRect> mDirtyTextureLayerVisualizationRegion;

    //
    // Debugging
    //

    bool mIsStructuralLayerInEphemeralVisualization;
    bool mIsElectricalLayerInEphemeralVisualization;
    bool mIsRopesLayerInEphemeralVisualization;
    bool mIsTextureLayerInEphemeralVisualization;
};

}