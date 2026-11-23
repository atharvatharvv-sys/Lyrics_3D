#include "AudioPlayerJni.hpp"
#include "Nametag3DRenderer.hpp"
#include "SongLibrary.hpp"

#include <pl/hook/Hook.h>
#include <pl/memory/Signature.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>

namespace {
constexpr std::size_t kLevelRendererPlayerOffset = 0x430;
constexpr std::size_t kCamPosOffset              = 0x614;

constexpr const char* kRenderLevelSig =
    "? ? ? D1 ? ? ? FD ? ? ? 6D ? ? ? 6D ? ? ? 6D ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? 91 57 D0 3B D5";
constexpr const char* kGetFovSig =
    "? ? ? FC ? ? ? 6D ? ? ? A9 ? ? ? F9 ? ? ? A9 ? ? ? 91 08 40 20 1E F4 03 01 2A";

using RenderLevelFn    = void (*)(void* levelRenderer, void* screenContext, void* frameRenderObj);
using GetFovFn         = float (*)(void* levelRendererPlayer, float a, bool b);
using EglSwapBuffersFn = EGLBoolean (*)(EGLDisplay, EGLSurface);

RenderLevelFn    g_renderLevelOriginal = nullptr;
GetFovFn         g_getFov              = nullptr;
EglSwapBuffersFn g_swapBuffersOriginal = nullptr;

std::atomic<glm::vec3> g_camPos{glm::vec3(0.0f, 70.0f, 0.0f)};
std::atomic<glm::vec3> g_camForward{glm::vec3(0.0f, 0.0f, 1.0f)};
std::atomic<float>     g_fov{70.0f};
std::atomic<bool>      g_inWorld{false};

SongLibrary       g_library;
AudioPlayerJni    g_audio;
Nametag3DRenderer g_nametag;
bool              g_imguiReady = false;
bool              g_menuOpen   = false;
auto              g_startTime  = std::chrono::steady_clock::now();

void renderLevelDetour(void* levelRenderer, void* screenContext, void* frameRenderObj) {
    if (levelRenderer) {
        auto* lrPlayer = *reinterpret_cast<std::byte**>(
            reinterpret_cast<std::byte*>(levelRenderer) + kLevelRendererPlayerOffset
        );
        if (lrPlayer) {
            glm::vec3 pos{};
            std::memcpy(&pos, lrPlayer + kCamPosOffset, sizeof(glm::vec3));

            glm::vec3 prevPos = g_camPos.load(std::memory_order_relaxed);
            glm::vec3 delta   = pos - prevPos;
            if (glm::length(delta) > 0.001f && glm::length(delta) < 5.0f) {
                g_camForward.store(glm::normalize(glm::vec3(delta.x, 0.0f, delta.z)), std::memory_order_relaxed);
            }

            g_camPos.store(pos, std::memory_order_relaxed);
            if (g_getFov) {
                float fov = g_getFov(lrPlayer, 1.0f, true);
                if (fov > 10.0f && fov < 170.0f) g_fov.store(fov, std::memory_order_relaxed);
            }
            g_inWorld.store(true, std::memory_order_relaxed);
        }
    }
    if (g_renderLevelOriginal) g_renderLevelOriginal(levelRenderer, screenContext, frameRenderObj);
}

EGLBoolean swapBuffersDetour(EGLDisplay dpy, EGLSurface surface) {
    if (eglGetCurrentContext() != EGL_NO_CONTEXT) {
        EGLint w = 0, h = 0;
        eglQuerySurface(dpy, surface, EGL_WIDTH, &w);
        eglQuerySurface(dpy, surface, EGL_HEIGHT, &h);

        if (!g_imguiReady && w > 0 && h > 0) {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGui::StyleColorsDark();
            ImGui_ImplOpenGL3_Init("#version 300 es");
            g_library.scan();
            g_imguiReady = true;
        }

        if (g_imguiReady && w > 0 && h > 0) {
            ImGuiIO& io    = ImGui::GetIO();
            io.DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));

            ImGui_ImplOpenGL3_NewFrame();
            ImGui::NewFrame();

            float nowSec = std::chrono::duration<float>(std::chrono::steady_clock::now() - g_startTime).count();

            if (g_inWorld.load(std::memory_order_relaxed) && g_library.activeIndex >= 0) {
                double songPos = g_audio.getPositionSeconds();
                auto [curLine, nextLine] = g_library.getLinesAt(songPos);
                g_nametag.drawInWorld(
                    curLine,
                    nextLine,
                    g_camPos.load(std::memory_order_relaxed),
                    g_camForward.load(std::memory_order_relaxed),
                    glm::vec3(0.0f, 1.0f, 0.0f),
                    g_fov.load(std::memory_order_relaxed),
                    nowSec
                );
            }

            ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
            ImGui::Begin("Lyrics 3D Nametags", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
            if (ImGui::Button(g_menuOpen ? "Hide Controls" : "Lyrics 3D")) {
                g_menuOpen = !g_menuOpen;
            }
            if (g_menuOpen) {
                if (ImGui::Button("Rescan Songs")) g_library.scan();
                ImGui::SameLine();
                if (ImGui::Button("Pause / Play")) g_audio.togglePause();

                if (ImGui::Button("Pin Nametag Here")) {
                    g_nametag.pinAt(g_camPos.load(), g_camForward.load());
                }
                ImGui::SameLine();
                if (ImGui::Button("Follow Player")) {
                    g_nametag.settings.pinnedInWorld = false;
                }

                ImGui::SliderFloat("Distance", &g_nametag.settings.distance, 2.0f, 24.0f);
                ImGui::SliderFloat("Height", &g_nametag.settings.heightOffset, -2.0f, 6.0f);
                ImGui::SliderFloat("Nametag Scale", &g_nametag.settings.scale, 0.4f, 2.5f);
                ImGui::Checkbox("Show Next Line", &g_nametag.settings.showNextLine);

                ImGui::Separator();
                for (int i = 0; i < static_cast<int>(g_library.songs.size()); ++i) {
                    if (ImGui::Selectable(g_library.songs[i].title.c_str(), g_library.activeIndex == i)) {
                        g_library.activeIndex = i;
                        g_audio.playFile(g_library.songs[i].mp3Path.string());
                    }
                }
            }
            ImGui::End();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
    }
    return g_swapBuffersOriginal ? g_swapBuffersOriginal(dpy, surface) : EGL_FALSE;
}

void installHooks() {
    auto resolved = pl::memory::resolveSignatures({kRenderLevelSig, kGetFovSig}, "libminecraftpe.so");
    if (auto it = resolved.find(kRenderLevelSig); it != resolved.end() && it->second) {
        pl::hook::pl_hook(
            reinterpret_cast<void*>(it->second),
            reinterpret_cast<void*>(renderLevelDetour),
            reinterpret_cast<void**>(&g_renderLevelOriginal),
            pl::hook::PriorityNormal
        );
    }
    if (auto it = resolved.find(kGetFovSig); it != resolved.end() && it->second) {
        g_getFov = reinterpret_cast<GetFovFn>(it->second);
    }

    if (void* swapAddr = reinterpret_cast<void*>(eglSwapBuffers)) {
        pl::hook::pl_hook(
            swapAddr,
            reinterpret_cast<void*>(swapBuffersDetour),
            reinterpret_cast<void**>(&g_swapBuffersOriginal),
            pl::hook::PriorityNormal
        );
    }
}
} // namespace

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
    g_audio.init(vm);
    installHooks();
    return JNI_VERSION_1_6;
}
