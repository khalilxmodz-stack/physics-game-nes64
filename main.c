// Tilt Ball - simple 3D physics game for N64 (libdragon, OpenGL + lighting)
// Stick: tilt board | C-left/right: orbit camera | A: jump | Start: reset
#include <libdragon.h>
#include <GL/gl.h>
#include <GL/gl_integration.h>
#include <GL/glu.h>
#include <math.h>

#define HALF      5.0f    // board half size
#define BALL_R    0.5f
#define G         18.0f
#define MAX_TILT  16.0f   // degrees
#define NPILLARS  4
#define NCOINS    6
#define FONT_ID   1

typedef struct { float x, z, r; } Pillar;
static const Pillar pillars[NPILLARS] = {
    {-2.0f, -1.0f, 0.7f}, {2.5f, -2.5f, 0.7f}, {0.5f, 2.0f, 0.7f}, {-3.0f, 3.0f, 0.7f}};
static const float coin_pos[NCOINS][2] = {
    {-3.5f,-3.5f},{3.5f,-3.5f},{3.8f,1.0f},{-1.0f,3.8f},{0.0f,-3.8f},{-3.8f,0.5f}};

static float bx, bz, bvx, bvz, by, bvy;   // ball (board space)
static bool  falling;
static bool  coin_got[NCOINS];
static float tiltx, tiltz;                // smoothed world tilt (deg)
static float cam_yaw;                     // radians
static float spin;
static int   score, lives;

static void reset_ball(void) {
    bx = 0; bz = 0; bvx = bvz = 0; by = 0; bvy = 0; falling = false;
}
static void reset_game(void) {
    reset_ball();
    for (int i = 0; i < NCOINS; i++) coin_got[i] = false;
    score = 0; lives = 3; tiltx = tiltz = 0;
}

static void draw_box(float sx, float sy, float sz) {
    float x = sx*0.5f, y = sy*0.5f, z = sz*0.5f;
    glBegin(GL_QUADS);
    glNormal3f(0,1,0);  glVertex3f(-x,y,-z); glVertex3f(-x,y,z);  glVertex3f(x,y,z);  glVertex3f(x,y,-z);
    glNormal3f(0,-1,0); glVertex3f(-x,-y,-z);glVertex3f(x,-y,-z); glVertex3f(x,-y,z); glVertex3f(-x,-y,z);
    glNormal3f(0,0,1);  glVertex3f(-x,-y,z); glVertex3f(x,-y,z);  glVertex3f(x,y,z);  glVertex3f(-x,y,z);
    glNormal3f(0,0,-1); glVertex3f(-x,-y,-z);glVertex3f(-x,y,-z); glVertex3f(x,y,-z); glVertex3f(x,-y,-z);
    glNormal3f(1,0,0);  glVertex3f(x,-y,-z); glVertex3f(x,y,-z);  glVertex3f(x,y,z);  glVertex3f(x,-y,z);
    glNormal3f(-1,0,0); glVertex3f(-x,-y,-z);glVertex3f(-x,-y,z); glVertex3f(-x,y,z); glVertex3f(-x,y,-z);
    glEnd();
}

static void draw_sphere(float r, int stacks, int slices) {
    for (int i = 0; i < stacks; i++) {
        float a0 = M_PI * i / stacks - M_PI/2, a1 = M_PI * (i+1) / stacks - M_PI/2;
        glBegin(GL_TRIANGLE_STRIP);
        for (int j = 0; j <= slices; j++) {
            float b = 2*M_PI * j / slices, cb = cosf(b), sb = sinf(b);
            glNormal3f(cosf(a0)*cb, sinf(a0), cosf(a0)*sb);
            glVertex3f(r*cosf(a0)*cb, r*sinf(a0), r*cosf(a0)*sb);
            glNormal3f(cosf(a1)*cb, sinf(a1), cosf(a1)*sb);
            glVertex3f(r*cosf(a1)*cb, r*sinf(a1), r*cosf(a1)*sb);
        }
        glEnd();
    }
}

static void draw_cylinder(float r, float h, int seg) {
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= seg; i++) {
        float a = 2*M_PI*i/seg, c = cosf(a), s = sinf(a);
        glNormal3f(c,0,s);
        glVertex3f(r*c, 0, r*s);
        glVertex3f(r*c, h, r*s);
    }
    glEnd();
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0,1,0); glVertex3f(0,h,0);
    for (int i = 0; i <= seg; i++) { float a = 2*M_PI*i/seg; glVertex3f(r*cosf(a), h, r*sinf(a)); }
    glEnd();
}

static void update(float dt, joypad_inputs_t in, joypad_buttons_t pressed, joypad_buttons_t held) {
    if (pressed.start) reset_game();
    if (held.c_left)  cam_yaw -= 1.6f*dt;
    if (held.c_right) cam_yaw += 1.6f*dt;

    // stick -> camera-relative world tilt
    float sx = in.stick_x / 70.0f, sy = in.stick_y / 70.0f;
    if (sx > 1) sx = 1; if (sx < -1) sx = -1;
    if (sy > 1) sy = 1; if (sy < -1) sy = -1;
    if (fabsf(sx) < 0.08f) sx = 0;
    if (fabsf(sy) < 0.08f) sy = 0;
    float cy = cosf(cam_yaw), sn = sinf(cam_yaw);
    // right = (cos,-sin), forward = (-sin,-cos) in (x,z)
    float tx = (sx*cy   + sy*(-sn)) * MAX_TILT;
    float tz = (sx*(-sn)+ sy*(-cy)) * MAX_TILT;
    float k = 1.0f - expf(-10.0f*dt);
    tiltx += (tx - tiltx) * k;
    tiltz += (tz - tiltz) * k;

    spin += 2.5f*dt;

    // ball physics
    float rx = tiltx * M_PI/180.0f, rz = tiltz * M_PI/180.0f;
    bvx += G * sinf(rx) * dt;
    bvz += G * sinf(rz) * dt;
    float damp = expf(-0.6f*dt);
    bvx *= damp; bvz *= damp;
    bx += bvx*dt; bz += bvz*dt;

    // vertical (jump / fall)
    if (!falling && by <= 0.001f && pressed.a) bvy = 6.5f;
    bvy -= 22.0f*dt;
    by += bvy*dt;
    if (!falling && by < 0) { by = 0; if (bvy < 0) bvy = 0; }

    // pillar collisions (only when ball is low)
    if (!falling && by < 1.2f) {
        for (int i = 0; i < NPILLARS; i++) {
            float dx = bx - pillars[i].x, dz = bz - pillars[i].z;
            float d = sqrtf(dx*dx + dz*dz), minD = pillars[i].r + BALL_R;
            if (d < minD && d > 0.0001f) {
                float nx = dx/d, nz = dz/d;
                bx = pillars[i].x + nx*minD; bz = pillars[i].z + nz*minD;
                float vn = bvx*nx + bvz*nz;
                if (vn < 0) { bvx -= 1.7f*vn*nx; bvz -= 1.7f*vn*nz; }
            }
        }
    }

    // coins
    for (int i = 0; i < NCOINS; i++) {
        if (coin_got[i]) continue;
        float dx = bx - coin_pos[i][0], dz = bz - coin_pos[i][1];
        if (dx*dx + dz*dz < 0.9f && by < 1.0f) { coin_got[i] = true; score += 100; }
    }
    bool all = true;
    for (int i = 0; i < NCOINS; i++) if (!coin_got[i]) all = false;
    if (all) { for (int i = 0; i < NCOINS; i++) coin_got[i] = false; score += 500; }

    // edge -> fall
    if (!falling && (fabsf(bx) > HALF || fabsf(bz) > HALF) && by < 0.05f) falling = true;
    if (falling && by < -12.0f) {
        if (--lives <= 0) { reset_game(); } else reset_ball();
    }
}

static void render(void) {
    glClearColor(0.08f, 0.1f, 0.2f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (float)display_get_width() / display_get_height();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(55.0f, aspect, 1.0f, 60.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(sinf(cam_yaw)*11.0f, 9.0f, cosf(cam_yaw)*11.0f, 0,0,0, 0,1,0);

    // light (directional, in world space)
    GLfloat lpos[] = {0.4f, 1.0f, 0.5f, 0.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lpos);

    glPushMatrix();
    glRotatef(-tiltx, 0, 0, 1);
    glRotatef( tiltz, 1, 0, 0);

    // board
    glColor3f(0.35f, 0.55f, 0.4f);
    glPushMatrix(); glTranslatef(0,-0.25f,0); draw_box(HALF*2, 0.5f, HALF*2); glPopMatrix();

    // pillars
    glColor3f(0.8f, 0.3f, 0.3f);
    for (int i = 0; i < NPILLARS; i++) {
        glPushMatrix(); glTranslatef(pillars[i].x, 0, pillars[i].z);
        draw_cylinder(pillars[i].r, 1.2f, 12); glPopMatrix();
    }

    // coins
    glColor3f(1.0f, 0.85f, 0.15f);
    for (int i = 0; i < NCOINS; i++) {
        if (coin_got[i]) continue;
        glPushMatrix(); glTranslatef(coin_pos[i][0], 0.7f, coin_pos[i][1]);
        glRotatef(spin*57.3f, 0, 1, 0); draw_box(0.5f, 0.5f, 0.12f); glPopMatrix();
    }

    // ball
    glColor3f(0.95f, 0.95f, 1.0f);
    glPushMatrix(); glTranslatef(bx, BALL_R + by, bz); draw_sphere(BALL_R, 8, 12); glPopMatrix();

    glPopMatrix();
}

int main(void) {
    debug_init_isviewer();
    debug_init_usblog();
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE_ANTIALIAS_DEDITHER);
    rdpq_init();
    gl_init();
    joypad_init();

    surface_t zbuf = surface_alloc(FMT_RGBA16, display_get_width(), display_get_height());
    rdpq_font_t *font = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO);
    rdpq_text_register_font(FONT_ID, font);

    // GL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_NORMALIZE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glShadeModel(GL_SMOOTH);
    GLfloat amb[]  = {0.25f, 0.25f, 0.3f, 1};
    GLfloat diff[] = {1.0f, 0.95f, 0.85f, 1};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diff);

    reset_game();

    while (1) {
        joypad_poll();
        joypad_inputs_t in = joypad_get_inputs(JOYPAD_PORT_1);
        joypad_buttons_t pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);
        joypad_buttons_t held = joypad_get_buttons_held(JOYPAD_PORT_1);

        float dt = display_get_delta_time();
        if (dt > 0.05f) dt = 0.05f;
        update(dt, in, pressed, held);

        surface_t *disp = display_get();
        rdpq_attach(disp, &zbuf);
        gl_context_begin();
        render();
        gl_context_end();

        rdpq_text_printf(&(rdpq_textparms_t){0}, FONT_ID, 16, 20, "SCORE %d   LIVES %d", score, lives);
        rdpq_text_printf(&(rdpq_textparms_t){0}, FONT_ID, 16, 226, "Stick:tilt  A:jump  C<>:cam  START:reset");
        rdpq_detach_show();
    }
}
