static const float NUM_SAMPLES = 30;
static const float PI = 3.1415926f;
static const float PI2 = PI * PI;

struct PossionDiskSampleGenerator
{
    float2 poissonDisk[NUM_SAMPLES] = {};

    __init()
    {
      poissonDiskSamples(float2(0.233f, 0.213f));
    }

    static float rand_1to1(float x)
    {
        // -1 -1
        return frac(sin(x) * 10000.0);
    }

    static float rand_2to1(float2 uv)
    {
        // 0 - 1
        float a = 12.9898, b = 78.233, c = 43758.5453;
        float dt = dot(uv.xy, float2(a, b)), sn = fmod(dt, PI);
        return frac(sin(sn) * c);
    }

    static float unpack(float4 rgbaDepth)
    {
        float4 bitShift = float4(1.0, 1.0 / 255.0, 1.0 / (255.0 * 255.0), 1.0 / (255.0 * 255.0 * 255.0));
        return dot(rgbaDepth, bitShift);
    }

    [mutating]
    void poissonDiskSamples(const float2 randomSeed)
    {
        float ANGLE_STEP = 3.883222077450933;// (sqrt(5)-1)/2 *2PI
        float INV_NUM_SAMPLES = 1.0 / float(NUM_SAMPLES);
        float angle = rand_2to1(randomSeed) * PI2;
        float radius = INV_NUM_SAMPLES;
        float radiusStep = radius;
        for (int i = 0; i < NUM_SAMPLES; i++)
        {
            poissonDisk[i] = float2(cos(angle), sin(angle)) * pow(radius, 0.75);
            radius += radiusStep;
            angle += ANGLE_STEP;
        }
    }

    float2 getSample(int index)
    {
        return poissonDisk[index];
    }
};

