#pragma once
#include "Block/BlockType.h"

#include <Aphelion/Core/Transform.h>
#include <Aphelion/Renderer/VertexArray.h>
#include <Aphelion/ECS/Registry.h>

#include <glm/vec3.hpp>
#include <vector>

#include <bitsery/traits/array.h>

/// IVec so that we can use it easily in calculations
constexpr glm::vec<3,uint8_t> chunkDimensions {32,32,32};
constexpr unsigned chunkCount = chunkDimensions.x * chunkDimensions.y * chunkDimensions.z;
/// TODO: Instead of using default alloc, create a ChunkAlloc
//template <typename T>
//using ChunkSlice = std::vector<T>;
using ChunkContainer = std::vector<BlockType>;
    //ChunkSlice<ChunkSlice<ChunkSlice<BlockType,chunkDimensions.z>,chunkDimensions.y>, chunkDimensions.x>;
    //ChunkSlice<ChunkSlice<ChunkSlice<BlockType>>>;

struct ChunkDataComponent
{
    ChunkDataComponent()
        : pos(0)
    {
    }

    glm::vec3 pos;
    ChunkContainer chunk;

    bool operator==(const ChunkDataComponent& rhs) const { return true; }
};

template <typename S>
void serialize(S& s, ChunkDataComponent& o)
{
    serialize(s,o.pos);
    s.container1b(o.chunk, chunkCount);
}

struct ChunkSpawnCooldownComponent
{
    float time = 0.f;
};
inline bool operator==(const ChunkSpawnCooldownComponent& lhs, const ChunkSpawnCooldownComponent& rhs) { return true; }
template <typename S> void serialize(S& s, ChunkSpawnCooldownComponent& o) { s.value4b(o.time); }

struct ChunkSpawnComponent
{
    glm::vec3 pos;
};
inline bool operator==(const ChunkSpawnComponent& lhs, const ChunkSpawnComponent& rhs) { return true; }
template <typename S> void serialize(S& s, ChunkSpawnComponent& o) { serialize(s, o.pos); }

struct ChunkMeshComponent
{
    ap::VertexArrayRef vao;
};
inline bool operator==(const ChunkMeshComponent& lhs, const ChunkMeshComponent& rhs) { return true; }
template <typename S> void serialize(S& s, ChunkMeshComponent& o) {}

struct ChunkModifiedComponent
{
    char empty;
};
inline bool operator==(const ChunkModifiedComponent& lhs, const ChunkModifiedComponent& rhs) { return true; }
template <typename S> void serialize(S& s, ChunkModifiedComponent& o) {}

inline BlockType& GetBlock(ChunkContainer& chunk, unsigned x, unsigned y, unsigned z)
{
    return chunk.at((x * chunkDimensions.x * chunkDimensions.y) + (y * chunkDimensions.y) + z);
}

inline const BlockType& GetBlock(const ChunkContainer& chunk, unsigned x, unsigned y, unsigned z)
{
    return chunk.at((x * chunkDimensions.x * chunkDimensions.y) + (y * chunkDimensions.y) + z);
}

/**
 * Expected parameters
 * block& x y z
 */
template <typename Callable>
void ForEach(ChunkContainer& chunk, Callable& callable)
{
    for(auto x = 0; x < chunkDimensions.x; x++)
        for(auto y = 0; y < chunkDimensions.y; y++)
            for(auto z = 0; z < chunkDimensions.z; z++)
                callable(GetBlock(chunk,x,y,z),x,y,z);
                //callable(chunk[x][y][x],x,y,z);
}

template <typename Callable>
void ForEach(const ChunkContainer& chunk, Callable& callable)
{
    for(auto x = 0; x < chunkDimensions.x; x++)
        for(auto y = 0; y < chunkDimensions.y; y++)
            for(auto z = 0; z < chunkDimensions.z; z++)
                callable(GetBlock(chunk,x,y,z),x,y,z);
}

inline void RegisterChunkComponents()
{
    ap::Registry::RegisterComponent<ChunkDataComponent>();
    ap::Registry::RegisterComponent<ChunkMeshComponent>();
    ap::Registry::RegisterComponent<ChunkModifiedComponent>();
    ap::Registry::RegisterComponent<ChunkSpawnCooldownComponent>();
    ap::Registry::RegisterComponent<ChunkSpawnComponent>();
}