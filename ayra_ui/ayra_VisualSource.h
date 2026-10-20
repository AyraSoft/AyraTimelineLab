/** Compile-time routing, not a second copy of the design system.
    In source mode these includes resolve directly inside the read-only
    ayra_multiverse checkout. Without it the already-vendored Lab snapshot is
    explicit. The application LAF itself is NOT imported by this leaf adapter.
*/
#pragma once
#define AYRA_LAB_STRINGIFY_IMPL(path) #path
#define AYRA_LAB_STRINGIFY(path) AYRA_LAB_STRINGIFY_IMPL(path)
#if defined(AYRA_LAB_VISUALS_FROM_MULTIVERSE) && AYRA_LAB_VISUALS_FROM_MULTIVERSE
 #define AYRA_LAB_LAF_FILE(path) <look_and_feel/path>
 #define AYRA_LAB_CONTROL_FILE(path) <widgets/controls/path>
#else
 #define AYRA_LAB_LAF_FILE(path) AYRA_LAB_STRINGIFY(path)
 #define AYRA_LAB_CONTROL_FILE(path) AYRA_LAB_STRINGIFY(controls/path)
#endif
