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
#include <GameCore/Vectors.h>

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
    auto image = ImageFileTools::LoadImageRgbUpperLeft(std::filesystem::path(inputFile));

    float const halfWidth = static_cast<float>(image.Size.Width) / 2.0f;

    // Load materials
    auto materials = MaterialDatabase::Load(materialsDir);

    // Visit all points
    ShipAnalyzer::AnalysisInfo analysisInfo;
    float totalMass = 0.0f;
    float airBuoyantMass = 0.0f;
    float waterBuoyantMass = 0.0f;
    float numPoints = 0.0f;
    for (int x = 0; x < image.Size.Width; ++x)
    {
        float worldX = static_cast<float>(x) - halfWidth;

        // From bottom to top
        for (int y = 0; y < image.Size.Height; ++y)
        {
            float worldY = static_cast<float>(y);

            auto pixelIndex = (x + (image.Size.Height - y - 1) * image.Size.Width);

            StructuralMaterial const * structuralMaterial = materials.FindStructuralMaterial(image.Data[pixelIndex]);
            if (nullptr != structuralMaterial)
            {
                numPoints += 1.0f;

                totalMass += structuralMaterial->GetMass();

                airBuoyantMass +=
                    structuralMaterial->GetMass()
                    - (structuralMaterial->BuoyancyVolumeFill * GameParameters::AirMass);

                waterBuoyantMass +=
                    structuralMaterial->GetMass()
                    - (structuralMaterial->BuoyancyVolumeFill * GameParameters::WaterMass);

                // Update center of mass
                analysisInfo.BaricentricX += worldX * structuralMaterial->GetMass();
                analysisInfo.BaricentricY += worldY * structuralMaterial->GetMass();
            }
        }
    }

    analysisInfo.TotalMass = totalMass;

    if (numPoints != 0.0f)
    {
        analysisInfo.MassPerPoint = totalMass / numPoints;
        analysisInfo.AirBuoyantMassPerPoint = airBuoyantMass / numPoints;
        analysisInfo.WaterBuoyantMassPerPoint = waterBuoyantMass / numPoints;
    }

    if (analysisInfo.TotalMass != 0.0f)
    {
        analysisInfo.BaricentricX /= analysisInfo.TotalMass;
        analysisInfo.BaricentricY /= analysisInfo.TotalMass;
    }

    return analysisInfo;
}