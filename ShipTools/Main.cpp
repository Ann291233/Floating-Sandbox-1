/***************************************************************************************
 * Original Author:		Gabriele Giuseppini
 * Created:				2018-06-25
 * Copyright:			Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
 ***************************************************************************************/

#include "Baker.h"
#include "Quantizer.h"
#include "Resizer.h"
#include "ShipAnalyzer.h"

#include <GameCore/Utils.h>

#include <IL/il.h>
#include <IL/ilu.h>

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

#define SEPARATOR "------------------------------------------------------"

int DoAnalyzeShip(int argc, char ** argv);
int DoBakeRegularAtlas(int argc, char ** argv);
int DoQuantize(int argc, char ** argv);
int DoResize(int argc, char ** argv);

void PrintUsage();

int main(int argc, char ** argv)
{
    // Initialize DevIL
    ilInit();
    iluInit();

    if (argc == 1)
    {
        PrintUsage();
        return 0;
    }

    std::string verb(argv[1]);
    try
    {
        if (verb == "analyze")
        {
            return DoAnalyzeShip(argc, argv);
        }
        else if (verb == "bake_regular_atlas")
        {
            return DoBakeRegularAtlas(argc, argv);
        }
        else if (verb == "quantize")
        {
            return DoQuantize(argc, argv);
        }
        else if (verb == "resize")
        {
            return DoResize(argc, argv);
        }
        else
        {
            throw std::runtime_error("Unrecognized verb '" + verb + "'");
        }
    }
    catch (std::exception & ex)
    {
        std::cout << "ERROR: " << ex.what() << std::endl;
        return -1;
    }
}

int DoAnalyzeShip(int argc, char ** argv)
{
    if (argc < 4)
    {
        PrintUsage();
        return 0;
    }

    std::string materialsDirectory(argv[2]);
    std::string inputFile(argv[3]);

    auto analysisInfo = ShipAnalyzer::Analyze(inputFile, materialsDirectory);

    std::cout << std::fixed;

    std::cout << "  Total mass             : " << analysisInfo.TotalMass << std::endl;
    std::cout << "  Equivalent mass        : " << analysisInfo.MassPerPoint << std::endl;
    std::cout << "  Equivalent buoyant mass: " << analysisInfo.BuoyantMassPerPoint << std::endl;
    std::cout << "  Center of mass         : " << analysisInfo.BaricentricX << "," << analysisInfo.BaricentricY << std::endl;

    return 0;
}

int DoBakeRegularAtlas(int argc, char ** argv)
{
    if (argc < 5)
    {
        PrintUsage();
        return 0;
    }

    std::filesystem::path jsonSpecificationFilePath(argv[2]);
    std::filesystem::path outputDirectoryPath(argv[3]);
    std::string atlasFilenamesStem(argv[4]);

    std::cout << SEPARATOR << std::endl;
    std::cout << "Running bake_regular_atlas:" << std::endl;
    std::cout << "  JSON specification file : " << jsonSpecificationFilePath << std::endl;
    std::cout << "  output directory        : " << outputDirectoryPath << std::endl;
    std::cout << "  atlas filenames' stem   : " << atlasFilenamesStem << std::endl;

    Baker::BakeRegularAtlas(
        jsonSpecificationFilePath,
        outputDirectoryPath,
        atlasFilenamesStem);

    std::cout << "Baking completed." << std::endl;

    return 0;
}

int DoQuantize(int argc, char ** argv)
{
    if (argc < 5)
    {
        PrintUsage();
        return 0;
    }

    std::string materialsDirectory(argv[2]);
    std::string inputFile(argv[3]);
    std::string outputFile(argv[4]);

    bool doKeepRopes = false;
    bool doKeepGlass = false;
    std::optional<rgbColor> targetFixedColor;
    std::string targetFixedColorStr;
    for (int i = 5; i < argc; ++i)
    {
        std::string option(argv[i]);
        if (option == "-r" || option == "--keep_ropes")
        {
            doKeepRopes = true;
        }
        else if (option == "-g" || option == "--keep_glass")
        {
            doKeepGlass = true;
        }
        else if (option == "-c")
        {
            ++i;
            if (i == argc)
            {
                throw std::runtime_error("-c option specified without a color");
            }

            targetFixedColorStr = argv[i];
            targetFixedColor = Utils::Hex2RgbColor(targetFixedColorStr);
        }
        else
        {
            throw std::runtime_error("Unrecognized option '" + option + "'");
        }
    }

    std::cout << SEPARATOR << std::endl;
    std::cout << "Running quantize:" << std::endl;
    std::cout << "  input file    : " << inputFile << std::endl;
    std::cout << "  output file   : " << outputFile << std::endl;
    std::cout << "  materials dir : " << materialsDirectory << std::endl;
    std::cout << "  keep ropes    : " << doKeepRopes << std::endl;
    std::cout << "  keep glass    : " << doKeepGlass << std::endl;
    if (!!targetFixedColor)
        std::cout << "  target color  : " << targetFixedColorStr << std::endl;

    Quantizer::Quantize(
        inputFile,
        outputFile,
        materialsDirectory,
        doKeepRopes,
        doKeepGlass,
        targetFixedColor);

    std::cout << "Quantize completed." << std::endl;

    return 0;
}

int DoResize(int argc, char ** argv)
{
    if (argc < 5)
    {
        PrintUsage();
        return 0;
    }

    std::string inputFile(argv[2]);
    std::string outputFile(argv[3]);
    int width = std::stoi(argv[4]);

    std::cout << SEPARATOR << std::endl;
    std::cout << "Running resize:" << std::endl;
    std::cout << "  input file : " << inputFile << std::endl;
    std::cout << "  output file: " << outputFile << std::endl;
    std::cout << "  width      : " << width << std::endl;

    Resizer::Resize(inputFile, outputFile, width);

    std::cout << "Resize completed." << std::endl;

    return 0;
}

void PrintUsage()
{
    std::cout << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << " analyze <materials_dir> <in_file>" << std::endl;
    std::cout << " bake_regular_atlas <json_spec_file> <out_dir> <out_stem>" << std::endl;
    std::cout << " quantize <materials_dir> <in_file> <out_png> [-c <target_fixed_color>]" << std::endl;
    std::cout << "          -r, --keep_ropes] [-g, --keep_glass]" << std::endl;
    std::cout << " resize <in_file> <out_png> <width>" << std::endl;
}