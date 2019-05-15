varying vec2 texCoord;  // The third coordinate is always 0.0 and is discarded
varying vec3 pos;
varying vec3 normal;
varying vec3 Lvec;
varying vec3 Lvec2;



vec4 color;


uniform sampler2D texture;
uniform vec3 AmbientProduct, DiffuseProduct, SpecularProduct;
uniform mat4 ModelView;
uniform vec4 LightPosition;
uniform float Shininess;
uniform float texScale;


void main()
{
    // Transfer from vStart

    // Unit direction vectors for Blinn-Phong shading calculation
    vec3 L = normalize( Lvec );   // Direction to the light source
    vec3 L2 = normalize( Lvec2 );   // Direction to the 2nd light source

    vec3 E = normalize( -pos );   // Direction to the eye/camera
    vec3 H = normalize( L + E );  // Halfway vector
    vec3 H2 = normalize( L2 + E );  // Halfway vector


    // The length to the light from the vertex 
    float Ldis = length(Lvec);
    float Ldis2 = length(Lvec2);


    // Transform vertex normal into eye coordinates (assumes scaling
    // is uniform across dimensions)
    vec3 N = normalize( (ModelView*vec4(normal, 0.0)).xyz );

    // Compute terms in the illumination equation
    vec4 ambient = vec4(AmbientProduct,1.0);

    float Kd = max( dot(L, N), 0.0 );
    float Kd2 = max( dot(L2, N), 0.0 );

    vec4  diffuse = Kd*vec4(DiffuseProduct,1.0);
    vec4  diffuse2 = Kd2*vec4(DiffuseProduct,1.0);

    //+Kd2*vec4(DiffuseProduct,1.0)

    float Ks = pow( max(dot(N, H), 0.0), Shininess );
    float Ks2 = pow( max(dot(N, H2), 0.0), Shininess );

    // vec4  specular = Ks * vec4(SpecularProduct,1.0); // FOR G
    vec4  specular = Ks * vec4(1,1,1,1.0); // FOR H +
    vec4 specular2 = Ks2 * vec4(1,1,1,1.0);

    
    if (dot(L, N) < 0.0 ) {
	specular = vec4(0.0, 0.0, 0.0,1.0); //???
    } 
    if (dot(L2, N) < 0.0 ) {
	specular2 = vec4(0.0, 0.0, 0.0,1.0); //???
    } 

    // globalAmbient is independent of distance from the light source
    vec3 globalAmbient = vec3(0.1, 0.1, 0.1);

    color.rgb = globalAmbient + ambient.xyz + (diffuse.xyz+ specular.xyz)*(1.0/Ldis)
    +ambient.xyz+diffuse2.xyz+ specular2.xyz    ;
    color.a = 1.0;

    gl_FragColor = color * texture2D( texture, texCoord * texScale );
}
