#pragma once

#include <EASTL/functional.h>
#include <EASTL/string.h>

#include <ActionList/Action.h>

// WaitWhileAction is used for waiting for some condition to become false
// example:
//
//    int someInt = 0;
//    return doList(ActionList(
//        "do stuff",
//        // will execute this 10 times
//        waitWhile([someInt](float dt) {
//            ++someInt;
//            return someInt < 10; //
//        })));
//
class WaitWhileAction : public Action {
public:
    using ConditionFuncType = eastl::function<bool(std::uint32_t dt)>;

    WaitWhileAction(ConditionFuncType f);
    WaitWhileAction(eastl::string conditionName, ConditionFuncType f);

    bool enter() override;
    void update(std::uint32_t dt) override;
    bool isFinished() const override;

    const eastl::string& getConditionName() const { return conditionName; }

private:
    ConditionFuncType f;
    eastl::string conditionName;
    bool finished{false};
};
