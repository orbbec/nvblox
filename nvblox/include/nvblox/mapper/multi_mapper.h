/*
Copyright 2022 NVIDIA CORPORATION

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#pragma once

#include "nvblox/mapper/mapper.h"

namespace nvblox {

enum class MappingType {
  kStaticTsdf,           /// only static tsdf
  kStaticOccupancy,      /// only static occupancy
  kDynamic,              /// static tsdf (incl. freespace) and dynamic occupancy
  kHumanWithStaticTsdf,  /// static tsdf and human occupancy
  kHumanWithStaticOccupancy  /// static occupancy and human occupancy
};

/// Whether the masked mapper is used for human mapping
inline bool isHumanMapping(MappingType mapping_type) {
  if (mapping_type == MappingType::kHumanWithStaticTsdf ||
      mapping_type == MappingType::kHumanWithStaticOccupancy) {
    return true;
  }
  return false;
}

/// Whether the masked mapper is used for dynamic mapping
inline bool isDynamicMapping(MappingType mapping_type) {
  if (mapping_type == MappingType::kDynamic) {
    return true;
  }
  return false;
}

/// Whether both the unmasked and masked mapper are active,
/// i.e. the masked mapper is used for dynamic/human mapping
inline bool isUsingBothMappers(MappingType mapping_type) {
  if (isHumanMapping(mapping_type) || isDynamicMapping(mapping_type)) {
    return true;
  }
  return false;
}

/// Whether the unmasked mapper is doing occupancy
inline bool isStaticOccupancy(MappingType mapping_type) {
  if (mapping_type == MappingType::kStaticOccupancy ||
      mapping_type == MappingType::kHumanWithStaticOccupancy) {
    return true;
  }
  return false;
}

template <>
inline std::string toString(const MappingType& mapping_type) {
  switch (mapping_type) {
    case MappingType::kStaticTsdf:
      return "kStaticTsdf";
      break;
    case MappingType::kStaticOccupancy:
      return "kStaticOccupancy";
      break;
    case MappingType::kDynamic:
      return "kDynamic";
      break;
    case MappingType::kHumanWithStaticTsdf:
      return "kHumanWithStaticTsdf";
      break;
    case MappingType::kHumanWithStaticOccupancy:
      return "kHumanWithStaticOccupancy";
      break;
    default:
      CHECK(false) << "Requested mapping type is not implemented.";
  }
}

/// The MultiMapper class is composed of two standard Mappers.
/// Depth and color are integrated into one of these Mappers according to a
/// mask.
/// Setup:
/// - masked mapper:   Handling general dynamics or humans in an occupancy
///                    layer.
/// - unmasked mapper: Handling static objects with a tsdf or a occupancy layer.
///                    Also updating a freespace layer if mapping type is
///                    kDynamic.
///
/// NOTE(remos): For dynamic mapping the full depth image is integrated into the
/// unmasked mapper (no masking). Otherwise freespace can not be reset as depth
/// measurements falling into the freespace will always be masked dynamic by the
/// DynamicsDetection module.
/// As a consequence, we need to ignore the esdf sites in the unmasked mapper
/// that fall into freespace because they are actually dynamic and handled by
/// the masked mapper.
class MultiMapper {
 public:
  static constexpr int kDefaultConnectedMaskComponentSizeThreshold = 2000;

  struct Params {
    /// The minimum number of pixels of a connected component in the mask image
    /// to count as a dynamic detection.
    int connected_mask_component_size_threshold =
        kDefaultConnectedMaskComponentSizeThreshold;
  };

  /// @param voxel_size_m The voxel size in meters for the contained layers.
  /// @param projective_layer_type The layer type to which the projective
  ///        data is integrated (either tsdf or occupancy).
  /// @param memory_type In which type of memory the layers should be stored.

  /// Constructor
  /// @param voxel_size_m The voxel size in meters for the contained layers.
  /// @param mapping_type Static, Dynamic etc. See MappingType.
  /// @param esdf_mode 2D or 3D. See EsdfMode.
  /// @param memory_type In which type of memory the layers should be stored.
  /// @param cuda_stream Optional cuda stream to perform all work on.
  MultiMapper(float voxel_size_m, MappingType mapping_type, EsdfMode esdf_mode,
              MemoryType memory_type = MemoryType::kDevice,
              std::shared_ptr<CudaStream> cuda_stream =
                  std::make_shared<CudaStreamOwning>());
  ~MultiMapper() = default;

  /// @brief Setting the multi mapper param struct
  /// @param multi_mapper_params the param struct
  void setMultiMapperParams(const Params& multi_mapper_params) {
    params_ = multi_mapper_params;
  }

  /// @brief Setting the mapper param struct to the two mappers
  /// @param unmasked_mapper_params param struct for unmasked mapper
  /// @param masked_mapper_params  param struct for masked mapper (optional)
  void setMapperParams(
      const MapperParams& unmasked_mapper_params,
      const std::optional<MapperParams>& masked_mapper_params = std::nullopt);

  /// @brief Integrates a depth frame into the reconstruction (for mapping type
  /// kStaticTsdf/kStaticOccupancy/kDynamic).
  /// @param depth_frameDepth frame to integrate.
  /// @param T_L_CD Pose of the depth camera, specified as a transform from
  ///              camera frame to layer frame transform.
  /// @param depth_camera Intrinsics model of the depth camera.
  /// @param update_time_ms Current update time in millisecond.
  void integrateDepth(const DepthImage& depth_frame, const Transform& T_L_CD,
                      const Camera& depth_camera,
                      const std::optional<Time>& update_time_ms = std::nullopt);

  /// @brief Integrates a depth frame into the reconstruction
  /// using the transformation between depth and mask frame and their
  /// intrinsics (for mapping type
  /// kHumanWithStaticTsdf/kHumanWithStaticOccupancy).
  ///@param depth_frame Depth frame to integrate. Depth in the image is
  ///                   specified as a float representing meters.
  ///@param mask Mask. Interpreted as 0=non-masked, >0=masked.
  ///@param T_L_CD Pose of the depth camera, specified as a transform from
  ///              camera frame to layer frame transform.
  ///@param T_CM_CD Transform from depth camera to mask camera frame.
  ///@param depth_camera Intrinsics model of the depth camera.
  ///@param mask_camera Intrinsics model of the mask camera.
  void integrateDepth(const DepthImage& depth_frame, const MonoImage& mask,
                      const Transform& T_L_CD, const Transform& T_CM_CD,
                      const Camera& depth_camera, const Camera& mask_camera);

  /// @brief Integrates a color frame into the reconstruction (for mapping type
  /// kStaticTsdf/kStaticOccupancy/kDynamic).
  /// @param color_frame Color image to integrate.
  /// @param T_L_C Pose of the camera, specified as a transform from camera
  /// frame to the layer frame.
  /// @param camera Intrinsics model of the camera.
  void integrateColor(const ColorImage& color_frame, const Transform& T_L_C,
                      const Camera& camera);

  /// @brief Integrates a color frame into the reconstruction (for mapping type
  /// kHumanWithStaticTsdf/kHumanWithStaticOccupancy).
  ///@param color_frame Color image to integrate.
  ///@param mask Mask. Interpreted as 0=non-masked, >0=masked.
  ///@param T_L_C Pose of the camera, specified as a transform from camera frame
  ///             to the layer frame.
  ///@param camera Intrinsics model of the camera.
  void integrateColor(const ColorImage& color_frame, const MonoImage& mask,
                      const Transform& T_L_C, const Camera& camera);

  /// @brief Updating the esdf layers of the mappers depending on the mapping
  /// type.
  void updateEsdf();

  /// @brief Updating the mesh layers of the mappers depending on the mapping
  /// type.
  /// @return A serialized mesh. If mesh_bandwidth_limit_mbps is positive,
  ///         the size of the mesh is limited based on estimated
  ///         bandwidth capacity.
  std::shared_ptr<const SerializedMesh> updateMesh(
      const std::optional<Transform>& maybe_T_L_C = std::nullopt,
      bool serialize_full_mesh = false);

  /// Access to one of the mappers
  const Mapper& unmasked_mapper() const { return *unmasked_mapper_.get(); }
  /// Access to one of the mappers
  const Mapper& masked_mapper() const { return *masked_mapper_.get(); }
  /// Access to one of the mappers
  std::shared_ptr<Mapper>& unmasked_mapper() { return unmasked_mapper_; }
  /// Access to one of the mappers
  std::shared_ptr<Mapper>& masked_mapper() { return masked_mapper_; }

  /// These functions return a reference to the masked images generated during
  /// the preceeding calls to integrateColor() and integrateDepth().
  const DepthImage& getLastDepthFrameUnmasked();
  const DepthImage& getLastDepthFrameMasked();
  const ColorImage& getLastColorFrameUnmasked();
  const ColorImage& getLastColorFrameMasked();
  const ColorImage& getLastDepthFrameMaskOverlay();
  const ColorImage& getLastColorFrameMaskOverlay();
  const ColorImage& getLastDynamicFrameMaskOverlay();
  const Pointcloud& getLastDynamicPointcloud();

  /// Return the parameter tree.
  /// @return the parameter tree
  virtual parameters::ParameterTreeNode getParameterTree(
      const std::string& name_remap = std::string()) const;

  /// Return the parameter tree represented as a string
  /// @return the parameter tree string
  virtual std::string getParametersAsString() const;

 protected:
  // Performs the esdf update on the passed mapper
  void updateEsdfOfMapper(const std::shared_ptr<Mapper>& mapper);

  // Mapping type needed on construction
  const MappingType mapping_type_;
  const EsdfMode esdf_mode_;

  // Parameter struct for multi mapper
  Params params_;

  // Helper to detect dynamics from a freespace layer
  DynamicsDetection dynamic_detector_;
  MonoImage cleaned_dynamic_mask_{MemoryType::kDevice};

  // Declared to use for cleaning up of semantic mask
  MonoImage cleaned_semantic_mask_{MemoryType::kDevice};

  // Split depth images based on a mask.
  // Note that we internally pre-allocate space for the split images on the
  // first call.
  ImageMasker image_masker_;
  DepthImage depth_frame_unmasked_{MemoryType::kDevice};
  DepthImage depth_frame_masked_{MemoryType::kDevice};
  ColorImage color_frame_unmasked_{MemoryType::kDevice};
  ColorImage color_frame_masked_{MemoryType::kDevice};

  // Mask overlays used as debug outputs
  ColorImage masked_depth_overlay_{MemoryType::kDevice};
  ColorImage masked_color_overlay_{MemoryType::kDevice};

  // The two mappers to which the frames are integrated.
  std::shared_ptr<Mapper> masked_mapper_;
  std::shared_ptr<Mapper> unmasked_mapper_;

  // The CUDA stream on which to process all work
  std::shared_ptr<CudaStream> cuda_stream_;
};

}  // namespace nvblox
