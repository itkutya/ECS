#include <algorithm>
#include <optional>
#include <concepts>
#include <cstdint>
#include <utility>
#include <vector>
#include <ranges>

#include "fmt/core.h"

// Represents an entity in the ECS system
struct Entity
{
    using IDType = std::uint32_t; // Entity ID type
    IDType id{};
};

// Concept to ensure a type is a valid component
template<class T>
concept ComponentConcept = std::movable<T> && std::default_initializable<T>;

// Concept to ensure a type is a valid system
template<class System, class... Args>
concept SystemConcept =
    requires(System system, typename System::ComponentType& component, Args&&... args)
    {
        typename System::ComponentType; // System must define a ComponentType
        { system(component, std::forward<Args>(args)...) } noexcept; // System must be callable with a component and arguments
    } && std::default_initializable<System> && ComponentConcept<typename System::ComponentType>;

// ECS (Entity-Component-System) class
class ECS final
{
    // Type alias for a pair of Entity ID and Component
    template<ComponentConcept Component>
    using EntityComponentPair = std::pair<Entity::IDType, Component>;

    // Type alias for a list of Entity-Component pairs
    template<ComponentConcept Component>
    using EntityComponentList = std::vector<EntityComponentPair<Component>>;

    // Helper function to match an entity ID in a list of components
    template<ComponentConcept Component>
    [[nodiscard]] static constexpr auto matchEntity(Entity::IDType entityID) noexcept
    {
        return [entityID](const EntityComponentPair<Component>& pair) noexcept -> bool
        {
            return pair.first == entityID;
        };
    }
public:
    // Delete default constructor and copy/move operations to enforce static usage
    ECS() noexcept = delete;
    ECS(const ECS&) noexcept = delete;
    ECS(ECS&&) noexcept = delete;
    ECS& operator=(const ECS&) noexcept = delete;
    ECS& operator=(ECS&&) noexcept = delete;
    ~ECS() noexcept = default;

    // Add a component to an entity
    template<ComponentConcept Component>
    static void addComponentToEntity(Entity::IDType entityID, Component&& component) noexcept
    {
        EntityComponentList<Component>& components = get<Component>();
        if (std::ranges::find_if(components, matchEntity<Component>(entityID)) == components.end())
            components.emplace_back(entityID, std::move(component));
    }

    // Remove a component from an entity
    template<ComponentConcept Component>
    static void removeComponentFromEntity(Entity::IDType entityID) noexcept
    {
        std::erase_if(get<Component>(), matchEntity<Component>(entityID));
    }

    // Check if an entity has a specific component
    template<ComponentConcept Component>
    [[nodiscard]] static bool entityHasComponent(Entity::IDType entityID) noexcept
    {
        const EntityComponentList<Component>& components = get<Component>();
        return std::ranges::find_if(components, matchEntity<Component>(entityID)) != components.end();
    }

    // Get a pointer to a component of an entity, if it exists
    template<ComponentConcept Component>
    [[nodiscard]] static std::optional<Component*> getComponentOfEntity(Entity::IDType entityID) noexcept
    {
        EntityComponentList<Component>& components = get<Component>();
        if (auto it = std::ranges::find_if(components, matchEntity<Component>(entityID)); it != components.end())
            return std::optional<Component*>{ &it->second };
        else
            return std::optional<Component*>{ std::nullopt };
    }

    // Get a view of all components of a specific type
    template<ComponentConcept Component>
    [[nodiscard]] static auto getComponentsView() noexcept
    {
        return get<Component>() | std::views::transform([](auto& pair) -> Component* { return &pair.second; });
    }

    // Get a view of all entity IDs that have a specific component
    template<ComponentConcept Component>
    [[nodiscard]] static auto getEntityIDsWithComponentView() noexcept
    {
        return get<Component>() | std::views::transform([](auto& pair) -> Entity::IDType { return pair.first; });
    }

    // Apply a system to an entity's component
    template<class System, class... Args> requires SystemConcept<System, Args...>
    static void applySystem(Entity::IDType entityID, Args&&... args) noexcept
    {
        using Component = typename System::ComponentType;
        EntityComponentList<Component>& components = get<Component>();
        if (auto it = std::ranges::find_if(components, matchEntity<Component>(entityID)); it != components.end())
            System{}(it->second, std::forward<Args>(args)...);
    }

private:
    // Get the static list of components for a specific type
    template<ComponentConcept Component>
    [[nodiscard]] static EntityComponentList<Component>& get() noexcept
    {
        static EntityComponentList<Component> instance{};
        return instance;
    }
};

// Position component representing an entity's position in 2D space
struct Position final
{
    float x{};
    float y{};
};

// MoveSystem to update an entity's position based on a time delta
struct MoveSystem final
{
    using ComponentType = Position;

    void operator()(Position& position, float dt) const noexcept
    {
        position.x += 1.0f * dt;
        position.y += 1.0f * dt;
    }
};

// GravitySystem to apply gravity to an entity's position
struct GravitySystem final
{
    using ComponentType = Position;

    void operator()(Position& position, float dt) const noexcept
    {
        position.y -= 9.81f * dt;
    }
};

int main([[maybe_unused]] int, [[maybe_unused]] char**)
{
    // Example usage of the ECS system
    {
        Entity entity{1};

        // Add, remove, and re-add a Position component to an entity
        ECS::addComponentToEntity(entity.id, Position{ 0.0f, 0.0f });
        ECS::removeComponentFromEntity<Position>(entity.id);
        ECS::addComponentToEntity(entity.id, Position{ 2.0f, 3.0f });

        // Apply the MoveSystem to the entity
        ECS::applySystem<MoveSystem>(entity.id, 0.016f);

        // Print all entities with a Position component
        const auto entities = ECS::getEntityIDsWithComponentView<Position>();
        fmt::print("Entities with Position component: {}\n", entities.size());
        for (const Entity::IDType entityID : entities)
            fmt::print("Entity ID: {}\n", entityID);

        // Print all Position components
        const auto positions = ECS::getComponentsView<Position>();
        for (const Position* position : positions)
            fmt::print("Position: ({}, {})\n", position->x, position->y);

        // Remove the Position component from the entity
        ECS::removeComponentFromEntity<Position>(entity.id);
    }

    // Additional examples with multiple entities
    {
        Entity entity2{2};
        Entity entity3{3};
        Entity entity4{4};
        Entity entity5{5};

        // Add Position components to multiple entities
        ECS::addComponentToEntity(entity2.id, Position{ 5.0f, 5.0f });
        ECS::addComponentToEntity(entity3.id, Position{ 10.0f, 10.0f });
        ECS::addComponentToEntity(entity4.id, Position{ 15.0f, 15.0f });
        ECS::addComponentToEntity(entity5.id, Position{ 20.0f, 20.0f });

        // Print entities with Position components
        {
            const auto entities = ECS::getEntityIDsWithComponentView<Position>();
            fmt::print("Entities with Position component: {}\n", entities.size());
        }

        // Apply the MoveSystem to the first two entities
        for (Entity::IDType entityID : ECS::getEntityIDsWithComponentView<Position>() | std::views::take(2))
        {
            ECS::applySystem<MoveSystem>(entityID, 0.016f);
            fmt::print("Entity ID after move: {}\n", entityID);
            if (const std::optional<Position*> pos = ECS::getComponentOfEntity<Position>(entityID); pos.has_value())
                fmt::print("Position after move: ({}, {})\n", (*pos)->x, (*pos)->y);
        }

        // Print all entities and their Position components
        {
            const auto entities = ECS::getEntityIDsWithComponentView<Position>();
            fmt::print("Entities with Position component: {}\n", entities.size());
            
            for (Entity::IDType entityID : entities)
                fmt::print("Entity ID: {}\n", entityID);

            const auto positions = ECS::getComponentsView<Position>();
            for (const Position* position : positions)
                fmt::print("Position: ({}, {})\n", position->x, position->y);
        }

        // Remove Position components from all entities
        ECS::removeComponentFromEntity<Position>(entity2.id);
        ECS::removeComponentFromEntity<Position>(entity3.id);
        ECS::removeComponentFromEntity<Position>(entity4.id);
        ECS::removeComponentFromEntity<Position>(entity5.id);
    }

    // Example with GravitySystem
    {
        Entity entity6{6};
        
        ECS::addComponentToEntity(entity6.id, Position{ 50.0f, 50.0f });

        // Apply the GravitySystem to the entity
        ECS::applySystem<GravitySystem>(entity6.id, 0.016f);
        if (const std::optional<Position*> pos = ECS::getComponentOfEntity<Position>(entity6.id); pos.has_value())
            fmt::print("Entity ID after gravity: {}\nPosition after gravity: ({}, {})\n", entity6.id, (*pos)->x, (*pos)->y);

        ECS::removeComponentFromEntity<Position>(entity6.id);
    }

    // Check if an entity has a component
    {
        Entity entity7{7};
        ECS::addComponentToEntity(entity7.id, Position{ 25.0f, 25.0f });
        fmt::print("Entity7 has Position component: {}\n", ECS::entityHasComponent<Position>(entity7.id));
        ECS::removeComponentFromEntity<Position>(entity7.id);
        fmt::print("Entity7 has Position component after removal: {}\n", ECS::entityHasComponent<Position>(entity7.id));
    }

    return 0;
}