#include "MathExpression.h"
#include "../../Include/RmlUi/Core/Element.h"
#include "../../Include/RmlUi/Core/Log.h"
#include "../../Include/RmlUi/Core/Math.h"
#include "../../Include/RmlUi/Core/NumericValue.h"
#include "../../Include/RmlUi/Core/PropertyDefinition.h"
#include "../../Include/RmlUi/Core/StringUtilities.h"
#include "ComputeProperty.h"
#include "ElementStyle.h"
#include "PropertyParserNumber.h"
#include <stdlib.h>
#include <string.h>

namespace Rml {

namespace {

	enum class MathKind : uint8_t { Number, Length, Percent, Angle };

	struct MathValue {
		float number = 0.f;
		MathKind kind = MathKind::Number;
	};

	enum class MathFunction : uint8_t { None, Calc, Min, Max, Clamp };

	struct MathFunctionName {
		const char* name;
		size_t length;
		MathFunction function;
	};

	constexpr MathFunctionName math_function_names[] = {
		{"calc", 4, MathFunction::Calc},
		{"min", 3, MathFunction::Min},
		{"max", 3, MathFunction::Max},
		{"clamp", 5, MathFunction::Clamp},
	};

	bool IsIdentifierChar(char c)
	{
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
	}

	bool IsNumberStart(char c)
	{
		return (c >= '0' && c <= '9') || c == '.' || c == '+' || c == '-';
	}

	MathFunction FindMathFunctionEndingAt(const String& value, size_t paren_pos, size_t& name_length)
	{
		for (const MathFunctionName& entry : math_function_names)
		{
			if (paren_pos < entry.length)
				continue;
			const size_t start = paren_pos - entry.length;
			if (value.compare(start, entry.length, entry.name) != 0)
				continue;
			if (start > 0 && IsIdentifierChar(value[start - 1]))
				continue;
			name_length = entry.length;
			return entry.function;
		}
		name_length = 0;
		return MathFunction::None;
	}

	class MathParser {
	public:
		MathParser(const String& source, size_t begin, size_t end, Element* element, PropertyId id) :
			source(source), pos(begin), end(end), element(element), id(id)
		{}

		bool ParseFunction(MathValue& out);

		bool AtEnd()
		{
			SkipWhitespace();
			return pos >= end;
		}

		const char* GetError() const { return error; }

	private:
		bool Fail(const char* message)
		{
			if (!error)
				error = message;
			return false;
		}

		char Peek() const { return pos < end ? source[pos] : '\0'; }

		bool SkipWhitespace()
		{
			const size_t start = pos;
			while (pos < end && StringUtilities::IsWhitespace(source[pos]))
				pos++;
			return pos != start;
		}

		bool Expect(char c)
		{
			SkipWhitespace();
			if (Peek() != c)
				return Fail(Peek() == '\0' ? "unexpected end of expression" : "unexpected character");
			pos++;
			return true;
		}

		bool ParseSum(MathValue& out);
		bool ParseProduct(MathValue& out);
		bool ParsePrimary(MathValue& out);
		bool ParseNumber(MathValue& out);
		bool ConvertTerm(float number, Unit unit, MathValue& out);

		const String& source;
		size_t pos;
		size_t end;
		Element* element;
		PropertyId id;
		const char* error = nullptr;
	};

	bool MathParser::ParseFunction(MathValue& out)
	{
		SkipWhitespace();
		const size_t name_start = pos;
		while (pos < end && IsIdentifierChar(source[pos]))
			pos++;
		if (Peek() != '(')
			return Fail("expected a math function");

		size_t name_length = 0;
		const MathFunction function = FindMathFunctionEndingAt(source, pos, name_length);
		if (function == MathFunction::None || name_start != pos - name_length)
			return Fail("unknown function");
		pos++;

		switch (function)
		{
		case MathFunction::Calc:
		{
			if (!ParseSum(out))
				return false;
		}
		break;
		case MathFunction::Min:
		case MathFunction::Max:
		{
			if (!ParseSum(out))
				return false;
			while (true)
			{
				SkipWhitespace();
				if (Peek() != ',')
					break;
				pos++;
				MathValue next;
				if (!ParseSum(next))
					return false;
				if (next.kind != out.kind)
					return Fail("mixed types in min() or max()");
				out.number = (function == MathFunction::Min ? Math::Min(out.number, next.number) : Math::Max(out.number, next.number));
			}
		}
		break;
		case MathFunction::Clamp:
		{
			MathValue low, value, high;
			if (!ParseSum(low) || !Expect(',') || !ParseSum(value) || !Expect(',') || !ParseSum(high))
				return false;
			if (low.kind != value.kind || high.kind != value.kind)
				return Fail("mixed types in clamp()");
			out.kind = value.kind;
			out.number = Math::Max(low.number, Math::Min(value.number, high.number));
		}
		break;
		case MathFunction::None: return false;
		}

		return Expect(')');
	}

	bool MathParser::ParseSum(MathValue& out)
	{
		if (!ParseProduct(out))
			return false;

		while (true)
		{
			SkipWhitespace();
			const char c = Peek();
			if (c != '+' && c != '-')
				break;
			const bool whitespace_before = (pos > 0 && StringUtilities::IsWhitespace(source[pos - 1]));
			if (!whitespace_before || pos + 1 >= end || !StringUtilities::IsWhitespace(source[pos + 1]))
				return Fail("'+' and '-' must be surrounded by whitespace");
			pos++;

			MathValue rhs;
			if (!ParseProduct(rhs))
				return false;
			if (rhs.kind != out.kind)
				return Fail("mixed types in addition");
			out.number = (c == '+' ? out.number + rhs.number : out.number - rhs.number);
		}

		return true;
	}

	bool MathParser::ParseProduct(MathValue& out)
	{
		if (!ParsePrimary(out))
			return false;

		while (true)
		{
			SkipWhitespace();
			const char c = Peek();
			if (c != '*' && c != '/')
				break;
			pos++;

			MathValue rhs;
			if (!ParsePrimary(rhs))
				return false;

			if (c == '*')
			{
				if (out.kind == MathKind::Number)
					out.kind = rhs.kind;
				else if (rhs.kind != MathKind::Number)
					return Fail("multiplication needs a plain number on one side");
				out.number *= rhs.number;
			}
			else
			{
				if (rhs.kind != MathKind::Number)
					return Fail("division needs a plain number on the right side");
				if (rhs.number == 0.f)
					return Fail("division by zero");
				out.number /= rhs.number;
			}
		}

		return true;
	}

	bool MathParser::ParsePrimary(MathValue& out)
	{
		SkipWhitespace();
		const char c = Peek();

		if (c == '(')
		{
			pos++;
			if (!ParseSum(out))
				return false;
			return Expect(')');
		}

		if (IsNumberStart(c))
			return ParseNumber(out);

		if (IsIdentifierChar(c))
			return ParseFunction(out);

		return Fail(c == '\0' ? "unexpected end of expression" : "unexpected character");
	}

	bool MathParser::ParseNumber(MathValue& out)
	{
		const char* begin = source.c_str() + pos;
		char* number_end = nullptr;
		const float number = strtof(begin, &number_end);
		if (number_end == begin || number_end > source.c_str() + end)
			return Fail("invalid number");
		pos = size_t(number_end - source.c_str());

		const size_t unit_start = pos;
		while (pos < end && (Peek() == '%' || (Peek() >= 'a' && Peek() <= 'z') || (Peek() >= 'A' && Peek() <= 'Z')))
			pos++;

		const Unit unit = PropertyParserNumber::LookupUnit(StringUtilities::ToLower(source.substr(unit_start, pos - unit_start)));
		if (unit == Unit::UNKNOWN)
			return Fail("unknown unit");

		return ConvertTerm(number, unit, out);
	}

	bool MathParser::ConvertTerm(float number, Unit unit, MathValue& out)
	{
		if (unit == Unit::NUMBER)
		{
			out = {number, MathKind::Number};
			return true;
		}
		if (unit == Unit::PERCENT)
		{
			out = {number, MathKind::Percent};
			return true;
		}
		if (unit == Unit::DEG || unit == Unit::RAD)
		{
			out = {ComputeAngle(NumericValue(number, unit)), MathKind::Angle};
			return true;
		}
		if (Any(unit & Unit::LENGTH))
		{
			if (!element)
				return Fail("relative length without an element");
			const NumericValue value(number, unit);
			if (id == PropertyId::FontSize && unit == Unit::EM)
				out = {element->GetStyle()->ResolveRelativeLength(value, RelativeTarget::ParentFontSize), MathKind::Length};
			else
				out = {ComputeLength(value, element), MathKind::Length};
			return true;
		}
		return Fail("unit not allowed in math expression");
	}

	String ToLiteral(const MathValue& value)
	{
		switch (value.kind)
		{
		case MathKind::Number: return CreateString("%.9g", value.number);
		case MathKind::Length: return CreateString("%.9gpx", value.number);
		case MathKind::Percent: return CreateString("%.9g%%", value.number);
		case MathKind::Angle: return CreateString("%.9grad", value.number);
		}
		return String();
	}

} // namespace

bool FoldMathFunctions(String& value, Element* element, PropertyId id)
{
	size_t search = 0;
	while (true)
	{
		const size_t paren = value.find('(', search);
		if (paren == String::npos)
			return true;

		size_t name_length = 0;
		if (FindMathFunctionEndingAt(value, paren, name_length) == MathFunction::None)
		{
			search = paren + 1;
			continue;
		}
		const size_t start = paren - name_length;

		size_t close = String::npos;
		int depth = 0;
		for (size_t i = paren; i < value.size(); i++)
		{
			if (value[i] == '(')
				depth++;
			else if (value[i] == ')' && --depth == 0)
			{
				close = i;
				break;
			}
		}

		const char* error = nullptr;
		MathValue result;
		if (close == String::npos)
		{
			error = "unbalanced parentheses";
		}
		else
		{
			MathParser parser(value, start, close + 1, element, id);
			if (!parser.ParseFunction(result) || !parser.AtEnd())
				error = (parser.GetError() ? parser.GetError() : "trailing characters");
		}

		if (error)
		{
			const String expression = value.substr(start, (close == String::npos ? String::npos : close + 1 - start));
			Log::Message(Log::LT_ERROR, "Invalid math expression '%s': %s. In element: %s", expression.c_str(), error,
				element ? element->GetAddress().c_str() : "");
			return false;
		}

		const String literal = ToLiteral(result);
		value.replace(start, close + 1 - start, literal);
		search = start + literal.size();
	}
}

} // namespace Rml
