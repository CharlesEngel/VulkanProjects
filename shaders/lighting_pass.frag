#version 450

layout (location = 0) out vec4 outFragColor;

layout (location = 0) flat in int index;

layout (set = 0, binding = 3) uniform sampler2D pos;
layout (set = 0, binding = 4) uniform sampler2D norm;
layout (set = 0, binding = 5) uniform sampler2D albedo_specular;
layout (set = 0, binding = 6) uniform sampler2D depth;

struct LightData
{
	vec4 pos_r;
	vec4 color;
};

layout(std140, set = 0, binding = 2) readonly buffer LightBuffer
{
	LightData lights[];
} lightBuffer;

const float INVPI = 0.318309886;

vec3 fresnel(vec3 f0, vec3 n, vec3 v)
{
	return f0 + (vec3(1.0f) - f0) * pow(1.0f - max(dot(n, v), 0.0f), 5.0f);
}

float NDF(vec3 n, vec3 m, float alpha)
{
	float alpha2 = alpha * alpha;
	float dnm = dot(n, m);
	float num = step(0.0f, dnm) * alpha2;
	float denom = 3.141592 * pow(1.0f + dnm * dnm * (alpha2 - 1), 2);

	return num / denom;
}

float mask(vec3 n, vec3 l, vec3 v, float alpha)
{
	float num = 0.5;
	float absnl = abs(dot(n, l));
	float absnv = abs(dot(n, v));
	float denom = mix(2 * absnl * absnv, absnl + absnv, alpha);
	return num / denom;
}

void main()
{
	vec2 texCoord = gl_FragCoord.xy / textureSize(pos, 0);
	vec4 norm_metal = texture(norm, texCoord);
	vec4 pss_r = texture(albedo_specular, texCoord);
	vec3 position = texture(pos, texCoord).xyz;

	vec3 normal = normalize(norm_metal.xyz);
	float metalness = norm_metal.w;
	float r = pss_r.w;
	vec3 pss = pss_r.rgb;
	vec3 f0 = mix(vec3(0.07f), pss_r.rgb, metalness);

	float alpha = r * r;
	vec3 view = normalize(-position);

	vec3 lightDir = lightBuffer.lights[index].pos_r.xyz - position;
	float dist = length(lightDir);
	lightDir = lightDir / dist;
	float light_radius = lightBuffer.lights[index].pos_r.w;
	float atten = clamp(1.0f - (dist * dist) / (light_radius * light_radius), 0.0f, 1.0f);
	atten *= atten;
	vec3 radiance = lightBuffer.lights[index].color.xyz * atten;
	float nl = max(dot(normal, normalize(lightDir)), 0.0);

	vec3 h = normalize(view + lightDir);
	vec3 F = fresnel(f0, h, view);
	float D = NDF(normal, h, alpha);
	float G = mask(normal, lightDir, view, alpha);
	vec3 spec = F * D * G;

	vec3 final_color = (pss * (vec3(1.0f) - F) * (1.0f - metalness) * INVPI + spec) * radiance * nl;
	final_color = final_color / (final_color + vec3(1.0));
	//final_color = pow(final_color, vec3(1.0f/2.2f));
	outFragColor = vec4(final_color, 1.0f);
}
