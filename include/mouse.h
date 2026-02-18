#pragma once

class MouseClass {
public:
    void Init();
    void Read(float &x, float &y);
    void Update();
    bool ReadButton();
};

extern MouseClass Mouse;
