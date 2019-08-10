/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2018-10-13
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#pragma once

#include "RenderCore.h"
#include "ResourceLoader.h"

#include <GameCore/ImageData.h>
#include <GameCore/ProgressCallback.h>
#include <GameCore/Vectors.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace Render
{

static constexpr char FontBaseCharacter = ' ';

class FontMetadata
{
public:

    inline ImageSize CalculateTextExtent(
        char const * text,
        size_t length) const
    {
        uint32_t width = 0;
        for (size_t c = 0; c < length; ++c)
            width += mGlyphWidths[static_cast<unsigned char>(text[c])];

        return ImageSize(width, mCellSize.Height);
    }

    inline size_t EmitQuadVertices(
        char const * text,
        size_t length,
        vec2f cursorPositionNdc,
        float alpha,
        float screenToNdcX,
        float screenToNdcY,
        std::vector<TextQuadVertex> & vertices) const
    {
        size_t totalVertices = 0;

        float const cellWidthNdc = screenToNdcX * mCellSize.Width;
        float const cellHeightNdc = screenToNdcY * mCellSize.Height;

        for (size_t c = 0; c < length; ++c)
        {
            unsigned char ch = static_cast<unsigned char>(text[c]);

            float const textureULeft = mGlyphTextureOrigins[ch].x;
            float const textureURight = textureULeft + mGlyphTextureWidth;
            float const textureVBottom = mGlyphTextureOrigins[ch].y;
            float const textureVTop = textureVBottom - mGlyphTextureHeight;

            // Top-left
            vertices.emplace_back(
                cursorPositionNdc.x,
                cursorPositionNdc.y + cellHeightNdc,
                textureULeft,
                textureVTop,
                alpha);

            // Bottom-left
            vertices.emplace_back(
                cursorPositionNdc.x,
                cursorPositionNdc.y,
                textureULeft,
                textureVBottom,
                alpha);

            // Top-right
            vertices.emplace_back(
                cursorPositionNdc.x + cellWidthNdc,
                cursorPositionNdc.y + cellHeightNdc,
                textureURight,
                textureVTop,
                alpha);

            // Bottom-left
            vertices.emplace_back(
                cursorPositionNdc.x,
                cursorPositionNdc.y,
                textureULeft,
                textureVBottom,
                alpha);

            // Top-right
            vertices.emplace_back(
                cursorPositionNdc.x + cellWidthNdc,
                cursorPositionNdc.y + cellHeightNdc,
                textureURight,
                textureVTop,
                alpha);

            // Bottom-right
            vertices.emplace_back(
                cursorPositionNdc.x + cellWidthNdc,
                cursorPositionNdc.y,
                textureURight,
                textureVBottom,
                alpha);

            cursorPositionNdc.x += screenToNdcX * mGlyphWidths[ch];

            totalVertices += 6;
        }

        return totalVertices;
    }

private:

    friend struct Font;

    FontMetadata(
        ImageSize cellSize,
        std::array<uint8_t, 256> glyphWidths,
        int glyphsPerTextureRow,
        float glyphTextureWidth,
        float glyphTextureHeight);

    ImageSize mCellSize;
    std::array<uint8_t, 256> mGlyphWidths;
    std::array<vec2f, 256> mGlyphTextureOrigins; // Bottom-left
    int mGlyphsPerTextureRow;
    float mGlyphTextureWidth;
    float mGlyphTextureHeight;
};

struct Font
{
    FontMetadata Metadata;

    RgbaImageData Texture;

    static std::vector<Render::Font> LoadAll(
        ResourceLoader const & resourceLoader,
        ProgressCallback const & progressCallback);

private:

    static Font Load(std::filesystem::path const & filepath);

    Font(
        FontMetadata metadata,
        RgbaImageData texture)
        : Metadata(std::move(metadata))
        , Texture(std::move(texture))
    {}
};

}
