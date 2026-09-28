#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>
#include <functional>
#include "Types.hpp"

namespace madfam::geom::cad {

// Forward declaration for internal shape storage
class InternalShape;

/**
 * @brief Central registry for all shapes in memory
 * 
 * Manages shape lifecycle, provides O(1) lookup, and tracks memory usage.
 * Thread-safe for use with Web Workers / pthreads.
 */
class ShapeRegistry {
public:
    static ShapeRegistry& instance();
    
    // Shape lifecycle
    std::string registerShape(std::unique_ptr<InternalShape> shape, ShapeType type);
    bool hasShape(const std::string& id) const;
    InternalShape* getShape(const std::string& id);
    const InternalShape* getShape(const std::string& id) const;
    ShapeHandle getHandle(const std::string& id) const;
    bool disposeShape(const std::string& id);
    void disposeAll();
    
    // Bulk operations
    std::vector<ShapeHandle> getAllHandles() const;
    std::vector<std::string> getShapeIds() const;
    
    // Memory management
    size_t getShapeCount() const;
    size_t getEstimatedMemoryBytes() const;
    void setMemoryLimit(size_t bytes);
    void evictLRU(size_t targetBytes);  // Evict least-recently-used until under target
    
    // Cache management for zero-lag
    void cacheResult(const std::string& operationKey, const std::string& resultShapeId);
    std::optional<std::string> getCachedResult(const std::string& operationKey) const;
    void invalidateCache();
    void invalidateCacheFor(const std::string& shapeId);
    
    // Metrics
    struct Stats {
        size_t totalShapes;
        size_t totalMemoryBytes;
        size_t cacheHits;
        size_t cacheMisses;
        double averageOperationMs;
    };
    Stats getStats() const;
    void resetStats();
    void recordOperation(double durationMs);
    
    // Callbacks for monitoring
    using ShapeCreatedCallback = std::function<void(const ShapeHandle&)>;
    using ShapeDisposedCallback = std::function<void(const std::string&)>;
    void onShapeCreated(ShapeCreatedCallback cb);
    void onShapeDisposed(ShapeDisposedCallback cb);
    
private:
    ShapeRegistry();
    ~ShapeRegistry();
    ShapeRegistry(const ShapeRegistry&) = delete;
    ShapeRegistry& operator=(const ShapeRegistry&) = delete;
    
    std::string generateId();
    void updateAccessTime(const std::string& id);
    
    struct ShapeEntry {
        std::unique_ptr<InternalShape> shape;
        ShapeHandle handle;
        std::chrono::steady_clock::time_point lastAccess;
        size_t estimatedBytes;
    };
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, ShapeEntry> shapes_;
    std::unordered_map<std::string, std::string> operationCache_;  // opKey -> shapeId
    
    size_t nextId_ = 1;
    size_t memoryLimit_ = 512 * 1024 * 1024;  // 512MB default
    
    // Stats
    mutable size_t cacheHits_ = 0;
    mutable size_t cacheMisses_ = 0;
    std::vector<double> operationDurations_;
    
    // Callbacks
    std::vector<ShapeCreatedCallback> createdCallbacks_;
    std::vector<ShapeDisposedCallback> disposedCallbacks_;
};

/**
 * @brief RAII guard for automatic shape disposal
 */
class ShapeGuard {
public:
    explicit ShapeGuard(const std::string& shapeId);
    ~ShapeGuard();
    
    ShapeGuard(const ShapeGuard&) = delete;
    ShapeGuard& operator=(const ShapeGuard&) = delete;
    ShapeGuard(ShapeGuard&& other) noexcept;
    ShapeGuard& operator=(ShapeGuard&& other) noexcept;
    
    void release();  // Don't dispose on destruction
    const std::string& id() const { return shapeId_; }
    
private:
    std::string shapeId_;
    bool shouldDispose_ = true;
};

/**
 * @brief Internal shape wrapper (abstracts OCCT TopoDS_Shape or mesh)
 */
class InternalShape {
public:
    virtual ~InternalShape() = default;
    
    virtual ShapeType getType() const = 0;
    virtual BoundingBox getBoundingBox() const = 0;
    virtual double getVolume() const = 0;
    virtual double getSurfaceArea() const = 0;
    virtual Vector3 getCenterOfMass() const = 0;
    virtual size_t getEstimatedMemoryBytes() const = 0;
    virtual std::string computeHash() const = 0;
    
    // Tessellation (for visualization)
    virtual MeshData tessellate(const TessellateOptions& options) const = 0;
    
    // Clone for copy operations
    virtual std::unique_ptr<InternalShape> clone() const = 0;
    
#ifdef GC_USE_OCCT
    // OCCT-specific access (only available when OCCT is enabled)
    virtual void* getOCCTShape() = 0;
    virtual const void* getOCCTShape() const = 0;
#endif
};

} // namespace madfam::geom::cad
