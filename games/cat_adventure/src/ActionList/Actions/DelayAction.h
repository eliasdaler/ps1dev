#pragma once

#include <ActionList/Action.h>

class DelayAction : public Action {
public:
    DelayAction(std::uint32_t delayDurationSeconds);

    bool enter() override;
    void update(std::uint32_t dt) override;
    bool isFinished() const override;

    std::uint32_t getCurrentTime() const { return currentTime; }
    std::uint32_t getDelayMicroseconds() const { return delayDurationMicroseconds; }

private:
    std::uint32_t currentTime{0};
    std::uint32_t delayDurationMicroseconds{0};
};
