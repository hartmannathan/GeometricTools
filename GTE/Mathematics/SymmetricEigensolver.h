// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2025
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
// https://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// Version: 6.0.2023.08.08

#pragma once

// The SymmetricEigensolver class is an implementation of Algorithm 8.2.3
// (Symmetric QR Algorithm) described in "Matrix Computations, 2nd edition"
// by G. H. Golub and C. F. Van Loan, The Johns Hopkins University Press,
// Baltimore MD, Fourth Printing 1993.  Algorithm 8.2.1 (Householder
// Tridiagonalization) is used to reduce matrix A to tridiagonal T.
// Algorithm 8.2.2 (Implicit Symmetric QR Step with Wilkinson Shift) is
// used for the iterative reduction from tridiagonal to diagonal.  If A is
// the original matrix, D is the diagonal matrix of eigenvalues, and Q is
// the orthogonal matrix of eigenvectors, then theoretically Q^T*A*Q = D.
// Numerically, we have errors E = Q^T*A*Q - D.  Algorithm 8.2.3 mentions
// that one expects |E| is approximately u*|A|, where |M| denotes the
// Frobenius norm of M and where u is the unit roundoff for the
// floating-point arithmetic: 2^{-23} for 'float', which is FLT_EPSILON
// = 1.192092896e-7f, and 2^{-52} for'double', which is DBL_EPSILON
// = 2.2204460492503131e-16.
//
// The condition |a(i,i+1)| <= epsilon*(|a(i,i) + a(i+1,i+1)|) used to
// determine when the reduction decouples to smaller problems is implemented
// as:  sum = |a(i,i)| + |a(i+1,i+1)|; sum + |a(i,i+1)| == sum.  The idea is
// that the superdiagonal term is small relative to its diagonal neighbors,
// and so it is effectively zero.  The unit tests have shown that this
// interpretation of decoupling is effective.
//
// The authors suggest that once you have the tridiagonal matrix, a practical
// implementation will store the diagonal and superdiagonal entries in linear
// arrays, ignoring the theoretically zero values not in the 3-band.  This is
// good for cache coherence.  The authors also suggest storing the Householder
// vectors in the lower-triangular portion of the matrix to save memory.  The
// implementation uses both suggestions.
//
// For matrices with randomly generated values in [0,1], the unit tests
// produce the following information for N-by-N matrices.
//
// N  |A|     |E|        |E|/|A|    iterations
// -------------------------------------------
//  2  1.2332 5.5511e-17 4.5011e-17  1
//  3  2.0024 1.1818e-15 5.9020e-16  5
//  4  2.8708 9.9287e-16 3.4585e-16  7
//  5  2.9040 2.5958e-15 8.9388e-16  9
//  6  4.0427 2.2434e-15 5.5493e-16 12
//  7  5.0276 3.2456e-15 6.4555e-16 15
//  8  5.4468 6.5626e-15 1.2048e-15 15
//  9  6.1510 4.0317e-15 6.5545e-16 17
// 10  6.7523 4.9334e-15 7.3062e-16 21
// 11  7.1322 7.1347e-15 1.0003e-15 22
// 12  7.8324 5.6106e-15 7.1633e-16 24
// 13  8.1073 5.1366e-15 6.3357e-16 25
// 14  8.6257 8.3496e-15 9.6798e-16 29
// 15  9.2603 6.9526e-15 7.5080e-16 31
// 16  9.9853 6.5807e-15 6.5904e-16 32
// 17 10.5388 8.0931e-15 7.6793e-16 35
// 18 11.2377 1.1149e-14 9.9218e-16 38
// 19 11.7105 1.0711e-14 9.1470e-16 42
// 20 12.2227 1.7723e-14 1.4500e-15 42
// 21 12.7411 1.2515e-14 9.8231e-16 47
// 22 13.4462 1.2645e-14 9.4046e-16 50
// 23 13.9541 1.2899e-14 9.2444e-16 47
// 24 14.3298 1.6337e-14 1.1400e-15 53
// 25 14.8050 1.4760e-14 9.9701e-16 54
// 26 15.3914 1.7076e-14 1.1094e-15 57
// 27 15.8430 1.9714e-14 1.2443e-15 60
// 28 16.4771 1.7148e-14 1.0407e-15 60
// 29 16.9909 1.7309e-14 1.0187e-15 60
// 30 17.4456 2.1453e-14 1.2297e-15 64
// 31 17.9909 2.2069e-14 1.2267e-15 68
//
// The eigenvalues and |E|/|A| values were compared to those generated by
// Mathematica Version 9.0; Wolfram Research, Inc., Champaign IL, 2012.
// The results were all comparable with eigenvalues agreeing to a large
// number of decimal places.
//
// Timing on an Intel (R) Core (TM) i7-3930K CPU @ 3.20 GHZ (in seconds):
//
// N    |E|/|A|    iters tridiag QR     evecs    evec[N]  comperr
// --------------------------------------------------------------
//  512 4.4149e-15 1017   0.180  0.005    1.151    0.848    2.166
// 1024 6.1691e-15 1990   1.775  0.031   11.894   12.759   21.179
// 2048 8.5108e-15 3849  16.592  0.107  119.744  116.56   212.227
//
// where iters is the number of QR steps taken, tridiag is the computation
// of the Householder reflection vectors, evecs is the composition of
// Householder reflections and Givens rotations to obtain the matrix of
// eigenvectors, evec[N] is N calls to get the eigenvectors separately, and
// comperr is the computation E = Q^T*A*Q - D.  The construction of the full
// eigenvector matrix is, of course, quite expensive.  If you need only a
// small number of eigenvectors, use function GetEigenvector(int32_t,Real*).

#include <Mathematics/RangeIteration.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace gte
{
    template <typename Real>
    class SymmetricEigensolver
    {
    public:
        // The solver processes NxN symmetric matrices, where N > 1 ('size'
        // is N) and the matrix is stored in row-major order.  The maximum
        // number of iterations ('maxIterations') must be specified for the
        // reduction of a tridiagonal matrix to a diagonal matrix.  The goal
        // is to compute NxN orthogonal Q and NxN diagonal D for which
        // Q^T*A*Q = D.
        SymmetricEigensolver(int32_t size, uint32_t maxIterations)
            :
            mSize(0),
            mMaxIterations(0),
            mEigenvectorMatrixType(-1)
        {
            if (size > 1 && maxIterations > 0)
            {
                size_t szSize = static_cast<size_t>(size);
                mSize = size;
                mMaxIterations = maxIterations;
                mMatrix.resize(szSize * szSize);
                mDiagonal.resize(szSize);
                mSuperdiagonal.resize(szSize - 1);
                mGivens.reserve(static_cast<size_t>(maxIterations) * (szSize - 1));
                mPermutation.resize(szSize);
                mVisited.resize(szSize);
                mPVector.resize(szSize);
                mVVector.resize(szSize);
                mWVector.resize(szSize);
            }
        }

        // A copy of the NxN symmetric input is made internally.  The order of
        // the eigenvalues is specified by sortType: -1 (decreasing), 0 (no
        // sorting), or +1 (increasing).  When sorted, the eigenvectors are
        // ordered accordingly.  The return value is the number of iterations
        // consumed when convergence occurred, 0xFFFFFFFF when convergence did
        // not occur, or 0 when N <= 1 was passed to the constructor.
        uint32_t Solve(Real const* input, int32_t sortType)
        {
            mEigenvectorMatrixType = -1;

            if (mSize > 0)
            {
                std::copy(input, input + static_cast<size_t>(mSize) * static_cast<size_t>(mSize), mMatrix.begin());
                Tridiagonalize();

                mGivens.clear();
                for (uint32_t j = 0; j < mMaxIterations; ++j)
                {
                    int32_t imin = -1, imax = -1;
                    for (int32_t i = mSize - 2; i >= 0; --i)
                    {
                        // When a01 is much smaller than its diagonal
                        // neighbors, it is effectively zero.
                        Real a00 = mDiagonal[i];
                        Real a01 = mSuperdiagonal[i];
                        Real a11 = mDiagonal[static_cast<size_t>(i) + 1];
                        Real sum = std::fabs(a00) + std::fabs(a11);
                        if (sum + std::fabs(a01) != sum)
                        {
                            if (imax == -1)
                            {
                                imax = i;
                            }
                            imin = i;
                        }
                        else
                        {
                            // The superdiagonal term is effectively zero
                            // compared to the neighboring diagonal terms.
                            if (imin >= 0)
                            {
                                break;
                            }
                        }
                    }

                    if (imax == -1)
                    {
                        // The algorithm has converged.
                        ComputePermutation(sortType);
                        return j;
                    }

                    // Process the lower-right-most unreduced tridiagonal
                    // block.
                    DoQRImplicitShift(imin, imax);
                }
                return 0xFFFFFFFF;
            }
            else
            {
                return 0;
            }
        }

        // Get the eigenvalues of the matrix passed to Solve(...).  The input
        // 'eigenvalues' must have N elements.
        void GetEigenvalues(Real* eigenvalues) const
        {
            if (eigenvalues && mSize > 0)
            {
                if (mPermutation[0] >= 0)
                {
                    // Sorting was requested.
                    for (int32_t i = 0; i < mSize; ++i)
                    {
                        int32_t p = mPermutation[i];
                        eigenvalues[i] = mDiagonal[p];
                    }
                }
                else
                {
                    // Sorting was not requested.
                    size_t numBytes = mSize * sizeof(Real);
                    std::memcpy(eigenvalues, &mDiagonal[0], numBytes);
                }
            }
        }

        // Accumulate the Householder reflections and Givens rotations to
        // produce the orthogonal matrix Q for which Q^T*A*Q = D.  The input
        // 'eigenvectors' must have N*N elements.  The array is filled in as
        // if the eigenvector matrix is stored in row-major order.  The i-th
        // eigenvector is
        //   (eigenvectors[i+size*0], ... eigenvectors[i+size*(size - 1)])
        // which is the i-th column of 'eigenvectors' as an NxN matrix stored
        // in row-major order.
        void GetEigenvectors(Real* eigenvectors) const
        {
            mEigenvectorMatrixType = -1;

            if (eigenvectors && mSize > 0)
            {
                // Start with the identity matrix.
                std::fill(eigenvectors, eigenvectors + static_cast<size_t>(mSize) * static_cast<size_t>(mSize), (Real)0);
                for (int32_t d = 0; d < mSize; ++d)
                {
                    eigenvectors[d + mSize * d] = (Real)1;
                }

                // Multiply the Householder reflections using backward
                // accumulation.
                int32_t r, c;
                for (int32_t i = mSize - 3, rmin = i + 1; i >= 0; --i, --rmin)
                {
                    // Copy the v vector and 2/Dot(v,v) from the matrix.
                    Real const* column = &mMatrix[i];
                    Real twoinvvdv = column[mSize * (i + 1)];
                    for (r = 0; r < i + 1; ++r)
                    {
                        mVVector[r] = (Real)0;
                    }
                    mVVector[r] = (Real)1;
                    for (++r; r < mSize; ++r)
                    {
                        mVVector[r] = column[mSize * r];
                    }

                    // Compute the w vector.
                    for (r = 0; r < mSize; ++r)
                    {
                        mWVector[r] = (Real)0;
                        for (c = rmin; c < mSize; ++c)
                        {
                            mWVector[r] += mVVector[c] * eigenvectors[r + mSize * c];
                        }
                        mWVector[r] *= twoinvvdv;
                    }

                    // Update the matrix, Q <- Q - v*w^T.
                    for (r = rmin; r < mSize; ++r)
                    {
                        for (c = 0; c < mSize; ++c)
                        {
                            eigenvectors[c + mSize * r] -= mVVector[r] * mWVector[c];
                        }
                    }
                }

                // Multiply the Givens rotations.
                for (auto const& givens : mGivens)
                {
                    for (r = 0; r < mSize; ++r)
                    {
                        int32_t j = givens.index + mSize * r;
                        Real& q0 = eigenvectors[j];
                        Real& q1 = eigenvectors[j + 1];
                        Real prd0 = givens.cs * q0 - givens.sn * q1;
                        Real prd1 = givens.sn * q0 + givens.cs * q1;
                        q0 = prd0;
                        q1 = prd1;
                    }
                }

                // The number of Householder reflections is H = mSize - 2.  If
                // H is even, the product of Householder reflections is a
                // rotation; otherwise, H is odd and the product is a
                // reflection.  The number of Givens rotations does not
                // influence the type of the product of Householder
                // reflections.
                mEigenvectorMatrixType = 1 - (mSize & 1);

                if (mPermutation[0] >= 0)
                {
                    // Sorting was requested.
                    std::fill(mVisited.begin(), mVisited.end(), 0);
                    for (int32_t i = 0; i < mSize; ++i)
                    {
                        if (mVisited[i] == 0 && mPermutation[i] != i)
                        {
                            // The item starts a cycle with 2 or more
                            // elements.
                            int32_t start = i, current = i, j, next;
                            for (j = 0; j < mSize; ++j)
                            {
                                mPVector[j] = eigenvectors[i + mSize * j];
                            }
                            while ((next = mPermutation[current]) != start)
                            {
                                mEigenvectorMatrixType = 1 - mEigenvectorMatrixType;
                                mVisited[current] = 1;
                                for (j = 0; j < mSize; ++j)
                                {
                                    eigenvectors[current + mSize * j] =
                                        eigenvectors[next + mSize * j];
                                }
                                current = next;
                            }
                            mVisited[current] = 1;
                            for (j = 0; j < mSize; ++j)
                            {
                                eigenvectors[current + mSize * j] = mPVector[j];
                            }
                        }
                    }
                }
            }
        }

        // The eigenvector matrix is a rotation (return +1) or a reflection
        // (return 0).  If the input 'size' to the constructor is 0 or the
        // input 'eigenvectors' to GetEigenvectors is null, the returned value
        // is -1.
        inline int32_t GetEigenvectorMatrixType() const
        {
            return mEigenvectorMatrixType;
        }

        // Compute a single eigenvector, which amounts to computing column c
        // of matrix Q.  The reflections and rotations are applied
        // incrementally.  This is useful when you want only a small number of
        // the eigenvectors.
        void GetEigenvector(int32_t c, Real* eigenvector) const
        {
            if (0 <= c && c < mSize)
            {
                // y = H*x, then x and y are swapped for the next H
                Real* x = eigenvector;
                Real* y = &mPVector[0];

                // Start with the Euclidean basis vector.
                std::memset(x, 0, mSize * sizeof(Real));
                if (mPermutation[0] >= 0)
                {
                    // Sorting was requested.
                    x[mPermutation[c]] = (Real)1;
                }
                else
                {
                    x[c] = (Real)1;
                }

                // Apply the Givens rotations.
                for (auto const& givens : gte::reverse(mGivens))
                {
                    Real& xr = x[givens.index];
                    Real& xrp1 = x[givens.index + 1];
                    Real tmp0 = givens.cs * xr + givens.sn * xrp1;
                    Real tmp1 = -givens.sn * xr + givens.cs * xrp1;
                    xr = tmp0;
                    xrp1 = tmp1;
                }

                // Apply the Householder reflections.
                for (int32_t i = mSize - 3; i >= 0; --i)
                {
                    // Get the Householder vector v.
                    Real const* column = &mMatrix[i];
                    Real twoinvvdv = column[mSize * (i + 1)];
                    int32_t r;
                    for (r = 0; r <= i; ++r)
                    {
                        y[r] = x[r];
                    }

                    // Compute s = Dot(x,v) * 2/v^T*v.
                    //
                    // NOTE: MSVS 2019 16+ generates for Real=double, mSize=6:
                    //   warning C6385: Reading invalid data from 'x':  the
                    //   readable size is '_Old_3`mSize*sizeof(Real)' bytes,
                    //   but '16' bytes may be read.
                    // This appears to be incorrect. At this point in the
                    // code, r = i + 1. On i-loop entry, r = mSize-2. On
                    // i-loop exit, r = 1. This keeps r in the valid range
                    // for x[].
#if defined(GTE_USE_MSWINDOWS)
#pragma warning(disable : 6385)
#endif
                    Real s = x[r];  // r = i+1, v[i+1] = 1

                    // Note that on i-loop entry, r = mSize-2 in which
                    // case r+1 = mSize-1. This keeps index j in the valid
                    // range for x[].
                    for (int32_t j = r + 1; j < mSize; ++j)
                    {
                        s += x[j] * column[mSize * j];
                    }
                    s *= twoinvvdv;

                    y[r] = x[r] - s;  // v[i+1] = 1

                    // Compute the remaining components of y.
                    for (++r; r < mSize; ++r)
                    {
                        y[r] = x[r] - s * column[mSize * r];
                    }

                    std::swap(x, y);
#if defined(GTE_USE_MSWINDOWS)
#pragma warning(default : 6385)
#endif
                }

                // The final product is stored in x.
                if (x != eigenvector)
                {
                    size_t numBytes = mSize * sizeof(Real);
                    std::memcpy(eigenvector, x, numBytes);
                }
            }
        }

        Real GetEigenvalue(int32_t c) const
        {
            if (mSize > 0)
            {
                if (mPermutation[0] >= 0)
                {
                    // Sorting was requested.
                    return mDiagonal[mPermutation[c]];
                }
                else
                {
                    // Sorting was not requested.
                    return mDiagonal[c];
                }
            }
            else
            {
                return std::numeric_limits<Real>::max();
            }
        }

    private:
        // Tridiagonalize using Householder reflections.  On input, mMatrix is
        // a copy of the input matrix.  On output, the upper-triangular part
        // of mMatrix including the diagonal stores the tridiagonalization.
        // The lower-triangular part contains 2/Dot(v,v) that are used in
        // computing eigenvectors and the part below the subdiagonal stores
        // the essential parts of the Householder vectors v (the elements of
        // v after the leading 1-valued component).
        void Tridiagonalize()
        {
            int32_t r, c;
            for (int32_t i = 0, ip1 = 1; i < mSize - 2; ++i, ++ip1)
            {
                // Compute the Householder vector.  Read the initial vector
                // from the row of the matrix.
                Real length = (Real)0;
                for (r = 0; r < ip1; ++r)
                {
                    mVVector[r] = (Real)0;
                }
                for (r = ip1; r < mSize; ++r)
                {
                    Real vr = mMatrix[r + static_cast<size_t>(mSize) * i];
                    mVVector[r] = vr;
                    length += vr * vr;
                }
                Real vdv = (Real)1;
                length = std::sqrt(length);
                if (length > (Real)0)
                {
                    Real& v1 = mVVector[ip1];
                    Real sgn = (v1 >= (Real)0 ? (Real)1 : (Real)-1);
                    Real invDenom = ((Real)1) / (v1 + sgn * length);
                    v1 = (Real)1;
                    for (r = ip1 + 1; r < mSize; ++r)
                    {
                        Real& vr = mVVector[r];
                        vr *= invDenom;
                        vdv += vr * vr;
                    }
                }

                // Compute the rank-1 offsets v*w^T and w*v^T.
                Real invvdv = (Real)1 / vdv;
                Real twoinvvdv = invvdv * (Real)2;
                Real pdvtvdv = (Real)0;
                for (r = i; r < mSize; ++r)
                {
                    mPVector[r] = (Real)0;
                    for (c = i; c < r; ++c)
                    {
                        mPVector[r] += mMatrix[r + static_cast<size_t>(mSize) * c] * mVVector[c];
                    }
                    for (/**/; c < mSize; ++c)
                    {
                        mPVector[r] += mMatrix[c + static_cast<size_t>(mSize) * r] * mVVector[c];
                    }
                    mPVector[r] *= twoinvvdv;
                    pdvtvdv += mPVector[r] * mVVector[r];
                }

                pdvtvdv *= invvdv;
                for (r = i; r < mSize; ++r)
                {
                    mWVector[r] = mPVector[r] - pdvtvdv * mVVector[r];
                }

                // Update the input matrix.
                for (r = i; r < mSize; ++r)
                {
                    Real vr = mVVector[r];
                    Real wr = mWVector[r];
                    Real offset = vr * wr * (Real)2;
                    mMatrix[r + static_cast<size_t>(mSize) * r] -= offset;
                    for (c = r + 1; c < mSize; ++c)
                    {
                        offset = vr * mWVector[c] + wr * mVVector[c];
                        mMatrix[c + static_cast<size_t>(mSize) * r] -= offset;
                    }
                }

                // Copy the vector to column i of the matrix.  The 0-valued
                // components at indices 0 through i are not stored.  The
                // 1-valued component at index i+1 is also not stored;
                // instead, the quantity 2/Dot(v,v) is stored for use in
                // eigenvector construction. That construction must take
                // into account the implied components that are not stored.
                mMatrix[i + static_cast<size_t>(mSize) * ip1] = twoinvvdv;
                for (r = ip1 + 1; r < mSize; ++r)
                {
                    mMatrix[i + static_cast<size_t>(mSize) * r] = mVVector[r];
                }
            }

            // Copy the diagonal and subdiagonal entries for cache coherence
            // in the QR iterations.
            int32_t k, ksup = mSize - 1, index = 0, delta = mSize + 1;
            for (k = 0; k < ksup; ++k, index += delta)
            {
                mDiagonal[k] = mMatrix[index];
                mSuperdiagonal[k] = mMatrix[static_cast<size_t>(index) + 1];
            }
            mDiagonal[k] = mMatrix[index];
        }

        // A helper for generating Givens rotation sine and cosine robustly.
        void GetSinCos(Real x, Real y, Real& cs, Real& sn)
        {
            // Solves sn*x + cs*y = 0 robustly.
            Real tau;
            if (y != (Real)0)
            {
                if (std::fabs(y) > std::fabs(x))
                {
                    tau = -x / y;
                    sn = (Real)1 / std::sqrt((Real)1 + tau * tau);
                    cs = sn * tau;
                }
                else
                {
                    tau = -y / x;
                    cs = (Real)1 / std::sqrt((Real)1 + tau * tau);
                    sn = cs * tau;
                }
            }
            else
            {
                cs = (Real)1;
                sn = (Real)0;
            }
        }

        // The QR step with implicit shift.  Generally, the initial T is
        // unreduced tridiagonal (all subdiagonal entries are nonzero).  If a
        // QR step causes a superdiagonal entry to become zero, the matrix
        // decouples into a block diagonal matrix with two tridiagonal blocks.
        // These blocks can be reduced independently of each other, which
        // allows for parallelization of the algorithm.  The inputs imin and
        // imax identify the subblock of T to be processed.   That block has
        // upper-left element T(imin,imin) and lower-right element
        // T(imax,imax).
        void DoQRImplicitShift(int32_t imin, int32_t imax)
        {
            // The implicit shift.  Compute the eigenvalue u of the
            // lower-right 2x2 block that is closer to a11.
            Real a00 = mDiagonal[imax];
            Real a01 = mSuperdiagonal[imax];
            Real a11 = mDiagonal[static_cast<size_t>(imax) + 1];
            Real dif = (a00 - a11) * (Real)0.5;
            Real sgn = (dif >= (Real)0 ? (Real)1 : (Real)-1);
            Real a01sqr = a01 * a01;
            Real u = a11 - a01sqr / (dif + sgn * std::sqrt(dif * dif + a01sqr));
            Real x = mDiagonal[imin] - u;
            Real y = mSuperdiagonal[imin];

            Real a12, a22, a23, tmp11, tmp12, tmp21, tmp22, cs, sn;
            Real a02 = (Real)0;
            int32_t i0 = imin - 1, i1 = imin, i2 = imin + 1;
            for (/**/; i1 <= imax; ++i0, ++i1, ++i2)
            {
                // Compute the Givens rotation and save it for use in
                // computing the eigenvectors.
                GetSinCos(x, y, cs, sn);
                mGivens.push_back(GivensRotation(i1, cs, sn));

                // Update the tridiagonal matrix.  This amounts to updating a
                // 4x4 subblock,
                //   b00 b01 b02 b03
                //   b01 b11 b12 b13
                //   b02 b12 b22 b23
                //   b03 b13 b23 b33
                // The four corners (b00, b03, b33) do not change values.  The
                // The interior block {{b11,b12},{b12,b22}} is updated on each
                // pass.  For the first pass, the b0c values are out of range,
                // so only the values (b13, b23) change.  For the last pass,
                // the br3 values are out of range, so only the values
                // (b01, b02) change.  For passes between first and last, the
                // values (b01, b02, b13, b23) change.
                if (i1 > imin)
                {
                    mSuperdiagonal[i0] = cs * mSuperdiagonal[i0] - sn * a02;
                }

                a11 = mDiagonal[i1];
                a12 = mSuperdiagonal[i1];
                a22 = mDiagonal[i2];
                tmp11 = cs * a11 - sn * a12;
                tmp12 = cs * a12 - sn * a22;
                tmp21 = sn * a11 + cs * a12;
                tmp22 = sn * a12 + cs * a22;
                mDiagonal[i1] = cs * tmp11 - sn * tmp12;
                mSuperdiagonal[i1] = sn * tmp11 + cs * tmp12;
                mDiagonal[i2] = sn * tmp21 + cs * tmp22;

                if (i1 < imax)
                {
                    a23 = mSuperdiagonal[i2];
                    a02 = -sn * a23;
                    mSuperdiagonal[i2] = cs * a23;

                    // Update the parameters for the next Givens rotation.
                    x = mSuperdiagonal[i1];
                    y = a02;
                }
            }
        }

        // Sort the eigenvalues and compute the corresponding permutation of
        // the indices of the array storing the eigenvalues.  The permutation
        // is used for reordering the eigenvalues and eigenvectors in the
        // calls to GetEigenvalues(...) and GetEigenvectors(...).
        void ComputePermutation(int32_t sortType)
        {
            // The number of Householder reflections is H = mSize - 2.  If H
            // is even, the product of Householder reflections is a rotation;
            // otherwise, H is odd and the product is a reflection.  The
            // number of Givens rotations does not influence the type of the
            // product of Householder reflections.
            mEigenvectorMatrixType = 1 - (mSize & 1);

            if (sortType == 0)
            {
                // Set a flag for GetEigenvalues() and GetEigenvectors() to
                // know that sorted output was not requested.
                mPermutation[0] = -1;
                return;
            }

            // Compute the permutation induced by sorting.  Initially, we
            // start with the identity permutation I = (0,1,...,N-1).
            struct SortItem
            {
                SortItem()
                    :
                    eigenvalue((Real)0),
                    index(0)
                {
                }

                Real eigenvalue;
                int32_t index;
            };

            std::vector<SortItem> items(mSize);
            int32_t i;
            for (i = 0; i < mSize; ++i)
            {
                items[i].eigenvalue = mDiagonal[i];
                items[i].index = i;
            }

            if (sortType > 0)
            {
                std::sort(items.begin(), items.end(),
                    [](SortItem const& item0, SortItem const& item1)
                    {
                        return item0.eigenvalue < item1.eigenvalue;
                    }
                );
            }
            else
            {
                std::sort(items.begin(), items.end(),
                    [](SortItem const& item0, SortItem const& item1)
                    {
                        return item0.eigenvalue > item1.eigenvalue;
                    }
                );
            }

            i = 0;
            for (auto const& item : items)
            {
                mPermutation[i++] = item.index;
            }

            // GetEigenvectors() has nontrivial code for computing the
            // orthogonal Q from the reflections and rotations.  To avoid
            // complicating the code further when sorting is requested, Q is
            // computed as in the unsorted case.  We then need to swap columns
            // of Q to be consistent with the sorting of the eigenvalues.  To
            // minimize copying due to column swaps, we use permutation P.
            // The minimum number of transpositions to obtain P from I is N
            // minus the number of cycles of P.  Each cycle is reordered with
            // a minimum number of transpositions; that is, the eigenitems are
            // cyclically swapped, leading to a minimum amount of copying.
            // For/ example, if there is a cycle i0 -> i1 -> i2 -> i3, then
            // the copying is
            //   save = eigenitem[i0];
            //   eigenitem[i1] = eigenitem[i2];
            //   eigenitem[i2] = eigenitem[i3];
            //   eigenitem[i3] = save;
        }

        // The number N of rows and columns of the matrices to be processed.
        int32_t mSize;

        // The maximum number of iterations for reducing the tridiagonal
        // matrix to a diagonal matrix.
        uint32_t mMaxIterations;

        // The internal copy of a matrix passed to the solver.  See the
        // comments about function Tridiagonalize() about what is stored in
        // the matrix.
        std::vector<Real> mMatrix;  // NxN elements

        // After the initial tridiagonalization by Householder reflections, we
        // no longer need the full mMatrix.  Copy the diagonal and
        // superdiagonal entries to linear arrays in order to be cache
        // friendly.
        std::vector<Real> mDiagonal;  // N elements
        std::vector<Real> mSuperdiagonal;  // N-1 elements

        // The Givens rotations used to reduce the initial tridiagonal matrix
        // to a diagonal matrix.  A rotation is the identity with the
        // following replacement entries:  R(index,index) = cs,
        // R(index,index+1) = sn, R(index+1,index) = -sn and
        // R(index+1,index+1) = cs.  If N is the matrix size and K is the
        // maximum number of iterations, the maximum number of Givens
        // rotations is K*(N-1).  The maximum amount of memory is allocated
        // to store these.
        struct GivensRotation
        {
            // No default initialization for fast creation of std::vector
            // of objects of this type.
            GivensRotation() = default;

            GivensRotation(int32_t inIndex, Real inCs, Real inSn)
                :
                index(inIndex),
                cs(inCs),
                sn(inSn)
            {
            }

            int32_t index;
            Real cs, sn;
        };

        std::vector<GivensRotation> mGivens;  // K*(N-1) elements

        // When sorting is requested, the permutation associated with the sort
        // is stored in mPermutation.  When sorting is not requested,
        // mPermutation[0] is set to -1.  mVisited is used for finding cycles
        // in the permutation. mEigenvectorMatrixType is +1 if GetEigenvectors
        // returns a rotation matrix, 0 if GetEigenvectors returns a
        // reflection matrix or -1 if an input to the constructor or to
        // GetEigenvectors is invalid.
        std::vector<int32_t> mPermutation;  // N elements
        mutable std::vector<int32_t> mVisited;  // N elements
        mutable int32_t mEigenvectorMatrixType;

        // Temporary storage to compute Householder reflections and to
        // support sorting of eigenvectors.
        mutable std::vector<Real> mPVector;  // N elements
        mutable std::vector<Real> mVVector;  // N elements
        mutable std::vector<Real> mWVector;  // N elements
    };
}
