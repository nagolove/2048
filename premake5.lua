workspace "ray_example"
    configurations { "Debug", "Release" }

    defines{"GRAPHICS_API_OPENGL_43"}
    includedirs { 
    }
    buildoptions { 
        "-ggdb3",
        "-fPIC",
        "-Wall",
    }
    links { 
        "m",
        "raylib",
    }
    libdirs { 
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

