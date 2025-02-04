#pragma once

#include <EASTL/map.h>

#include <Core/StringHash.h>

#include "ActionList.h"

class ActionListManager {
public:
    using ActionListMap = eastl::map<StringHash, ActionList>;

public:
    void update(std::uint32_t dt, bool gamePaused);

    void addActionList(ActionList actionList);

    void stopActionList(StringHash actionListName);
    bool isActionListPlaying(const StringHash actionListName) const;
    const ActionList& getActionList(const StringHash actionListName) const;

    const ActionListMap& getActionLists() const { return actionLists; }

private:
    ActionListMap actionLists;
};
