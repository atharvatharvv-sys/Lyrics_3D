#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <string>

struct NametagSettings {
    bool      pinnedInWorld = false;
    glm::vec3 worldAnchor   = {0.0f, 70.0f, 0.0f};
    float     distance      = 6.0f;
    float     heightOffset  = 0.8f;
    float     scale         = 1.0f;
    float     swayAmount    = 0.08f;
    float     bgAlpha       = 0.35f;
    bool      showNextLine  = true;
};

class Nametag3DRenderer {
public:
    NametagSettings settings;

    void pinAt(const glm::vec3& camPos, const glm::vec3& forwardDir) {
        settings.worldAnchor   = camPos + glm::normalize(forwardDir) * settings.distance + glm::vec3(0.0f, settings.heightOffset, 0.0f);
        settings.pinnedInWorld = true;
    }

    void drawInWorld(
        const std::string& currentLine,
        const std::string& nextLine,
        const glm::vec3&   camPos,
        const glm::vec3&   camForward,
        const glm::vec3&   camUp,
        float              fovDegrees,
        float              timeSec
    ) {
        if (currentLine.empty()) return;

        glm::vec3 fwd = glm::normalize(camForward);
        glm::vec3 anchor = settings.pinnedInWorld
            ? settings.worldAnchor
            : (camPos + fwd * settings.distance + glm::vec3(0.0f, settings.heightOffset, 0.0f));

        anchor.y += std::sin(timeSec * 2.0f) * settings.swayAmount;

        float dist = glm::distance(camPos, anchor);
        if (dist < 0.5f || dist > 64.0f) return;

        ImVec2 display = ImGui::GetIO().DisplaySize;
        if (display.x <= 0.0f || display.y <= 0.0f) return;

        glm::mat4 view = glm::lookAt(camPos, camPos + fwd, camUp);
        glm::mat4 proj = glm::perspective(glm::radians(fovDegrees), display.x / display.y, 0.05f, 256.0f);
        glm::vec4 clip = proj * view * glm::vec4(anchor, 1.0f);

        if (clip.w <= 0.05f) return;

        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        float screenX = (display.x * 0.5f) * (1.0f + ndc.x);
        float screenY = (display.y * 0.5f) * (1.0f - ndc.y);

        ImFont* font     = ImGui::GetFont();
        float   fontSize = std::clamp((140.0f / dist) * settings.scale, 11.0f, 54.0f);
        float   padX     = fontSize * 0.35f;
        float   padY     = fontSize * 0.16f;

        ImVec2 mainSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, currentLine.c_str());
        ImDrawList* draw = ImGui::GetBackgroundDrawList();

        ImVec2 boxMin(screenX - mainSize.x * 0.5f - padX, screenY - mainSize.y * 0.5f - padY);
        ImVec2 boxMax(screenX + mainSize.x * 0.5f + padX, screenY + mainSize.y * 0.5f + padY);
        ImU32  plateCol = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, settings.bgAlpha));

        draw->AddRectFilled(boxMin, boxMax, plateCol, 2.0f);
        draw->AddText(font, fontSize, ImVec2(screenX - mainSize.x * 0.5f, screenY - mainSize.y * 0.5f), IM_COL32(255, 255, 255, 255), currentLine.c_str());

        if (settings.showNextLine && !nextLine.empty()) {
            float  subSizePx = fontSize * 0.72f;
            ImVec2 subSize   = font->CalcTextSizeA(subSizePx, FLT_MAX, 0.0f, nextLine.c_str());
            float  subTop    = boxMax.y + (fontSize * 0.14f);

            ImVec2 subMin(screenX - subSize.x * 0.5f - padX * 0.8f, subTop);
            ImVec2 subMax(screenX + subSize.x * 0.5f + padX * 0.8f, subTop + subSize.y + padY * 1.4f);

            draw->AddRectFilled(subMin, subMax, ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, settings.bgAlpha * 0.85f)), 2.0f);
            draw->AddText(font, subSizePx, ImVec2(screenX - subSize.x * 0.5f, subTop + padY * 0.7f), IM_COL32(185, 185, 185, 225), nextLine.c_str());
        }
    }
};
