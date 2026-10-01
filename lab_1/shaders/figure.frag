#version 450
layout(location=0) in vec3 vertexColor;
layout(location=1) in vec3 worldPosition;
layout(location=0) out vec4 outColor;
layout(push_constant) uniform Mode { int edge; } mode;
void main() {
    if(mode.edge==1) { outColor=vec4(0.80,0.91,1.0,1.0); return; }
    vec3 n=normalize(cross(dFdx(worldPosition),dFdy(worldPosition)));
    float light=0.65+0.35*abs(dot(n,normalize(vec3(0.4,0.8,0.6))));
    outColor=vec4(vertexColor*light,1.0);
}
