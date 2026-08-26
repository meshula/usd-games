/**
 * schema_memory_layout.h
 * 
 * Memory layout optimization techniques for USD schemas in game engines.
 * This implementation demonstrates:
 * - Schema-specific memory pools
 * - Optimized memory layouts for common game schemas
 * - Transformations between USD and optimized layouts
 * - Cache-aligned property structures
 * - SIMD-friendly data structures
 */

#pragma once

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/gf/matrix4f.h>

#include <vector>
#include <memory>
#include <unordered_map>
#include <array>
#include <algorithm>
#include <cassert>
#include <cstring>

#ifdef _MSC_VER
#define ALIGNED(x) __declspec(align(x))
#else
#define ALIGNED(x) __attribute__((aligned(x)))
#endif

// Choose cache line size based on target architecture
// 64 bytes is common for modern CPUs
#define CACHE_LINE_SIZE 64

PXR_NAMESPACE_USING_DIRECTIVE

namespace SchemaMemoryLayout {

/**
 * Memory Pool for schema data
 * 
 * Manages contiguous chunks of memory for schema data structures
 * with proper alignment for cache efficiency.
 */
class MemoryPool {
public:
    MemoryPool(size_t blockSize = 16384, size_t alignment = CACHE_LINE_SIZE)
        : m_blockSize(blockSize)
        , m_alignment(alignment)
        , m_currentBlock(nullptr)
        , m_currentPos(0)
        , m_currentBlockSize(0)
    {
        // Allocate first block
        allocateBlock();
    }
    
    ~MemoryPool() {
        // Free all blocks
        for (void* block : m_blocks) {
            // Alignment-aware free
            #ifdef _MSC_VER
            _aligned_free(block);
            #else
            free(block);
            #endif
        }
    }
    
    /**
     * Allocate memory from the pool with proper alignment
     * 
     * @param size Size in bytes to allocate
     * @return Pointer to allocated memory
     */
    void* allocate(size_t size) {
        // Align size to ensure subsequent allocations remain aligned
        size_t alignedSize = ((size + m_alignment - 1) / m_alignment) * m_alignment;
        
        // Check if we need a new block
        if (m_currentPos + alignedSize > m_currentBlockSize) {
            // If requested size is larger than our standard block size,
            // allocate a custom-sized block for it
            if (alignedSize > m_blockSize) {
                void* specialBlock;
                #ifdef _MSC_VER
                specialBlock = _aligned_malloc(alignedSize, m_alignment);
                #else
                posix_memalign(&specialBlock, m_alignment, alignedSize);
                #endif
                
                m_blocks.push_back(specialBlock);
                return specialBlock;
            }
            
            // Otherwise allocate a new standard block
            allocateBlock();
        }
        
        void* ptr = static_cast<char*>(m_currentBlock) + m_currentPos;
        m_currentPos += alignedSize;
        
        return ptr;
    }

private:
    void allocateBlock() {
        #ifdef _MSC_VER
        m_currentBlock = _aligned_malloc(m_blockSize, m_alignment);
        #else
        posix_memalign(&m_currentBlock, m_alignment, m_blockSize);
        #endif
        
        m_blocks.push_back(m_currentBlock);
        m_currentPos = 0;
        m_currentBlockSize = m_blockSize;
    }
    
    size_t m_blockSize;
    size_t m_alignment;
    void* m_currentBlock;
    size_t m_currentPos;
    size_t m_currentBlockSize;
    std::vector<void*> m_blocks;
};

/**
 * Schema-Specific Memory Pools
 * 
 * Maintains separate memory pools for different schema types
 * to ensure optimal memory layout and reduce fragmentation.
 */
class SchemaPoolManager {
public:
    /**
     * Get the memory pool for a specific schema type
     * 
     * @param schemaName The name of the schema
     * @return Reference to the memory pool
     */
    MemoryPool& getPool(const std::string& schemaName) {
        auto it = m_pools.find(schemaName);
        if (it == m_pools.end()) {
            // Create new pool for this schema
            return m_pools.emplace(schemaName, MemoryPool()).first->second;
        }
        return it->second;
    }
    
    /**
     * Reset all memory pools
     */
    void reset() {
        m_pools.clear();
    }
    
    /**
     * Get the singleton instance
     */
    static SchemaPoolManager& getInstance() {
        static SchemaPoolManager instance;
        return instance;
    }

private:
    SchemaPoolManager() = default;
    std::unordered_map<std::string, MemoryPool> m_pools;
};

/**
 * Cache-aligned optimized layout for HealthComponent data
 */
struct ALIGNED(CACHE_LINE_SIZE) OptimizedHealthData {
    float currentHealth;
    float maxHealth;
    float regenerationRate;
    uint32_t flags;  // Bit 0: invulnerable, others reserved for future use
    
    // Default constructor initializes with reasonable values
    OptimizedHealthData()
        : currentHealth(100.0f)
        , maxHealth(100.0f)
        , regenerationRate(0.0f)
        , flags(0)
    {}
    
    // Set invulnerable flag
    void setInvulnerable(bool invulnerable) {
        if (invulnerable) {
            flags |= 1;
        } else {
            flags &= ~1;
        }
    }
    
    // Get invulnerable flag
    bool isInvulnerable() const {
        return (flags & 1) != 0;
    }
    
    /**
     * Load data from USD prim
     * 
     * @param prim The USD prim to load from
     * @return Whether the load was successful
     */
    bool loadFromUsd(const UsdPrim& prim) {
        // Cache tokens for property access
        static const TfToken currentHealthToken("sparkle:health:current");
        static const TfToken maxHealthToken("sparkle:health:maximum");
        static const TfToken regenRateToken("sparkle:health:regenerationRate");
        static const TfToken invulnerableToken("sparkle:health:invulnerable");
        
        // Get attributes
        UsdAttribute currentHealthAttr = prim.GetAttribute(currentHealthToken);
        UsdAttribute maxHealthAttr = prim.GetAttribute(maxHealthToken);
        UsdAttribute regenRateAttr = prim.GetAttribute(regenRateToken);
        UsdAttribute invulnerableAttr = prim.GetAttribute(invulnerableToken);
        
        // Read values (with default fallbacks if attributes don't exist)
        if (currentHealthAttr) {
            currentHealthAttr.Get(&currentHealth);
        }
        
        if (maxHealthAttr) {
            maxHealthAttr.Get(&maxHealth);
        }
        
        if (regenRateAttr) {
            regenRateAttr.Get(&regenerationRate);
        }
        
        if (invulnerableAttr) {
            bool invulnerable = false;
            invulnerableAttr.Get(&invulnerable);
            setInvulnerable(invulnerable);
        }
        
        return true;
    }
    
    /**
     * Save data to USD prim
     * 
     * @param prim The USD prim to save to
     * @return Whether the save was successful
     */
    bool saveToUsd(const UsdPrim& prim) const {
        // Cache tokens for property access
        static const TfToken currentHealthToken("sparkle:health:current");
        static const TfToken maxHealthToken("sparkle:health:maximum");
        static const TfToken regenRateToken("sparkle:health:regenerationRate");
        static const TfToken invulnerableToken("sparkle:health:invulnerable");
        
        // Get or create attributes
        UsdAttribute currentHealthAttr = prim.GetAttribute(currentHealthToken);
        if (!currentHealthAttr) {
            currentHealthAttr = prim.CreateAttribute(currentHealthToken, SdfValueTypeNames->Float);
        }
        
        UsdAttribute maxHealthAttr = prim.GetAttribute(maxHealthToken);
        if (!maxHealthAttr) {
            maxHealthAttr = prim.CreateAttribute(maxHealthToken, SdfValueTypeNames->Float);
        }
        
        UsdAttribute regenRateAttr = prim.GetAttribute(regenRateToken);
        if (!regenRateAttr) {
            regenRateAttr = prim.CreateAttribute(regenRateToken, SdfValueTypeNames->Float);
        }
        
        UsdAttribute invulnerableAttr = prim.GetAttribute(invulnerableToken);
        if (!invulnerableAttr) {
            invulnerableAttr = prim.CreateAttribute(invulnerableToken, SdfValueTypeNames->Bool);
        }
        
        // Set values
        currentHealthAttr.Set(currentHealth);
        maxHealthAttr.Set(maxHealth);
        regenRateAttr.Set(regenerationRate);
        invulnerableAttr.Set(isInvulnerable());
        
        return true;
    }
    
    /**
     * Create a new optimized health data object from a USD prim
     * 
     * @param prim The USD prim to load from
     * @return Pointer to the new optimized data
     */
    static OptimizedHealthData* createFromUsd(const UsdPrim& prim) {
        // Allocate from schema-specific pool
        void* memory = SchemaPoolManager::getInstance().getPool("SparkleHealthAPI").allocate(
            sizeof(OptimizedHealthData));
        
        // Placement new for construction
        OptimizedHealthData* data = new(memory) OptimizedHealthData();
        data->loadFromUsd(prim);
        
        return data;
    }
};

/**
 * SIMD-friendly optimized layout for TransformComponent data
 * Aligned to 16 bytes for SSE or 32 bytes for AVX
 */
struct ALIGNED(32) OptimizedTransformData {
    // Position and rotation are frequently accessed together, so we keep them together
    // and aligned for SIMD operations
    GfVec4f position;  // Using Vec4f for alignment (w component is unused)
    GfVec4f rotation;  // Quaternion rotation
    
    // Scale is less frequently accessed, but still kept in same struct
    GfVec4f scale;     // Using Vec4f for alignment (w component is unused)
    
    // Matrix cache - computed when needed
    GfMatrix4f worldMatrix;
    
    // Flag to track if matrix is dirty and needs recomputation
    uint32_t flags;
    
    OptimizedTransformData()
        : position(0.0f, 0.0f, 0.0f, 1.0f)
        , rotation(0.0f, 0.0f, 0.0f, 1.0f)  // Identity quaternion
        , scale(1.0f, 1.0f, 1.0f, 1.0f)
        , flags(1)  // Matrix starts dirty
    {}
    
    /**
     * Load data from USD prim
     * 
     * @param prim The USD prim to load from
     * @return Whether the load was successful
     */
    bool loadFromUsd(const UsdPrim& prim) {
        // Use UsdGeomXformable for transform data if possible
        UsdGeomXformable xformable(prim);
        if (xformable) {
            // Get local transformation
            GfMatrix4d localTransform;
            bool resetsXformStack;
            xformable.GetLocalTransformation(&localTransform, &resetsXformStack);
            
            // Extract components from matrix
            GfVec3d pos, scl;
            GfRotation rot;
            GfMatrix4d scaleOrient;
            
            if (localTransform.DecomposeTransform(
                    pos, rot, scl, 
                    GfVec3d(0), GfRotation(), scaleOrient)) {
                
                // Convert to our optimized layout
                position = GfVec4f(
                    static_cast<float>(pos[0]), 
                    static_cast<float>(pos[1]),
                    static_cast<float>(pos[2]),
                    1.0f);
                
                GfQuatd quatd = rot.GetQuat();
                rotation = GfVec4f(
                    static_cast<float>(quatd.GetReal()),
                    static_cast<float>(quatd.GetImaginary()[0]),
                    static_cast<float>(quatd.GetImaginary()[1]),
                    static_cast<float>(quatd.GetImaginary()[2]));
                
                scale = GfVec4f(
                    static_cast<float>(scl[0]),
                    static_cast<float>(scl[1]),
                    static_cast<float>(scl[2]),
                    1.0f);
                
                // Mark matrix as dirty
                flags |= 1;
                
                return true;
            }
        }
        
        // Fallback for custom transform properties
        static const TfToken positionToken("sparkle:transform:position");
        static const TfToken rotationToken("sparkle:transform:rotation");
        static const TfToken scaleToken("sparkle:transform:scale");
        
        UsdAttribute posAttr = prim.GetAttribute(positionToken);
        UsdAttribute rotAttr = prim.GetAttribute(rotationToken);
        UsdAttribute scaleAttr = prim.GetAttribute(scaleToken);
        
        if (posAttr) {
            GfVec3f pos;
            if (posAttr.Get(&pos)) {
                position = GfVec4f(pos[0], pos[1], pos[2], 1.0f);
            }
        }
        
        if (rotAttr) {
            GfQuatf rot;
            if (rotAttr.Get(&rot)) {
                rotation = GfVec4f(
                    rot.GetReal(),
                    rot.GetImaginary()[0],
                    rot.GetImaginary()[1],
                    rot.GetImaginary()[2]);
            }
        }
        
        if (scaleAttr) {
            GfVec3f scl;
            if (scaleAttr.Get(&scl)) {
                scale = GfVec4f(scl[0], scl[1], scl[2], 1.0f);
            }
        }
        
        // Mark matrix as dirty
        flags |= 1;
        
        return posAttr || rotAttr || scaleAttr;
    }
    
    /**
     * Save data to USD prim
     * 
     * @param prim The USD prim to save to
     * @return Whether the save was successful
     */
    bool saveToUsd(const UsdPrim& prim) const {
        // Try to use UsdGeomXformable for transform data
        UsdGeomXformable xformable(prim);
        if (xformable) {
            // Convert to matrix for Xformable
            GfMatrix4d matrix = computeTransformMatrix();
            
            // Clear existing transform ops and set a new one
            xformable.ClearXformOpOrder();
            UsdGeomXformOp matrixOp = xformable.AddTransformOp();
            matrixOp.Set(matrix);
            
            return true;
        }
        
        // Fallback for custom transform properties
        static const TfToken positionToken("sparkle:transform:position");
        static const TfToken rotationToken("sparkle:transform:rotation");
        static const TfToken scaleToken("sparkle:transform:scale");
        
        // Get or create attributes
        UsdAttribute posAttr = prim.GetAttribute(positionToken);
        if (!posAttr) {
            posAttr = prim.CreateAttribute(positionToken, SdfValueTypeNames->Float3);
        }
        
        UsdAttribute rotAttr = prim.GetAttribute(rotationToken);
        if (!rotAttr) {
            rotAttr = prim.CreateAttribute(rotationToken, SdfValueTypeNames->Quatf);
        }
        
        UsdAttribute scaleAttr = prim.GetAttribute(scaleToken);
        if (!scaleAttr) {
            scaleAttr = prim.CreateAttribute(scaleToken, SdfValueTypeNames->Float3);
        }
        
        // Set values
        GfVec3f pos(position[0], position[1], position[2]);
        posAttr.Set(pos);
        
        GfQuatf rot(rotation[0], GfVec3f(rotation[1], rotation[2], rotation[3]));
        rotAttr.Set(rot);
        
        GfVec3f scl(scale[0], scale[1], scale[2]);
        scaleAttr.Set(scl);
        
        return true;
    }
    
    /**
     * Get the world transform matrix
     * Computes it only when needed based on dirty flag
     * 
     * @return The world transform matrix
     */
    const GfMatrix4f& getWorldMatrix() {
        if (flags & 1) {
            worldMatrix = computeTransformMatrix();
            flags &= ~1;  // Clear dirty flag
        }
        return worldMatrix;
    }
    
    /**
     * Mark the matrix as dirty, needing recomputation
     */
    void markDirty() {
        flags |= 1;
    }
    
    /**
     * Set the position
     * 
     * @param x X coordinate
     * @param y Y coordinate
     * @param z Z coordinate
     */
    void setPosition(float x, float y, float z) {
        position = GfVec4f(x, y, z, 1.0f);
        markDirty();
    }
    
    /**
     * Set the rotation quaternion
     * 
     * @param w Real component
     * @param x Imaginary X component
     * @param y Imaginary Y component
     * @param z Imaginary Z component
     */
    void setRotation(float w, float x, float y, float z) {
        rotation = GfVec4f(w, x, y, z);
        markDirty();
    }
    
    /**
     * Set the scale
     * 
     * @param x X scale
     * @param y Y scale
     * @param z Z scale
     */
    void setScale(float x, float y, float z) {
        scale = GfVec4f(x, y, z, 1.0f);
        markDirty();
    }
    
    /**
     * Create a new optimized transform data object from a USD prim
     * 
     * @param prim The USD prim to load from
     * @return Pointer to the new optimized data
     */
    static OptimizedTransformData* createFromUsd(const UsdPrim& prim) {
        // Allocate from schema-specific pool
        void* memory = SchemaPoolManager::getInstance().getPool("TransformData").allocate(
            sizeof(OptimizedTransformData));
        
        // Placement new for construction
        OptimizedTransformData* data = new(memory) OptimizedTransformData();
        data->loadFromUsd(prim);
        
        return data;
    }

private:
    /**
     * Compute the transform matrix from components
     * 
     * @return The computed matrix
     */
    GfMatrix4d computeTransformMatrix() const {
        // Convert position to translation matrix
        GfMatrix4d translationMatrix;
        translationMatrix.SetTranslate(GfVec3d(position[0], position[1], position[2]));
        
        // Convert quaternion to rotation matrix
        GfQuatd rotQuat(rotation[0], GfVec3d(rotation[1], rotation[2], rotation[3]));
        GfMatrix4d rotationMatrix;
        rotationMatrix.SetRotate(rotQuat);
        
        // Convert scale to scale matrix
        GfMatrix4d scaleMatrix;
        scaleMatrix.SetScale(GfVec3d(scale[0], scale[1], scale[2]));
        
        // Combine matrices: Translation * Rotation * Scale
        return translationMatrix * rotationMatrix * scaleMatrix;
    }
};

/**
 * Optimized animation data with SoA layout for SIMD processing
 */
class OptimizedAnimationData {
public:
    OptimizedAnimationData(size_t initialCapacity = 32)
        : m_dirty(true)
    {
        // Pre-allocate memory for animation channels
        m_timePoints.reserve(initialCapacity);
        m_positions.reserve(initialCapacity);
        m_rotations.reserve(initialCapacity);
        m_scales.reserve(initialCapacity);
    }
    
    /**
     * Load data from USD prim
     * 
     * @param prim The USD prim to load from
     * @return Whether the load was successful
     */
    bool loadFromUsd(const UsdPrim& prim) {
        // Try to find animation data on the prim
        static const TfToken timePointsToken("sparkle:animation:timePoints");
        static const TfToken positionsToken("sparkle:animation:positions");
        static const TfToken rotationsToken("sparkle:animation:rotations");
        static const TfToken scalesToken("sparkle:animation:scales");
        
        UsdAttribute timePointsAttr = prim.GetAttribute(timePointsToken);
        UsdAttribute positionsAttr = prim.GetAttribute(positionsToken);
        UsdAttribute rotationsAttr = prim.GetAttribute(rotationsToken);
        UsdAttribute scalesAttr = prim.GetAttribute(scalesToken);
        
        if (!timePointsAttr || !positionsAttr) {
            return false;  // Minimum required data not found
        }
        
        // Get time points
        VtArray<float> timePoints;
        if (!timePointsAttr.Get(&timePoints)) {
            return false;
        }
        
        // Get position data
        VtArray<GfVec3f> positions;
        if (!positionsAttr.Get(&positions) || positions.size() != timePoints.size()) {
            return false;
        }
        
        // Get rotation data (optional)
        VtArray<GfQuatf> rotations;
        if (rotationsAttr) {
            rotationsAttr.Get(&rotations);
        }
        
        // Get scale data (optional)
        VtArray<GfVec3f> scales;
        if (scalesAttr) {
            scalesAttr.Get(&scales);
        }
        
        // Clear existing data
        m_timePoints.clear();
        m_positions.clear();
        m_rotations.clear();
        m_scales.clear();
        
        // Convert to SoA layout
        const size_t keyCount = timePoints.size();
        m_timePoints.resize(keyCount);
        m_positions.resize(keyCount);
        m_rotations.resize(keyCount);
        m_scales.resize(keyCount);
        
        for (size_t i = 0; i < keyCount; ++i) {
            // Time point
            m_timePoints[i] = timePoints[i];
            
            // Position
            GfVec3f pos = positions[i];
            m_positions[i] = GfVec4f(pos[0], pos[1], pos[2], 1.0f);
            
            // Rotation (with default if not provided)
            if (i < rotations.size()) {
                GfQuatf rot = rotations[i];
                m_rotations[i] = GfVec4f(
                    rot.GetReal(), 
                    rot.GetImaginary()[0],
                    rot.GetImaginary()[1],
                    rot.GetImaginary()[2]);
            } else {
                m_rotations[i] = GfVec4f(1.0f, 0.0f, 0.0f, 0.0f);  // Identity quaternion
            }
            
            // Scale (with default if not provided)
            if (i < scales.size()) {
                GfVec3f scl = scales[i];
                m_scales[i] = GfVec4f(scl[0], scl[1], scl[2], 1.0f);
            } else {
                m_scales[i] = GfVec4f(1.0f, 1.0f, 1.0f, 1.0f);  // Unity scale
            }
        }
        
        m_dirty = true;
        buildIndexCache();
        
        return true;
    }
    
    /**
     * Save data to USD prim
     * 
     * @param prim The USD prim to save to
     * @return Whether the save was successful
     */
    bool saveToUsd(const UsdPrim& prim) const {
        if (m_timePoints.empty()) {
            return false;  // No data to save
        }
        
        static const TfToken timePointsToken("sparkle:animation:timePoints");
        static const TfToken positionsToken("sparkle:animation:positions");
        static const TfToken rotationsToken("sparkle:animation:rotations");
        static const TfToken scalesToken("sparkle:animation:scales");
        
        // Get or create attributes
        UsdAttribute timePointsAttr = prim.GetAttribute(timePointsToken);
        if (!timePointsAttr) {
            timePointsAttr = prim.CreateAttribute(timePointsToken, SdfValueTypeNames->FloatArray);
        }
        
        UsdAttribute positionsAttr = prim.GetAttribute(positionsToken);
        if (!positionsAttr) {
            positionsAttr = prim.CreateAttribute(positionsToken, SdfValueTypeNames->Float3Array);
        }
        
        UsdAttribute rotationsAttr = prim.GetAttribute(rotationsToken);
        if (!rotationsAttr) {
            rotationsAttr = prim.CreateAttribute(rotationsToken, SdfValueTypeNames->QuatfArray);
        }
        
        UsdAttribute scalesAttr = prim.GetAttribute(scalesToken);
        if (!scalesAttr) {
            scalesAttr = prim.CreateAttribute(scalesToken, SdfValueTypeNames->Float3Array);
        }
        
        // Convert back to USD format
        const size_t keyCount = m_timePoints.size();
        
        VtArray<float> timePoints(keyCount);
        VtArray<GfVec3f> positions(keyCount);
        VtArray<GfQuatf> rotations(keyCount);
        VtArray<GfVec3f> scales(keyCount);
        
        for (size_t i = 0; i < keyCount; ++i) {
            timePoints[i] = m_timePoints[i];
            
            positions[i] = GfVec3f(
                m_positions[i][0], 
                m_positions[i][1], 
                m_positions[i][2]);
            
            rotations[i] = GfQuatf(
                m_rotations[i][0],
                GfVec3f(
                    m_rotations[i][1],
                    m_rotations[i][2],
                    m_rotations[i][3]));
                    
            scales[i] = GfVec3f(
                m_scales[i][0],
                m_scales[i][1],
                m_scales[i][2]);
        }
        
        // Set values
        timePointsAttr.Set(timePoints);
        positionsAttr.Set(positions);
        rotationsAttr.Set(rotations);
        scalesAttr.Set(scales);
        
        return true;
    }
    
    /**
     * Get interpolated transform at a specific time
     * 
     * @param time The time to evaluate at
     * @param position Output position
     * @param rotation Output rotation
     * @param scale Output scale
     */
    void evaluate(float time, GfVec4f& position, GfVec4f& rotation, GfVec4f& scale) const {
        if (m_timePoints.empty()) {
            // Default transform if no animation data
            position = GfVec4f(0.0f, 0.0f, 0.0f, 1.0f);
            rotation = GfVec4f(1.0f, 0.0f, 0.0f, 0.0f);  // Identity quaternion
            scale = GfVec4f(1.0f, 1.0f, 1.0f, 1.0f);
            return;
        }
        
        if (m_timePoints.size() == 1) {
            // Only one keyframe, no interpolation needed
            position = m_positions[0];
            rotation = m_rotations[0];
            scale = m_scales[0];
            return;
        }
        
        // Find keyframes to interpolate between
        int index = findTimeIndex(time);
        
        // Clamp to available range
        if (index < 0) {
            position = m_positions[0];
            rotation = m_rotations[0];
            scale = m_scales[0];
            return;
        }
        
        if (index >= static_cast<int>(m_timePoints.size() - 1)) {
            position = m_positions.back();
            rotation = m_rotations.back();
            scale = m_scales.back();
            return;
        }
        
        // Calculate interpolation factor
        float t1 = m_timePoints[index];
        float t2 = m_timePoints[index + 1];
        float factor = (time - t1) / (t2 - t1);
        
        // Interpolate position (linear)
        position = lerp(m_positions[index], m_positions[index + 1], factor);
        
        // Interpolate rotation (spherical linear)
        rotation = slerp(m_rotations[index], m_rotations[index + 1], factor);
        
        // Interpolate scale (linear)
        scale = lerp(m_scales[index], m_scales[index + 1], factor);
    }
    
    /**
     * Add a keyframe to the animation
     * 
     * @param time Time point
     * @param position Position at time
     * @param rotation Rotation at time
     * @param scale Scale at time
     */
    void addKeyframe(float time, const GfVec4f& position, 
                    const GfVec4f& rotation, const GfVec4f& scale) {
        // Find insertion point to maintain sorted order
        auto it = std::lower_bound(m_timePoints.begin(), m_timePoints.end(), time);
        size_t index = it - m_timePoints.begin();
        
        // Check if this time already exists
        if (it != m_timePoints.end() && *it == time) {
            // Update existing keyframe
            m_positions[index] = position;
            m_rotations[index] = rotation;
            m_scales[index] = scale;
        } else {
            // Insert new keyframe
            m_timePoints.insert(it, time);
            m_positions.insert(m_positions.begin() + index, position);
            m_rotations.insert(m_rotations.begin() + index, rotation);
            m_scales.insert(m_scales.begin() + index, scale);
        }
        
        m_dirty = true;
    }
    
    /**
     * Remove a keyframe at the specified time
     * 
     * @param time Time point to remove
     * @return Whether a keyframe was removed
     */
    bool removeKeyframe(float time) {
        auto it = std::find(m_timePoints.begin(), m_timePoints.end(), time);
        if (it == m_timePoints.end()) {
            return false;
        }
        
        size_t index = it - m_timePoints.begin();
        
        m_timePoints.erase(m_timePoints.begin() + index);
        m_positions.erase(m_positions.begin() + index);
        m_rotations.erase(m_rotations.begin() + index);
        m_scales.erase(m_scales.begin() + index);
        
        m_dirty = true;
        return true;
    }
    
    /**
     * Create a new optimized animation data object from a USD prim
     * 
     * @param prim The USD prim to load from
     * @return Pointer to the new optimized data
     */
    static OptimizedAnimationData* createFromUsd(const UsdPrim& prim) {
        // Allocate from schema-specific pool
        void* memory = SchemaPoolManager::getInstance().getPool("AnimationData").allocate(
            sizeof(OptimizedAnimationData));
        
        // Placement new for construction
        OptimizedAnimationData* data = new(memory) OptimizedAnimationData();
        data->loadFromUsd(prim);
        
        return data;
    }

private:
    std::vector<float> m_timePoints;
    std::vector<GfVec4f> m_positions;
    std::vector<GfVec4f> m_rotations;
    std::vector<GfVec4f> m_scales;
    
    // Cached index data for faster lookup
    std::vector<std::pair<float, int>> m_indexCache;
    bool m_dirty;
    
    /**
     * Build the index cache for faster time lookups
     */
    void buildIndexCache() {
        if (!m_dirty) {
            return;
        }
        
        const size_t keyCount = m_timePoints.size();
        
        // Build time-to-index cache for faster lookup
        m_indexCache.resize(keyCount);
        for (size_t i = 0; i < keyCount; ++i) {
            m_indexCache[i] = std::make_pair(m_timePoints[i], static_cast<int>(i));
        }
        
        // Sort by time (should already be sorted, but just in case)
        std::sort(m_indexCache.begin(), m_indexCache.end());
        
        m_dirty = false;
    }
    
    /**
     * Find the index for the keyframe just before the given time
     * 
     * @param time Time to find
     * @return Index of the keyframe, or -1 if before first keyframe
     */
    int findTimeIndex(float time) const {
        if (m_dirty) {
            const_cast<OptimizedAnimationData*>(this)->buildIndexCache();
        }
        
        // Binary search in the index cache
        auto it = std::lower_bound(
            m_indexCache.begin(), 
            m_indexCache.end(), 
            std::make_pair(time, 0),
            [](const std::pair<float, int>& a, const std::pair<float, int>& b) {
                return a.first < b.first;
            });
        
        if (it == m_indexCache.begin()) {
            return -1;  // Before first keyframe
        }
        
        // Return the index right before the target time
        return (--it)->second;
    }
    
    /**
     * Linear interpolation for vectors
     */
    static GfVec4f lerp(const GfVec4f& a, const GfVec4f& b, float t) {
        return GfVec4f(
            a[0] + (b[0] - a[0]) * t,
            a[1] + (b[1] - a[1]) * t,
            a[2] + (b[2] - a[2]) * t,
            a[3] + (b[3] - a[3]) * t);
    }
    
    /**
     * Spherical linear interpolation for quaternions
     */
    static GfVec4f slerp(const GfVec4f& a, const GfVec4f& b, float t) {
        // Extract quaternion components
        float aw = a[0], ax = a[1], ay = a[2], az = a[3];
        float bw = b[0], bx = b[1], by = b[2], bz = b[3];
        
        // Calculate cosine of angle between quaternions
        float cosTheta = aw * bw + ax * bx + ay * by + az * bz;
        
        // If cosTheta < 0, negate one quaternion to take shorter path
        if (cosTheta < 0.0f) {
            bw = -bw;
            bx = -bx;
            by = -by;
            bz = -bz;
            cosTheta = -cosTheta;
        }
        
        // Use linear interpolation for very close quaternions
        float epsilon = 0.001f;
        if (cosTheta > 1.0f - epsilon) {
            return lerp(a, b, t);
        }
        
        // Calculate interpolation parameters
        float theta = acosf(cosTheta);
        float sinTheta = sinf(theta);
        float ratioA = sinf((1.0f - t) * theta) / sinTheta;
        float ratioB = sinf(t * theta) / sinTheta;
        
        // Calculate interpolated quaternion
        return GfVec4f(
            aw * ratioA + bw * ratioB,
            ax * ratioA + bx * ratioB,
            ay * ratioA + by * ratioB,
            az * ratioA + bz * ratioB);
    }
};

/**
 * SIMD-friendly physics data with aligned memory for specific CPU instructions
 */
struct ALIGNED(CACHE_LINE_SIZE) OptimizedPhysicsData {
    // Dynamic properties (frequently changing)
    GfVec4f linearVelocity;     // XYZ + padding
    GfVec4f angularVelocity;    // XYZ + padding
    GfVec4f forces;             // XYZ + padding
    GfVec4f torques;            // XYZ + padding
    
    // Static properties (rarely changing)
    float mass;                 // Mass in kg
    float inverseMass;          // Cached 1/mass for faster calculations
    GfVec4f localInertia;       // Inertia tensor diagonal + padding
    GfVec4f inverseInertia;     // Cached inverse inertia tensor diagonal + padding
    
    // Collision properties
    float restitution;          // Bounce factor
    float friction;             // Surface friction
    uint32_t collisionGroup;    // Collision group membership
    uint32_t collisionMask;     // Which groups to collide with
    
    // Flags for physics behavior
    uint32_t flags;
    
    // Constants for flags
    enum PhysicsFlags {
        DYNAMIC       = 1 << 0,   // Object is dynamic (affected by forces)
        KINEMATIC     = 1 << 1,   // Object is kinematic (moved by animation)
        GRAVITY       = 1 << 2,   // Object is affected by gravity
        SLEEPING      = 1 << 3,   // Object is sleeping (optimization)
        TRIGGER       = 1 << 4,   // Object is a trigger volume
        NO_ROTATION   = 1 << 5    // Object can translate but not rotate
    };
    
    OptimizedPhysicsData()
        : linearVelocity(0.0f, 0.0f, 0.0f, 0.0f)
        , angularVelocity(0.0f, 0.0f, 0.0f, 0.0f)
        , forces(0.0f, 0.0f, 0.0f, 0.0f)
        , torques(0.0f, 0.0f, 0.0f, 0.0f)
        , mass(1.0f)
        , inverseMass(1.0f)
        , localInertia(1.0f, 1.0f, 1.0f, 0.0f)
        , inverseInertia(1.0f, 1.0f, 1.0f, 0.0f)
        , restitution(0.5f)
        , friction(0.5f)
        , collisionGroup(1)
        , collisionMask(0xFFFFFFFF)
        , flags(DYNAMIC | GRAVITY)
    {}
    
    /**
     * Load data from USD prim
     * 
     * @param prim The USD prim to load from
     * @return Whether the load was successful
     */
    bool loadFromUsd(const UsdPrim& prim) {
        // Cache tokens for property access
        static const TfToken massToken("sparkle:physics:mass");
        static const TfToken restitutionToken("sparkle:physics:restitution");
        static const TfToken frictionToken("sparkle:physics:friction");
        static const TfToken dynamicToken("sparkle:physics:dynamic");
        static const TfToken gravityToken("sparkle:physics:useGravity");
        static const TfToken triggerToken("sparkle:physics:isTrigger");
        static const TfToken groupToken("sparkle:physics:collisionGroup");
        static const TfToken maskToken("sparkle:physics:collisionMask");
        
        // Get attributes with fallbacks
        UsdAttribute massAttr = prim.GetAttribute(massToken);
        if (massAttr) {
            massAttr.Get(&mass);
            inverseMass = mass > 0.001f ? 1.0f / mass : 0.0f;
        }
        
        UsdAttribute restitutionAttr = prim.GetAttribute(restitutionToken);
        if (restitutionAttr) {
            restitutionAttr.Get(&restitution);
        }
        
        UsdAttribute frictionAttr = prim.GetAttribute(frictionToken);
        if (frictionAttr) {
            frictionAttr.Get(&friction);
        }
        
        // Set flags based on attributes
        bool isDynamic = true;
        UsdAttribute dynamicAttr = prim.GetAttribute(dynamicToken);
        if (dynamicAttr) {
            dynamicAttr.Get(&isDynamic);
        }
        
        bool useGravity = true;
        UsdAttribute gravityAttr = prim.GetAttribute(gravityToken);
        if (gravityAttr) {
            gravityAttr.Get(&useGravity);
        }
        
        bool isTrigger = false;
        UsdAttribute triggerAttr = prim.GetAttribute(triggerToken);
        if (triggerAttr) {
            triggerAttr.Get(&isTrigger);
        }
        
        // Set flags
        flags = 0;
        if (isDynamic) flags |= DYNAMIC;
        if (useGravity) flags |= GRAVITY;
        if (isTrigger) flags |= TRIGGER;
        
        // Get collision groups
        UsdAttribute groupAttr = prim.GetAttribute(groupToken);
        if (groupAttr) {
            groupAttr.Get(&collisionGroup);
        }
        
        UsdAttribute maskAttr = prim.GetAttribute(maskToken);
        if (maskAttr) {
            maskAttr.Get(&collisionMask);
        }
        
        // Calculate inertia tensor for a box (simplification)
        // A more robust implementation would check for a physics schema with inertia tensor
        localInertia = GfVec4f(
            mass / 6.0f, // For a 1x1x1 box
            mass / 6.0f,
            mass / 6.0f,
            0.0f);
            
        // Calculate inverse inertia
        for (int i = 0; i < 3; ++i) {
            inverseInertia[i] = localInertia[i] > 0.001f ? 1.0f / localInertia[i] : 0.0f;
        }
        
        return massAttr || restitutionAttr || frictionAttr;
    }
    
    /**
     * Save data to USD prim
     * 
     * @param prim The USD prim to save to
     * @return Whether the save was successful
     */
    bool saveToUsd(const UsdPrim& prim) const {
        // Cache tokens for property access
        static const TfToken massToken("sparkle:physics:mass");
        static const TfToken restitutionToken("sparkle:physics:restitution");
        static const TfToken frictionToken("sparkle:physics:friction");
        static const TfToken dynamicToken("sparkle:physics:dynamic");
        static const TfToken gravityToken("sparkle:physics:useGravity");
        static const TfToken triggerToken("sparkle:physics:isTrigger");
        static const TfToken groupToken("sparkle:physics:collisionGroup");
        static const TfToken maskToken("sparkle:physics:collisionMask");
        static const TfToken velocityToken("sparkle:physics:linearVelocity");
        static const TfToken angVelocityToken("sparkle:physics:angularVelocity");
        
        // Get or create attributes
        UsdAttribute massAttr = prim.GetAttribute(massToken);
        if (!massAttr) {
            massAttr = prim.CreateAttribute(massToken, SdfValueTypeNames->Float);
        }
        
        UsdAttribute restitutionAttr = prim.GetAttribute(restitutionToken);
        if (!restitutionAttr) {
            restitutionAttr = prim.CreateAttribute(restitutionToken, SdfValueTypeNames->Float);
        }
        
        UsdAttribute frictionAttr = prim.GetAttribute(frictionToken);
        if (!frictionAttr) {
            frictionAttr = prim.CreateAttribute(frictionToken, SdfValueTypeNames->Float);
        }
        
        UsdAttribute dynamicAttr = prim.GetAttribute(dynamicToken);
        if (!dynamicAttr) {
            dynamicAttr = prim.CreateAttribute(dynamicToken, SdfValueTypeNames->Bool);
        }
        
        UsdAttribute gravityAttr = prim.GetAttribute(gravityToken);
        if (!gravityAttr) {
            gravityAttr = prim.CreateAttribute(gravityToken, SdfValueTypeNames->Bool);
        }
        
        UsdAttribute triggerAttr = prim.GetAttribute(triggerToken);
        if (!triggerAttr) {
            triggerAttr = prim.CreateAttribute(triggerToken, SdfValueTypeNames->Bool);
        }
        
        UsdAttribute groupAttr = prim.GetAttribute(groupToken);
        if (!groupAttr) {
            groupAttr = prim.CreateAttribute(groupToken, SdfValueTypeNames->UInt);
        }
        
        UsdAttribute maskAttr = prim.GetAttribute(maskToken);
        if (!maskAttr) {
            maskAttr = prim.CreateAttribute(maskToken, SdfValueTypeNames->UInt);
        }
        
        UsdAttribute velocityAttr = prim.GetAttribute(velocityToken);
        if (!velocityAttr) {
            velocityAttr = prim.CreateAttribute(velocityToken, SdfValueTypeNames->Float3);
        }
        
        UsdAttribute angVelocityAttr = prim.GetAttribute(angVelocityToken);
        if (!angVelocityAttr) {
            angVelocityAttr = prim.CreateAttribute(angVelocityToken, SdfValueTypeNames->Float3);
        }
        
        // Set values
        massAttr.Set(mass);
        restitutionAttr.Set(restitution);
        frictionAttr.Set(friction);
        dynamicAttr.Set((flags & DYNAMIC) != 0);
        gravityAttr.Set((flags & GRAVITY) != 0);
        triggerAttr.Set((flags & TRIGGER) != 0);
        groupAttr.Set(collisionGroup);
        maskAttr.Set(collisionMask);
        
        // Save current dynamic state
        GfVec3f linearVel(linearVelocity[0], linearVelocity[1], linearVelocity[2]);
        velocityAttr.Set(linearVel);
        
        GfVec3f angularVel(angularVelocity[0], angularVelocity[1], angularVelocity[2]);
        angVelocityAttr.Set(angularVel);
        
        return true;
    }
    
    /**
     * Apply a force to the center of mass
     * 
     * @param force Force vector
     */
    void applyForce(const GfVec3f& force) {
        if (!(flags & DYNAMIC)) return;
        
        forces[0] += force[0];
        forces[1] += force[1];
        forces[2] += force[2];
    }
    
    /**
     * Apply a torque
     * 
     * @param torque Torque vector
     */
    void applyTorque(const GfVec3f& torque) {
        if (!(flags & DYNAMIC) || (flags & NO_ROTATION)) return;
        
        torques[0] += torque[0];
        torques[1] += torque[1];
        torques[2] += torque[2];
    }
    
    /**
     * Integrate physics for a time step
     * 
     * @param deltaTime Time step in seconds
     * @param gravity Global gravity vector
     * @param position Current position (will be updated)
     * @param rotation Current rotation (will be updated)
     */
    void integrate(float deltaTime, const GfVec3f& gravity, 
                  GfVec4f& position, GfVec4f& rotation) {
        if (!(flags & DYNAMIC) || (flags & SLEEPING)) return;
        
        // Apply gravity if enabled
        if (flags & GRAVITY) {
            forces[0] += gravity[0] * mass;
            forces[1] += gravity[1] * mass;
            forces[2] += gravity[2] * mass;
        }
        
        // Update linear velocity
        linearVelocity[0] += forces[0] * inverseMass * deltaTime;
        linearVelocity[1] += forces[1] * inverseMass * deltaTime;
        linearVelocity[2] += forces[2] * inverseMass * deltaTime;
        
        // Update position
        position[0] += linearVelocity[0] * deltaTime;
        position[1] += linearVelocity[1] * deltaTime;
        position[2] += linearVelocity[2] * deltaTime;
        
        // Update angular velocity if rotation is enabled
        if (!(flags & NO_ROTATION)) {
            angularVelocity[0] += torques[0] * inverseInertia[0] * deltaTime;
            angularVelocity[1] += torques[1] * inverseInertia[1] * deltaTime;
            angularVelocity[2] += torques[2] * inverseInertia[2] * deltaTime;
            
            // Update rotation (simplified quaternion integration)
            // A more robust implementation would use quaternion differential equations
            GfVec4f deltaRotation(
                0.0f,
                angularVelocity[0] * deltaTime * 0.5f,
                angularVelocity[1] * deltaTime * 0.5f,
                angularVelocity[2] * deltaTime * 0.5f);
            
            // Quaternion multiplication (simplified)
            float qw = rotation[0];
            float qx = rotation[1];
            float qy = rotation[2];
            float qz = rotation[3];
            
            rotation[0] = qw - deltaRotation[1] * qx - deltaRotation[2] * qy - deltaRotation[3] * qz;
            rotation[1] = qx + deltaRotation[1] * qw + deltaRotation[2] * qz - deltaRotation[3] * qy;
            rotation[2] = qy - deltaRotation[1] * qz + deltaRotation[2] * qw + deltaRotation[3] * qx;
            rotation[3] = qz + deltaRotation[1] * qy - deltaRotation[2] * qx + deltaRotation[3] * qw;
            
            // Normalize quaternion
            float lengthSq = rotation[0]*rotation[0] + rotation[1]*rotation[1] + 
                             rotation[2]*rotation[2] + rotation[3]*rotation[3];
            if (lengthSq > 0.0f) {
                float invLength = 1.0f / sqrtf(lengthSq);
                rotation[0] *= invLength;
                rotation[1] *= invLength;
                rotation[2] *= invLength;
                rotation[3] *= invLength;
            }
        }
        
        // Reset forces and torques
        forces = GfVec4f(0.0f, 0.0f, 0.0f, 0.0f);
        torques = GfVec4f(0.0f, 0.0f, 0.0f, 0.0f);
    }
    
    /**
     * Create a new optimized physics data object from a USD prim
     * 
     * @param prim The USD prim to load from
     * @return Pointer to the new optimized data
     */
    static OptimizedPhysicsData* createFromUsd(const UsdPrim& prim) {
        // Allocate from schema-specific pool
        void* memory = SchemaPoolManager::getInstance().getPool("PhysicsData").allocate(
            sizeof(OptimizedPhysicsData));
        
        // Placement new for construction
        OptimizedPhysicsData* data = new(memory) OptimizedPhysicsData();
        data->loadFromUsd(prim);
        
        return data;
    }
};

/**
 * EntityOptimizedData
 * 
 * Container for all optimized schema data for an entity
 */
class EntityOptimizedData {
public:
    EntityOptimizedData(const UsdPrim& prim)
        : m_prim(prim)
        , m_healthData(nullptr)
        , m_transformData(nullptr)
        , m_animationData(nullptr)
        , m_physicsData(nullptr)
    {
        // Detect and create optimized data based on schema types
        if (hasSchema(prim, "SparkleHealthAPI")) {
            m_healthData = OptimizedHealthData::createFromUsd(prim);
        }
        
        // All entities should have transform data
        m_transformData = OptimizedTransformData::createFromUsd(prim);
        
        // Check for animation data
        static const TfToken timePointsToken("sparkle:animation:timePoints");
        if (prim.HasAttribute(timePointsToken)) {
            m_animationData = OptimizedAnimationData::createFromUsd(prim);
        }
        
        // Check for physics data
        if (hasSchema(prim, "SparklePhysicsAPI")) {
            m_physicsData = OptimizedPhysicsData::createFromUsd(prim);
        }
    }
    
    ~EntityOptimizedData() {
        // Memory is managed by the schema-specific pools,
        // so we don't need to delete anything here
    }
    
    // Accessors for optimized data
    OptimizedHealthData* getHealthData() const { return m_healthData; }
    OptimizedTransformData* getTransformData() const { return m_transformData; }
    OptimizedAnimationData* getAnimationData() const { return m_animationData; }
    OptimizedPhysicsData* getPhysicsData() const { return m_physicsData; }
    
    /**
     * Sync all data back to USD
     * 
     * @return Whether all syncs were successful
     */
    bool syncToUsd() {
        bool success = true;
        
        if (m_healthData) {
            success &= m_healthData->saveToUsd(m_prim);
        }
        
        if (m_transformData) {
            success &= m_transformData->saveToUsd(m_prim);
        }
        
        if (m_animationData) {
            success &= m_animationData->saveToUsd(m_prim);
        }
        
        if (m_physicsData) {
            success &= m_physicsData->saveToUsd(m_prim);
        }
        
        return success;
    }
    
    /**
     * Update entity for a single frame
     * 
     * @param deltaTime Time since last update in seconds
     * @param time Current game time
     */
    void update(float deltaTime, float time) {
        // Apply animation if available
        if (m_animationData && m_transformData) {
            GfVec4f position, rotation, scale;
            m_animationData->evaluate(time, position, rotation, scale);
            
            // Update transform with animated values
            m_transformData->setPosition(position[0], position[1], position[2]);
            m_transformData->setRotation(rotation[0], rotation[1], rotation[2], rotation[3]);
            m_transformData->setScale(scale[0], scale[1], scale[2]);
        }
        
        // Apply physics if available
        if (m_physicsData && m_transformData) {
            // Get current transform
            GfVec4f position = m_transformData->position;
            GfVec4f rotation = m_transformData->rotation;
            
            // Integrate physics
            GfVec3f gravity(0.0f, -9.81f, 0.0f);  // Standard gravity
            m_physicsData->integrate(deltaTime, gravity, position, rotation);
            
            // Update transform with physics results
            m_transformData->setPosition(position[0], position[1], position[2]);
            m_transformData->setRotation(rotation[0], rotation[1], rotation[2], rotation[3]);
        }
        
        // Apply health regeneration if available
        if (m_healthData) {
            float currentHealth = m_healthData->currentHealth;
            float maxHealth = m_healthData->maxHealth;
            float regenRate = m_healthData->regenerationRate;
            
            if (regenRate > 0.0f && currentHealth < maxHealth) {
                currentHealth += regenRate * deltaTime;
                if (currentHealth > maxHealth) {
                    currentHealth = maxHealth;
                }
                m_healthData->currentHealth = currentHealth;
            }
        }
    }

private:
    UsdPrim m_prim;
    OptimizedHealthData* m_healthData;
    OptimizedTransformData* m_transformData;
    OptimizedAnimationData* m_animationData;
    OptimizedPhysicsData* m_physicsData;
    
    /**
     * Check if a prim has a specific schema applied
     */
    bool hasSchema(const UsdPrim& prim, const std::string& schemaName) {
        std::vector<std::string> schemas;
        prim.GetAppliedSchemas(&schemas);
        
        return std::find(schemas.begin(), schemas.end(), schemaName) != schemas.end();
    }
};

/**
 * Struct-of-Arrays layout for batch processing similar entities
 */
template<size_t MaxEntities = 1024>
class EntityBatchProcessor {
public:
    EntityBatchProcessor() : m_entityCount(0) {}
    
    /**
     * Add an entity to the batch
     * 
     * @param entity Entity data to add
     * @return Whether the entity was added
     */
    bool addEntity(EntityOptimizedData* entity) {
        if (m_entityCount >= MaxEntities || !entity) {
            return false;
        }
        
        // Store entity pointer
        m_entities[m_entityCount] = entity;
        
        // Extract transform data for SIMD processing
        OptimizedTransformData* transform = entity->getTransformData();
        if (transform) {
            m_positions[m_entityCount] = transform->position;
            m_rotations[m_entityCount] = transform->rotation;
            m_scales[m_entityCount] = transform->scale;
        }
        
        // Extract health data if available
        OptimizedHealthData* health = entity->getHealthData();
        if (health) {
            m_healthValues[m_entityCount] = health->currentHealth;
            m_maxHealthValues[m_entityCount] = health->maxHealth;
            m_regenRates[m_entityCount] = health->regenerationRate;
        } else {
            m_healthValues[m_entityCount] = 0.0f;
            m_maxHealthValues[m_entityCount] = 0.0f;
            m_regenRates[m_entityCount] = 0.0f;
        }
        
        m_entityCount++;
        return true;
    }
    
    /**
     * Batch update all entities
     * 
     * @param deltaTime Time since last update
     */
    void updateAll(float deltaTime) {
        // Example: Update all health values in a SIMD-friendly way
        batchUpdateHealth(deltaTime);
        
        // Other batch updates could go here
        
        // Sync updated values back to entity objects
        syncBackToEntities();
    }

private:
/**
 * schema_memory_layout.h
 * 
 * Memory layout optimization techniques for USD schemas in game engines.
 * This implementation demonstrates:
 * - Schema-specific memory pools
 * - Optimized memory layouts for common game schemas
 * - Transformations between USD and optimized layouts
 * - Cache-aligned property structures
 * - SIMD-friendly data structures
 */

#pragma once

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/gf/matrix4f.h>

#include <vector>
#include <memory>
#include <unordered_map>
#include <array>
#include <algorithm>
#include <cassert>
#include <cstring>

#ifdef _MSC_VER
#define ALIGNED(x) __declspec(align(x))
#else
#define ALIGNED(x) __attribute__((aligned(x)))
#endif

// Choose cache line size based on target architecture
// 64 bytes is common for modern CPUs
#define CACHE_LINE_SIZE 64

PXR_NAMESPACE_USING_DIRECTIVE

namespace SchemaMemoryLayout {

/**
 * Memory Pool for schema data
 * 
 * Manages contiguous chunks of memory for schema data structures
 * with proper alignment for cache efficiency.
 */
class MemoryPool {
public:
    MemoryPool(size_t blockSize = 16384, size_t alignment = CACHE_LINE_SIZE)
        : m_blockSize(blockSize)
        , m_alignment(alignment)
        , m_currentBlock(nullptr)
        , m_currentPos(0)
        , m_currentBlockSize(0)
    {
        // Allocate first block
        allocateBlock();
    }
    
    ~MemoryPool() {
        // Free all blocks
        for (void* block : m_blocks) {
            // Alignment-aware free
            #ifdef _MSC_VER
            _aligned_free(block);
            #else
            free(block);
            #endif
        }
    }
    
    /**
     * Allocate memory from the pool with proper alignment
     * 
     * @param size Size in bytes to allocate
     * @return Pointer to allocated memory
     */
    void* allocate(size_t size) {
        // Align size to ensure subsequent allocations remain aligned
        size_t alignedSize = ((size + m_alignment - 1) / m_alignment) * m_alignment;
        
        // Check if we need a new block
        if (m_currentPos + alignedSize > m_currentBlockSize) {
            // If requested size is larger than our standard block size,
            // allocate a custom-sized block for it
            if (alignedSize > m_blockSize) {
                void* specialBlock;
                #ifdef _MSC_VER
                specialBlock = _aligned_malloc(alignedSize, m_alignment);
                #else
                posix_memalign(&specialBlock, m_alignment, alignedSize);
                #endif
                
                m_blocks.push_back(specialBlock);
                return specialBlock;
            }
            
            // Otherwise allocate a new standard block
            allocateBlock();
        }
        
        void* ptr = static_cast<char*>(m_currentBlock) + m_currentPos;
        m_currentPos += alignedSize;
        
        return ptr;
    }

private:
    void allocateBlock() {
        #ifdef _MSC_VER
        m_currentBlock = _aligned_malloc(m_blockSize, m_alignment);
        #else
        posix_memalign(&m_currentBlock, m_alignment, m_blockSize);
        #endif
        
        m_blocks.push_back(m_currentBlock);
        m_currentPos = 0;
        m_currentBlockSize = m_blockSize;
    }
    
    size_t m_blockSize;
    size_t m_alignment;
    void* m_currentBlock;
    size_t m_currentPos;
    size_t m_currentBlockSize;
    std::vector<void*> m_blocks;
};

/**
 * Schema-Specific Memory Pools
 * 
 * Maintains separate memory pools for different schema types
 * to ensure optimal memory layout and reduce fragmentation.
 */
class SchemaPoolManager {
public:
    /**
     * Get the memory pool for a specific schema type
     * 
     * @param schemaName The name of the schema
     * @return Reference to the memory pool
     */
    MemoryPool& getPool(const std::string& schemaName) {
        auto it = m_pools.find(schemaName);
        if (it == m_pools.end()) {
            // Create new pool for this schema
            return m_pools.emplace(schemaName, MemoryPool()).first->second;
        }
        return it->second;
    }
    
    /**
     * Reset all memory pools
     */
    void reset() {
        m_pools.clear();
    }
    
    /**
     * Get the singleton instance
     */
    static SchemaPoolManager& getInstance() {
        static SchemaPoolManager instance;
        return instance;
    }

private:
    SchemaPoolManager() = default;
    std::unordered_map<std::string, MemoryPool> m_pools;
};

/**
 * Cache-aligned optimized layout for HealthComponent data
 */
struct ALIGNED(CACHE_LINE_SIZE) OptimizedHealthData {
    float currentHealth;
    float maxHealth;
    float regenerationRate;
    uint32_t flags;  // Bit 0: invulnerable, others reserved for future use
    
    // Default constructor initializes with reasonable values
    OptimizedHealthData()
        : currentHealth(100.0f)
        , maxHealth(100.0f)
        , regenerationRate(0.0f)
        , flags(0)
    {}
    
    // Set invulnerable flag
    void setInvulnerable(bool invulnerable) {
        if (invulnerable) {
            flags |= 1;
        } else {
            flags &= ~1;
        }
    }
    
    // Get invulnerable flag
    bool isInvulnerable() const {
        return (flags & 1) != 0;
    }
    
    /**
     * Load data from USD prim
     * 
     * @param prim The USD prim to load from
     * @return Whether the load was successful
     */
    bool loadFromUsd(const UsdPrim& prim) {
        // Cache tokens for property access
        static const TfToken currentHealthToken("sparkle:health:current");
        static const TfToken maxHealthToken("sparkle:health:maximum");
        static const TfToken regenRateToken("sparkle:health:regenerationRate");
        static const TfToken invulnerableToken("sparkle:health:invulnerable");
        
        // Get attributes
        UsdAttribute currentHealthAttr = prim.GetAttribute(currentHealthToken);
        UsdAttribute maxHealthAttr = prim.GetAttribute(maxHealthToken);
        UsdAttribute regenRateAttr = prim.GetAttribute(regenRateToken);
        UsdAttribute invulnerableAttr = prim.GetAttribute(invulnerableToken);
        
        // Read values (with default fallbacks if attributes don't exist)
        if (currentHealthAttr) {
            currentHealthAttr.Get(&currentHealth);
        }
        
        if (maxHealthAttr) {
            maxHealthAttr.Get(&maxHealth);
        }
        
        if (regenRateAttr) {
            regenRateAttr.Get(&regenerationRate);
        }
        
        if (invulnerableAttr) {
            bool invulnerable = false;
            invulnerableAttr.Get(&invulnerable);
            setInvulnerable(invulnerable);
        }
        
        return true;
    }
    
    /**
     * Save data to USD prim
     * 
     * @param prim The USD prim to save to
     * @return Whether the save was successful
     */
    bool saveToUsd(const UsdPrim& prim) const {
        // Cache tokens for property access
        static const TfToken currentHealthToken("sparkle:health:current");
        static const TfToken maxHealthToken("sparkle:health:maximum");
        static const TfToken regenRateToken("sparkle:health:regenerationRate");
        static const TfToken invulnerableToken("sparkle:health:invulnerable");
        
        // Get or create attributes
        UsdAttribute currentHealthAttr = prim.GetAttribute(currentHealthToken);
        if (!currentHealthAttr) {
            currentHealthAttr = prim.CreateAttribute(currentHealthToken, SdfValueTypeNames->Float);
        }
        
        UsdAttribute maxHealthAttr = prim.GetAttribute(maxHealthToken);
        if (!maxHealthAttr) {
            maxHealthAttr = prim.CreateAttribute(maxHealthToken, SdfValueTypeNames->Float);
        }
        
        UsdAttribute regenRateAttr = prim.GetAttribute(regenRateToken);
        if (!regenRateAttr) {
            regenRateAttr = prim.CreateAttribute(regenRateToken, SdfValueTypeNames->Float);
        }
        
        UsdAttribute invulnerableAttr = prim.GetAttribute(invulnerableToken);
        if (!invulnerableAttr) {
            invulnerableAttr = prim.CreateAttribute(invulnerableToken, SdfValueTypeNames->Bool);
        }
        
        // Set values
        currentHealthAttr.Set(currentHealth);
        maxHealthAttr.Set(maxHealth);
        regenRateAttr.Set(regenerationRate);
        invulnerableAttr.Set(isInvulnerable());
        
        return true;
    }
    
    /**
     * Create a new optimized health data object from a USD prim
     * 
     * @param prim The USD prim to load from
     * @return Pointer to the new optimized data
     */
    static OptimizedHealthData* createFromUsd(const UsdPrim& prim) {
        // Allocate from schema-specific pool
        void* memory = SchemaPoolManager::getInstance().getPool("SparkleHealthAPI").allocate(
            sizeof(OptimizedHealthData));
        
        // Placement new for construction
        OptimizedHealthData* data = new(memory) OptimizedHealthData();
        data->loadFromUsd(prim);
        
        return data;
    }
};

/**
 * SIMD-friendly optimized layout for TransformComponent data
 * Aligned to 16 bytes for SSE or 32 bytes for AVX
 */
struct ALIGNED(32) OptimizedTransformData {
    // Position and rotation are frequently accessed together, so we keep them together
    // and aligned for SIMD operations
    GfVec4f position;  // Using Vec4f for alignment (w component is unused)
    GfVec4f rotation;  // Quaternion rotation
    
    // Scale is less frequently accessed, but still kept in same struct
    GfVec4f scale;     // Using Vec4f for alignment (w component is unused)
    
    // Matrix cache - computed when needed
    GfMatrix4f worldMatrix;
    
    // Flag to track if matrix is dirty and needs recomputation
    uint32_t flags;
    
    OptimizedTransformData()
        : position(0.0f, 0.0f, 0.0f, 1.0f)
        , rotation(0.0f, 0.0f, 0.0f, 1.0f)  // Identity quaternion
        , scale(1.0f, 1.0f, 1.0f, 1.0f)
        , flags(1)  // Matrix starts dirty
    {}
    
    /**
     * Load data from USD prim
     * 
     * @param prim The USD prim to load from
     * @return Whether the load was successful
     */
    bool loadFromUsd(const UsdPrim& prim) {
        // Use UsdGeomXformable for transform data if possible
        UsdGeomXformable xformable(prim);
        if (xformable) {
            // Get local transformation
            GfMatrix4d localTransform;
            bool resetsXformStack;
            xformable.GetLocalTransformation(&localTransform, &resetsXformStack);
            
            // Extract components from matrix
            GfVec3d pos, scl;
            GfRotation rot;
            GfMatrix4d scaleOrient;
            
            if (localTransform.DecomposeTransform(
                    pos, rot, scl, 
                    GfVec3d(0), GfRotation(), scaleOrient)) {
                
                // Convert to our optimized layout
                position = GfVec4f(
                    static_cast<float>(pos[0]), 
                    static_cast<float>(pos[1]),
                    static_cast<float>(pos[2]),
                    1.0f);
                
                GfQuatd quatd = rot.GetQuat();
                rotation = GfVec4f(
                    static_cast<float>(quatd.GetReal()),
                    static_cast<float>(quatd.GetImaginary()[0]),
                    static_cast<float>(quatd.GetImaginary()[1]),
                    static_cast<float>(quatd.GetImaginary()[2]));
                
                scale = GfVec4f(
                    static_cast<float>(scl[0]),
                    static_cast<float>(scl[1]),
                    static_cast<float>(scl[2]),
                    1.0f);
                
                // Mark matrix as dirty
                flags |= 1;
                
                return true;
            }
        }
        
        // Fallback for custom transform properties
        static const TfToken positionToken("sparkle:transform:position");
        static const TfToken rotationToken("sparkle:transform:rotation");
        static const TfToken scaleToken("sparkle:transform:scale");
        
        UsdAttribute posAttr = prim.GetAttribute(positionToken);
        UsdAttribute rotAttr = prim.GetAttribute(rotationToken);
        UsdAttribute scaleAttr = prim.GetAttribute(scaleToken);
        
        if (posAttr) {
            GfVec3f pos;
            if (posAttr.Get(&pos)) {
                position = GfVec4f(pos[0], pos[1], pos[2], 1.0f);
            }
        }
        
        if (rotAttr) {
            GfQuatf rot;
            if (rotAttr.Get(&rot)) {
                rotation = GfVec4f(
                    rot.GetReal(),
                    rot.GetImaginary()[0],
                    rot.GetImaginary()[1],
                    rot.GetImaginary()[2]);
            }
        }
        
        if (scaleAttr) {
            GfVec3f scl;
            if (scaleAttr.Get(&scl)) {
                scale = GfVec4f(scl[0], scl[1], scl[2], 1.0f);
            }
        }
        
        // Mark matrix as dirty
        flags |= 1;
        
        return posAttr || rotAttr || scaleAttr;
    }
    
    /**
     * Save data to USD prim
     * 
     * @param prim The USD prim to save to
     * @return Whether the save was successful
     */
    bool saveToUsd(const UsdPrim& prim) const {
        // Try to use UsdGeomXformable for transform data
        UsdGeomXformable xformable(prim);
        if (xformable) {
            // Convert to matrix for Xformable
            GfMatrix4d matrix = computeTransformMatrix();
            
            // Clear existing transform ops and set a new one
            xformable.ClearXformOpOrder();
            UsdGeomXformOp matrixOp = xformable.AddTransformOp();
            matrixOp.Set(matrix);
            
            return true;
        }
        
        // Fallback for custom transform properties
        static const TfToken positionToken("sparkle:transform:position");
        static const TfToken rotationToken("sparkle:transform:rotation");
        static const TfToken scaleToken("sparkle:transform:scale");
        
        // Get or create attributes
        UsdAttribute posAttr = prim.GetAttribute(positionToken);
        if (!posAttr) {
            posAttr = prim.CreateAttribute(positionToken, SdfValueTypeNames->Float3);
        }
        
        UsdAttribute rotAttr = prim.GetAttribute(rotationToken);
        if (!rotAttr) {
            rotAttr = prim.CreateAttribute(rotationToken, SdfValueTypeNames->Quatf);
        }
        
        UsdAttribute scaleAttr = prim.GetAttribute(scaleToken);
        if (!scaleAttr) {
            scaleAttr = prim.CreateAttribute(scaleToken, SdfValueTypeNames->Float3);
        }
        
        // Set values
        GfVec3f pos(position[0], position[1], position[2]);
        posAttr.Set(pos);
        
        GfQuatf rot(rotation[0], GfVec3f(rotation[1], rotation[2], rotation[3]));
        rotAttr.Set(rot);
        
        GfVec3f scl(scale[0], scale[1], scale[2]);
        scaleAttr.Set(scl);
        
        return true;
    }
    
    /**
     * Get the world transform matrix
     * Computes it only when needed based on dirty flag
     * 
     * @return The world transform matrix
     */
    const GfMatrix4f& getWorldMatrix() {
        if (flags & 1) {
            worldMatrix = computeTransformMatrix();
            flags &= ~1;  // Clear dirty flag
        }
        return worldMatrix;
    }
    
    /**
     * Mark the matrix as dirty, needing recomputation
     */
    void markDirty() {
        flags |= 1;
    }
    
    /**
     * Set the position
     * 
     * @param x X coordinate
     * @param y Y coordinate
     * @param z Z coordinate
     */
    void setPosition(float x, float y, float z) {
        position = GfVec4f(x, y, z, 1.0f);
        markDirty();
    }
    
    /**
     * Set the rotation quaternion
     * 
     * @param w Real component
     * @param x Imaginary X component
     * @param y Imaginary Y component
     * @param z Imaginary Z component
     */
    void setRotation(float w, float x, float y, float z) {
        rotation = GfVec4f(w, x, y, z);
        markDirty();
    }
    
    /**
     * Set the scale
     * 
     * @param x X scale
     * @param y Y scale
     * @param z Z scale
     */
    void setScale(float x, float y, float z) {
        scale = GfVec4f(x, y, z, 1.0f);
        markDirty();
    }
    
    /**
     * Create a new optimized transform data object from a USD prim
     * 
     * @param prim The USD prim to load from
     * @return Pointer to the new optimized data
     */
    static OptimizedTransformData* createFromUsd(const UsdPrim& prim) {
        // Allocate from schema-specific pool
        void* memory = SchemaPoolManager::getInstance().getPool("TransformData").allocate(
            sizeof(OptimizedTransformData));
        
        // Placement new for construction
        OptimizedTransformData* data = new(memory) OptimizedTransformData();
        data->loadFromUsd(prim);
        
        return data;
    }

private:
    /**
     * Compute the transform matrix from components
     * 
     * @return The computed matrix
     */
    GfMatrix4d computeTransformMatrix() const {
        // Convert position to translation matrix
        GfMatrix4d translationMatrix;
        translationMatrix.SetTranslate(GfVec3d(position[0], position[1], position[2]));
        
        // Convert quaternion to rotation matrix
        GfQuatd rotQuat(rotation[0], GfVec3d(rotation[1], rotation[2], rotation[3]));
        GfMatrix4d rotationMatrix;
        rotationMatrix.SetRotate(rotQuat);
        
        // Convert scale to scale matrix
        GfMatrix4d scaleMatrix;
        scaleMatrix.SetScale(GfVec3d(scale[0], scale[1], scale[2]));
        
        // Combine matrices: Translation * Rotation * Scale
        return translationMatrix * rotationMatrix * scaleMatrix;
    }
};

/**
 * Optimized animation data with SoA layout for SIMD processing
 */
class OptimizedAnimationData {
public:
    OptimizedAnimationData(size_t initialCapacity = 32)
        : m_dirty(true)
    {
        // Pre-allocate memory for animation channels
        m_timePoints.reserve(initialCapacity);
        m_positions.reserve(initialCapacity);
        m_rotations.reserve(initialCapacity);
        m_scales.reserve(initialCapacity);
    }
    
    /**
     * Load data from USD prim
     * 
     * @param prim The USD prim to load from
     * @return Whether the load was successful
     */
    bool loadFromUsd(const UsdPrim& prim) {
        // Try to find animation data on the prim
        static const TfToken timePointsToken("sparkle:animation:timePoints");
        static const TfToken positionsToken("sparkle:animation:positions");
        static const TfToken rotationsToken("sparkle:animation:rotations");
        static const TfToken scalesToken("sparkle:animation:scales");
        
        UsdAttribute timePointsAttr = prim.GetAttribute(timePointsToken);
        UsdAttribute positionsAttr = prim.GetAttribute(positionsToken);
        UsdAttribute rotationsAttr = prim.GetAttribute(rotationsToken);
        UsdAttribute scalesAttr = prim.GetAttribute(scalesToken);
        
        if (!timePointsAttr || !positionsAttr) {
            return false;  // Minimum required data not found
        }
        
        // Get time points
        VtArray<float> timePoints;
        if (!timePointsAttr.Get(&timePoints)) {
            return false;
        }
        
        // Get position data
        VtArray<GfVec3f> positions;
        if (!positionsAttr.Get(&positions) || positions.size() != timePoints.size()) {
            return false;
        }
        
        // Get rotation data (optional)
        VtArray<GfQuatf> rotations;
        if (rotationsAttr) {
            rotationsAttr.Get(&rotations);
        }
        
        // Get scale data (optional)
        VtArray<GfVec3f> scales;
        if (scalesAttr) {
            scalesAttr.Get(&scales);
        }
        
        // Clear existing data
        m_timePoints.clear();
        m_positions.clear();
        m_rotations.clear();
        m_scales.clear();
        
        // Convert to SoA layout
        const size_t keyCount = timePoints.size();
        m_timePoints.resize(keyCount);
        m_positions.resize(keyCount);
        m_rotations.resize(keyCount);
        m_scales.resize(keyCount);
        
        for (size_t i = 0; i < keyCount; ++i) {
            // Time point
            m_timePoints[i] = timePoints[i];
            
            // Position
            GfVec3f pos = positions[i];
            m_positions[i] = GfVec4f(pos[0], pos[1], pos[2], 1.0f);
            
            // Rotation (with default if not provided)
            if (i < rotations.size()) {
                GfQuatf rot = rotations[i];
                m_rotations[i] = GfVec4f(
                    rot.GetReal(), 
                    rot.GetImaginary()[0],
                    rot.GetImaginary()[1],
                    rot.GetImaginary()[2]);
            } else {
                m_rotations[i] = GfVec4f(1.0f, 0.0f, 0.0f, 0.0f);  // Identity quaternion
            }
            
            // Scale (with default if not provided)
            if (i < scales.size()) {
                GfVec3f scl = scales[i];
                m_scales[i] = GfVec4f(scl[0], scl[1], scl[2], 1.0f);
            } else {
                m_scales[i] = GfVec4f(1.0f, 1.0f, 1.0f, 1.0f);  // Unity scale
            }
        }
        
        m_dirty = true;
        buildIndexCache();
        
        return true;
    }
    
    /**
     * Save data to USD prim
     * 
     * @param prim The USD prim to save to
     * @return Whether the save was successful
     */
    bool saveToUsd(const UsdPrim& prim) const {
        if (m_timePoints.empty()) {
            return false;  // No data to save
        }
        
        static const TfToken timePointsToken("sparkle:animation:timePoints");
        static const TfToken positionsToken("sparkle:animation:positions");
        static const TfToken rotationsToken("sparkle:animation:rotations");
        static const TfToken scalesToken("sparkle:animation:scales");
        
        // Get or create attributes
        UsdAttribute timePointsAttr = prim.GetAttribute(timePointsToken);
        if (!timePointsAttr) {
            timePointsAttr = prim.CreateAttribute(timePointsToken, SdfValueTypeNames->FloatArray);
        }
        
        UsdAttribute positionsAttr = prim.GetAttribute(positionsToken);
        if (!positionsAttr) {
            positionsAttr = prim.CreateAttribute(positionsToken, SdfValueTypeNames->Float3Array);
        }
        
        UsdAttribute rotationsAttr = prim.GetAttribute(rotationsToken);
        if (!rotationsAttr) {
            rotationsAttr = prim.CreateAttribute(rotationsToken, SdfValueTypeNames->QuatfArray);
        }
        
        UsdAttribute scalesAttr = prim.GetAttribute(scalesToken);
        if (!scalesAttr) {
            scalesAttr = prim.CreateAttribute(scalesToken, SdfValueTypeNames->Float3Array);
        }
        
        // Convert back to USD format
        const size_t keyCount = m_timePoints.size();
        
        VtArray<float> timePoints(keyCount);
        VtArray<GfVec3f> positions(keyCount);
        VtArray<GfQuatf> rotations(keyCount);
        VtArray<GfVec3f> scales(keyCount);
        
        for (size_t i = 0; i < keyCount; ++i) {
            timePoints[i] = m_timePoints[i];
            
            positions[i] = GfVec3f(
                m_positions[i][0], 
                m_positions[i][1], 
                m_positions[i][2]);
            
            rotations[i] = GfQuatf(
                m_rotations[i][0],
                GfVec3f(
                    m_rotations[i][1],
                    m_rotations[i][2],
                    m_rotations[i][3]));
                    
            scales[i] = GfVec3f(
                m_scales[i][0],
                m_scales[i][1],
                m_scales[i][2]);
        }
        
        // Set values
        timePointsAttr.Set(timePoints);
        positionsAttr.Set(positions);
        rotationsAttr.Set(rotations);
        scalesAttr.Set(scales);
        
        return true;
    }
    
    /**
     * Get interpolated transform at a specific time
     * 
     * @param time The time to evaluate at
     * @param position Output position
     * @param rotation Output rotation
     * @param scale Output scale
     */
    void evaluate(float time, GfVec4f& position, GfVec4f& rotation, GfVec4f& scale) const {
        if (m_timePoints.empty()) {
            // Default transform if no animation data
            position = GfVec4f(0.0f, 0.0f, 0.0f, 1.0f);
            rotation = GfVec4f(1.0f, 0.0f, 0.0f, 0.0f);  // Identity quaternion
            scale = GfVec4f(1.0f, 1.0f, 1.0f, 1.0f);
            return;
        }
        
        if (m_timePoints.size() == 1) {
            // Only one keyframe, no interpolation needed
            position = m_positions[0];
            rotation = m_rotations[0];
            scale = m_scales[0];
            return;
        }
        
        // Find keyframes to interpolate between
        int index = findTimeIndex(time);
        
        // Clamp to available range
        if (index < 0) {
            position = m_positions[0];
            rotation = m_rotations[0];
            scale = m_scales[0];
            return;
        }
        
        if (index >= static_cast<int>(m_timePoints.size() - 1)) {
            position = m_positions.back();
            rotation = m_rotations.back();
            scale = m_scales.back();
            return;
        }
        
        // Calculate interpolation factor
        float t1 = m_timePoints[index];
        float t2 = m_timePoints[index + 1];
        float factor = (time - t1) / (t2 - t1);
        
        // Interpolate position (linear)
        position = lerp(m_positions[index], m_positions[index + 1], factor);
        
        // Interpolate rotation (spherical linear)
        rotation = slerp(m_rotations[index], m_rotations[index + 1], factor);
        
        // Interpolate scale (linear)
        scale = lerp(m_scales[index], m_scales[index + 1], factor);
    }
    
    /**
     * Add a keyframe to the animation
     * 
     * @param time Time point
     * @param position Position at time
     * @param rotation Rotation at time
     * @param scale Scale at time
     */
    void addKeyframe(float time, const GfVec4f& position, 
                    const GfVec4f& rotation, const GfVec4f& scale) {
        // Find insertion point to maintain sorted order
        auto it = std::lower_bound(m_timePoints.begin(), m_timePoints.end(), time);
        size_t index = it - m_timePoints.begin();
        
        // Check if this time already exists
        if (it != m_timePoints.end() && *it == time) {
            // Update existing keyframe
            m_positions[index] = position;
            m_rotations[index] = rotation;
            m_scales[index] = scale;
        } else {
            // Insert new keyframe
            m_timePoints.insert(it, time);
            m_positions.insert(m_positions.begin() + index, position);
            m_rotations.insert(m_rotations.begin() + index, rotation);
            m_scales.insert(m_scales.begin() + index, scale);
        }
        
        m_dirty = true;
    }
    
    /**
     * Remove a keyframe at the specified time
     * 
     * @param time Time point to remove
     * @return Whether a keyframe was removed
     */
    bool removeKeyframe(float time) {
        auto it = std::find(m_timePoints.begin(), m_timePoints.end(), time);
        if (it == m_timePoints.end()) {
            return false;
        }
        
        size_t index = it - m_timePoints.begin();
        
        m_timePoints.erase(m_timePoints.begin() + index);
        m_positions.erase(m_positions.begin() + index);
        m_rotations.erase(m_rotations.begin() + index);
        m_scales.erase(m_scales.begin() + index);
        
        m_dirty = true;
        return true;
    }
    
    /**
     * Create a new optimized animation data object from a USD prim
     * 
     * @param prim The USD prim to load from
     * @return Pointer to the new optimized data
     */
    static OptimizedAnimationData* createFromUsd(const UsdPrim& prim) {
        // Allocate from schema-specific pool
        void* memory = SchemaPoolManager::getInstance().getPool("AnimationData").allocate(
            sizeof(OptimizedAnimationData));
        
        // Placement new for construction
        OptimizedAnimationData* data = new(memory) OptimizedAnimationData();
        data->loadFromUsd(prim);
        
        return data;
    }

private:
    std::vector<float> m_timePoints;
    std::vector<GfVec4f> m_positions;
    std::vector<GfVec4f> m_rotations;
    std::vector<GfVec4f> m_scales;
    
    // Cached index data for faster lookup
    std::vector<std::pair<float, int>> m_indexCache;
    bool m_dirty;
    
    /**
     * Build the index cache for faster time lookups
     */
    void buildIndexCache() {
        if (!m_dirty) {
            return;
        }
        
        const size_t keyCount = m_timePoints.size();
        
        // Build time-to-index cache for faster lookup
        m_indexCache.resize(keyCount);
        for (size_t i = 0; i < keyCount; ++i) {
            m_indexCache[i] = std::make_pair(m_timePoints[i], static_cast<int>(i));
        }
        
        // Sort by time (should already be sorted, but just in case)
        std::sort(m_indexCache.begin(), m_indexCache.end());
        
        m_dirty = false;
    }
    
    /**
     * Find the index for the keyframe just before the given time
     * 
     * @param time Time to find
     * @return Index of the keyframe, or -1 if before first keyframe
     */
    int findTimeIndex(float time) const {
        if (m_dirty) {
            const_cast<OptimizedAnimationData*>(this)->buildIndexCache();
        }
        
        // Binary search in the index cache
        auto it = std::lower_bound(
            m_indexCache.begin(), 
            m_indexCache.end(), 
            std::make_pair(time, 0),
            [](const std::pair<float, int>& a, const std::pair<float, int>& b) {
                return a.first < b.first;
            });
        
        if (it == m_indexCache.begin()) {
            return -1;  // Before first keyframe
        }
        
        // Return the index right before the target time
        return (--it)->second;
    }
    
    /**
     * Linear interpolation for vectors
     */
    static GfVec4f lerp(const GfVec4f& a, const GfVec4f& b, float t) {
        return GfVec4f(
            a[0] + (b[0] - a[0]) * t,
            a[1] + (b[1] - a[1]) * t,
            a[2] + (b[2] - a[2]) * t,
            a[3] + (b[3] - a[3]) * t);
    }
    
    /**
     * Spherical linear interpolation for quaternions
     */
    static GfVec4f slerp(const GfVec4f& a, const GfVec4f& b, float t) {
        // Extract quaternion components
        float aw = a[0], ax = a[1], ay = a[2], az = a[3];
        float bw = b[0], bx = b[1], by = b[2], bz = b[3];
        
        // Calculate cosine of angle between quaternions
        float cosTheta = aw * bw + ax * bx + ay * by + az * bz;
        
        // If cosTheta < 0, negate one quaternion to take shorter path
        if (cosTheta < 0.0f) {
            bw = -bw;
            bx = -bx;
            by = -by;
            bz = -bz;
            cosTheta = -cosTheta;
        }
        
        // Use linear interpolation for very close quaternions
        float epsilon = 0.001f;
        if (cosTheta > 1.0f - epsilon) {
            return lerp(a, b, t);
        }
        
        // Calculate interpolation parameters
        float theta = acosf(cosTheta);
        float sinTheta = sinf(theta);
        float ratioA = sinf((1.0f - t) * theta) / sinTheta;
        float ratioB = sinf(t * theta) / sinTheta;
        
        // Calculate interpolated quaternion
        return GfVec4f(
            aw * ratioA + bw * ratioB,
            ax * ratioA + bx * ratioB,
            ay * ratioA + by * ratioB,
            az * ratioA + bz * ratioB);
    }
};

/**
 * SIMD-friendly physics data with aligned memory for specific CPU instructions
 */
struct ALIGNED(CACHE_LINE_SIZE) OptimizedPhysicsData {
    // Dynamic properties (frequently changing)
    GfVec4f linearVelocity;     // XYZ + padding
    GfVec4f angularVelocity;    // XYZ + padding
    GfVec4f forces;             // XYZ + padding
    GfVec4f torques;            // XYZ + padding
    
    // Static properties (rarely changing)
    float mass;                 // Mass in kg
    float inverseMass;          // Cached 1/mass for faster calculations
    GfVec4f localInertia;       // Inertia tensor diagonal + padding
    GfVec4f inverseInertia;     // Cached inverse inertia tensor diagonal + padding
    
    // Collision properties
    float restitution;          // Bounce factor
    float friction;             // Surface friction
    uint32_t collisionGroup;    // Collision group membership
    uint32_t collisionMask;     // Which groups to collide with
    
    // Flags for physics behavior
    uint32_t flags;
    
    // Constants for flags
    enum PhysicsFlags {
        DYNAMIC       = 1 << 0,   // Object is dynamic (affected by forces)
        KINEMATIC     = 1 << 1,   // Object is kinematic (moved by animation)
        GRAVITY       = 1 << 2,   // Object is affected by gravity
        SLEEPING      = 1 << 3,   // Object is sleeping (optimization)
        TRIGGER       = 1 << 4,   // Object is a trigger volume
        NO_ROTATION   = 1 << 5    // Object can translate but not rotate
    };
    
    OptimizedPhysicsData()
        : linearVelocity(0.0f, 0.0f, 0.0f, 0.0f)
        , angularVelocity(0.0f, 0.0f, 0.0f, 0.0f)
        , forces(0.0f, 0.0f, 0.0f, 0.0f)
        , torques(0.0f, 0.0f, 0.0f, 0.0f)
        , mass(1.0f)
        , inverseMass(1.0f)
        , localInertia(1.0f, 1.0f, 1.0f, 0.0f)
        , inverseInertia(1.0f, 1.0f, 1.0f, 0.0f)
        , restitution(0.5f)
        , friction(0.5f)
        , collisionGroup(1)
        , collisionMask(0xFFFFFFFF)
        , flags(DYNAMIC | GRAVITY)
    {}
    
    /**
     * Load data from USD prim
     * 
     * @param prim The USD prim to load from
     * @return Whether the load was successful
     */
    bool loadFromUsd(const UsdPrim& prim) {
        // Cache tokens for property access
        static const TfToken massToken("sparkle:physics:mass");
        static const TfToken restitutionToken("sparkle:physics:restitution");
        static const TfToken frictionToken("sparkle:physics:friction");
        static const TfToken dynamicToken("sparkle:physics:dynamic");
        static const TfToken gravityToken("sparkle:physics:useGravity");
        static const TfToken triggerToken("sparkle:physics:isTrigger");
        static const TfToken groupToken("sparkle:physics:collisionGroup");
        static const TfToken maskToken("sparkle:physics:collisionMask");
        
        // Get attributes with fallbacks
        UsdAttribute massAttr = prim.GetAttribute(massToken);
        if (massAttr) {
            massAttr.Get(&mass);
            inverseMass = mass > 0.001f ? 1.0f / mass : 0.0f;
        }
        
        UsdAttribute restitutionAttr = prim.GetAttribute(restitutionToken);
        if (restitutionAttr) {
            restitutionAttr.Get(&restitution);
        }
        
        UsdAttribute frictionAttr = prim.GetAttribute(frictionToken);
        if (frictionAttr) {
            frictionAttr.Get(&friction);
        }
        
        // Set flags based on attributes
        bool isDynamic = true;
        UsdAttribute dynamicAttr = prim.GetAttribute(dynamicToken);
        if (dynamicAttr) {
            dynamicAttr.Get(&isDynamic);
        }
        
        bool useGravity = true;
        UsdAttribute gravityAttr = prim.GetAttribute(gravityToken);
        if (gravityAttr) {
            gravityAttr.Get(&useGravity);
        }
        
        bool isTrigger = false;
        UsdAttribute triggerAttr = prim.GetAttribute(triggerToken);
        if (triggerAttr) {
            triggerAttr.Get(&isTrigger);
        }
        
        // Set flags
        flags = 0;
        if (isDynamic) flags |= DYNAMIC;
        if (useGravity) flags |= GRAVITY;
        if (isTrigger) flags |= TRIGGER;
        
        // Get collision groups
        UsdAttribute groupAttr = prim.GetAttribute(groupToken);
        if (groupAttr) {
            groupAttr.Get(&collisionGroup);
        }
        
        UsdAttribute maskAttr = prim.GetAttribute(maskToken);
        if (maskAttr) {
            maskAttr.Get(&collisionMask);
        }
        
        // Calculate inertia tensor for a box (simplification)
        // A more robust implementation would check for a physics schema with inertia tensor
        localInertia = GfVec4f(
            mass / 6.0f, // For a 1x1x1 box
            mass / 6.0f,
            mass / 6.0f,
            0.0f);
            
        // Calculate inverse inertia
        for (int i = 0; i < 3; ++i) {
            inverseInertia[i] = localInertia[i] > 0.001f ? 1.0f / localInertia[i] : 0.0f;
        }
        
        return massAttr || restitutionAttr || frictionAttr;
    }
    
    /**
     * Save data to USD prim
     * 
     * @param prim The USD prim to save to
     * @return Whether the save was successful
     */
    bool saveToUsd(const UsdPrim& prim) const {
        // Cache tokens for property access
        static const TfToken massToken("sparkle:physics:mass");
        static const TfToken restitutionToken("sparkle:physics:restitution");
        static const TfToken frictionToken("sparkle:physics:friction");
        static const TfToken dynamicToken("sparkle:physics:dynamic");
        static const TfToken gravityToken("sparkle:physics:useGravity");
        static const TfToken triggerToken("sparkle:physics:isTrigger");
        static const TfToken groupToken("sparkle:physics:collisionGroup");
        static const TfToken maskToken("sparkle:physics:collisionMask");
        static const TfToken velocityToken("sparkle:physics:linearVelocity");
        static const TfToken angVelocityToken("sparkle:physics:angularVelocity");
        
        // Get or create attributes
        UsdAttribute massAttr = prim.GetAttribute(massToken);
        if (!massAttr) {
            massAttr = prim.CreateAttribute(massToken, SdfValueTypeNames->Float);
        }
        
        UsdAttribute restitutionAttr = prim.GetAttribute(restitutionToken);
        if (!restitutionAttr) {
            restitutionAttr = prim.CreateAttribute(restitutionToken, SdfValueTypeNames->Float);
        }
        
        UsdAttribute frictionAttr = prim.GetAttribute(frictionToken);
        if (!frictionAttr) {
            frictionAttr = prim.CreateAttribute(frictionToken, SdfValueTypeNames->Float);
        }
        
        UsdAttribute dynamicAttr = prim.GetAttribute(dynamicToken);
        if (!dynamicAttr) {
            dynamicAttr = prim.CreateAttribute(dynamicToken, SdfValueTypeNames->Bool);
        }
        
        UsdAttribute gravityAttr = prim.GetAttribute(gravityToken);
        if (!gravityAttr) {
            gravityAttr = prim.CreateAttribute(gravityToken, SdfValueTypeNames->Bool);
        }
        
        UsdAttribute triggerAttr = prim.GetAttribute(triggerToken);
        if (!triggerAttr) {
            triggerAttr = prim.CreateAttribute(triggerToken, SdfValueTypeNames->Bool);
        }
        
        UsdAttribute groupAttr = prim.GetAttribute(groupToken);
        if (!groupAttr) {
            groupAttr = prim.CreateAttribute(groupToken, SdfValueTypeNames->UInt);
        }
        
        UsdAttribute maskAttr = prim.GetAttribute(maskToken);
        if (!maskAttr) {
            maskAttr = prim.CreateAttribute(maskToken, SdfValueTypeNames->UInt);
        }
        
        UsdAttribute velocityAttr = prim.GetAttribute(velocityToken);
        if (!velocityAttr) {
            velocityAttr = prim.CreateAttribute(velocityToken, SdfValueTypeNames->Float3);
        }
        
        UsdAttribute angVelocityAttr = prim.GetAttribute(angVelocityToken);
        if (!angVelocityAttr) {
            angVelocityAttr = prim.CreateAttribute(angVelocityToken, SdfValueTypeNames->Float3);
        }
        
        // Set values
        massAttr.Set(mass);
        restitutionAttr.Set(restitution);
        frictionAttr.Set(friction);
        dynamicAttr.Set((flags & DYNAMIC) != 0);
        gravityAttr.Set((flags & GRAVITY) != 0);
        triggerAttr.Set((flags & TRIGGER) != 0);
        groupAttr.Set(collisionGroup);
        maskAttr.Set(collisionMask);
        
        // Save current dynamic state
        GfVec3f linearVel(linearVelocity[0], linearVelocity[1], linearVelocity[2]);
        velocityAttr.Set(linearVel);
        
        GfVec3f angularVel(angularVelocity[0], angularVelocity[1], angularVelocity[2]);
        angVelocityAttr.Set(angularVel);
        
        return true;
    }
    
    /**
     * Apply a force to the center of mass
     * 
     * @param force Force vector
     */
    void applyForce(const GfVec3f& force) {
        if (!(flags & DYNAMIC)) return;
        
        forces[0] += force[0];
        forces[1] += force[1];
        forces[2] += force[2];
    }
    
    /**
     * Apply a torque
     * 
     * @param torque Torque vector
     */
    void applyTorque(const GfVec3f& torque) {
        if (!(flags & DYNAMIC) || (flags & NO_ROTATION)) return;
        
        torques[0] += torque[0];
        torques[1] += torque[1];
        torques[2] += torque[2];
    }
    
    /**
     * Integrate physics for a time step
     * 
     * @param deltaTime Time step in seconds
     * @param gravity Global gravity vector
     * @param position Current position (will be updated)
     * @param rotation Current rotation (will be updated)
     */
    void integrate(float deltaTime, const GfVec3f& gravity, 
                  GfVec4f& position, GfVec4f& rotation) {
        if (!(flags & DYNAMIC) || (flags & SLEEPING)) return;
        
        // Apply gravity if enabled
        if (flags & GRAVITY) {
            forces[0] += gravity[0] * mass;
            forces[1] += gravity[1] * mass;
            forces[2] += gravity[2] * mass;
        }
        
        // Update linear velocity
        linearVelocity[0] += forces[0] * inverseMass * deltaTime;
        linearVelocity[1] += forces[1] * inverseMass * deltaTime;
        linearVelocity[2] += forces[2] * inverseMass * deltaTime;
        
        // Update position
        position[0] += linearVelocity[0] * deltaTime;
        position[1] += linearVelocity[1] * deltaTime;
        position[2] += linearVelocity[2] * deltaTime;
        
        // Update angular velocity if rotation is enabled
        if (!(flags & NO_ROTATION)) {
            angularVelocity[0] += torques[0] * inverseInertia[0] * deltaTime;
            angularVelocity[1] += torques[1] * inverseInertia[1] * deltaTime;
            angularVelocity[2] += torques[2] * inverseInertia[2] * deltaTime;
            
            // Update rotation (simplified quaternion integration)
            // A more robust implementation would use quaternion differential equations
            GfVec4f deltaRotation(
                0.0f,
                angularVelocity[0] * deltaTime * 0.5f,
                angularVelocity[1] * deltaTime * 0.5f,
                angularVelocity[2] * deltaTime * 0.5f);
            
            // Quaternion multiplication (simplified)
            float qw = rotation[0];
            float qx = rotation[1];
            float qy = rotation[2];
            float qz = rotation[3];
            
            rotation[0] = qw - deltaRotation[1] * qx - deltaRotation[2] * qy - deltaRotation[3] * qz;
            rotation[1] = qx + deltaRotation[1] * qw + deltaRotation[2] * qz - deltaRotation[3] * qy;
            rotation[2] = qy - deltaRotation[1] * qz + deltaRotation[2] * qw + deltaRotation[3] * qx;
            rotation[3] = qz + deltaRotation[1] * qy - deltaRotation[2] * qx + deltaRotation[3] * qw;
            
            // Normalize quaternion
            float lengthSq = rotation[0]*rotation[0] + rotation[1]*rotation[1] + 
                             rotation[2]*rotation[2] + rotation[3]*rotation[3];
            if (lengthSq > 0.0f) {
                float invLength = 1.0f / sqrtf(lengthSq);
                rotation[0] *= invLength;
                rotation[1] *= invLength;
                rotation[2] *= invLength;
                rotation[3] *= invLength;
            }
        }
        
        // Reset forces and torques
        forces = GfVec4f(0.0f, 0.0f, 0.0f, 0.0f);
        torques = GfVec4f(0.0f, 0.0f, 0.0f, 0.0f);
    }
    
    /**
     * Create a new optimized physics data object from a USD prim
     * 
     * @param prim The USD prim to load from
     * @return Pointer to the new optimized data
     */
    static OptimizedPhysicsData* createFromUsd(const UsdPrim& prim) {
        // Allocate from schema-specific pool
        void* memory = SchemaPoolManager::getInstance().getPool("PhysicsData").allocate(
            sizeof(OptimizedPhysicsData));
        
        // Placement new for construction
        OptimizedPhysicsData* data = new(memory) OptimizedPhysicsData();
        data->loadFromUsd(prim);
        
        return data;
    }
};

/**
 * EntityOptimizedData
 * 
 * Container for all optimized schema data for an entity
 */
class EntityOptimizedData {
public:
    EntityOptimizedData(const UsdPrim& prim)
        : m_prim(prim)
        , m_healthData(nullptr)
        , m_transformData(nullptr)
        , m_animationData(nullptr)
        , m_physicsData(nullptr)
    {
        // Detect and create optimized data based on schema types
        if (hasSchema(prim, "SparkleHealthAPI")) {
            m_healthData = OptimizedHealthData::createFromUsd(prim);
        }
        
        // All entities should have transform data
        m_transformData = OptimizedTransformData::createFromUsd(prim);
        
        // Check for animation data
        static const TfToken timePointsToken("sparkle:animation:timePoints");
        if (prim.HasAttribute(timePointsToken)) {
            m_animationData = OptimizedAnimationData::createFromUsd(prim);
        }
        
        // Check for physics data
        if (hasSchema(prim, "SparklePhysicsAPI")) {
            m_physicsData = OptimizedPhysicsData::createFromUsd(prim);
        }
    }
    
    ~EntityOptimizedData() {
        // Memory is managed by the schema-specific pools,
        // so we don't need to delete anything here
    }
    
    // Accessors for optimized data
    OptimizedHealthData* getHealthData() const { return m_healthData; }
    OptimizedTransformData* getTransformData() const { return m_transformData; }
    OptimizedAnimationData* getAnimationData() const { return m_animationData; }
    OptimizedPhysicsData* getPhysicsData() const { return m_physicsData; }
    
    /**
     * Sync all data back to USD
     * 
     * @return Whether all syncs were successful
     */
    bool syncToUsd() {
        bool success = true;
        
        if (m_healthData) {
            success &= m_healthData->saveToUsd(m_prim);
        }
        
        if (m_transformData) {
            success &= m_transformData->saveToUsd(m_prim);
        }
        
        if (m_animationData) {
            success &= m_animationData->saveToUsd(m_prim);
        }
        
        if (m_physicsData) {
            success &= m_physicsData->saveToUsd(m_prim);
        }
        
        return success;
    }
    
    /**
     * Update entity for a single frame
     * 
     * @param deltaTime Time since last update in seconds
     * @param time Current game time
     */
    void update(float deltaTime, float time) {
        // Apply animation if available
        if (m_animationData && m_transformData) {
            GfVec4f position, rotation, scale;
            m_animationData->evaluate(time, position, rotation, scale);
            
            // Update transform with animated values
            m_transformData->setPosition(position[0], position[1], position[2]);
            m_transformData->setRotation(rotation[0], rotation[1], rotation[2], rotation[3]);
            m_transformData->setScale(scale[0], scale[1], scale[2]);
        }
        
        // Apply physics if available
        if (m_physicsData && m_transformData) {
            // Get current transform
            GfVec4f position = m_transformData->position;
            GfVec4f rotation = m_transformData->rotation;
            
            // Integrate physics
            GfVec3f gravity(0.0f, -9.81f, 0.0f);  // Standard gravity
            m_physicsData->integrate(deltaTime, gravity, position, rotation);
            
            // Update transform with physics results
            m_transformData->setPosition(position[0], position[1], position[2]);
            m_transformData->setRotation(rotation[0], rotation[1], rotation[2], rotation[3]);
        }
        
        // Apply health regeneration if available
        if (m_healthData) {
            float currentHealth = m_healthData->currentHealth;
            float maxHealth = m_healthData->maxHealth;
            float regenRate = m_healthData->regenerationRate;
            
            if (regenRate > 0.0f && currentHealth < maxHealth) {
                currentHealth += regenRate * deltaTime;
                if (currentHealth > maxHealth) {
                    currentHealth = maxHealth;
                }
                m_healthData->currentHealth = currentHealth;
            }
        }
    }

private:
    UsdPrim m_prim;
    OptimizedHealthData* m_healthData;
    OptimizedTransformData* m_transformData;
    OptimizedAnimationData* m_animationData;
    OptimizedPhysicsData* m_physicsData;
    
    /**
     * Check if a prim has a specific schema applied
     */
    bool hasSchema(const UsdPrim& prim, const std::string& schemaName) {
        std::vector<std::string> schemas;
        prim.GetAppliedSchemas(&schemas);
        
        return std::find(schemas.begin(), schemas.end(), schemaName) != schemas.end();
    }
};

/**
 * Struct-of-Arrays layout for batch processing similar entities
 */
template<size_t MaxEntities = 1024>
class EntityBatchProcessor {
public:
    EntityBatchProcessor() : m_entityCount(0) {}
    
    /**
     * Add an entity to the batch
     * 
     * @param entity Entity data to add
     * @return Whether the entity was added
     */
    bool addEntity(EntityOptimizedData* entity) {
        if (m_entityCount >= MaxEntities || !entity) {
            return false;
        }
        
        // Store entity pointer
        m_entities[m_entityCount] = entity;
        
        // Extract transform data for SIMD processing
        OptimizedTransformData* transform = entity->getTransformData();
        if (transform) {
            m_positions[m_entityCount] = transform->position;
            m_rotations[m_entityCount] = transform->rotation;
            m_scales[m_entityCount] = transform->scale;
        }
        
        // Extract health data if available
        OptimizedHealthData* health = entity->getHealthData();
        if (health) {
            m_healthValues[m_entityCount] = health->currentHealth;
            m_maxHealthValues[m_entityCount] = health->maxHealth;
            m_regenRates[m_entityCount] = health->regenerationRate;
        } else {
            m_healthValues[m_entityCount] = 0.0f;
            m_maxHealthValues[m_entityCount] = 0.0f;
            m_regenRates[m_entityCount] = 0.0f;
        }
        
        m_entityCount++;
        return true;
    }
    
    /**
     * Batch update all entities
     * 
     * @param deltaTime Time since last update
     */
    void updateAll(float deltaTime) {
        // Example: Update all health values in a SIMD-friendly way
        batchUpdateHealth(deltaTime);
        
        // Other batch updates could go here
        
        // Sync updated values back to entity objects
        syncBackToEntities();
    }

private:
    // Entity references
    EntityOptimizedData* m_entities[MaxEntities];
    size_t m_entityCount;
    
    // Transform data in SoA layout
    GfVec4f m_positions[MaxEntities];
    GfVec4f m_rotations[MaxEntities];
    GfVec4f m_scales[MaxEntities];
    
    // Health data in SoA layout
    float m_healthValues[MaxEntities];
    float m_maxHealthValues[MaxEntities];
    float m_regenRates[MaxEntities];
    
    /**
     * Batch update health values using SIMD-friendly layout
     * 
     * @param deltaTime Time since last update
     */
    void batchUpdateHealth(float deltaTime) {
        // This could be optimized with SIMD instructions (SSE/AVX/NEON)
        // For simplicity, using standard loop
        for (size_t i = 0; i < m_entityCount; ++i) {
            // Skip entities without regeneration
            if (m_regenRates[i] <= 0.0f) continue;
            
            // Apply regeneration
            if (m_healthValues[i] < m_maxHealthValues[i]) {
                m_healthValues[i] += m_regenRates[i] * deltaTime;
                
                // Clamp to max health
                if (m_healthValues[i] > m_maxHealthValues[i]) {
                    m_healthValues[i] = m_maxHealthValues[i];
                }
            }
        }
    }
    
    /**
     * Sync batch-processed data back to entity objects
     */
    void syncBackToEntities() {
        for (size_t i = 0; i < m_entityCount; ++i) {
            EntityOptimizedData* entity = m_entities[i];
            
            // Sync health data
            OptimizedHealthData* health = entity->getHealthData();
            if (health) {
                health->currentHealth = m_healthValues[i];
            }
            
            // Sync transform data
            OptimizedTransformData* transform = entity->getTransformData();
            if (transform) {
                transform->position = m_positions[i];
                transform->rotation = m_rotations[i];
                transform->scale = m_scales[i];
                transform->markDirty();
            }
        }
    }
};

/**
 * World containing optimized schema data for all entities
 */
class OptimizedWorld {
public:
    OptimizedWorld() {}
    
    /**
     * Load a USD stage into optimized memory layouts
     * 
     * @param stage The USD stage to load
     */
    void loadStage(const UsdStageRefPtr& stage) {
        // Clear existing data
        m_entities.clear();
        
        // Process all game entity prims
        for (const UsdPrim& prim : stage->Traverse()) {
            if (isGameEntity(prim)) {
                // Create optimized data for this entity
                std::unique_ptr<EntityOptimizedData> entityData = 
                    std::make_unique<EntityOptimizedData>(prim);
                
                // Store entity data
                m_entities[prim.GetPath()] = std::move(entityData);
            }
        }
        
        // Create batch processor for similar entities
        createBatchProcessors();
    }
    
    /**
     * Update all entities
     * 
     * @param deltaTime Time since last update
     * @param gameTime Current game time
     */
    void update(float deltaTime, float gameTime) {
        // Update batch-processed entities
        for (auto& processor : m_batchProcessors) {
            processor.updateAll(deltaTime);
        }
        
        // Update remaining entities individually
        for (auto& pair : m_entities) {
            EntityOptimizedData* entity = pair.second.get();
            entity->update(deltaTime, gameTime);
        }
    }
    
    /**
     * Sync all entities back to USD
     */
    void syncToUsd() {
        for (auto& pair : m_entities) {
            pair.second->syncToUsd();
        }
    }
    
    /**
     * Get entity data by path
     * 
     * @param path Path to the entity
     * @return Pointer to entity data, or nullptr if not found
     */
    EntityOptimizedData* getEntity(const SdfPath& path) {
        auto it = m_entities.find(path);
        if (it != m_entities.end()) {
            return it->second.get();
        }
        return nullptr;
    }

private:
    std::unordered_map<SdfPath, std::unique_ptr<EntityOptimizedData>, SdfPath::Hash> m_entities;
    std::vector<EntityBatchProcessor<1024>> m_batchProcessors;
    
    /**
     * Check if a prim is a game entity
     * 
     * @param prim The prim to check
     * @return Whether the prim is a game entity
     */
    bool isGameEntity(const UsdPrim& prim) {
        // Use type registry to check if this is a game entity
        // This is a simplified example - actual implementation would use schema registry
        return prim.IsA(TfType::FindByName("SparkleGameEntity")) ||
               prim.IsA(TfType::FindByName("SparkleEnemyCarrot")) ||
               prim.IsA(TfType::FindByName("SparklePlayer"));
    }
    
    /**
     * Create batch processors for similar entities
     */
    void createBatchProcessors() {
        // Group similar entities together for batch processing
        
        // Health entities
        EntityBatchProcessor<1024> healthProcessor;
        for (auto& pair : m_entities) {
            EntityOptimizedData* entity = pair.second.get();
            if (entity->getHealthData()) {
                healthProcessor.addEntity(entity);
            }
        }
        
        // Store batch processors
        m_batchProcessors.push_back(healthProcessor);
        
        // Additional processors could be created for other component types
    }
};

/**
 * Benchmark function to demonstrate performance improvements from optimized layouts
 */
inline void runMemoryLayoutBenchmark() {
    // Load a USD stage
    UsdStageRefPtr stage = UsdStage::Open("game_level.usda");
    if (!stage) {
        std::cerr << "Failed to open stage" << std::endl;
        return;
    }
    
    // Find all game entities for testing
    std::vector<UsdPrim> gameEntities;
    for (const UsdPrim& prim : stage->Traverse()) {
        if (prim.IsA(TfType::FindByName("SparkleGameEntity"))) {
            gameEntities.push_back(prim);
        }
    }
    
    if (gameEntities.empty()) {
        std::cerr << "No game entities found in stage" << std::endl;
        return;
    }
    
    std::cout << "Testing with " << gameEntities.size() << " entities" << std::endl;
    
    // Benchmark 1: Standard USD access
    {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Run 100 updates using standard USD access
        for (int frame = 0; frame < 100; ++frame) {
            for (const UsdPrim& prim : gameEntities) {
                // Standard USD attribute access
                static const TfToken healthToken("sparkle:health:current");
                static const TfToken maxHealthToken("sparkle:health:maximum");
                static const TfToken regenToken("sparkle:health:regenerationRate");
                
                UsdAttribute healthAttr = prim.GetAttribute(healthToken);
                UsdAttribute maxHealthAttr = prim.GetAttribute(maxHealthToken);
                UsdAttribute regenAttr = prim.GetAttribute(regenToken);
                
                // Get values
                float health = 0.0f;
                float maxHealth = 0.0f;
                float regenRate = 0.0f;
                
                if (healthAttr) healthAttr.Get(&health);
                if (maxHealthAttr) maxHealthAttr.Get(&maxHealth);
                if (regenAttr) regenAttr.Get(&regenRate);
                
                // Update health with regeneration
                if (regenRate > 0.0f && health < maxHealth) {
                    health += regenRate * 0.016f; // 60fps
                    if (health > maxHealth) health = maxHealth;
                    healthAttr.Set(health);
                }
            }
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();
            
        std::cout << "Standard USD access: " << duration << " ms" << std::endl;
    }
    
    // Benchmark 2: Optimized memory layout
    {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Create optimized world
        OptimizedWorld world;
        world.loadStage(stage);
        
        // Run 100 updates using optimized memory layout
        for (int frame = 0; frame < 100; ++frame) {
            world.update(0.016f, frame * 0.016f);
        }
        
        // Sync back to USD
        world.syncToUsd();
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();
            
        std::cout << "Optimized memory layout: " << duration << " ms" << std::endl;
    }
}

} // namespace SchemaMemoryLayout
