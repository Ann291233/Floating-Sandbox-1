/***************************************************************************************
* Original Author:		Gabriele Giuseppini
* Created:				2018-03-19
* Copyright:			Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#pragma once

#include "ShipMetadata.h"

#include <GameCore/ImageData.h>
#include <GameCore/Vectors.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

/*
* The complete definition of a ship.
*/
struct ShipDefinition
{
public:

    RgbImageData StructuralLayerImage;

    std::optional<RgbImageData> RopesLayerImage;

    std::optional<RgbImageData> ElectricalLayerImage;

    RgbaImageData TextureLayerImage;

    enum class TextureOriginType
    {
        // The texture comes from the proper texture layer
        Texture,

        // The texture is a fallback to the structural image
        StructuralImage
    };

    TextureOriginType TextureOrigin;

    ShipMetadata const Metadata;

    static ShipDefinition Load(std::filesystem::path const & filepath);

private:

    ShipDefinition(
        RgbImageData structuralLayerImage,
        std::optional<RgbImageData> ropesLayerImage,
        std::optional<RgbImageData> electricalLayerImage,
        RgbaImageData textureLayerImage,
        TextureOriginType textureOrigin,
        ShipMetadata const metadata)
        : StructuralLayerImage(std::move(structuralLayerImage))
        , RopesLayerImage(std::move(ropesLayerImage))
        , ElectricalLayerImage(std::move(electricalLayerImage))
        , TextureLayerImage(std::move(textureLayerImage))
        , TextureOrigin(textureOrigin)
        , Metadata(std::move(metadata))
    {
    }
};
