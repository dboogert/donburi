add_executable(tool main.cpp
        imgui_impl_sdl_gl3.cpp
        LogReader.cpp
        imgui-1.53/imgui.cpp
        imgui-1.53/imgui_draw.cpp
        imgui-1.53/imgui_demo.cpp)

target_include_directories(tool PUBLIC imgui-1.53
        /home/don/dev/sdl-install/include/SDL2
        /home/don/dev/glew/out/include)
target_compile_definitions(tool PUBLIC _REENTRANT)

target_link_libraries(tool
        /home/don/dev/glew/out/lib/libGLEW.a
        /home/don/dev/sdl-install/lib/libSDL2.so
        GL
        )