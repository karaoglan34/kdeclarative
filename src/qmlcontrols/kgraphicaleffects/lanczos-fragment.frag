varying highp vec2 vTexCoord;
uniform sampler2D u_tex;
// uniform highp vec4 u_tex_rect;
// uniform lowp float qt_Opacity;

uniform vec2 offsets[16];
uniform vec4 kernel[16];

void main() {

    vec4 sum = texture2D(u_tex, vTexCoord.st) * kernel[0];
    for (int i = 1; i < 16; i++) {
        sum += texture2D(u_tex, vTexCoord.st - offsets[i]) * kernel[i];
        sum += texture2D(u_tex, vTexCoord.st + offsets[i]) * kernel[i];
    }
    gl_FragColor = sum;
}
