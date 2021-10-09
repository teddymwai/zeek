// See the file "COPYING" in the main distribution directory for copyright.

// Classes for tracking C++ globals used by the generated code.
//
// One group of classes manages the information for defining the globals in
// C++ (i.e., generating C++ code for the declarations and initializations).
//
// Another group embodies those globals as used at run-time by the
// generated code.  This second group is created by compiling the code
// generated by the first group.

#include "zeek/Val.h"

#pragma once

namespace zeek::detail
	{

class CPPCompile;

// Abstract class for tracking the information about a single global.
// This might be a stand-alone global, or a global that's ultimately
// instantiated as part of a CPP_Globals object.
class CPP_GlobalInfo;

// Abstract class for tracking the information about a set of globals,
// each of which is an element of a CPP_Globals object.
class CPP_GlobalsInfo
	{
public:
	CPP_GlobalsInfo(std::string _tag, std::string _type)
		: tag(std::move(_tag)), type(std::move(_type))
		{
		base_name = std::string("CPP__") + tag + "const__";
		}

	virtual ~CPP_GlobalsInfo() { }

	std::string Name(int index) const;
	std::string NextName() const { return Name(Size()); }

	int Size() const { return size; }
	int MaxCohort() const { return static_cast<int>(instances.size()) - 1; }

	const std::string& Type() const { return type; }
	std::string CPPType() const { return type + "ValPtr"; }

	void AddInstance(std::shared_ptr<CPP_GlobalInfo> g);

	std::string Declare() const;

	void GenerateInitializers(CPPCompile* cc);

protected:
	int size = 0;	// total number of globals

	// The outer vector is indexed by initialization cohort.
	std::vector<std::vector<std::shared_ptr<CPP_GlobalInfo>>> instances;

	// Tag used to distinguish a particular set of constants.
	std::string tag;

	// C++ type associated with a single instance of these constants.
	std::string type;

	// C++ name for this set of constants.
	std::string base_name;
	};


class CPP_GlobalInfo
	{
public:
	// Constructor used for stand-alone globals.  The second
	// argument specifies the "initialization cohort", i.e.,
	// the group of initializations to use for this global.
	// Initializations begin with cohort 0 and then move on to
	// cohort 1, 2, etc.
	CPP_GlobalInfo(std::string _name, std::string _type, int _init_cohort)
		: name(std::move(_name)), type(std::move(_type)), init_cohort(_init_cohort)
		{ }

	// Constructor used for a global that will be part of a CPP_GlobalsInfo
	// object.  The rest of its initialization will be done by
	// CPP_GlobalsInfo::AddInstance.
	CPP_GlobalInfo(int _init_cohort)
		: init_cohort(_init_cohort)
		{ }

	virtual ~CPP_GlobalInfo() { }

	void SetOffset(const CPP_GlobalsInfo* _gls, int _offset)
		{
		gls = _gls;
		offset = _offset;
		}

	// Returns the name that should be used for referring to this
	// global in the generated code.
	std::string Name() const { return gls ? gls->Name(offset) : name; }

	int InitCohort() const { return init_cohort; }

	// Returns a C++ declaration for this global.  Not used if
	// the global is part of a CPP_Globals object.
	std::string Declare() const { return type + " " + Name() + ";"; }

	// Some globals require *pre*-initialization before they are
	// fully initialized.  (These arise when there are circularities
	// in definitions, such as for recursive types.)  The first of
	// these methods is a predicate indicating whether the global
	// needs such pre-initialization, and if so, the second provides
	// the pre-initialization code snippet.
	virtual bool HasPreInit() const { return false; }
	virtual std::string PreInit() const { return ""; }

	// Returns a C++ initialization for creating this global.
	virtual std::string Initializer() const = 0;

protected:
	std::string name;
	std::string type;
	int init_cohort;

	const CPP_GlobalsInfo* gls = nullptr;
	int offset = -1;	// offset for CPP_GlobalsInfo, if non-nil
	};

class StringConstantInfo : public CPP_GlobalInfo
	{
public:
	StringConstantInfo(ValPtr v);

	std::string Initializer() const override;

private:
	std::string rep;
	int len;
	};

class PatternConstantInfo : public CPP_GlobalInfo
	{
public:
	PatternConstantInfo(ValPtr v);

	std::string Initializer() const override;

private:
	std::string pattern;
	int is_case_insensitive;
	};

class DescConstantInfo : public CPP_GlobalInfo
	{
public:
	DescConstantInfo(ValPtr v);

	std::string Initializer() const override;

private:
	std::string init;
	};


template <class T>
class CPP_Global
	{
public:
	virtual ~CPP_Global() { }
	virtual T Generate() const { return nullptr; }
	};

template <class T>
class CPP_Globals
	{
public:
	CPP_Globals(std::vector<CPP_Global<T>> _inits)
		: inits(std::move(_inits))
		{ }

private:
	std::vector<CPP_Global<T>> inits;
	};

class CPP_StringConst : public CPP_Global<StringValPtr>
	{
public:
	CPP_StringConst(int _len, const char* _chars)
		: len(_len), chars(_chars) { }

	StringValPtr Generate() const override
		{ return make_intrusive<StringVal>(len, chars); }

private:
	int len;
	const char* chars;
	};

class CPP_PatternConst : public CPP_Global<PatternValPtr>
	{
public:
	CPP_PatternConst(const char* _pattern, int _is_case_insensitive)
		: pattern(_pattern), is_case_insensitive(_is_case_insensitive) { }

	PatternValPtr Generate() const override;

private:
	const char* pattern;
	int is_case_insensitive;
	};

class CPP_AddrConst : public CPP_Global<AddrValPtr>
	{
public:
	CPP_AddrConst(const char* _init)
		: init(_init) { }

	AddrValPtr Generate() const override
		{ return make_intrusive<AddrVal>(init); }

private:
	const char* init;
	};

class CPP_SubNetConst : public CPP_Global<SubNetValPtr>
	{
public:
	CPP_SubNetConst(const char* _init)
		: init(_init) { }

	SubNetValPtr Generate() const override
		{ return make_intrusive<SubNetVal>(init); }

private:
	const char* init;
	};


	} // zeek::detail

// base_type(char*)
// get_enum_type__CPP(" + char* + ");
// get_record_type__CPP(" + char* + ");
// get_record_type__CPP(nullptr);
// make_intrusive<SubNetType>();
// make_intrusive<TypeList>();
// make_intrusive<OpaqueType>(" char* + ");
// 
// make_intrusive<FileType>(TYPEINDEX);
// make_intrusive<FuncType>(cast_intrusive<RecordType>(TYPEINDEX), TYPEINDEX|nullpt
// r, FLAVOR);
// make_intrusive<SetType>(cast_intrusive<TypeList>(TYPEINDEX), nullptr);
// make_intrusive<TableType>(cast_intrusive<TypeList>(TYPEINDEX), TYPEINDEX);
// make_intrusive<TypeType>(TYPEINDEX);
// make_intrusive<VectorType>(TYPEINDEX);