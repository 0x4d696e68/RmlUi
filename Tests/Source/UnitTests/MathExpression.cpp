#include "../Common/TestsInterface.h"
#include "../Common/TestsShell.h"
#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Transform.h>
#include <doctest.h>

using namespace Rml;

static const String document_rml_prefix = R"(
<rml>
<head>
	<style>
	body {
		width: 500px;
		height: 500px;
		font-size: 20px;
		--gap: 6px;
		--n: 3;
	}
	div {
)";

static const String document_rml_suffix = R"(
	}
	</style>
</head>
<body>
	<div id="div"/>
</body>
</rml>
)";

static ElementDocument* LoadDivDocument(Context* context, const String& div_style)
{
	ElementDocument* document = context->LoadDocumentFromMemory(document_rml_prefix + div_style + document_rml_suffix);
	REQUIRE(document);
	document->Show();
	context->Update();
	return document;
}

TEST_CASE("math_expression.lengths")
{
	Context* context = TestsShell::GetContext();
	REQUIRE(context);
	context->SetViewportScale(Vector2f(2.f, 3.f));

	ElementDocument* document = LoadDivDocument(context, R"(
		width: calc(10px + 2 * 5px);
		height: calc((100px - 40px) / 3);
		left: min(30px, 2vm);
		top: max(30px, 2vx, 1vy);
		margin-left: clamp(10px, 4vm, 12px);
		margin-top: calc(1em + 2px);
		margin-right: calc(-1 * 5px);
		padding-left: calc(5px * -2);
	)");

	Element* div = document->GetElementById("div");
	REQUIRE(div);
	const ComputedValues& computed = div->GetComputedValues();

	CHECK(computed.width().type == Style::LengthPercentageAuto::Length);
	CHECK(computed.width().value == doctest::Approx(20.f));
	CHECK(computed.height().value == doctest::Approx(20.f));
	CHECK(computed.left().value == doctest::Approx(4.f));
	CHECK(computed.top().value == doctest::Approx(30.f));
	CHECK(computed.margin_left().value == doctest::Approx(10.f));
	CHECK(computed.margin_top().value == doctest::Approx(22.f));
	CHECK(computed.margin_right().value == doctest::Approx(-5.f));
	CHECK(computed.padding_left().value == doctest::Approx(-10.f));

	CHECK(div->GetProperty(PropertyId::Width)->ToString() == "20px");

	context->SetViewportScale(Vector2f(4.f, 6.f));
	context->Update();
	CHECK(computed.left().value == doctest::Approx(8.f));
	CHECK(computed.margin_left().value == doctest::Approx(12.f));

	context->SetViewportScale(Vector2f(1.f, 1.f));
	document->Close();
	TestsShell::ShutdownShell();
}

TEST_CASE("math_expression.percent_number_angle")
{
	Context* context = TestsShell::GetContext();
	REQUIRE(context);

	ElementDocument* document = LoadDivDocument(context, R"(
		width: calc(100% / 4);
		left: max(10%, 20%);
		opacity: calc(1 / 4);
		z-index: calc(2 * var(--n));
		transform: rotate(calc(90deg * 2));
	)");

	Element* div = document->GetElementById("div");
	REQUIRE(div);
	const ComputedValues& computed = div->GetComputedValues();

	CHECK(computed.width().type == Style::LengthPercentageAuto::Percentage);
	CHECK(computed.width().value == doctest::Approx(25.f));
	CHECK(computed.left().type == Style::LengthPercentageAuto::Percentage);
	CHECK(computed.left().value == doctest::Approx(20.f));
	CHECK(computed.opacity() == doctest::Approx(0.25f));
	CHECK(computed.z_index().value == doctest::Approx(6.f));

	const Property* transform = div->GetProperty(PropertyId::Transform);
	REQUIRE(transform);
	const TransformPtr transform_ptr = transform->Get<TransformPtr>();
	REQUIRE(transform_ptr);
	CHECK(transform_ptr->GetNumPrimitives() == 1);

	document->Close();
	TestsShell::ShutdownShell();
}

TEST_CASE("math_expression.variables_and_shorthands")
{
	Context* context = TestsShell::GetContext();
	REQUIRE(context);

	ElementDocument* document = LoadDivDocument(context, R"(
		--w: calc(1px + 1px);
		width: var(--w);
		height: calc(var(--gap) * 2 + 1px);
		margin: calc(1px + 1px) 0;
		padding: 0 min(var(--gap), 100px);
	)");

	Element* div = document->GetElementById("div");
	REQUIRE(div);
	const ComputedValues& computed = div->GetComputedValues();

	CHECK(computed.width().value == doctest::Approx(2.f));
	CHECK(computed.height().value == doctest::Approx(13.f));
	CHECK(computed.margin_top().value == doctest::Approx(2.f));
	CHECK(computed.margin_left().value == doctest::Approx(0.f));
	CHECK(computed.padding_right().value == doctest::Approx(6.f));
	CHECK(div->GetProperty("--w")->ToString() == "calc(1px + 1px)");

	div->SetProperty("--gap", "10px");
	context->Update();
	CHECK(computed.height().value == doctest::Approx(21.f));
	CHECK(computed.padding_right().value == doctest::Approx(10.f));

	document->Close();
	TestsShell::ShutdownShell();
}

TEST_CASE("math_expression.font_size")
{
	Context* context = TestsShell::GetContext();
	REQUIRE(context);

	ElementDocument* document = LoadDivDocument(context, R"(
		font-size: calc(1em + 2px);
		width: calc(1em + 1px);
	)");

	Element* div = document->GetElementById("div");
	REQUIRE(div);
	const ComputedValues& computed = div->GetComputedValues();

	CHECK(computed.font_size() == doctest::Approx(22.f));
	CHECK(computed.width().value == doctest::Approx(23.f));

	document->Close();
	TestsShell::ShutdownShell();
}

TEST_CASE("math_expression.invalid")
{
	const String invalid_values[] = {
		"calc(10% + 5px)",
		"calc(5px * 5px)",
		"calc(1px / 0)",
		"calc(1px+1px)",
		"calc(1px + )",
		"min(1px, 2)",
		"clamp(1px, 2px)",
		"calc(1foo)",
	};

	Context* context = TestsShell::GetContext();
	REQUIRE(context);

	for (const String& value : invalid_values)
	{
		TestsShell::SetNumExpectedWarnings(3);

		ElementDocument* document = LoadDivDocument(context, "width: " + value + ";");
		Element* div = document->GetElementById("div");
		REQUIRE(div);
		CHECK(div->GetComputedValues().width().type == Style::LengthPercentageAuto::Auto);

		document->Close();
	}

	TestsShell::ShutdownShell();
}
