/* Stefan-Gabriel Mirea - 331CC */
#version 330

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;

/* MAX_OBJ_NUM trebuie sa coincida cu cel din main.cpp */
#define MAX_OBJ_NUM 500

uniform mat4 view_matrix, projection_matrix;
uniform int obj_num;
uniform mat4 model_matrices[MAX_OBJ_NUM];
uniform int obj_offset[MAX_OBJ_NUM];
uniform int main_active;
uniform float time;
uniform int shake;
uniform int first_ramp_vertex, last_ramp_vertex;

/* spune daca e activa camera FPS a personajului principal activ */
uniform int main_fps;

/* culorile personajului principal activ */
uniform vec3 color1, color2;

out vec3 vertex_to_fragment_color;

void main()
{
    /* spune daca varful curent ii apartine personajului principal activ */
    int is_main_active = 0;
    if (main_active != -1)
        if (gl_VertexID >= obj_offset[main_active])
            if (main_active == obj_num - 1)
                is_main_active = 1;
            else if (gl_VertexID < obj_offset[main_active + 1])
                is_main_active = 1;
            else
                is_main_active = 0;
        else
            is_main_active = 0;
    else
        is_main_active = 0;
    if (is_main_active == 1)
    {
        float time_normalized = (sin(time) + 1) / 2;
        vertex_to_fragment_color = vec4(
            (1 - time_normalized) * color1[0] + time_normalized * color2[0],
            (1 - time_normalized) * color1[1] + time_normalized * color2[1],
            (1 - time_normalized) * color1[2] + time_normalized * color2[2],
            1.0);
    }
    else
        vertex_to_fragment_color = in_normal;

    /* caut binar pozitia obiectului din care face parte varful curent */
    int a = 0, b = obj_num - 1, mid;
    while (a <= b)
    {
        int aux = (b - a) / 2;
        mid = a + aux;
        if (obj_offset[mid] > gl_VertexID)
            b = mid - 1;
        else
            a = mid + 1;
    }

    vec4 initial_vertex = model_matrices[a - 1] * vec4(in_position,1);

    /* cutremur */
    if (shake > 0 && ((is_main_active == 1 && main_fps == 0) ||
        (gl_VertexID >= first_ramp_vertex && gl_VertexID <= last_ramp_vertex)))
    {
        initial_vertex[0] += initial_vertex[1] * 0.001 * shake * cos(7 * time);
        initial_vertex[2] += initial_vertex[1] * 0.001 * shake * sin(7 * time);
    }
    gl_Position = projection_matrix * view_matrix * initial_vertex;
}
