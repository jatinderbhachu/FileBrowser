add_rules("mode.debug", "mode.release")

-- switch to debug mode using:
-- xmake f -m debug


target("main")
    set_languages("c++17")
    set_kind("binary")

    add_linkdirs("C:/dev/libs/glfw/libs/src/Release")
    add_includedirs(
        "./src",
        "src/glad/include",
        "third_party/imgui",
        "third_party/IconFontCppHeaders",
        "C:/dev/libs/glfw/include"
        )

    if is_mode("debug") then
      set_symbols("debug")
      add_cxflags("/EHsc", "/Zi", "/MTd", "/DEBUG:FULL")
      add_ldflags("/LTCG")
    else
      add_cxflags("/EHsc", "/MT")
      add_ldflags("/LTCG")
      set_optimize("fastest")
    end

    add_ldflags("/SUBSYSTEM:CONSOLE")

    add_links("glfw3", "user32", "gdi32", "shlwapi", "shell32", "Ole32")

    add_files(
        "src/glad/**.c",
        "src/**.cpp",
        "third_party/imgui/imgui.cpp",
        "third_party/imgui/imgui_draw.cpp",
        "third_party/imgui/imgui_demo.cpp",
        "third_party/imgui/imgui_widgets.cpp",
        "third_party/imgui/imgui_tables.cpp",
        "third_party/imgui/misc/cpp/imgui_stdlib.cpp",
        "third_party/imgui/backends/imgui_impl_glfw.cpp",
        "third_party/imgui/backends/imgui_impl_opengl3.cpp"
    )
    add_deps("copy_fonts")


target("copy_fonts")
    on_build(function(target)
        os.trycp("$(projectdir)/third_party/Fork-Awesome/fonts/forkawesome-webfont.ttf", "$(buildir)/windows/x64/debug/forkawesome-webfont.ttf")
        os.trycp("$(projectdir)/third_party/Fork-Awesome/fonts/forkawesome-webfont.ttf", "$(buildir)/windows/x64/release/forkawesome-webfont.ttf")
    end)


target("tests")
    set_languages("c++17")
    set_kind("binary")

    add_includedirs(
        "C:/dev/libs/catch2",
        "./src"
        )

    if is_mode("debug") then
      set_symbols("debug")
      add_cxflags("/EHsc", "/Zi", "/MTd", "/DEBUG:FULL")
      add_ldflags("/LTCG")
    else
      add_cxflags("/EHsc", "/MT")
      add_ldflags("/LTCG")
      set_optimize("fastest")
    end

    add_ldflags("/SUBSYSTEM:CONSOLE")

    add_links("user32", "gdi32", "shell32", "Ole32")

    add_files(
        "src/filebrowser.cpp",
        "src/string_util.cpp",
        "tests/main.cpp",
        "C:/dev/libs/catch2/catch_amalgamated.cpp"
    )


--
-- If you want to known more usage about xmake, please see https://xmake.io
--
-- ## FAQ
--
-- You can enter the project directory firstly before building project.
--
--   $ cd projectdir
--
-- 1. How to build project?
--
--   $ xmake
--
-- 2. How to configure project?
--
--   $ xmake f -p [macosx|linux|iphoneos ..] -a [x86_64|i386|arm64 ..] -m [debug|release]
--
-- 3. Where is the build output directory?
--
--   The default output directory is `./build` and you can configure the output directory.
--
--   $ xmake f -o outputdir
--   $ xmake
--
-- 4. How to run and debug target after building project?
--
--   $ xmake run [targetname]
--   $ xmake run -d [targetname]
--
-- 5. How to install target to the system directory or other output directory?
--
--   $ xmake install
--   $ xmake install -o installdir
--
-- 6. Add some frequently-used compilation flags in xmake.lua
--
-- @code
--    -- add debug and release modes
--    add_rules("mode.debug", "mode.release")
--
--    -- add macro defination
--    add_defines("NDEBUG", "_GNU_SOURCE=1")
--
--    -- set warning all as error
--    set_warnings("all", "error")
--
--    -- set language: c99, c++11
--    set_languages("c99", "c++11")
--
--    -- set optimization: none, faster, fastest, smallest
--    set_optimize("fastest")
--
--    -- add include search directories
--    add_includedirs("/usr/include", "/usr/local/include")
--
--    -- add link libraries and search directories
--    add_links("tbox")
--    add_linkdirs("/usr/local/lib", "/usr/lib")
--
--    -- add system link libraries
--    add_syslinks("z", "pthread")
--
--    -- add compilation and link flags
--    add_cxflags("-stdnolib", "-fno-strict-aliasing")
--    add_ldflags("-L/usr/local/lib", "-lpthread", {force = true})
--
-- @endcode
--

