#pragma once

#include <EASTL/map.h>
#include <EASTL/string.h>

#include "ActionList.h"

class ActionListManager {
public:
    using ActionListMap = eastl::map<eastl::string, ActionList*>;

public:
    void update(std::uint32_t dt, bool gamePaused);

    void addActionList(ActionList& actionList);

    void stopActionList(const eastl::string& actionListName);
    bool isActionListPlaying(const eastl::string& actionListName) const;

    const ActionListMap& getActionLists() const { return actionLists; }

private:
    ActionListMap actionLists;
};
