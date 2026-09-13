#pragma once

#include "../../Include/RmlUi/Core/ID.h"
#include "../../Include/RmlUi/Core/Types.h"

namespace Rml {

class Element;

/// Replaces every calc(), min(), max() and clamp() in the value with the literal it evaluates to, in the canonical
/// unit of its type: 'px' for lengths, '%' for percentages, 'rad' for angles, none for numbers. Lengths are resolved
/// against the element like any other length, so the result is only valid for that element at the time of the call.
/// @param[in,out] value The property value, possibly containing math functions.
/// @param[in] element The element the value belongs to, used to resolve relative and viewport units.
/// @param[in] id The property being resolved, or Invalid; 'em' in font-size refers to the parent font size.
/// @return True if the value contained no math functions or all of them were folded, false on an invalid expression.
bool FoldMathFunctions(String& value, Element* element, PropertyId id);

} // namespace Rml
