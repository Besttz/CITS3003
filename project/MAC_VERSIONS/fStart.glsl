varying vec2 texCoord;  // The third coordinate is always 0.0 and is discarded
varying vec3 pos;
varying vec4 normal;

vec4 color;


uniform sampler2D texture;
uniform vec3 AmbientProduct, DiffuseProduct, SpecularProduct;
uniform mat4 ModelView;
uniform float Shininess;
uniform float texScale;
uniform vec4 LightPosition;
uniform vec4 LightPosition2;
uniform vec3 Light1Rgb;
uniform vec3 Light2Rgb;


void main()
{
    //Vectors for the light direction
    vec3 fL1 = LightPosition.xyz - pos;
    vec3 fL2 = LightPosition2.xyz;     

    // Transform vertex normal into eye coordinates (assumes scaling
    // is uniform across dimensions)
    vec3 N = normalize((ModelView * normal).xyz); 

    // Unit direction vectors for Blinn-Phong shading calculation
    vec3 E = normalize(- pos);  // Direction to the eye/camera
    vec3 L1 = normalize(fL1);// Direction to the light source
    vec3 L2 = normalize(fL2);// Direction to the 2nd light source
    vec3 H1 = normalize( L1 + E );  // Halfway vector
    vec3 H2 = normalize( L2 + E );  // Halfway vector

    // Compute terms in the illumination equation
    vec4 ambient1 = vec4(Light1Rgb*AmbientProduct,1.0);
    vec4 ambient2 = vec4(Light2Rgb*AmbientProduct,1.0);


    float Kd1 = max( dot(L1, N), 0.0 );
    float Kd2 = max( dot(L2, N), 0.0 );

    vec4  diffuse1 = Kd1*vec4(Light1Rgb*DiffuseProduct,1.0);
    vec4  diffuse2 = Kd2*vec4(Light2Rgb*DiffuseProduct,1.0);


    float Ks1 = pow( max(dot(N, H1), 0.0), Shininess );
    float Ks2 = pow( max(dot(N, H2), 0.0), Shininess );

    vec4 specular1 = Ks1 * vec4(Light1Rgb*SpecularProduct,1.0); 
    vec4 specular2 = Ks2 * vec4(Light2Rgb*SpecularProduct,1.0);

    
    if (dot(L1, N) < 0.0 ) {
	specular1 = vec4(0.0, 0.0, 0.0,1.0); 
    } 
    if (dot(L2, N) < 0.0 ) {
	specular2 = vec4(0.0, 0.0, 0.0,1.0); 
    } 

    // globalAmbient is independent of distance from the light source
    vec3 globalAmbient = vec3(0.1, 0.1, 0.1);

    // The length to the light from the vertex 
    float Ldis = length(fL1);

    //Calculate the color
    color.rgb = globalAmbient + ambient1.xyz + diffuse1.xyz
    *(1.0/(1.0+Ldis*Ldis))+ambient2.xyz+diffuse2.xyz;
    color.a = 1.0;

    gl_FragColor = color * texture2D( texture, texCoord *2.0* texScale )
    + specular1*(1.0/(1.0+Ldis*Ldis))+ specular2;
}
