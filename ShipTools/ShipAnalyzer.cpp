/***************************************************************************************
 * Original Author:		Gabriele Giuseppini
 * Created:				2018-06-30
 * Copyright:			Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
 ***************************************************************************************/
#include "ShipAnalyzer.h"

#include <Game/GameParameters.h>
#include <Game/ImageFileTools.h>
#include <Game/MaterialDatabase.h>

#include <GameCore/Colors.h>

#include <IL/il.h>
#include <IL/ilu.h>

#include <limits>
#include <stdexcept>
#include <vector>

ShipAnalyzer::AnalysisInfo ShipAnalyzer::Analyze(
    std::string const & inputFile,
    std::string const & materialsDir)
{
    // Load image
    auto image = ImageFileTools::LoadImageRgbLowerLeft(std::filesystem::path(inputFile));

    float const halfWidth = static_cast<float>(image.Size.Width) / 2.0f;

    // Load materials
    auto materials = MaterialDatabase::Load(materialsDir);

    // Visit all points
    ShipAnalyzer::AnalysisInfo analysisInfo;
    float totalMass = 0.0f;
    float totalDisplacedDensity = 0.0f; // Assuming fully submersed
    float numPoints = 0.0f;
    for (int x = 0; x < image.Size.Width; ++x)
    {
        float const worldX = static_cast<float>(x) - halfWidth;

        // From bottom to top
        for (int y = 0; y < image.Size.Height; ++y)
        {
            vec2f const worldPosition(worldX, static_cast<float>(y));

            auto const pixelIndex = (x + y * image.Size.Width);
            StructuralMaterial const * const structuralMaterial = materials.FindStructuralMaterial(image.Data[pixelIndex]);
            if (nullptr != structuralMaterial)
            {
                // Update total masses
                totalMass += structuralMaterial->GetMass();
                totalDisplacedDensity += structuralMaterial->BuoyancyVolumeFill;

                // Update centers of mass
                analysisInfo.CenterOfMass += worldPosition * structuralMaterial->GetMass();
                analysisInfo.CenterOfDisplacedDensity += worldPosition * structuralMaterial->BuoyancyVolumeFill;

                numPoints += 1.0f;
            }
        }
    }

    if (totalMass != 0.0f)
    {
        analysisInfo.CenterOfMass /= totalMass;
        analysisInfo.CenterOfDisplacedDensity /= totalDisplacedDensity;

        // (Xg - Xb) x (M * G0)
        analysisInfo.EquilibriumMomentum =
            (analysisInfo.CenterOfMass - analysisInfo.CenterOfDisplacedDensity)
            .cross(GameParameters::GravityNormalized * totalMass);
    }

    analysisInfo.TotalMass = totalMass;

    if (numPoints != 0.0f)
    {
        analysisInfo.AverageMassPerPoint = totalMass / numPoints;
        analysisInfo.AverageAirBuoyantMassPerPoint = (totalDisplacedDensity * GameParameters::AirMass) / numPoints;
        analysisInfo.AverageWaterBuoyantMassPerPoint = (totalDisplacedDensity * GameParameters::WaterMass) / numPoints;
    }

    return analysisInfo;
}