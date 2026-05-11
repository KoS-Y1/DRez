module Utils;

__include "Utils/utils_attribute.slang";
__include "Utils/utils_general.slang";
__include "Utils/utils_pbr.slang";
__include "Utils/utils_shadow.slang";

public static const float PI = 3.14159265359f;
public static const float LN2 = 0.6931471805599453;
public static const float kSmallValue = 1e-4f;

public static const float3 kWorldUp = float3(0.0f, 1.0f, 0.0f);