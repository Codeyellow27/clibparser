//
// Project: clibparser
// Created by bajdcc
//

#ifndef CLIBPARSER_CGEN_H
#define CLIBPARSER_CGEN_H

#include <vector>
#include <memory>
#include "cast.h"
#include "cparser.h"
#include "cvm.h"

/* 用户代码段基址 */
#define USER_BASE 0xc0000000
/* 用户数据段基址 */
#define DATA_BASE 0xd0000000
/* 用户栈基址 */
#define STACK_BASE 0xe0000000
/* 用户堆基址 */
#define HEAP_BASE 0xf0000000
/* 用户堆大小 */
#define HEAP_SIZE 1000
/* 段掩码 */
#define SEGMENT_MASK 0x0fffffff

namespace clib {

    enum symbol_t {
        s_sym,
        s_type,
        s_type_base,
        s_type_typedef,
        s_id,
        s_struct,
        s_function,
        s_var,
        s_var_id,
        s_expression,
        s_unop,
        s_sinop,
        s_binop,
        s_triop,
        s_list,
        s_ctrl,
        s_statement,
    };

    enum gen_t {
        g_ok,
        g_error,
    };

    class igen {
    public:
        virtual void emit(ins_t) = 0;
        virtual void emit(ins_t, int) = 0;
        virtual void emit(ins_t, int, int) = 0;
        virtual void emit(keyword_t) = 0;
        virtual int current() const = 0;
        virtual int edit(int, int) = 0;
        virtual int load_string(const string_t &) = 0;
        virtual void error(const string_t &) = 0;
    };

    enum sym_size_t {
        x_size,
        x_inc,
    };

    class sym_t {
    public:
        using ref = std::shared_ptr<sym_t>;
        using weak_ref = std::weak_ptr<sym_t>;
        virtual symbol_t get_type() const;
        virtual symbol_t get_base_type() const;
        virtual int size(sym_size_t t) const;
        virtual string_t get_name() const;
        virtual string_t to_string() const;
        virtual gen_t gen_lvalue(igen &gen);
        virtual gen_t gen_rvalue(igen &gen);
        virtual gen_t gen_invoke(igen &gen, ref &list);
        int line{0}, column{0};
    };

    class type_t : public sym_t {
    public:
        using ref = std::shared_ptr<type_t>;
        explicit type_t(int ptr = 0);
        symbol_t get_type() const override;
        symbol_t get_base_type() const override;
        virtual ref clone() const;
        int ptr;
    };

    class type_base_t : public type_t {
    public:
        explicit type_base_t(lexer_t type, int ptr = 0);
        symbol_t get_type() const override;
        int size(sym_size_t t) const override;
        string_t get_name() const override;
        string_t to_string() const override;
        type_t::ref clone() const override;
        lexer_t type;
    };

    class type_typedef_t : public type_t {
    public:
        explicit type_typedef_t(const sym_t::ref &refer, int ptr = 0);
        symbol_t get_type() const override;
        int size(sym_size_t t) const override;
        string_t to_string() const override;
        ref clone() const override;
        sym_t::weak_ref refer;
    };

    enum sym_class_t {
        z_undefined,
        z_global_var,
        z_local_var,
        z_param_var,
        z_struct_var,
        z_function,
        z_end,
    };

    const string_t &sym_class_string(sym_class_t);

    class type_exp_t : public sym_t {
    public:
        using ref = std::shared_ptr<type_exp_t>;
        explicit type_exp_t(const type_t::ref &base);
        symbol_t get_type() const override;
        symbol_t get_base_type() const override;
        type_t::ref base;
    };

    class sym_id_t : public sym_t {
    public:
        using ref = std::shared_ptr<sym_id_t>;
        explicit sym_id_t(const type_t::ref &base, const string_t &id);
        symbol_t get_type() const override;
        symbol_t get_base_type() const override;
        int size(sym_size_t t) const override;
        string_t get_name() const override;
        string_t to_string() const override;
        gen_t gen_lvalue(igen &gen) override;
        gen_t gen_rvalue(igen &gen) override;
        type_t::ref base;
        type_exp_t::ref init;
        string_t id;
        sym_class_t clazz{z_undefined};
        int addr{0};
        int addr_end{0};
    };

    class sym_struct_t : public sym_t {
    public:
        using ref = std::shared_ptr<sym_id_t>;
        explicit sym_struct_t(const string_t &id);
        symbol_t get_type() const override;
        symbol_t get_base_type() const override;
        int size(sym_size_t t) const override;
        string_t get_name() const override;
        string_t to_string() const override;
        string_t id;
        int _size{0};
        std::vector<sym_id_t::ref> decls;
    };

    class sym_func_t : public sym_id_t {
    public:
        explicit sym_func_t(const type_t::ref &base, const string_t &id);
        symbol_t get_type() const override;
        symbol_t get_base_type() const override;
        int size(sym_size_t t) const override;
        string_t to_string() const override;
        gen_t gen_invoke(igen &gen, sym_t::ref &list) override;
        std::vector<sym_id_t::ref> params;
        int ebp{0}, ebp_local{0};
        int entry{0};
    };

    class sym_var_t : public type_exp_t {
    public:
        using ref = std::shared_ptr<sym_var_t>;
        explicit sym_var_t(const type_t::ref &base, ast_node *node);
        symbol_t get_type() const override;
        int size(sym_size_t t) const override;
        string_t get_name() const override;
        string_t to_string() const override;
        gen_t gen_lvalue(igen &gen) override;
        gen_t gen_rvalue(igen &gen) override;
        ast_node *node{nullptr};
    };

    class sym_var_id_t : public sym_var_t {
    public:
        using ref = std::shared_ptr<sym_var_id_t>;
        explicit sym_var_id_t(const type_t::ref &base, ast_node *node, const sym_t::ref &symbol);
        symbol_t get_type() const override;
        int size(sym_size_t t) const override;
        string_t get_name() const override;
        string_t to_string() const override;
        gen_t gen_lvalue(igen &gen) override;
        gen_t gen_rvalue(igen &gen) override;
        gen_t gen_invoke(igen &gen, sym_t::ref &list) override;
        sym_t::weak_ref id;
    };

    class sym_unop_t : public type_exp_t {
    public:
        using ref = std::shared_ptr<sym_unop_t>;
        explicit sym_unop_t(const type_exp_t::ref &exp, ast_node *op);
        symbol_t get_type() const override;
        int size(sym_size_t t) const override;
        string_t get_name() const override;
        string_t to_string() const override;
        gen_t gen_lvalue(igen &gen) override;
        gen_t gen_rvalue(igen &gen) override;
        type_exp_t::ref exp;
        ast_node *op{nullptr};
    };

    class sym_sinop_t : public type_exp_t {
    public:
        using ref = std::shared_ptr<sym_sinop_t>;
        explicit sym_sinop_t(const type_exp_t::ref &exp, ast_node *op);
        symbol_t get_type() const override;
        int size(sym_size_t t) const override;
        string_t get_name() const override;
        string_t to_string() const override;
        gen_t gen_lvalue(igen &gen) override;
        gen_t gen_rvalue(igen &gen) override;
        type_exp_t::ref exp;
        ast_node *op{nullptr};
    };

    class sym_binop_t : public type_exp_t {
    public:
        using ref = std::shared_ptr<sym_binop_t>;
        explicit sym_binop_t(const type_exp_t::ref &exp1, const type_exp_t::ref &exp2, ast_node *op);
        symbol_t get_type() const override;
        int size(sym_size_t t) const override;
        string_t get_name() const override;
        string_t to_string() const override;
        gen_t gen_lvalue(igen &gen) override;
        gen_t gen_rvalue(igen &gen) override;
        type_exp_t::ref exp1, exp2;
        ast_node *op{nullptr};
    };

    class sym_triop_t : public type_exp_t {
    public:
        using ref = std::shared_ptr<sym_triop_t>;
        explicit sym_triop_t(const type_exp_t::ref &exp1, const type_exp_t::ref &exp2,
                             const type_exp_t::ref &exp3, ast_node *op1, ast_node *op2);
        symbol_t get_type() const override;
        int size(sym_size_t t) const override;
        string_t get_name() const override;
        string_t to_string() const override;
        gen_t gen_lvalue(igen &gen) override;
        gen_t gen_rvalue(igen &gen) override;
        type_exp_t::ref exp1, exp2, exp3;
        ast_node *op1{nullptr}, *op2{nullptr};
    };

    class sym_list_t : public type_exp_t {
    public:
        using ref = std::shared_ptr<sym_list_t>;
        sym_list_t();
        symbol_t get_type() const override;
        int size(sym_size_t t) const override;
        string_t get_name() const override;
        string_t to_string() const override;
        gen_t gen_lvalue(igen &gen) override;
        gen_t gen_rvalue(igen &gen) override;
        std::vector<type_exp_t::ref> exps;
    };

    class sym_ctrl_t : public sym_t {
    public:
        using ref = std::shared_ptr<sym_ctrl_t>;
        explicit sym_ctrl_t(ast_node *op);
        symbol_t get_type() const override;
        int size(sym_size_t t) const override;
        string_t get_name() const override;
        string_t to_string() const override;
        gen_t gen_lvalue(igen &gen) override;
        gen_t gen_rvalue(igen &gen) override;
        type_exp_t::ref exp;
        ast_node *op{nullptr};
    };

    struct cycle_t {
        uint _break;
        uint _continue;
    };

    // 生成虚拟机指令
    class cgen : public csemantic, public igen {
    public:
        cgen();
        ~cgen() = default;

        cgen(const cgen &) = delete;
        cgen &operator=(const cgen &) = delete;

        backtrace_direction check(pda_edge_t, ast_node *) override;

        void gen(ast_node *node);
        void reset();
        bool eval(int cycle, int &cycles);

        void emit(ins_t) override;
        void emit(ins_t, int) override;
        void emit(ins_t, int, int) override;
        void emit(keyword_t) override;
        int current() const override;
        int edit(int, int) override;
        int load_string(const string_t &) override;
        void error(const string_t &) override;
    private:
        void gen_rec(ast_node *node, int level);
        void gen_coll(const std::vector<ast_node *> &nodes, int level, ast_node *node);
        void gen_stmt(const std::vector<ast_node *> &nodes, int level, ast_node *node);

        void allocate(sym_id_t::ref id, const type_exp_t::ref &init);
        sym_id_t::ref add_id(const type_base_t::ref &, sym_class_t, ast_node *, const type_exp_t::ref &);

        sym_t::ref find_symbol(const string_t &name);
        sym_var_t::ref primary_node(ast_node *node);

        void error(ast_node *, const string_t &, bool info = false);
        void error(sym_t::ref s, const string_t &);

        static type_exp_t::ref to_exp(sym_t::ref s);

    private:
        std::vector<LEX_T(int)> text; // 代码
        std::vector<LEX_T(char)> data; // 数据
        std::vector<std::unordered_map<LEX_T(string), std::shared_ptr<sym_t>>> symbols; // 符号表
        std::vector<std::vector<ast_node *>> ast;
        std::vector<std::vector<sym_t::ref>> tmp;
        std::vector<cycle_t> cycle;
        std::unique_ptr<cvm> vm;
        sym_t::weak_ref ctx;
    };
}

#endif //CLIBPARSER_CGEN_H
