// ShaderSources.h - GLSL кодът на шейдърите, вграден като низове.
//
// Вграждането избягва проблеми с пътища до файлове при стартиране от
// Visual Studio или от друга директория.
#pragma once

namespace ShaderSrc {

// ---------------------------------------------------------------------------
//  Основен шейдър: Blinn-Phong осветление, сенки, прожектор, мъгла.
// ---------------------------------------------------------------------------
inline const char* sceneVS = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;        // моделна матрица (локални -> световни координати)
uniform mat3 uNormalMat;    // (M^-1)^T - правилна трансформация на нормалите
uniform mat4 uViewProj;     // P * V
uniform mat4 uLightSpace;   // матрица на слънцето (за картата на сенките)

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out vec4 vLightSpacePos;

void main()
{
    vec4 world = uModel * vec4(aPos, 1.0);
    vWorldPos = world.xyz;
    vNormal = uNormalMat * aNormal;
    vUV = aUV;
    vLightSpacePos = uLightSpace * world;
    gl_Position = uViewProj * world;
}
)GLSL";

inline const char* sceneFS = R"GLSL(
#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
in vec4 vLightSpacePos;
out vec4 FragColor;

// Материал
uniform sampler2D uTex;
uniform vec3  uTint;
uniform vec2  uUvScale;
uniform float uSpecular;
uniform float uShininess;
uniform vec3  uEmissive;
uniform float uAlpha;

// Сцена и светлини
uniform vec3 uCamPos;
uniform vec3 uSunDir;        // единичен вектор КЪМ слънцето
uniform vec3 uSunColor;
uniform vec3 uSkyAmbient;    // околна светлина отгоре (небе)
uniform vec3 uGroundAmbient; // околна светлина отдолу (отразена от земята)
uniform sampler2DShadow uShadowMap;

uniform int   uSpotOn;
uniform vec3  uSpotPos;
uniform vec3  uSpotDir;
uniform vec3  uSpotColor;
uniform float uSpotCosInner;
uniform float uSpotCosOuter;

uniform vec3  uFogColor;
uniform float uFogDensity;

// Процент на осветеност от слънцето (1 = осветено, 0 = в сянка).
// PCF: 3x3 проби около точката; sampler2DShadow връща вече сравнения и
// билинейно филтриран резултат за всяка проба -> меки ръбове на сянката.
float sunVisibility(vec3 N)
{
    vec3 p = vLightSpacePos.xyz / vLightSpacePos.w;   // NDC [-1,1]
    p = p * 0.5 + 0.5;                                // -> [0,1]
    if (p.z > 1.0) return 1.0;
    // Наклонено отместване (slope-scaled bias) срещу "shadow acne".
    float bias = max(0.0025 * (1.0 - dot(N, uSunDir)), 0.0006);
    vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));
    float sum = 0.0;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            sum += texture(uShadowMap, vec3(p.xy + vec2(x, y) * texel, p.z - bias));
    return sum / 9.0;
}

void main()
{
    vec4 tex = texture(uTex, vUV * uUvScale);
    vec3 albedo = tex.rgb * uTint;

    vec3 N = normalize(vNormal);
    if (!gl_FrontFacing) N = -N;          // двустранни повърхности (дискове)
    vec3 V = normalize(uCamPos - vWorldPos);

    // 1) Полусферична околна светлина: смес небе/земя според посоката на N.
    vec3 color = mix(uGroundAmbient, uSkyAmbient, 0.5 + 0.5 * N.y) * albedo;

    // 2) Слънце: дифузна (Ламберт) + огледална (Blinn-Phong) компонента.
    float NdotL = max(dot(N, uSunDir), 0.0);
    vec3  H = normalize(uSunDir + V);
    float spec = NdotL > 0.0 ? pow(max(dot(N, H), 0.0), uShininess) * uSpecular : 0.0;
    color += sunVisibility(N) * uSunColor * (albedo * NdotL + spec);

    // 3) Прожектор: конус с плавен ръб и затихване с разстоянието.
    if (uSpotOn == 1) {
        vec3  Ls = uSpotPos - vWorldPos;
        float d = length(Ls);
        Ls /= d;
        float cone = smoothstep(uSpotCosOuter, uSpotCosInner, dot(-Ls, uSpotDir));
        float att = 1.0 / (1.0 + 0.045 * d + 0.0075 * d * d);
        float nl = max(dot(N, Ls), 0.0);
        vec3  Hs = normalize(Ls + V);
        float ss = pow(max(dot(N, Hs), 0.0), uShininess) * uSpecular;
        color += uSpotColor * cone * att * (albedo * nl + ss);
    }

    // 4) Собствено светене.
    color += uEmissive;

    // 5) Експоненциална квадратична мъгла - отдалечените обекти избледняват
    //    към цвета на хоризонта (въздушна перспектива).
    float dist = length(uCamPos - vWorldPos);
    float fog = 1.0 - exp(-pow(dist * uFogDensity, 2.0));
    color = mix(color, uFogColor, fog);

    // 6) Тонално картографиране (HDR -> [0,1]) и гама корекция.
    color = vec3(1.0) - exp(-color);
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, tex.a * uAlpha);
}
)GLSL";

// ---------------------------------------------------------------------------
//  Шейдър за картата на сенките - записва само дълбочина от гледна точка
//  на слънцето.
// ---------------------------------------------------------------------------
inline const char* depthVS = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uModel;
uniform mat4 uLightSpace;
void main() { gl_Position = uLightSpace * uModel * vec4(aPos, 1.0); }
)GLSL";

inline const char* depthFS = R"GLSL(
#version 330 core
void main() { }   // дълбочината се записва автоматично
)GLSL";

// ---------------------------------------------------------------------------
//  Небе: един триъгълник, покриващ целия екран. За всеки пиксел се
//  възстановява посоката на лъча и се изчислява градиент + слънчев диск.
// ---------------------------------------------------------------------------
inline const char* skyVS = R"GLSL(
#version 330 core
out vec2 vNdc;
void main()
{
    // Върхове (-1,-1), (3,-1), (-1,3) - без нито един буфер.
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0;
    vNdc = p;
    gl_Position = vec4(p, 0.0, 1.0);
}
)GLSL";

inline const char* skyFS = R"GLSL(
#version 330 core
in vec2 vNdc;
out vec4 FragColor;
uniform mat4 uInvViewProj;   // обратна на P * V (без транслацията на V)
uniform vec3 uSunDir;
uniform float uSunIntensity;
uniform vec3 uZenith;
uniform vec3 uHorizon;
uniform vec3 uGroundFar;
void main()
{
    vec4 w = uInvViewProj * vec4(vNdc, 1.0, 1.0);
    vec3 dir = normalize(w.xyz / w.w);

    float h = dir.y;
    vec3 sky = mix(uHorizon, uZenith, pow(max(h, 0.0), 0.55));
    if (h < 0.0) sky = mix(uHorizon, uGroundFar, min(-h * 6.0, 1.0));

    float s = max(dot(dir, uSunDir), 0.0);
    sky += vec3(1.0, 0.9, 0.7) * (pow(s, 900.0) * 30.0 + pow(s, 12.0) * 0.35) * uSunIntensity;

    sky = vec3(1.0) - exp(-sky);
    FragColor = vec4(pow(sky, vec3(1.0 / 2.2)), 1.0);
}
)GLSL";

} // namespace ShaderSrc
