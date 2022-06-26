#pragma once
#include "Utils/HostDeviceShared.slangh"

BEGIN_NAMESPACE_FALCOR

struct SSSParams
{
    float4x4 InverseScreenAndProj;
    float uScale;
    float vScale;
    float d;
};

END_NAMESPACE_FALCOR
