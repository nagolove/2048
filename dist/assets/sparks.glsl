// Screen Welding 0.6.230524 by QuantumSuper
// pseudorandom particles leaving traces on a buffer
// 

/*
void mainImage( out vec4 col, in vec2 fC){
    col = getDat( iChannel0, fC);
    col.rgb *= .5 + .5*abs( cos( .06*iTime + PI/vec3(.5,2.,4.) - PI/3.)); 
}
*/

#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
//uniform sampler2D texture0;
//uniform sampler2D mask;
//uniform int frame;

// Output fragment color
out vec4 finalColor;

#define PI 3.14159265359

#define getDat(buf,addr) texelFetch( buf, ivec2(addr), 0)

// BUFFER A (0.61) of Screen Welding by QuantumSuper
// draw points on lots of different parabolas & use unclamped buffer as heatmap history
// 

#define numOfParts 50.
#define aTime iTime/34.

float hash21(vec2 p){ //pseudorandom generator, see The Art of Code on youtu.be/rvDo9LvfoVE
    p = fract(p*vec2(13.81, 741.76));
    p += dot(p, p+42.23);
    return fract(p.x*p.y);
}

vec2 parametricParabola(float t, vec2 seed){
    float d = .1 + 3.*hash21(.678*seed.yx); //y-stretch
    float c = sign(t) * (.01 + 2.*hash21(.987*seed)); //maximum shift
    float b = abs(c) + hash21(.285*seed) + .001; //x-stretch
    float a = c*c/b/b; //origin height
    t -= c/b;
    return vec2( b*t+c, (a-t*t)*d);
}

float lightUp(float dist, vec2 modif){ //light around dist=0
    return 6.*smoothstep(.025*modif.x, .0, dist)+clamp(.00008/dist/dist,.0,1.)+(1.+modif.y)*.0001/dist; 
    //combined semi-hard shape with semi-soft & soft glow
}


void mainImage( out vec4 fragColor, in vec2 fragCoord){

    // View definition
    vec2 uv = 6. * (2.*fragCoord-iResolution.xy) / max(iResolution.x, iResolution.y); //long edge -3 to 3, square aspect ratio       
    uv += 0.01; //shape definitions

    // Draw particles
    float mySpeed, 
          myTime;
    vec2 myMod,
         seed = vec2(iTime);
    vec3 myColor,
         col = vec3(0);   
    
    for (float n=0.;n++<numOfParts;){
        myTime = iTime/2. + n/numOfParts;
        seed = vec2(ceil(myTime)*.123,ceil(myTime)*.456);
        mySpeed = sign(.5-hash21(seed+.123*n)) * (1.5+2.5*hash21(seed*n*.456));
        myMod = vec2( hash21(seed/n*.123), 5.+25.*hash21(seed/n*.456));
        myColor = fract(-myTime) * vec3( 1.+fract(myTime), .5+.6*fract(-myTime), .2+fract(-myTime)*fract(-myTime));
        col += myColor * lightUp( length( uv - parametricParabola( .7*fract(myTime)*mySpeed, seed*n)), myMod);
        
	} 
    
    // Utility
    //col += step(fract(uv.x)-.005,.01)+step(fract(uv.y)-.005,.01); //grid

    fragColor = vec4(col,1.);
}


/*
// raylib main
void main()
{

    //col = getDat( iChannel0, fC);
    vec4 col = texture(mask, fragTexCoord + vec2(sin(-frame/150.0)/10.0, cos(-frame/170.0)/10.0));
    col.rgb *= .5 + .5*abs( cos( .06*iTime + PI/vec3(.5,2.,4.) - PI/3.)); 

    //vec4 maskColour = texture(mask, fragTexCoord + vec2(sin(-frame/150.0)/10.0, cos(-frame/170.0)/10.0));
    //if (maskColour.r < 0.25) discard;
    //vec4 texelColor = texture(texture0, fragTexCoord + vec2(sin(frame/90.0)/8.0, cos(frame/60.0)/8.0));
    //finalColor = texelColor*maskColour;
}
*/
