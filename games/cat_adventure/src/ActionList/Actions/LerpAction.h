#pragma once

#include <EASTL/functional.h>
#include <EASTL/string.h>

#include <ActionList/Action.h>

#include <psyqo/fixed-point.hh>

// LerpAction is used for lerping stuff for some amount of time
// It passes a lerpFactor into the callback function
class LerpAction : public Action {
public:
    using LerpFuncType = eastl::function<void(psyqo::FixedPoint<> dt)>;

    LerpAction(LerpFuncType f, psyqo::FixedPoint<> lerpTime);

    bool enter() override;
    void update(std::uint32_t dt) override;
    bool isFinished() const override;

private:
    LerpFuncType f;
    std::uint32_t currentTime{0};
    std::uint32_t lerpDurationMicroseconds{0};
};
