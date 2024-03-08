// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2024
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
// https://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// Version: 6.0.2022.01.06

#include <Graphics/GTGraphicsPCH.h>
#include <Graphics/Texture1Array.h>
using namespace gte;

Texture1Array::Texture1Array(uint32_t numItems, uint32_t format,
    uint32_t length, bool hasMipmaps, bool createStorage)
    :
    TextureArray(numItems, format, 1, length, 1, 1, hasMipmaps, createStorage)
{
    mType = GT_TEXTURE1_ARRAY;
}
