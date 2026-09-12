#include "Matrix.hpp"
#include "lua/plus.hpp"

using std::string_view_literals::operator""sv;

namespace luastg::binding
{
    std::string_view Matrix::class_name{"lstg.Matrix"};

    struct MatrixBinding : Matrix
    {
        static int __tostring(lua_State *vm)
        {
            lua::stack_t const ctx(vm);
            std::ignore = as(vm, 1);
            ctx.push_value(class_name);
            return 1;
        }

        static int __index(lua_State *vm)
        {
            lua::stack_t const ctx(vm);
            auto const self = as(vm, 1);
            auto const key = ctx.get_value<std::string_view>(2);

            if (key == "rows"sv)
                ctx.push_value(static_cast<lua_Integer>(self->data.rows));
            else if (key == "columns"sv)
                ctx.push_value(static_cast<lua_Integer>(self->data.columns));
            else
            {
                auto const method_table = ctx.push_module(class_name);
                ctx.push_map_value(method_table, key);
                if (ctx.is_nil(ctx.index_of_top()))
                    return luaL_error(vm, "field '%s' not exists", key.data());
            }
            return 1;
        }

        static int __newindex(lua_State *vm)
        {
            lua::stack_t const ctx(vm);
            auto const key = ctx.get_value<std::string_view>(2);
            if (key == "rows"sv || key == "columns"sv)
                return luaL_error(vm, "matrix dimensions are read-only");
            return luaL_error(vm, "field '%s' not exists", key.data());
        }

        static int __eq(lua_State *vm)
        {
            lua::stack_t const ctx(vm);
            auto const self = as(vm, 1);
            ctx.push_value(is(vm, 2) && self->data == as(vm, 2)->data);
            return 1;
        }

        static int __add(lua_State *vm)
        {
            if (!is(vm, 1) || !is(vm, 2))
                return luaL_error(vm, "matrix addition requires two Matrix values");
            auto const left = as(vm, 1);
            auto const right = as(vm, 2);
            if (left->data.rows != right->data.rows || left->data.columns != right->data.columns)
            {
                return luaL_error(vm, "matrix addition requires equal dimensions");
            }
            auto const copy = Matrix::create(vm);
            copy->data = left->data + right->data;
            return 1;
        }

        static int __sub(lua_State *vm)
        {
            if (!is(vm, 1) || !is(vm, 2))
                return luaL_error(vm, "matrix subtraction requires two Matrix values");
            auto const left = as(vm, 1);
            auto const right = as(vm, 2);
            if (left->data.rows != right->data.rows || left->data.columns != right->data.columns)
            {
                return luaL_error(vm, "matrix subtraction requires equal dimensions");
            }
            auto const copy = Matrix::create(vm);
            copy->data = left->data - right->data;
            return 1;
        }

        static int __mul(lua_State *vm)
        {
            lua::stack_t const ctx(vm);
            if (is(vm, 1) && is(vm, 2))
            {
                auto const left = as(vm, 1);
                auto const right = as(vm, 2);
                if (left->data.columns != right->data.rows)
                    return luaL_error(vm, "matrix multiplication dimensions do not match");
                auto const copy = Matrix::create(vm);
                copy->data = left->data * right->data;
                return 1;
            }
            if (is(vm, 1) && ctx.is_number(2))
            {
                auto const copy = Matrix::create(vm);
                copy->data = as(vm, 1)->data * ctx.get_value<lua_Number>(2);
                return 1;
            }
            if (ctx.is_number(1) && is(vm, 2))
            {
                auto const copy = Matrix::create(vm);
                copy->data = as(vm, 2)->data * ctx.get_value<lua_Number>(1);
                return 1;
            }
            return luaL_error(vm, "matrix multiplication requires Matrix values or a number");
        }

        static int create(lua_State *vm)
        {
            lua::stack_t const ctx(vm);
            if (ctx.index_of_top() < 2)
                return Matrix::create(vm) != nullptr ? 1 : 0;

            auto const rows = ctx.get_value<lua_Integer>(1);
            auto const columns = ctx.get_value<lua_Integer>(2);

            if (rows < 0 || columns < 0)
                return luaL_error(vm, "matrix dimensions must be non-negative");

            auto const self = Matrix::create(vm);
            self->data = core::Matrix<lua_Number>(static_cast<size_t>(rows), static_cast<size_t>(columns));

            for (size_t i = 0; i < self->data.values.size(); ++i)
            {
                if (ctx.index_of_top() < static_cast<int>(i + 3))
                    break;
                self->data.values[i] = ctx.get_value<lua_Number>(static_cast<int>(i + 3));
            }
            return 1;
        }

        static int identity(lua_State *vm)
        {
            lua::stack_t const ctx(vm);
            if (!ctx.is_number(1))
                return luaL_error(vm, "Matrix.identity requires a size");

            auto const size = ctx.get_value<lua_Integer>(1);
            if (size < 0)
                return luaL_error(vm, "matrix dimensions must be non-negative");

            auto const self = Matrix::create(vm);
            self->data = core::Matrix<lua_Number>::identity(static_cast<size_t>(size));
            return 1;
        }

        static int get(lua_State *vm)
        {
            lua::stack_t const ctx(vm);
            auto const self = as(vm, 1);
            auto const row = ctx.get_value<lua_Integer>(2);
            auto const column = ctx.get_value<lua_Integer>(3);
            if (row < 1 || column < 1 || static_cast<size_t>(row) > self->data.rows || static_cast<size_t>(column) > self->data.columns)
            {
                return luaL_error(vm, "matrix index out of range");
            }

            ctx.push_value(self->data.at(static_cast<size_t>(row - 1), static_cast<size_t>(column - 1)));
            return 1;
        }

        static int set(lua_State *vm)
        {
            lua::stack_t const ctx(vm);
            auto const self = as(vm, 1);
            auto const row = ctx.get_value<lua_Integer>(2);
            auto const column = ctx.get_value<lua_Integer>(3);
            if (row < 1 || column < 1 || static_cast<size_t>(row) > self->data.rows || static_cast<size_t>(column) > self->data.columns)
            {
                return luaL_error(vm, "matrix index out of range");
            }

            self->data.at(static_cast<size_t>(row - 1), static_cast<size_t>(column - 1)) = ctx.get_value<lua_Number>(4);
            ctx.push_value(lua::stack_index_t(1));
            return 1;
        }

        static int transpose(lua_State *vm)
        {
            auto const copy = Matrix::create(vm);
            copy->data = as(vm, 1)->data.transposed();
            return 1;
        }

        static int inverse(lua_State *vm)
        {
            auto const copy = Matrix::create(vm);
            if (!as(vm, 1)->data.tryInverse(copy->data))
                return luaL_error(vm, "cannot invert a non-square or singular Matrix");
            return 1;
        }
    };

    bool Matrix::is(lua_State *vm, int const index)
    {
        lua::stack_t const ctx(vm);
        return ctx.is_metatable(index, class_name);
    }
    Matrix *Matrix::as(lua_State *vm, int const index)
    {
        lua::stack_t const ctx(vm);
        return ctx.as_userdata<Matrix>(index);
    }
    Matrix *Matrix::create(lua_State *vm)
    {
        lua::stack_t const ctx(vm);
        auto const self = ctx.create_userdata<Matrix>();
        ctx.set_metatable(ctx.index_of_top(), class_name);
        self->data = {};
        return self;
    }
    void Matrix::registerClass(lua_State *vm)
    {
        [[maybe_unused]] lua::stack_balancer_t stack_balancer(vm);
        lua::stack_t const ctx(vm);

        auto const method_table = ctx.create_module(class_name);
        ctx.set_map_value(method_table, "create", &MatrixBinding::create);
        ctx.set_map_value(method_table, "identity", &MatrixBinding::identity);
        ctx.set_map_value(method_table, "get", &MatrixBinding::get);
        ctx.set_map_value(method_table, "set", &MatrixBinding::set);
        ctx.set_map_value(method_table, "transpose", &MatrixBinding::transpose);
        ctx.set_map_value(method_table, "inverse", &MatrixBinding::inverse);

        auto const metatable = ctx.create_metatable(class_name);
        ctx.set_map_value(metatable, "__tostring", &MatrixBinding::__tostring);
        ctx.set_map_value(metatable, "__index", &MatrixBinding::__index);
        ctx.set_map_value(metatable, "__newindex", &MatrixBinding::__newindex);
        ctx.set_map_value(metatable, "__eq", &MatrixBinding::__eq);
        ctx.set_map_value(metatable, "__add", &MatrixBinding::__add);
        ctx.set_map_value(metatable, "__sub", &MatrixBinding::__sub);
        ctx.set_map_value(metatable, "__mul", &MatrixBinding::__mul);
    }
}
