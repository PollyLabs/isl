#ifndef ISL_INTERFACE_GENERATOR_H
#define ISL_INTERFACE_GENERATOR_H

#include <map>
#include <set>
#include <string>

#include <clang/AST/Decl.h>

using namespace std;
using namespace clang;

/* isl_class collects all constructors and methods for an isl "class".
 * "name" is the name of the class.
 * If this object describes a subclass of a C type, then
 * "subclass_name" is the name of that subclass.  Otherwise,
 * it is equal to "name".
 * "type" is the declaration that introduces the type.
 * "methods" contains the set of methods, grouped by method name.
 * "fn_to_str" is a reference to the *_to_str method of this class, if any.
 * "fn_copy" is a reference to the *_copy method of this class, if any.
 * "fn_free" is a reference to the *_free method of this class, if any.
 * "fn_type" is a reference to a function that described subclasses, if any.
 * If "fn_type" is set, then "type_subclasses" maps the values returned
 * by that function to the names of the corresponding subclasses.
 */
struct isl_class {
	string name;
	string subclass_name;
	RecordDecl *type;
	set<FunctionDecl *> constructors;
	map<string, set<FunctionDecl *> > methods;
	map<int, string> type_subclasses;
	FunctionDecl *fn_type;
	FunctionDecl *fn_to_str;
	FunctionDecl *fn_copy;
	FunctionDecl *fn_free;

	/* Is this class a subclass based on a type function? */
	bool is_type_subclass() const { return name != subclass_name; }
	/* Extract the method name from the C function name. */
	string method_suffix(const string &function_name) const {
		return function_name.substr(subclass_name.length() + 1);
	}
};

/* Base class for interface generators.
 */
class generator {
protected:
	map<string,isl_class> classes;
	map<string, FunctionDecl *> functions_by_name;

public:
	generator(set<RecordDecl *> &exported_types,
		set<FunctionDecl *> exported_functions,
		set<FunctionDecl *> functions);

	virtual void generate() = 0;
	virtual ~generator() {};

protected:
	void add_subclass(RecordDecl *decl, const string &sub_name);
	void add_class(RecordDecl *decl);
	void add_type_subclasses(FunctionDecl *method);
	void print_class_header(const isl_class &clazz, const string &name,
		const vector<string> &super);
	string drop_type_suffix(string name, FunctionDecl *method);
	void die(const char *msg) __attribute__((noreturn));
	void die(string msg) __attribute__((noreturn));
	vector<string> find_superclasses(Decl *decl);
	bool is_subclass(FunctionDecl *decl);
	bool is_overload(Decl *decl);
	bool is_constructor(Decl *decl);
	bool takes(Decl *decl);
	bool keeps(Decl *decl);
	bool gives(Decl *decl);
	isl_class *method2class(FunctionDecl *fd);
	bool is_isl_ctx(QualType type);
	bool first_arg_is_isl_ctx(FunctionDecl *fd);
	bool callback_takes_arguments(const FunctionProtoType *fn_type);
	bool is_isl_type(QualType type);
	bool is_isl_bool(QualType type);
	bool is_isl_stat(QualType type);
	bool is_long(QualType type);
	bool is_callback(QualType type);
	bool is_string(QualType type);
	bool is_static(const isl_class &clazz, FunctionDecl *method);
	string extract_type(QualType type);
	FunctionDecl *find_by_name(const string &name, bool required);
};

#endif /* ISL_INTERFACE_GENERATOR_H */
