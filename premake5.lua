workspace "ray_example"
    configurations { "Debug", "Release" }

    defines{"GRAPHICS_API_OPENGL_43"}
    includedirs { 
        "Chipmunk2d/include/",
        "Chipmunk2d/include/include",
        "/home/nagolove/proekt80/src"
    }
    buildoptions { 
        "-ggdb3",
        "-fPIC",
        "-Wall",
        --"-pedantic",
    }
    links { 
        "m",
        "raylib",
        "chipmunk",
        "/home/nagolove/proekt80/obj/Debug/t80/table.o",
    }
    libdirs { 
        "Chipmunk2d/src/",
    }
    language "C"
    kind "ConsoleApp"
    targetprefix ""
    targetdir "."
    symbols "On"

    project "2048"
        buildoptions { 
            "-ggdb3",
        }
        files { 
            "./*.h", 
            "./*.c",
            "/home/nagolove/proekt80/src/table.c",
        }
        linkoptions {
            "-fsanitize=address",
            "-fsanitize-recover=address",
        }
        buildoptions { 
            "-fsanitize=address",
            "-fsanitize-recover=address",
        }

    
    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"
        buildoptions { 
            "-ggdb3"
        }
        linkoptions {
            "-fsanitize=address",
        }
        buildoptions { 
            "-fsanitize=address",
        }

    filter "configurations:Release"
        --buildoptions { 
            --"-O2"
        --}
        --symbols "On"
        --defines { "NDEBUG" }
        --optimize "On"

