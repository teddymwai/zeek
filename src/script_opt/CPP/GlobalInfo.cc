// See the file "COPYING" in the main distribution directory for copyright.

#include "zeek/ZeekString.h"
#include "zeek/Desc.h"
#include "zeek/RE.h"
#include "zeek/script_opt/CPP/Compile.h"

using namespace std;

namespace zeek::detail
	{

string CPP_GlobalsInfo::Name(int index) const
	{
	return base_name + "[" + Fmt(index) + "]";
	}

void CPP_GlobalsInfo::AddInstance(shared_ptr<CPP_GlobalInfo> g)
	{
	auto init_cohort = g->InitCohort();

	if ( static_cast<int>(instances.size()) <= init_cohort )
		instances.resize(init_cohort + 1);

	g->SetOffset(this, size++);

	instances[init_cohort].push_back(move(g));
	}

string CPP_GlobalsInfo::Declare() const
	{
	return string("std::vector<") + CPPType() + "> " + base_name + ";";
	}

void CPP_GlobalsInfo::GenerateInitializers(CPPCompile* c)
	{
	c->NL();

	c->Emit("CPP_Globals<%s> %s = CPP_Globals<%s>(%s, ", CPPType(),
	        InitializersName(), CPPType(), base_name);

	c->IndentUp();
	c->Emit("{");

	for ( auto& cohort : instances )
		{
		c->Emit("{");

		for ( auto& co : cohort )
			c->Emit("%s,", co->Initializer());

		c->Emit("},");
		}

	c->Emit("}");
	c->IndentDown();
	c->Emit(");");
	}


StringConstantInfo::StringConstantInfo(ValPtr v)
	: CPP_GlobalInfo()
	{
	auto s = v->AsString();
	const char* b = (const char*)(s->Bytes());

	len = s->Len();
	rep = CPPEscape(b, len);
	}

string StringConstantInfo::Initializer() const
	{
	return string("CPP_StringConst(") + Fmt(len) + ", " + rep + ")";
	}

PatternConstantInfo::PatternConstantInfo(ValPtr v)
	: CPP_GlobalInfo()
	{
	auto re = v->AsPatternVal()->Get();
	pattern = CPPEscape(re->OrigText());
	is_case_insensitive = re->IsCaseInsensitive();
	}

string PatternConstantInfo::Initializer() const
	{
	return string("CPP_PatternConst(") + pattern + ", " + Fmt(is_case_insensitive) + ")";
	}

DescConstantInfo::DescConstantInfo(ValPtr v)
	: CPP_GlobalInfo()
	{
	ODesc d;
	v->Describe(&d);
	init = d.Description();
	}

string DescConstantInfo::Initializer() const
	{
	return string("CPP_") + gls->Tag() + "Const(\"" + init + "\")";
	}


AttrInfo::AttrInfo(CPPCompile* c, const AttrPtr& attr)
	: CPP_GlobalInfo()
	{
	tag = c->AttrName(attr->Tag());
	auto a_e = attr->GetExpr();

	if ( a_e )
		{
		auto gi = c->RegisterType(a_e->GetType());
		init_cohort = max(init_cohort, gi->InitCohort() + 1);

		auto expr_type = gi->Name();

		if ( ! CPPCompile::IsSimpleInitExpr(a_e) )
			expr_param = c->InitExprName(a_e);

		else if ( a_e->Tag() == EXPR_CONST )
			expr_param = string("make_intrusive<ConstExpr>(") + c->GenExpr(a_e, CPPCompile::GEN_VAL_PTR) + ")";

		else if ( a_e->Tag() == EXPR_NAME )
                        expr_param = string("make_intrusive<NameExpr>(") + c->GlobalName(a_e) + ")";

		else
			{
			ASSERT(a_e->Tag() == EXPR_RECORD_COERCE);
                        expr_param = "make_intrusive<RecordCoerceExpr>(make_intrusive<RecordConstructorExpr>(";
			expr_param += "make_intrusive<ListExpr>()), cast_intrusive<RecordType>(";
			expr_param += expr_type + "))";
			}
		}

	else
		expr_param = "nullptr";
	}

string AttrInfo::Initializer() const
	{
	return string("CPP_Attr(") + tag + ", " + expr_param + ")";
	}

AttrsInfo::AttrsInfo(CPPCompile* c, const AttributesPtr& _attrs)
	: CPP_GlobalInfo()
	{
	for ( const auto& a : _attrs->GetAttrs() )
		{
		ASSERT(c->processed_attr.count(a.get()) > 0);
		auto gi = c->processed_attr[a.get()];
		init_cohort = max(init_cohort, gi->InitCohort() + 1);
		attrs.push_back(gi->Offset());
		}
	}

string AttrsInfo::Initializer() const
	{
	string attr_list;

	for ( auto a : attrs )
		attr_list += Fmt(a) + ", ";

	return string("CPP_Attrs({ ") + attr_list + "})";
	}


string BaseTypeInfo::Initializer() const
	{
	return string("CPP_BaseType(") + CPPCompile::TypeTagName(t->Tag()) + ")";
	}

string EnumTypeInfo::Initializer() const
	{
	string elem_list, val_list;
	auto et = t->AsEnumType();

	for ( const auto& name_pair : et->Names() )
		{
		elem_list += string("\"") + name_pair.first + "\", ";
		val_list += Fmt(int(name_pair.second)) + ", ";
		}

	return string("CPP_EnumType(\"") + t->GetName() + "\", { " + elem_list + "}, { " + val_list + "})";
	}

string OpaqueTypeInfo::Initializer() const
	{
	return string("CPP_OpaqueType(\"") + t->GetName() + "\")";
	}

TypeTypeInfo::TypeTypeInfo(CPPCompile* c, TypePtr _t)
	: AbstractTypeInfo(move(_t))
	{
	auto gi = c->RegisterType(t->AsTypeType()->GetType());
	tt_offset = gi->Offset();
	init_cohort = gi->InitCohort();
	}

string TypeTypeInfo::Initializer() const
	{
	return string("CPP_TypeType(") + Fmt(tt_offset) + ")";
	}

VectorTypeInfo::VectorTypeInfo(CPPCompile* c, TypePtr _t)
	: AbstractTypeInfo(move(_t))
	{
	auto gi = c->RegisterType(t->Yield());
	yt_offset = gi->Offset();
	init_cohort = gi->InitCohort();
	}

string VectorTypeInfo::Initializer() const
	{
	return string("CPP_VectorType(") + Fmt(yt_offset) + ")";
	}

ListTypeInfo::ListTypeInfo(CPPCompile* c, TypePtr _t)
	: AbstractTypeInfo(move(_t))
	{
	const auto& tl = t->AsTypeList()->GetTypes();

	for ( auto& tl_i : tl )
		{
		auto gi = c->RegisterType(tl_i);
		type_offsets.push_back(gi->Offset());
		init_cohort = max(init_cohort, gi->InitCohort());
		}
	}

string ListTypeInfo::Initializer() const
	{
	string type_list;
	for ( auto& t : type_offsets )
		type_list += Fmt(t) + ", ";

	return string("CPP_TypeList({ ") + type_list + "})";
	}

TableTypeInfo::TableTypeInfo(CPPCompile* c, TypePtr _t)
	: AbstractTypeInfo(move(_t))
	{
	auto tbl = t->AsTableType();

	auto gi = c->RegisterType(tbl->GetIndices());
	indices = gi->Offset();
	init_cohort = gi->InitCohort();

	if ( tbl->Yield() )
		{
		gi = c->RegisterType(tbl->Yield());
		yield = gi->Offset();
		init_cohort = max(init_cohort, gi->InitCohort());
		}
	}

string TableTypeInfo::Initializer() const
	{
	return string("CPP_TableType(") + Fmt(indices) + ", " + Fmt(yield) + ")";
	}

FuncTypeInfo::FuncTypeInfo(CPPCompile* c, TypePtr _t)
	: AbstractTypeInfo(move(_t))
	{
	auto f = t->AsFuncType();

	flavor = f->Flavor();
	auto gi = c->RegisterType(f->Params());
	params = gi->Offset();
	init_cohort = gi->InitCohort();

	if ( f->Yield() )
		{
		gi = c->RegisterType(f->Yield());
		yield = gi->Offset();
		init_cohort = max(init_cohort, gi->InitCohort());
		}
	}

string FuncTypeInfo::Initializer() const
	{
	string fl_name;
	if ( flavor == FUNC_FLAVOR_FUNCTION )
		fl_name = "FUNC_FLAVOR_FUNCTION";
	else if ( flavor == FUNC_FLAVOR_EVENT )
		fl_name = "FUNC_FLAVOR_EVENT";
	else if ( flavor == FUNC_FLAVOR_HOOK )
		fl_name = "FUNC_FLAVOR_HOOK";

	return string("CPP_FuncType(") + Fmt(params) + ", " + Fmt(yield) + ", " + fl_name + ")";
	}

	} // zeek::detail