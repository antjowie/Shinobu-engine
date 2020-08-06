#include "Aphelion/ECS/Registry.h"

namespace ap
{
    std::unordered_map<entt::id_type, Registry::CompData> Registry::m_compData;

    Entity Registry::Create()
    {
        auto id = m_reg.create();
        if(m_onCreate) m_onCreate(id);
        return id;
    }

    Entity Registry::Create(Entity hint)
    {
        auto id = m_reg.create(hint);
        // I've added this assert since there may be an issue when we predict new entities
        // locally but the server creates a new one. In this case, the ID's may not match. To solve this,
        // we either disallow the client from ever making entities themselves or we 
        // use a different ID system
        //
        // For example, a system that the Source engine uses is if the entity does not yet exist, 
        // create it. And only then. (I think)
        //
        // I HAVE A WAY BETTER SOLUTION!
        // Create a network ID component which stores the network ID of the entity.
        // This allows us to always refer to the same object
        AP_CORE_ASSERT(id == hint, "Could not recreate the hint ID");
        if(m_onCreate) m_onCreate(id);
        return id;
    }

    void Registry::Destroy(Entity entity)
    {
        if(m_onDestroy) m_onDestroy(entity);
        m_reg.destroy(entity);
    }

    void Registry::HandlePacket(Entity entity, Packet& packet)
    {
        //m_compData[compID].unpack(m_reg, entity, packet);
        AP_CORE_ASSERT(m_compData.count(packet.id) == 1, "Component is not registered or is incorrect");
        m_compData.at(packet.id).unpack(*this, entity, packet);
    }

    bool Registry::HandleAndReconcilePacket(Entity entity, Packet& packet)
    {
        AP_CORE_ASSERT(m_compData.count(packet.id) == 1, "Component is not registered or is incorrect");
        return m_compData.at(packet.id).unpackAndReconcile(*this, entity, packet);
    }

    void Registry::Clone(Registry& from)
    {
        Get().clear();
        // Iterate over each entity in from registry
        from.Get().each([&](const entt::entity e)
            {
                // If entity does not exist create it
                // NOTE: This will pretty much always throw an exception since ENTT does not expect you to check for an entity
                // that has never been created. Because of this, I just always recreate each entity (which should be the same
                // runtime cost but I'm not worried about that atm)
                //if (Get().valid(e)) AP_CORE_VERIFY(Get().create(e) == e, "Could not copy entity from registry");

                // NOTE: For some reason I can't really copy a entity with all their values (such as version and id etc) so I 
                // ignore these details since I don't think we'll need them

                auto newE = Get().create(e);
                AP_CORE_VERIFY(e == newE, "Could not copy entity from registry");
                //AP_CORE_VERIFY(entt::to_integral(Get().create(e)) == entt::to_integral(e), "Could not copy entity from registry");
                //entt::to_integral(Get().create(e)) == entt::to_integral(e);

                // Copy all components into here
                from.Get().visit(e, [&](const entt::id_type component)
                    {
                        //AP_CORE_TRACE(m_compData.at(component).name);
                        AP_CORE_ASSERT(m_compData.count(component) == 1, "Component is not registered")
                        m_compData.at(component).stamp(from.Get(), e, Get(), newE);
                        //Get().emplace_or_replace<T>(dst, from.get<T>(src));
                    });
            });
    }
}