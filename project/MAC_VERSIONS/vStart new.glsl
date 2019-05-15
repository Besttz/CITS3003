attribute vec3 vPosition;
attribute vec3 vNormal;
attribute vec2 vTexCoord;

varying vec2 texCoord;
varying vec3 Lvec;
varying vec3 Lvec2;

varying vec3 normal;
varying vec3 pos;

uniform mat4 ModelView;
uniform mat4 Projection;
uniform vec4 LightPosition;
uniform vec4 LightPosition2;


//part 2
// attribute vec4 boneIDs;
// attribute vec4 boneWeights;
// uniform mat4 boneTransforms[64];


void main()
{
    vec4 vpos = vec4(vPosition, 1.0);

    // Transform vertex position into eye coordinates
    pos = (ModelView * vpos).xyz;

    // The vector to the light from the vertex    
    Lvec = LightPosition.xyz - pos;
    Lvec2 =LightPosition2.xyz - pos;
    normal= vNormal;

    gl_Position = Projection * ModelView * vpos;
    texCoord = vTexCoord;
}
