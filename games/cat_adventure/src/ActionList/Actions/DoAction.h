#pragma once

#include <EASTL/functional.h>
#include <EASTL/string.h>

#include <ActionList/Action.h>

class DoAction : public Action {
public:
    DoAction(eastl::function<void()> f);
    DoAction(eastl::string name, eastl::function<void()> f);

    bool enter() override;
    bool isFinished() const override { return true; }

    const eastl::string& getName() const { return name; }

private:
    eastl::string name;
    eastl::function<void()> f;
};
