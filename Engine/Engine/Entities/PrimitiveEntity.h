#pragma once

#include "../Engine.h"

#include "../BaseComponents/MeshComponent.h"

namespace diffusion {

::entt::entity create_primitive_entity_base(
	::entt::registry & registry,
	const std::vector<PrimitiveColoredVertex>& verticies,
	const std::vector<uint32_t>& indexes,
	const glm::vec3& position,
	const glm::vec3& rotation,
	const glm::vec3& scale,
	const SubMesh::AABB& bounding_box = {});

void add_default_unlit_material_component(::entt::registry& registry, ::entt::entity parent_entity);
void add_default_lit_material_component(::entt::registry& registry, ::entt::entity parent_entity);

} // namespace diffusion {