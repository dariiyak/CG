#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 color;
layout(binding=0) uniform Scene { mat4 model; mat4 view; mat4 projection; vec4 tint; } scene;
layout(location=0) out vec3 vertexColor;
layout(location=1) out vec3 worldPosition;
void main() {
    vec4 world=scene.model*vec4(position,1.0);
    gl_Position=scene.projection*scene.view*world;
    worldPosition=world.xyz;
    vertexColor=mix(vec3(1.0),color,scene.tint.w)*scene.tint.rgb;
}
