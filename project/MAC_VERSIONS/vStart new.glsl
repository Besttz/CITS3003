attribute vec3 vPosition;
attribute vec3 vNormal;
attribute vec2 vTexCoord;

varying vec2 texCoord;
varying vec3 fL1;
varying vec3 fL2;
varying vec4 normal;
varying vec3 pos;

uniform mat4 ModelView;
uniform mat4 Projection;




void main()
{
    vec4 vpos =vec4(vPosition, 1.0);
    // Transform vertex position into eye coordinates
    pos = (ModelView * vpos).xyz;
    normal = vec4(vNormal, 0.0);
    //Calculate the final position
    gl_Position = Projection * ModelView * vpos;
    
    texCoord = vTexCoord;
}
