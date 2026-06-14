#pragma once

#include <JuceHeader.h>

namespace NovaStudioUI
{
    struct AnimatedValue
    {
        float value = 0.0f;
        float target = 0.0f;
        float speed = 0.18f;

        void setTarget(float newTarget) noexcept
        {
            target = newTarget;
        }

        bool update() noexcept
        {
            const float delta = target - value;
            if (std::abs(delta) < 0.001f)
            {
                value = target;
                return false;
            }
            value += delta * speed;
            return true;
        }
    };
}
