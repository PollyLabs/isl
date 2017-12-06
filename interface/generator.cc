/*
 * Copyright 2011,2015 Sven Verdoolaege. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SVEN VERDOOLAEGE ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SVEN VERDOOLAEGE OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as
 * representing official policies, either expressed or implied, of
 * Sven Verdoolaege.
 */

#include <stdio.h>
#include <iostream>

#include <clang/AST/Attr.h>

#include "isl_config.h"
#include "extract_interface.h"
#include "generator.h"

/* Should "method" be considered to be a static method?
 * That is, is the first argument something other than
 * an instance of the class?
 */
bool generator::is_static(const isl_class &clazz, FunctionDecl *method)
{
	ParmVarDecl *param = method->getParamDecl(0);
	QualType type = param->getOriginalType();

	if (!is_isl_type(type))
		return true;
	return extract_type(type) != clazz.name;
}

/* Find the FunctionDecl with name "name",
 * returning NULL if there is no such FunctionDecl.
 * If "required" is set, then error out if no FunctionDecl can be found.
 */
FunctionDecl *generator::find_by_name(const string &name, bool required)
{
	map<string, FunctionDecl *>::iterator i;

	i = functions_by_name.find(name);
	if (i != functions_by_name.end())
		return i->second;
	if (required)
		die("No " + name + " function found");
	return NULL;
}

/* Add a subclass derived from "decl" called "sub_name" to the set of classes,
 * keeping track of the _to_str, _copy and _free functions, if any, separately.
 * "sub_name" is either the name of the class itself or
 * the name of a type based subclass.
 */
void generator::add_subclass(RecordDecl *decl, const string &sub_name)
{
	string name = decl->getName();

	classes[sub_name].name = name;
	classes[sub_name].subclass_name = sub_name;
	classes[sub_name].type = decl;
	classes[sub_name].fn_to_str = find_by_name(name + "_to_str", false);
	classes[sub_name].fn_copy = find_by_name(name + "_copy", true);
	classes[sub_name].fn_free = find_by_name(name + "_free", true);
}

/* Add a class derived from "decl" to the set of classes,
 * keeping track of the _to_str, _copy and _free functions, if any, separately.
 */
void generator::add_class(RecordDecl *decl)
{
	return add_subclass(decl, decl->getName());
}

/* Given a function "fn_type" that returns the subclass type
 * of a C object, create subclasses for each of the (non-negative)
 * return values.
 *
 * The function "fn_type" is also stored in the superclass,
 * along with all pairs of type values and subclass names.
 */
void generator::add_type_subclasses(FunctionDecl *fn_type)
{
	QualType return_type = fn_type->getReturnType();
	const EnumType *enum_type = return_type->getAs<EnumType>();
	EnumDecl *decl = enum_type->getDecl();
	isl_class *c = method2class(fn_type);
	DeclContext::decl_iterator i;

	c->fn_type = fn_type;
	for (i = decl->decls_begin(); i != decl->decls_end(); ++i) {
		EnumConstantDecl *ecd = dyn_cast<EnumConstantDecl>(*i);
		int val = (int) ecd->getInitVal().getExtValue();
		string name = ecd->getNameAsString();

		if (val < 0)
			continue;
		c->type_subclasses[val] = name;
		add_subclass(c->type, name);
	}
}

/* Collect all functions that belong to a certain type, separating
 * constructors from regular methods and keeping track of the _to_str,
 * _copy and _free functions, if any, separately.  If there are any overloaded
 * functions, then they are grouped based on their name after removing the
 * argument type suffix.
 * Check for functions that describe subclasses before considering
 * any other functions in order to be able to detect those other
 * functions as belonging to the subclasses.
 */
generator::generator(set<RecordDecl *> &exported_types,
	set<FunctionDecl *> exported_functions, set<FunctionDecl *> functions)
{
	map<string, isl_class>::iterator ci;

	set<FunctionDecl *>::iterator in;
	for (in = functions.begin(); in != functions.end(); ++in) {
		FunctionDecl *decl = *in;
		functions_by_name[decl->getName()] = decl;
	}

	set<RecordDecl *>::iterator it;
	for (it = exported_types.begin(); it != exported_types.end(); ++it)
		add_class(*it);

	for (in = exported_functions.begin(); in != exported_functions.end();
	     ++in) {
		if (!is_subclass(*in))
			continue;
		add_type_subclasses(*in);
	}

	for (in = exported_functions.begin(); in != exported_functions.end();
	     ++in) {
		isl_class *c;

		if (is_subclass(*in))
			continue;

		c = method2class(*in);
		if (!c)
			continue;
		if (is_constructor(*in)) {
			c->constructors.insert(*in);
		} else {
			FunctionDecl *method = *in;
			string fullname = method->getName();
			fullname = drop_type_suffix(fullname, method);
			c->methods[fullname].insert(method);
		}
	}
}

/* Print error message "msg" and abort.
 */
void generator::die(const char *msg)
{
	fprintf(stderr, "%s\n", msg);
	abort();
}

/* Print error message "msg" and abort.
 */
void generator::die(string msg)
{
	die(msg.c_str());
}

/* Return a sequence of the types of which the given type declaration is
 * marked as being a subtype.
 * The order of the types is the opposite of the order in which they
 * appear in the source.  In particular, the first annotation
 * is the one that is closest to the annotated type and the corresponding
 * type is then also the first that will appear in the sequence of types.
 */
std::vector<string> generator::find_superclasses(Decl *decl)
{
	vector<string> super;

	if (!decl->hasAttrs())
		return super;

	string sub = "isl_subclass";
	size_t len = sub.length();
	AttrVec attrs = decl->getAttrs();
	for (AttrVec::const_iterator i = attrs.begin(); i != attrs.end(); ++i) {
		const AnnotateAttr *ann = dyn_cast<AnnotateAttr>(*i);
		if (!ann)
			continue;
		string s = ann->getAnnotation().str();
		if (s.substr(0, len) == sub) {
			s = s.substr(len + 1, s.length() - len  - 2);
			super.push_back(s);
		}
	}

	return super;
}

/* Is "decl" marked as describing subclasses?
 */
bool generator::is_subclass(FunctionDecl *decl)
{
	return find_superclasses(decl).size() > 0;
}

/* Is decl marked as being part of an overloaded method?
 */
bool generator::is_overload(Decl *decl)
{
	return has_annotation(decl, "isl_overload");
}

/* Is decl marked as a constructor?
 */
bool generator::is_constructor(Decl *decl)
{
	return has_annotation(decl, "isl_constructor");
}

/* Is decl marked as consuming a reference?
 */
bool generator::takes(Decl *decl)
{
	return has_annotation(decl, "isl_take");
}

/* Is decl marked as preserving a reference?
 */
bool generator::keeps(Decl *decl)
{
	return has_annotation(decl, "isl_keep");
}

/* Is decl marked as returning a reference that is required to be freed.
 */
bool generator::gives(Decl *decl)
{
	return has_annotation(decl, "isl_give");
}

/* Return the class that has a name that best matches the initial part
 * of the name of function "fd" or NULL if no such class could be found.
 */
isl_class *generator::method2class(FunctionDecl *fd)
{
	string best;
	map<string, isl_class>::iterator ci;
	string name = fd->getNameAsString();

	for (ci = classes.begin(); ci != classes.end(); ++ci) {
		size_t len = ci->first.length();
		if (len > best.length() && name.substr(0, len) == ci->first)
			best = ci->first;
	}

	if (classes.find(best) == classes.end()) {
		cerr << "Unable to find class of " << name << endl;
		return NULL;
	}

	return &classes[best];
}

/* Is "type" the type "isl_ctx *"?
 */
bool generator::is_isl_ctx(QualType type)
{
	if (!type->isPointerType())
		return 0;
	type = type->getPointeeType();
	if (type.getAsString() != "isl_ctx")
		return false;

	return true;
}

/* Is the first argument of "fd" of type "isl_ctx *"?
 */
bool generator::first_arg_is_isl_ctx(FunctionDecl *fd)
{
	ParmVarDecl *param;

	if (fd->getNumParams() < 1)
		return false;

	param = fd->getParamDecl(0);
	return is_isl_ctx(param->getOriginalType());
}

/* Does a callback of type "fn_type" take its arguments?
 *
 * The memory management annotations of arguments to function pointers
 * are not recorded by clang, so the information cannot be extracted
 * from "fn_type".
 * Assume all callbacks take their arguments.
 */
bool generator::callback_takes_arguments(const FunctionProtoType *fn_type)
{
	return true;
}

/* Is "type" that of a pointer to an isl_* structure?
 */
bool generator::is_isl_type(QualType type)
{
	if (type->isPointerType()) {
		string s;

		type = type->getPointeeType();
		if (type->isFunctionType())
			return false;
		s = type.getAsString();
		return s.substr(0, 4) == "isl_";
	}

	return false;
}

/* Is "type" the type isl_bool?
 */
bool generator::is_isl_bool(QualType type)
{
	string s;

	if (type->isPointerType())
		return false;

	s = type.getAsString();
	return s == "isl_bool";
}

/* Is "type" the type isl_stat?
 */
bool generator::is_isl_stat(QualType type)
{
	string s;

	if (type->isPointerType())
		return false;

	s = type.getAsString();
	return s == "isl_stat";
}


/* Is "type" that of a pointer to a function?
 */
bool generator::is_callback(QualType type)
{
	if (!type->isPointerType())
		return false;
	type = type->getPointeeType();
	return type->isFunctionType();
}

/* Is "type" that of "char *" of "const char *"?
 */
bool generator::is_string(QualType type)
{
	if (type->isPointerType()) {
		string s = type->getPointeeType().getAsString();
		return s == "const char" || s == "char";
	}

	return false;
}

/* Is "type" that of "long"?
 */
bool generator::is_long(QualType type)
{
	const BuiltinType *builtin = type->getAs<BuiltinType>();
	return builtin && builtin->getKind() == BuiltinType::Long;
}

/* Return the name of the type that "type" points to.
 * The input "type" is assumed to be a pointer type.
 */
string generator::extract_type(QualType type)
{
	if (type->isPointerType())
		return type->getPointeeType().getAsString();
	die("Cannot extract type from non-pointer type");
}

/* If "method" is overloaded, then drop the suffix of "name"
 * corresponding to the type of the final argument and
 * return the modified name (or the original name if
 * no modifications were made).
 */
string generator::drop_type_suffix(string name, FunctionDecl *method)
{
	int num_params;
	ParmVarDecl *param;
	string type;
	size_t name_len, type_len;

	if (!is_overload(method))
		return name;

	num_params = method->getNumParams();
	param = method->getParamDecl(num_params - 1);
	type = extract_type(param->getOriginalType());
	type = type.substr(4);
	name_len = name.length();
	type_len = type.length();

	if (name_len > type_len && name.substr(name_len - type_len) == type)
		name = name.substr(0, name_len - type_len - 1);

	return name;
}
