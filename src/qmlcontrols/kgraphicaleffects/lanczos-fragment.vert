attribute highp vec4 aPos;
attribute highp vec2 aTexCoord;
varying mediump vec2 vTexCoord;

void main() {
    gl_Position = aPos;
    vTexCoord = aTexCoord;
}
