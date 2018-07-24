#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <map>

#include "semant.h"
#include "utilities.h"

extern int semant_debug;
extern char *curr_filename;

ClassTable *classtable;

std::map<Symbol, Class_> class_map;

typedef std::pair<Symbol, Symbol> method_id;
std::map<method_id, method_class *> method_env;


//////////////////////////////////////////////////////////////////////
//
// Symbols
//
// For convenience, a large number of symbols are predefined here.
// These symbols include the primitive type and method names, as well
// as fixed names used by the runtime system.
//
//////////////////////////////////////////////////////////////////////
static Symbol
    arg,
    arg2,
    Bool,
    concat,
    cool_abort,
    copy,
    Int,
    in_int,
    in_string,
    IO,
    length,
    Main,
    main_meth,
    No_class,
    No_type,
    Object,
    out_int,
    out_string,
    prim_slot,
    self,
    SELF_TYPE,
    Str,
    str_field,
    substr,
    type_name,
    val;
//
// Initializing the predefined symbols.
//
static void initialize_constants(void)
{
    arg         = idtable.add_string("arg");
    arg2        = idtable.add_string("arg2");
    Bool        = idtable.add_string("Bool");
    concat      = idtable.add_string("concat");
    cool_abort  = idtable.add_string("abort");
    copy        = idtable.add_string("copy");
    Int         = idtable.add_string("Int");
    in_int      = idtable.add_string("in_int");
    in_string   = idtable.add_string("in_string");
    IO          = idtable.add_string("IO");
    length      = idtable.add_string("length");
    Main        = idtable.add_string("Main");
    main_meth   = idtable.add_string("main");
    //   _no_class is a symbol that can't be the name of any
    //   user-defined class.
    No_class    = idtable.add_string("_no_class");
    No_type     = idtable.add_string("_no_type");
    Object      = idtable.add_string("Object");
    out_int     = idtable.add_string("out_int");
    out_string  = idtable.add_string("out_string");
    prim_slot   = idtable.add_string("_prim_slot");
    self        = idtable.add_string("self");
    SELF_TYPE   = idtable.add_string("SELF_TYPE");
    Str         = idtable.add_string("String");
    str_field   = idtable.add_string("_str_field");
    substr      = idtable.add_string("substr");
    type_name   = idtable.add_string("type_name");
    val         = idtable.add_string("_val");
}


ClassTable::ClassTable(Classes classes) : semant_errors(0) , error_stream(cerr) {
    install_basic_classes();

    for(int i = classes->first(); classes->more(i); i = classes->next(i)) {
        Class_ cls = classes->nth(i);
        Symbol name = cls->get_name();

        if (class_map.find(name) != class_map.end()) {
            semant_error(cls) << "redefinition of class " << name << "." << std::endl;
            return;
        }

        class_map.insert(std::make_pair(name, cls));
    }

    if (class_map.find(Main) == class_map.end()) {
        semant_error() << "Class Main is not defined." << std::endl;
        return;
    }

    // This is probably slow, but I put simplicity first in this case.
    for (int i = classes->first(); classes->more(i); i = classes->next(i)) {
        Class_ cls = classes->nth(i);
        Symbol starting_class = cls->get_name();

        for (Symbol parent = cls->get_parent(); parent != Object; cls = class_map[parent], parent = cls->get_parent()) {
            if (class_map.find(parent) == class_map.end()) {
                semant_error(cls) << "Parent class " << parent << " is not defined." << std::endl;
                return;
            }

            if (parent == Int || parent == Bool || parent == Str || parent == SELF_TYPE) {
                semant_error(cls) << "Classes cannot inherit from basic class" << parent << std::endl;
                return;
            }

            if (parent == starting_class) {
                semant_error(cls) << "An inheritance cycle has been detected." << std::endl;
                return;
            }
        }
    }
}

void ClassTable::install_basic_classes() {

    // The tree package uses these globals to annotate the classes built below.
   // curr_lineno  = 0;
    Symbol filename = stringtable.add_string("<basic class>");

    // The following demonstrates how to create dummy parse trees to
    // refer to basic Cool classes.  There's no need for method
    // bodies -- these are already built into the runtime system.

    // IMPORTANT: The results of the following expressions are
    // stored in local variables.  You will want to do something
    // with those variables at the end of this method to make this
    // code meaningful.

    //
    // The Object class has no parent class. Its methods are
    //        abort() : Object    aborts the program
    //        type_name() : Str   returns a string representation of class name
    //        copy() : SELF_TYPE  returns a copy of the object
    //
    // There is no need for method bodies in the basic classes---these
    // are already built in to the runtime system.

    Class_ Object_class =
    class_(Object,
           No_class,
           append_Features(
                           append_Features(
                           single_Features(method(cool_abort, nil_Formals(), Object, no_expr())),
                           single_Features(method(type_name, nil_Formals(), Str, no_expr()))),
                           single_Features(method(copy, nil_Formals(), SELF_TYPE, no_expr()))),
       filename);

    //
    // The IO class inherits from Object. Its methods are
    //        out_string(Str) : SELF_TYPE       writes a string to the output
    //        out_int(Int) : SELF_TYPE            "    an int    "  "     "
    //        in_string() : Str                 reads a string from the input
    //        in_int() : Int                      "   an int     "  "     "
    //
    Class_ IO_class =
    class_(IO,
           Object,
           append_Features(
               append_Features(
                       append_Features(
                               single_Features(method(out_string, single_Formals(formal(arg, Str)),
                                               SELF_TYPE, no_expr())),
                                   single_Features(method(out_int, single_Formals(formal(arg, Int)),
                                                   SELF_TYPE, no_expr()))),
                           single_Features(method(in_string, nil_Formals(), Str, no_expr()))),
               single_Features(method(in_int, nil_Formals(), Int, no_expr()))),
           filename);

    //
    // The Int class has no methods and only a single attribute, the
    // "val" for the integer.
    //
    Class_ Int_class =
    class_(Int,
           Object,
           single_Features(attr(val, prim_slot, no_expr())),
           filename);

    //
    // Bool also has only the "val" slot.
    //
    Class_ Bool_class =
    class_(Bool, Object, single_Features(attr(val, prim_slot, no_expr())),filename);

    //
    // The class Str has a number of slots and operations:
    //       val                                  the length of the string
    //       str_field                            the string itself
    //       length() : Int                       returns length of the string
    //       concat(arg: Str) : Str               performs string concatenation
    //       substr(arg: Int, arg2: Int): Str     substring selection
    //
    Class_ Str_class =
    class_(Str,
           Object,
           append_Features(
                   append_Features(
                           append_Features(
                                   append_Features(
                                           single_Features(attr(val, Int, no_expr())),
                                           single_Features(attr(str_field, prim_slot, no_expr()))),
                                   single_Features(method(length, nil_Formals(), Int, no_expr()))),
                           single_Features(method(concat,
                                           single_Formals(formal(arg, Str)),
                                           Str,
                                           no_expr()))),
                   single_Features(method(substr,
                                   append_Formals(single_Formals(formal(arg, Int)),
                                                  single_Formals(formal(arg2, Int))),
                                   Str,
                                   no_expr()))),
           filename);

    class_map.insert(std::make_pair(Object, Object_class));
    class_map.insert(std::make_pair(IO, IO_class));
    class_map.insert(std::make_pair(Int, Int_class));
    class_map.insert(std::make_pair(Bool, Bool_class));
    class_map.insert(std::make_pair(Str, Str_class));
}

////////////////////////////////////////////////////////////////////
//
// semant_error is an overloaded function for reporting errors
// during semantic analysis.  There are three versions:
//
//    ostream& ClassTable::semant_error()
//
//    ostream& ClassTable::semant_error(Class_ c)
//       print line number and filename for `c'
//
//    ostream& ClassTable::semant_error(Symbol filename, tree_node *t)
//       print a line number and filename
//
///////////////////////////////////////////////////////////////////

ostream& ClassTable::semant_error(Class_ c)
{
    return semant_error(c->get_filename(),c);
}

ostream& ClassTable::semant_error(Symbol filename, tree_node *t)
{
    error_stream << filename << ":" << t->get_line_number() << ": ";
    return semant_error();
}

ostream& ClassTable::semant_error()
{
    semant_errors++;
    return error_stream;
}

bool cls_is_defined(Symbol cls_name) {
    if (cls_name == SELF_TYPE) {
        return true;
    }

    if (class_map.find(cls_name) == class_map.end()) {
        return false;
    }
    return true;
}

/*
 * Returns true if sub is a subclass of super.
 */
bool is_subclass(Symbol sub, Symbol super, type_env &tenv) {
    if (sub == SELF_TYPE) {
        sub = tenv.c->get_name();
    }
    if (super == SELF_TYPE) {
        super = tenv.c->get_name();
    }

    for (auto c_iter = class_map.find(sub);
         c_iter != class_map.end();
         c_iter = class_map.find(c_iter->second->get_parent())
         ) {

        if (c_iter->second->get_name() == super) {
            return true;
        }
    }

    return false;
}

/*
 * Returns the first common ancestor of classes a and b.
 */
Symbol cls_join(Symbol a, Symbol b, type_env &tenv) {
    Class_ cls = class_map[a];

    for (; !is_subclass(b, cls->get_name(), tenv); cls = class_map[cls->get_parent()]) {
    }

    return cls->get_name();
}

// Type Checking Methods

Symbol method_class::typecheck(type_env &tenv) {
    tenv.o.enterscope();

    tenv.o.addid(self, new Symbol(SELF_TYPE));

    for (int i = formals->first(); formals->more(i); i = formals->next(i)) {
        Formal f = formals->nth(i);
        tenv.o.addid(f->get_name(), new Symbol(f->get_type_decl()));
    }

    Symbol t0_ = expr->typecheck(tenv);
    tenv.o.exitscope();

    return Object;
}

Symbol attr_class::typecheck(type_env &tenv) {
    Symbol t0 = type_decl;
    Symbol t1 = init->typecheck(tenv);

    if (t1 != No_type) {
        if (is_subclass(t1, t0, tenv) == false) {
            classtable->semant_error(tenv.c->get_filename(), this) <<
                "Inferred type " << t1 << " of initialization of attribute a " <<
                "does not conform to declared type " << t0 << "." << std::endl;
        }
    }

    return t0;
}

Symbol assign_class::typecheck(type_env &tenv) {
    type = Object;
    Symbol *t = tenv.o.lookup(name);

    if (!t) {
        classtable->semant_error(tenv.c->get_filename(), this) <<
            "Assignment to undeclared variable " << name << "." << std::endl;
        return type;
    }

    Symbol t_ = expr->typecheck(tenv);

    if (is_subclass(t_, *t, tenv) == false) {
        classtable->semant_error(tenv.c->get_filename(), this) <<
            "Type " << t_ << " of assigned expression does not conform to "
            "declared type " << *t << " of identifier " << name << "." << std::endl;
        return type;
    }

    type = t_;
    return type;
}

Symbol static_dispatch_class::typecheck(type_env &tenv) { return Object; }
Symbol dispatch_class::typecheck(type_env &tenv) { return Object; }

Symbol cond_class::typecheck(type_env &tenv) {
    Symbol t1 = pred->typecheck(tenv);
    Symbol t2 = then_exp->typecheck(tenv);
    Symbol t3 = else_exp->typecheck(tenv);

    if (t1 != Bool) {
        classtable->semant_error(tenv.c->get_filename(), this) <<
            "Predicate of 'if' does not have type Bool." << std::endl;
    }

    type = cls_join(t2, t3, tenv);
    return type;
}

Symbol loop_class::typecheck(type_env &tenv) { return Object; }
Symbol typcase_class::typecheck(type_env &tenv) { return Object; }

Symbol block_class::typecheck(type_env &tenv) {
    for (int i = body->first(); body->more(i); i = body->next(i)) {
        type = body->nth(i)->typecheck(tenv);
    }

    return type;
}

Symbol let_class::typecheck(type_env &tenv) { return Object; }

Symbol plus_class::typecheck(type_env &tenv) {
    Symbol t_e1 = e1->typecheck(tenv);
    Symbol t_e2 = e2->typecheck(tenv);

    if (t_e1 != Int || t_e2 != Int) {
        classtable->semant_error(tenv.c->get_filename(), this) <<
            "non-Int arguments: " << t_e1 << " + " << t_e2 << std::endl;
        type = Object;
    } else {
        type = Int;
    }

    return type;
}

Symbol sub_class::typecheck(type_env &tenv) {
    Symbol t_e1 = e1->typecheck(tenv);
    Symbol t_e2 = e2->typecheck(tenv);

    if (t_e1 != Int || t_e2 != Int) {
        classtable->semant_error(tenv.c->get_filename(), this) <<
            "non-Int arguments: " << t_e1 << " - " << t_e2 << std::endl;
        type = Object;
    } else {
        type = Int;
    }

    return type;
}

Symbol mul_class::typecheck(type_env &tenv) {
    Symbol t_e1 = e1->typecheck(tenv);
    Symbol t_e2 = e2->typecheck(tenv);

    if (t_e1 != Int || t_e2 != Int) {
        classtable->semant_error(tenv.c->get_filename(), this) <<
            "non-Int arguments: " << t_e1 << " * " << t_e2 << std::endl;
        type = Object;
    } else {
        type = Int;
    }

    return type;
}

Symbol divide_class::typecheck(type_env &tenv) {
    Symbol t_e1 = e1->typecheck(tenv);
    Symbol t_e2 = e2->typecheck(tenv);

    if (t_e1 != Int || t_e2 != Int) {
        classtable->semant_error(tenv.c->get_filename(), this) <<
            "non-Int arguments: " << t_e1 << " / " << t_e2 << std::endl;
        type = Object;
    } else {
        type = Int;
    }

    return type;
}

Symbol neg_class::typecheck(type_env &tenv) {
    type = e1->typecheck(tenv);

    if (type != Int) {
        classtable->semant_error(tenv.c->get_filename(), this) <<
            "Argument of ~ has type " << type << " instead of Int." << std::endl;
        type = Int;
    }

    return type;
}

Symbol lt_class::typecheck(type_env &tenv) {
    Symbol t1 = e1->typecheck(tenv);
    Symbol t2 = e2->typecheck(tenv);

    if (t1 != Int || t2 != Int) {
        classtable->semant_error(tenv.c->get_filename(), this) <<
            "non-Int arguments: " << t1 << " < " << t2 << std::endl;
    }

    type = Bool;
    return type;
}

Symbol eq_class::typecheck(type_env &tenv) {
    Symbol t1 = e1->typecheck(tenv);
    Symbol t2 = e2->typecheck(tenv);

    if (t1 == Int || t1 == Str || t1 == Bool || t2 == Int || t2 == Str || t2 == Bool) {
        if (t1 != t2) {
            classtable->semant_error(tenv.c->get_filename(), this) <<
                "Illegal comparison with a basic type." << std::endl;
        }
    }

    type = Bool;
    return type;
}

Symbol leq_class::typecheck(type_env &tenv) {
    Symbol t1 = e1->typecheck(tenv);
    Symbol t2 = e2->typecheck(tenv);

    if (t1 != Int || t2 != Int) {
        classtable->semant_error(tenv.c->get_filename(), this) <<
            "non-Int arguments: " << t1 << " <= " << t2 << std::endl;
    }

    type = Bool;
    return type;
}

Symbol comp_class::typecheck(type_env &tenv) {
    type = e1->typecheck(tenv);

    if (type != Bool) {
        classtable->semant_error(tenv.c->get_filename(), this) <<
            "Argument of 'not' has type " << type << " instead of Bool." << std::endl;
    }

    type = Bool;
    return type;
}


Symbol int_const_class::typecheck(type_env &tenv) {
    type = Int;
    return type;
}

Symbol bool_const_class::typecheck(type_env &tenv) {
    type = Bool;
    return type;
}

Symbol string_const_class::typecheck(type_env &tenv) {
    type = Str;
    return type;
}

Symbol new__class::typecheck(type_env &tenv) {
    Symbol t = type_name;

    if (! cls_is_defined(t)) {
        classtable->semant_error(tenv.c->get_filename(), this) <<
            "'new' used with undefined class " << t << "." << std::endl;
        type = Object;
    } else {
        type = t;
    }

    return type;
}

Symbol isvoid_class::typecheck(type_env &tenv) {
    e1->typecheck(tenv);

    type = Bool;
    return type;
}

Symbol no_expr_class::typecheck(type_env &tenv) {
    type = No_type;
    return type;
}

Symbol object_class::typecheck(type_env &tenv) {
    Symbol *t = tenv.o.lookup(name);

    if (!t) {
        classtable->semant_error(tenv.c->get_filename(), this) <<
            "Undeclared identifier " << name << "." << std::endl;

        type = Object;
    } else {
        type = *t;
    }

    return type;
}

// ------------------------

void build_method_env() {
    for (auto iter = class_map.begin(); iter != class_map.end(); iter++) {
        Class_ cls = iter->second;

        Features features = cls->get_features();
        for (int i = features->first(); features->more(i); i = features->next(i)) {
            Feature f = features->nth(i);

            method_class *method = dynamic_cast<method_class *>(f);
            if (!method) {
                continue; // f is an attribute not a method, so skip it
            }

            method_env[std::make_pair(cls->get_name(), f->get_name())] = method;
        }
    }
}

/*
 * This function returns true if the method is explicitly declared in the
 * given class else false.
 *
 * IT DOESN'T CHECK WHETHER THE METHOD IS DECLARED IN ANY OF ITS SUPERCLASSES
 */
bool method_is_defined(Symbol class_name, Symbol method_name) {
    if (method_env.find(std::make_pair(class_name, method_name)) == method_env.end()) {
        return false;
    }

    return true;
}

/*
 * This is the get interface of the global Method Environment or formally it
 * returns the result of M(C,f).
 */
method_class *lookup_method(Symbol class_name, Symbol method_name) {
    for (auto c_iter = class_map.find(class_name);
         c_iter != class_map.end();
         c_iter = class_map.find(c_iter->second->get_parent())
         ) {

        auto m_iter = method_env.find(std::make_pair(c_iter->second->get_name(), method_name));
        if (m_iter != method_env.end()) {
            return m_iter->second;
        }
    }

    return nullptr;
}

/*
 * This function initiates the object environment of a class by adding to it
 * all class attributes declared in the class itself along with any inherited
 * attributes.
 */
void build_initial_obj_env(type_env &tenv) {
    // First add attributes of superclasses to the object environment.
    for (auto c_iter = class_map.find(tenv.c->get_parent());
         c_iter != class_map.end();
         c_iter = class_map.find(c_iter->second->get_parent())
         ) {

        Features features = c_iter->second->get_features();
        for (int i = features->first(); features->more(i); i = features->next(i)) {
            Feature f = features->nth(i);

            attr_class *attribute = dynamic_cast<attr_class *>(f);
            if (!attribute) {
                continue; // f is a method not an attribute, so skip it
            }

            tenv.o.addid(attribute->get_name(), new Symbol(attribute->get_type_decl()));
        }
    }

    // Then add attributes declared on that class to the object environment.
    Features features = tenv.c->get_features();
    for (int i = features->first(); features->more(i); i = features->next(i)) {
        Feature f = features->nth(i);

        attr_class *attribute = dynamic_cast<attr_class *>(f);
        if (!attribute) {
            continue; // f is a method not an attribute, so skip it
        }

        if (tenv.o.lookup(attribute->get_name())) {
            classtable->semant_error(tenv.c->get_filename(), attribute) <<
                "Attribute " << attribute->get_name() << " is already defined "
                "either in the same class or in a superclass." << std::endl;
        } else {
            tenv.o.addid(attribute->get_name(), new Symbol(attribute->get_type_decl()));
        }
    }

    // PS. I feel bad for the code repetition in this function, but I needed
    // a way to go top-down instead of bottom-up in order to report any
    // attribute redefinitions where last declared (i.e. reporting them in the
    // subclass instead of reporting them in the superclass).
}

void class__class::check() {
    type_env tenv;
    tenv.c = this;
    tenv.o.enterscope();

    build_initial_obj_env(tenv);

    Features features = tenv.c->get_features();
    for (int i = features->first(); features->more(i); i = features->next(i)) {
        features->nth(i)->typecheck(tenv);
    }
}

/*   This is the entry point to the semantic checker.

     Your checker should do the following two things:

     1) Check that the program is semantically correct
     2) Decorate the abstract syntax tree with type information
        by setting the `type' field in each Expression node.
        (see `tree.h')

     You are free to first do 1), make sure you catch all semantic
     errors. Part 2) can be done in a second stage, when you want
     to build mycoolc.
 */
void program_class::semant()
{
    initialize_constants();

    classtable = new ClassTable(classes);

    // If the class hierarchy is not well-defined it is acceptable to abort.
    if (classtable->errors()) {
        cerr << "Compilation halted due to static semantic errors." << endl;
        exit(1);
    }

    build_method_env();

    for(int i = classes->first(); classes->more(i); i = classes->next(i)) {
        classes->nth(i)->check();
    }

    if (classtable->errors()) {
        cerr << "Compilation halted due to static semantic errors." << endl;
        exit(1);
    }
}
