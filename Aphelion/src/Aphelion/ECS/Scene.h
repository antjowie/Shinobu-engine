#pragma once
#include "Aphelion/Core/Core.h"
#include "Aphelion/ECS/Registry.h"

namespace ap
{
    class Timestep;

    /**
     * A Scene (aka game world) hold our registry and versions of it.
     * 
     * This is used to rollback the simulation for net features such as
     * server reconciliation, client prediction and interpolation
     *
     * TODO: Make the simulation multi-threaded
     */
    class APHELION_API Scene : public NonCopyable
    {
    public:
        constexpr static unsigned maxSimulations = 256 / 4;
        using SystemFunc = std::function<void(Scene& scene)>;

    public:
        Scene();

        Registry& GetRegistry(unsigned rollback = 0);
        PhysicsScene& GetPhysicsScene() { return m_physicsScene; }

        unsigned GetSimulationCount() const { return m_simulationCount; }
        unsigned GetCurrentSimulationIndex() const { return m_currentSimulation; }

        void Simulate(Timestep ts);
        std::vector<std::pair<Entity,PhysicsRaycastHit>> Raycast(const glm::vec3& origin, const glm::vec3& dir, float distance);

        /**
         * Register a system that this ECS should use
         * 
         * Registering systems allows us to simulate the world via one call.
         * This allows us to reconcile the world.
         *
         * @param system Must be a callable
         */
        template <typename T>
        void RegisterSystem(T&& system)
        {
            static_assert(std::is_invocable_v<T, Scene&>,
                "T should be callable, make sure operator() is overloaded with argument Registry&");
            m_systems.push_back(std::forward<T>(system));
        }

        void ClearSystems();


        void SetOnEntityDestroyCb(Registry::EntityCb cb);
        void SetOnEntityCreateCb(Registry::EntityCb cb);

        /**
         * Called in the create/remove component callback if they need the scene
         * An example is the physics component
         */
        void HandleComponentCreate(Entity e, unsigned compID);
        void HandleComponentRemove(Entity e, unsigned compID);

    private:
        Registry m_registries[maxSimulations];
        PhysicsScene m_physicsScene;
        unsigned m_currentSimulation = 0;
        unsigned m_simulationCount = 0;

        std::vector<SystemFunc> m_systems;
    };
}