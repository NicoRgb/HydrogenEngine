#pragma once
#include <string>

class Panel
{
public:
    virtual ~Panel() = default;

    virtual void OnImGuiRender() = 0;
    virtual const char* GetName() const = 0;
};
