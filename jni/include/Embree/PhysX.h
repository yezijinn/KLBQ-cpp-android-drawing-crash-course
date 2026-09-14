#pragma once

#include "foundation/PxTransform.h"
#include "foundation/PxVec3.h"
#include "foundation/PxVec4.h"
#include "driver.h"
#include "VectorTools.h"
#include "rtcore.h"
#include "Hack.h"

#include <cstdlib>
#include <iostream>
#include <thread>
#include <set>
#include <numeric>
#include <sstream>

#define PhysXPtr 0xb476f20
#define RigidActors 0x25E8
#define SQManager 0x2430

using namespace std;

template <typename T>
struct TArray
{
    uintptr_t base;
    int32_t count;
    int32_t max;

    [[nodiscard]] vector<T> ToVec() const
    {
        if (!IsValid())
        {
            return {};
        }
        std::vector<T> vec{};
        vec.resize(static_cast<size_t>(count));
        return vec;
    }

    T operator[](size_t u) const
    {
        return dr->Read<T>(base + u * sizeof(T));
    }

    [[nodiscard]] bool IsValid() const
    {
        return base && count > 0 && count <= max && max > 0;
    }
};

enum class PxGeometryType : int32_t
{
    eSPHERE        = 0, // 球体
    ePLANE         = 1, // 平面
    eCAPSULE       = 2, // 胶囊体（圆柱 + 半球两端）
    eBOX           = 3, // 盒子（立方体或矩形体）
    eCONVEXMESH    = 4, // 凸包网格（凸面多边形网格）
    eTRIANGLEMESH  = 5, // 三角形网格（可凹多边形，常用于静态物体）
    eHEIGHTFIELD   = 6, // 高度图（用于地形碰撞）
    eGEOMETRY_COUNT, //!< internal use only!
    eINVALID = -1	 //!< internal use only!
};

struct FIntVector2D {
    int X, Y;
};

struct FilterDataT
{
    uint32_t word0;
    uint32_t word1;
    uint32_t word2;
    uint32_t word3;
};

struct PrunerPayload
{
    uint64_t Shape;
    uint64_t Actor;
    bool operator==(const PrunerPayload& other) const {
        return Shape == other.Shape && Actor == other.Actor;
    }
    bool operator<(const PrunerPayload& other) const {
        return std::tie(Shape, Actor) < std::tie(other.Shape, other.Actor);
    }
};

struct PrunerPayloadHash {
    size_t operator()(const PrunerPayload& p) const {
        return std::hash<uint64_t>()(p.Shape) ^ (std::hash<uint64_t>()(p.Actor) << 1);
    }
};

struct Int64Hash {
    size_t operator()(const uint64_t& p) const {
        return std::hash<uint64_t>()(p);
    }
};

struct TriangleMeshData
{
    std::vector<physx::PxVec3> Vertices{};
    std::vector<uint32_t> Indices{};
    uint8_t Flags{};
    FilterDataT QueryFilterData{};
    FilterDataT SimulationFilterData{};
    PrunerPayload UniqueKey1;
    uint64_t UniqueKey2;
    PrunerPayload UniqueKey3;
    PxGeometryType Type{};
    physx::PxTransform Transform;
};

namespace PhysX
{
    enum class PxConcreteType : uint16_t
    {

        eUNDEFINED,

        eHEIGHTFIELD,
        eCONVEX_MESH,
        eTRIANGLE_MESH_BVH33,
        eTRIANGLE_MESH_BVH34,
        eCLOTH_FABRIC,

        eRIGID_DYNAMIC,
        eRIGID_STATIC,
        eSHAPE,
        eMATERIAL,
        eCONSTRAINT,
        eCLOTH,
        ePARTICLE_SYSTEM,
        ePARTICLE_FLUID,
        eAGGREGATE,
        eARTICULATION,
        eARTICULATION_LINK,
        eARTICULATION_JOINT,
        ePRUNING_STRUCTURE,

        ePHYSX_CORE_COUNT,
        eFIRST_PHYSX_EXTENSION = 256,
        eFIRST_VEHICLE_EXTENSION = 512,
        eFIRST_USER_EXTENSION = 1024
    };

    enum class PxBaseFlag : uint16_t
    {
        eOWNS_MEMORY = (1 << 0),
        eIS_RELEASABLE = (1 << 1)
    };

    enum class PxRigidBodyFlag : uint8_t
    {

        eKINEMATIC = (1 << 0), //!< Enable kinematic mode for the body.

        eUSE_KINEMATIC_TARGET_FOR_SCENE_QUERIES = (1 << 1),

        eENABLE_CCD = (1 << 2), //!< Enable CCD for the body.

        eENABLE_CCD_FRICTION = (1 << 3),

        eENABLE_POSE_INTEGRATION_PREVIEW = (1 << 4),

        eENABLE_SPECULATIVE_CCD = (1 << 5),

        eENABLE_CCD_MAX_CONTACT_IMPULSE = (1 << 6)
    };

    struct PxMatrix3x3T
    {

        physx::PxVec3 column0{};
        physx::PxVec3 column1{};
        physx::PxVec3 column2{};

        PxMatrix3x3T() = default;

        explicit PxMatrix3x3T(const physx::PxVec4& q)
        {
            const float x = q.x;
            const float y = q.y;
            const float z = q.z;
            const float w = q.w;

            const float x2 = x + x;
            const float y2 = y + y;
            const float z2 = z + z;

            const float xx = x2 * x;
            const float yy = y2 * y;
            const float zz = z2 * z;

            const float xy = x2 * y;
            const float xz = x2 * z;
            const float xw = x2 * w;

            const float yz = y2 * z;
            const float yw = y2 * w;
            const float zw = z2 * w;

            column0 = physx::PxVec3(1.0f - yy - zz, xy + zw, xz - yw);
            column1 = physx::PxVec3(xy - zw, 1.0f - xx - zz, yz + xw);
            column2 = physx::PxVec3(xz + yw, yz - xw, 1.0f - xx - yy);
        }

        PxMatrix3x3T(const physx::PxVec3& col0, const physx::PxVec3& col1, const physx::PxVec3& col2)
                : column0(col0), column1(col1), column2(col2)
        {
        }

        [[nodiscard]] PxMatrix3x3T getTranspose() const {
            const physx::PxVec3 v0(column0.x, column1.x, column2.x);
            const physx::PxVec3 v1(column0.y, column1.y, column2.y);
            const physx::PxVec3 v2(column0.z, column1.z, column2.z);

            return {v0, v1, v2};
        }

        [[nodiscard]] physx::PxVec3 transform(const physx::PxVec3& other) const
        {
            return column0 * other.x + column1 * other.y + column2 * other.z;
        }

         PxMatrix3x3T operator*(const PxMatrix3x3T& other) const
        {
            // Rows from this <dot> columns from other
            // column0 = transform(other.column0) etc
            return {transform(other.column0), transform(other.column1), transform(other.column2)};
        }

        physx::PxVec3 operator*(const physx::PxVec3& vec) const
        {
            return transform(vec);
        }
    };

    //struct PxsRigidCore
    struct PxsRigidCoreT
    {
        alignas(16)
        physx::PxTransform mBodyToWorld alignas(16);
        PxRigidBodyFlag Flags{};// API body flags
        uint8_t m_idt_body_to_actor{};// PT: true if PxsBodyCore::body2Actor is identity
        uint16_t m_solver_iteration_counts{};//vel iters are in low word and pos iters in high word.
    };

    struct BodyCoreT
    {
        char mPad[0x10]{};
        alignas(16) PxsRigidCoreT mCore {};
        alignas(16) physx::PxTransform mBodyToActor;
    };

    // rigid base class
    struct BodyT
    {
        char mPad[0x60]{};
        uint64_t mScene{};
        uint64_t mControlState{};
        uint64_t mStreamPtr{};
        BodyCoreT mRigid{};
    };


    struct PxActorT
    {
        char mPad[0x8]{};
        PxConcreteType mType{};// concrete type identifier - see PxConcreteType.
        PxBaseFlag mBaseFlags{};// internal flags
    };

    struct PxGeometryT
    {
        PxGeometryType mType{};
    };

    struct PxBoxGeometry : PxGeometryT
    {

        // 盒子宽度、高度和深度的一半。
        physx::PxVec3 mHalfExtents{};

        bool valid() const {

            if (mType != PxGeometryType::eBOX) return false;
            // if (!halfExtents.isFinite())
            //	return false;
            if (mHalfExtents.x <= 0.0f || mHalfExtents.y <= 0.0f || mHalfExtents.z <= 0.0f) return false;

            return true;
        }
    };

    struct PxSphereGeometryT : PxGeometryT
    {
        float mRadius{};

        bool valid()
        {

            if (mType != PxGeometryType::eSPHERE) return false;
            // if (!halfExtents.isFinite())
            //	return false;
            if (mRadius <= 0.f) return false;
            return true;
        }
    };

    struct PxCapsuleGeometryT : PxGeometryT
    {

        float mRadius{};
        float mHalfHeight{};

        bool valid()
        {

            if (mType != PxGeometryType::eCAPSULE)
                return false;
            // if (!halfExtents.isFinite())
            //	return false;
            if (mRadius <= 0.f || mHalfHeight <= 0.f)
                return false;

            return true;
        }
    };

    struct PxMeshScale
    {
        physx::PxVec3 transform(const physx::PxVec3& v) const
        {
            return rotation.rotateInv(scale.multiply(rotation.rotate(v)));
        }

        physx::PxVec3 scale;	  //!< A nonuniform scaling
        physx::PxQuat rotation; //!< The orientation of the scaling axes
    };

    struct CenterExtentsT
    {

        physx::PxVec3 mCenter{};
        physx::PxVec3 mExtents{};
    };

    struct PxPlaneT
    {
        physx::PxVec3 n; //!< The normal to the plane
        float d{};  //!< The distance from the origin
        bool operator==(const PxPlaneT& p) const
        {
            return n == p.n && d == p.d;
        }
    };

    struct HullPolygonDataT
    {
        PxPlaneT mPlane; //!< Plane equation for this polygon	//Could drop 4th elem as it can be computed from any vertex as: d = - p.dot(n);
        uint16_t mVRef8{};   //!< Offset of vertex references in hull vertex data (CS: can we assume indices are tightly packed and offsets are ascending?? DrawObjects makes and uses this assumption)
        uint8_t mNbVerts{};  //!< Number of vertices/edges in the polygon
        uint8_t mMinIndex{}; //!< Index of the polygon vertex that has minimal projection along this plane's normal.
    };

    template <typename storageType, storageType bitMask>
    class PxBitAndDataT
    {
    public:
        PxBitAndDataT(const physx::PxEMPTY)
        {
        }
        PxBitAndDataT() : mData(0)
        {
        }
        PxBitAndDataT(storageType data, bool bit = false)
        {
            mData = bit ? storageType(data | bitMask) : data;
        }

        operator storageType() const
        {
            return storageType(mData & ~bitMask);
        }
        void setBit()
        {
            mData |= bitMask;
        }
        void clearBit()
        {
            mData &= ~bitMask;
        }
        storageType isBitSet() const
        {
            return storageType(mData & bitMask);
        }

    protected:
        storageType mData;
    };
    typedef PxBitAndDataT<unsigned char, 0x80> PxBitAndByte;
    typedef PxBitAndDataT<unsigned short, 0x8000> PxBitAndWord;
    typedef PxBitAndDataT<unsigned int, 0x80000000> PxBitAndDword;
    template<uint8_t TNumBytes>
    struct PxPadding
    {
        uint8_t mPadding[TNumBytes]{};
        PxPadding()
        {
            for ( uint8_t idx =0; idx < TNumBytes; ++idx )
                mPadding[idx] = 0;
        }
    };
    struct ConvexHullDataT
    {
        CenterExtentsT mAABB{};//+0x20
        physx::PxVec3 m_center_of_mass{};//+0x38
        PxBitAndWord mNbEdges{};//
        uint8_t HullVerticesNb{};//+0x46
        uint8_t PolygonsNb{};//+0x47
        HullPolygonDataT* mPolygons{};//0x48

        const physx::PxVec3* getHullVertices() const //!< Convex hull vertices
        {
            const char* tmp = reinterpret_cast<const char*>(mPolygons);
            tmp += sizeof(HullPolygonDataT) * PolygonsNb;
            return reinterpret_cast<const physx::PxVec3*>(tmp);
        }

        const uint8_t* getVertexData8() const //!< Vertex indices indexed by hull polygons
        {
            const char* tmp = reinterpret_cast<const char*>(mPolygons);
            tmp += sizeof(HullPolygonDataT) * PolygonsNb;
            tmp += sizeof(physx::PxVec3) * HullVerticesNb;
            tmp += sizeof(uint8_t) * mNbEdges * 2;
            tmp += sizeof(uint8_t) * HullVerticesNb * 3;
            if (mNbEdges.isBitSet())
                tmp += sizeof(uint16_t) * mNbEdges * 2;
            return reinterpret_cast<const uint8_t*>(tmp);
        }

        const uint8_t* getIndexBuffer() const
        {

            int64_t v1;			// r10
            const char* result; // rax

            v1 = mNbEdges & 0x7FFF;
            result = reinterpret_cast<const char*>(mPolygons) + 15 * HullVerticesNb + 2 * v1;
            if ((mNbEdges & 0x8000u) != 0)
                result += 4 * v1;
            return reinterpret_cast<const uint8_t*>(result);
        }

        const uint8_t* getFacesByEdges8() const //!< for each edge, gives 2 adjacent polygons; used by convex-convex code to come up with all the convex' edge normals.
        {

            const char* tmp = reinterpret_cast<const char*>(mPolygons);
            tmp += 20 * PolygonsNb;
            tmp += 12 * HullVerticesNb;
            return reinterpret_cast<const uint8_t*>(tmp);
        }

        const uint8_t* getFacesByVertices8() const //!< for each edge, gives 2 adjacent polygons; used by convex-convex code to come up with all the convex' edge normals.
        {

            const char* tmp = reinterpret_cast<const char*>(mPolygons);
            tmp += 20 * PolygonsNb;
            tmp += 12 * HullVerticesNb;
            tmp += 1 * mNbEdges * 2;
            return reinterpret_cast<const uint8_t*>(tmp);
        }
    };

    struct PxConvexMeshT
    {
    };
    struct ConvexMeshT : PxConvexMeshT
    {
        char mPad[0x8]{};
        PxConcreteType mType{};
        PxBaseFlag mBaseFlags{};
        uint64_t mRefCountableVfptr{};
        int32_t mRefCount{};
        ConvexHullDataT HullData{};
        uint32_t mNb{};// ### PT: added for serialization. Try to remove later?
    };

    struct PxConvexMeshGeometryT : PxGeometryT
    {
        PxMeshScale Scale{};//!< The scaling transformation (from vertex space to shape space).
        ConvexMeshT* ConvexMesh{};//!< A reference to the convex mesh object.
        float mMaxMargin{};//!< Max shrunk amount permitted by PCM contact gen
        uint8_t mMeshFlags{};//!< Mesh flags.
        //char mFlagPad[0x3]{};
        PxPadding<3> paddingFromFlags;	//!< padding for mesh flags
    };

    // Possible optimization: align the whole struct to cache line
    struct TriangleMeshT
    {
        char mPad[0x8]{};//
        PxConcreteType mType{};
        PxBaseFlag mBaseFlags{};
        uint64_t mRefCountableVfptr{};
        int32_t mRefCount{};
        uint32_t mNbVertices{};//
        uint32_t mNbTriangles{};//!< 16 (<= 0xffff #vertices) or 32 bit trig indices (mNbTriangles * 3)
        physx::PxVec3* Vertices{};//
        void* mTriangles{};//
        CenterExtentsT mAABB{};//
        uint8_t* mExtraTrigData{};// PT: WARNING: bounds must be followed by at least 32bits of data for safe SIMD loading
        float mGeomEpsilon{};//!< see comments in cooking code referencing this variable
        uint8_t Flags{};//!< Flag whether indices are 16 or 32 bits wide
    };

    struct PxTriangleMeshGeometryT : PxGeometryT
    {
        PxMeshScale Scale{};//!< The scaling transformation.
        uint8_t mMeshFlags{};//!< Mesh flags.
        PxPadding<3> paddingFromFlags;	//!< padding for mesh flags
        TriangleMeshT* mTriangleMesh{};//!< A reference to the mesh object.
    };

    struct PxHeightFieldSampleT
    {

        int16_t mHeight{};
        PxBitAndByte mMaterialIndex0{};
        PxBitAndByte mMaterialIndex1{};
    };

    struct HeightFieldDataT
    {
        CenterExtentsT mAABB{};
        uint32_t Rows{};// PT: WARNING: don't change this member's name (used in ConvX)
        uint32_t Columns{};// PT: WARNING: don't change this member's name (used in ConvX)
        float mRowLimit{};// PT: to avoid runtime int-to-float conversions on Xbox
        float mColumnLimit{};// PT: to avoid runtime int-to-float conversions on Xbox
        float mNbColumns{};// PT: to avoid runtime int-to-float conversions on Xbox
        PxHeightFieldSampleT* mSamples{};// PT: WARNING: don't change this member's name (used in ConvX)
        float mThickness{};
        float mConvexEdgeThreshold{};
        uint16_t mFlags{};
        uint8_t mFormat{};
    };

    struct HeightFieldT
    {
        char mPad[0x8]{};
        PxConcreteType mType{};
        PxBaseFlag mBaseFlags{};
        uint64_t mRefCountableVfptr{};
        int32_t mRefCount{};
        HeightFieldDataT mData{};
        uint32_t mSampleStride{};
        uint32_t mNbSamples{};// PT: added for platform conversion. Try to remove later.
        float mMinHeight{};
        float mMaxHeight{};
        int32_t mModifyCount{};
        // methods
        void* mMeshFactory{};// PT: changed to pointer for serialization
    };

    struct PxHeightFieldGeometryT : PxGeometryT
    {
        HeightFieldT* mHeightField{};
        float HeightScale{};
        float RowScale{};
        float ColumnScale{};
        int8_t Flags{};
        PxPadding<3> paddingFromFlags;	//!< padding for mesh flags.
    };

    struct GeometryUnionT
    {

        union
        {
            void* alignment; // PT: Makes sure the class is at least aligned to pointer size. See DE6803.
            uint8_t box[16];
            uint8_t sphere[8];
            uint8_t capsule[12];
            uint8_t plane[4];
            uint8_t convex[64];
            uint8_t mesh[80];
            uint8_t heightfield[56];
            uint8_t invalid[4];
        } mGeometry;

        PxGeometryType getType() const { return reinterpret_cast<const PxGeometryT&>(mGeometry).mType; }
    };

    struct PxShapeCoreT
    {

        alignas(16) physx::PxTransform transform;
        float contactOffset;
        uint8_t mShapeFlags;			// !< API shape flags	// PT: TODO: use PxShapeFlags here. Needs to move flags to separate file.
        uint8_t mOwnsMaterialIdxMemory; // PT: for de-serialization to avoid deallocating material index list. Moved there from Sc::ShapeCore (since one byte was free).
        uint16_t materialIndex;
        GeometryUnionT geometry;
    };

    struct ShapeCoreT
    {

        FilterDataT QueryFilterData{};
        FilterDataT SimulationFilterData{};
        alignas(16) PxShapeCoreT mCore {};
        float mRestOffset{};

        PxGeometryType getGeometryType() const { return mCore.geometry.getType(); }
        const physx::PxTransform getShape2Actor() const { return mCore.transform; }
    };

    struct ShapeT
    {
        char mPad[0x30]{};
        uint64_t mScene{};
        uint32_t mControlState{};
        uint64_t mStreamPtr{};
        ShapeCoreT ShapeCore{};
        const PxGeometryT& GetGeometry() const { return reinterpret_cast<const PxGeometryT&>(ShapeCore.mCore.geometry.mGeometry); }
    };

    struct PxBounds3
    {
        physx::PxVec3 min, max;
    };


    // Pool of AABBs
    struct PruningPoolT
    {
        uint32_t mNbObjects;
        uint32_t mMaxNbObjects;
        PxBounds3* mWorldBoxes;
        PrunerPayload* mObjects;
    };

    //// PT: extended pruner structure. We might want to move the additional data to the pruner itself later.
    struct PrunerExtT
    {
        uint64_t mPruner{};
        char mPad[0x24]{};
        uint32_t mTimestamp{};
    };

    struct NpSceneT
    {
        char mPad[SQManager]{};
        PrunerExtT exts[2];
    };

    struct ShapeDataT
    {
        ShapeT Shape{};
        BodyT Actor{};
        PxGeometryType mType{};
        uint8_t m_shape_flags{};
        PxMeshScale Scale{};
        PrunerPayload UniqueKey{};

        // ConvexData
        ConvexMeshT ConvexMesh{};
        std::vector<HullPolygonDataT> polygons{};
        std::vector<uint8_t> ConvexIndices{};

        // TriangleData
        std::vector<uint32_t> Indices{};
        std::vector<uint16_t> SmallIndices{};
        TriangleMeshT TriangleMesh{};

        // ConvexData & TriangleData
        std::vector<physx::PxVec3> Vertices{};

        // HeightFieldData
        std::vector<PxHeightFieldSampleT> mSamples{};
        HeightFieldT mHeightField{};
    };

}// namespace physx

template<typename T>
void AddReadVecScatter(uint64_t address, size_t size, std::vector<T>* vec)
{
    if (!size) return;
    vec->resize(size);
    dr->Read(address, vec->data(), sizeof(T) * size);
}

template <class _Pr>
std::vector<TriangleMeshData> GetMeshData(
        std::vector<PrunerPayload>& objects,
        _Pr filter,
        bool isDynamic = false,
        bool autoTransform = true
) {
    std::vector<PhysX::ShapeDataT> ShapeDatas{};
    std::vector<TriangleMeshData> TriangleMeshDatas{};
    // Using index iteration instead of reference

    ShapeDatas.resize(objects.size());
    for (size_t i = 0; i < objects.size(); i++)
    {
        auto obj = objects[i];
        ShapeDatas[i].UniqueKey = obj;
        dr->Read(obj.Shape, &ShapeDatas[i].Shape, sizeof(PhysX::ShapeT));
        dr->Read(obj.Actor, &ShapeDatas[i].Actor, sizeof(PhysX::BodyT));
    }

    for (auto& ShapeData : ShapeDatas)
    {
        ShapeData.m_shape_flags = ShapeData.Shape.ShapeCore.mCore.mShapeFlags;
        ShapeData.mType = ShapeData.Shape.ShapeCore.getGeometryType();
//printf("ShapeData.m_shape_flags %d, ShapeData.mType %d \n",ShapeData.m_shape_flags,ShapeData.mType);
    }

    ShapeDatas.erase(
            std::remove_if(ShapeDatas.begin(), ShapeDatas.end(), filter),ShapeDatas.end()
    );

    for (auto& ShapeData : ShapeDatas)
    {
        if (ShapeData.mType == PxGeometryType::eHEIGHTFIELD)
        {
            PhysX::PxHeightFieldGeometryT field_geometry = (PhysX::PxHeightFieldGeometryT&)ShapeData.Shape.GetGeometry();
            dr->Read((uint64_t)field_geometry.mHeightField, &ShapeData.mHeightField, sizeof(PhysX::HeightFieldT));
        }
        else if (ShapeData.mType == PxGeometryType::eCONVEXMESH)
        {
            PhysX::PxConvexMeshGeometryT convex_geometry = (PhysX::PxConvexMeshGeometryT&)ShapeData.Shape.GetGeometry();
            ShapeData.Scale = convex_geometry.Scale;
            dr->Read((uint64_t)convex_geometry.ConvexMesh, &ShapeData.ConvexMesh, sizeof(PhysX::ConvexMeshT));
        }
    }

    for (auto& shapeData : ShapeDatas)
    {
        if (shapeData.mType == PxGeometryType::eCONVEXMESH)
        {
            auto nbPolygons = shapeData.ConvexMesh.HullData.PolygonsNb;
            auto mPolygons = (uint64_t)shapeData.ConvexMesh.HullData.mPolygons;
            auto size = sizeof(PhysX::HullPolygonDataT);
            shapeData.polygons.resize(nbPolygons);
            for (uint32_t i = 0; i < nbPolygons; i++) {
                shapeData.polygons[i] = dr->Read<PhysX::HullPolygonDataT>(mPolygons + (i * size));
            }
        }
    }

    for (auto& ShapeData : ShapeDatas)
    {
        if (ShapeData.mType == PxGeometryType::eCONVEXMESH)
        {
            uint32_t indices_number = std::accumulate(ShapeData.polygons.begin(), ShapeData.polygons.end(), 0u,
                                                      [](uint32_t sum, const PhysX::HullPolygonDataT& polygon) {
                                                          return sum + polygon.mNbVerts;
                                                      });
            ShapeData.ConvexIndices.reserve(indices_number);
            AddReadVecScatter<uint8_t>((uint64_t)ShapeData.ConvexMesh.HullData.getVertexData8(), indices_number, &ShapeData.ConvexIndices);
        }
        else  if (ShapeData.mType == PxGeometryType::eTRIANGLEMESH)
        {
            auto& geometry = (PhysX::PxTriangleMeshGeometryT&)ShapeData.Shape.GetGeometry();
            const PhysX::PxTriangleMeshGeometryT& triangle_geometry = (PhysX::PxTriangleMeshGeometryT&)ShapeData.Shape.GetGeometry();
            ShapeData.Scale = triangle_geometry.Scale;
            ShapeData.TriangleMesh = dr->Read<PhysX::TriangleMeshT>((uintptr_t)triangle_geometry.mTriangleMesh);
        }
    }

    for (auto& ShapeData : ShapeDatas)
    {
        if (ShapeData.mType == PxGeometryType::eTRIANGLEMESH)
        {
            bool has16BitIndices = (ShapeData.TriangleMesh.Flags & 2U) ? true : false;
            AddReadVecScatter<physx::PxVec3>((uint64_t)ShapeData.TriangleMesh.Vertices, ShapeData.TriangleMesh.mNbVertices, &ShapeData.Vertices);
            if (has16BitIndices)
            {
                AddReadVecScatter<uint16_t >((uint64_t)ShapeData.TriangleMesh.mTriangles, ShapeData.TriangleMesh.mNbTriangles * 3, &ShapeData.SmallIndices);
            }
            else
            {
                AddReadVecScatter<uint32_t >((uint64_t)ShapeData.TriangleMesh.mTriangles, ShapeData.TriangleMesh.mNbTriangles * 3, &ShapeData.Indices);
            }
        }
        else if (ShapeData.mType == PxGeometryType::eHEIGHTFIELD)
        {
            const uint32_t nb = ShapeData.mHeightField.mNbSamples;
            AddReadVecScatter((uint64_t)ShapeData.mHeightField.mData.mSamples, nb, &ShapeData.mSamples);
        }
        else if (ShapeData.mType == PxGeometryType::eCONVEXMESH)
        {
            AddReadVecScatter<physx::PxVec3>((uint64_t)ShapeData.ConvexMesh.HullData.getHullVertices(), (size_t)ShapeData.ConvexMesh.HullData.HullVerticesNb, &ShapeData.Vertices);
        }
    }

//#pragma omp parallel for
    for (int i = 0; i < ShapeDatas.size(); i++)
    {
        auto& ShapeData = ShapeDatas[i];
        if (ShapeData.mType == PxGeometryType::eTRIANGLEMESH && !ShapeData.SmallIndices.empty())
        {
//这个位置有点问题 会导致 0
            const size_t size = ShapeData.SmallIndices.size();
            ShapeData.Indices.resize(size);
            std::transform(
                    ShapeData.SmallIndices.begin(),
                    ShapeData.SmallIndices.end(),
                    ShapeData.Indices.begin(),
                    [](uint16_t val) { return static_cast<uint32_t>(val); }
            );
            ShapeData.SmallIndices.clear();
            ShapeData.SmallIndices.shrink_to_fit();
        }
    }

//#pragma omp parallel for

    for (int i = 0; i < ShapeDatas.size(); i++)
    {
        const auto& ShapeDataItem = ShapeDatas[i];
        physx::PxTransform GlobalPose{};
        if (isDynamic) {
            GlobalPose = ShapeDataItem.Actor.mRigid.mCore.mBodyToWorld * ShapeDataItem.Actor.mRigid.mBodyToActor.getInverse();
        }
        else {
            GlobalPose = ShapeDataItem.Actor.mRigid.mCore.mBodyToWorld;
        }
        auto LocalPose = ShapeDataItem.Shape.ShapeCore.mCore.transform;
        auto CombinePose = GlobalPose * LocalPose;
        CombinePose.p = CombinePose.p/* + WorldLocation*/;//暂时不加
        auto UniqueKey = ShapeDataItem.UniqueKey;

        if (ShapeDataItem.mType == PxGeometryType::eBOX){
            const PhysX::PxBoxGeometry& boxGeometry = (PhysX::PxBoxGeometry&)ShapeDataItem.Shape.GetGeometry();
            auto halfExtents = boxGeometry.mHalfExtents;

            physx::PxVec3 vertices[8];
            vertices[0] = physx::PxVec3(-halfExtents.x, -halfExtents.y, -halfExtents.z);
            vertices[1] = physx::PxVec3( halfExtents.x, -halfExtents.y, -halfExtents.z);
            vertices[2] = physx::PxVec3( halfExtents.x,  halfExtents.y, -halfExtents.z);
            vertices[3] = physx::PxVec3(-halfExtents.x,  halfExtents.y, -halfExtents.z);
            vertices[4] = physx::PxVec3(-halfExtents.x, -halfExtents.y,  halfExtents.z);
            vertices[5] = physx::PxVec3( halfExtents.x, -halfExtents.y,  halfExtents.z);
            vertices[6] = physx::PxVec3( halfExtents.x,  halfExtents.y,  halfExtents.z);
            vertices[7] = physx::PxVec3(-halfExtents.x,  halfExtents.y,  halfExtents.z);

            if (autoTransform) {
                for (int i = 0; i < 8; i++)
                {
                    vertices[i] = CombinePose.transform(vertices[i]);
                }
            }
// Box indices for triangles
            uint32_t indices[] = {
                    // Front face
                    0, 1, 2,
                    0, 2, 3,
                    // Back face
                    4, 6, 5,
                    4, 7, 6,
                    // Top face
                    0, 4, 5,
                    0, 5, 1,
                    // Bottom face
                    2, 6, 7,
                    2, 7, 3,
                    // Right face
                    0, 3, 7,
                    0, 7, 4,
                    // Left face
                    1, 5, 6,
                    1, 6, 2 };

            TriangleMeshData mesh_data{};
            mesh_data.Vertices.assign(vertices, vertices + 8);
            mesh_data.Indices.assign(indices, indices + 36);
            mesh_data.Flags = ShapeDataItem.m_shape_flags;
            mesh_data.QueryFilterData = ShapeDataItem.Shape.ShapeCore.QueryFilterData;
            mesh_data.SimulationFilterData = ShapeDataItem.Shape.ShapeCore.SimulationFilterData;
            mesh_data.UniqueKey1 = UniqueKey;
            mesh_data.Type = PxGeometryType::eBOX;
            mesh_data.Transform = CombinePose;

//#pragma omp critical
            {
                TriangleMeshDatas.push_back(mesh_data);
            }
        }
        else if (ShapeDataItem.mType == PxGeometryType::eCAPSULE)
        {
            const PhysX::PxCapsuleGeometryT& capsuleGeometry = (const PhysX::PxCapsuleGeometryT&)ShapeDataItem.Shape.GetGeometry();
            float radius = capsuleGeometry.mRadius;
            float halfHeight = capsuleGeometry.mHalfHeight;

        }
        else if (ShapeDataItem.mType == PxGeometryType::eSPHERE)
        {
            const PhysX::PxSphereGeometryT& sphereGeometry = (const PhysX::PxSphereGeometryT&)ShapeDataItem.Shape.GetGeometry();
            float radius = sphereGeometry.mRadius;
            const PhysX::PxMeshScale& ScaleVal = ShapeDataItem.Scale;

        }
        else if (ShapeDataItem.mType == PxGeometryType::eTRIANGLEMESH)
        {
            TriangleMeshData MeshData{};
            PhysX::PxMeshScale ScaleVal = ShapeDataItem.Scale;
            const size_t vertexCount = ShapeDataItem.Vertices.size();
            MeshData.Vertices.resize(vertexCount);
            for (size_t i = 0; i < vertexCount; i++)
            {
                physx::PxVec3& vertex = MeshData.Vertices[i];
                vertex = ScaleVal.transform(ShapeDataItem.Vertices[i]);
                if (autoTransform) {
                    vertex = CombinePose.transform(vertex);
                }
            }
            MeshData.Indices = ShapeDataItem.Indices;
            MeshData.Flags = ShapeDataItem.m_shape_flags;
            MeshData.QueryFilterData = ShapeDataItem.Shape.ShapeCore.QueryFilterData;
            MeshData.SimulationFilterData = ShapeDataItem.Shape.ShapeCore.SimulationFilterData;
            MeshData.UniqueKey1 = UniqueKey;
            MeshData.Type = PxGeometryType::eTRIANGLEMESH;
            MeshData.Transform = CombinePose;
//#pragma omp critical
            {
                TriangleMeshDatas.push_back(MeshData);
            }
        }
        else if (ShapeDataItem.mType == PxGeometryType::eHEIGHTFIELD)
        {
            PhysX::PxHeightFieldGeometryT field_geometry = (PhysX::PxHeightFieldGeometryT&)ShapeDataItem.Shape.GetGeometry();

            auto NumRows = ShapeDataItem.mHeightField.mData.Rows;
            auto NumColumns = ShapeDataItem.mHeightField.mData.Columns;
            auto ColumnScale = field_geometry.ColumnScale;
            auto RowScale = field_geometry.RowScale;
            auto HeightScale = field_geometry.HeightScale;

            TriangleMeshData MeshData{};

            if (NumRows == 0 || NumColumns == 0 || ShapeDataItem.mSamples.empty()) {
// fix unsign int
                continue;
            }

            for (uint32_t row = 0; row < NumRows - 1; row++) {
                for (uint32_t col = 0; col < NumColumns - 1; col++) {
                    uint32_t idx00 = row * NumColumns + col;
                    uint32_t idx10 = idx00 + 1;
                    uint32_t idx01 = idx00 + NumColumns;
                    uint32_t idx11 = idx01 + 1;


                    auto sample00 = ShapeDataItem.mSamples[idx00];
                    auto sample10 = ShapeDataItem.mSamples[idx10];
                    auto sample01 = ShapeDataItem.mSamples[idx01];
                    auto sample11 = ShapeDataItem.mSamples[idx11];
                    physx::PxVec3 v00, v10, v01, v11;
                    v00 = { row * RowScale, sample00.mHeight * HeightScale, col * ColumnScale };
                    v10 = { row * RowScale, sample10.mHeight * HeightScale, (col + 1) * ColumnScale };
                    v01 = { (row + 1) * RowScale, sample01.mHeight * HeightScale, col * ColumnScale };
                    v11 = { (row + 1) * RowScale, sample11.mHeight * HeightScale, (col + 1) * ColumnScale };

                    if (autoTransform)
                    {
                        v00 = CombinePose.transform(v00);
                        v10 = CombinePose.transform(v10);
                        v01 = CombinePose.transform(v01);
                        v11 = CombinePose.transform(v11);
                    }

                    MeshData.Vertices.push_back(v00);
                    MeshData.Vertices.push_back(v10);
                    MeshData.Vertices.push_back(v01);
                    MeshData.Vertices.push_back(v11);

                    size_t base_idx = (row * (NumColumns - 1) + col) * 4;

                    MeshData.Indices.push_back(base_idx);
                    MeshData.Indices.push_back(base_idx + 1);
                    MeshData.Indices.push_back(base_idx + 2);

                    MeshData.Indices.push_back(base_idx + 1);
                    MeshData.Indices.push_back(base_idx + 3);
                    MeshData.Indices.push_back(base_idx + 2);
                }
            }

            MeshData.Flags = ShapeDataItem.m_shape_flags;
            MeshData.QueryFilterData = ShapeDataItem.Shape.ShapeCore.QueryFilterData;
            MeshData.SimulationFilterData = ShapeDataItem.Shape.ShapeCore.SimulationFilterData;
            MeshData.UniqueKey1 = UniqueKey;
            MeshData.UniqueKey2 = (uint64_t)((PhysX::PxHeightFieldGeometryT&)ShapeDataItem.Shape.GetGeometry()).mHeightField;
            MeshData.Type = PxGeometryType::eHEIGHTFIELD;
            MeshData.Transform = CombinePose;

//#pragma omp critical
            {
                TriangleMeshDatas.push_back(MeshData);
            }

        }
        else if (ShapeDataItem.mType == PxGeometryType::eCONVEXMESH)
        {
//凸包网格
//PxConvexMeshGeometryT convex_geometry = (PxConvexMeshGeometryT&)ShapeDataItem.Shape.GetGeometry();
            const PhysX::PxMeshScale ScaleVal = ShapeDataItem.Scale;
            const uint8_t nbVertices = ShapeDataItem.ConvexMesh.HullData.HullVerticesNb;
            const uint8_t nbPolygons = ShapeDataItem.ConvexMesh.HullData.PolygonsNb;
            TriangleMeshData MeshData{};
            MeshData.Vertices = ShapeDataItem.Vertices;
            const size_t vertexCount = MeshData.Vertices.size();
            for (size_t i = 0; i < vertexCount; i++) {
//MeshData.Vertices[i] = ScaleVal.transform(MeshData.Vertices[i]);
                if (autoTransform) {
                    MeshData.Vertices[i] = CombinePose.transform(ScaleVal.transform(MeshData.Vertices[i]));
                }
            }
            for (const auto& polygon : ShapeDataItem.polygons) {
                for (uint16_t j = 0; j < polygon.mNbVerts - 2; j++) {
                    uint32_t index_idx0 = polygon.mVRef8 + 0;
                    uint32_t index_idx1 = polygon.mVRef8 + j + 1;
                    uint32_t index_idx2 = polygon.mVRef8 + j + 2;

                    if (index_idx0 >= ShapeDataItem.ConvexIndices.size() || index_idx2 >= ShapeDataItem.ConvexIndices.size()) continue;
                    uint32_t idx0 = ShapeDataItem.ConvexIndices[index_idx0];
                    uint32_t idx1 = ShapeDataItem.ConvexIndices[index_idx1];
                    uint32_t idx2 = ShapeDataItem.ConvexIndices[index_idx2];

                    MeshData.Indices.push_back(idx0);
                    MeshData.Indices.push_back(idx1);
                    MeshData.Indices.push_back(idx2);
                }
            }
            MeshData.Flags = ShapeDataItem.m_shape_flags;
            MeshData.QueryFilterData = ShapeDataItem.Shape.ShapeCore.QueryFilterData;
            MeshData.SimulationFilterData = ShapeDataItem.Shape.ShapeCore.SimulationFilterData;
            MeshData.UniqueKey1 = UniqueKey;
            MeshData.Type = PxGeometryType::eCONVEXMESH;
            MeshData.Transform = CombinePose;

            {
                TriangleMeshDatas.push_back(MeshData);
            }
        }
    }
    return TriangleMeshDatas;
}

inline std::vector<PrunerPayload> CollectAllDynamicRigidActorShapes() {
    std::vector<PrunerPayload> result;
    uint64_t PhysxInstancePtr = dr->Read<uint64_t>(AppBase.libUE4 + PhysXPtr);
    TArray<uint64_t> physx_scenes = dr->Read<TArray<uint64_t>>(PhysxInstancePtr + 0x8);//+0x8
    for(int i = 0; i < physx_scenes.count; i++){
        uint64_t scenes_ptr = dr->Read<uint64_t>(physx_scenes.base + i * sizeof(uint64_t));
        TArray<uint64_t> actors = dr->Read<TArray<uint64_t>>(scenes_ptr + RigidActors);
        for(int j = 0; j < actors.count; j++){
            uint64_t actor_ptr = dr->Read<uint64_t>(actors.base + j * sizeof(uint64_t));
            uint16_t Actor_Type = dr->Read<uint16_t>(actor_ptr + 0x8);
            if (Actor_Type != 6) continue;
            uint64_t shape_manager = actor_ptr + 0x28;
            uint64_t shape_ptr = dr->Read<uint64_t>(shape_manager + 0x0); // m_single
            result.push_back({.Shape = shape_ptr, .Actor = actor_ptr});
        }
    }
    return result;
}


inline std::vector<PrunerPayload> CollectAllHeightRigidActorShapes() {
    std::vector<PrunerPayload> result;
    uint64_t PhysxInstancePtr = dr->Read<uint64_t>(AppBase.libUE4 + PhysXPtr);
    TArray<uint64_t> physx_scenes = dr->Read<TArray<uint64_t>>(PhysxInstancePtr + 0x8);//+0x8
    for(int i = 0; i < physx_scenes.count; i++){
        uint64_t scenes_ptr = dr->Read<uint64_t>(physx_scenes.base + i * sizeof(uint64_t));
        TArray<uint64_t> actors = dr->Read<TArray<uint64_t>>(scenes_ptr + RigidActors);
        for(int j = 0; j < actors.count; j++){
            uint64_t actor_ptr = dr->Read<uint64_t>(actors.base + j *  sizeof(uint64_t));
            uint16_t Actor_Type = dr->Read<uint16_t>(actor_ptr + 0x8);
            if (Actor_Type != 7) continue;
            uint64_t shape_manager = actor_ptr + 0x28;
            uint64_t shape_ptr = dr->Read<uint64_t>(shape_manager + 0x0); // m_single
            result.push_back(PrunerPayload{.Shape = shape_ptr, .Actor = actor_ptr});
        }
    }
    return result;
}

inline std::vector<PrunerPayload> CollectAllStaticRigidActorShapes() {
    std::vector<PrunerPayload> result;
    uint64_t PhysxInstancePtr = dr->Read<uint64_t>(AppBase.libUE4 + PhysXPtr);
    TArray<uint64_t> physx_scenes = dr->Read<TArray<uint64_t>>(PhysxInstancePtr + 0x8);//+0x8
    for(int i = 0; i < physx_scenes.count; i++) {
        uint64_t scenes_ptr = dr->Read<uint64_t>(physx_scenes.base + i * sizeof(uint64_t));
        TArray<uint64_t> actors = dr->Read<TArray<uint64_t>>(scenes_ptr + RigidActors);
        for (int j = 0; j < actors.count; j++) {
            uint64_t actor_ptr = dr->Read<uint64_t>(actors.base + j * sizeof(uint64_t));
            uint16_t Actor_Type = dr->Read<uint16_t>(actor_ptr + 0x8);
            if (Actor_Type != 7) continue;
            uint64_t shape_manager = actor_ptr + 0x28;
            uint64_t shape_ptr = dr->Read<uint64_t>(shape_manager + 0x0); // m_single
            result.push_back({.Shape = shape_ptr, .Actor = actor_ptr});
        }
    }
    return result;
}

inline std::vector<TriangleMeshData> LoadDynamicRigidShape(
        std::set<PrunerPayload>& currentSceneObjects,
        std::unordered_map<PrunerPayload, physx::PxTransform, PrunerPayloadHash>& cache,
        std::unordered_map<PrunerPayload, uint64_t, PrunerPayloadHash>& ptrCache,
        std::set<PrunerPayload>& willRemoveObjects
){
    std::vector<PrunerPayload> mObjects = CollectAllDynamicRigidActorShapes();//第一个Shape 第二个 Actor
    std::set<uint64_t> queryActor{};
    for (const auto& obj : mObjects) {
        queryActor.insert(obj.Actor);
    }
    std::unordered_map<uint64_t, PhysX::BodyT> actorPos{};
    std::unordered_map<uint64_t, PhysX::ShapeT> shapePos{};
    actorPos.reserve(queryActor.size());
    shapePos.reserve(mObjects.size());

    for (const auto& actor : queryActor)
    {
        actorPos[actor] = {};
        dr->Read(actor, &actorPos[actor], sizeof(PhysX::BodyT));
    }

    for (const auto& obj : mObjects) {
        shapePos[obj.Shape] = {};
        dr->Read(obj.Shape, &shapePos[obj.Shape], sizeof(PhysX::ShapeT));
    }

    std::set<PrunerPayload> removeObjects{};
    std::vector<PrunerPayload> addObjects{};
    for (const auto& obj : mObjects) {
        auto& body = actorPos[obj.Actor];
        auto& shape = shapePos[obj.Shape];
        physx::PxTransform GlobalPose = (body.mRigid.mCore.mBodyToWorld * body.mRigid.mBodyToActor.getInverse()) * shape.ShapeCore.mCore.transform;

        auto it = cache.find(obj);
        if (it != cache.end()) {
            const float tolerance = 0.1f;

            const physx::PxTransform& a = it->second;
            const physx::PxTransform& b = GlobalPose;
            bool positionChanged =
                    std::abs(a.p.x - b.p.x) > tolerance ||
                    std::abs(a.p.y - b.p.y) > tolerance ||
                    std::abs(a.p.z - b.p.z) > tolerance;

            bool rotationChanged =
                    std::abs(a.q.x - b.q.x) > tolerance ||
                    std::abs(a.q.y - b.q.y) > tolerance ||
                    std::abs(a.q.z - b.q.z) > tolerance ||
                    std::abs(a.q.w - b.q.w) > tolerance;

            if (positionChanged || rotationChanged) {
                // 发生改变，移除对象
                currentSceneObjects.erase(obj);
                willRemoveObjects.insert(obj);
            }
        }
        cache[obj] = GlobalPose;
        auto ptrIt = ptrCache.find(obj);
        auto trianglePtr = (uint64_t)((PhysX::PxTriangleMeshGeometryT&)shape.GetGeometry()).mTriangleMesh;
        if (ptrIt != ptrCache.end()) {
            if (ptrIt->second != trianglePtr) {
// 指针变化 移除对象
                currentSceneObjects.erase(obj);
                willRemoveObjects.insert(obj);
            }
        }
        ptrCache[obj] = trianglePtr;
    }


// 删除缓存过期形状
    std::set<PrunerPayload> mObjectsSet(mObjects.begin(), mObjects.end());
    for (auto it = cache.begin(); it != cache.end(); ) {
        if (mObjectsSet.find(it->first) == mObjectsSet.end()) {
            it = cache.erase(it);//缓存中的这个物体不在场景中了，删除
        }
        else {
            ++it;
        }
    }

    for (auto it = ptrCache.begin(); it != ptrCache.end(); ) {
        if (mObjectsSet.find(it->first) == mObjectsSet.end()) {
            it = ptrCache.erase(it);//缓存中的这个物体不在场景中了 清除 mesh 指针
        }
        else {
            ++it;
        }
    }

// 根据缓存筛选出在指定范围内的 Shape
    std::set<PrunerPayload> result;
    for (const auto& [obj, pos] : cache) {
//if(currentPosition.Distance(pos.mPosition) < radius){
        result.insert(obj);
// }
    }
// 判断移除或新增的 Shape 是否已经在场景列表中
    std::copy_if(currentSceneObjects.begin(), currentSceneObjects.end(), std::inserter(removeObjects, removeObjects.begin()),[&result](const PrunerPayload& obj) {
        return result.find(obj) == result.end();
    });

    std::copy_if(result.begin(), result.end(), std::back_inserter(addObjects),[&currentSceneObjects](const PrunerPayload& obj) {
                     return currentSceneObjects.find(obj) == currentSceneObjects.end();
                 }
    );

// 将需要移除的 Shape 从当前场景中删除，并加入待移除列表
    for (const auto& obj : removeObjects) {
        currentSceneObjects.erase(obj);
        willRemoveObjects.insert(obj);
    }

    for (const auto& obj : addObjects) {
        currentSceneObjects.insert(obj);
    }

    return GetMeshData(addObjects,
                       [](const PhysX::ShapeDataT& shape_data) {
                           PxGeometryType type = shape_data.mType;
                           uint8_t Flags = shape_data.m_shape_flags;
                           return !(type == PxGeometryType::eTRIANGLEMESH || type == PxGeometryType::eBOX || type == PxGeometryType::eCONVEXMESH);
                       },
                       true
    );
}


inline std::vector<TriangleMeshData> RefreshDynamicLoadHeightField(
        uint32_t& lastTimestamp,
        std::set<PrunerPayload>& UniqueKeySet,
        std::set<PrunerPayload>& HeightFieldSet,
        std::set<uint64_t>& HeightFieldSamplePtrSet,
        std::set<uint64_t>& RemoveHeightFieldKey
) {
    uint64_t PhysxInstancePtr = dr->Read<uint64_t>(AppBase.libUE4 + PhysXPtr);
    auto px_scene_arr_ptr = dr->Read<uint64_t>(PhysxInstancePtr + 0x8);
    auto px_scene_ptr = dr->Read<uint64_t>(px_scene_arr_ptr);
    auto scene = dr->Read<PhysX::NpSceneT>(px_scene_ptr);
    // SQManager 偏移错误时时间戳可能读到 0：0 不拦截，避免"永远不加载"
    if (scene.exts[0].mTimestamp != 0 && scene.exts[0].mTimestamp == lastTimestamp) {
        return std::vector<TriangleMeshData>{};
    }
    lastTimestamp = scene.exts[0].mTimestamp;
    std::vector<PrunerPayload> mObjects = CollectAllHeightRigidActorShapes();
    std::set<PrunerPayload> newUnionKeySet(mObjects.begin(), mObjects.end());
    std::vector<PrunerPayload> newObjects;
    std::copy_if(mObjects.begin(), mObjects.end(), std::back_inserter(newObjects),[&UniqueKeySet, &HeightFieldSet](const PrunerPayload& obj) {
    // if current ptr not contains or current ptr is height field, load it data!
        return UniqueKeySet.find(obj) == UniqueKeySet.end() || HeightFieldSet.find(obj) != HeightFieldSet.end();
    });

    UniqueKeySet.insert(newObjects.begin(), newObjects.end());

    std::set<PrunerPayload> RemoveKey{};

    std::set_difference(UniqueKeySet.begin(), UniqueKeySet.end(),newUnionKeySet.begin(), newUnionKeySet.end(),std::inserter(RemoveKey, RemoveKey.begin()));
    for (const auto& key : RemoveKey) {
        UniqueKeySet.erase(key);
    }
    std::set<uint64_t> currentHeightFieldPtrSet{};
    auto result = GetMeshData(newObjects,
                              [&HeightFieldSamplePtrSet, &currentHeightFieldPtrSet, &HeightFieldSet](const PhysX::ShapeDataT& shape_data) {
                                  PxGeometryType type = shape_data.mType;
                                  auto queryFlag = shape_data.Shape.ShapeCore.QueryFilterData;
                                  auto simFilter = shape_data.Shape.ShapeCore.SimulationFilterData;
                                  uint8_t Flags = shape_data.m_shape_flags;
                                  if (type == PxGeometryType::eHEIGHTFIELD)
                                  {
                                      PhysX::PxHeightFieldGeometryT field_geometry = (PhysX::PxHeightFieldGeometryT&)shape_data.Shape.GetGeometry();
                                      HeightFieldSet.insert(shape_data.UniqueKey);
                                      // klbqm: 高度场全部接收(不再限制 RowScale==200)
                                      {
                                          auto ptr = (uint64_t)field_geometry.mHeightField;
                                          currentHeightFieldPtrSet.insert(ptr);
                                          return !HeightFieldSamplePtrSet.insert(ptr).second;
                                      }
                                  }
                                  return true;
                              }
    );
    std::set_difference(HeightFieldSamplePtrSet.begin(), HeightFieldSamplePtrSet.end(),currentHeightFieldPtrSet.begin(), currentHeightFieldPtrSet.end(),std::inserter(RemoveHeightFieldKey, RemoveHeightFieldKey.begin()));
    for (const auto& key : RemoveHeightFieldKey) {
        HeightFieldSamplePtrSet.erase(key);
    }
    return result;
}

inline std::vector<TriangleMeshData> LoadShapeByRange(
        uint32_t& lastTimestamp,
        std::unordered_map<PrunerPayload, physx::PxTransform, PrunerPayloadHash>& cache,
        std::set<PrunerPayload>& currentSceneObjects,
        std::set<PrunerPayload>& willRemoveObjects,
        std::unordered_map<PrunerPayload, uint64_t, PrunerPayloadHash>& alwaysCheckShape
) {
    uint64_t PhysxInstancePtr = dr->Read<uint64_t>(AppBase.libUE4 + PhysXPtr);
    auto px_scene_arr_ptr = dr->Read<uint64_t>(PhysxInstancePtr + 0x8);
    auto px_scene_ptr = dr->Read<uint64_t>(px_scene_arr_ptr);
    auto scene = dr->Read<PhysX::NpSceneT>(px_scene_ptr);
    // SQManager 偏移错误时时间戳可能读到 0：0 不拦截，避免"永远不加载"
    if (scene.exts[0].mTimestamp != 0 && scene.exts[0].mTimestamp == lastTimestamp) {
        return std::vector<TriangleMeshData>{};
    }
    lastTimestamp = scene.exts[0].mTimestamp;
    std::vector<PrunerPayload> mObjects = CollectAllStaticRigidActorShapes();
    // 查询当前场景中未被缓存的 Shape，准备刷新缓存
    std::set<PrunerPayload> queryObjects{};
    std::set<PrunerPayload> mObjectsSet(mObjects.begin(), mObjects.end());
    for (const auto& obj : mObjectsSet) {
        if (cache.find(obj) == cache.end()) {
            queryObjects.insert(obj);
        }
    }

    std::set<uint64_t> queryActor{};
    for (const auto& obj : queryObjects) {
        queryActor.insert(obj.Actor);
    }

    std::unordered_map<uint64_t, PhysX::BodyT> actorPos{};
    std::unordered_map<uint64_t, PhysX::ShapeT> shapePos{};
    actorPos.reserve(queryActor.size());
    shapePos.reserve(queryObjects.size());

    for (const auto& actor : queryActor)
    {
        actorPos[actor] = {};
        dr->Read(actor, &actorPos[actor], sizeof(PhysX::BodyT));
    }

    for (const auto& obj : queryObjects) {
        shapePos[obj.Shape] = {};
        dr->Read(obj.Shape, &shapePos[obj.Shape], sizeof(PhysX::ShapeT));
    }

// 更新未查询过的 Shape 的位置
    for (const auto& obj : queryObjects) {
        auto& body = actorPos[obj.Actor];
        auto& shape = shapePos[obj.Shape];
        physx::PxTransform GlobalPose = body.mRigid.mCore.mBodyToWorld * shape.ShapeCore.mCore.transform;
        cache[obj] = GlobalPose;
    }

    std::set<PrunerPayload> removeObjects{};
    std::vector<PrunerPayload> addObjects{};

// 移除当前已不在池中的 Shape
    for (auto it = cache.begin(); it != cache.end(); ) {
        if (mObjectsSet.find(it->first) == mObjectsSet.end()) {
            it = cache.erase(it);
        }
        else {
            ++it;
        }
    }


// 根据位置过滤在指定范围内的 Shape
    std::set<PrunerPayload> result;
    for (const auto& [obj, pos] : cache) {
//if(currentPosition.Distance(pos.mPosition) < radius){
        result.insert(obj);
//  }
    }
// 判断原有的 Shape 是否已不在当前结果中，如果是则需要移除（适用于静态）
    std::copy_if(currentSceneObjects.begin(), currentSceneObjects.end(), std::inserter(removeObjects, removeObjects.begin()),
                 [&result](const PrunerPayload& obj) { return result.find(obj) == result.end(); });

// 判断新增的 Shape 是否需要添加（包括总是检查的动态 Shape）
    std::copy_if(
            result.begin(), result.end(), std::back_inserter(addObjects),
            [&currentSceneObjects, &alwaysCheckShape](const PrunerPayload& obj) {
                return currentSceneObjects.find(obj) == currentSceneObjects.end() || alwaysCheckShape.find(obj) != alwaysCheckShape.end();
            }
    );

    for (const auto& obj : removeObjects) {
        currentSceneObjects.erase(obj);
        alwaysCheckShape.erase(obj);
        willRemoveObjects.insert(obj);
    }

    for (const auto& obj : addObjects) {
        currentSceneObjects.insert(obj);
    }
    auto res = GetMeshData(
            addObjects,
            [&alwaysCheckShape, &willRemoveObjects, &currentSceneObjects](const PhysX::ShapeDataT& shape_data) {
                PxGeometryType type = shape_data.mType;
                uint8_t Flags = shape_data.m_shape_flags;
                auto queryFlag = shape_data.Shape.ShapeCore.QueryFilterData;
                //if (queryFlag.word0 > 0uLL && queryFlag.word2 > 0uLL) {
                uint8_t masked = Flags & 0xF;
                if (type != PxGeometryType::eTRIANGLEMESH && type != PxGeometryType::eBOX && type != PxGeometryType::eCONVEXMESH) {
                    willRemoveObjects.insert(shape_data.UniqueKey);
                    currentSceneObjects.erase(shape_data.UniqueKey);
                    return true;
                }
                // break able wall
                auto findRes = alwaysCheckShape.find(shape_data.UniqueKey);
                auto trianglePtr = (uint64_t)((PhysX::PxTriangleMeshGeometryT&)shape_data.Shape.GetGeometry()).mTriangleMesh;
                if (findRes == alwaysCheckShape.end()) {
                    alwaysCheckShape.insert({ shape_data.UniqueKey, trianglePtr });
                }
                else {
                    if (findRes->second == trianglePtr) {
                        // not change, filter it.
                        return true;
                    }
                    else {
                        alwaysCheckShape[shape_data.UniqueKey] = trianglePtr;
                        willRemoveObjects.insert(shape_data.UniqueKey);
                        currentSceneObjects.erase(shape_data.UniqueKey);
                        return false;
                    }
                }
                return !(type == PxGeometryType::eTRIANGLEMESH || type == PxGeometryType::eBOX || type == PxGeometryType::eCONVEXMESH);
            }
    );
    return res;
}


namespace Physics {

    using namespace physx;
    using namespace std;

    static auto prunerPayloadExtractor = [](const TriangleMeshData& mesh) -> PrunerPayload {
        return mesh.UniqueKey1;
    };

    static auto int64Extractor = [](const TriangleMeshData& mesh) -> uint64_t {
        return mesh.UniqueKey2;
    };

    static auto normal64Extractor = [](const TriangleMeshData& mesh) -> uint64_t {
        return (uint64_t)rand();
    };

    static void embreeErrorFunction(void* userPtr, RTCError code, const char* str) {
        printf("[DEBUG] Embree Error [%d]: %s \n", code, str);
    }

    template <typename T, typename Hash>
    class VisibleScene
    {
    public:
        using KeyExtractor = T(*)(const TriangleMeshData&);
        const std::vector<std::shared_ptr<TriangleMeshData>>& GetMeshDatas() const {
            return mesh_datas;
        }
        VisibleScene(KeyExtractor keyExtractor) :
                getKey(keyExtractor) {
            this->device = rtcNewDevice(nullptr);
            rtcSetDeviceErrorFunction(device, embreeErrorFunction, nullptr);
            this->scene = rtcNewScene(device);
            //rtcSetDeviceProperty(device, RTC_DEVICE_PROPERTY_TASKING_SYSTEM, 1); // USE TBB
            rtcSetSceneBuildQuality(scene, RTC_BUILD_QUALITY_LOW);
            rtcSetSceneFlags(scene, RTC_SCENE_FLAG_DYNAMIC);
            rtcCommitScene(this->scene);
        }

        ~VisibleScene() {
            mesh_datas.clear();
            if (scene) {
                rtcReleaseScene(scene);
                scene = nullptr; // 避免重复释放
            }
            if (device) {
                rtcReleaseDevice(device);
                device = nullptr; // 避免重复释放
            }
        }

        void UpdateMesh(
                const vector<TriangleMeshData>& willAddMeshs,
                const set<T>& RemoveKey
        ){
            // remove geometry use disable
            vector<RTCGeometry> willRemoveGeometry;
            for (auto& key : RemoveKey) {
                if (geometry_id_map.find(key) != geometry_id_map.end()) {
                    auto geometry_id = geometry_id_map[key];
                    auto geometry = rtcGetGeometry(scene, geometry_id);
                    rtcDisableGeometry(geometry);
                    disabled_geometry_ids.insert(geometry_id);
                    geometry_id_map.erase(key);
                }
            }

            // remove mesh data
            if (!mesh_datas.empty()) {
                mesh_datas.erase(
                        remove_if(
                                mesh_datas.begin(), mesh_datas.end(),
                                [this, &RemoveKey](const shared_ptr<TriangleMeshData>& mesh) {
                                    return RemoveKey.find(this->getKey(*mesh)) != RemoveKey.end();
                                }
                        ),
                        mesh_datas.end()
                );
            }


            // add geometry or enable
            for (auto& mesh : willAddMeshs) {
                if (mesh.Vertices.size() == 0 || mesh.Indices.size() == 0) {
                    continue;
                }
                RTCGeometry geom;
                bool should_release = false;
                uint32_t geometry_id = 0;
                auto mesh_copy = make_shared<TriangleMeshData>(mesh);
                mesh_datas.push_back(mesh_copy);
                if (!disabled_geometry_ids.empty()) {
                    // use disabled geometry
                    geometry_id = *disabled_geometry_ids.begin();
                    disabled_geometry_ids.erase(disabled_geometry_ids.begin());
                    geom = rtcGetGeometry(scene, geometry_id);
                    rtcEnableGeometry(geom);
                } else {
                    // create new geometry
                    geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);
                    should_release = true;
                }


                // set vertices buffer
                float* vertices = (float*)rtcSetNewGeometryBuffer(geom,
                                                                  RTC_BUFFER_TYPE_VERTEX,
                                                                  0,
                                                                  RTC_FORMAT_FLOAT3,
                                                                  3 * sizeof(float),
                                                                  mesh.Vertices.size());

                if (!vertices) {
                    printf( "Error: Failed to allocate vertex buffer \n");
                    if (should_release) {
                        rtcReleaseGeometry(geom);
                    } else {
                        rtcDisableGeometry(geom);
                        disabled_geometry_ids.insert(geometry_id);
                    }
                    break;
                }

                // Copie data
                for (size_t i = 0; i < mesh.Vertices.size(); i++) {
                    vertices[i * 3] = mesh.Vertices[i].x;
                    vertices[i * 3 + 1] = mesh.Vertices[i].y;
                    vertices[i * 3 + 2] = mesh.Vertices[i].z;
                }

                auto bufferSize = mesh.Indices.size() / 3;
                // set indices buffer
                unsigned int* indices = (unsigned int*)rtcSetNewGeometryBuffer(geom,
                                                                               RTC_BUFFER_TYPE_INDEX,
                                                                               0,
                                                                               RTC_FORMAT_UINT3,
                                                                               3 * sizeof(unsigned int),
                                                                               bufferSize);

                if (!indices) {
                    printf("Error: Failed to allocate index buffer\n");
                    if (should_release) {
                        rtcReleaseGeometry(geom);
                    } else {
                        rtcDisableGeometry(geom);
                        disabled_geometry_ids.insert(geometry_id);
                    }
                    break;
                }


                // copy indices buffer
                memcpy(indices, mesh.Indices.data(), mesh.Indices.size() * sizeof(uint32_t));

                rtcSetGeometryUserData(geom, mesh_copy.get());
                rtcCommitGeometry(geom);

                if (should_release) {
                    try {
                        geometry_id = rtcAttachGeometry(scene, geom);
                    }
                    catch (...) {
                        printf( "Error attach geom.\n");
                        rtcReleaseGeometry(geom);
                        return;
                    }
                    rtcReleaseGeometry(geom);
                }
                // 使用key提取器获取key
                T key = getKey(mesh);
                geometry_id_map.insert({key, geometry_id});
            }

            // commit scene
            rtcCommitScene(scene);
        }

        RTCRayHit Raycast(physx::PxVec3& origin, physx::PxVec3& target)
        {
            try {
                // define ray
                RTCRay ray;
                ray.org_x = origin.x;
                ray.org_y = origin.y;
                ray.org_z = origin.z;
                ray.dir_x = target.x - origin.x;
                ray.dir_y = target.y - origin.y;
                ray.dir_z = target.z - origin.z;

                // norm
                float dir_length = std::sqrt(ray.dir_x * ray.dir_x + ray.dir_y * ray.dir_y + ray.dir_z * ray.dir_z);
                ray.dir_x /= dir_length;
                ray.dir_y /= dir_length;
                ray.dir_z /= dir_length;

                ray.tnear = 0.0f;       // start distance
                ray.tfar = dir_length;  // total distance
                ray.mask = -1;
                ray.flags = 0;

                // init result var
                RTCRayHit rayhit;
                rayhit.ray = ray;
                rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;


                if (this->scene) {
                    rtcIntersect1(this->scene, &rayhit, nullptr); // 直接传 nullptr
                }
                return rayhit;
            }
            catch (...) {
                printf( "Raycast error\n");
                RTCRayHit rayhit;
                rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
                return rayhit;
            }
        }

        TriangleMeshData* GetGeomeoryData(uint32_t geomId) {
            return (TriangleMeshData*)rtcGetGeometryUserData(rtcGetGeometry(scene, geomId));
        }

    private:
        RTCDevice device;
        RTCScene  scene;
        std::unordered_map<T, uint32_t, Hash> geometry_id_map = {};
        set<uint32_t> disabled_geometry_ids = {};
        vector<shared_ptr<TriangleMeshData>> mesh_datas = {};
        KeyExtractor getKey;
    };
} // namespace Physics

inline Physics::VisibleScene<PrunerPayload, PrunerPayloadHash>* DynamicLoadScene;
inline Physics::VisibleScene<PrunerPayload, PrunerPayloadHash>* DynamicRigidScene;
inline Physics::VisibleScene<uint64_t, Int64Hash>* HeightFieldScene;

class Throttler {
public:
    Throttler();
    void executeTaskWithSleep(const std::string& taskName,std::chrono::duration<double> interval,const std::function<void()>& task);
private:
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> lastExecuted_;
    //std::mutex mutex_;
};

inline Throttler::Throttler() {}

inline void Throttler::executeTaskWithSleep(const std::string& taskName,std::chrono::duration<double> interval,const std::function<void()>& task)
{
    using Clock = std::chrono::steady_clock;
    auto now = Clock::now();
    {
        //std::lock_guard<std::mutex> lock(mutex_);
        auto it = lastExecuted_.find(taskName);
        if (it != lastExecuted_.end()) {
            auto elapsed = now - it->second;
            if (elapsed < interval) {
                std::this_thread::sleep_for(interval - elapsed);
            }
        }
        // 记录更新时间点，防止 sleep 精度误差积累
        lastExecuted_[taskName] = Clock::now();
    }
    task();
}

namespace VisibleCheck {
    inline bool IsReady() {
    return dr->GetGlobalPid() > 0 && AppBase.libUE4 > 0x10000;
}

    //按范围更新场景
    inline void UpdateSceneByRange() {
        std::unordered_map<PrunerPayload, physx::PxTransform, PrunerPayloadHash> cache{};
        std::set<PrunerPayload> currentSceneObjects{};
        std::unordered_map<PrunerPayload, uint64_t, PrunerPayloadHash> alwaysCheckShape{};
        uint32_t lastUpdateTimestamp = 0;
        Throttler Throttlered;
        while (true) {
            if (!IsReady()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            Throttlered.executeTaskWithSleep("UpdateSceneByRangeSleep", std::chrono::milliseconds(2000), [&cache, &currentSceneObjects, &lastUpdateTimestamp, &alwaysCheckShape] {
                std::set<PrunerPayload> willRemoveObjects{};
                auto Meshs = LoadShapeByRange(lastUpdateTimestamp,cache,currentSceneObjects,willRemoveObjects,alwaysCheckShape);
                if (!Meshs.empty() || !willRemoveObjects.empty()) DynamicLoadScene->UpdateMesh(Meshs, willRemoveObjects);
            });
        }
    }

    inline void UpdateDynamicHeightField() {
        std::set<PrunerPayload> UniqueSet{};
        std::set<PrunerPayload> HeightFieldSet{};
        std::set<uint64_t> HeightFieldSamplePtrSet{};
        uint32_t lastUpdateTimestamp = 0;
        Throttler Throttlered;
        while (true) {
            if (!IsReady()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            Throttlered.executeTaskWithSleep("HeightFieldUpdateSleep", std::chrono::milliseconds(3000), [&UniqueSet, &HeightFieldSet, &HeightFieldSamplePtrSet, &lastUpdateTimestamp] {
                std::set<uint64_t> RemoveHeightFieldKey{};
                auto Meshs = RefreshDynamicLoadHeightField(lastUpdateTimestamp, UniqueSet,HeightFieldSet,HeightFieldSamplePtrSet,RemoveHeightFieldKey);
                if (!Meshs.empty() || !RemoveHeightFieldKey.empty())HeightFieldScene->UpdateMesh(Meshs, RemoveHeightFieldKey);
            });
        }
    }

    inline void UpdateDynamicRigid() {
        Throttler Throttlered;
        std::unordered_map<PrunerPayload, physx::PxTransform, PrunerPayloadHash> cache{};
        std::unordered_map<PrunerPayload, uint64_t, PrunerPayloadHash> ptrCache{};
        std::set<PrunerPayload> currentSceneObjects{};
        while (true) {
            if (!IsReady()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            Throttlered.executeTaskWithSleep("DynamicRigidUpdateSleep", std::chrono::milliseconds(300), [&currentSceneObjects, &cache, &ptrCache] {
                std::set<PrunerPayload> willRemoveShape{};
                auto Meshs = LoadDynamicRigidShape(currentSceneObjects,cache,ptrCache,willRemoveShape);
                if (!Meshs.empty() || !willRemoveShape.empty()) DynamicRigidScene->UpdateMesh(Meshs, willRemoveShape);
            });
        }
    }
}

namespace LineTrace {

    inline bool initPhysX() {
        DynamicLoadScene  = new Physics::VisibleScene<PrunerPayload, PrunerPayloadHash>(Physics::prunerPayloadExtractor);
        DynamicRigidScene = new Physics::VisibleScene<PrunerPayload, PrunerPayloadHash>(Physics::prunerPayloadExtractor);
        HeightFieldScene  = new Physics::VisibleScene<uint64_t, Int64Hash>(Physics::int64Extractor);

        if (DynamicLoadScene && DynamicRigidScene && HeightFieldScene) {
            return true;
        }
        return false;
    }
    static physx::PxVec3 ToPx(const Vec3& v) {
        return physx::PxVec3(v.x, v.y, v.y);
    }

    inline bool LineTraceSingle(Vec3 Location, Vec3 TraceEnd)
    {
        if (DynamicLoadScene == nullptr || HeightFieldScene == nullptr || DynamicRigidScene == nullptr) {
            return false;
        }
        physx::PxVec3 origin = ToPx(Location);
        physx::PxVec3 target = ToPx(TraceEnd);

        auto dynamicRayHit = DynamicLoadScene->Raycast(origin, target);
        if (dynamicRayHit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
            return true;
        }

        auto heightFieldRayHit = HeightFieldScene->Raycast(origin, target);
        if (heightFieldRayHit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
            return true;
        }

        auto globalSceneRayHit = DynamicRigidScene->Raycast(origin, target);
        if (globalSceneRayHit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
            return true;
        }

        return false;
    }
}
