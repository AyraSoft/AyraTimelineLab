/** Include-only adapter to the canonical curve provider. No copied maths. */
#pragma once
#if AYRA_LAB_CURVES_FROM_MATH
 #include <algorithm>
 #include <cmath>
 #include <cstdint>
 #include <utility>
 #include <ayra_math/curves/ayra_CurveSegment.h>
 #include <ayra_math/curves/ayra_FadeCurves.h>
#else
 #include <ayra_curves/ayra_curves.h>
#endif
