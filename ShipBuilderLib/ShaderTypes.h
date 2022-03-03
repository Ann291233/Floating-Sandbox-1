/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2021-08-28
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#pragma once

#include <GameOpenGL/GameOpenGL.h>

#include <cstdint>
#include <string>

namespace ShipBuilder {

//
// Shaders
//

enum class ProgramType
{
    Canvas = 0,
    CircleOverlay,
    DashedLineOverlay,
    Grid,
    RectOverlay,
    Rope,
    StructureMesh,
    Texture,
    TextureNdc,

    _Last = TextureNdc
};

ProgramType ShaderFilenameToProgramType(std::string const & str);

std::string ProgramTypeToStr(ProgramType program);

enum class ProgramParameterType : uint8_t
{
    Opacity = 0,
    OrthoMatrix,
    PixelsPerShipParticle,
    PixelSize,
    PixelStep,
    ShipParticleTextureSize,

    // Texture units
    BackgroundTextureUnit,
    LinearTexturesAtlasTexture,
    TextureUnit1,

    _FirstTexture = BackgroundTextureUnit,
    _LastTexture = TextureUnit1
};

ProgramParameterType StrToProgramParameterType(std::string const & str);

std::string ProgramParameterTypeToStr(ProgramParameterType programParameter);

/*
 * This enum serves merely to associate a vertex attribute index to each vertex attribute name.
 */
enum class VertexAttributeType : GLuint
{
    Canvas = 0,

    CircleOverlay1 = 0,
    CircleOverlay2 = 1,

    DashedLineOverlay1 = 0,
    DashedLineOverlay2 = 1,

    Grid1 = 0,
    Grid2 = 1,

    RectOverlay1 = 0,
    RectOverlay2 = 1,

    Rope1 = 0,
    Rope2 = 1,

    Texture = 0,

    TextureNdc = 0
};

VertexAttributeType StrToVertexAttributeType(std::string const & str);

struct ShaderManagerTraits
{
    using ProgramType = ShipBuilder::ProgramType;
    using ProgramParameterType = ShipBuilder::ProgramParameterType;
    using VertexAttributeType = ShipBuilder::VertexAttributeType;

    static constexpr auto ShaderFilenameToProgramType = ShipBuilder::ShaderFilenameToProgramType;
    static constexpr auto ProgramTypeToStr = ShipBuilder::ProgramTypeToStr;
    static constexpr auto StrToProgramParameterType = ShipBuilder::StrToProgramParameterType;
    static constexpr auto ProgramParameterTypeToStr = ShipBuilder::ProgramParameterTypeToStr;
    static constexpr auto StrToVertexAttributeType = ShipBuilder::StrToVertexAttributeType;
};

}
