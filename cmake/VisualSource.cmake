# Read original leaf renderers/controls in place. Never copy private providers
# into the public Lab and never pretend the fallback is ApplicationLookAndFeel.
set(AYRA_SOURCE_DIR "${AYRA_SOURCE_DIR}" CACHE PATH "Read-only ayra_multiverse checkout for visual sources")
# Native application builds use the real visual source by default. A snapshot
# is an explicit compatibility opt-out, never an unnoticed substitution.
set(_ayra_require_default OFF)
if(AYRA_LAB_BUILD_GUI)
  set(_ayra_require_default ON)
endif()
option(AYRA_LAB_REQUIRE_MULTIVERSE_VISUALS "Reject the explicit ayra_ui snapshot fallback" ${_ayra_require_default})
add_library(ayra_lab_visual_source INTERFACE)
if(AYRA_LAB_REQUIRE_MULTIVERSE_VISUALS AND NOT AYRA_SOURCE_DIR)
  message(FATAL_ERROR "Set AYRA_SOURCE_DIR to the ayra_multiverse checkout; source visuals are required")
endif()
set(_ayra_visual_leaves
  look_and_feel/tokens/ayra_Tokens_Colours.h
  look_and_feel/tokens/ayra_Tokens_Geometry.h
  look_and_feel/tokens/ayra_Tokens_Typography.h
  look_and_feel/tokens/ayra_Tokens.h
  look_and_feel/ayra_NodeButtons.h
  look_and_feel/ayra_ChromePrimitives.h
  look_and_feel/ayra_CommonDefaultLookAndFeel_Mixer.cpp)
foreach(_file Glyph TransportGlyphs StateGlyphs FileGlyphs LayoutGlyphs LogoGlyphs ControlGlyphs SystemGlyphs Glyphs DeviceGlyphs SnapshotGlyphs CommandGlyphs ActionGlyphs GlyphHandles)
  list(APPEND _ayra_visual_leaves "look_and_feel/glyphs/ayra_${_file}.h")
endforeach()
foreach(_pair "shape_icon_button/ayra_ShapeIconButton" "add_button/ayra_AddButton" "tab_strip/ayra_TabStrip" "drag_value_field/ayra_DragValueField")
  list(APPEND _ayra_visual_leaves "widgets/controls/${_pair}.h" "widgets/controls/${_pair}.cpp")
endforeach()
if(AYRA_SOURCE_DIR)
  get_filename_component(_ayra_source_real "${CMAKE_SOURCE_DIR}" REALPATH)
  get_filename_component(_ayra_binary_real "${CMAKE_BINARY_DIR}" REALPATH)
  if(_ayra_source_real STREQUAL _ayra_binary_real)
    message(FATAL_ERROR "Use an out-of-source build: generated private renderer adapters must stay in the build directory")
  endif()
  get_filename_component(AYRA_SOURCE_DIR "${AYRA_SOURCE_DIR}" ABSOLUTE)
  foreach(_file IN LISTS _ayra_visual_leaves)
    if(NOT EXISTS "${AYRA_SOURCE_DIR}/modules/${_file}")
      message(FATAL_ERROR "Incomplete visual source checkout: ${AYRA_SOURCE_DIR}/modules/${_file}")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${AYRA_SOURCE_DIR}/modules/${_file}")
  endforeach()
  target_compile_definitions(ayra_lab_visual_source INTERFACE AYRA_LAB_VISUALS_FROM_MULTIVERSE=1)
  target_include_directories(ayra_lab_visual_source INTERFACE "${AYRA_SOURCE_DIR}/modules")
  # Adapt only the application-qualified standard-button TU, never paint bodies.
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  set(_ayra_buttons "${AYRA_SOURCE_DIR}/modules/look_and_feel/app/ApplicationLookAndFeel_Buttons.cpp")
  set(AYRA_STANDARD_ARTWORK_SOURCE "${CMAKE_BINARY_DIR}/generated/MultiverseButtons.cpp")
  get_filename_component(_ayra_lab_tools "${CMAKE_CURRENT_LIST_DIR}/../tools" ABSOLUTE)
  execute_process(COMMAND "${Python3_EXECUTABLE}" "${_ayra_lab_tools}/adapt_multiverse_buttons.py"
    "${_ayra_buttons}" "${AYRA_STANDARD_ARTWORK_SOURCE}"
    RESULT_VARIABLE _ayra_adapt_result ERROR_VARIABLE _ayra_adapt_error)
  if(NOT _ayra_adapt_result EQUAL 0)
    message(FATAL_ERROR "${_ayra_adapt_error}")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_ayra_buttons}")
  list(APPEND _ayra_visual_leaves "look_and_feel/app/ApplicationLookAndFeel_Buttons.cpp")
  set(_ayra_origin "Multiverse source leaves + original standard buttons (Lab adapter, not full ApplicationLookAndFeel)")
else()
  target_compile_definitions(ayra_lab_visual_source INTERFACE AYRA_LAB_VISUALS_FROM_MULTIVERSE=0)
  set(_ayra_origin "ayra_ui snapshot (not live Multiverse)")
endif()
# Audit artifact is generated in the build, contains no credentials or source code.
set(_ayra_manifest "Visual source: ${_ayra_origin}\n")
get_filename_component(_ayra_lab_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
foreach(_file IN LISTS _ayra_visual_leaves)
  if(AYRA_SOURCE_DIR)
    set(_read "${AYRA_SOURCE_DIR}/modules/${_file}")
  else()
    string(REGEX REPLACE "^look_and_feel/" "" _relative "${_file}")
    string(REGEX REPLACE "^widgets/controls/" "controls/" _relative "${_relative}")
    set(_read "${_ayra_lab_root}/ayra_ui/${_relative}")
  endif()
  file(SHA256 "${_read}" _digest)
  string(APPEND _ayra_manifest "${_digest}  ${_file}\n")
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/visual-source-manifest.txt" "${_ayra_manifest}")
message(STATUS "Ayra visual source: ${_ayra_origin}")
