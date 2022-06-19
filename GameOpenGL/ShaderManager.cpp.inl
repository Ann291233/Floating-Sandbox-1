/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2018-10-03
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "ShaderManager.h"

#include <GameCore/GameException.h>
#include <GameCore/Utils.h>

#include <regex>
#include <unordered_map>
#include <unordered_set>

static const std::string StaticParametersFilenameStem = "static_parameters";

template<typename Traits>
ShaderManager<Traits>::ShaderManager(std::filesystem::path const & shadersRoot)
{
    if (!std::filesystem::exists(shadersRoot))
        throw GameException("Shaders root path \"" + shadersRoot.string() + "\" does not exist");

    //
    // Make static parameters
    //

    std::map<std::string, std::string> staticParameters;

    // 1) From file
    std::filesystem::path localStaticParametersFilepath = shadersRoot / (StaticParametersFilenameStem + ".glslinc");
    if (std::filesystem::exists(localStaticParametersFilepath))
    {
        std::string localStaticParametersSource = Utils::LoadTextFile(localStaticParametersFilepath);
        ParseLocalStaticParameters(localStaticParametersSource, staticParameters);
    }

    //
    // Load all shader files
    //

    // Filename -> (isShader, source)
    std::unordered_map<std::string, std::pair<bool, std::string>> shaderSources;

    for (auto const & entryIt : std::filesystem::directory_iterator(shadersRoot))
    {
        if (std::filesystem::is_regular_file(entryIt.path())
            && entryIt.path().stem() != StaticParametersFilenameStem)
        {
            if (entryIt.path().extension() == ".glsl" || entryIt.path().extension() == ".glslinc")
            {
                std::string shaderFilename = entryIt.path().filename().string();

                assert(shaderSources.count(shaderFilename) == 0); // Guaranteed by file system

                shaderSources[shaderFilename] = std::make_pair<bool, std::string>(
                    entryIt.path().extension() == ".glsl",
                    Utils::LoadTextFile(entryIt.path()));
            }
            else
            {
                LogMessage("WARNING: found file \"" + entryIt.path().string() + "\" with unexpected extension while loading shaders");
            }
        }
    }


    //
    // Compile all shader files
    //

    for (auto const & entryIt : shaderSources)
    {
        if (entryIt.second.first)
        {
            CompileShader(
                entryIt.first,
                entryIt.second.second,
                shaderSources,
                staticParameters);
        }
    }


    //
    // Verify all expected programs have been loaded
    //

    for (uint32_t i = 0; i <= static_cast<uint32_t>(Traits::ProgramType::_Last); ++i)
    {
        if (i >= mPrograms.size() || !(mPrograms[i].OpenGLHandle))
        {
            throw GameException("Cannot find GLSL source file for program \"" + Traits::ProgramTypeToStr(static_cast<typename Traits::ProgramType>(i)) + "\"");
        }
    }
}

template<typename Traits>
void ShaderManager<Traits>::CompileShader(
    std::string const & shaderFilename,
    std::string const & shaderSource,
    std::unordered_map<std::string, std::pair<bool, std::string>> const & allShaderSources,
    std::map<std::string, std::string> const & staticParameters)
{
    try
    {
        // Get the program type
        std::filesystem::path shaderFilenamePath(shaderFilename);
        typename Traits::ProgramType const program = Traits::ShaderFilenameToProgramType(shaderFilenamePath.stem().string());
        std::string const programName = Traits::ProgramTypeToStr(program);

        // Make sure we have room for it
        size_t programIndex = static_cast<size_t>(program);
        if (programIndex + 1 > mPrograms.size())
        {
            mPrograms.resize(programIndex + 1);
        }

        // First time we see it (guaranteed by file system)
        assert(!(mPrograms[programIndex].OpenGLHandle));

        // Resolve includes
        std::string preprocessedShaderSource = ResolveIncludes(
            shaderSource,
            allShaderSources);

        // Split the source file
        auto [vertexShaderSource, fragmentShaderSource] = SplitSource(preprocessedShaderSource);


        //
        // Create program
        //

        mPrograms[programIndex].OpenGLHandle = glCreateProgram();
        CheckOpenGLError();


        //
        // Compile vertex shader
        //

        vertexShaderSource = SubstituteStaticParameters(vertexShaderSource, staticParameters);

        GameOpenGL::CompileShader(
            vertexShaderSource,
            GL_VERTEX_SHADER,
            mPrograms[programIndex].OpenGLHandle,
            programName);


        //
        // Compile fragment shader
        //

        fragmentShaderSource = SubstituteStaticParameters(fragmentShaderSource, staticParameters);

        GameOpenGL::CompileShader(
            fragmentShaderSource,
            GL_FRAGMENT_SHADER,
            mPrograms[programIndex].OpenGLHandle,
            programName);


        //
        // Link a first time, to enable extraction of attributes and uniforms
        //

        GameOpenGL::LinkShaderProgram(mPrograms[programIndex].OpenGLHandle, programName);


        //
        // Extract attribute names from vertex shader and bind them
        //

        std::set<std::string> vertexAttributeNames = ExtractVertexAttributeNames(mPrograms[programIndex].OpenGLHandle);

        for (auto const & vertexAttributeName : vertexAttributeNames)
        {
            auto vertexAttribute = Traits::StrToVertexAttributeType(vertexAttributeName);

            GameOpenGL::BindAttributeLocation(
                mPrograms[programIndex].OpenGLHandle,
                static_cast<GLuint>(vertexAttribute),
                "in" + vertexAttributeName);
        }


        //
        // Link a second time, to freeze vertex attribute binding
        //

        GameOpenGL::LinkShaderProgram(mPrograms[programIndex].OpenGLHandle, programName);


        //
        // Extract uniform locations
        //

        std::vector<GLint> uniformLocations;

        std::set<std::string> parameterNames = ExtractParameterNames(mPrograms[programIndex].OpenGLHandle);

        for (auto const & parameterName : parameterNames)
        {
            auto programParameter = Traits::StrToProgramParameterType(parameterName);

            // Make sure there is room
            size_t programParameterIndex = static_cast<size_t>(programParameter);
            while (mPrograms[programIndex].UniformLocations.size() <= programParameterIndex)
            {
                mPrograms[programIndex].UniformLocations.push_back(NoParameterLocation);
            }

            // Get and store
            mPrograms[programIndex].UniformLocations[programParameterIndex] = GameOpenGL::GetParameterLocation(
                mPrograms[programIndex].OpenGLHandle,
                "param" + Traits::ProgramParameterTypeToStr(programParameter));
        }
    }
    catch (GameException const & ex)
    {
        throw GameException("Error compiling shader file \"" + shaderFilename + "\": " + ex.what());
    }
}

template<typename Traits>
std::string ShaderManager<Traits>::ResolveIncludes(
    std::string const & shaderSource,
    std::unordered_map<std::string, std::pair<bool, std::string>> const & shaderSources)
{
    static std::regex const IncludeRegex(R"!(^\s*#include\s+\"\s*([_a-zA-Z0-9\.]+)\s*\"\s*$)!");

    std::unordered_set<std::string> resolvedIncludes;

    std::string resolvedSource = shaderSource;

    for (bool hasResolved = true; hasResolved; )
    {
        std::stringstream sSource(resolvedSource);
        std::stringstream sSubstitutedSource;

        hasResolved = false;

        std::string line;
        while (std::getline(sSource, line))
        {
            std::smatch match;
            if (std::regex_search(line, match, IncludeRegex))
            {
                //
                // Found an include
                //

                assert(2 == match.size());

                auto includeFilename = match[1].str();
                auto includeIt = shaderSources.find(includeFilename);
                if (includeIt == shaderSources.end())
                {
                    throw GameException("Cannot find include file \"" + includeFilename + "\"");
                }

                if (resolvedIncludes.count(includeFilename) > 0)
                {
                    throw GameException("Detected include file loop at include file \"" + includeFilename + "\"");
                }

                // Insert include
                sSubstitutedSource << includeIt->second.second << sSource.widen('\n');

                // Remember the files we've included in this path
                resolvedIncludes.insert(includeFilename);

                hasResolved = true;
            }
            else
            {
                sSubstitutedSource << line << sSource.widen('\n');
            }
        }

        resolvedSource = sSubstitutedSource.str();
    }

    return resolvedSource;
}

template<typename Traits>
std::tuple<std::string, std::string> ShaderManager<Traits>::SplitSource(std::string const & source)
{
    static std::regex const VertexHeaderRegex(R"!(\s*###VERTEX-(\d{3})\s*)!");
    static std::regex const FragmentHeaderRegex(R"!(\s*###FRAGMENT-(\d{3})\s*)!");

    std::stringstream sSource(source);

    std::string line;

    std::stringstream commonCode;
    std::stringstream vertexShaderCode;
    std::stringstream fragmentShaderCode;

    //
    // Common code
    //

    while (true)
    {
        if (!std::getline(sSource, line))
            throw GameException("Cannot find ###VERTEX declaration");

        std::smatch match;
        if (std::regex_search(line, match, VertexHeaderRegex))
        {
            // Found beginning of vertex shader

            // Initialize vertex shader GLSL version
            vertexShaderCode << "#version " << match[1].str() << sSource.widen('\n');

            // Initialize vertex shader with common code
            vertexShaderCode << commonCode.str();

            break;
        }
        else
        {
            commonCode << line << sSource.widen('\n');
        }
    }

    //
    // Vertex shader
    //

    while (true)
    {
        if (!std::getline(sSource, line))
            throw GameException("Cannot find ###FRAGMENT declaration");

        std::smatch match;
        if (std::regex_search(line, match, FragmentHeaderRegex))
        {
            // Found beginning of fragment shader

            // Initialize fragment shader GLSL version
            fragmentShaderCode << "#version " << match[1].str() << sSource.widen('\n');

            // Initialize fragment shader with common code
            fragmentShaderCode << commonCode.str();

            break;
        }
        else
        {
            vertexShaderCode << line << sSource.widen('\n');
        }
    }

    //
    // Fragment shader
    //

    while (std::getline(sSource, line))
    {
        fragmentShaderCode << line << sSource.widen('\n');
    }


    return std::make_tuple(
        vertexShaderCode.str(),
        fragmentShaderCode.str());
}

template<typename Traits>
void ShaderManager<Traits>::ParseLocalStaticParameters(
    std::string const & localStaticParametersSource,
    std::map<std::string, std::string> & staticParameters)
{
    static std::regex const StaticParamDefinitionRegex(R"!(^\s*([_a-zA-Z][_a-zA-Z0-9]*)\s*=\s*(.*?)\s*$)!");

    std::stringstream sSource(localStaticParametersSource);
    std::string line;
    while (std::getline(sSource, line))
    {
        line = Utils::Trim(line);

        if (!line.empty())
        {
            std::smatch match;
            if (!std::regex_search(line, match, StaticParamDefinitionRegex))
            {
                throw GameException("Error parsing static parameter definition \"" + line + "\"");
            }

            assert(3 == match.size());
            auto staticParameterName = match[1].str();
            auto staticParameterValue = match[2].str();

            // Check whether it's a dupe
            if (staticParameters.count(staticParameterName) > 0)
            {
                throw GameException("Static parameters \"" + staticParameterName + "\" has already been defined");
            }

            // Store
            staticParameters.insert(
                std::make_pair(
                    staticParameterName,
                    staticParameterValue));
        }
    }
}

template<typename Traits>
std::string ShaderManager<Traits>::SubstituteStaticParameters(
    std::string const & source,
    std::map<std::string, std::string> const & staticParameters)
{
    static std::regex const StaticParamNameRegex("%([_a-zA-Z][_a-zA-Z0-9]*)%");

    std::string remainingSource = source;
    std::stringstream sSubstitutedSource;
    std::smatch match;
    while (std::regex_search(remainingSource, match, StaticParamNameRegex))
    {
        assert(2 == match.size());
        auto staticParameterName = match[1].str();

        // Lookup the parameter
        auto const & paramIt = staticParameters.find(staticParameterName);
        if (paramIt == staticParameters.end())
        {
            throw GameException("Static parameter \"" + staticParameterName + "\" is not recognized");
        }

        // Substitute the parameter
        sSubstitutedSource << match.prefix();
        sSubstitutedSource << paramIt->second;

        // Advance
        remainingSource = match.suffix();
    }

    sSubstitutedSource << remainingSource;

    return sSubstitutedSource.str();
}

template<typename Traits>
std::set<std::string> ShaderManager<Traits>::ExtractVertexAttributeNames(GameOpenGLShaderProgram const & shaderProgram)
{
    std::set<std::string> attributeNames;

    GLint count;
    glGetProgramiv(*shaderProgram, GL_ACTIVE_ATTRIBUTES, &count);

    for (GLuint i = 0; i < static_cast<GLuint>(count); ++i)
    {
        char nameBuffer[256];
        GLsizei nameLength;
        GLint attributeSize;
        GLenum attributeType;

        glGetActiveAttrib(
            *shaderProgram,
            i,
            static_cast<GLsizei>(sizeof(nameBuffer)),
            &nameLength,
            &attributeSize,
            &attributeType,
            nameBuffer);
        CheckOpenGLError();

        if (nameLength < 2 || strncmp(nameBuffer, "in", 2))
        {
            throw GameException("Attribute name \"" + std::string(nameBuffer, nameLength) + "\" does not follow the expected name structure: missing \"in\" prefix");
        }

        std::string const attributeName(nameBuffer + 2, nameLength - 2);

        // Lookup the attribute name - just as a sanity check
        Traits::StrToVertexAttributeType(attributeName);

        // Store it, making sure it's not specified more than once
        if (!attributeNames.insert(attributeName).second)
        {
            throw GameException("Attribute name \"" + attributeName + "\" is declared more than once");
        }
    }

    return attributeNames;
}

template<typename Traits>
std::set<std::string> ShaderManager<Traits>::ExtractParameterNames(GameOpenGLShaderProgram const & shaderProgram)
{
    static std::regex const ArrayParameterRegEx(R"!(^(.+)\[[0-9]+\]$)!");
    static char constexpr ParamPrefix[5] = { 'p', 'a', 'r', 'a', 'm' };

    std::set<std::string> parameterNames;

    GLint count;
    glGetProgramiv(*shaderProgram, GL_ACTIVE_UNIFORMS, &count);

    for (GLuint i = 0; i < static_cast<GLuint>(count); ++i)
    {
        char nameBuffer[256];
        GLsizei nameLength;
        GLint attributeSize;
        GLenum attributeType;

        glGetActiveUniform(
            *shaderProgram,
            i,
            static_cast<GLsizei>(sizeof(nameBuffer)),
            &nameLength,
            &attributeSize,
            &attributeType,
            nameBuffer);
        CheckOpenGLError();

        if (nameLength < sizeof(ParamPrefix) || strncmp(nameBuffer, ParamPrefix, sizeof(ParamPrefix)))
        {
            throw GameException("Uniform name \"" + std::string(nameBuffer, nameLength) + "\" does not follow the expected name structure: missing \"param\" prefix");
        }

        // Remove "param" prefix
        std::string parameterName(nameBuffer + sizeof(ParamPrefix), nameLength - sizeof(ParamPrefix));

        // Check if it's an array (element)
        std::smatch arrayParameterMatch;
        if (std::regex_match(parameterName, arrayParameterMatch, ArrayParameterRegEx))
        {
            // Remove suffix
            assert(arrayParameterMatch.size() == 1 + 1);
            parameterName = arrayParameterMatch[1].str();
        }

        // Lookup the parameter name - just as a sanity check
        Traits::StrToProgramParameterType(parameterName);

        // Store it, making sure it's not specified more than once
        if (!parameterNames.insert(parameterName).second
            && arrayParameterMatch.empty())
        {
            throw GameException("Uniform name \"" + parameterName + "\" is declared more than once");
        }
    }

    return parameterNames;
}