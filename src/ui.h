#pragma once

struct UiWindow {
    std::string caption = "-NO-CAPTION-";
    bool open = true;
    std::function<void()> contents = nullptr;
};