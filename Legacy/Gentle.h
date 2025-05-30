#pragma once

#include <vector>
#include <random>
#include <stdint.h>
#include <execution>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <unordered_map>

namespace GTL
{
	using UUID = uint64_t;

	struct IEntity
	{
		UUID uuid = 0LU;
		bool Enabled = true;
	};

	class FunctionWrapperBase
	{
	public:
		virtual ~FunctionWrapperBase() {}
	};

	template<typename... Args>
	class FunctionWrapper : public FunctionWrapperBase
	{
	public:
		FunctionWrapper() = default;
		std::function<void(Args&...)> f;
	};

	class VariantFunctionHolder
	{
	public:
		FunctionWrapperBase* f;

		template<typename... Args>
		void operator()(Args&&... args)
		{
			FunctionWrapper<std::decay<Args>::type...>* g = dynamic_cast<FunctionWrapper<std::decay<Args>::type...>*>(f);

			if (g == nullptr)
			{
				std::cout << "Error! g is null.\n";
				return;
			}

			g->f(std::forward<Args>(args)...);
		}
	};

	template<typename... Args>
	class HBL_API IMultiView
	{
	public:
		IMultiView()
		{
			for (HBL::IEntity& entt : Registry::Get().GetEntities())
			{
				m_Iterations = 0;
				m_FoundComponents = 0;

				GroupComponents<Args...>(entt);
			}
		}

		IMultiView& ForEach(std::function<void(Args&...)> f)
		{
			FunctionWrapper<Args...>* func = new FunctionWrapper<Args...>();
			func->f = f;
			holder.f = func;

			return *this;
		}

		void Run()
		{
			for (std::tuple<Args...> components : m_GroupedComponents)
			{
				std::apply(holder, components);
			}
		}

	private:
		template <typename... Ts>
		typename std::enable_if<sizeof...(Ts) == 0>::type StoreComponents(IEntity& entt)
		{
			m_GroupedComponents.push_back(m_TemporaryGroup);
		}

		template <typename T, typename... Ts>
		void StoreComponents(HBL::IEntity& entt)
		{
			std::tuple_cat(m_TemporaryGroup, std::make_tuple(Registry::Get().GetComponent<T>(entt)));

			m_ComponentsGroupIndex++;

			StoreComponents<Ts...>(entt);
		}

		template <typename... Ts>
		typename std::enable_if<sizeof...(Ts) == 0>::type GroupComponents(IEntity& entt)
		{
			if (m_Iterations == m_FoundComponents)
			{
				m_ComponentsGroupIndex = 0;
				StoreComponents<Args...>(entt);
			}
		}

		template <typename T, typename... Ts>
		void GroupComponents(IEntity& entt)
		{
			m_Iterations++;

			if (Registry::Get().HasComponent<T>(entt))
			{
				m_FoundComponents++;
			}

			GroupComponents<Ts...>(entt);
		}

		std::vector<std::tuple<Args...>> m_GroupedComponents;
		std::tuple<Args...> m_TemporaryGroup;
		VariantFunctionHolder holder;

		std::size_t m_ComponentsGroupIndex = 0;
		uint32_t m_FoundComponents = 0;
		uint32_t m_Iterations = 0;
	};

	template<typename T>
	class IView
	{
	public:

		IView(const IView&) = delete;

		static IView& Get()
		{
			static IView instance;
			return instance;
		}

		IView& ForEach(std::function<void(T&)> func)
		{
			m_FunctionView = func;
			return *this;
		}

		void Run()
		{
			for (auto&& [uuid, component] : Registry::Get().GetArray<T>())
			{
				m_FunctionView(component);
			}
		}

		void Scedule()
		{
			auto& array = Registry::Get().GetArray<T>();

			std::for_each(
				std::execution::par,
				array.begin(),
				array.end(),
				[&](auto&& item)
				{
					m_FunctionView(item.second);
				});
		}

	private:
		IView() { };

		std::function<void(T&)> m_FunctionView = nullptr;
	};

	class IGroup
	{
	public:
		IGroup(const IGroup&) = delete;

		static IGroup& Get()
		{
			static IGroup instance;
			return instance;
		}

		template<typename... Ts>
		IGroup& Group()
		{
			m_FunctionFilter = nullptr;

			if (!CachedRelationship<Ts...>())
			{
				m_HashCodes.push_back(std::vector<std::size_t>());
				m_AllFilteredEntities.push_back(std::vector<IEntity>());
				m_ActiveRelationShips.push_back(true);

				m_Index = m_AllFilteredEntities.size() - 1;

				for (IEntity& entt : Registry::GetEntities())
				{
					m_ComponentCounter = 0;
					m_Recursions = 0;

					HashCodeFiller<Ts...>(entt);

					// If we found all the components requested, filter entt.
					if (m_ComponentCounter == m_Recursions)
						m_AllFilteredEntities[m_Index].emplace_back(entt);
				}

				if (m_AllFilteredEntities[m_Index].size() == 0)
				{
					m_ActiveRelationShips[m_Index] = false;
					EmptyHashCodeFiller<Ts...>(m_Index);
				}
			}
			return *this;
		}

		IGroup& ForEach(std::function<void(IEntity&)> func)
		{
			m_FunctionFilter = func;
			return *this;
		}

		void Run()
		{
			if (m_Index == UINT32_MAX)
				return;

			for (IEntity& entt : m_AllFilteredEntities[m_Index])
			{
				m_FunctionFilter(entt);
			}
		}

		void Scedule()
		{
			if (m_Index == UINT32_MAX)
				return;

			std::for_each(
				std::execution::par,
				m_AllFilteredEntities[m_Index].begin(),
				m_AllFilteredEntities[m_Index].end(),
				[&](auto&& item)
				{
					m_FunctionFilter(item);
				});
		}

		template<typename T>
		void UpdateGroupsOnAdd(IEntity& entt)
		{
			std::vector<uint32_t> indices = FindHashCodeIndices<T>();

			for (uint32_t index : indices)
			{
				m_AllFilteredEntities[index].emplace_back(entt);
			}
		}

		template<typename T>
		void UpdateGroupsOnRemove(IEntity& entt)
		{
			std::vector<uint32_t> indices = FindHashCodeIndices<T>();

			for (uint32_t index : indices)
			{
				for (int i = 0; i < m_AllFilteredEntities[index].size(); i++)
				{
					if (m_AllFilteredEntities[index][i].uuid == entt.uuid)
						m_AllFilteredEntities[index].erase(m_AllFilteredEntities[index].begin() + i);
				}
			}
		}

		void Clean()
		{
			for (int i = 0; i < m_HashCodes.size(); i++)
			{
				m_HashCodes[i].clear();
			}
			m_HashCodes.clear();

			for (int i = 0; i < m_AllFilteredEntities.size(); i++)
			{
				m_AllFilteredEntities[i].clear();
			}
			m_AllFilteredEntities.clear();

			m_ActiveRelationShips.clear();
		}

	private:
		IGroup() { };

		template<typename T>
		std::vector<uint32_t> FindHashCodeIndices()
		{
			std::vector<uint32_t> indices;

			for (int i = 0; i < m_HashCodes.size(); i++)
			{
				for (int j = 0; j < m_HashCodes[i].size(); j++)
				{
					if (m_HashCodes[i][j] == typeid(T).hash_code())
					{
						indices.push_back(i);
					}
				}
			}

			return indices;
		}

		template <typename... Ts>
		typename std::enable_if<sizeof...(Ts) == 0>::type HashCodeFiller(IEntity& entt) { }

		template <typename T, typename... Ts>
		void HashCodeFiller(IEntity& entt)
		{
			m_Recursions++;

			if (Registry::Get().HasComponent<T>(entt))
			{
				bool exists = false;
				for (auto& code : m_HashCodes[m_HashCodes.size() - 1])
				{
					if (code == typeid(T).hash_code())
					{
						exists = true;
						break;
					}
				}

				if (!exists)
					m_HashCodes[m_HashCodes.size() - 1].push_back(typeid(T).hash_code());

				m_ComponentCounter++;
			}

			HashCodeFiller<Ts...>(entt);
		}

		template <typename... Ts>
		typename std::enable_if<sizeof...(Ts) == 0>::type EmptyHashCodeFiller(uint32_t index) { }

		template <typename T, typename... Ts>
		void EmptyHashCodeFiller(uint32_t index)
		{
			m_HashCodes[index].push_back(typeid(T).hash_code());
			EmptyHashCodeFiller<Ts...>(index);
		}

		template <typename... Ts>
		typename std::enable_if<sizeof...(Ts) == 0>::type HashCodeSearcher(int index)
		{
			if (m_ComponentRecursions != m_ComponentsFound)
			{
				m_Index = UINT32_MAX;
			}
			else
			{
				m_Index = index;
			}
		}

		template <typename T, typename... Ts>
		void HashCodeSearcher(int index)
		{
			m_ComponentRecursions++;

			for (int j = 0; j < m_HashCodes[index].size(); j++)
			{
				if (m_HashCodes[index][j] == typeid(T).hash_code())
				{
					m_ComponentsFound++;
					break;
				}
			}

			HashCodeSearcher<Ts...>(index);
		}

		template <typename... Ts>
		bool CachedRelationship()
		{
			for (int i = 0; i < m_HashCodes.size(); i++)
			{
				m_ComponentRecursions = 0;
				m_ComponentsFound = 0;
				m_Index = UINT32_MAX;

				HashCodeSearcher<Ts...>(i);

				if (m_Index != UINT32_MAX)
					return true;
			}

			return false;
		}

		std::function<void(IEntity&)> m_FunctionFilter = nullptr;

		uint32_t m_Recursions = 0;
		uint32_t m_ComponentCounter = 0;

		uint32_t m_ComponentsFound = 0;
		uint32_t m_ComponentRecursions = 0;
		uint32_t m_EmptyRelationShips = 0;

		uint32_t m_Index = 0;
		uint32_t m_EmptyIndex = 0;

		std::vector<bool> m_ActiveRelationShips;
		std::vector<std::vector<std::size_t>> m_HashCodes;
		std::vector<std::vector<IEntity>> m_AllFilteredEntities;
	};

	class Registry
	{
	public:
		Registry(const Registry&) = delete;

		static Registry& Get()
		{
			static Registry instance;
			return instance;
		}

		void Initialize()
		{
			m_RandomEngine.seed(std::random_device()());
		}

		IEntity CreateEntity()
		{
			IEntity entity;
			entity.uuid = GenerateUUID();
			return entity;
		}

		template<typename T>
		T& AddComponent(IEntity& Entity)
		{
			//ASSERT(!HasComponent<T>(Entity));

			T component;

			auto& array = GetArray<T>();
			array.emplace(Entity.uuid, component);

			IGroup::Get().UpdateGroupsOnAdd<T>(Entity);

			return component;
		}

		template<typename T>
		T& GetComponent(IEntity& Entity)
		{
			//ASSERT(HasComponent<T>(Entity));

			auto& array = GetArray<T>();
			return array[Entity.uuid];
		}

		template<typename T>
		bool HasComponent(IEntity& Entity)
		{
			auto& array = GetArray<T>();
			return (array.find(Entity.uuid) != array.end());
		}

		template<typename T>
		void RemoveComponent(IEntity& Entity)
		{
			ASSERT(HasComponent<T>(Entity));

			auto& array = GetArray<T>();
			array.erase(Entity.uuid);

			IGroup::Get().UpdateGroupsOnRemove<T>(Entity);
		}

		template<typename T>
		void AddArray()
		{
			std::unordered_map<UUID, T>* array = new std::unordered_map<UUID, T>();
			m_ComponentArrays.emplace(typeid(T).hash_code(), array);
		}

		template<typename T>
		std::unordered_map<UUID, T>& GetArray()
		{
			auto& array = *(std::unordered_map<UUID, T>*)m_ComponentArrays[typeid(T).hash_code()];
			return array;
		}

		template<typename T>
		void ClearArray()
		{
			GetArray<T>().clear();
		}

		std::vector<IEntity>& GetEntities()
		{
			return m_Entities;
		}

		void Flush()
		{
			m_Entities.clear();
		}

		void Clean()
		{
			IGroup::Get().Clean();
		}

		template<typename T>
		IView<T>& View()
		{
			return IView<T>::Get();
		}

		template<typename... Ts>
		IGroup& Group()
		{
			return IGroup::Get().Group<Ts...>();
		}

		template<typename... Ts>
		IMultiView<Ts...>& MultiView()
		{
			IMultiView<Ts...>* mv = new IMultiView<Ts...>();
			return *mv;
		}

	private:
		Registry() { }

		UUID GenerateUUID()
		{
			uint64_t floor = 0;
			uint64_t ceiling = UINT64_MAX;
			return (UUID)((((double)m_Distribution(m_RandomEngine) / (double)std::numeric_limits<uint64_t>::max()) * (ceiling - floor) + floor));
		}

		std::mt19937_64 m_RandomEngine;
		std::uniform_int_distribution<std::mt19937_64::result_type> m_Distribution;

		std::vector<IEntity> m_Entities;
		std::unordered_map<std::size_t, void*> m_ComponentArrays;
	};
}