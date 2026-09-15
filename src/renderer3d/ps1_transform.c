#include "renderer3d/ps1_transform.h"

void ps1_transform_model(const Ps1Transform* t, Ps1Mat4* model,
                         Ps1Mat4* normal) {
    ps1_matrix_model(&t->position, &t->rotation, &t->scale, model, normal);
}