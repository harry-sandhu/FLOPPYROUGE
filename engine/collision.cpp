#include "collision.h"

namespace Collision {

bool CheckAABB(const Rect& a, const Rect& b) {
    return a.Intersects(b);
}

} // namespace Collision