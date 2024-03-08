// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2024
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
// https://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// Version: 6.9.2023.12.02

#include "IntersectLineRectangleWindow3.h"
#include <iostream>

int main()
{
    try
    {
        Window::Parameters parameters(L"IntersectLineRectangleWindow3", 0, 0, 768, 768);
        auto window = TheWindowSystem.Create<IntersectLineRectangleWindow3>(parameters);
        TheWindowSystem.MessagePump(window, TheWindowSystem.DEFAULT_ACTION);
        TheWindowSystem.Destroy(window);
    }
    catch (std::exception const& e)
    {
        std::cout << e.what() << std::endl;
    }
    return 0;
}
