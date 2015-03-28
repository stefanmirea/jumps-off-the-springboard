//--------------------------------------------------------------------------------------------------
// Descriere: fisier main
//
// Autor: Stefan-Gabriel Mirea, 331CC
// Data: 7 dec 2014
//--------------------------------------------------------------------------------------------------

/* incarcator de meshe */
#include "lab_mesh_loader.hpp"

/* geometrie: drawSolidCube, drawWireTeapot... */
#include "lab_geometry.hpp"

/* incarcator de shadere */
#include "lab_shader_loader.hpp"

/* interfata cu glut, ne ofera fereastra, input, context opengl */
#include "lab_glut.hpp"

/* camera */
#include "lab_camera.hpp"

/* headere utile */
#include <ctime>
#include <vector>
#include <list>
#include <bitset>
#include <algorithm>
#include <cstdlib>
#include <ctime>

/* constanta ce determina viteza de rulare; valoarea 1 corespunde variatiei culorii personajului
 * principal activ proportional cu sinusul timpului curent in secunde */
#define SPEED           4

/* vitezele de variatie ale parametrilor pentru toate tipurile de personaje si in toate modurile */
#define DELTA_ANG       100.0f /* grade/s */
#define DELTA_DIST      40.0f /* unitati/s */

/* numarul maxim de obiecte permise (impus de vertex shader, caruita ii trimit un vector de aceasta
 * dimensiune) - trebuie sa coincida cu cel din shader_vertex.glsl */
#define MAX_OBJ_NUM     500

/* latura personajelor principale */
#define MAIN_EDGE       3.3f

/* identificatori pentru camere / categorii de camere */
#define CAM_FPS_MAIN    0
#define CAM_STATIC_MAIN 1
#define CAM_FPS_SEC     2
#define CAM_STATIC_SEC  3
#define CAM_TPS_MAIN    4
#define CAM_TPS_SEC     5
#define CAM_FPS_ABOVE   6

/* identificatori pentru modurile de deplasare */
#define MODE_RAMP       0
#define MODE_JUMP       1
#define MODE_FALL       2
#define MODE_QUAD       3

/* tastele sageti */
#define KEY_LEFT        100
#define KEY_UP          101
#define KEY_RIGHT       102
#define KEY_DOWN        103

/* culorile personajului principal activ */
#define R_1             1.0f
#define G_1             1.0f
#define B_1             0.0f
#define R_2             0.0f
#define G_2             0.5f
#define B_2             0.0f

/* toleranta testarii egalitatii float-urilor */
#define EPS             1e-10

using namespace std;

struct Color {
    float r, g, b;

    Color(const float r, const float g, const float b) : r(r), g(g), b(b) {}
};

class Tema : public lab::glut::WindowListener {
/* variabile */
private:
    /* matrice 4x4 pt proiectie */
    glm::mat4 projection_matrix;
    /* matrice 4x4 pentru modelare (cate una pentru fiecare obiect) */
    vector<glm::mat4> model_matrices;
    /* pozitiile din vertices unde incep varfurile fiecarui obiect */
    vector<int> obj_offset;
    /* id-ul de opengl al obiectului de tip program shader */
    unsigned int gl_program_shader;
    unsigned int screen_width, screen_height;
    /* containere opengl pentru vertecsi, indecsi si stare */
    unsigned int mesh_vbo, mesh_ibo, mesh_vao, mesh_num_indices;
    /* variabile pentru evidenta timpului */
    float current_time, last_time;
    LARGE_INTEGER perf_freq;

    /* structurile ce contin geometria tuturor obiectelor */
    vector<lab::VertexFormat> vertices;
    vector<unsigned int> indices;

    /* geometria unui personaj secundar (pentru a nu citi din fisier de mai multe ori) */
    vector<lab::VertexFormat> sec_vertices;
    vector<unsigned int> sec_indices;

    /* dimensiunile personajului secundar */
    glm::vec3 sec_dim;

    /* numerele de ordine ale obiectelor importante (ce vor mai suferi modificari) */
    int main_active, sec_active, main_tps, sec_tps;
    vector<int> main_inactive, sec_inactive;

    /* camere */
    list<lab::Camera> cams;
    list<lab::Camera>::iterator cam_fps_main, cam_static_main, cam_fps_sec, cam_static_sec,
        cam_tps_main, cam_tps_sec, cam_fps_above;
    /* camera curenta */
    list<lab::Camera>::iterator current_cam;

    /* tastele apasate la un momentdat */
    bitset<256> pressed_keys;
    /* pentru tastele apasate, spune daca sunt sau nu speciale */
    bitset<256> is_special;

    /* intensitatea cutremurului (0 daca nu e un cutremur) */
    int shake = 0;

    /* rangul de varfuri ce intra in alcatuirea rampei personajelor principale */
    int first_ramp_vertex, last_ramp_vertex;

    /* starile in care se pot afla personajele, cu toti parametrii necesari determinarii evolutiei
     * lor */
    struct {
        int mode;
        /* unghi ce nu trebuie sa depaseasca intervalul [-90, 90] (valabil in modul MODE_RAMP) */
        float y_angle;
        /* transformarea pe care o avea personajul cand a trecut in modul MODE_JUMP */
        glm::mat4 model_before_jump;
        /* vectorul "inainte" la trecerea in modul MODE_JUMP */
        glm::vec3 forward_before_jump;
        /* unghiul subintins in timpul actiunii de jump (rad) */
        float jump_angle;
        /* spune daca personajul secundar activ efectueaza o rotatie pe quad (altfel, efectueaza o
         * translatie) */
        bool rotation;
        /* numarul de grade ramase in cadrul rotatiei personajului secundar activ */
        float deg_left;
        /* distanta ramasa in cadrul translatiei personajului secundar activ */
        float dist_left;
    } main, sec;

/* metode */
public:
    /* constructor - e apelat cand e instantiata clasa */
    Tema()
    {
        /* setari pentru desenare, clear color seteaza culoarea de clear pentru ecran
         * (format R, G, B, A) */
        glClearColor(0.5, 0.5, 0.5, 1);
        /* clear depth si depth test (nu le studiem momentan, dar avem nevoie de ele!) */
        glClearDepth(1);
        /* sunt folosite pentru a determina obiectele cele mai apropiate de camera (la curs:
         * algoritmul pictorului, algoritmul zbuffer) */
        glEnable(GL_DEPTH_TEST);

        /* incarca un shader din fisiere si gaseste locatiile matricelor relativ la programul
         * creat */
        gl_program_shader = lab::loadShader("shadere\\shader_vertex.glsl",
                                            "shadere\\shader_fragment.glsl");

        /* incarc personajul secundar, il redimensionez si ii fac flip fata de Ox */
        loadSecGeometry("resurse\\dragon.obj", glm::vec3(-0.1, 0.1, 0.1));

        /* desenez obiectele statice */
        drawScene();

        /* creez camerele */
        cams.resize(5);
        list<lab::Camera>::iterator it = cams.begin();
        cam_fps_main    = it++;
        cam_static_main = it;
        cam_fps_sec     = it++;
        cam_static_sec  = it;
        cam_tps_main    = it++;
        cam_tps_sec     = it++;
        cam_fps_above   = it;

        current_cam = cam_fps_above;

        main_tps = addTPSCam(glm::vec3(-2.7f * MAIN_EDGE, MAIN_EDGE, 1.5f * MAIN_EDGE));
        sec_tps  = addTPSCam(glm::vec3(-sec_dim[0], sec_dim[1] * 0.75f, sec_dim[2] * 1.4f));

        createMainCharacter();
        createSecCharacter();
        resetCam(CAM_FPS_ABOVE);

        /* desenare implicit in modul solid */
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        srand(static_cast<unsigned int>(time(NULL)));

        QueryPerformanceFrequency(&perf_freq);
    }

    /* destructor - e apelat cand e distrusa clasa */
    ~Tema(){
        /* distruge shader */
        glDeleteProgram(gl_program_shader);

        /* distruge mesh incarcat */
        glDeleteBuffers(1,&mesh_vbo);
        glDeleteBuffers(1,&mesh_ibo);
        glDeleteVertexArrays(1,&mesh_vao);
    }

    //----------------------------------------------------------------------------------------------
    //metode de constructie geometrie --------------------------------------------------------------

    /* actualizeaza vao, vbo, ibo, in functie de vertices si indices */
    void updateObjects()
    {
        /* vertex array object -> un obiect ce reprezinta un container pentru starea de desenare */
        glGenVertexArrays(1, &mesh_vao);
        glBindVertexArray(mesh_vao);

        /* vertex buffer object -> un obiect in care tinem vertecsii */
        glGenBuffers(1, &mesh_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, mesh_vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(lab::VertexFormat), &vertices[0],
                     GL_STATIC_DRAW);

        /* index buffer object -> un obiect in care tinem indecsii */
        glGenBuffers(1, &mesh_ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0],
                     GL_STATIC_DRAW);

        /* legatura intre atributele vertecsilor si pipeline, datele noastre sunt INTERLEAVED. */
        glEnableVertexAttribArray(0);
        /* trimite pozitii pe pipe 0 */
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(lab::VertexFormat), (void*)0);
        glEnableVertexAttribArray(1);
        /* trimite normale pe pipe 1 */
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(lab::VertexFormat),
                              (void*)(sizeof(float) * 3));

        mesh_num_indices = indices.size();
    }

    int add3DObject(const vector<lab::VertexFormat> &new_vertices,
                    const vector<unsigned int> &new_indices)
    {
        int obj_pos = model_matrices.size();
        int offset = vertices.size();
        model_matrices.push_back(glm::mat4());
        obj_offset.push_back(offset);

        vertices.insert(vertices.end(), new_vertices.begin(), new_vertices.end());
        for (unsigned int i = 0; i < new_indices.size(); ++i)
            indices.push_back(new_indices[i] + offset);

        updateObjects();
        return obj_pos;
    }

    int addParallelepiped(const glm::vec3 &dim, const glm::vec3 &off, const Color &color)
    {
        return add3DObject(
            vector<lab::VertexFormat>{
                lab::VertexFormat(         off[0],          off[1], dim[2] + off[2], color.r, color.g, color.b),
                lab::VertexFormat(dim[0] + off[0],          off[1], dim[2] + off[2], color.r, color.g, color.b),
                lab::VertexFormat(         off[0], dim[1] + off[1], dim[2] + off[2], color.r, color.g, color.b),
                lab::VertexFormat(dim[0] + off[0], dim[1] + off[1], dim[2] + off[2], color.r, color.g, color.b),
                lab::VertexFormat(         off[0],          off[1],          off[2], color.r, color.g, color.b),
                lab::VertexFormat(dim[0] + off[0],          off[1],          off[2], color.r, color.g, color.b),
                lab::VertexFormat(         off[0], dim[1] + off[1],          off[2], color.r, color.g, color.b),
                lab::VertexFormat(dim[0] + off[0], dim[1] + off[1],          off[2], color.r, color.g, color.b)},
            vector<unsigned int>{
                0, 1, 2, 1, 3, 2, 2, 3, 7, 2, 7, 6, 1, 7, 3, 1, 5, 7,
                6, 7, 4, 7, 5, 4, 0, 4, 1, 1, 4, 5, 2, 6, 4, 0, 2, 4});
    }

    int addParallelepiped(const glm::vec3 &dim, const Color &color)
    {
        return addParallelepiped(dim, glm::vec3(0.0), color);
    }

    /* adauga o camera video (doar grafic) */
    int addTPSCam(const glm::vec3 &off)
    {
        return add3DObject(
            vector<lab::VertexFormat>{
                lab::VertexFormat(-3.75f + off[0],  -0.75f + off[1],   0.75f + off[2],  0.0f,  0.0f,  0.0f),
                lab::VertexFormat(-0.75f + off[0],  -0.75f + off[1],   0.75f + off[2], 0.12f, 0.12f, 0.12f),
                lab::VertexFormat(-3.75f + off[0],   0.75f + off[1],   0.75f + off[2], 0.12f, 0.12f, 0.12f),
                lab::VertexFormat(-0.75f + off[0],   0.75f + off[1],   0.75f + off[2],  0.0f,  0.0f,  0.0f),
                lab::VertexFormat(-3.75f + off[0],  -0.75f + off[1],  -0.75f + off[2], 0.12f, 0.12f, 0.12f),
                lab::VertexFormat(-0.75f + off[0],  -0.75f + off[1],  -0.75f + off[2],  0.0f,  0.0f,  0.0f),
                lab::VertexFormat(-3.75f + off[0],   0.75f + off[1],  -0.75f + off[2],  0.0f,  0.0f,  0.0f),
                lab::VertexFormat(-0.75f + off[0],   0.75f + off[1],  -0.75f + off[2], 0.12f, 0.12f, 0.12f),
                lab::VertexFormat(-0.75f + off[0], -0.375f + off[1],  0.375f + off[2],  0.0f,  0.0f,  0.0f),
                lab::VertexFormat(  0.0f + off[0],  -0.75f + off[1],   0.75f + off[2], 0.12f, 0.12f, 0.12f),
                lab::VertexFormat(-0.75f + off[0],  0.375f + off[1],  0.375f + off[2], 0.12f, 0.12f, 0.12f),
                lab::VertexFormat(  0.0f + off[0],   0.75f + off[1],   0.75f + off[2],  0.0f,  0.0f,  0.0f),
                lab::VertexFormat(-0.75f + off[0], -0.375f + off[1], -0.375f + off[2], 0.12f, 0.12f, 0.12f),
                lab::VertexFormat(  0.0f + off[0],  -0.75f + off[1],  -0.75f + off[2],  0.0f,  0.0f,  0.0f),
                lab::VertexFormat(-0.75f + off[0],  0.375f + off[1], -0.375f + off[2],  0.0f,  0.0f,  0.0f),
                lab::VertexFormat(  0.0f + off[0],   0.75f + off[1],  -0.75f + off[2], 0.12f, 0.12f, 0.12f)},
            vector<unsigned int>{
                0, 1, 2, 1, 3, 2, 2, 3, 7, 2, 7, 6, 1, 7, 3, 1, 5, 7,
                6, 7, 4, 7, 5, 4, 0, 4, 1, 1, 4, 5, 2, 6, 4, 0, 2, 4,
                8, 9, 10, 9, 11, 10, 10, 11, 15, 10, 15, 14,
                14, 15, 12, 15, 13, 12, 8, 12, 9, 9, 12, 13});
    }

    /* incarca geometria personajului secundar */
    void loadSecGeometry(const string &filename, const glm::vec3 &scale)
    {
        lab::loadObj(filename, mesh_vao, mesh_vbo, mesh_ibo, mesh_num_indices, sec_vertices,
                     sec_indices);

        /* redimensionare */
        for (unsigned int i = 0; i < sec_vertices.size(); ++i)
        {
            sec_vertices[i].position_x *= scale[0];
            sec_vertices[i].position_y *= scale[1];
            sec_vertices[i].position_z *= scale[2];
        }
        /* determinare dimensiuni */
        glm::vec3 min_coords(sec_vertices[0].position_x, sec_vertices[0].position_y,
                             sec_vertices[0].position_z);
        glm::vec3 max_coords(min_coords);

        for (unsigned int i = 1; i < sec_vertices.size(); ++i)
        {
            min_coords[0] = min(min_coords[0], sec_vertices[i].position_x);
            min_coords[1] = min(min_coords[1], sec_vertices[i].position_y);
            min_coords[2] = min(min_coords[2], sec_vertices[i].position_z);

            max_coords[0] = max(max_coords[0], sec_vertices[i].position_x);
            max_coords[1] = max(max_coords[1], sec_vertices[i].position_y);
            max_coords[2] = max(max_coords[2], sec_vertices[i].position_z);
        }
        sec_dim = max_coords - min_coords;
    }

    /* deseneaza trambulinele si quadul */
    void drawScene()
    {
        /* trambulina personajelor principale */
        first_ramp_vertex = vertices.size();
        /* prima coloana */
        for (int i = 0; i < 6; ++i)
        {
            float gray = i % 2 == 0 ? 0.87f : 0.75f;
            int pos = addParallelepiped(glm::vec3(3.2, i < 5 ? 20.0 : 13.0, 10.0),
                                        Color(gray, gray, gray));
            model_matrices[pos] = glm::translate(model_matrices[pos],
                                                 glm::vec3(0.0, i * 20.0, 0.0));
        }
        /* a doua coloana */
        for (int i = 0; i < 5; ++i)
        {
            float gray = i % 2 == 0 ? 0.87f : 0.75f;
            int pos = addParallelepiped(glm::vec3(3.2, i < 4 ? 20.0 : 10.0, 10.0),
                                        Color(gray, gray, gray));
            model_matrices[pos] = glm::translate(model_matrices[pos],
                                                 glm::vec3(45.28, i * 20.0, 0.0));
        }
        /* rampa */
        for (int i = 0; i < 10; ++i)
        {
            /* baza */
            Color color = i % 2 == 0 ? Color(0.7f, 0.91f, 1) : Color(0.6f, 0.67f, 0.89f);
            int base = addParallelepiped(glm::vec3(13.5, 3.2, 13.5), color);
            /* doi stalpi ai balustradei */
            color = Color(0.21f, 0.12f, 0.04f);
            int pillar1 = addParallelepiped(glm::vec3(0.5, 1.5, 0.5), glm::vec3(6.5, 3.2,  0.0),
                                            color);
            int pillar2 = addParallelepiped(glm::vec3(0.5, 1.5, 0.5), glm::vec3(6.5, 3.2, 13.0),
                                            color);
            for (int pos = base; pos <= pillar2; ++pos)
            {
                model_matrices[pos] = glm::translate(model_matrices[pos],
                                                     glm::vec3(79.346096 - 12.000565 * i,
                                                               71.460749 + i * 6.183561,
                                                               -1.7));
                model_matrices[pos] = glm::rotate(model_matrices[pos], -27.2608052f,
                                                  glm::vec3(0.0, 0.0, 1.0));
            }
        }

        /* balustrada */
        int main_b1 = addParallelepiped(glm::vec3(135.0, 0.5, 0.5), glm::vec3(0.0, 4.7,  0.0),
                                        Color(0.21f, 0.12f, 0.04f));
        int main_b2 = addParallelepiped(glm::vec3(135.0, 0.5, 0.5), glm::vec3(0.0, 4.7, 13.0),
                                        Color(0.21f, 0.12f, 0.04f));
        for (int pos = main_b1; pos <= main_b2; ++pos)
        {
            model_matrices[pos] = glm::translate(model_matrices[pos],
                                                 glm::vec3(-28.658989, 127.112798, -1.7));
            model_matrices[pos] = glm::rotate(model_matrices[pos], -27.2608052f,
                                              glm::vec3(0.0, 0.0, 1.0));
        }
        last_ramp_vertex = vertices.size() - 1;

        /* trambulina personajelor secundare */
        /* prima coloana */
        for (int i = 0; i < 4; ++i)
        {
            float gray = i % 2 == 0 ? 0.87f : 0.75f;
            int pos = addParallelepiped(glm::vec3(3.2, i < 3 ? 20.0 : 14.0, 10.0),
                                        Color(gray, gray, gray));
            model_matrices[pos] = glm::translate(model_matrices[pos],
                                                 glm::vec3(0.0, i * 20.0, 66.0));
        }
        /* a doua coloana */
        for (int i = 0; i < 3; ++i)
        {
            float gray = i % 2 == 0 ? 0.87f : 0.75f;
            int pos = addParallelepiped(glm::vec3(3.2, i < 2 ? 20.0 : 19.0, 10.0),
                                        Color(gray, gray, gray));
            model_matrices[pos] = glm::translate(model_matrices[pos],
                                                 glm::vec3(57.05, i * 20.0, 66.0));
        }
        /* rampa */
        for (int i = 0; i < 11; ++i)
        {
            /* baza */
            Color color = i % 2 == 1 ? Color(0.7f, 0.91f, 1) : Color(0.6f, 0.67f, 0.89f);
            int base = addParallelepiped(glm::vec3(13.5, 3.2, 13.5), color);
            /* doi stalpi ai balustradei */
            color = Color(0.21f, 0.12f, 0.04f);
            int pillar1 = addParallelepiped(glm::vec3(0.5, 1.5, 0.5), glm::vec3(6.5, 3.2,  0.0),
                                            color);
            int pillar2 = addParallelepiped(glm::vec3(0.5, 1.5, 0.5), glm::vec3(6.5, 3.2, 13.0),
                                            color);
            for (int pos = base; pos <= pillar2; ++pos)
            {
                model_matrices[pos] = glm::translate(model_matrices[pos],
                                                     glm::vec3(94.359003 - 13.046933 * i,
                                                               47.942592 + i * 3.468073, 64.3));
                model_matrices[pos] = glm::rotate(model_matrices[pos], -14.885859f,
                                                  glm::vec3(0.0, 0.0, 1.0));
            }
        }

        /* balustrada */
        int sec_b1 = addParallelepiped(glm::vec3(148.5, 0.5, 0.5), glm::vec3(0.0, 4.7,  0.0),
                                       Color(0.21f, 0.12f, 0.04f));
        int sec_b2 = addParallelepiped(glm::vec3(148.5, 0.5, 0.5), glm::vec3(0.0, 4.7, 13.0),
                                       Color(0.21f, 0.12f, 0.04f));
        for (int pos = sec_b1; pos <= sec_b2; ++pos)
        {
            model_matrices[pos] = glm::translate(model_matrices[pos],
                                                 glm::vec3(-36.110327, 82.623322, 64.3));
            model_matrices[pos] = glm::rotate(model_matrices[pos], -14.885859f,
                                              glm::vec3(0.0, 0.0, 1.0));
        }

        /* quadul */
        int offset = vertices.size();
        obj_offset.push_back(offset);
        for (int i = 0; i < 10; ++i)
            for (int j = 0; j < 13; ++j)
            {
                Color color = (i % 2) ^ (j % 2) ?
                              Color(0.78f, 0.87f, 0.87f) : Color(0.69f, 0.81f, 0.82f);
                vertices.push_back(lab::VertexFormat(i * 18.0f, 0, j * 18.0f,
                                                     color.r, color.g, color.b));
                if (i > 0 && j > 0)
                {
                    vector<int> new_indices{
                        (i - 1) * 13 + j - 1,
                        (i - 1) * 13 + j,
                        i * 13 + j,
                        (i - 1) * 13 + j - 1,
                        i * 13 + j,
                        i * 13 + j - 1};
                    for (unsigned int k = 0; k < new_indices.size(); ++k)
                        indices.push_back((unsigned int)new_indices[k] + offset);
                }
            }
        updateObjects();
        model_matrices.push_back(glm::translate(glm::mat4(), glm::vec3(78.748188, 0.0, -70.0)));

        /* peretii quadului */
        Color color(0, 0, 0.29f);
        add3DObject(
            vector<lab::VertexFormat>{
                lab::VertexFormat (78.748188f,    0, -70, color.r, color.g, color.b),
                lab::VertexFormat(240.748188f,    0, -70, color.r, color.g, color.b),
                lab::VertexFormat(240.748188f,    0, 146, color.r, color.g, color.b),
                lab::VertexFormat( 78.748188f,    0, 146, color.r, color.g, color.b),
                lab::VertexFormat( 78.748188f, 2.5f, -70, color.r, color.g, color.b),
                lab::VertexFormat(240.748188f, 2.5f, -70, color.r, color.g, color.b),
                lab::VertexFormat(240.748188f, 2.5f, 146, color.r, color.g, color.b),
                lab::VertexFormat( 78.748188f, 2.5f, 146, color.r, color.g, color.b)},
            vector<unsigned int>{
                3, 7, 4, 4, 0, 3, 4, 5, 0, 0, 5, 1,
                6, 5, 1, 6, 1, 2, 7, 6, 2, 3, 7, 2});
    }

    //----------------------------------------------------------------------------------------------
    //metode legate mai mult de logica programului -------------------------------------------------
    bool isPressed(const unsigned char key, const bool special_key) const
    {
        return pressed_keys.test(key) && (is_special.test(key) == special_key);
    }

    void createMainCharacter()
    {
        main_active = addParallelepiped(glm::vec3(MAIN_EDGE), glm::vec3(-MAIN_EDGE / 2),
                                        Color(0, 0, 0));
        model_matrices[main_active] = glm::translate(model_matrices[main_active],
                                                     glm::vec3(-21.196693, 126.858387, 5.0));
        model_matrices[main_active] = glm::rotate(model_matrices[main_active], -27.2608052f,
                                                  glm::vec3(0.0, 0.0, 1.0));
        model_matrices[main_active] = glm::translate(model_matrices[main_active],
                                                     glm::vec3(0.0, MAIN_EDGE / 2, 0.0));
        resetCam(CAM_FPS_MAIN);
        resetCam(CAM_TPS_MAIN);

        main.mode = MODE_RAMP;
        main.y_angle = 0;
    }

    void createSecCharacter()
    {
        sec_active = add3DObject(sec_vertices, sec_indices);
        model_matrices[sec_active] = glm::translate(model_matrices[sec_active],
                                                    glm::vec3(-28.77, 84.0, 71.0));
        model_matrices[sec_active] = glm::rotate(model_matrices[sec_active], -14.885859f,
                                                 glm::vec3(0.0, 0.0, 1.0));
        model_matrices[sec_active] = glm::translate(model_matrices[sec_active],
                                                    glm::vec3(0.0, sec_dim[1] / 2, 0.0));
        resetCam(CAM_FPS_SEC);
        resetCam(CAM_TPS_SEC);

        sec.mode = MODE_RAMP;
    }

    /* actualizeaza pozitia unei camere sau a camerelor dintr-o categorie */
    void resetCam(const int cam)
    {
        glm::vec3 cam_up;
        if (cam == CAM_FPS_MAIN || cam == CAM_TPS_MAIN)
            cam_up = glm::vec3(model_matrices[main_active] * glm::vec4(0.0, 1.0, 0.0, 0.0));
        else if (cam == CAM_FPS_SEC || cam == CAM_TPS_SEC)
            cam_up = glm::vec3(model_matrices[sec_active] * glm::vec4(0.0, 1.0, 0.0, 0.0));
        switch (cam)
        {
            case CAM_FPS_MAIN:
            {
                cam_fps_main->set(
                    glm::vec3(model_matrices[main_active] * glm::vec4(MAIN_EDGE / 2 + 0.1,
                                                                      MAIN_EDGE / 2, 0.0, 1.0)),
                    glm::vec3(model_matrices[main_active] * glm::vec4(MAIN_EDGE / 2 + 1.1,
                                                                      MAIN_EDGE / 2, 0.0, 1.0)),
                    cam_up);
                break;
            }
            case CAM_STATIC_MAIN:
            {
                glm::vec3 main_active_pos(model_matrices[main_active] *
                                          glm::vec4(0.0, 0.0, 0.0, 1.0));
                glm::vec2 main_active_pos_xz(main_active_pos[0], main_active_pos[2]);
                float radius = static_cast<float>(MAIN_EDGE * sqrt(2) / 2) + 0.1f;
                /* actualizez toate camerele statice ale personajelor principale inactive */
                list<lab::Camera>::iterator it = cam_fps_main;
                for (unsigned int i = 0; i < main_inactive.size(); ++i)
                {
                    ++it;
                    glm::vec3 main_inactive_pos(model_matrices[main_inactive[i]] *
                                                glm::vec4(0.0, 0.0, 0.0, 1.0));
                    glm::vec2 main_inactive_pos_xz(main_inactive_pos[0], main_inactive_pos[2]);
                    glm::vec3 cam_pos;
                    float dist_xz = glm::distance(main_active_pos_xz, main_inactive_pos_xz);
                    if (dist_xz <= radius)
                    {
                        cam_pos = glm::vec3(main_active_pos[0], MAIN_EDGE + 0.1,
                                            main_active_pos[2]);
                        it->set(cam_pos, main_active_pos, glm::vec3(1.0, 0.0, 0.0));
                    }
                    else
                    {
                        float factor = radius / dist_xz;
                        cam_pos = glm::vec3(
                            main_active_pos[0] * factor + main_inactive_pos[0] * (1 - factor),
                            MAIN_EDGE,
                            main_active_pos[2] * factor + main_inactive_pos[2] * (1 - factor));
                        it->set(cam_pos, main_active_pos, glm::vec3(0.0, 1.0, 0.0));
                    }
                }
                break;
            }
            case CAM_FPS_SEC:
            {
                cam_fps_sec->set(
                    glm::vec3(model_matrices[sec_active] * glm::vec4(sec_dim[0] / 2, sec_dim[1] / 4,
                                                                     0.0, 1.0)),
                    glm::vec3(model_matrices[sec_active] * glm::vec4(sec_dim[0] / 2 + 1,
                                                                     sec_dim[1] / 4, 0.0, 1.0)),
                    cam_up);
                break;
            }
            case CAM_STATIC_SEC:
            {
                glm::vec3 sec_active_pos(model_matrices[sec_active] *
                                         glm::vec4(0.0, 0.0, 0.0, 1.0));
                glm::vec2 sec_active_pos_xz(sec_active_pos[0], sec_active_pos[2]);
                float radius = max(sec_dim[0], sec_dim[2]) * static_cast<float>(sqrt(2) / 2) + 0.1f;
                /* actualizez toate camerele statice ale personajelor secundare inactive */
                list<lab::Camera>::iterator it = cam_fps_sec;
                for (unsigned int i = 0; i < sec_inactive.size(); ++i)
                {
                    ++it;
                    glm::vec3 sec_inactive_pos(model_matrices[sec_inactive[i]] *
                                               glm::vec4(0.0, 0.0, 0.0, 1.0));
                    glm::vec2 sec_inactive_pos_xz(sec_inactive_pos[0], sec_inactive_pos[2]);
                    glm::vec3 cam_pos;
                    float dist_xz = glm::distance(sec_active_pos_xz, sec_inactive_pos_xz);
                    if (dist_xz <= radius)
                    {
                        cam_pos = glm::vec3(sec_active_pos[0], sec_dim[1] + 0.1, sec_active_pos[2]);
                        it->set(cam_pos, sec_active_pos, glm::vec3(1.0, 0.0, 0.0));
                    }
                    else
                    {
                        float factor = radius / dist_xz;
                        cam_pos = glm::vec3(
                            sec_active_pos[0] * factor + sec_inactive_pos[0] * (1 - factor),
                            sec_dim[1],
                            sec_active_pos[2] * factor + sec_inactive_pos[2] * (1 - factor));
                        it->set(cam_pos, sec_active_pos, glm::vec3(0.0, 1.0, 0.0));
                    }
                }
                break;
            }
            case CAM_TPS_MAIN:
            {
                cam_tps_main->set(
                    glm::vec3(model_matrices[main_active] *
                              glm::vec4(-2.7 * MAIN_EDGE, MAIN_EDGE, 1.5 * MAIN_EDGE, 1.0)),
                    glm::vec3(model_matrices[main_active] *
                              glm::vec4(-2.7 * MAIN_EDGE + 1, MAIN_EDGE, 1.5 * MAIN_EDGE, 1.0)),
                    cam_up);
                model_matrices[main_tps] = model_matrices[main_active];
                break;
            }
            case CAM_TPS_SEC:
            {
                cam_tps_sec->set(
                    glm::vec3(model_matrices[sec_active] *
                              glm::vec4(-sec_dim[0], sec_dim[1] * 0.75, sec_dim[2] * 1.4, 1.0)),
                    glm::vec3(model_matrices[sec_active] *
                              glm::vec4(-sec_dim[0] + 1, sec_dim[1] * 0.75, sec_dim[2] * 1.4, 1.0)),
                    cam_up);
                model_matrices[sec_tps] = model_matrices[sec_active];
                break;
            }
            case CAM_FPS_ABOVE:
                cam_fps_above->set(glm::vec3(102.0, 155.0, 203.0), glm::vec3(102.0, 20.0, 38.0),
                                   glm::vec3(0.0, 1.0, 0.0));
        }
    }

    /* primeste un punct in plan si varfurile unui poligon convex si spune daca punctul se afla in
     * interiorul poligonului */
    bool inPoly(const glm::vec2 &point, const vector<glm::vec2> &vertices) const
    {
        int num_vert = vertices.size();
        for (int i = 0; i < num_vert; ++i)
        {
            int j = i + 1;
            if (j >= num_vert)
                j -= num_vert;
            int k = i + 2;
            if (k >= num_vert)
                k -= num_vert;
            /* verific daca dreapta determinata de varfurile i si j separa punctul point de varful
             * k */
            if (fabs(vertices[i][0] - vertices[j][0]) <= EPS)
            {
                if ((vertices[i][0] - point[0]) * (vertices[i][0] - vertices[k][0]) < 0)
                    return false;
            }
            else
            {
                float m = (vertices[i][1] - vertices[j][1]) / (vertices[i][0] - vertices[j][0]);
                float y0 = vertices[i][1] - m * vertices[i][0];
                if ((m * point[0] + y0 - point[1]) * (m * vertices[k][0] + y0 - vertices[k][1]) < 0)
                    return false;
            }
        }
        return true;
    }

    /* verifica daca doua paralelipipede asezate orizontal (posibil rotite doar fata de Oy) se
     * intersecteaza */
    bool collided(const glm::mat4 &first_mat,  const glm::vec3 &first_dim,
                  const glm::mat4 &second_mat, const glm::vec3 &second_dim) const
    {
        float first_h  = (first_mat  * glm::vec4(0.0, 0.0, 0.0, 1.0))[1];
        float second_h = (second_mat * glm::vec4(0.0, 0.0, 0.0, 1.0))[1];
        if (first_h - first_dim[1] / 2 <= second_h + second_dim[1] / 2 &&
            first_h + first_dim[1] / 2 >= second_h - second_dim[1] / 2)
        {
            /* calculez coordonatele x si y ale varfurilor bazelor */
            vector<glm::vec2> first_vert, second_vert;
            for (int i = 0; i < 2; ++i)
                for (int j = 0; j < 2; ++j)
                {
                    /* varfurile trebuie retinute in ordine */
                    int sign_x = i == 0 ? -1 : 1;
                    int sign_z = j == i ? -1 : 1;
                    glm::vec4 first_transf(first_mat * glm::vec4(sign_x * first_dim[0] / 2, 0.0,
                                                                 sign_z * first_dim[2] / 2, 1.0));
                    first_vert.push_back(glm::vec2(first_transf[0], first_transf[2]));
                    glm::vec4 second_transf(second_mat * glm::vec4(sign_x * second_dim[0] / 2, 0.0,
                                                                   sign_z * second_dim[2] / 2,
                                                                   1.0));
                    second_vert.push_back(glm::vec2(second_transf[0], second_transf[2]));
                }

            for (int i = 0; i < 4; ++i)
            {
                if (inPoly(first_vert[i], second_vert))
                    return true;
                if (inPoly(second_vert[i], first_vert))
                    return true;
            }
            if (inPoly((first_vert[0] + first_vert[2]) / 2.0f, second_vert))
                return true;
            if (inPoly((second_vert[0] + second_vert[2]) / 2.0f, first_vert))
                return true;
        }
        return false;
    }

    /* verifica daca un obiect s-a ciocnit de oricare dintre celelalte */
    bool collisionFound(const glm::mat4 &transform, const glm::vec3 &dim, const int index) const
    {
        /* ciocnire cu personajul principal activ */
        if (main_active != index && collided(transform, dim, model_matrices[main_active],
                                             glm::vec3(MAIN_EDGE)))
            return true;

        /* ciocniri cu personajele principale inactive */
        for (unsigned int i = 0; i < main_inactive.size(); ++i)
            if (main_inactive[i] != index &&
                collided(transform, dim, model_matrices[main_inactive[i]], glm::vec3(MAIN_EDGE)))
                return true;

        /* ciocnire cu personajul secundar activ */
        if (sec_active != index && collided(transform, dim, model_matrices[sec_active], sec_dim))
            return true;

        /* ciocniri cu personajele secundare inactive */
        for (unsigned int i = 0; i < sec_inactive.size(); ++i)
            if (sec_inactive[i] != index && collided(transform, dim,
                                                     model_matrices[sec_inactive[i]], sec_dim))
                return true;

        return false;
    }

    /* initiaza o miscare aleatoare pe care personajul secundar activ va incerca sa o realizeze pe
     * quad */
    void randomMove(const bool rotation, float &deg_left, float &dist_left)
    {
        if (rotation)
            deg_left = rand() * 180.0f / RAND_MAX - 90;
        else
            dist_left = 10.0f + rand() % 65;
    }

    /* verifica daca s-a depasit quadul si, in caz afirmativ, realizeaza o corectie */
    bool outOfQuad(const glm::vec3 &dim, const int index)
    {
        bool exceed_left = false, exceed_right = false, exceed_up = false, exceed_down = false;
        glm::vec3 diff(0.0);
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 2; ++j)
            {
                float x = dim[0] / 2, z = dim[1] / 2;
                if (!i)
                    x = -x;
                if (!j)
                    z = -z;
                glm::vec4 new_vert(model_matrices[index] * glm::vec4(x, 0.0, z, 1.0));
                if (new_vert[0] < 78.748188)
                {
                    exceed_down = true;
                    diff[0] = max(diff[0], 78.748188f - new_vert[0]);
                }
                if (new_vert[0] > 240.748188)
                {
                    exceed_up = true;
                    diff[0] = min(diff[0], 240.748188f - new_vert[0]);
                }
                if (new_vert[2] < -70)
                {
                    exceed_left = true;
                    diff[2] = max(diff[2], -70 - new_vert[2]);
                }
                if (new_vert[2] > 146)
                {
                    exceed_right = true;
                    diff[2] = min(diff[2], 146 - new_vert[2]);
                }
            }
        if (exceed_left || exceed_right || exceed_up || exceed_down)
        {
            glm::mat4 translation_mat = glm::translate(glm::mat4(), diff);
            model_matrices[index] = translation_mat * model_matrices[index];
            return true;
        }
        return false;
    }

    //----------------------------------------------------------------------------------------------
    //metode de cadru ------------------------------------------------------------------------------

    /* metoda chemata inainte de a incepe cadrul de desenare, o folosim ca sa updatam situatia
     * scenei (modelam / simulam scena) */
    void notifyBeginFrame()
    {
        static bool first_frame = true;
        LARGE_INTEGER perf_time;
        QueryPerformanceCounter(&perf_time);
        float new_time = static_cast<float>(perf_time.QuadPart) / perf_freq.QuadPart;

        if (first_frame)
        {
            first_frame = false;
            last_time = new_time;

            /* pentru ca timpul normalizat sa fie 0 initial */
            current_time = -glm::half_pi<float>();
            glUniform1f(glGetUniformLocation(gl_program_shader, "time"), current_time);
            return;
        }

        float frame_delta_ang  = (new_time - last_time) * DELTA_ANG;
        float frame_delta_dist = (new_time - last_time) * DELTA_DIST;

        /* actualizarea camerei FPS de sus */
        if (current_cam == cam_fps_above)
        {
            float cam_delta_ang = frame_delta_ang * 0.01f;
            float cam_delta_dist = frame_delta_dist * 2.5f;

            if (isPressed('w', false)) cam_fps_above->translateForward(cam_delta_dist);
            if (isPressed('a', false)) cam_fps_above->translateRight(-cam_delta_dist);
            if (isPressed('s', false)) cam_fps_above->translateForward(-cam_delta_dist);
            if (isPressed('d', false)) cam_fps_above->translateRight(cam_delta_dist);
            if (isPressed('r', false)) cam_fps_above->translateUpward(cam_delta_dist);
            if (isPressed('f', false)) cam_fps_above->translateUpward(-cam_delta_dist);
            if (isPressed('q', false)) cam_fps_above->rotateFPSoY(cam_delta_ang);
            if (isPressed('e', false)) cam_fps_above->rotateFPSoY(-cam_delta_ang);
            if (isPressed('z', false)) cam_fps_above->rotateFPSoZ(-cam_delta_ang);
            if (isPressed('x', false)) cam_fps_above->rotateFPSoZ(cam_delta_ang);
            if (isPressed('t', false)) cam_fps_above->rotateFPSoX(cam_delta_ang);
            if (isPressed('g', false)) cam_fps_above->rotateFPSoX(-cam_delta_ang);
        }

        /* actualizarea personajului principal activ */
        switch (main.mode)
        {
            case MODE_RAMP:
            {
                bool modified = false;
                if (isPressed(KEY_UP, true))
                {
                    model_matrices[main_active] = glm::translate(model_matrices[main_active],
                                                                 glm::vec3(frame_delta_dist, 0.0,
                                                                           0.0));
                    modified = true;
                }
                if (isPressed(KEY_LEFT, true))
                {
                    float inc = frame_delta_ang;
                    if (main.y_angle + inc > 90)
                        inc = 90 - main.y_angle;
                    main.y_angle += inc;
                    model_matrices[main_active] = glm::rotate(model_matrices[main_active], inc,
                                                  glm::vec3(0.0, 1.0, 0.0));
                    modified = true;
                }
                if (isPressed(KEY_RIGHT, true))
                {
                    float inc = -frame_delta_ang;
                    if (main.y_angle + inc < -90)
                        inc = -90 - main.y_angle;
                    main.y_angle += inc;
                    model_matrices[main_active] = glm::rotate(model_matrices[main_active], inc,
                                                  glm::vec3(0.0, 1.0, 0.0));
                    modified = true;
                }
                if (modified)
                {
                    /* verific daca sunt pe cale sa ies de pe rampa */
                    bool exceed_left = false, exceed_right = false;
                    float diff = 0;
                    for (int i = 0; i < 2; ++i)
                        for (int j = 0; j < 2; ++j)
                        {
                            float x = MAIN_EDGE / 2, z = MAIN_EDGE / 2;
                            if (!i)
                                x = -x;
                            if (!j)
                                z = -z;
                            glm::vec4 new_vert(model_matrices[main_active] *
                                               glm::vec4(x, 0.0, z, 1.0));
                            if (new_vert[2] < -1.25)
                            {
                                exceed_left = true;
                                diff = max(diff, -1.25f - new_vert[2]);
                            }
                            if (new_vert[2] > 11.25)
                            {
                                exceed_right = true;
                                diff = min(diff, 11.25f - new_vert[2]);
                            }
                        }
                    if (exceed_left || exceed_right)
                    {
                        glm::mat4 translation_mat = glm::translate(glm::mat4(),
                                                                   glm::vec3(0.0, 0.0, diff));
                        model_matrices[main_active] = translation_mat * model_matrices[main_active];
                    }
                    resetCam(CAM_FPS_MAIN);
                    resetCam(CAM_TPS_MAIN);
                    resetCam(CAM_STATIC_MAIN);
                }
                /* verific daca am ajuns la cel mai de jos punct de pe rampa */
                if (glm::vec4(model_matrices[main_active] *
                              glm::vec4(0.0, 0.0, 0.0, 1.0))[0] > 92.808679)
                {
                    main.mode = MODE_JUMP;
                    main.model_before_jump = model_matrices[main_active];
                    main.forward_before_jump = glm::vec3(model_matrices[main_active] *
                                                         glm::vec4(MAIN_EDGE / 2, 0.0, 0.0, 0.0));
                    main.jump_angle = 2.444f;
                    shake = 0;
                }
                break;
            }
            case MODE_JUMP:
            {
                if (isPressed(KEY_UP, true))
                {
                    main.jump_angle -= frame_delta_dist / 30.6f;
                    if (main.jump_angle < 0)
                    {
                        main.jump_angle = 0;
                        main.mode = MODE_FALL;
                    }
                    float dx, dy, dz;
                    glm::vec3 origin_before_jump(main.model_before_jump *
                                                 glm::vec4(0.0, 0.0, 0.0, 1.0));
                    float dxz = 30.6f * (-cos(2.444f) + cos(main.jump_angle));
                    float factor =
                        dxz / sqrt(main.forward_before_jump[0] * main.forward_before_jump[0] +
                                   main.forward_before_jump[2] * main.forward_before_jump[2]);
                    dx = main.forward_before_jump[0] * factor;
                    dz = main.forward_before_jump[2] * factor;
                    dy = 30.6f * (-sin(2.444f) + sin(main.jump_angle));
                    glm::mat4 translation_mat = glm::translate(glm::mat4(), glm::vec3(dx, dy, dz));
                    model_matrices[main_active] = translation_mat * main.model_before_jump;
                    model_matrices[main_active] = glm::rotate(model_matrices[main_active],
                                                              -main.y_angle,
                                                              glm::vec3(0.0, 1.0, 0.0));
                    model_matrices[main_active] = glm::rotate(model_matrices[main_active],
                                                              (1 - main.jump_angle / 2.444f)
                                                                  * 27.2608052f,
                                                              glm::vec3(0.0, 0.0, 1.0));
                    model_matrices[main_active] = glm::rotate(model_matrices[main_active],
                                                              main.y_angle,
                                                              glm::vec3(0.0, 1.0, 0.0));
                    resetCam(CAM_FPS_MAIN);
                    resetCam(CAM_TPS_MAIN);
                    resetCam(CAM_STATIC_MAIN);
                }
                break;
            }
            case MODE_FALL:
            {
                if (isPressed(KEY_UP, true))
                {
                    glm::mat4 new_mat(model_matrices[main_active]);
                    new_mat = glm::translate(new_mat, glm::vec3(0.0, -frame_delta_dist, 0.0));
                    if (collisionFound(new_mat, glm::vec3(MAIN_EDGE), main_active))
                        break;
                    else
                        model_matrices[main_active] = new_mat;

                    float h = (model_matrices[main_active] *
                               glm::vec4(0.0, -MAIN_EDGE / 2, 0.0, 1.0))[1];
                    if (h < 0)
                    {
                        model_matrices[main_active] = glm::translate(model_matrices[main_active],
                                                                     glm::vec3(0.0, -h, 0.0));
                        main.mode = MODE_QUAD;
                    }
                    resetCam(CAM_FPS_MAIN);
                    resetCam(CAM_TPS_MAIN);
                    resetCam(CAM_STATIC_MAIN);
                }
                break;
            }
            case MODE_QUAD:
            {
                glm::mat4 new_mat(model_matrices[main_active]);
                if (isPressed(KEY_UP, true))
                    new_mat = glm::translate(new_mat, glm::vec3(frame_delta_dist, 0.0, 0.0));
                if (isPressed(KEY_DOWN, true))
                    new_mat = glm::translate(new_mat, glm::vec3(-frame_delta_dist, 0.0, 0.0));
                if (isPressed(KEY_LEFT, true))
                    new_mat = glm::rotate(new_mat, frame_delta_ang, glm::vec3(0.0, 1.0, 0.0));
                if (isPressed(KEY_RIGHT, true))
                    new_mat = glm::rotate(new_mat, -frame_delta_ang, glm::vec3(0.0, 1.0, 0.0));

                if (isPressed(KEY_UP, true) || isPressed(KEY_DOWN, true) ||
                    isPressed(KEY_LEFT, true) || isPressed(KEY_RIGHT, true))
                {
                    if (collisionFound(new_mat, glm::vec3(MAIN_EDGE), main_active))
                        break;
                    else
                        model_matrices[main_active] = new_mat;

                    outOfQuad(glm::vec3(MAIN_EDGE), main_active);
                    resetCam(CAM_FPS_MAIN);
                    resetCam(CAM_TPS_MAIN);
                }
            }
        }

        /* actualizarea personajului secundar activ */
        switch (sec.mode)
        {
            case MODE_RAMP:
            {
                model_matrices[sec_active] = glm::translate(model_matrices[sec_active],
                                                            glm::vec3(frame_delta_dist, 0.0, 0.0));
                resetCam(CAM_FPS_SEC);
                resetCam(CAM_TPS_SEC);
                resetCam(CAM_STATIC_SEC);
                if (glm::vec4(model_matrices[sec_active] * glm::vec4(0.0, 0.0, 0.0, 1.0))[0] >
                    108.227998)
                {
                    sec.mode = MODE_JUMP;
                    sec.model_before_jump = model_matrices[sec_active];
                    sec.jump_angle = 2.444f;
                }
                break;
            }
            case MODE_JUMP:
            {
                sec.jump_angle -= frame_delta_dist / 30.6f;
                if (sec.jump_angle < 0)
                {
                    sec.jump_angle = 0;
                    sec.mode = MODE_FALL;
                }
                float dx, dy;
                glm::vec3 origin_before_jump(sec.model_before_jump * glm::vec4(0.0, 0.0, 0.0, 1.0));
                dx = 30.6f * (-cos(2.444f) + cos(sec.jump_angle));
                dy = 30.6f * (-sin(2.444f) + sin(sec.jump_angle));
                glm::mat4 translation_mat = glm::translate(glm::mat4(), glm::vec3(dx, dy, 0.0));
                model_matrices[sec_active] = translation_mat * sec.model_before_jump;
                model_matrices[sec_active] = glm::rotate(model_matrices[sec_active],
                                                         (1 - sec.jump_angle / 2.444f) * 14.885859f,
                                                         glm::vec3(0.0, 0.0, 1.0));
                resetCam(CAM_FPS_SEC);
                resetCam(CAM_TPS_SEC);
                resetCam(CAM_STATIC_SEC);
                break;
            }
            case MODE_FALL:
            {
                glm::mat4 new_mat(model_matrices[sec_active]);
                new_mat = glm::translate(new_mat, glm::vec3(0.0, -frame_delta_dist, 0.0));
                if (collisionFound(new_mat, sec_dim, sec_active))
                    break;
                else
                    model_matrices[sec_active] = new_mat;

                float h = (model_matrices[sec_active] *
                           glm::vec4(0.0, -sec_dim[1] / 2, 0.0, 1.0))[1];
                if (h < 0)
                {
                    model_matrices[sec_active] = glm::translate(model_matrices[sec_active],
                                                                glm::vec3(0.0, -h, 0.0));
                    sec.mode = MODE_QUAD;
                    sec.rotation = rand() % 2 == 0;
                    randomMove(sec.rotation, sec.deg_left, sec.dist_left);
                }
                resetCam(CAM_FPS_SEC);
                resetCam(CAM_TPS_SEC);
                resetCam(CAM_STATIC_SEC);
                break;
            }
            case MODE_QUAD:
            {
                bool done = false;
                if (sec.rotation)
                {
                    float angle = sec.deg_left > 0 ? -frame_delta_ang : frame_delta_ang;
                    glm::mat4 new_mat(model_matrices[sec_active]);
                    new_mat = glm::rotate(new_mat, angle, glm::vec3(0.0, 1.0, 0.0));
                    if (!collisionFound(new_mat, sec_dim, sec_active))
                    {
                        model_matrices[sec_active] = new_mat;
                        if (sec.deg_left * (sec.deg_left + angle) < 0)
                        {
                            sec.rotation = false;
                            randomMove(sec.rotation, sec.deg_left, sec.dist_left);
                        }
                        else
                            sec.deg_left += angle;
                    }
                    else
                        done = true;
                }
                else
                {
                    glm::mat4 new_mat(model_matrices[sec_active]);
                    new_mat = glm::translate(new_mat, glm::vec3(frame_delta_dist, 0.0, 0.0));
                    if (!collisionFound(new_mat, sec_dim, sec_active))
                    {
                        model_matrices[sec_active] = new_mat;
                        sec.dist_left -= frame_delta_dist;
                        if (sec.dist_left < 0)
                        {
                            sec.rotation = true;
                            randomMove(sec.rotation, sec.deg_left, sec.dist_left);
                        }
                    }
                    else
                        done = true;
                }
                if (!done && outOfQuad(sec_dim, sec_active))
                    done = true;
                if (!done)
                {
                    resetCam(CAM_FPS_SEC);
                    resetCam(CAM_TPS_SEC);
                }
                else
                {
                    sec_inactive.push_back(sec_active);
                    createSecCharacter();

                    /* creez o camera statica */
                    cams.insert(cam_static_sec, lab::Camera());
                    resetCam(CAM_STATIC_SEC);
                }
                break;
            }
        }

        current_time += SPEED * (new_time - last_time);
        last_time = new_time;
        glUniform1f(glGetUniformLocation(gl_program_shader, "time"), current_time);
    }

    /* metoda de afisare (lucram cu banda grafica) */
    void notifyDisplayFrame()
    {
        /* pe tot ecranul */
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, screen_width, screen_height);

        /* foloseste shaderul */
        glUseProgram(gl_program_shader);

        /* trimite variabile uniforme la shader */
        glUniform1i(glGetUniformLocation(gl_program_shader, "obj_num"),
                    static_cast<int>(obj_offset.size()));
        glUniformMatrix4fv(glGetUniformLocation(gl_program_shader, "model_matrices"),
                           model_matrices.size(), false, glm::value_ptr(model_matrices[0]));
        glUniform1iv(glGetUniformLocation(gl_program_shader, "obj_offset"), obj_offset.size(),
                     &obj_offset[0]);
        glUniform1i(glGetUniformLocation(gl_program_shader, "main_active"), main_active);
        glUniform1i(glGetUniformLocation(gl_program_shader, "shake"), shake);
        glUniform1i(glGetUniformLocation(gl_program_shader, "first_ramp_vertex"),
                    first_ramp_vertex);
        glUniform1i(glGetUniformLocation(gl_program_shader, "last_ramp_vertex"), last_ramp_vertex);
        glUniform1i(glGetUniformLocation(gl_program_shader, "main_fps"),
                    current_cam == cam_fps_main ? 1 : 0);
        glUniform3f(glGetUniformLocation(gl_program_shader, "color1"), R_1, G_1, B_1);
        glUniform3f(glGetUniformLocation(gl_program_shader, "color2"), R_2, G_2, B_2);

        const glm::lowp_float *view_matrix = glm::value_ptr(current_cam->getViewMatrix());
        glUniformMatrix4fv(glGetUniformLocation(gl_program_shader, "view_matrix"), 1, false,
                           view_matrix);
        glUniformMatrix4fv(glGetUniformLocation(gl_program_shader, "projection_matrix"), 1, false,
                           glm::value_ptr(projection_matrix));

        /* bind obiect */
        glBindVertexArray(mesh_vao);
        glDrawElements(GL_TRIANGLES, mesh_num_indices, GL_UNSIGNED_INT, 0);
    }

    /* metoda chemata dupa ce am terminat cadrul de desenare (poate fi folosita pt modelare /
     * simulare) */
    void notifyEndFrame() {}

    /* metoda care e chemata cand se schimba dimensiunea ferestrei initiale */
    void notifyReshape(int width, int height, int previos_width, int previous_height)
    {
        /* reshape */
        if (height == 0)
            height = 1;
        screen_width = width;
        screen_height = height;
        float aspect = width * 1.0f / height;
        projection_matrix = glm::perspective(75.0f, aspect, 0.1f, 10000.0f);
    }

    //----------------------------------------------------------------------------------------------
    //metoda de input output -----------------------------------------------------------------------

    /* tasta apasata */
    void notifyKeyPressed(unsigned char key_pressed, int mouse_x, int mouse_y)
    {
        pressed_keys.set(key_pressed);

        /* ESC inchide glut */
        if (key_pressed == 27)
            lab::glut::close();
        else if (key_pressed == 32)
        {
            /* SPACE reincarca shaderul si recalculeaza locatiile (offseti / pointeri) */
            glDeleteProgram(gl_program_shader);
            gl_program_shader = lab::loadShader("shadere\\shader_vertex.glsl",
                                                "shadere\\shader_fragment.glsl");
        }
        else if ((current_cam != cam_fps_above && key_pressed == 'w') || key_pressed == 'W')
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        else if ((current_cam != cam_fps_above && key_pressed == 's') || key_pressed == 'S')
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        else if (key_pressed == 'c')
        {
            ++current_cam;
            if (current_cam == cams.end())
                current_cam = cams.begin();
        }
        else if (key_pressed == 'C')
        {
            if (current_cam == cams.begin())
                current_cam = cams.end();
            --current_cam;
        }
        else if (key_pressed == 'o' && current_cam == cam_fps_above)
            resetCam(CAM_FPS_ABOVE);
        else if (main.mode == MODE_QUAD && ((current_cam != cam_fps_above && key_pressed == 'a') ||
                                            key_pressed == 'A'))
        {
            /* setez culoarea finala a personajului */
            float time_normalized = (sin(current_time) + 1) / 2;
            int last_vertex;
            if (main_active == obj_offset.size() - 1)
                last_vertex = vertices.size() - 1;
            else
                last_vertex = obj_offset[main_active + 1] - 1;
            for (int i = obj_offset[main_active]; i <= last_vertex; ++i)
            {
                vertices[i].normal_x = (1 - time_normalized) * R_1 + time_normalized * R_2;
                vertices[i].normal_y = (1 - time_normalized) * G_1 + time_normalized * G_2;
                vertices[i].normal_z = (1 - time_normalized) * B_1 + time_normalized * B_2;
            }

            main_inactive.push_back(main_active);
            createMainCharacter();

            /* creez o camera statica */
            cams.insert(cam_static_main, lab::Camera());
            resetCam(CAM_STATIC_MAIN);
        }
        else if (main.mode == MODE_RAMP && ((current_cam != cam_fps_above && key_pressed == 'e') ||
                                            key_pressed == 'E'))
            shake = rand() % 10 + 1;
        else if (main.mode == MODE_RAMP && ((current_cam != cam_fps_above && key_pressed == 'r') ||
                                            key_pressed == 'R'))
            shake = 0;
        else if (key_pressed >= '1' && key_pressed <= '5')
            switch (key_pressed)
            {
                case '1': current_cam = cam_fps_main; break;
                case '2': current_cam = cam_tps_main; break;
                case '3': current_cam = cam_fps_sec; break;
                case '4': current_cam = cam_tps_sec; break;
                case '5': current_cam = cam_fps_above;
            }
    }

    /* tasta ridicata */
    void notifyKeyReleased(unsigned char key_released, int mouse_x, int mouse_y)
    {
        pressed_keys.set(key_released, 0);
    }

    /* tasta speciala (up / down / F1 / F2...) apasata */
    void notifySpecialKeyPressed(int key_pressed, int mouse_x, int mouse_y)
    {
        if (key_pressed == GLUT_KEY_F1)
            lab::glut::enterFullscreen();
        if (key_pressed == GLUT_KEY_F2)
            lab::glut::exitFullscreen();

        pressed_keys.set(key_pressed);
        is_special.set(key_pressed);
    }

    /* tasta speciala ridicata */
    void notifySpecialKeyReleased(int key_released, int mouse_x, int mouse_y)
    {
        pressed_keys.set(key_released, 0);
        is_special.set(key_released, 0);
    }

    /* drag cu mouse-ul */
    void notifyMouseDrag(int mouse_x, int mouse_y) {}
    /* am miscat mouseul (fara sa apas vreun buton) */
    void notifyMouseMove(int mouse_x, int mouse_y) {}
    /* am apasat pe un buton */
    void notifyMouseClick(int button, int state, int mouse_x, int mouse_y) {}
    /* scroll cu mouse-ul */
    void notifyMouseScroll(int wheel, int direction, int mouse_x, int mouse_y)
    {
        std::cout << "Mouse scroll" << std::endl;
    }
};

int main()
{
    /* initializeaza GLUT (fereastra + input + context OpenGL) */
    lab::glut::WindowInfo window(std::string("Tema 3 EGC"), 800, 600, 100, 50, true);
    lab::glut::ContextInfo context(3, 3, false);
    lab::glut::FramebufferInfo framebuffer(true, true, true, true);
    lab::glut::init(window, context, framebuffer);

    /* initializeaza GLEW (ne incarca functiile openGL, altfel ar trebui sa facem asta manual!) */
    glewExperimental = true;
    glewInit();
    std::cout << "GLEW:initializare" << std::endl;

    /* cream clasa noastra si o punem sa asculte evenimentele de la GLUT */
    /* DUPA GLEW!!! ca sa avem functiile de OpenGL incarcate inainte sa ii fie apelat constructorul
     * (care creeaza obiecte OpenGL) */
    Tema tema;
    lab::glut::setListener(&tema);

    /* run */
    lab::glut::run();

    return 0;
}
