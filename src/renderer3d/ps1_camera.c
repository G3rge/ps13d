#include "renderer3d/ps1_camera.h"
#include "core/ps1_lut.h"

void ps1_cam_init(Ps1Camera* cam) {
    cam->pos = fv3(fx_int(0), fx_int(0), fx_int(6));
    cam->yaw = 0;
    cam->pitch = 0;
    cam->focal = fx_int(208);
    cam->near_z = fx_div(fx_int(1), fx_int(16));
    cam->far_z = fx_int(64);
    ps1_cam_update(cam);
}

void ps1_cam_update(Ps1Camera* cam) {
    fixed_t cy = ps1_cos(cam->yaw);
    fixed_t sy = ps1_sin(cam->yaw);
    fixed_t cp = ps1_cos(cam->pitch);
    fixed_t sp = ps1_sin(cam->pitch);

    {
        FVec3 r = fv3(-cy, 0, sy);
        FVec3 u = fv3(fx_mul(-sy, sp), cp, fx_mul(-cy, sp));
        FVec3 f = fv3(fx_mul(cp, sy), sp, fx_mul(cp, cy));
        Ps1Mat4* m = &cam->view;

        m->m[0][0] = r.x; m->m[0][1] = r.y; m->m[0][2] = r.z;
        m->m[0][3] = -fv3_dot(&r, &cam->pos);
        m->m[1][0] = u.x; m->m[1][1] = u.y; m->m[1][2] = u.z;
        m->m[1][3] = -fv3_dot(&u, &cam->pos);
        m->m[2][0] = f.x; m->m[2][1] = f.y; m->m[2][2] = f.z;
        m->m[2][3] = -fv3_dot(&f, &cam->pos);
        m->m[3][0] = 0; m->m[3][1] = 0; m->m[3][2] = 0; m->m[3][3] = FX_ONE;
    }
}