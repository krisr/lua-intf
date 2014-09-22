//
// https://github.com/SteveKChiu/lua-intf
//
// Copyright 2014, Steve K. Chiu <steve.k.chiu@gmail.com>
//
// The MIT License (http://www.opensource.org/licenses/mit-license.php)
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//

template <typename SP, typename T, typename ARGS>
struct CppBindClassConstructor;

template <typename T, typename... P>
struct CppBindClassConstructor <T, T, _arg(*)(P...)>
{
    /**
     * lua_CFunction to call a class constructor (constructed inside userdata)
     */
    static int call(lua_State* L)
    {
        try {
            typename CppArgTuple<P...>::Type args;
            CppArgInput<P...>::get(L, 2, args);
            CppObjectValue<T>::pushToStack(L, args, false);
            return 1;
        } catch (std::exception& e) {
            return luaL_error(L, e.what());
        }
    }
};

template <typename SP, typename T, typename... P>
struct CppBindClassConstructor <SP, T, _arg(*)(P...)>
{
    /**
     * lua_CFunction to call a class constructor (stored via smart pointer)
     */
    static int call(lua_State* L)
    {
        try {
            typename CppArgTuple<P...>::Type args;
            CppArgInput<P...>::get(L, 2, args);
            CppObjectSharedPtr<SP, T>::pushToStack(L, args, false);
            return 1;
        } catch (std::exception& e) {
            return luaL_error(L, e.what());
        }
    }
};

//----------------------------------------------------------------------------

template <typename T, bool IS_CONST>
struct CppBindClassDestructor
{
    /**
     * lua_CFunction to call CppObject destructor (for __gc)
     */
    static int call(lua_State* L)
    {
        try {
            CppObject* obj = CppObject::getExactObject<T>(L, 1, IS_CONST);
            obj->~CppObject();
            return 0;
        } catch (std::exception& e) {
            return luaL_error(L, e.what());
        }
    }
};

//----------------------------------------------------------------------------

template <int CHK, typename T, bool IS_PROXY, bool IS_CONST, typename FN, typename R, typename... P>
struct CppBindClassMethodBase
{
    static_assert(CHK != 1 || (!std::is_same<R, void>::value && sizeof...(P) == 0),
        "the specified function is not getter function");

    static_assert(CHK != -1 || (std::is_same<R, void>::value && sizeof...(P) == 1),
        "the specified function is not setter function");

    static constexpr bool IsConst = IS_CONST;

    /**
     * lua_CFunction to call a class member function
     *
     * The pointer to function object is in the first upvalue.
     * The class userdata object is at the top of the Lua stack.
     */
    static int call(lua_State* L)
    {
        try {
            assert(lua_isuserdata(L, lua_upvalueindex(1)));

            const FN& fn = *reinterpret_cast<const FN*>(lua_touserdata(L, lua_upvalueindex(1)));
            assert(fn);

            typename CppArgTuple<P...>::Type args;
            T* obj = CppObject::get<T>(L, 1, IS_CONST);
            CppArgInput<P...>::get(L, 2, args);

            int n = CppInvokeClassMethod<T, IS_PROXY, FN, R, typename CppArg<P>::PasserType...>::push(L, obj, fn, args);
            return n + CppArgOutput<P...>::push(L, args);
        } catch (std::exception& e) {
            return luaL_error(L, e.what());
        }
    }

    template <typename PROC>
    static FN function(const PROC& fn)
    {
        return static_cast<FN>(fn);
    }
};

template <typename T, typename FN, typename ARGS = FN, int CHK = 0>
struct CppBindClassMethod;

template <typename T, typename TF, typename R, typename... P, int CHK>
struct CppBindClassMethod <T, R(TF::*)(P...), R(TF::*)(P...), CHK>
    : CppBindClassMethodBase <CHK, T, false, false, R(T::*)(P...), R, P...>
{
    static_assert(std::is_same<T, TF>::value, "class type and member function does not match");
};

template <typename T, typename TF, typename R, typename... P, int CHK>
struct CppBindClassMethod <T, R(TF::*)(P...) const, R(TF::*)(P...) const, CHK>
    : CppBindClassMethodBase <CHK, T, false, true, R(T::*)(P...) const, R, P...>
{
    static_assert(std::is_same<T, TF>::value, "class type and member function does not match");
};

template <typename T, typename TF, typename R, typename... P, int CHK>
struct CppBindClassMethod <T, R(*)(TF*, P...), R(*)(TF*, P...), CHK>
    : CppBindClassMethodBase <CHK, T, true, false, R(*)(T*, P...), R, P...>
{
    static_assert(std::is_same<T, TF>::value, "class type and function argument type does not match");
};

template <typename T, typename TF, typename R, typename... P, int CHK>
struct CppBindClassMethod <T, R(*)(const TF*, P...), R(*)(const TF*, P...), CHK>
    : CppBindClassMethodBase <CHK, T, true, true, R(*)(const T*, P...), R, P...>
{
    static_assert(std::is_same<T, TF>::value, "class type and function argument type does not match");
};

template <typename T, typename TF, typename R, typename... P, int CHK>
struct CppBindClassMethod <T, std::function<R(TF*, P...)>, std::function<R(TF*, P...)>, CHK>
    : CppBindClassMethodBase <CHK, T, true, false, std::function<R(T*, P...)>, R, P...>
{
    static_assert(std::is_same<T, TF>::value, "class type and function argument type does not match");
};

template <typename T, typename TF, typename R, typename... P, int CHK>
struct CppBindClassMethod <T, std::function<R(const TF*, P...)>, std::function<R(const TF*, P...)>, CHK>
    : CppBindClassMethodBase <CHK, T, true, true, std::function<R(T*, P...)>, R, P...>
{
    static_assert(std::is_same<T, TF>::value, "class type and function argument type does not match");
};

template <typename T, typename TF, typename R, typename... A, typename... P, int CHK>
struct CppBindClassMethod <T, R(TF::*)(A...), _arg(*)(P...), CHK>
    : CppBindClassMethodBase <CHK, T, false, false, R(T::*)(A...), R, P...>
{
    static_assert(std::is_same<T, TF>::value, "class type and member function does not match");
    static_assert(sizeof...(A) == sizeof...(P), "the number of arguments and argument-specs do not match");
};

template <typename T, typename TF, typename R, typename... A, typename... P, int CHK>
struct CppBindClassMethod <T, R(TF::*)(A...) const, _arg(*)(P...), CHK>
    : CppBindClassMethodBase <CHK, T, false, true, R(T::*)(A...) const, R, P...>
{
    static_assert(std::is_same<T, TF>::value, "class type and member function does not match");
    static_assert(sizeof...(A) == sizeof...(P), "the number of arguments and argument-specs do not match");
};

template <typename T, typename TF, typename R, typename... A, typename... P, int CHK>
struct CppBindClassMethod <T, R(*)(TF*, A...), _arg(*)(P...), CHK>
    : CppBindClassMethodBase <CHK, T, true, false, R(*)(T*, A...), R, P...>
{
    static_assert(std::is_same<T, TF>::value, "class type and function argument type does not match");
    static_assert(sizeof...(A) == sizeof...(P), "the number of arguments and argument-specs do not match");
};

template <typename T, typename TF, typename R, typename... A, typename... P, int CHK>
struct CppBindClassMethod <T, R(*)(const TF*, A...), _arg(*)(P...), CHK>
    : CppBindClassMethodBase <CHK, T, true, true, R(*)(const T*, A...), R, P...>
{
    static_assert(std::is_same<T, TF>::value, "class type and function argument type does not match");
    static_assert(sizeof...(A) == sizeof...(P), "the number of arguments and argument-specs do not match");
};

template <typename T, typename TF, typename R, typename... A, typename... P, int CHK>
struct CppBindClassMethod <T, std::function<R(TF*, A...)>, _arg(*)(P...), CHK>
    : CppBindClassMethodBase <CHK, T, true, false, std::function<R(T*, A...)>, R, P...>
{
    static_assert(std::is_same<T, TF>::value, "class type and function argument type does not match");
    static_assert(sizeof...(A) == sizeof...(P), "the number of arguments and argument-specs do not match");
};

template <typename T, typename TF, typename R, typename... A, typename... P, int CHK>
struct CppBindClassMethod <T, std::function<R(const TF*, A...)>, _arg(*)(P...), CHK>
    : CppBindClassMethodBase <CHK, T, true, true, std::function<R(const T*, A...)>, R, P...>
{
    static_assert(std::is_same<T, TF>::value, "class type and function argument type does not match");
    static_assert(sizeof...(A) == sizeof...(P), "the number of arguments and argument-specs do not match");
};

template <typename T, typename FN, int CHK>
struct CppBindClassMethod <T, FN, FN, CHK>
    : CppBindClassMethod <T, typename CppLambdaTraits<FN>::FunctionType, typename CppLambdaTraits<FN>::FunctionType, CHK> {};

template <typename T, typename FN, typename... P, int CHK>
struct CppBindClassMethod <T, FN, _arg(*)(P...), CHK>
    : CppBindClassMethod <T, typename CppLambdaTraits<FN>::FunctionType, _arg(*)(P...), CHK> {};

//--------------------------------------------------------------------------

template <typename T, typename V>
struct CppBindClassVariable
{
    /**
     * lua_CFunction to get a class data member.
     *
     * The pointer-to-member is in the first upvalue.
     * The class userdata object is at the top of the Lua stack.
     */
    static int get(lua_State* L)
    {
        try {
            const T* obj = CppObject::get<T>(L, 1, true);
            auto mp = static_cast<V T::**>(lua_touserdata(L, lua_upvalueindex(1)));
            LuaType<V&>::push(L, obj->**mp);
            return 1;
        } catch (std::exception& e) {
            return luaL_error(L, e.what());
        }
    }

    /**
     * lua_CFunction to set a class data member.
     *
     * The pointer-to-member is in the first upvalue.
     * The class userdata object is at the top of the Lua stack.
     */
    static int set(lua_State* L)
    {
        try {
            T* obj = CppObject::get<T>(L, 1, false);
            auto mp = static_cast<V T::**>(lua_touserdata(L, lua_upvalueindex(1)));
            obj->**mp = LuaType<V>::get(L, 2);
            return 0;
        } catch (std::exception& e) {
            return luaL_error(L, e.what());
        }
    }
};

//--------------------------------------------------------------------------

struct CppBindClassMetaMethod
{
    /**
     * __index metamethod for class.
     */
    static int index(lua_State* L);

    /**
     * __newindex metamethod for class.
     */
    static int newIndex(lua_State* L);

    /**
     * lua_CFunction to report an error writing to a read-only value.
     *
     * The name of the variable is in the first upvalue.
     */
    static int errorReadOnly(lua_State* L);

    /**
     * lua_CFunction to report an error attempt to call non-const member function with const object.
     *
     * The name of the function is in the first upvalue.
     */
    static int errorConstMismatch(lua_State* L);

    /**
     * key for metatable signature.
     */
    static void* signature()
    {
        static bool v;
        return &v;
    }
};

//--------------------------------------------------------------------------

class CppBindModule;

class CppBindClassBase
{
protected:
    explicit CppBindClassBase(const LuaRef& meta)
        : m_meta(meta)
    {
        m_meta.checkTable();
    }

    explicit CppBindClassBase(LuaRef&& meta)
        : m_meta(std::move(meta))
    {
        m_meta.checkTable();
    }

    static bool buildMetaTable(LuaRef& meta, LuaRef& parent, const char* name, void* static_id, void* class_id, void* const_id);
    static bool buildMetaTable(LuaRef& meta, LuaRef& parent, const char* name, void* static_id, void* class_id, void* const_id, void* super_static_id);

    void setStaticGetter(const char* name, const LuaRef& getter);
    void setStaticSetter(const char* name, const LuaRef& setter);
    void setStaticReadOnly(const char* name);

    void setMemberGetter(const char* name, const LuaRef& getter);
    void setMemberSetter(const char* name, const LuaRef& setter);
    void setMemberReadOnly(const char* name);
    void setMemberFunction(const char* name, const LuaRef& proc, bool is_const);

public:
    /**
     * The underlying lua state.
     */
    lua_State* state() const
    {
        return m_meta.state();
    }

    /**
     * The underlying meta table.
     */
    LuaRef meta() const
    {
        return m_meta;
    }

protected:
    LuaRef m_meta;
};

//--------------------------------------------------------------------------

/**
 * Provides a class registration in a lua_State.
 */
template <typename T, typename PARENT>
class CppBindClass : public CppBindClassBase
{
    friend class CppBindModule;
    template <typename T2, typename P2> friend class CppBindClass;

private:
    explicit CppBindClass(const LuaRef& meta)
        : CppBindClassBase(meta)
        {}

    /**
     * Register a new class or add to an existing class registration.
     *
     * @param parent_meta the parent module meta table (or class meta table if this is inner class)
     * @param name the name of class
     * @return new or existing class
     */
    static CppBindClass<T, PARENT> bind(LuaRef& parent_meta, const char* name)
    {
        LuaRef meta;
        if (buildMetaTable(meta, parent_meta, name,
            CppObjectType<T>::staticID(), CppObjectType<T>::classID(), CppObjectType<T>::constID()))
        {
            meta.rawget<LuaRef>("___class").rawset("__gc", &CppBindClassDestructor<T, false>::call);
            meta.rawget<LuaRef>("___const").rawset("__gc", &CppBindClassDestructor<T, true>::call);
        }
        return CppBindClass<T, PARENT>(meta);
    }

    /**
     * Derive a new class or add to an existing class registration.
     *
     * @param parent_meta the parent module meta table (or class meta table if this is inner class)
     * @param name the name of class
     * @param super_type_id the super class id
     * @return new or existing class
     */
    static CppBindClass<T, PARENT> extend(LuaRef& parent_meta, const char* name, void* super_type_id)
    {
        LuaRef meta;
        if (buildMetaTable(meta, parent_meta, name,
            CppObjectType<T>::staticID(), CppObjectType<T>::classID(), CppObjectType<T>::constID(), super_type_id))
        {
            meta.rawget<LuaRef>("___class").rawset("__gc", &CppBindClassDestructor<T, false>::call);
            meta.rawget<LuaRef>("___const").rawset("__gc", &CppBindClassDestructor<T, true>::call);
        }
        return CppBindClass<T, PARENT>(meta);
    }

public:
    /**
     * Copy constructor.
     */
    CppBindClass(const CppBindClass& that)
        : CppBindClassBase(that.m_meta)
        {}

    /**
     * Move constructor for temporaries.
     */
    CppBindClass(CppBindClass&& that)
        : CppBindClassBase(std::move(that.m_meta))
        {}

    /**
     * Copy assignment.
     */
    CppBindClass<T, PARENT>& operator = (const CppBindClass<T, PARENT>& that)
    {
        m_meta = that.m_meta;
        return *this;
    }

    /**
     * Move assignment for temporaries.
     */
    CppBindClass<T, PARENT>& operator = (CppBindClass<T, PARENT>&& that)
    {
        m_meta = std::move(that.m_meta);
        return *this;
    }

    /**
     * Add or replace a static data member.
     */
    template <typename V>
    CppBindClass<T, PARENT>& addStaticVariable(const char* name, V* v, bool writable = true)
    {
        setStaticGetter(name, LuaRef::createFunctionWithPtr(state(), &CppBindVariable<V>::get, v));
        if (writable) {
            setStaticSetter(name, LuaRef::createFunctionWithPtr(state(), &CppBindVariable<V>::set, v));
        } else {
            setStaticReadOnly(name);
        }
        return *this;
    }

    /**
     * Add or replace a read-write property.
     */
    template <typename FG, typename FS>
    CppBindClass<T, PARENT>& addStaticProperty(const char* name, const FG& get, const FS& set)
    {
        static_assert(!std::is_function<FG>::value, "function pointer is needed, please prepend & to function name");
        static_assert(!std::is_function<FS>::value, "function pointer is needed, please prepend & to function name");
        using CppGetter = CppBindMethod<FG, FG, 1, 1>;
        using CppSetter = CppBindMethod<FS, FS, 1, -1>;
        setStaticGetter(name, LuaRef::createFunction(state(), &CppGetter::call, CppGetter::function(get)));
        setStaticSetter(name, LuaRef::createFunction(state(), &CppSetter::call, CppSetter::function(set)));
        return *this;
    }

    /**
     * Add or replace a read-only property.
     */
    template <typename FN>
    CppBindClass<T, PARENT>& addStaticProperty(const char* name, const FN& get)
    {
        static_assert(!std::is_function<FN>::value, "function pointer is needed, please prepend & to function name");
        using CppGetter = CppBindMethod<FN, FN, 1, 1>;
        setStaticGetter(name, LuaRef::createFunction(state(), &CppGetter::call, CppGetter::function(get)));
        setStaticReadOnly(name);
        return *this;
    }

    /**
     * Add or replace a static member function.
     */
    template <typename FN>
    CppBindClass<T, PARENT>& addStaticFunction(const char* name, const FN& proc)
    {
        static_assert(!std::is_function<FN>::value, "function pointer is needed, please prepend & to function name");
        using CppProc = CppBindMethod<FN>;
        m_meta.rawset(name, LuaRef::createFunction(state(), &CppProc::call, CppProc::function(proc)));
        return *this;
    }

    /**
     * Add or replace a static member function, user can specify augument spec.
     */
    template <typename FN, typename ARGS>
    CppBindClass<T, PARENT>& addStaticFunction(const char* name, const FN& proc, ARGS)
    {
        static_assert(!std::is_function<FN>::value, "function pointer is needed, please prepend & to function name");
        using CppProc = CppBindMethod<FN, ARGS>;
        m_meta.rawset(name, LuaRef::createFunction(state(), &CppProc::call, CppProc::function(proc)));
        return *this;
    }

    /**
     * Add or replace a constructor function. Argument spec is needed to match the constructor:
     *
     * addConstructor(LUA_ARGS(int, int))
     *
     * The constructor is invoked when calling the class type table
     * like a function. You can have only one constructor or factory function for a lua class.
     *
     * The template parameter should matches the desired Constructor
     */
    template <typename ARGS>
    CppBindClass<T, PARENT>& addConstructor(ARGS)
    {
        m_meta.rawset("__call", &CppBindClassConstructor<T, T, ARGS>::call);
        return *this;
    }

    /**
     * Add or replace a constructor function, the object is stored via the given SP container.
     * The SP class is ususally a shared pointer class. Argument spec is needed to match the constructor:
     *
     * addConstructor(LUA_SP(std::shared_ptr<OBJ>), LUA_ARGS(int, int))
     *
     * The constructor is invoked when calling the class type table
     * like a function. You can have only one constructor or factory function for a lua class.
     */
    template <typename SP, typename ARGS>
    CppBindClass<T, PARENT>& addConstructor(SP*, ARGS)
    {
        m_meta.rawset("__call", &CppBindClassConstructor<SP, T, ARGS>::call);
        return *this;
    }

    /**
     * Add or replace a factory function, that is a normal/static function that
     * return the object, pointer or smart pointer of the type:
     *
     * static std::shared_ptr<OBJ> create_obj(ARG1_TYPE arg1, ARG2_TYPE arg2);
     *
     * addFactory(&create_obj)
     *
     * The factory function is invoked when calling the class type table
     * like a function. You can have only one constructor or factory function for a lua class.
     */
    template <typename FN>
    CppBindClass<T, PARENT>& addFactory(const FN& proc)
    {
        static_assert(!std::is_function<FN>::value, "function pointer is needed, please prepend & to function name");
        using CppProc = CppBindMethod<FN, FN, 2>;
        m_meta.rawset("__call", LuaRef::createFunction(state(), &CppProc::call, CppProc::function(proc)));
        return *this;
    }

    /**
     * Add or replace a factory function, that is a normal/static function that
     * return the object, pointer or smart pointer of the type:
     *
     * static std::shared_ptr<OBJ> create_obj(ARG1_TYPE arg1, ARG2_TYPE arg2);
     *
     * addFactory(&create_obj, LUA_ARGS(ARG1_TYPE, _opt<ARG2_TYPE>))
     *
     * The factory function is invoked when calling the class type table
     * like a function. You can have only one constructor or factory function for a lua class.
     */
    template <typename FN, typename ARGS>
    CppBindClass<T, PARENT>& addFactory(const FN& proc, ARGS)
    {
        static_assert(!std::is_function<FN>::value, "function pointer is needed, please prepend & to function name");
        using CppProc = CppBindMethod<FN, ARGS, 2>;
        m_meta.rawset("__call", LuaRef::createFunction(state(), &CppProc::call, CppProc::function(proc)));
        return *this;
    }

    /**
     * Add or replace a non-const data member with optional setter, but only if
     * the type of the data member is copy assignable.
     */
    template <typename V>
    typename std::enable_if<std::is_copy_assignable<V>::value, CppBindClass<T, PARENT>&>::type addVariable(const char* name, V T::* v, bool writable = true)
    {
        setMemberGetter(name, LuaRef::createFunction(state(), &CppBindClassVariable<T, V>::get, v));
        if (writable) {
            setMemberSetter(name, LuaRef::createFunction(state(), &CppBindClassVariable<T, V>::set, v));
        } else {
            setMemberReadOnly(name);
        }
        return *this;
    }

    /**
     * Add or replace a const read-only data member.
     */
    template <typename V>
    CppBindClass<T, PARENT>& addVariable(const char* name, const V T::* v)
    {
        setMemberGetter(name, LuaRef::createFunction(state(), &CppBindClassVariable<T, V>::get, v));
        setMemberReadOnly(name);
        return *this;
    }

    /**
     * Add or replace a property member.
     */
    template <typename FG, typename FS>
    CppBindClass<T, PARENT>& addProperty(const char* name, const FG& get, const FS& set)
    {
        static_assert(!std::is_function<FG>::value, "function pointer is needed, please prepend & to function name");
        static_assert(!std::is_function<FS>::value, "function pointer is needed, please prepend & to function name");
        using CppGetter = CppBindClassMethod<T, FG, FG, 1>;
        using CppSetter = CppBindClassMethod<T, FS, FS, -1>;
        setMemberGetter(name, LuaRef::createFunction(state(), &CppGetter::call, CppGetter::function(get)));
        setMemberSetter(name, LuaRef::createFunction(state(), &CppSetter::call, CppSetter::function(set)));
        return *this;
    }

    /**
     * Add or replace a read-only property member.
     */
    template <typename FN>
    CppBindClass<T, PARENT>& addProperty(const char* name, const FN& get)
    {
        static_assert(!std::is_function<FN>::value, "function pointer is needed, please prepend & to function name");
        using CppGetter = CppBindClassMethod<T, FN, FN, 1>;
        setMemberGetter(name, LuaRef::createFunction(state(), &CppGetter::call, CppGetter::function(get)));
        setMemberReadOnly(name);
        return *this;
    }

    /**
     * Add or replace a member function.
     */
    template <typename FN>
    CppBindClass<T, PARENT>& addFunction(const char* name, const FN& proc)
    {
        static_assert(!std::is_function<FN>::value, "function pointer is needed, please prepend & to function name");
        using CppProc = CppBindClassMethod<T, FN>;
        setMemberFunction(name, LuaRef::createFunction(state(), &CppProc::call, CppProc::function(proc)), CppProc::IsConst);
        return *this;
    }

    /**
     * Add or replace a member function, user can specify augument spec.
     */
    template <typename FN, typename ARGS>
    CppBindClass<T, PARENT>& addFunction(const char* name, const FN& proc, ARGS)
    {
        static_assert(!std::is_function<FN>::value, "function pointer is needed, please prepend & to function name");
        using CppProc = CppBindClassMethod<T, FN, ARGS>;
        setMemberFunction(name, LuaRef::createFunction(state(), &CppProc::call, CppProc::function(proc)), CppProc::IsConst);
        return *this;
    }

    /**
     * Open a new or existing class for registrations.
     */
    template <typename T2>
    CppBindClass<T2, CppBindClass<T, PARENT>> beginClass(const char* name)
    {
        return CppBindClass<T2, CppBindClass<T, PARENT>>::bind(m_meta, name);
    }

    /**
     * Open a new class to extend the base class.
     */
    template <typename T2, typename SUPER>
    CppBindClass<T2, CppBindClass<T, PARENT>> beginExtendClass(const char* name)
    {
        return CppBindClass<T2, CppBindClass<T, PARENT>>::extend(m_meta, name, CppObjectType<SUPER>::staticID());
    }
    
    /**
     * Continue registration in the enclosing module.
     */
    PARENT endClass() {
      return PARENT(m_meta.rawget<LuaRef>("___module"));
    }
};

