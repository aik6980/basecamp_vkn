Current Task: Technique Instance Expansion

Goal

Make technique instance production-ready for framegraph and GPU-driven migration.
Requirements

Support multiple bindings per descriptor set - done
Bind resources by reflected resource name - done
Validate type compatibility: CBV, SRV, UAV, Sampler
Provide one apply step to write and bind all descriptor sets
Support bindless resources
Bindless Scope

Descriptor array binding by reflected name
Variable descriptor count support
Partially bound descriptor arrays
Validation-safe indexing and descriptor writes

Done Criteria
One shader path uses bindless sampled texture array successfully
One renderer path migrated to new technique instance API
No new Vulkan validation errors
Missing optional bindless slots handled safely when partially bound