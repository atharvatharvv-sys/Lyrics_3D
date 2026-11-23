#pragma once
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct LyricLine {
    double      timeSec = 0.0;
    std::string text;
};

struct SongEntry {
    std::string            title;
    fs::path               mp3Path;
    fs::path               lrcPath;
    std::vector<LyricLine> lyrics;
};

class SongLibrary {
public:
    std::vector<SongEntry> songs;
    int                    activeIndex = -1;

    static fs::path resolveSongsDir() {
        const std::vector<fs::path> candidates = {
            "/storage/emulated/0/games/org.levimc/minecraft/mods/Lyrics 3D/data/songs",
            "/storage/emulated/0/Android/data/org.levimc.launcher/files/mods/Lyrics 3D/data/songs",
            "/storage/emulated/0/games/com.mojang/mods/Lyrics 3D/data/songs"
        };
        for (const auto& dir : candidates) {
            std::error_code ec;
            if (fs::exists(dir, ec) && fs::is_directory(dir, ec)) return dir;
        }
        return candidates.front();
    }

    void scan() {
        songs.clear();
        fs::path dir = resolveSongsDir();
        std::error_code ec;
        if (!fs::exists(dir, ec)) return;

        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".mp3") continue;

            fs::path lrc = entry.path();
            lrc.replace_extension(".lrc");
            if (!fs::exists(lrc, ec)) continue;

            SongEntry song;
            song.title   = entry.path().stem().string();
            song.mp3Path = entry.path();
            song.lrcPath = lrc;
            song.lyrics  = parseLrcFile(lrc);
            songs.push_back(std::move(song));
        }

        std::sort(songs.begin(), songs.end(), [](const SongEntry& a, const SongEntry& b) {
            return a.title < b.title;
        });
    }

    std::pair<std::string, std::string> getLinesAt(double seconds) const {
        if (activeIndex < 0 || activeIndex >= static_cast<int>(songs.size())) return {"", ""};
        const auto& lines = songs[activeIndex].lyrics;
        if (lines.empty()) return {"", ""};

        int current = -1;
        for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
            if (seconds >= lines[i].timeSec) current = i;
            else break;
        }

        if (current < 0) return {"", lines[0].text};
        std::string curText  = lines[current].text;
        std::string nextText = (current + 1 < static_cast<int>(lines.size())) ? lines[current + 1].text : "";
        return {curText, nextText};
    }

private:
    static std::vector<LyricLine> parseLrcFile(const fs::path& path) {
        std::vector<LyricLine> result;
        std::ifstream file(path);
        std::string line;

        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            size_t close = line.find(']');
            int min = 0;
            double sec = 0.0;
            if (close != std::string::npos && std::sscanf(line.c_str(), "[%d:%lf]", &min, &sec) == 2) {
                std::string lyric = line.substr(close + 1);
                if (!lyric.empty()) {
                    result.push_back({min * 60.0 + sec, lyric});
                }
            }
        }
        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            return a.timeSec < b.timeSec;
        });
        return result;
    }
};
