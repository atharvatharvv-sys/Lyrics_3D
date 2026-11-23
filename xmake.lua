add_rules("mode.debug", "mode.release")

package("preloader")
    set_homepage("https://github.com/LiteLDev/preloader-android")
    set_description("Preloader Android")
    add_urls("https://github.com/LiteLDev/preloader-android.git")
    add_versions("main", "main")
    add_deps("cmake")
    on_install("android", function (package)
        import("package.tools.cmake").install(package)
    end)
package_end()

add_requires("preloader")
add_requires("imgui v1.91.5", {configs = {opengl3 = true, android = true}})
add_requires("glm")

target("lyrics3d")
    set_kind("shared")
    set_languages("c++20")
    set_strip("all")
    add_files("src/*.cpp")
    add_includedirs("src")
    add_packages("preloader", "imgui", "glm")

    if is_plat("android") then
        add_cxflags("-fPIC", "-O2", "-ffunction-sections", "-fdata-sections", "-fexceptions", "-w")
        add_cxxflags("-fno-rtti")
        add_shflags("-Wl,--gc-sections", "-Wl,--hash-style=gnu", "-Wl,-z,max-page-size=16384")
        add_links("android", "log", "EGL", "GLESv3")
    end

    after_build(function (target)
        if not target:is_plat("android") then return end
        import("lib.detect.find_tool")
        local python = find_tool("python3") or find_tool("python")
        assert(python, "Python 3 is required to package lyrics3d.levipack")
        os.vrunv(python.program, {
            path.join(os.projectdir(), "scripts", "package_levipack.py"),
            "--library", target:targetfile(),
            "--output", path.join(target:targetdir(), "lyrics3d.levipack")
        })
    end)
