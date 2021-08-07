/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2018-05-12
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#pragma once

#include "GameEventDispatcher.h"
#include "GameParameters.h"
#include "Materials.h"
#include "RenderContext.h"

#include <GameCore/Buffer.h>
#include <GameCore/BufferAllocator.h>
#include <GameCore/ElementContainer.h>
#include <GameCore/EnumFlags.h>
#include <GameCore/FixedSizeVector.h>

#include <cassert>
#include <functional>
#include <limits>

namespace Physics
{

class Springs : public ElementContainer
{
public:

    enum class DestroyOptions
    {
        DoNotFireBreakEvent = 0,
        FireBreakEvent = 1,

        DestroyOnlyConnectedTriangle = 0,
        DestroyAllTriangles = 2
    };

    /*
     * The endpoints of a spring.
     */
    struct Endpoints
    {
        ElementIndex PointAIndex;
        ElementIndex PointBIndex;

        Endpoints(
            ElementIndex pointAIndex,
            ElementIndex pointBIndex)
            : PointAIndex(pointAIndex)
            , PointBIndex(pointBIndex)
        {}
    };

    /*
     * The pre-calculated coefficients used for the spring dynamics.
     */
    struct DynamicsCoefficients
    {
        float StiffnessCoefficient;
        float DampingCoefficient;

        DynamicsCoefficients(
            float stiffnessCoefficient,
            float dampingCoefficient)
            : StiffnessCoefficient(stiffnessCoefficient)
            , DampingCoefficient(dampingCoefficient)
        {}
    };

private:

    /*
     * The factory angle of the spring from the point of view
     * of each endpoint.
     *
     * Angle 0 is E, angle 1 is SE, ..., angle 7 is NE.
     */
    struct EndpointOctants
    {
        Octant PointAOctant;
        Octant PointBOctant;

        EndpointOctants(
            Octant pointAOctant,
            Octant pointBOctant)
            : PointAOctant(pointAOctant)
            , PointBOctant(pointBOctant)
        {}
    };

    /*
     * The triangles that have an edge along this spring.
     */
    using SuperTrianglesVector = FixedSizeVector<ElementIndex, 2>;

    /*
     * Lump of properties that are commonly used together .
     */
    struct MaterialProperties
    {
        float MaterialStiffness;
        float MaterialStrength;
        float MaterialMeltingTemperature;
        float ExtraMeltingInducedTolerance; // Pre-calcd

        MaterialProperties(
            float materialStiffness,
            float materialStrength,
            float materialMeltingTemperature,
            float extraMeltingInducedTolerance)
            : MaterialStiffness(materialStiffness)
            , MaterialStrength(materialStrength)
            , MaterialMeltingTemperature(materialMeltingTemperature)
            , ExtraMeltingInducedTolerance(extraMeltingInducedTolerance)
        {}
    };

public:

    Springs(
        ElementCount elementCount,
        World & parentWorld,
        std::shared_ptr<GameEventDispatcher> gameEventDispatcher,
        GameParameters const & gameParameters)
        : ElementContainer(elementCount)
        //////////////////////////////////
        // Buffers
        //////////////////////////////////
        , mIsDeletedBuffer(mBufferElementCount, mElementCount, true)
        // Endpoints
        , mEndpointsBuffer(mBufferElementCount, mElementCount, Endpoints(NoneElementIndex, NoneElementIndex))
        // Factory endpoint octants
        , mFactoryEndpointOctantsBuffer(mBufferElementCount, mElementCount, EndpointOctants(0, 4))
        // Super triangles
        , mSuperTrianglesBuffer(mBufferElementCount, mElementCount, SuperTrianglesVector())
        , mFactorySuperTrianglesBuffer(mBufferElementCount, mElementCount, SuperTrianglesVector())
        // Covering triangles
        , mCoveringTrianglesCountBuffer(mBufferElementCount, mElementCount, 0)
        // Physical
        , mBreakingElongationBuffer(mBufferElementCount, mElementCount, 0.0f)
        , mFactoryRestLengthBuffer(mBufferElementCount, mElementCount, 1.0f)
        , mRestLengthBuffer(mBufferElementCount, mElementCount, 1.0f)
        , mDynamicsCoefficientsBuffer(mBufferElementCount, mElementCount, DynamicsCoefficients(0.0f, 0.0f))
        , mMaterialPropertiesBuffer(mBufferElementCount, mElementCount, MaterialProperties(0.0f, 0.0f, 0.0f, 0.0f))
        , mBaseStructuralMaterialBuffer(mBufferElementCount, mElementCount, nullptr)
        , mIsRopeBuffer(mBufferElementCount, mElementCount, false)
        // Water
        , mWaterPermeabilityBuffer(mBufferElementCount, mElementCount, 0.0f)
        // Heat
        , mMaterialThermalConductivityBuffer(mBufferElementCount, mElementCount, 0.0f)
        // Stress
        , mIsStressedBuffer(mBufferElementCount, mElementCount, false)
        //////////////////////////////////
        // Container
        //////////////////////////////////
        , mParentWorld(parentWorld)
        , mGameEventHandler(std::move(gameEventDispatcher))
        , mShipPhysicsHandler(nullptr)
        , mCurrentNumMechanicalDynamicsIterations(gameParameters.NumMechanicalDynamicsIterations<float>())
        , mCurrentNumMechanicalDynamicsIterationsAdjustment(gameParameters.NumMechanicalDynamicsIterationsAdjustment)
        , mCurrentSpringStiffnessAdjustment(gameParameters.SpringStiffnessAdjustment)
        , mCurrentSpringDampingAdjustment(gameParameters.SpringDampingAdjustment)
        , mCurrentSpringStrengthAdjustment(gameParameters.SpringStrengthAdjustment)
        , mCurrentMeltingTemperatureAdjustment(gameParameters.MeltingTemperatureAdjustment)
        , mFloatBufferAllocator(mBufferElementCount)
        , mVec2fBufferAllocator(mBufferElementCount)
    {
    }

    Springs(Springs && other) = default;

    void RegisterShipPhysicsHandler(IShipPhysicsHandler * shipPhysicsHandler)
    {
        mShipPhysicsHandler = shipPhysicsHandler;
    }

    void Add(
        ElementIndex pointAIndex,
        ElementIndex pointBIndex,
        Octant factoryPointAOctant,
        Octant factoryPointBOctant,
        SuperTrianglesVector const & superTriangles,
        ElementCount coveringTrianglesCount,
        Points const & points);

    void Destroy(
        ElementIndex springElementIndex,
        DestroyOptions destroyOptions,
        GameParameters const & gameParameters,
        Points const & points);

    void Restore(
        ElementIndex springElementIndex,
        GameParameters const & gameParameters,
        Points const & points);

    void UpdateForGameParameters(
        GameParameters const & gameParameters,
        Points const & points);

    void UpdateForDecayAndTemperatureAndGameParameters(
        GameParameters const & gameParameters,
        Points const & points);

    void UpdateForRestLength(
        ElementIndex springElementIndex,
        Points const & points)
    {
        // Recalculate parameters for this spring
        UpdateForDecayAndTemperatureAndGameParameters(
            springElementIndex,
            mCurrentNumMechanicalDynamicsIterations,
            mCurrentSpringStiffnessAdjustment,
            mCurrentSpringDampingAdjustment,
            mCurrentSpringStrengthAdjustment,
            CalculateSpringStrengthIterationsAdjustment(mCurrentNumMechanicalDynamicsIterationsAdjustment),
            mCurrentMeltingTemperatureAdjustment,
            points);
    }

    void UpdateForMass(
        ElementIndex springElementIndex,
        Points const & points)
    {
        // Recalculate parameters for this spring
        UpdateForDecayAndTemperatureAndGameParameters(
            springElementIndex,
            mCurrentNumMechanicalDynamicsIterations,
            mCurrentSpringStiffnessAdjustment,
            mCurrentSpringDampingAdjustment,
            mCurrentSpringStrengthAdjustment,
            CalculateSpringStrengthIterationsAdjustment(mCurrentNumMechanicalDynamicsIterationsAdjustment),
            mCurrentMeltingTemperatureAdjustment,
            points);
    }

    /*
     * Calculates the current strain - due to tension or compression - and acts depending on it,
     * eventually breaking springs.
     */
    void UpdateForStrains(
        GameParameters const & gameParameters,
        Points & points);

    //
    // Render
    //

    void UploadElements(
        ShipId shipId,
        Render::RenderContext & renderContext) const;

    void UploadStressedSpringElements(
        ShipId shipId,
        Render::RenderContext & renderContext) const;

public:

    //
    // IsDeleted
    //

    bool IsDeleted(ElementIndex springElementIndex) const
    {
        return mIsDeletedBuffer[springElementIndex];
    }

    //
    // Endpoints
    //

    ElementIndex GetEndpointAIndex(ElementIndex springElementIndex) const noexcept
    {
        return mEndpointsBuffer[springElementIndex].PointAIndex;
    }

    ElementIndex GetEndpointBIndex(ElementIndex springElementIndex) const noexcept
    {
        return mEndpointsBuffer[springElementIndex].PointBIndex;
    }

    ElementIndex GetOtherEndpointIndex(
        ElementIndex springElementIndex,
        ElementIndex pointElementIndex) const
    {
        if (pointElementIndex == mEndpointsBuffer[springElementIndex].PointAIndex)
            return mEndpointsBuffer[springElementIndex].PointBIndex;
        else
        {
            assert(pointElementIndex == mEndpointsBuffer[springElementIndex].PointBIndex);
            return mEndpointsBuffer[springElementIndex].PointAIndex;
        }
    }

    Endpoints const * GetEndpointsBuffer() const noexcept
    {
        return mEndpointsBuffer.data();
    }

    // Returns +1.0 if the spring is directed outward from the specified point;
    // otherwise, -1.0.
    float GetSpringDirectionFrom(
        ElementIndex springElementIndex,
        ElementIndex pointIndex) const
    {
        if (pointIndex == mEndpointsBuffer[springElementIndex].PointAIndex)
            return 1.0f;
        else
            return -1.0f;
    }

    vec2f const & GetEndpointAPosition(
        ElementIndex springElementIndex,
        Points const & points) const
    {
        return points.GetPosition(mEndpointsBuffer[springElementIndex].PointAIndex);
    }

    vec2f const & GetEndpointBPosition(
        ElementIndex springElementIndex,
        Points const & points) const
    {
        return points.GetPosition(mEndpointsBuffer[springElementIndex].PointBIndex);
    }

    vec2f GetMidpointPosition(
        ElementIndex springElementIndex,
        Points const & points) const
    {
        return (GetEndpointAPosition(springElementIndex, points)
            + GetEndpointBPosition(springElementIndex, points)) / 2.0f;
    }

    PlaneId GetPlaneId(
        ElementIndex springElementIndex,
        Points const & points) const
    {
        // Return, quite arbitrarily, the plane of point A
        // (the two endpoints might have different plane IDs in case, for example,
        // this spring connects a "string" to a triangle)
        return points.GetPlaneId(GetEndpointAIndex(springElementIndex));
    }

    //
    // Factory endpoint octants
    //

    Octant GetFactoryEndpointAOctant(ElementIndex springElementIndex) const
    {
        return mFactoryEndpointOctantsBuffer[springElementIndex].PointAOctant;
    }

    Octant GetFactoryEndpointBOctant(ElementIndex springElementIndex) const
    {
        return mFactoryEndpointOctantsBuffer[springElementIndex].PointBOctant;
    }

    Octant GetFactoryEndpointOctant(
        ElementIndex springElementIndex,
        ElementIndex pointElementIndex) const
    {
        if (pointElementIndex == GetEndpointAIndex(springElementIndex))
            return GetFactoryEndpointAOctant(springElementIndex);
        else
        {
            assert(pointElementIndex == GetEndpointBIndex(springElementIndex));
            return GetFactoryEndpointBOctant(springElementIndex);
        }
    }

    Octant GetFactoryOtherEndpointOctant(
        ElementIndex springElementIndex,
        ElementIndex pointElementIndex) const
    {
        if (pointElementIndex == GetEndpointAIndex(springElementIndex))
            return GetFactoryEndpointBOctant(springElementIndex);
        else
        {
            assert(pointElementIndex == GetEndpointBIndex(springElementIndex));
            return GetFactoryEndpointAOctant(springElementIndex);
        }
    }

    //
    // Super triangles
    //

    inline auto const & GetSuperTriangles(ElementIndex springElementIndex) const
    {
        return mSuperTrianglesBuffer[springElementIndex];
    }

    inline void AddSuperTriangle(
        ElementIndex springElementIndex,
        ElementIndex superTriangleElementIndex)
    {
        assert(mFactorySuperTrianglesBuffer[springElementIndex].contains(
            [superTriangleElementIndex](auto st)
            {
                return st == superTriangleElementIndex;
            }));

        mSuperTrianglesBuffer[springElementIndex].push_back(superTriangleElementIndex);
    }

    inline void RemoveSuperTriangle(
        ElementIndex springElementIndex,
        ElementIndex superTriangleElementIndex)
    {
        bool found = mSuperTrianglesBuffer[springElementIndex].erase_first(superTriangleElementIndex);

        assert(found);
        (void)found;
    }

    inline void ClearSuperTriangles(ElementIndex springElementIndex)
    {
        mSuperTrianglesBuffer[springElementIndex].clear();
    }

    auto const & GetFactorySuperTriangles(ElementIndex springElementIndex) const
    {
        return mFactorySuperTrianglesBuffer[springElementIndex];
    }

    void RestoreFactorySuperTriangles(ElementIndex springElementIndex)
    {
        assert(mSuperTrianglesBuffer[springElementIndex].empty());

        std::copy(
            mFactorySuperTrianglesBuffer[springElementIndex].cbegin(),
            mFactorySuperTrianglesBuffer[springElementIndex].cend(),
            mSuperTrianglesBuffer[springElementIndex].begin());
    }

    //
    // Covering triangles
    //

    ElementCount GetCoveringTrianglesCount(ElementIndex springElementIndex) const
    {
        return mCoveringTrianglesCountBuffer[springElementIndex];
    }

    inline void AddCoveringTriangle(ElementIndex springElementIndex)
    {
        assert(mCoveringTrianglesCountBuffer[springElementIndex] < 2);
        ++(mCoveringTrianglesCountBuffer[springElementIndex]);
    }

    inline void RemoveCoveringTriangle(ElementIndex springElementIndex)
    {
        assert(mCoveringTrianglesCountBuffer[springElementIndex] > 0);
        --(mCoveringTrianglesCountBuffer[springElementIndex]);
    }

    //
    // Physical
    //

    float GetLength(
        ElementIndex springElementIndex,
        Points const & points) const
    {
        return
            (points.GetPosition(GetEndpointAIndex(springElementIndex)) - points.GetPosition(GetEndpointBIndex(springElementIndex)))
            .length();
    }

    float GetFactoryRestLength(ElementIndex springElementIndex) const
    {
        return mFactoryRestLengthBuffer[springElementIndex];
    }

    float GetRestLength(ElementIndex springElementIndex) const noexcept
    {
        return mRestLengthBuffer[springElementIndex];
    }

    float const * GetRestLengthBuffer() const noexcept
    {
        return mRestLengthBuffer.data();
    }

    void SetRestLength(
        ElementIndex springElementIndex,
        float restLength)
    {
        mRestLengthBuffer[springElementIndex] = restLength;
    }

    float GetStiffnessCoefficient(ElementIndex springElementIndex) const noexcept
    {
        return mDynamicsCoefficientsBuffer[springElementIndex].StiffnessCoefficient;
    }

    float GetDampingCoefficient(ElementIndex springElementIndex) const noexcept
    {
        return mDynamicsCoefficientsBuffer[springElementIndex].DampingCoefficient;
    }

    DynamicsCoefficients const * GetDynamicsCoefficientsBuffer() const noexcept
    {
        return mDynamicsCoefficientsBuffer.data();
    }

    float GetMaterialStrength(ElementIndex springElementIndex) const
    {
        return mMaterialPropertiesBuffer[springElementIndex].MaterialStrength;
    }

    float GetMaterialStiffness(ElementIndex springElementIndex) const
    {
        return mMaterialPropertiesBuffer[springElementIndex].MaterialStiffness;
    }

    float GetMaterialMeltingTemperature(ElementIndex springElementIndex) const
    {
        return mMaterialPropertiesBuffer[springElementIndex].MaterialMeltingTemperature;
    }

    float GetExtraMeltingInducedTolerance(ElementIndex springElementIndex) const
    {
        return mMaterialPropertiesBuffer[springElementIndex].ExtraMeltingInducedTolerance;
    }

    StructuralMaterial const & GetBaseStructuralMaterial(ElementIndex springElementIndex) const
    {
        // If this method is invoked, this is not a placeholder
        assert(nullptr != mBaseStructuralMaterialBuffer[springElementIndex]);
        return *(mBaseStructuralMaterialBuffer[springElementIndex]);
    }

    inline bool IsRope(ElementIndex springElementIndex) const
    {
        return mIsRopeBuffer[springElementIndex];
    }

    //
    // Water
    //

    float GetWaterPermeability(ElementIndex springElementIndex) const
    {
        return mWaterPermeabilityBuffer[springElementIndex];
    }

    void SetWaterPermeability(
        ElementIndex springElementIndex,
        float value)
    {
        mWaterPermeabilityBuffer[springElementIndex] = value;
    }

    float const * GetWaterPermeabilityBuffer() const
    {
        return mWaterPermeabilityBuffer.data();
    }

    //
    // Heat
    //

    float GetMaterialThermalConductivity(ElementIndex springElementIndex) const
    {
        return mMaterialThermalConductivityBuffer[springElementIndex];
    }

    //
    // Temporary buffer
    //

    std::shared_ptr<Buffer<float>> AllocateWorkBufferFloat()
    {
        return mFloatBufferAllocator.Allocate();
    }

    std::shared_ptr<Buffer<vec2f>> AllocateWorkBufferVec2f()
    {
        return mVec2fBufferAllocator.Allocate();
    }

private:

    static float CalculateSpringStrengthIterationsAdjustment(float numMechanicalDynamicsIterationsAdjustment)
    {
        // We need to adjust the strength - i.e. the displacement tolerance or spring breaking point - based
        // on the actual number of mechanics iterations we'll be performing.
        //
        // After one iteration the spring displacement dL = L - L0 is reduced to:
        //  dL * (1-SRF)
        // where SRF is the value of the SpringReductionFraction parameter. After N iterations this would be:
        //  dL * (1-SRF)^N
        //
        // This formula suggests a simple exponential relationship, but empirical data (e.g. explosions on the Titanic)
        // suggest the following relationship:
        //
        //  s' = s * 4 / (1 + 3*(R^1.3))
        //
        // Where R is the N'/N ratio.

        return
            4.0f
            / (1.0f + 3.0f * pow(numMechanicalDynamicsIterationsAdjustment, 1.3f));
    }

    static float CalculateExtraMeltingInducedTolerance(float strength)
    {
        // The extra elongation tolerance due to melting:
        //  - For small factory tolerances (~0.1), we are keen to get up to many times that tolerance
        //  - For large factory tolerances (~5.0), we are keen to get up to fewer times that tolerance
        //    (i.e. allow smaller change in length)
        float constexpr MaxMeltingInducedTolerance = 20; // Was 20 up to 1.16.5
        float constexpr MinMeltingInducedTolerance = 0.0f;
        float constexpr StartStrength = 0.3f; // At this strength, we allow max tolerance
        float constexpr EndStrength = 3.0f; // At this strength, we allow min tolerance

        return MaxMeltingInducedTolerance -
            (MaxMeltingInducedTolerance - MinMeltingInducedTolerance)
            / (EndStrength - StartStrength)
            * (Clamp(strength, StartStrength, EndStrength) - StartStrength);
    }

    void UpdateForDecayAndTemperatureAndGameParameters(
        float numMechanicalDynamicsIterations,
        float numMechanicalDynamicsIterationsAdjustment,
        float stiffnessAdjustment,
        float dampingAdjustment,
        float strengthAdjustment,
        float meltingTemperatureAdjustment,
        Points const & points);

    void UpdateForDecayAndTemperatureAndGameParameters(
        ElementIndex springIndex,
        float numMechanicalDynamicsIterations,
        float stiffnessAdjustment,
        float dampingAdjustment,
        float strengthAdjustment,
        float strengthIterationsAdjustment,
        float meltingTemperatureAdjustment,
        Points const & points);

    inline void inline_UpdateForDecayAndTemperatureAndGameParameters(
        ElementIndex springIndex,
        float numMechanicalDynamicsIterations,
        float stiffnessAdjustment,
        float dampingAdjustment,
        float strengthAdjustment,
        float strengthIterationsAdjustment,
        float meltingTemperatureAdjustment,
        Points const & points);

private:

    //////////////////////////////////////////////////////////
    // Buffers
    //////////////////////////////////////////////////////////

    // Deletion
    Buffer<bool> mIsDeletedBuffer;

    // Endpoints
    Buffer<Endpoints> mEndpointsBuffer;

    // Factory-time endpoint octants
    Buffer<EndpointOctants> mFactoryEndpointOctantsBuffer;

    // Indexes of the triangles having this spring as edge.
    // A spring may have between 0 and 2 super triangles.
    Buffer<SuperTrianglesVector> mSuperTrianglesBuffer;
    Buffer<SuperTrianglesVector> mFactorySuperTrianglesBuffer;

    // Number of triangles covering this spring.
    // "Covering triangles" are triangles that "cover" this spring when they're rendered - it's either triangles that
    // have this spring as one of their edges (i.e. super triangles), or triangles that (partially) cover this spring
    // (i.e. when this spring is the non-edge diagonal of a two-triangle square, i.e. a "traverse" spring).
    // A spring may have between 0 and 2 covering triangles.
    Buffer<ElementCount> mCoveringTrianglesCountBuffer;

    //
    // Physical
    //

    Buffer<float> mBreakingElongationBuffer;
    Buffer<float> mFactoryRestLengthBuffer;
    Buffer<float> mRestLengthBuffer;
    Buffer<DynamicsCoefficients> mDynamicsCoefficientsBuffer;
    Buffer<MaterialProperties> mMaterialPropertiesBuffer;
    Buffer<StructuralMaterial const *> mBaseStructuralMaterialBuffer;
    Buffer<bool> mIsRopeBuffer;

    //
    // Water
    //

    // Water propagates through this spring according to this value;
    // 0.0 makes water not propagate.
    // Changed externally dynamically, as a resultant of material
    // hullness and other dynamic factors
    Buffer<float> mWaterPermeabilityBuffer;

    //
    // Heat
    //

    Buffer<float> mMaterialThermalConductivityBuffer;

    //
    // Stress
    //

    // State variable that tracks when we enter and exit the stressed state
    Buffer<bool> mIsStressedBuffer;

    //////////////////////////////////////////////////////////
    // Container
    //////////////////////////////////////////////////////////

    World & mParentWorld;
    std::shared_ptr<GameEventDispatcher> const mGameEventHandler;
    IShipPhysicsHandler * mShipPhysicsHandler;

    // The game parameter values that we are current with; changes
    // in the values of these parameters will trigger a re-calculation
    // of pre-calculated coefficients
    float mCurrentNumMechanicalDynamicsIterations;
    float mCurrentNumMechanicalDynamicsIterationsAdjustment;
    float mCurrentSpringStiffnessAdjustment;
    float mCurrentSpringDampingAdjustment;
    float mCurrentSpringStrengthAdjustment;
    float mCurrentMeltingTemperatureAdjustment;

    // Allocators for work buffers
    BufferAllocator<float> mFloatBufferAllocator;
    BufferAllocator<vec2f> mVec2fBufferAllocator;
};

}

template <> struct is_flag<Physics::Springs::DestroyOptions> : std::true_type {};
