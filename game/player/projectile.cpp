#include "projectile.h"

// Currently just a data holder — update logic lives in projectile_system
// since it needs to interact with enemies/collision. Kept as its own file
// so it's easy to extend later (piercing, homing, etc. per-projectile flags).