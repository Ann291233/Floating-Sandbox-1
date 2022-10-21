/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2021-08-28
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "Model.h"

#include <Game/MaterialDatabase.h>

namespace ShipBuilder {

Model::Model(
    ShipSpaceSize const & shipSize,
    std::string const & shipName)
    : mShipSize(shipSize)
    , mShipMetadata(shipName)
    , mShipPhysicsData()
    , mShipAutoTexturizationSettings()
    , mLayers(
        MakeNewEmptyStructuralLayer(mShipSize), 
        nullptr, 
        nullptr, 
        nullptr)
    , mDirtyState()
{
}

Model::Model(ShipDefinition && shipDefinition)
    : mShipSize(shipDefinition.Size)
    , mShipMetadata(shipDefinition.Metadata)
    , mShipPhysicsData(shipDefinition.PhysicsData)
    , mShipAutoTexturizationSettings(shipDefinition.AutoTexturizationSettings)
    , mLayers(
        std::move(shipDefinition.Layers.StructuralLayer), 
        std::move(shipDefinition.Layers.ElectricalLayer),
        std::move(shipDefinition.Layers.RopesLayer),
        std::move(shipDefinition.Layers.TextureLayer))
    , mDirtyState()
{
}

ShipDefinition Model::MakeShipDefinition() const
{
    return ShipDefinition(
        GetShipSize(),
        ShipLayers(
            CloneStructuralLayer(),
            CloneElectricalLayer(),
            CloneRopesLayer(),
            CloneTextureLayer()),
        GetShipMetadata(),
        GetShipPhysicsData(),
        GetShipAutoTexturizationSettings());
}

std::unique_ptr<ShipLayers> Model::CloneRegion(
    ShipSpaceRect const & region,
    std::optional<LayerType> layerSelection) const
{
    // TODOHERE
    (void)region;
    (void)layerSelection;
    return nullptr;
}

void Model::SetStructuralLayer(StructuralLayerData && structuralLayer)
{
    assert(structuralLayer.Buffer.Size == GetShipSize());

    // Update layer
    mLayers.StructuralLayer = std::move(structuralLayer);
}

StructuralLayerData Model::CloneStructuralLayer() const
{
    return mLayers.StructuralLayer.Clone();
}

void Model::RestoreStructuralLayer(StructuralLayerData && structuralLayer)
{
    // Replace layer
    mLayers.StructuralLayer = std::move(structuralLayer);
}

void Model::SetElectricalLayer(ElectricalLayerData && electricalLayer)
{
    assert(electricalLayer.Buffer.Size == GetShipSize());

    // Update layer
    mLayers.ElectricalLayer.reset(new ElectricalLayerData(std::move(electricalLayer)));
}

void Model::RemoveElectricalLayer()
{
    // Remove layer
    mLayers.ElectricalLayer.reset();
}

std::unique_ptr<ElectricalLayerData> Model::CloneElectricalLayer() const
{
    std::unique_ptr<ElectricalLayerData> clonedLayer;

    if (mLayers.ElectricalLayer)
    {
        clonedLayer.reset(new ElectricalLayerData(mLayers.ElectricalLayer->Clone()));
    }

    return clonedLayer;
}

void Model::RestoreElectricalLayer(std::unique_ptr<ElectricalLayerData> electricalLayer)
{
    // Replace layer
    mLayers.ElectricalLayer = std::move(electricalLayer);
}

void Model::SetRopesLayer(RopesLayerData && ropesLayer)
{
    // Update layer
    mLayers.RopesLayer.reset(new RopesLayerData(std::move(ropesLayer)));
}

void Model::RemoveRopesLayer()
{
    // Remove layer
    mLayers.RopesLayer.reset();
}

std::unique_ptr<RopesLayerData> Model::CloneRopesLayer() const
{
    std::unique_ptr<RopesLayerData> clonedLayer;

    if (mLayers.RopesLayer)
    {
        clonedLayer.reset(new RopesLayerData(mLayers.RopesLayer->Clone()));
    }

    return clonedLayer;
}

void Model::RestoreRopesLayer(std::unique_ptr<RopesLayerData> ropesLayer)
{
    // Replace layer
    mLayers.RopesLayer = std::move(ropesLayer);
}

void Model::SetTextureLayer(TextureLayerData && textureLayer)
{
    // Update layer
    mLayers.TextureLayer.reset(new TextureLayerData(std::move(textureLayer)));
}

void Model::RemoveTextureLayer()
{
    // Remove layer
    mLayers.TextureLayer.reset();
}

std::unique_ptr<TextureLayerData> Model::CloneTextureLayer() const
{
    std::unique_ptr<TextureLayerData> clonedLayer;

    if (mLayers.TextureLayer)
    {
        clonedLayer.reset(new TextureLayerData(mLayers.TextureLayer->Clone()));
    }

    return clonedLayer;
}

void Model::RestoreTextureLayer(std::unique_ptr<TextureLayerData> textureLayer)
{
    // Replace layer
    mLayers.TextureLayer = std::move(textureLayer);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

StructuralLayerData Model::MakeNewEmptyStructuralLayer(ShipSpaceSize const & shipSize)
{
    return StructuralLayerData(
        shipSize,
        StructuralElement(nullptr)); // No material
}

}