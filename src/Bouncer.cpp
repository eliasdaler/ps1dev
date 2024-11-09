#include "Bouncer.h"

Bouncer::Bouncer(std::uint32_t bounceTime, int maxBounceOffset) :
    timer(bounceTime), offset(0), maxBounceOffset(maxBounceOffset), moveDown{true}
{}

void Bouncer::update()
{
    timer.update();
    if (timer.tick()) {
        if (moveDown) {
            ++offset;
            if (offset == maxBounceOffset) {
                moveDown = false;
            }
        } else {
            --offset;
            if (offset == -maxBounceOffset) {
                moveDown = true;
            }
        }
    }
}
