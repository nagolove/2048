local sanit = false
local inspect = require 'inspect'
local caustic = loadfile("../caustic/caustic.lua")()
--print('caustic', inspect(caustic))

workspace "ray_example"
    configurations { "Debug", "Release" }

    defines{"GRAPHICS_API_OPENGL_43"}
    includedirs { 
        "Chipmunk2d/include/",
        "Chipmunk2d/include/include",
        "../caustic/src",
    }
    buildoptions { 
        "-ggdb3",
        "-fPIC",
        "-Wall",
        --"-pedantic",
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
        print(inspect((caustic.links)))
        links(caustic.links)
        links('caustic:static')
        libdirs(caustic.libdirs)
        print(inspect(caustic.libdirs))
        --libdirs { "Chipmunk2d/src/", "../caustic/", }
        if sanit then
            linkoptions {
                "-fsanitize=address",
                "-fsanitize-recover=address",
            }
            buildoptions { 
                "-fsanitize=address",
                "-fsanitize-recover=address",
            }
        end

    
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

