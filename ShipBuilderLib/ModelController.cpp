/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2021-08-28
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "ModelController.h"

#include "ModelValidator.h"

#include <cassert>
#include <queue>

namespace ShipBuilder {

std::unique_ptr<ModelController> ModelController::CreateNew(
    ShipSpaceSize const & shipSpaceSize,
    std::string const & shipName,
    ShipTexturizer const & shipTexturizer)
{
    Model model = Model(shipSpaceSize, shipName);

    return std::unique_ptr<ModelController>(
        new ModelController(
            std::move(model),
            shipTexturizer));
}

std::unique_ptr<ModelController> ModelController::CreateForShip(
    ShipDefinition && shipDefinition,
    ShipTexturizer const & shipTexturizer)
{
    Model model = Model(std::move(shipDefinition));

    return std::unique_ptr<ModelController>(
        new ModelController(
            std::move(model),
            shipTexturizer));
}

ModelController::ModelController(
    Model && model,
    ShipTexturizer const & shipTexturizer)
    : mModel(std::move(model))
    , mShipTexturizer(shipTexturizer)
    , mMassParticleCount(0)
    , mTotalMass(0.0f)
    , mCenterOfMassSum(vec2f::zero())
    , mInstancedElectricalElementSet()
    , mElectricalParticleCount(0)
    /////
    , mGameVisualizationMode(GameVisualizationModeType::None)
    , mGameVisualizationAutoTexturizationTexture()
    , mGameVisualizationTexture()
    , mGameVisualizationTextureMagnificationFactor(0)
    , mStructuralLayerVisualizationMode(StructuralLayerVisualizationModeType::None)
    , mStructuralLayerVisualizationTexture()
    , mElectricalLayerVisualizationMode(ElectricalLayerVisualizationModeType::None)
    , mElectricalLayerVisualizationTexture()
    , mRopesLayerVisualizationMode(RopesLayerVisualizationModeType::None)
    , mTextureLayerVisualizationMode(TextureLayerVisualizationModeType::None)
    /////
    , mIsStructuralLayerInEphemeralVisualization(false)
    , mIsElectricalLayerInEphemeralVisualization(false)
    , mIsRopesLayerInEphemeralVisualization(false)
    , mIsTextureLayerInEphemeralVisualization(false)
{
    // Model is not dirty now
    assert(!mModel.GetIsDirty());

    // Initialize layers' analyses
    InitializeStructuralLayerAnalysis();
    InitializeElectricalLayerAnalysis();
    InitializeRopesLayerAnalysis();
}

ShipDefinition ModelController::MakeShipDefinition() const
{
    return mModel.MakeShipDefinition();
}

std::unique_ptr<RgbaImageData> ModelController::MakePreview() const
{
    assert(mModel.HasLayer(LayerType::Structural));

    auto previewTexture = std::make_unique<RgbaImageData>(ImageSize(mModel.GetShipSize().width, mModel.GetShipSize().height));

    RenderStructureInto(
        GetWholeShipRect(),
        *previewTexture);

    return previewTexture;
}

std::optional<ShipSpaceRect> ModelController::CalculateBoundingBox() const
{
    std::optional<ShipSpaceRect> boundingBox;

    //
    // Structural layer
    //

    assert(mModel.HasLayer(LayerType::Structural));

    StructuralLayerData const & structuralLayer = mModel.GetStructuralLayer();

    for (int y = 0; y < structuralLayer.Buffer.Size.height; ++y)
    {
        for (int x = 0; x < structuralLayer.Buffer.Size.width; ++x)
        {
            ShipSpaceCoordinates coords(x, y);

            if (structuralLayer.Buffer[{x, y}].Material != nullptr)
            {
                if (!boundingBox.has_value())
                    boundingBox = ShipSpaceRect(coords);
                else
                    boundingBox->UnionWith(coords);
            }
        }
    }

    //
    // Ropes layer
    //

    if (mModel.HasLayer(LayerType::Ropes))
    {
        for (auto const & e : mModel.GetRopesLayer().Buffer)
        {
            if (!boundingBox.has_value())
                boundingBox = ShipSpaceRect(e.StartCoords);
            else
                boundingBox->UnionWith(e.StartCoords);

            boundingBox->UnionWith(e.EndCoords);
        }
    }

    return boundingBox;
}

ModelValidationResults ModelController::ValidateModel() const
{
    return ModelValidator::ValidateModel(mModel);
}

std::optional<SampledInformation> ModelController::SampleInformationAt(ShipSpaceCoordinates const & coordinates, LayerType layer) const
{
    switch (layer)
    {
        case LayerType::Electrical:
        {
            assert(mModel.HasLayer(LayerType::Electrical));
            assert(!mIsElectricalLayerInEphemeralVisualization);
            assert(coordinates.IsInSize(GetShipSize()));

            ElectricalElement const & element = mModel.GetElectricalLayer().Buffer[coordinates];
            if (element.Material != nullptr)
            {
                assert((element.InstanceIndex != NoneElectricalElementInstanceIndex && element.Material->IsInstanced)
                    || (element.InstanceIndex == NoneElectricalElementInstanceIndex && !element.Material->IsInstanced));

                return SampledInformation(
                    element.Material->Name,
                    element.Material->IsInstanced ? element.InstanceIndex : std::optional<ElectricalElementInstanceIndex>());
            }
            else
            {
                return std::nullopt;
            }
        }

        case LayerType::Ropes:
        {
            assert(mModel.HasLayer(LayerType::Ropes));
            assert(!mIsRopesLayerInEphemeralVisualization);
            assert(coordinates.IsInSize(mModel.GetShipSize()));

            auto const * material = mModel.GetRopesLayer().Buffer.SampleMaterialEndpointAt(coordinates);
            if (material != nullptr)
            {
                return SampledInformation(material->Name, std::nullopt);
            }
            else
            {
                return std::nullopt;
            }
        }

        case LayerType::Structural:
        {
            assert(mModel.HasLayer(LayerType::Structural));
            assert(!mIsStructuralLayerInEphemeralVisualization);
            assert(coordinates.IsInSize(GetShipSize()));

            auto const * material = mModel.GetStructuralLayer().Buffer[coordinates].Material;
            if (material != nullptr)
            {
                return SampledInformation(material->Name, std::nullopt);
            }
            else
            {
                return std::nullopt;
            }
        }

        case LayerType::Texture:
        {
            // Nothing to do
            return std::nullopt;
        }
    }

    assert(false);
    return std::nullopt;
}

void ModelController::Flip(DirectionType direction)
{
    // Structural layer
    {
        assert(mModel.HasLayer(LayerType::Structural));

        assert(!mIsStructuralLayerInEphemeralVisualization);

        mModel.GetStructuralLayer().Buffer.Flip(direction);

        RegisterDirtyVisualization<VisualizationType::StructuralLayer>(GetWholeShipRect());

        InitializeStructuralLayerAnalysis();
    }

    // Electrical layer
    if (mModel.HasLayer(LayerType::Electrical))
    {
        assert(!mIsElectricalLayerInEphemeralVisualization);

        mModel.GetElectricalLayer().Buffer.Flip(direction);

        RegisterDirtyVisualization<VisualizationType::ElectricalLayer>(GetWholeShipRect());

        InitializeElectricalLayerAnalysis();
    }

    // Ropes layer
    if (mModel.HasLayer(LayerType::Ropes))
    {
        assert(!mIsRopesLayerInEphemeralVisualization);

        mModel.GetRopesLayer().Buffer.Flip(direction, mModel.GetShipSize());

        RegisterDirtyVisualization<VisualizationType::RopesLayer>(GetWholeShipRect());

        InitializeRopesLayerAnalysis();
    }

    // Texture layer
    if (mModel.HasLayer(LayerType::Texture))
    {
        assert(!mIsTextureLayerInEphemeralVisualization);

        mModel.GetTextureLayer().Buffer.Flip(direction);

        RegisterDirtyVisualization<VisualizationType::TextureLayer>(GetWholeTextureRect());
    }

    //...and Game we do regardless, as there's always a structural layer at least
    RegisterDirtyVisualization<VisualizationType::Game>(GetWholeShipRect());
}

void ModelController::Rotate90(RotationDirectionType direction)
{
    // Rotate size
    auto const originalSize = mModel.GetShipSize();
    mModel.SetShipSize(ShipSpaceSize(originalSize.height, originalSize.width));

    // Structural layer
    {
        assert(mModel.HasLayer(LayerType::Structural));

        assert(!mIsStructuralLayerInEphemeralVisualization);

        mModel.GetStructuralLayer().Buffer.Rotate90(direction);

        mStructuralLayerVisualizationTexture.reset();
        RegisterDirtyVisualization<VisualizationType::StructuralLayer>(GetWholeShipRect());

        InitializeStructuralLayerAnalysis();
    }

    // Electrical layer
    if (mModel.HasLayer(LayerType::Electrical))
    {
        assert(!mIsElectricalLayerInEphemeralVisualization);

        mModel.GetElectricalLayer().Buffer.Rotate90(direction);

        mElectricalLayerVisualizationTexture.reset();
        RegisterDirtyVisualization<VisualizationType::ElectricalLayer>(GetWholeShipRect());

        InitializeElectricalLayerAnalysis();
    }

    // Ropes layer
    if (mModel.HasLayer(LayerType::Ropes))
    {
        assert(!mIsRopesLayerInEphemeralVisualization);

        mModel.GetRopesLayer().Buffer.Rotate90(direction, originalSize);

        RegisterDirtyVisualization<VisualizationType::RopesLayer>(GetWholeShipRect());

        InitializeRopesLayerAnalysis();
    }

    // Texture layer
    if (mModel.HasLayer(LayerType::Texture))
    {
        assert(!mIsTextureLayerInEphemeralVisualization);

        mModel.GetTextureLayer().Buffer.Rotate90(direction);

        RegisterDirtyVisualization<VisualizationType::TextureLayer>(GetWholeTextureRect());
    }

    //...and Game we do regardless, as there's always a structural layer at least
    mGameVisualizationTexture.reset();
    mGameVisualizationAutoTexturizationTexture.reset();
    RegisterDirtyVisualization<VisualizationType::Game>(GetWholeShipRect());
}

void ModelController::ResizeShip(
    ShipSpaceSize const & newSize,
    ShipSpaceCoordinates const & originOffset)
{
    auto const originalShipSize = mModel.GetShipSize();

    ShipSpaceRect newWholeShipRect({ 0, 0 }, newSize);

    mModel.SetShipSize(newSize);

    // Structural layer
    {
        assert(mModel.HasLayer(LayerType::Structural));

        assert(!mIsStructuralLayerInEphemeralVisualization);

        mModel.SetStructuralLayer(
            mModel.GetStructuralLayer().MakeReframed(
                newSize,
                originOffset,
                StructuralElement(nullptr)));

        InitializeStructuralLayerAnalysis();

        // Initialize visualization
        mStructuralLayerVisualizationTexture.reset();
        RegisterDirtyVisualization<VisualizationType::StructuralLayer>(newWholeShipRect);
    }

    // Electrical layer
    if (mModel.HasLayer(LayerType::Electrical))
    {
        assert(!mIsElectricalLayerInEphemeralVisualization);

        mModel.SetElectricalLayer(
            mModel.GetElectricalLayer().MakeReframed(
                newSize,
                originOffset,
                ElectricalElement(nullptr, NoneElectricalElementInstanceIndex)));

        InitializeElectricalLayerAnalysis();

        // Initialize visualization
        mElectricalLayerVisualizationTexture.reset();
        RegisterDirtyVisualization<VisualizationType::ElectricalLayer>(newWholeShipRect);
    }

    // Ropes layer
    if (mModel.HasLayer(LayerType::Ropes))
    {
        assert(!mIsRopesLayerInEphemeralVisualization);

        mModel.SetRopesLayer(
            mModel.GetRopesLayer().MakeReframed(
                newSize,
                originOffset));

        InitializeRopesLayerAnalysis();

        RegisterDirtyVisualization<VisualizationType::RopesLayer>(newWholeShipRect);
    }

    // Texture layer
    if (mModel.HasLayer(LayerType::Texture))
    {
        assert(!mIsTextureLayerInEphemeralVisualization);

        // Convert (scale) rect to texture coordinates space
        vec2f const shipToImage(
            static_cast<float>(mModel.GetTextureLayer().Buffer.Size.width) / static_cast<float>(originalShipSize.width),
            static_cast<float>(mModel.GetTextureLayer().Buffer.Size.height) / static_cast<float>(originalShipSize.height));
        ImageSize const imageNewSize = ImageSize::FromFloatRound(newSize.ToFloat().scale(shipToImage));
        ImageCoordinates imageOriginOffset = ImageCoordinates::FromFloatRound(originOffset.ToFloat().scale(shipToImage));

        mModel.SetTextureLayer(
            mModel.GetTextureLayer().MakeReframed(
                imageNewSize,
                imageOriginOffset,
                rgbaColor(0, 0, 0, 0)));

        RegisterDirtyVisualization<VisualizationType::TextureLayer>(GetWholeTextureRect());
    }

    // Initialize game visualizations
    {
        mGameVisualizationTexture.reset();
        mGameVisualizationAutoTexturizationTexture.reset();
        RegisterDirtyVisualization<VisualizationType::Game>(newWholeShipRect);
    }

    assert(mModel.GetShipSize() == newSize);
    assert(GetWholeShipRect() == newWholeShipRect);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Structural
////////////////////////////////////////////////////////////////////////////////////////////////////

StructuralLayerData const & ModelController::GetStructuralLayer() const
{
    assert(mModel.HasLayer(LayerType::Structural));
    assert(!mIsStructuralLayerInEphemeralVisualization);

    return mModel.GetStructuralLayer();
}

void ModelController::SetStructuralLayer(StructuralLayerData && structuralLayer)
{
    assert(mModel.HasLayer(LayerType::Structural));

    mModel.SetStructuralLayer(std::move(structuralLayer));

    InitializeStructuralLayerAnalysis();

    RegisterDirtyVisualization<VisualizationType::Game>(GetWholeShipRect());
    RegisterDirtyVisualization<VisualizationType::StructuralLayer>(GetWholeShipRect());

    mIsStructuralLayerInEphemeralVisualization = false;
}

StructuralLayerData ModelController::CloneStructuralLayer() const
{
    return mModel.CloneStructuralLayer();
}

StructuralMaterial const * ModelController::SampleStructuralMaterialAt(ShipSpaceCoordinates const & coords) const
{
    assert(mModel.HasLayer(LayerType::Structural));
    assert(!mIsStructuralLayerInEphemeralVisualization);

    assert(coords.IsInSize(mModel.GetShipSize()));

    return mModel.GetStructuralLayer().Buffer[coords].Material;
}

void ModelController::StructuralRegionFill(
    ShipSpaceRect const & region,
    StructuralMaterial const * material)
{
    assert(mModel.HasLayer(LayerType::Structural));
    assert(!mIsStructuralLayerInEphemeralVisualization);

    //
    // Update model
    //

    for (int y = region.origin.y; y < region.origin.y + region.size.height; ++y)
    {
        for (int x = region.origin.x; x < region.origin.x + region.size.width; ++x)
        {
            WriteParticle(ShipSpaceCoordinates(x, y), material);
        }
    }

    //
    // Update visualization
    //

    RegisterDirtyVisualization<VisualizationType::Game>(region);
    RegisterDirtyVisualization<VisualizationType::StructuralLayer>(region);
}

std::optional<ShipSpaceRect> ModelController::StructuralFlood(
    ShipSpaceCoordinates const & start,
    StructuralMaterial const * material,
    bool doContiguousOnly)
{
    assert(mModel.HasLayer(LayerType::Structural));

    assert(!mIsStructuralLayerInEphemeralVisualization);

    //
    // Update model
    //

    std::optional<ShipSpaceRect> affectedRect = DoFlood<LayerType::Structural>(
        start,
        material,
        doContiguousOnly,
        mModel.GetStructuralLayer());

    if (affectedRect.has_value())
    {
        //
        // Update visualization
        //

        RegisterDirtyVisualization<VisualizationType::Game>(*affectedRect);
        RegisterDirtyVisualization<VisualizationType::StructuralLayer>(*affectedRect);
    }

    return affectedRect;
}

void ModelController::RestoreStructuralLayerRegion(
    StructuralLayerData && sourceLayerRegion,
    ShipSpaceRect const & sourceRegion,
    ShipSpaceCoordinates const & targetOrigin)
{
    assert(mModel.HasLayer(LayerType::Structural));

    assert(!mIsStructuralLayerInEphemeralVisualization);

    //
    // Restore model
    //

    mModel.GetStructuralLayer().Buffer.BlitFromRegion(
        sourceLayerRegion.Buffer,
        sourceRegion,
        targetOrigin);

    //
    // Re-initialize layer analysis
    //

    InitializeStructuralLayerAnalysis();

    //
    // Update visualization
    //

    RegisterDirtyVisualization<VisualizationType::Game>(ShipSpaceRect(targetOrigin, sourceRegion.size));
    RegisterDirtyVisualization<VisualizationType::StructuralLayer>(ShipSpaceRect(targetOrigin, sourceRegion.size));
}

void ModelController::RestoreStructuralLayer(StructuralLayerData && sourceLayer)
{
    assert(!mIsStructuralLayerInEphemeralVisualization);

    //
    // Restore model
    //

    mModel.RestoreStructuralLayer(std::move(sourceLayer));

    //
    // Re-initialize layer analysis
    //

    InitializeStructuralLayerAnalysis();

    //
    // Update visualization
    //

    mGameVisualizationTexture.reset();
    mGameVisualizationAutoTexturizationTexture.reset();
    RegisterDirtyVisualization<VisualizationType::Game>(GetWholeShipRect());
    mStructuralLayerVisualizationTexture.reset();
    RegisterDirtyVisualization<VisualizationType::StructuralLayer>(GetWholeShipRect());
}

void ModelController::StructuralRegionFillForEphemeralVisualization(
    ShipSpaceRect const & region,
    StructuralMaterial const * material)
{
    assert(mModel.HasLayer(LayerType::Structural));

    //
    // Update model with just material - no analyses
    //

    auto & structuralLayerBuffer = mModel.GetStructuralLayer().Buffer;

    for (int y = region.origin.y; y < region.origin.y + region.size.height; ++y)
    {
        for (int x = region.origin.x; x < region.origin.x + region.size.width; ++x)
        {
            structuralLayerBuffer[ShipSpaceCoordinates(x, y)].Material = material;
        }
    }

    //
    // Update visualization
    //

    RegisterDirtyVisualization<VisualizationType::Game>(region);
    RegisterDirtyVisualization<VisualizationType::StructuralLayer>(region);

    // Remember we are in temp visualization now
    mIsStructuralLayerInEphemeralVisualization = true;
}

void ModelController::RestoreStructuralLayerRegionForEphemeralVisualization(
    StructuralLayerData const & sourceLayerRegion,
    ShipSpaceRect const & sourceRegion,
    ShipSpaceCoordinates const & targetOrigin)
{
    assert(mModel.HasLayer(LayerType::Structural));

    assert(mIsStructuralLayerInEphemeralVisualization);

    //
    // Restore model, and nothing else
    //

    mModel.GetStructuralLayer().Buffer.BlitFromRegion(
        sourceLayerRegion.Buffer,
        sourceRegion,
        targetOrigin);

    //
    // Update visualization
    //

    RegisterDirtyVisualization<VisualizationType::Game>(ShipSpaceRect(targetOrigin, sourceRegion.size));
    RegisterDirtyVisualization<VisualizationType::StructuralLayer>(ShipSpaceRect(targetOrigin, sourceRegion.size));

    // Remember we are not anymore in temp visualization
    mIsStructuralLayerInEphemeralVisualization = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Electrical
////////////////////////////////////////////////////////////////////////////////////////////////////

ElectricalPanelMetadata const & ModelController::GetElectricalPanelMetadata() const
{
    assert(mModel.HasLayer(LayerType::Electrical));

    return mModel.GetElectricalLayer().Panel;
}

void ModelController::SetElectricalLayer(ElectricalLayerData && electricalLayer)
{
    mModel.SetElectricalLayer(std::move(electricalLayer));

    InitializeElectricalLayerAnalysis();

    RegisterDirtyVisualization<VisualizationType::ElectricalLayer>(GetWholeShipRect());

    mIsElectricalLayerInEphemeralVisualization = false;
}

void ModelController::RemoveElectricalLayer()
{
    assert(mModel.HasLayer(LayerType::Electrical));

    mModel.RemoveElectricalLayer();

    InitializeElectricalLayerAnalysis();

    RegisterDirtyVisualization<VisualizationType::ElectricalLayer>(GetWholeShipRect());

    mIsElectricalLayerInEphemeralVisualization = false;
}

std::unique_ptr<ElectricalLayerData> ModelController::CloneElectricalLayer() const
{
    return mModel.CloneElectricalLayer();
}

ElectricalMaterial const * ModelController::SampleElectricalMaterialAt(ShipSpaceCoordinates const & coords) const
{
    assert(mModel.HasLayer(LayerType::Electrical));
    assert(!mIsElectricalLayerInEphemeralVisualization);

    assert(coords.IsInSize(mModel.GetShipSize()));

    return mModel.GetElectricalLayer().Buffer[coords].Material;
}

bool ModelController::IsElectricalParticleAllowedAt(ShipSpaceCoordinates const & coords) const
{
    assert(mModel.HasLayer(LayerType::Structural));
    assert(!mIsStructuralLayerInEphemeralVisualization);

    return mModel.GetStructuralLayer().Buffer[coords].Material != nullptr;
}

std::optional<ShipSpaceRect> ModelController::TrimElectricalParticlesWithoutSubstratum()
{
    assert(mModel.HasLayer(LayerType::Electrical));

    assert(!mIsElectricalLayerInEphemeralVisualization);

    //
    // Update model
    //

    std::optional<ShipSpaceRect> affectedRect;

    StructuralLayerData const & structuralLayer = mModel.GetStructuralLayer();
    ElectricalLayerData const & electricalLayer = mModel.GetElectricalLayer();

    assert(structuralLayer.Buffer.Size == electricalLayer.Buffer.Size);

    ElectricalMaterial const * nullMaterial = nullptr;

    for (int y = 0; y < structuralLayer.Buffer.Size.height; ++y)
    {
        for (int x = 0; x < structuralLayer.Buffer.Size.width; ++x)
        {
            auto const coords = ShipSpaceCoordinates(x, y);
            if (electricalLayer.Buffer[coords].Material != nullptr
                && structuralLayer.Buffer[coords].Material == nullptr)
            {
                WriteParticle(coords, nullMaterial);

                if (!affectedRect.has_value())
                {
                    affectedRect = ShipSpaceRect(coords);
                }
                else
                {
                    affectedRect->UnionWith(coords);
                }
            }
        }
    }

    //
    // Update visualization
    //

    if (affectedRect.has_value())
    {
        RegisterDirtyVisualization<VisualizationType::ElectricalLayer>(*affectedRect);
    }

    return affectedRect;
}

void ModelController::ElectricalRegionFill(
    ShipSpaceRect const & region,
    ElectricalMaterial const * material)
{
    assert(mModel.HasLayer(LayerType::Electrical));

    assert(!mIsElectricalLayerInEphemeralVisualization);

    //
    // Update model
    //

    for (int y = region.origin.y; y < region.origin.y + region.size.height; ++y)
    {
        for (int x = region.origin.x; x < region.origin.x + region.size.width; ++x)
        {
            WriteParticle(ShipSpaceCoordinates(x, y), material);
        }
    }

    //
    // Update visualization
    //

    RegisterDirtyVisualization<VisualizationType::ElectricalLayer>(region);
}

void ModelController::RestoreElectricalLayerRegion(
    ElectricalLayerData && sourceLayerRegion,
    ShipSpaceRect const & sourceRegion,
    ShipSpaceCoordinates const & targetOrigin)
{
    assert(mModel.HasLayer(LayerType::Electrical));

    assert(!mIsElectricalLayerInEphemeralVisualization);

    //
    // Restore model
    //

    mModel.GetElectricalLayer().Buffer.BlitFromRegion(
        sourceLayerRegion.Buffer,
        sourceRegion,
        targetOrigin);

    mModel.GetElectricalLayer().Panel = std::move(sourceLayerRegion.Panel);

    //
    // Re-initialize layer analysis (and instance IDs)
    //

    InitializeElectricalLayerAnalysis();

    //
    // Update visualization
    //

    RegisterDirtyVisualization<VisualizationType::ElectricalLayer>(ShipSpaceRect(targetOrigin, sourceRegion.size));
}

void ModelController::RestoreElectricalLayer(std::unique_ptr<ElectricalLayerData> sourceLayer)
{
    assert(!mIsElectricalLayerInEphemeralVisualization);

    //
    // Restore model
    //

    mModel.RestoreElectricalLayer(std::move(sourceLayer));

    //
    // Re-initialize layer analysis (and instance IDs)
    //

    InitializeElectricalLayerAnalysis();

    //
    // Update visualization
    //

    mElectricalLayerVisualizationTexture.reset();
    RegisterDirtyVisualization<VisualizationType::ElectricalLayer>(GetWholeShipRect());
}

void ModelController::ElectricalRegionFillForEphemeralVisualization(
    ShipSpaceRect const & region,
    ElectricalMaterial const * material)
{
    assert(mModel.HasLayer(LayerType::Electrical));

    //
    // Update model just with material - no instance ID, no analyses, no panel
    //

    auto & electricalLayerBuffer = mModel.GetElectricalLayer().Buffer;

    for (int y = region.origin.y; y < region.origin.y + region.size.height; ++y)
    {
        for (int x = region.origin.x; x < region.origin.x + region.size.width; ++x)
        {
            electricalLayerBuffer[ShipSpaceCoordinates(x, y)].Material = material;
        }
    }

    //
    // Update visualization
    //

    RegisterDirtyVisualization<VisualizationType::ElectricalLayer>(region);

    // Remember we are in temp visualization now
    mIsElectricalLayerInEphemeralVisualization = true;
}

void ModelController::RestoreElectricalLayerRegionForEphemeralVisualization(
    ElectricalLayerData const & sourceLayerRegion,
    ShipSpaceRect const & sourceRegion,
    ShipSpaceCoordinates const & targetOrigin)
{
    assert(mModel.HasLayer(LayerType::Electrical));

    assert(mIsElectricalLayerInEphemeralVisualization);

    //
    // Restore model, and nothing else
    //

    mModel.GetElectricalLayer().Buffer.BlitFromRegion(
        sourceLayerRegion.Buffer,
        sourceRegion,
        targetOrigin);

    //
    // Update visualization
    //

    RegisterDirtyVisualization<VisualizationType::ElectricalLayer>(ShipSpaceRect(targetOrigin, sourceRegion.size));

    // Remember we are not anymore in temp visualization
    mIsElectricalLayerInEphemeralVisualization = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Ropes
////////////////////////////////////////////////////////////////////////////////////////////////////

void ModelController::SetRopesLayer(RopesLayerData && ropesLayer)
{
    mModel.SetRopesLayer(std::move(ropesLayer));

    InitializeRopesLayerAnalysis();

    RegisterDirtyVisualization<VisualizationType::RopesLayer>(GetWholeShipRect());

    mIsRopesLayerInEphemeralVisualization = false;
}

void ModelController::RemoveRopesLayer()
{
    assert(mModel.HasLayer(LayerType::Ropes));

    mModel.RemoveRopesLayer();

    InitializeRopesLayerAnalysis();

    RegisterDirtyVisualization<VisualizationType::RopesLayer>(GetWholeShipRect());

    mIsRopesLayerInEphemeralVisualization = false;
}

std::unique_ptr<RopesLayerData> ModelController::CloneRopesLayer() const
{
    return mModel.CloneRopesLayer();
}

StructuralMaterial const * ModelController::SampleRopesMaterialAt(ShipSpaceCoordinates const & coords) const
{
    assert(mModel.HasLayer(LayerType::Ropes));
    assert(!mIsRopesLayerInEphemeralVisualization);

    assert(coords.IsInSize(mModel.GetShipSize()));

    return mModel.GetRopesLayer().Buffer.SampleMaterialEndpointAt(coords);
}

std::optional<size_t> ModelController::GetRopeElementIndexAt(ShipSpaceCoordinates const & coords) const
{
    assert(mModel.HasLayer(LayerType::Ropes));

    assert(!mIsRopesLayerInEphemeralVisualization);

    auto const searchIt = std::find_if(
        mModel.GetRopesLayer().Buffer.cbegin(),
        mModel.GetRopesLayer().Buffer.cend(),
        [&coords](RopeElement const & e)
        {
            return coords == e.StartCoords
                || coords == e.EndCoords;
        });

    if (searchIt != mModel.GetRopesLayer().Buffer.cend())
    {
        return std::distance(mModel.GetRopesLayer().Buffer.cbegin(), searchIt);
    }
    else
    {
        return std::nullopt;
    }
}

void ModelController::AddRope(
    ShipSpaceCoordinates const & startCoords,
    ShipSpaceCoordinates const & endCoords,
    StructuralMaterial const * material)
{
    assert(mModel.HasLayer(LayerType::Ropes));

    assert(!mIsRopesLayerInEphemeralVisualization);

    //
    // Update model
    //

    AppendRope(
        startCoords,
        endCoords,
        material);

    //
    // Update visualization
    //

    RegisterDirtyVisualization<VisualizationType::RopesLayer>(GetWholeShipRect());
}

void ModelController::MoveRopeEndpoint(
    size_t ropeElementIndex,
    ShipSpaceCoordinates const & oldCoords,
    ShipSpaceCoordinates const & newCoords)
{
    assert(mModel.HasLayer(LayerType::Ropes));

    assert(!mIsRopesLayerInEphemeralVisualization);

    //
    // Update model
    //

    assert(ropeElementIndex < mModel.GetRopesLayer().Buffer.GetSize());

    MoveRopeEndpoint(
        mModel.GetRopesLayer().Buffer[ropeElementIndex],
        oldCoords,
        newCoords);

    //
    // Update visualization
    //

    RegisterDirtyVisualization<VisualizationType::RopesLayer>(GetWholeShipRect());
}

bool ModelController::EraseRopeAt(ShipSpaceCoordinates const & coords)
{
    assert(mModel.HasLayer(LayerType::Ropes));

    assert(!mIsRopesLayerInEphemeralVisualization);

    //
    // Update model
    //

    bool const hasRemoved = InternalEraseRopeAt(coords);

    if (hasRemoved)
    {
        // Update visualization
        RegisterDirtyVisualization<VisualizationType::RopesLayer>(GetWholeShipRect());

        return true;
    }
    else
    {
        return false;
    }
}

void ModelController::RestoreRopesLayer(std::unique_ptr<RopesLayerData> sourceLayer)
{
    assert(!mIsRopesLayerInEphemeralVisualization);

    //
    // Restore model
    //

    mModel.RestoreRopesLayer(std::move(sourceLayer));

    //
    // Re-initialize layer analysis
    //

    InitializeRopesLayerAnalysis();

    //
    // Update visualization
    //

    RegisterDirtyVisualization<VisualizationType::RopesLayer>(GetWholeShipRect());
}

void ModelController::AddRopeForEphemeralVisualization(
    ShipSpaceCoordinates const & startCoords,
    ShipSpaceCoordinates const & endCoords,
    StructuralMaterial const * material)
{
    assert(mModel.HasLayer(LayerType::Ropes));

    //
    // Update model with just material - no analyses
    //

    AppendRope(
        startCoords,
        endCoords,
        material);

    //
    // Update visualization
    //

    RegisterDirtyVisualization<VisualizationType::RopesLayer>(GetWholeShipRect());

    // Remember we are in temp visualization now
    mIsRopesLayerInEphemeralVisualization = true;
}

void ModelController::ModelController::MoveRopeEndpointForEphemeralVisualization(
    size_t ropeElementIndex,
    ShipSpaceCoordinates const & oldCoords,
    ShipSpaceCoordinates const & newCoords)
{
    assert(mModel.HasLayer(LayerType::Ropes));

    //
    // Update model with jsut movement - no analyses
    //

    assert(ropeElementIndex < mModel.GetRopesLayer().Buffer.GetSize());

    MoveRopeEndpoint(
        mModel.GetRopesLayer().Buffer[ropeElementIndex],
        oldCoords,
        newCoords);

    //
    // Update visualization
    //

    RegisterDirtyVisualization<VisualizationType::RopesLayer>(GetWholeShipRect());

    // Remember we are in temp visualization now
    mIsRopesLayerInEphemeralVisualization = true;
}

void ModelController::RestoreRopesLayerForEphemeralVisualization(RopesLayerData const & sourceLayer)
{
    assert(mModel.HasLayer(LayerType::Ropes));

    assert(mIsRopesLayerInEphemeralVisualization);

    //
    // Restore model, and nothing else
    //

    mModel.GetRopesLayer().Buffer = sourceLayer.Buffer;

    //
    // Update visualization
    //

    RegisterDirtyVisualization<VisualizationType::RopesLayer>(GetWholeShipRect());

    // Remember we are not anymore in temp visualization
    mIsRopesLayerInEphemeralVisualization = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Texture
////////////////////////////////////////////////////////////////////////////////////////////////////

TextureLayerData const & ModelController::GetTextureLayer() const
{
    assert(mModel.HasLayer(LayerType::Texture));

    return mModel.GetTextureLayer();
}

void ModelController::SetTextureLayer(
    TextureLayerData && textureLayer,
    std::optional<std::string> originalTextureArtCredits)
{
    mModel.SetTextureLayer(std::move(textureLayer));
    mModel.GetShipMetadata().ArtCredits = std::move(originalTextureArtCredits);

    RegisterDirtyVisualization<VisualizationType::Game>(GetWholeShipRect());
    RegisterDirtyVisualization<VisualizationType::TextureLayer>(GetWholeTextureRect());

    mIsTextureLayerInEphemeralVisualization = false;
}

void ModelController::RemoveTextureLayer()
{
    assert(mModel.HasLayer(LayerType::Texture));

    auto const oldWholeTextureRect = GetWholeTextureRect();

    mModel.RemoveTextureLayer();
    mModel.GetShipMetadata().ArtCredits.reset();

    RegisterDirtyVisualization<VisualizationType::Game>(GetWholeShipRect());
    RegisterDirtyVisualization<VisualizationType::TextureLayer>(oldWholeTextureRect);

    mIsTextureLayerInEphemeralVisualization = false;
}

std::unique_ptr<TextureLayerData> ModelController::CloneTextureLayer() const
{
    return mModel.CloneTextureLayer();
}

void ModelController::TextureRegionErase(ImageRect const & region)
{
    assert(mModel.HasLayer(LayerType::Texture));

    assert(!mIsTextureLayerInEphemeralVisualization);

    assert(region.IsContainedInRect(mModel.GetTextureLayer().Buffer.Size));

    //
    // Update model
    //

    auto & textureLayerBuffer = mModel.GetTextureLayer().Buffer;

    for (int y = region.origin.y; y < region.origin.y + region.size.height; ++y)
    {
        for (int x = region.origin.x; x < region.origin.x + region.size.width; ++x)
        {
            textureLayerBuffer[{x, y}].a = 0;
        }
    }

    //
    // Update visualization
    //

    RegisterDirtyVisualization<VisualizationType::TextureLayer>(region);
}

std::optional<ImageRect> ModelController::TextureMagicWandEraseBackground(
    ImageCoordinates const & start,
    unsigned int tolerance,
    bool isAntiAlias,
    bool doContiguousOnly)
{
    assert(mModel.HasLayer(LayerType::Texture));

    assert(!mIsTextureLayerInEphemeralVisualization);

    //
    // Update model
    //

    std::optional<ImageRect> affectedRect = DoTextureMagicWandEraseBackground(
        start,
        tolerance,
        isAntiAlias,
        doContiguousOnly,
        mModel.GetTextureLayer());

    if (affectedRect.has_value())
    {
        //
        // Update visualization
        //

        RegisterDirtyVisualization<VisualizationType::TextureLayer>(*affectedRect);
    }

    return affectedRect;
}

void ModelController::RestoreTextureLayerRegion(
    TextureLayerData && sourceLayerRegion,
    ImageRect const & sourceRegion,
    ImageCoordinates const & targetOrigin)
{
    assert(mModel.HasLayer(LayerType::Texture));

    assert(!mIsTextureLayerInEphemeralVisualization);

    //
    // Restore model
    //

    mModel.GetTextureLayer().Buffer.BlitFromRegion(
        sourceLayerRegion.Buffer,
        sourceRegion,
        targetOrigin);

    //
    // Update visualization
    //

    RegisterDirtyVisualization<VisualizationType::TextureLayer>(GetWholeTextureRect());
}

void ModelController::RestoreTextureLayer(
    std::unique_ptr<TextureLayerData> textureLayer,
    std::optional<std::string> originalTextureArtCredits)
{
    assert(!mIsTextureLayerInEphemeralVisualization);

    //
    // Calculate largest affected rect between before and after
    //

    ImageRect dirtyTextureRect;
    if (mModel.HasLayer(LayerType::Texture))
    {
        dirtyTextureRect = mModel.GetTextureLayer().Buffer.Size;

        if (textureLayer)
        {
            // Largest
            dirtyTextureRect.UnionWith(textureLayer->Buffer.Size);
        }
    }
    else if (textureLayer)
    {
        dirtyTextureRect = textureLayer->Buffer.Size;
    }

    //
    // Restore model
    //

    mModel.RestoreTextureLayer(std::move(textureLayer));
    mModel.GetShipMetadata().ArtCredits = std::move(originalTextureArtCredits);

    //
    // Update visualization
    //

    mGameVisualizationTexture.reset();
    mGameVisualizationAutoTexturizationTexture.release();
    RegisterDirtyVisualization<VisualizationType::Game>(GetWholeShipRect());
    RegisterDirtyVisualization<VisualizationType::TextureLayer>(dirtyTextureRect);
}

void ModelController::TextureRegionEraseForEphemeralVisualization(ImageRect const & region)
{
    assert(mModel.HasLayer(LayerType::Texture));

    //
    // Update model
    //

    auto & textureLayerBuffer = mModel.GetTextureLayer().Buffer;

    for (int y = region.origin.y; y < region.origin.y + region.size.height; ++y)
    {
        for (int x = region.origin.x; x < region.origin.x + region.size.width; ++x)
        {
            textureLayerBuffer[{x, y}].a = 0;
        }
    }

    //
    // Update visualization
    //

    RegisterDirtyVisualization<VisualizationType::TextureLayer>(region);

    // Remember we are in temp visualization now
    mIsTextureLayerInEphemeralVisualization = true;
}

void ModelController::RestoreTextureLayerRegionForEphemeralVisualization(
    TextureLayerData const & sourceLayerRegion,
    ImageRect const & sourceRegion,
    ImageCoordinates const & targetOrigin)
{
    assert(mModel.HasLayer(LayerType::Texture));

    assert(mIsTextureLayerInEphemeralVisualization);

    //
    // Restore model, and nothing else
    //

    mModel.GetTextureLayer().Buffer.BlitFromRegion(
        sourceLayerRegion.Buffer,
        sourceRegion,
        targetOrigin);

    //
    // Update visualization
    //

    RegisterDirtyVisualization<VisualizationType::TextureLayer>(ImageRect(targetOrigin, sourceRegion.size));

    // Remember we are not anymore in temp visualization
    mIsTextureLayerInEphemeralVisualization = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Visualizations
////////////////////////////////////////////////////////////////////////////////////////////////////

void ModelController::SetGameVisualizationMode(GameVisualizationModeType mode)
{
    if (mode == mGameVisualizationMode)
    {
        // Nop
        return;
    }

    if (mode != GameVisualizationModeType::None)
    {
        if (mode != GameVisualizationModeType::AutoTexturizationMode)
        {
            mGameVisualizationAutoTexturizationTexture.reset();
        }

        mGameVisualizationMode = mode;

        RegisterDirtyVisualization<VisualizationType::Game>(GetWholeShipRect());
    }
    else
    {
        // Shutdown game visualization
        mGameVisualizationMode = GameVisualizationModeType::None;
        mGameVisualizationAutoTexturizationTexture.reset();
        mGameVisualizationTexture.reset();
    }
}

void ModelController::ForceWholeGameVisualizationRefresh()
{
    if (mGameVisualizationMode != GameVisualizationModeType::None)
    {
        RegisterDirtyVisualization<VisualizationType::Game>(GetWholeShipRect());
    }
}

void ModelController::SetStructuralLayerVisualizationMode(StructuralLayerVisualizationModeType mode)
{
    if (mode == mStructuralLayerVisualizationMode)
    {
        // Nop
        return;
    }

    if (mode != StructuralLayerVisualizationModeType::None)
    {
        mStructuralLayerVisualizationMode = mode;

        RegisterDirtyVisualization<VisualizationType::StructuralLayer>(GetWholeShipRect());
    }
    else
    {
        // Shutdown structural visualization
        mStructuralLayerVisualizationMode = StructuralLayerVisualizationModeType::None;
        mStructuralLayerVisualizationTexture.reset();
    }
}

void ModelController::SetElectricalLayerVisualizationMode(ElectricalLayerVisualizationModeType mode)
{
    if (mode == mElectricalLayerVisualizationMode)
    {
        // Nop
        return;
    }

    if (mode != ElectricalLayerVisualizationModeType::None)
    {
        mElectricalLayerVisualizationMode = mode;

        RegisterDirtyVisualization<VisualizationType::ElectricalLayer>(GetWholeShipRect());
    }
    else
    {
        // Shutdown electrical visualization
        mElectricalLayerVisualizationMode = ElectricalLayerVisualizationModeType::None;
        mElectricalLayerVisualizationTexture.reset();
    }
}

void ModelController::SetRopesLayerVisualizationMode(RopesLayerVisualizationModeType mode)
{
    if (mode == mRopesLayerVisualizationMode)
    {
        // Nop
        return;
    }

    if (mode != RopesLayerVisualizationModeType::None)
    {
        mRopesLayerVisualizationMode = mode;

        RegisterDirtyVisualization<VisualizationType::RopesLayer>(GetWholeShipRect());
    }
    else
    {
        // Shutdown ropes visualization
        mRopesLayerVisualizationMode = RopesLayerVisualizationModeType::None;
    }
}

void ModelController::SetTextureLayerVisualizationMode(TextureLayerVisualizationModeType mode)
{
    if (mode == mTextureLayerVisualizationMode)
    {
        // Nop
        return;
    }

    if (mode != TextureLayerVisualizationModeType::None)
    {
        mTextureLayerVisualizationMode = mode;

        assert(mModel.HasLayer(LayerType::Texture));

        RegisterDirtyVisualization<VisualizationType::TextureLayer>(GetWholeTextureRect());
    }
    else
    {
        // Shutdown texture visualization
        mTextureLayerVisualizationMode = TextureLayerVisualizationModeType::None;
    }
}

void ModelController::UpdateVisualizations(View & view)
{
    //
    // Update and upload visualizations that are dirty, and
    // remove visualizations that are not needed
    //

    // Game

    if (mGameVisualizationMode != GameVisualizationModeType::None)
    {
        if (!mGameVisualizationTexture)
        {
            // Initialize game visualization texture
            mGameVisualizationTextureMagnificationFactor = ShipTexturizer::CalculateHighDefinitionTextureMagnificationFactor(mModel.GetShipSize());
            ImageSize const textureSize = ImageSize(
                mModel.GetShipSize().width * mGameVisualizationTextureMagnificationFactor,
                mModel.GetShipSize().height * mGameVisualizationTextureMagnificationFactor);

            mGameVisualizationTexture = std::make_unique<RgbaImageData>(textureSize);
        }

        if (mGameVisualizationMode == GameVisualizationModeType::AutoTexturizationMode && !mGameVisualizationAutoTexturizationTexture)
        {
            // Initialize auto-texturization texture
            mGameVisualizationAutoTexturizationTexture = std::make_unique<RgbaImageData>(mGameVisualizationTexture->Size);
        }

        if (mDirtyGameVisualizationRegion.has_value())
        {
            // Update visualization
            ImageRect const dirtyTextureRegion = UpdateGameVisualization(*mDirtyGameVisualizationRegion);

            // Upload visualization
            if (dirtyTextureRegion != mGameVisualizationTexture->Size)
            {
                //
                // For better performance, we only upload the dirty sub-texture
                //

                auto subTexture = RgbaImageData(dirtyTextureRegion.size);
                subTexture.BlitFromRegion(
                    *mGameVisualizationTexture,
                    dirtyTextureRegion,
                    { 0, 0 });

                view.UpdateGameVisualization(
                    subTexture,
                    dirtyTextureRegion.origin);
            }
            else
            {
                // Upload whole texture
                view.UploadGameVisualization(*mGameVisualizationTexture);
            }
        }
    }
    else
    {
        assert(!mGameVisualizationTexture);

        if (view.HasGameVisualization())
        {
            view.RemoveGameVisualization();
        }
    }

    mDirtyGameVisualizationRegion.reset();

    // Structural

    if (mStructuralLayerVisualizationMode != StructuralLayerVisualizationModeType::None)
    {
        if (!mStructuralLayerVisualizationTexture)
        {
            // Initialize structural visualization
            mStructuralLayerVisualizationTexture = std::make_unique<RgbaImageData>(ImageSize(mModel.GetShipSize().width, mModel.GetShipSize().height));
        }

        if (mDirtyStructuralLayerVisualizationRegion.has_value())
        {
            // Refresh viz mode
            if (mStructuralLayerVisualizationMode == StructuralLayerVisualizationModeType::MeshMode)
            {
                view.SetStructuralLayerVisualizationDrawMode(View::StructuralLayerVisualizationDrawMode::MeshMode);
            }
            else
            {
                assert(mStructuralLayerVisualizationMode == StructuralLayerVisualizationModeType::PixelMode);
                view.SetStructuralLayerVisualizationDrawMode(View::StructuralLayerVisualizationDrawMode::PixelMode);
            }

            // Update visualization
            ImageRect const dirtyTextureRegion = UpdateStructuralLayerVisualization(*mDirtyStructuralLayerVisualizationRegion);

            // Upload visualization
            if (dirtyTextureRegion != mStructuralLayerVisualizationTexture->Size)
            {
                //
                // For better performance, we only upload the dirty sub-texture
                //

                auto subTexture = RgbaImageData(dirtyTextureRegion.size);
                subTexture.BlitFromRegion(
                    *mStructuralLayerVisualizationTexture,
                    dirtyTextureRegion,
                    { 0, 0 });

                view.UpdateStructuralLayerVisualization(
                    subTexture,
                    dirtyTextureRegion.origin);
            }
            else
            {
                // Upload whole texture
                view.UploadStructuralLayerVisualization(*mStructuralLayerVisualizationTexture);
            }
        }
    }
    else
    {
        assert(!mStructuralLayerVisualizationTexture);

        if (view.HasStructuralLayerVisualization())
        {
            view.RemoveStructuralLayerVisualization();
        }
    }

    mDirtyStructuralLayerVisualizationRegion.reset();

    // Electrical

    if (mElectricalLayerVisualizationMode != ElectricalLayerVisualizationModeType::None)
    {
        if (!mElectricalLayerVisualizationTexture)
        {
            // Initialize electrical visualization
            mElectricalLayerVisualizationTexture = std::make_unique<RgbaImageData>(mModel.GetShipSize().width, mModel.GetShipSize().height);
        }

        if (mDirtyElectricalLayerVisualizationRegion.has_value())
        {
            // Update visualization
            UpdateElectricalLayerVisualization(*mDirtyElectricalLayerVisualizationRegion);

            // Upload visualization
            view.UploadElectricalLayerVisualization(*mElectricalLayerVisualizationTexture);
        }
    }
    else
    {
        assert(!mElectricalLayerVisualizationTexture);

        if (view.HasElectricalLayerVisualization())
        {
            view.RemoveElectricalLayerVisualization();
        }
    }

    mDirtyElectricalLayerVisualizationRegion.reset();

    // Ropes

    if (mRopesLayerVisualizationMode != RopesLayerVisualizationModeType::None)
    {
        assert(mModel.HasLayer(LayerType::Ropes));

        if (mDirtyRopesLayerVisualizationRegion.has_value())
        {
            // Update visualization
            UpdateRopesLayerVisualization(); // Dirty region not needed in this implementation

            // Upload visualization
            view.UploadRopesLayerVisualization(mModel.GetRopesLayer().Buffer);
        }
    }
    else
    {
        if (view.HasRopesLayerVisualization())
        {
            view.RemoveRopesLayerVisualization();
        }
    }

    mDirtyRopesLayerVisualizationRegion.reset();

    // Texture

    if (mTextureLayerVisualizationMode != TextureLayerVisualizationModeType::None)
    {
        assert(mModel.HasLayer(LayerType::Texture));

        if (mDirtyTextureLayerVisualizationRegion.has_value())
        {
            // Update visualization
            UpdateTextureLayerVisualization(); // Dirty region not needed for updating viz in this implementation

            // Upload visualization
            if (*mDirtyTextureLayerVisualizationRegion != mModel.GetTextureLayer().Buffer.Size)
            {
                //
                // For better performance, we only upload the dirty sub-texture
                //

                auto subTexture = RgbaImageData(mDirtyTextureLayerVisualizationRegion->size);
                subTexture.BlitFromRegion(
                    mModel.GetTextureLayer().Buffer,
                    *mDirtyTextureLayerVisualizationRegion,
                    { 0, 0 });

                view.UpdateTextureLayerVisualization(
                    subTexture,
                    mDirtyTextureLayerVisualizationRegion->origin);
            }
            else
            {
                // Upload whole texture
                view.UploadTextureLayerVisualization(mModel.GetTextureLayer().Buffer);
            }
        }
    }
    else
    {
        if (view.HasTextureLayerVisualization())
        {
            view.RemoveTextureLayerVisualization();
        }
    }

    mDirtyTextureLayerVisualizationRegion.reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void ModelController::InitializeStructuralLayerAnalysis()
{
    mMassParticleCount = 0;
    mTotalMass = 0.0f;
    mCenterOfMassSum = vec2f::zero();

    auto const & structuralLayerBuffer = mModel.GetStructuralLayer().Buffer;

    for (int y = 0; y < structuralLayerBuffer.Size.height; ++y)
    {
        for (int x = 0; x < structuralLayerBuffer.Size.width; ++x)
        {
            ShipSpaceCoordinates const coords{ x, y };
            if (structuralLayerBuffer[coords].Material != nullptr)
            {
                auto const mass = structuralLayerBuffer[coords].Material->GetMass();

                ++mMassParticleCount;
                mTotalMass += mass;
                mCenterOfMassSum += coords.ToFloat() * mass;
            }
        }
    }
}

void ModelController::InitializeElectricalLayerAnalysis()
{
    // Reset factory
    mInstancedElectricalElementSet.Reset();

    // Reset particle count
    mElectricalParticleCount = 0;

    if (mModel.HasLayer(LayerType::Electrical))
    {
        // Register existing instance indices with factory, and initialize running analysis
        auto const & electricalLayerBuffer = mModel.GetElectricalLayer().Buffer;
        for (size_t i = 0; i < electricalLayerBuffer.Size.GetLinearSize(); ++i)
        {
            if (electricalLayerBuffer.Data[i].Material != nullptr)
            {
                ++mElectricalParticleCount;

                if (electricalLayerBuffer.Data[i].Material->IsInstanced)
                {
                    assert(electricalLayerBuffer.Data[i].InstanceIndex != NoneElectricalElementInstanceIndex);
                    mInstancedElectricalElementSet.Register(electricalLayerBuffer.Data[i].InstanceIndex, electricalLayerBuffer.Data[i].Material);
                }
            }
        }
    }
}

void ModelController::InitializeRopesLayerAnalysis()
{
    // Nop
}

void ModelController::WriteParticle(
    ShipSpaceCoordinates const & coords,
    StructuralMaterial const * material)
{
    // Write particle and maintain analysis

    auto & structuralLayerBuffer = mModel.GetStructuralLayer().Buffer;

    if (structuralLayerBuffer[coords].Material != nullptr)
    {
        auto const mass = structuralLayerBuffer[coords].Material->GetMass();

        assert(mMassParticleCount > 0);
        --mMassParticleCount;
        if (mMassParticleCount == 0)
        {
            mTotalMass = 0.0f;
        }
        else
        {
            mTotalMass -= mass;
        }
        mCenterOfMassSum -= coords.ToFloat() * mass;
    }

    structuralLayerBuffer[coords] = StructuralElement(material);

    if (material != nullptr)
    {
        auto const mass = material->GetMass();

        ++mMassParticleCount;
        mTotalMass += mass;
        mCenterOfMassSum += coords.ToFloat() * mass;
    }
}

void ModelController::WriteParticle(
    ShipSpaceCoordinates const & coords,
    ElectricalMaterial const * material)
{
    auto & electricalLayerBuffer = mModel.GetElectricalLayer().Buffer;

    auto const & oldElement = electricalLayerBuffer[coords];

    // Decide instance index and maintain panel
    ElectricalElementInstanceIndex instanceIndex;
    if (oldElement.Material == nullptr
        || !oldElement.Material->IsInstanced)
    {
        if (material != nullptr
            && material->IsInstanced)
        {
            // New instanced element...

            // ...new instance index
            instanceIndex = mInstancedElectricalElementSet.Add(material);
        }
        else
        {
            // None instanced...

            // ...keep it none
            instanceIndex = NoneElectricalElementInstanceIndex;
        }
    }
    else
    {
        assert(oldElement.Material->IsInstanced);
        assert(oldElement.InstanceIndex != NoneElectricalElementInstanceIndex);

        if (material == nullptr
            || !material->IsInstanced)
        {
            // Old instanced, new one not...

            // ...disappeared instance index
            mInstancedElectricalElementSet.Remove(oldElement.InstanceIndex);
            instanceIndex = NoneElectricalElementInstanceIndex;

            // Remove from panel
            mModel.GetElectricalLayer().Panel.erase(oldElement.InstanceIndex);
        }
        else
        {
            // Both instanced...

            // ...keep old instanceIndex
            instanceIndex = oldElement.InstanceIndex;
        }
    }

    // Update electrical element count
    if (material != nullptr)
    {
        if (oldElement.Material == nullptr)
        {
            ++mElectricalParticleCount;
        }
    }
    else
    {
        if (oldElement.Material != nullptr)
        {
            assert(mElectricalParticleCount > 0);
            --mElectricalParticleCount;
        }
    }

    // Store
    electricalLayerBuffer[coords] = ElectricalElement(material, instanceIndex);
}

void ModelController::AppendRope(
    ShipSpaceCoordinates const & startCoords,
    ShipSpaceCoordinates const & endCoords,
    StructuralMaterial const * material)
{
    assert(material != nullptr);

    mModel.GetRopesLayer().Buffer.EmplaceBack(
        startCoords,
        endCoords,
        material,
        material->RenderColor);
}

void ModelController::MoveRopeEndpoint(
    RopeElement & ropeElement,
    ShipSpaceCoordinates const & oldCoords,
    ShipSpaceCoordinates const & newCoords)
{
    if (ropeElement.StartCoords == oldCoords)
    {
        ropeElement.StartCoords = newCoords;
    }
    else
    {
        assert(ropeElement.EndCoords == oldCoords);
        ropeElement.EndCoords = newCoords;
    }
}

bool ModelController::InternalEraseRopeAt(ShipSpaceCoordinates const & coords)
{
    auto const srchIt = std::find_if(
        mModel.GetRopesLayer().Buffer.cbegin(),
        mModel.GetRopesLayer().Buffer.cend(),
        [&coords](auto const & e)
        {
            return e.StartCoords == coords
                || e.EndCoords == coords;
        });

    if (srchIt != mModel.GetRopesLayer().Buffer.cend())
    {
        // Remove
        mModel.GetRopesLayer().Buffer.Erase(srchIt);

        return true;
    }
    else
    {
        return false;
    }
}

template<LayerType TLayer>
std::optional<ShipSpaceRect> ModelController::DoFlood(
    ShipSpaceCoordinates const & start,
    typename LayerTypeTraits<TLayer>::material_type const * material,
    bool doContiguousOnly,
    typename LayerTypeTraits<TLayer>::layer_data_type const & layer)
{
    // Pick material to flood
    auto const * const startMaterial = layer.Buffer[start].Material;
    if (material == startMaterial)
    {
        // Nop
        return std::nullopt;
    }

    ShipSpaceSize const shipSize = mModel.GetShipSize();

    if (doContiguousOnly)
    {
        //
        // Flood from point
        //

        //
        // Init visit from this point
        //

        WriteParticle(start, material);
        ShipSpaceRect affectedRect(start);

        std::queue<ShipSpaceCoordinates> pointsToPropagateFrom;
        pointsToPropagateFrom.push(start);

        //
        // Propagate
        //

        auto const checkPropagateToNeighbor = [&](ShipSpaceCoordinates neighborCoords)
        {
            if (neighborCoords.IsInSize(shipSize) && layer.Buffer[neighborCoords].Material == startMaterial)
            {
                // Visit point
                WriteParticle(neighborCoords, material);
                affectedRect.UnionWith(neighborCoords);

                // Propagate from point
                pointsToPropagateFrom.push(neighborCoords);
            }
        };

        while (!pointsToPropagateFrom.empty())
        {
            // Pop point that we have to propagate from
            auto const currentPoint = pointsToPropagateFrom.front();
            pointsToPropagateFrom.pop();

            // Push neighbors
            checkPropagateToNeighbor({ currentPoint.x - 1, currentPoint.y });
            checkPropagateToNeighbor({ currentPoint.x + 1, currentPoint.y });
            checkPropagateToNeighbor({ currentPoint.x, currentPoint.y - 1 });
            checkPropagateToNeighbor({ currentPoint.x, currentPoint.y + 1 });
        }

        return affectedRect;
    }
    else
    {
        //
        // Replace material
        //

        std::optional<ShipSpaceRect> affectedRect;

        for (int y = 0; y < shipSize.height; ++y)
        {
            for (int x = 0; x < shipSize.width; ++x)
            {
                ShipSpaceCoordinates const coords(x, y);

                if (layer.Buffer[coords].Material == startMaterial)
                {
                    WriteParticle(coords, material);

                    if (!affectedRect.has_value())
                    {
                        affectedRect = ShipSpaceRect(coords);
                    }
                    else
                    {
                        affectedRect->UnionWith(coords);
                    }
                }
            }
        }

        return affectedRect;
    }
}

std::optional<ImageRect> ModelController::DoTextureMagicWandEraseBackground(
    ImageCoordinates const & start,
    unsigned int tolerance,
    bool isAntiAlias,
    bool doContiguousOnly,
    TextureLayerData & layer)
{
    //
    // For the purposes of this operation, a pixel might exist or not;
    // it exists if and only if its alpha is not zero.
    //

    auto const textureSize = layer.Buffer.Size;

    assert(start.IsInSize(textureSize));

    // Get starting color
    rgbaColor const seedColorRgb = layer.Buffer[start];
    if (seedColorRgb.a == 0)
    {
        // Starting pixel does not exist
        return std::nullopt;
    }

    vec3f const seedColor = seedColorRgb.toVec3f();

    // Our distance function - from https://www.photoshopgurus.com/forum/threads/tolerance.52555/page-2
    auto const distanceFromSeed = [seedColor](vec3f const & sampleColor) -> float
    {
        vec3f const deltaColor = (sampleColor - seedColor).abs();

        return std::max({
            deltaColor.x,
            deltaColor.y,
            deltaColor.z,
            std::abs(deltaColor.x - deltaColor.y),
            std::abs(deltaColor.y - deltaColor.z),
            std::abs(deltaColor.z - deltaColor.x) });
    };

    // Transform tolerance into max distance (included)
    float const maxColorDistance = static_cast<float>(tolerance) / 100.0f;

    // Save original alpha mask for neighbors
    Buffer2D<typename rgbaColor::data_type, ImageTag> originalAlphaMask = layer.Buffer.Transform<typename rgbaColor::data_type>(
        [](rgbaColor const & color) -> typename rgbaColor::data_type
        {
            return color.a;
        });

    // Initialize affected region
    ImageRect affectedRegion(start); // We're sure we'll erase the start pixel

    // Anti-alias functor
    auto const doAntiAliasNeighbor = [&](ImageCoordinates const & neighborCoordinates) -> void
    {
        layer.Buffer[neighborCoordinates].a = originalAlphaMask[neighborCoordinates] / 3;
        affectedRegion.UnionWith(neighborCoordinates);
    };

    if (doContiguousOnly)
    {
        //
        // Flood from starting point
        //

        std::queue<ImageCoordinates> pixelsToPropagateFrom;

        // Erase this pixel
        layer.Buffer[start].a = 0;
        affectedRegion.UnionWith(start);

        // Visit from here
        pixelsToPropagateFrom.push(start);

        while (!pixelsToPropagateFrom.empty())
        {
            auto const sourceCoords = pixelsToPropagateFrom.front();
            pixelsToPropagateFrom.pop();

            // Check neighbors
            for (int yn = sourceCoords.y - 1; yn <= sourceCoords.y + 1; ++yn)
            {
                for (int xn = sourceCoords.x - 1; xn <= sourceCoords.x + 1; ++xn)
                {
                    ImageCoordinates const neighborCoordinates{ xn, yn };
                    if (neighborCoordinates.IsInSize(textureSize)
                        && layer.Buffer[neighborCoordinates].a != 0)
                    {
                        // Check distance
                        if (distanceFromSeed(layer.Buffer[neighborCoordinates].toVec3f()) <= maxColorDistance)
                        {
                            // Erase this pixel
                            layer.Buffer[neighborCoordinates].a = 0;
                            affectedRegion.UnionWith(neighborCoordinates);

                            // Continue visit from here
                            pixelsToPropagateFrom.push(neighborCoordinates);
                        }
                        else if (isAntiAlias)
                        {
                            // Anti-alias this neighbor
                            doAntiAliasNeighbor(neighborCoordinates);
                        }
                    }
                }
            }
        }
    }
    else
    {
        //
        // Color substitution
        //

        for (int y = 0; y < textureSize.height; ++y)
        {
            for (int x = 0; x < textureSize.width; ++x)
            {
                ImageCoordinates const sampleCoordinates{ x, y };

                if (layer.Buffer[sampleCoordinates].a != 0
                    && distanceFromSeed(layer.Buffer[sampleCoordinates].toVec3f()) <= maxColorDistance)
                {
                    // Erase
                    layer.Buffer[sampleCoordinates].a = 0;
                    affectedRegion.UnionWith(sampleCoordinates);

                    if (isAntiAlias)
                    {
                        //
                        // Do anti-aliasing on neighboring, too-distant pixels
                        //

                        for (int yn = y - 1; yn <= y + 1; ++yn)
                        {
                            for (int xn = x - 1; xn <= x + 1; ++xn)
                            {
                                ImageCoordinates const neighborCoordinates{ xn, yn };
                                if (neighborCoordinates.IsInSize(textureSize)
                                    && layer.Buffer[neighborCoordinates].a != 0
                                    && distanceFromSeed(layer.Buffer[neighborCoordinates].toVec3f()) > maxColorDistance)
                                {
                                    doAntiAliasNeighbor(neighborCoordinates);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return affectedRegion;
}

//////////////////////////////////////////////////////////////////////////////////////////////

template<VisualizationType TVisualization, typename TRect>
void ModelController::RegisterDirtyVisualization(TRect const & region)
{
    if constexpr (TVisualization == VisualizationType::Game)
    {
        if (!mDirtyGameVisualizationRegion.has_value())
        {
            mDirtyGameVisualizationRegion = region;
        }
        else
        {
            mDirtyGameVisualizationRegion->UnionWith(region);
        }
    }
    else if constexpr (TVisualization == VisualizationType::StructuralLayer)
    {
        if (!mDirtyStructuralLayerVisualizationRegion.has_value())
        {
            mDirtyStructuralLayerVisualizationRegion = region;
        }
        else
        {
            mDirtyStructuralLayerVisualizationRegion->UnionWith(region);
        }
    }
    else if constexpr (TVisualization == VisualizationType::ElectricalLayer)
    {
        if (!mDirtyElectricalLayerVisualizationRegion.has_value())
        {
            mDirtyElectricalLayerVisualizationRegion = region;
        }
        else
        {
            mDirtyElectricalLayerVisualizationRegion->UnionWith(region);
        }
    }
    else if constexpr (TVisualization == VisualizationType::RopesLayer)
    {
        if (!mDirtyRopesLayerVisualizationRegion.has_value())
        {
            mDirtyRopesLayerVisualizationRegion = region;
        }
        else
        {
            mDirtyRopesLayerVisualizationRegion->UnionWith(region);
        }
    }
    else
    {
        static_assert(TVisualization == VisualizationType::TextureLayer);

        if (!mDirtyTextureLayerVisualizationRegion.has_value())
        {
            mDirtyTextureLayerVisualizationRegion = region;
        }
        else
        {
            mDirtyTextureLayerVisualizationRegion->UnionWith(region);
        }
    }
}

ImageRect ModelController::UpdateGameVisualization(ShipSpaceRect const & region)
{
    //
    // 1. Prepare source of triangularized rendering
    //

    RgbaImageData const * sourceTexture = nullptr;

    if (mGameVisualizationMode == GameVisualizationModeType::AutoTexturizationMode)
    {
        assert(mModel.HasLayer(LayerType::Structural));
        assert(mGameVisualizationAutoTexturizationTexture);

        ShipAutoTexturizationSettings settings = mModel.GetShipAutoTexturizationSettings().value_or(ShipAutoTexturizationSettings());

        mShipTexturizer.AutoTexturizeInto(
            mModel.GetStructuralLayer(),
            region,
            *mGameVisualizationAutoTexturizationTexture,
            mGameVisualizationTextureMagnificationFactor,
            settings);

        sourceTexture = mGameVisualizationAutoTexturizationTexture.get();
    }
    else
    {
        assert(mGameVisualizationMode == GameVisualizationModeType::TextureMode);

        assert(mModel.HasLayer(LayerType::Structural));
        assert(mModel.HasLayer(LayerType::Texture));

        sourceTexture = &mModel.GetTextureLayer().Buffer;
    }

    assert(sourceTexture != nullptr);

    //
    // 2. Do triangularized rendering
    //

    // Given that texturization looks at x+1 and y+1, we enlarge the region down and to the left
    ShipSpaceRect effectiveRegion = region;
    if (effectiveRegion.origin.x > 0)
    {
        effectiveRegion.origin.x -= 1;
        effectiveRegion.size.width += 1;
    }

    if (effectiveRegion.origin.y > 0)
    {
        effectiveRegion.origin.y -= 1;
        effectiveRegion.size.height += 1;
    }

    assert(mGameVisualizationTexture);

    mShipTexturizer.RenderShipInto(
        mModel.GetStructuralLayer(),
        effectiveRegion,
        *sourceTexture,
        *mGameVisualizationTexture,
        mGameVisualizationTextureMagnificationFactor);

    //
    // 3. Return dirty image region
    //

    return ImageRect(
        { effectiveRegion.origin.x * mGameVisualizationTextureMagnificationFactor, effectiveRegion.origin.y * mGameVisualizationTextureMagnificationFactor },
        { effectiveRegion.size.width * mGameVisualizationTextureMagnificationFactor, effectiveRegion.size.height * mGameVisualizationTextureMagnificationFactor });
}

ImageRect ModelController::UpdateStructuralLayerVisualization(ShipSpaceRect const & region)
{
    switch (mStructuralLayerVisualizationMode)
    {
        case StructuralLayerVisualizationModeType::MeshMode:
        case StructuralLayerVisualizationModeType::PixelMode:
        {
            assert(mStructuralLayerVisualizationTexture);

            RenderStructureInto(
                region,
                *mStructuralLayerVisualizationTexture);

            break;
        }

        case StructuralLayerVisualizationModeType::None:
        {
            // Nop
            break;
        }
    }

    return ImageRect(
        { region.origin.x, region.origin.y },
        { region.size.width, region.size.height });
}

void ModelController::RenderStructureInto(
    ShipSpaceRect const & structureRegion,
    RgbaImageData & texture) const
{
    assert(mModel.HasLayer(LayerType::Structural));

    assert(texture.Size.width == mModel.GetStructuralLayer().Buffer.Size.width
        && texture.Size.height == mModel.GetStructuralLayer().Buffer.Size.height);

    rgbaColor const emptyColor = rgbaColor(EmptyMaterialColorKey, 0); // Fully transparent

    auto const & structuralLayerBuffer = mModel.GetStructuralLayer().Buffer;

    for (int y = structureRegion.origin.y; y < structureRegion.origin.y + structureRegion.size.height; ++y)
    {
        for (int x = structureRegion.origin.x; x < structureRegion.origin.x + structureRegion.size.width; ++x)
        {
            auto const structuralMaterial = structuralLayerBuffer[{x, y}].Material;

            texture[{x, y}] = (structuralMaterial != nullptr)
                ? structuralMaterial->RenderColor
                : emptyColor;
        }
    }
}

void ModelController::UpdateElectricalLayerVisualization(ShipSpaceRect const & region)
{
    switch (mElectricalLayerVisualizationMode)
    {
        case ElectricalLayerVisualizationModeType::PixelMode:
        {
            assert(mModel.HasLayer(LayerType::Electrical));

            assert(mElectricalLayerVisualizationTexture);
            assert(mElectricalLayerVisualizationTexture->Size.width == mModel.GetShipSize().width
                && mElectricalLayerVisualizationTexture->Size.height == mModel.GetShipSize().height);

            rgbaColor const emptyColor = rgbaColor(EmptyMaterialColorKey, 0); // Fully transparent

            auto const & electricalLayerBuffer = mModel.GetElectricalLayer().Buffer;
            RgbaImageData & electricalRenderColorTexture = *mElectricalLayerVisualizationTexture;

            for (int y = region.origin.y; y < region.origin.y + region.size.height; ++y)
            {
                for (int x = region.origin.x; x < region.origin.x + region.size.width; ++x)
                {
                    auto const electricalMaterial = electricalLayerBuffer[{x, y}].Material;

                    electricalRenderColorTexture[{x, y}] = electricalMaterial != nullptr
                        ? rgbaColor(electricalMaterial->RenderColor, 255)
                        : emptyColor;
                }
            }

            break;
        }

        case ElectricalLayerVisualizationModeType::None:
        {
            return;
        }
    }
}

void ModelController::UpdateRopesLayerVisualization()
{
    switch (mRopesLayerVisualizationMode)
    {
        case RopesLayerVisualizationModeType::LinesMode:
        {
            assert(mModel.HasLayer(LayerType::Ropes));

            // Nop
            break;
        }

        case RopesLayerVisualizationModeType::None:
        {
            return;
        }
    }
}

void ModelController::UpdateTextureLayerVisualization()
{
    switch(mTextureLayerVisualizationMode)
    {
        case TextureLayerVisualizationModeType::MatteMode:
        {
            assert(mModel.HasLayer(LayerType::Texture));

            // Nop
            break;
        }

        case TextureLayerVisualizationModeType::None:
        {
            return;
        }
    }
}

}