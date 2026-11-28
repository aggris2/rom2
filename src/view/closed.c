#include "view/closed.h"

#include <stddef.h>

void closed_marks(const Terrain *terrain, uint8_t whom, uint8_t *marks)
{
    size_t squares = (size_t)terrain->width * terrain->height;
    for (size_t i = 0; i < squares; i++) {
        marks[i] = (terrain->closed[i] & whom) != 0 ? CLOSED_MARK : 0;
    }
}
