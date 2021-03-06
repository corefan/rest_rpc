#pragma once

//member function
#define TIMAX_FUNCTION_TRAITS(...)\
template <typename ReturnType, typename ClassType, typename... Args>\
struct function_traits<ReturnType(ClassType::*)(Args...) __VA_ARGS__> : function_traits<ReturnType(Args...)>{};\

namespace std
{
	template <int Size>
	struct is_placeholder<boost::arg<Size>>
		: integral_constant<int, Size>
	{};
}

namespace timax 
{
	 /*
	  * 1. function type							==>	Ret(Args...)
	  * 2. function pointer							==>	Ret(*)(Args...)
	  * 3. function reference						==>	Ret(&)(Args...)
	  * 4. pointer to non-static member function	==> Ret(T::*)(Args...)
	  * 5. function object and functor				==>> &T::operator()
	  * 6. function with generic operator call		==> template <typeanme ... Args> &T::operator()
	  */

	//转换为std::function和函数指针 
	template<typename T>
	struct function_traits;

	//普通函数
	template<typename Ret, typename... Args>
	struct function_traits<Ret(Args...)>
	{
	public:
		enum { arity = sizeof...(Args) };
		typedef Ret function_type(Args...);
		typedef Ret result_type;
		using stl_function_type = std::function<function_type>;
		typedef Ret(*pointer)(Args...);

		template<size_t I>
		struct args
		{
			static_assert(I < arity, "index is out of range, index must less than sizeof Args");
			using type = typename std::tuple_element<I, std::tuple<Args...>>::type;
		};

		typedef std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...> tuple_type;
		using raw_tuple_type = std::tuple<Args...>;
	};

	//函数指针
	template<typename Ret, typename... Args>
	struct function_traits<Ret(*)(Args...)> : function_traits<Ret(Args...)> {};

	//函数引用
	template<typename Ret, typename... Args>
	struct function_traits<Ret(&)(Args...)> : function_traits<Ret(Args...)> {};

	//std::function
	template <typename Ret, typename... Args>
	struct function_traits<std::function<Ret(Args...)>> : function_traits<Ret(Args...)> {};

	TIMAX_FUNCTION_TRAITS()
	TIMAX_FUNCTION_TRAITS(const)
	TIMAX_FUNCTION_TRAITS(volatile)
	TIMAX_FUNCTION_TRAITS(const volatile)

	//函数对象
	template<typename Callable>
	struct function_traits : function_traits<decltype(&std::remove_reference_t<Callable>::operator())> {};

	template <typename Function>
	typename function_traits<Function>::stl_function_type to_function(const Function& lambda)
	{
		return static_cast<typename function_traits<Function>::stl_function_type>(lambda);
	}

	template <typename Function>
	typename function_traits<Function>::stl_function_type to_function(Function&& lambda)
	{
		return static_cast<typename function_traits<Function>::stl_function_type>(std::forward<Function>(lambda));
	}

	template <typename Function>
	typename function_traits<Function>::pointer to_function_pointer(const Function& lambda)
	{
		return static_cast<typename function_traits<Function>::pointer>(lambda);
	}
}

namespace timax
{
	template <typename Arg0, typename Tuple>
	struct push_front_to_tuple_type;

	template <typename Arg0, typename ... Args>
	struct push_front_to_tuple_type<Arg0, std::tuple<Args...>>
	{
		using type = std::tuple<Arg0, Args...>;
	};

	template <size_t I, typename IndexSequence>
	struct push_front_to_index_sequence;

	template <size_t I, size_t ... Is>
	struct push_front_to_index_sequence<I, std::index_sequence<Is...>>
	{
		using type = std::index_sequence<I, Is...>;
	};

	template <typename ... BindArgs>
	struct make_bind_index_sequence;

	template <>
	struct make_bind_index_sequence<>
	{
		using type = std::index_sequence<>;
	};

	template <typename Arg0, typename ... Args>
	struct make_bind_index_sequence<Arg0, Args...>
	{
	private:
		using arg0_type = std::remove_reference_t<std::remove_cv_t<Arg0>>;
		constexpr static auto ph_value = std::is_placeholder<arg0_type>::value;
		using rests_index_sequence_t = typename make_bind_index_sequence<Args...>::type;
		
	public:
		using type = std::conditional_t<0 == ph_value, rests_index_sequence_t,
			typename push_front_to_index_sequence<ph_value - 1, rests_index_sequence_t>::type>;
	};

	template <typename Ret, typename ... Args>
	struct function_helper
	{
		using type = std::function<Ret(Args...)>;
	};

	template <typename IndexSequence, typename Ret, typename ArgsTuple>
	struct bind_traits;

	template <size_t ... Is, typename Ret, typename ArgsTuple>
	struct bind_traits<std::index_sequence<Is...>, Ret, ArgsTuple>
	{
		using type = typename function_helper<Ret, std::tuple_element_t<Is, ArgsTuple>...>::type;
	};

	template <typename F, typename ... Args>
	struct bind_to_function
	{
	private:
		using index_sequence_t = typename make_bind_index_sequence<Args...>::type;
		using function_traits_t = function_traits<F>;
	public:
		using type = typename bind_traits<index_sequence_t, typename function_traits_t::result_type, typename function_traits_t::raw_tuple_type>::type;
	};

	template <typename F, typename Arg0, typename ... Args>
	auto bind_impl(std::false_type /*IsNoPmf*/, F&& f, Arg0&& arg, Args&& ... args)
		-> typename bind_to_function<F, Args...>::type
	{
		return std::bind(std::forward<F>(f), std::forward<Arg0>(arg0), std::forward<Args>(args)...);
	}

	template <typename F>
	auto bind_impl(std::false_type /*IsNoPmf*/, F&& f)
		-> typename function_traits<F>::stl_function_type
	{
		return [func = std::forward<F>(f)](auto&& ... args){ return func(std::forward<decltype(args)>(args)...); };
	}

	template <typename F, typename Caller, typename Arg0, typename ... Args>
	auto bind_impl(std::true_type /*IsPmf*/, F&& pmf, Caller&& caller, Arg0&& arg0, Args&& ... args)
		-> typename bind_to_function<typename function_traits<F>::function_type, Arg0, Args...>::type
	{
		return std::bind(pmf, std::forward<Caller>(caller), std::forward<Arg0>(arg0), std::forward<Args>(args)...);
	}

	template <typename ...>
	struct voider
	{
		using type = void;
	};

	template <typename ... Args>
	using voider_t = typename voider<Args...>::type;

	template <typename T, typename = void>
	struct is_smart_pointer : std::false_type
	{
	};

	template <typename T>
	struct is_smart_pointer<T, 
		voider_t<
			decltype(std::declval<T>().operator ->()),
			decltype(std::declval<T>().get())
		>> : std::true_type
	{
	};

	struct caller_is_a_pointer {};
	struct caller_is_a_smart_pointer {};
	struct caller_is_a_reference {};

	template <typename F, typename Caller>
	auto bind_impl_pmf_no_placeholder(caller_is_a_pointer, F&& pmf, Caller&& caller)
		-> typename function_traits<F>::stl_function_type
	{
		return [pmf, c = std::forward<Caller>(caller)](auto&& ... args) { return (c->*pmf)(std::forward<decltype(args)>(args)...); };
	}

	template <typename F, typename Caller>
	auto bind_impl_pmf_no_placeholder(caller_is_a_smart_pointer, F&& pmf, Caller&& caller)
		-> typename function_traits<F>::stl_function_type
	{
		return[pmf, c = std::forward<Caller>(caller)](auto&& ... args) { return (c.get()->*pmf)(std::forward<decltype(args)>(args)...); };
	}

	template <typename F, typename Caller>
	auto bind_impl_pmf_no_placeholder(caller_is_a_reference, F&& pmf, Caller&& caller)
		-> typename function_traits<F>::stl_function_type
	{
		return [pmf, c = std::forward<Caller>(caller)](auto&& ... args) { return (c.*pmf)(std::forward<decltype(args)>(args)...); };
	}

	template <typename F, typename Caller>
	auto bind_impl(std::true_type /*IsPmf*/, F&& pmf, Caller&& caller)
	{
		using caller_category_t = std::conditional_t<std::is_pointer<Caller>::value,
			caller_is_a_pointer, std::conditional_t<is_smart_pointer<Caller>::value,
				caller_is_a_smart_pointer, caller_is_a_reference >> ;

		return bind_impl_pmf_no_placeholder(caller_category_t{}, std::forward<F>(pmf), std::forward<Caller>(caller));
	}

	template <typename F, typename ... Args>
	auto bind(F&& f, Args&& ... args)
	{
		using is_pmf = typename std::is_member_function_pointer<F>::type;

		return bind_impl(is_pmf{}, std::forward<F>(f), std::forward<Args>(args)...);
	}
}