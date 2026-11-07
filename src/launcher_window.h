#pragma once
#include <cstdint>
#include <functional>

namespace openshao::launcher {
// A native window that owns a Dear ImGui context for the launcher.
// Windows renders it with Direct3D 12; Linux uses SDL and Vulkan.
class Window {
public:
    Window();
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // `prepare` adds fonts and styles after the context exists and before the renderer uploads them.
    // It receives the display scale of the window.
    bool Open(const char* title, int width, int height, const std::function<void(float scale)>& prepare);
    // Processes events and starts an ImGui frame. Returns false after the window's close button.
    // With `animate`, the window redraws at display rate; otherwise it waits for input.
    bool BeginFrame(bool animate);
    void EndFrame();
    void Close();
    float Scale() const;

    // An image for the interface from raw RGBA, top row first, as the handle that ImGui::Image takes (cast to ImTextureID).
    // 0 when the renderer cannot make one, and the caller draws something else.
    uint64_t CreateImage(const uint8_t* rgba, int width, int height);
    void DestroyImage(uint64_t image);

    struct State;
private:
    State* s_;
};
}
