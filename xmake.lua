add_rules("mode.debug", "mode.release")

set_encodings("utf-8")
set_optimize("aggressive")

local DOTNET_PATH = os.getenv("DOTNET_PATH")

target("sw2tracer")
    set_kind("shared")
    set_languages("cxx23")
    set_arch("x64")
    add_files("src/*.cpp")
    add_headerfiles("src/*.h")

    add_rules("asm")
    if is_plat("windows") then
        add_files("src/*.def")
        add_files("src/asm/windows/*")
    else
        add_files("src/asm/systemv/*")
    end

    if is_plat("windows") then
        add_defines("WIN32")
    else 
        add_defines("HOST_AMD64")
    end

    add_files("vendor/loguru/*.cpp")
    add_includedirs("vendor/loguru")

    add_includedirs(path.join(DOTNET_PATH, "src/coreclr/pal/prebuilt/inc"))
    if is_plat("linux") then 
        add_includedirs(path.join(DOTNET_PATH, "src/coreclr/pal/inc"))
        add_includedirs(path.join(DOTNET_PATH, "src/coreclr/pal/inc/rt"))
    end
    add_includedirs(path.join(DOTNET_PATH, "src/coreclr/inc"))
    add_includedirs(path.join(DOTNET_PATH, "src/coreclr"))
    add_includedirs(path.join(DOTNET_PATH, "src/native"))



