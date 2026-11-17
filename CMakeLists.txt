cmake_minimum_required(VERSION 3.28)
project(Helix.UI LANGUAGES CXX)

if(NOT COMMAND helix_add_library)
    include("${CMAKE_CURRENT_SOURCE_DIR}/External/Helix.CMake/Helix.cmake")
endif()

helix_require(Core)
helix_require(Platform)
helix_require(Profiler)
helix_require(Jobs)
helix_require(Math)
helix_require(Font)

# The UI core: the element tree, the layout, the style sheets, the markup and the text, without the engine.
helix_add_library(helix_ui ALIAS UI PUBLIC_DEPS Helix::Core Helix::Math Helix::Font PRIVATE_DEPS Helix::Jobs Helix::Platform)

helix_add_test_suite(helix_ui_tests EXCLUDE "/Tests/Engine/" DEPS Helix::UI Helix::Jobs Helix::Platform)
if(TARGET helix_ui_tests)
    # The official bidi and line break tests of the Unicode data.
    target_compile_definitions(helix_ui_tests PRIVATE "HELIX_UI_UNICODE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/Unicode\"")
    # In node the tests read the Unicode files from the disk itself.
    if(HELIX_PLATFORM STREQUAL "web")
        target_link_options(helix_ui_tests PRIVATE -sNODERAWFS=1)
    endif()
    # Fonts built in code with Helix.Font's test writers.
    helix_dependency_dir(font_dir Font)
    target_include_directories(helix_ui_tests PRIVATE "${font_dir}/Tests")
endif()
helix_add_benchmark_suite(helix_ui_bench DEPS Helix::UI Helix::Jobs Helix::Platform)

# The UI in the engine: a native module (HelixUI) with the UI's assets and views, a shared library beside the engine's
# (a static one linked into the app on the Web).
helix_require(Engine)
helix_require(ScriptCore)
include("${HELIX_BINDGEN_ROOT}/CMake/HelixBindgen.cmake")
find_program(HELIX_DOTNET NAMES dotnet HINTS "$ENV{HELIX_DOTNET_ROOT}" "$ENV{DOTNET_ROOT}" REQUIRED)
helix_dependency_dir(shaders_dir Shaders)
helix_dependency_dir(engine_dir Engine)
helix_dependency_dir(math_dir Math)
helix_dependency_dir(ui_platform_dir Platform)
set(engine_source "${engine_dir}/Source")
set(ui_engine_source "${CMAKE_CURRENT_SOURCE_DIR}/Source/Helix.UI.Engine")
set(ui_artifacts "${CMAKE_BINARY_DIR}/HelixUI")

file(GLOB_RECURSE ui_engine_cpp CONFIGURE_DEPENDS "${ui_engine_source}/Helix/UI/Engine/*.cpp")
file(GLOB_RECURSE ui_engine_headers CONFIGURE_DEPENDS "${ui_engine_source}/Helix/UI/Engine/*.h")
if(HELIX_PLATFORM STREQUAL "web")
    add_library(helix_ui_engine STATIC ${ui_engine_cpp} ${ui_engine_headers})
    target_link_libraries(helix_ui_engine PUBLIC Helix::UI Helix::Engine)
else()
    # The UI core and Helix.Font built into the module against the shared engine, which holds Core, Platform, Jobs and
    # Image once for every module (static copies of them would be second instances of their state).
    get_target_property(ui_core_sources helix_ui SOURCES)
    get_target_property(font_sources helix_font SOURCES)
    get_target_property(font_root helix_font SOURCE_DIR)
    add_library(helix_ui_engine SHARED ${ui_engine_cpp} ${ui_engine_headers} ${ui_core_sources} ${font_sources})
    target_include_directories(helix_ui_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source" "${font_root}/Source" PRIVATE "${font_root}/ThirdParty")
    target_link_libraries(helix_ui_engine PUBLIC Helix::EngineShared)
    set_target_properties(helix_ui_engine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
                                                     LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
add_library(Helix::UIEngine ALIAS helix_ui_engine)
set_target_properties(helix_ui_engine PROPERTIES OUTPUT_NAME HelixUI)
target_include_directories(helix_ui_engine PUBLIC "${ui_engine_source}")
# The default theme's text, built into the module as bytes.
set(ui_default_theme "${CMAKE_CURRENT_SOURCE_DIR}/Themes/HelixDefault.tss")
file(READ "${ui_default_theme}" ui_theme_hex HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," ui_theme_bytes "${ui_theme_hex}")
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Generated/HelixDefaultTheme.inc" CONTENT "${ui_theme_bytes}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ui_default_theme}")
target_include_directories(helix_ui_engine PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Generated")
helix_apply_options(helix_ui_engine)

# The headers whose tagged types C# sees, and the engine's they name.
set(ui_bound_headers "${CMAKE_CURRENT_SOURCE_DIR}/Source/Helix/UI/Elements/ElementTree.h" "${ui_engine_source}/Helix/UI/Engine/Element.h"
    "${ui_engine_source}/Helix/UI/Engine/UIAssets.h" "${ui_engine_source}/Helix/UI/Engine/FontAsset.h" "${ui_engine_source}/Helix/UI/Engine/UIView.h"
    "${ui_engine_source}/Helix/UI/Engine/TemplateContainer.h" "${ui_engine_source}/Helix/UI/Engine/UIPanelSettings.h"
    "${ui_engine_source}/Helix/UI/Engine/PanelInputConfiguration.h" "${ui_engine_source}/Helix/UI/Engine/Controls.h" "${ui_engine_source}/Helix/UI/Engine/GuiTypes.h" "${ui_engine_source}/Helix/UI/Engine/Gui.h"
    "${ui_engine_source}/Helix/UI/Engine/Fields.h" "${ui_engine_source}/Helix/UI/Engine/Containers.h"
    "${ui_engine_source}/Helix/UI/Engine/VectorImage.h")
set(ui_engine_references "${engine_source}/Helix/Engine/Object/EntityId.h" "${engine_source}/Helix/Engine/Object/Object.h"
    "${engine_source}/Helix/Engine/GameObject/GameObject.h" "${engine_source}/Helix/Engine/Script/GameBehaviour.h"
    "${engine_source}/Helix/Engine/Script/ScriptableObject.h" "${engine_source}/Helix/Engine/Transform/Transform.h"
    "${engine_source}/Helix/Engine/Texture/Texture.h" "${engine_source}/Helix/Engine/Texture/RenderTexture.h"
    "${engine_source}/Helix/Engine/Rendering/Camera.h" "${engine_source}/Helix/Engine/Math/Hash128.h" "${math_dir}/Source/Helix/Math/Vector.h" "${math_dir}/Source/Helix/Math/Color.h" "${math_dir}/Source/Helix/Math/Matrix4x4.h"
    "${math_dir}/Source/Helix/Math/Geometry.h" "${ui_platform_dir}/Source/Helix/Platform/Window/Input.h")
helix_bindgen(helix_ui_engine MODULE HelixUI NAMESPACE HelixEngine.UI KIND NativeGame API HEADERS ${ui_bound_headers}
              REFERENCES ${ui_engine_references} REFERENCE_NAMESPACE HelixEngine
              INCLUDE_ROOTS "${ui_engine_source}" "${CMAKE_CURRENT_SOURCE_DIR}/Source" "${engine_source}" "${math_dir}/Source" "${ui_platform_dir}/Source"
              CS_OUTPUT ui_cs)
set(HELIX_UI_BINDINGS "${ui_cs}" CACHE INTERNAL "")

# The C# side: the generated bindings and the C# written by hand in the reference's shape, over HelixEngine.CoreModule.
set(runtime_dir "${CMAKE_CURRENT_SOURCE_DIR}/Source/Helix.UI.Runtime")
set(ui_runtime_dll "${ui_artifacts}/bin/Helix.UI.Runtime/release/Helix.UI.Runtime.dll")
file(GLOB_RECURSE ui_runtime_sources CONFIGURE_DEPENDS "${runtime_dir}/*.cs" "${runtime_dir}/*.csproj")
list(FILTER ui_runtime_sources EXCLUDE REGEX "/(bin|obj)/")
add_custom_command(
    OUTPUT "${ui_runtime_dll}"
    COMMAND "${HELIX_DOTNET}" build "${runtime_dir}/Helix.UI.Runtime.csproj" -c Release --artifacts-path "${ui_artifacts}" --nologo -v quiet
            "-p:HelixCoreModule=${HELIX_CORE_MODULE}" "-p:HelixScriptingRuntime=${HELIX_SCRIPTING_RUNTIME}" "-p:HelixModuleBindings=${ui_cs}"
    DEPENDS ${ui_runtime_sources} "${HELIX_CORE_MODULE}" "${ui_cs}"
    COMMENT "Building Helix.UI.Runtime")
add_custom_target(helix_ui_runtime ALL DEPENDS "${ui_runtime_dll}")
add_dependencies(helix_ui_runtime helix_core_module helix_ui_engine)
set(HELIX_UI_RUNTIME "${ui_runtime_dll}" CACHE INTERNAL "")

# The UI's C# shaders: an assembly loaded as a shader asset.
set(ui_shaders_dll "${ui_artifacts}/bin/Helix.UI.Shaders/release/Helix.UI.Shaders.dll")
file(GLOB_RECURSE ui_shader_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.cs" "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.csproj"
     "${shaders_dir}/Source/*.cs" "${engine_dir}/Shaders/*.cs")
list(FILTER ui_shader_sources EXCLUDE REGEX "/(bin|obj)/")
add_custom_command(
    OUTPUT "${ui_shaders_dll}"
    COMMAND "${HELIX_DOTNET}" build "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Helix.UI.Shaders.csproj" -c Release --artifacts-path "${ui_artifacts}"
            --nologo -v quiet "-p:HelixShadersDir=${shaders_dir}" "-p:HelixEngineDir=${engine_dir}"
    DEPENDS ${ui_shader_sources}
    COMMENT "Building the UI's C# shaders")
add_custom_target(helix_ui_shaders ALL DEPENDS "${ui_shaders_dll}")
add_dependencies(helix_ui_engine helix_ui_shaders)
set(HELIX_UI_SHADERS "${ui_shaders_dll}" CACHE INTERNAL "")

# Tests of the UI in the engine: views drawn by a scriptable pipeline of the tests on every device.
if(HELIX_PLATFORM STREQUAL "web")
    helix_add_test_suite(helix_ui_engine_tests DOTNET EXCLUDE "/Tests/Core/" DEPS Helix::UIEngine Helix::Engine)
else()
    helix_add_test_suite(helix_ui_engine_tests EXCLUDE "/Tests/Core/" DEPS Helix::UIEngine Helix::EngineShared)
endif()
if(TARGET helix_ui_engine_tests)
    set(tests_dir "${CMAKE_CURRENT_SOURCE_DIR}/Tests")
    helix_bindgen(helix_ui_engine_tests MODULE UIEngineTests NAMESPACE UIEngineTests KIND NativeGame HEADERS "${tests_dir}/Engine/Support/TestPipeline.h"
                  "${tests_dir}/Engine/Support/CppScripts.h" REFERENCES ${ui_engine_references} ${ui_bound_headers} REFERENCE_NAMESPACE HelixEngine INCLUDE_ROOTS "${tests_dir}" "${engine_source}" "${math_dir}/Source" "${ui_platform_dir}/Source"
                  "${ui_engine_source}" "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    set(tests_generated "${CMAKE_CURRENT_BINARY_DIR}/Generated")
    add_custom_command(
        OUTPUT "${tests_generated}/UIShadersAssembly.h"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${tests_generated}"
        COMMAND "${CMAKE_COMMAND}" "-DINPUT=${ui_shaders_dll}" "-DOUTPUT=${tests_generated}/UIShadersAssembly.h" -DNAME=kUIShadersAssembly
                -P "${HELIX_EMBED_FILE}"
        DEPENDS "${ui_shaders_dll}" "${HELIX_EMBED_FILE}"
        COMMENT "Embedding the UI's shaders")
    target_sources(helix_ui_engine_tests PRIVATE "${tests_generated}/UIShadersAssembly.h")
    helix_dependency_dir(font_tests_root Font)
    target_include_directories(helix_ui_engine_tests PRIVATE "${tests_generated}" "${engine_dir}/Tests" "${font_tests_root}/Tests")
    target_compile_definitions(helix_ui_engine_tests PRIVATE "HELIX_ENGINE_GOLDEN_DIR=\"${tests_dir}/Golden\"")

    # The C# tests: GameBehaviours over the UI's C# side, run by the C++ tests on the .NET host.
    set(ui_tests_dll "${ui_artifacts}/bin/Helix.UI.Tests/release/Helix.UI.Tests.dll")
    get_filename_component(ui_tests_bin "${ui_tests_dll}" DIRECTORY)
    file(GLOB_RECURSE ui_test_sources CONFIGURE_DEPENDS "${tests_dir}/Managed/*.cs" "${tests_dir}/Managed/*.csproj")
    list(FILTER ui_test_sources EXCLUDE REGEX "/(bin|obj)/")
    add_custom_command(
        OUTPUT "${ui_tests_dll}"
        COMMAND "${HELIX_DOTNET}" build "${tests_dir}/Managed/Helix.UI.Tests.csproj" -c Release --artifacts-path "${ui_artifacts}" --nologo -v quiet
                "-p:HelixCoreModule=${HELIX_CORE_MODULE}" "-p:HelixScriptingRuntime=${HELIX_SCRIPTING_RUNTIME}" "-p:HelixUIRuntime=${ui_runtime_dll}"
        DEPENDS ${ui_test_sources} "${ui_runtime_dll}"
        COMMENT "Building the C# tests of Helix.UI")
    add_custom_target(helix_ui_test_scripts DEPENDS "${ui_tests_dll}")
    add_dependencies(helix_ui_test_scripts helix_ui_runtime)
    add_dependencies(helix_ui_engine_tests helix_ui_test_scripts)
    target_compile_definitions(helix_ui_engine_tests PRIVATE "HX_TEST_RUNTIME_ASSEMBLY=\"${HELIX_SCRIPTING_RUNTIME}\""
                                                            "HX_TEST_CORE_MODULE=\"${HELIX_CORE_MODULE}\"" "HX_TEST_SCRIPTS=\"${ui_tests_dll}\""
                                                            "HX_TEST_UI_RUNTIME=\"${ui_tests_bin}/Helix.UI.Runtime.dll\"")
    set_property(TARGET helix_ui_engine_tests APPEND PROPERTY HELIX_DOTNET_ASSEMBLIES "${HELIX_CORE_MODULE}" ${HELIX_CORE_MODULE_REFERENCES}
                                                                                     "${ui_runtime_dll}" "${ui_tests_dll}")
    if(TARGET helix_ui_engine_tests_web)
        add_dependencies(helix_ui_engine_tests_web helix_ui_test_scripts helix_core_module)
    endif()
endif()

# The .NET runtime is never unloaded: LeakSanitizer is told about its modules (as in Helix.Scripting's tests).
if(TEST helix_ui_engine_tests AND HELIX_ENABLE_ASAN AND HELIX_PLATFORM STREQUAL "linux")
    helix_dependency_dir(scripting_dir Scripting)
    set_tests_properties(helix_ui_engine_tests PROPERTIES ENVIRONMENT "LSAN_OPTIONS=suppressions=${scripting_dir}/Tests/Support/LeakSuppressions.txt")
endif()

# The public surface of the C# side against its inventory (Tests/Api/Helix.UI.csv), with Helix.ScriptCore's tool.
if(HELIX_BUILD_TESTS AND PROJECT_IS_TOP_LEVEL AND HELIX_API_INVENTORY AND TARGET helix_ui_runtime)
    add_test(NAME helix_ui_api COMMAND "${HELIX_DOTNET}" "${HELIX_API_INVENTORY}" --assembly "${ui_runtime_dll}" --reference "${HELIX_CORE_MODULE}"
             --reference "${HELIX_SCRIPTING_RUNTIME}" --inventory "${CMAKE_CURRENT_SOURCE_DIR}/Tests/Api/Helix.UI.csv")
    add_dependencies(helix_ui_runtime helix_api_inventory)
endif()
