#pragma once

#include <EASTL/functional.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>

#include <Core/StringHash.h>

#include "Action.h"

// ActionList is a list of actions which are performed one after another.
// This pattern allows to easily to multi-frame actions in sequence without
// writing state machines or "hasActionCompleted" booleans.
class ActionList {
public:
    ActionList();
    ActionList(StringHash name);
    ActionList(StringHash name, eastl::vector<eastl::unique_ptr<Action>> actions);

    // Move only (because we want to move actions, not copy them)
    ActionList(ActionList&&) = default;
    ActionList& operator=(ActionList&&) = default;
    ActionList(const ActionList&) = delete;
    ActionList& operator=(const ActionList&) = delete;

    template<typename... Args>
    ActionList(StringHash name, Args&&... args);

    template<typename... Args>
    void addActions(Args&&... args);

    void addAction(eastl::unique_ptr<Action> action);
    void addAction(eastl::function<void()> action);
    void addAction(ActionList actionList);

    void play(); // starts playback of an action list from the beginning

    void setName(StringHash n) { name = n; }
    StringHash getName() const { return name; }

    void update(std::uint32_t dt);
    bool isFinished() const;

    void setLooping(bool b) { looping = b; }
    bool isLooping() const { return looping; }

    bool isEmpty() { return actions.empty(); }

    void setRunWhenGameIsPaused(bool b) { runWhenGameIsPaused = b; }
    bool shouldRunWhenGameIsPaused() const { return runWhenGameIsPaused; }

    const eastl::vector<eastl::unique_ptr<Action>>& getActions() const { return actions; }
    std::size_t getCurrentActionIndex() const { return currentIdx; }

    bool isPlaying() const { return !isFinished() && wasPlayCalled; }

    void enableParallelActionsMode() { onAddSetParallel = true; }
    void disableParallelActionsMode() { onAddSetParallel = false; }

private:
    bool runWhenGameIsPaused{false};
    bool onAddSetParallel{false}; // when true - addAction will mark actions as parallel

    void updateParallelActions(std::uint32_t dt);
    void goToNextAction();
    void advanceIndex();

    StringHash name;
    eastl::vector<eastl::unique_ptr<Action>> actions;
    std::size_t currentIdx{0};
    bool stopped{true}; // if true, goToNextAction() will start with the first action
    bool wasPlayCalled{false};
    bool looping{false};
};

template<typename... Args>
ActionList::ActionList(StringHash name, Args&&... args) : name(name)
{
    actions.reserve(sizeof...(Args));
    addActions(eastl::forward<Args>(args)...);
}

template<typename... Args>
void ActionList::addActions(Args&&... args)
{
    (addAction(eastl::move(args)), ...);
}
