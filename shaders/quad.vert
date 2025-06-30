#version 450

//u/ZRM2
//Yep, either that or a full screen triangle. A full
//screen triangle is one that covers the whole viewport and more besides,
//and is then clipped to the viewport. I believe that's
//the more "correct" approach these days that GPUs prefer.
//source: https://www.reddit.com/r/GraphicsProgramming/comments/iz6b45/do_almost_all_games_render_to_a_fullscreen_quad/

void main() 
{
    const vec2 pos[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
}
