// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2024
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
// https://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// Version: 6.0.2023.08.08

#pragma once

#include <Mathematics/ParticleSystem.h>
#include <cstdint>
#include <set>
#include <vector>

namespace gte
{
    template <int32_t N, typename Real>
    class MassSpringArbitrary : public ParticleSystem<N, Real>
    {
    public:
        // Construction and destruction.  This class represents a set of M
        // masses that are connected by S springs with arbitrary topology.
        // The function SetSpring(...) should be called for each spring that
        // you want in the system.
        virtual ~MassSpringArbitrary() = default;

        MassSpringArbitrary(int32_t numParticles, int32_t numSprings, Real step)
            :
            ParticleSystem<N, Real>(numParticles, step),
            mSpring(numSprings, Spring()),
            mAdjacent(numParticles)
        {
        }

        struct Spring
        {
            Spring()
                :
                particle0(0),
                particle1(0),
                constant((Real)0),
                length((Real)0)
            {
            }

            int32_t particle0, particle1;
            Real constant, length;
        };

        // Member access.
        inline int32_t GetNumSprings() const
        {
            return static_cast<int32_t>(mSpring.size());
        }

        void SetSpring(int32_t index, Spring const& spring)
        {
            mSpring[index] = spring;
            mAdjacent[spring.particle0].insert(index);
            mAdjacent[spring.particle1].insert(index);
        }

        inline Spring const& GetSpring(int32_t index) const
        {
            return mSpring[index];
        }

        // The default external force is zero.  Derive a class from this one
        // to provide nonzero external forces such as gravity, wind, friction,
        // and so on.  This function is called by Acceleration(...) to compute
        // the impulse F/m generated by the external force F.
        virtual Vector<N, Real> ExternalAcceleration(int32_t, Real,
            std::vector<Vector<N, Real>> const&,
            std::vector<Vector<N, Real>> const&)
        {
            return Vector<N, Real>::Zero();
        }

    protected:
        // Callback for acceleration (ODE solver uses x" = F/m) applied to
        // particle i.  The positions and velocities are not necessarily
        // mPosition and mVelocity, because the ODE solver evaluates the
        // impulse function at intermediate positions.
        virtual Vector<N, Real> Acceleration(int32_t i, Real time,
            std::vector<Vector<N, Real>> const& position,
            std::vector<Vector<N, Real>> const& velocity)
        {
            // Compute spring forces on position X[i].  The positions are not
            // necessarily mPosition, because the RK4 solver in ParticleSystem
            // evaluates the acceleration function at intermediate positions.

            Vector<N, Real> acceleration = ExternalAcceleration(i, time, position, velocity);

            for (auto adj : mAdjacent[i])
            {
                // Process a spring connected to particle i.
                Spring const& spring = mSpring[adj];
                Vector<N, Real> diff;
                if (i != spring.particle0)
                {
                    diff = position[spring.particle0] - position[i];
                }
                else
                {
                    diff = position[spring.particle1] - position[i];
                }

                Real ratio = spring.length / Length(diff);
                Vector<N, Real> force = spring.constant * ((Real)1 - ratio) * diff;
                acceleration += this->mInvMass[i] * force;
            }

            return acceleration;
        }

        std::vector<Spring> mSpring;

        // Each particle has an associated array of spring indices for those
        // springs adjacent to the particle.  The set elements are spring
        // indices, not indices of adjacent particles.
        std::vector<std::set<int32_t>> mAdjacent;
    };
}
