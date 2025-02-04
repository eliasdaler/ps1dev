#pragma once

#include <ActionList/Action.h>

#include <psyqo/fixed-point.hh>

#include <Camera.h>

class CameraMoveAction : public Action {
public:
    CameraMoveAction(Camera& camera,
        const CameraTransform& targetTransform,
        psyqo::FixedPoint<> time);

    bool enter() override;
    void update(std::uint32_t dt) override;
    bool isFinished() const;

private:
    Camera* cameraPtr;
    CameraTransform start;
    CameraTransform end;

    std::uint32_t currentTime{0};
    std::uint32_t moveDurationMicroseconds{0};
};
