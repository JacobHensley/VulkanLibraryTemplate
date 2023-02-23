#Shader Vertex
#version 450

// Attributes
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_Tangent;

// Vertex Out
layout(location = 0) out vec3 v_Normal;
layout(location = 1) out vec2 v_TexCoord;

// Push Constants
layout(push_constant) uniform constants
{
    mat4 Transform;
} PushConstants;

// Uniforms
layout(set = 0, binding = 0) uniform CameraBuffer
{
    mat4 ViewProjection;
} u_CameraBuffer;

void main() 
{
    gl_Position = u_CameraBuffer.ViewProjection * PushConstants.Transform * vec4(a_Position, 1.0);

    v_Normal = a_Normal;
    v_TexCoord = a_TexCoord;
}

#Shader Fragment
#version 450

// Fragment In
layout(location = 0) in vec3 v_Normal;
layout(location = 1) in vec2 v_TexCoord;

// Fragment Out
layout(location = 0) out vec4 outColor;

// Uniforms
layout(set = 0, binding = 1) uniform sampler2D u_AlbedoTexture;
layout(set = 0, binding = 2) uniform sampler2D u_MetallicRoughnessTexture;

void main() 
{
    outColor.rgb = texture(u_AlbedoTexture, v_TexCoord).rgb;
    outColor.a = 1.0;
}