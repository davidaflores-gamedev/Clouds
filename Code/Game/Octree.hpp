#pragma once
#include <vector>
#include "Engine/Math/AABB3.hpp" // Assuming you have a 3D bounding box class
#include "Cloud.hpp" // Assuming you have a Voxel struct or class
#include <functional>
#include <unordered_map>

// Constants
constexpr int MAX_OCTREE_DEPTH = 8;
constexpr int ELEMENTS_PER_LEAF = 4;

//try 8 32 and 54
// 
// GPU-Ready Node Structure
struct OctreeNodeGPU {
	Vec3 minBounds = Vec3();
	Vec3 maxBounds = Vec3();
	float densitySum = 0.f;    // Aggregate density
	int firstChildIndex = 0; // Index of the first child (-1 if leaf)
	int numChildren = 0;     // Number of children (0 if leaf)
	int firstElementIndex = 0; // Index of the first voxel (-1 if none)
	int numElements = 0;       // Number of voxels in this node
	int depth = 0;           // Depth of this node
};

// OctreeNode Class
template <typename T>
class OctreeNode {
public:
	AABB3 boundingBox;
	float densitySum = 0.0f;
	bool isLeaf = false;
	int depth = 0;

	// Raw pointers for children
	OctreeNode* children[8] = { nullptr };

	// Voxels contained in this node (if leaf)
	std::vector<T*> elements;

	OctreeNode(const AABB3& bounds);
	~OctreeNode(); // Cleans up dynamically allocated children
};

template<typename T>
struct DefaultGetAABB3
{
	AABB3 operator()(const T& element, const Vec3 size) const
	{
		Vec3 halfSize = size * 0.5f;

		return AABB3(element.m_position - halfSize, element.m_position + halfSize);
	}
};

template<typename T>
struct DefaultGetCenter
{
	Vec3 operator()(const T& element) const
	{
		return element.m_position;
	}
};

template<typename T>
struct DefaultGetDensity
{
	float operator()(const T& element) const
	{
		return element.m_density;
	}
};

template<typename T>
struct ListBasedGetDensity
{
	ListBasedGetDensity(const std::vector<float>& densities, const IntVec3& dimensions)
		: m_densities(densities)
		, m_dimensions(dimensions)
	{}

	float operator()(const T& element) const {
		// For example, assume T has an 'index' member.

		int index = element.m_position.x + element.m_position.y * m_dimensions.x + element.m_position.z * m_dimensions.x * m_dimensions.y;

		return m_densities[index];
	}

private:
	const std::vector<float>& m_densities;
	IntVec3 m_dimensions;
};

// Octree Class
template
<
	typename T,
	typename GetAABB = DefaultGetAABB3<T>,
	typename GetCenter = DefaultGetCenter<T>,
	typename GetDensity = DefaultGetDensity<T>
>
class Octree {
public:
	Octree(const AABB3& bounds, const GetDensity& densityGetter);
	~Octree();

	// Public interface
	void Build(const std::vector<T*>& Elements, const Vec3& elementSize);
	void SerializeToGPU(std::vector<OctreeNodeGPU>& gpuNodes, std::vector<T>& gpuElements, std::unordered_map<Vec3, int, Vec3Hasher>& voxelMap) const;

	int GetAllChildrenSize() const;

private:
	OctreeNode<T>* root;

	int m_totalElements = 0;

	void BuildRecursive(OctreeNode<T>* node, const std::vector<T*>& elements, const Vec3& elementSize, int depth);

	int SerializeRecursive(			const OctreeNode<T>* node, 
									std::vector<OctreeNodeGPU>& gpuNodes,
									std::vector<T>& gpuElements,
									std::unordered_map<Vec3, int, Vec3Hasher>& elementMap) const;

	void SerializeIntoPlaceholder(	const OctreeNode<T>* node, 
									std::vector<OctreeNodeGPU>& gpuNodes,
									int index,
									std::vector<T>& gpuVoxels, 
									std::unordered_map<Vec3, int, Vec3Hasher>& voxelMap) const;

	GetAABB getAABB;

	GetCenter getCenter;

	GetDensity getDensity;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//CPP start
////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#include <algorithm>

// OctreeNode Constructor
template<typename T>
OctreeNode<T>::OctreeNode(const AABB3& bounds)
	: boundingBox(bounds) {}

// OctreeNode Destructor
template<typename T>
OctreeNode<T>::~OctreeNode() {
	for (OctreeNode<T>*& child : children) {
		delete child;
		child = nullptr;
	}
}

// Octree Constructor
template<typename T, typename GetAABB, typename GetCenter, typename GetDensity>
Octree<T, GetAABB, GetCenter, GetDensity>::Octree(const AABB3& bounds, const GetDensity& densityGetter)
	: root(new OctreeNode<T>(bounds)), getDensity(densityGetter)
{}

// Octree Destructor
template<typename T, typename GetAABB, typename GetCenter, typename GetDensity>
Octree<T, GetAABB, GetCenter, GetDensity>::~Octree() {
	delete root;
}

// Build the Octree
template<typename T, typename GetAABB, typename GetCenter, typename GetDensity>
void Octree<T, GetAABB, GetCenter, GetDensity>::Build(const std::vector<T*>& elements, const Vec3& elementSize) {
	m_totalElements = 0;
	BuildRecursive(root, elements, elementSize, 0);
}

// Recursive Octree Building
template<typename T, typename GetAABB, typename GetCenter, typename GetDensity>
void Octree<T, GetAABB, GetCenter, GetDensity>::BuildRecursive(OctreeNode<T>* node, const std::vector<T*>& elements, const Vec3& elementSize, int depth)
{
	//=============================================
	// Calculate the bounding box for the leaf node to enclose all voxels
	//=============================================
	Vec3 leafMin = Vec3::MAX;
	Vec3 leafMax = Vec3::MIN;

	node->densitySum = 0.0f;
	node->depth = depth;

	for (const T* element : elements)
	{
		AABB3 elementAABB = getAABB(*element, elementSize);

		leafMin = GetMin(leafMin, elementAABB.m_mins);
		leafMax = GetMax(leafMax, elementAABB.m_maxs);

		node->densitySum += getDensity(*element);
	}
	node->boundingBox = AABB3(leafMin, leafMax);

	//=============================================
	// Leaf node criteria
	//=============================================
	if (depth >= MAX_OCTREE_DEPTH || elements.size() <= ELEMENTS_PER_LEAF) {
		node->isLeaf = true;
		node->elements = elements;
		m_totalElements += static_cast<int>(elements.size());
		// Debug print can be added here if needed.
		return;
	}

	// Otherwise, subdivide.
	node->isLeaf = false;
	node->elements.clear();

	for (int i = 0; i < 8; ++i) {
		// Compute the child bounding box. (Assumes AABB3 has a GetChild(i) method.)
		AABB3 childBounds = node->boundingBox.GetChild(i);
		std::vector<T*> childElements;
		childElements.reserve(elements.size());

		// Assign elements to this child based on their center.
		for (T* element : elements) {
			Vec3 center = getCenter(*element);
			if (childBounds.IsPointInsideLesser(center)) {
				childElements.push_back(element);
			}
		}

		if (!childElements.empty()) {
			// If the child receives all the elements, then no further subdivision is possible.
			if (childElements.size() == elements.size()) {
				node->isLeaf = true;
				node->elements = elements;
				m_totalElements += static_cast<int>(elements.size());
				// Debug: Forced leaf due to lack of subdivision.
				return;
			}

			// Create the child node and build its subtree.
			node->children[i] = new OctreeNode<T>(AABB3(childBounds.m_mins, childBounds.m_maxs));
			BuildRecursive(node->children[i], childElements, elementSize, depth + 1);
		}
		else {
			// If no element falls into this child, mark it explicitly as empty.
			node->children[i] = nullptr;
		}
	}
}

template<typename T, typename GetAABB, typename GetCenter, typename GetDensity>
int Octree<T, GetAABB, GetCenter, GetDensity>::GetAllChildrenSize() const {
	int size = 0;
	std::vector<OctreeNode<T>*> nodes;
	nodes.push_back(root);
	while (!nodes.empty()) {
		OctreeNode<T>* node = nodes.back();
		nodes.pop_back();
		size++;
		if (!node->isLeaf) {
			for (int i = 0; i < 8; i++) {
				if (node->children[i])
					nodes.push_back(node->children[i]);
			}
		}
	}
	return size;
}

template<typename T, typename GetAABB, typename GetCenter, typename GetDensity>
void Octree<T, GetAABB, GetCenter, GetDensity>::SerializeToGPU(std::vector<OctreeNodeGPU>& gpuNodes,
	std::vector<T>& gpuElements,
	std::unordered_map<Vec3, int, Vec3Hasher>& elementMap) const {
	SerializeRecursive(root, gpuNodes, gpuElements, elementMap);
}

template<typename T, typename GetAABB, typename GetCenter, typename GetDensity>
int Octree<T, GetAABB, GetCenter, GetDensity>::SerializeRecursive(const OctreeNode<T>* node,
	std::vector<OctreeNodeGPU>& gpuNodes,
	std::vector<T>& gpuElements,
	std::unordered_map<Vec3, int, Vec3Hasher>& elementMap) const {
	if (!node)
		return -1;

	// Reserve a slot for the current node.
	int currentIndex = static_cast<int>(gpuNodes.size());
	gpuNodes.push_back(OctreeNodeGPU{});  // placeholder

	OctreeNodeGPU currentGPU;
	currentGPU.minBounds = node->boundingBox.m_mins;
	currentGPU.maxBounds = node->boundingBox.m_maxs;
	currentGPU.densitySum = node->densitySum;
	currentGPU.depth = node->depth;

	if (node->isLeaf) {
		// For leaf nodes, record the element indices.
		currentGPU.firstElementIndex = static_cast<int>(gpuElements.size());
		currentGPU.numElements = 0;
		for (const T* element : node->elements) {
			Vec3 key = getCenter(*element);
			if (elementMap.find(key) == elementMap.end()) {
				elementMap[key] = static_cast<int>(gpuElements.size());
				gpuElements.push_back(*element);
			}
			currentGPU.numElements++;
		}
		if (currentGPU.numElements == 0)
			currentGPU.firstElementIndex = -1;

		currentGPU.firstChildIndex = -1;
		currentGPU.numChildren = 0;
	}
	else {
		// For non–leaf nodes, first gather the non–null children.
		std::vector<const OctreeNode<T>*> directChildren;
		for (const auto& child : node->children) {
			if (child)
				directChildren.push_back(child);
		}
		currentGPU.numChildren = static_cast<int>(directChildren.size());

		// Reserve contiguous slots for all direct children.
		currentGPU.firstChildIndex = static_cast<int>(gpuNodes.size());
		for (size_t i = 0; i < directChildren.size(); ++i)
			gpuNodes.push_back(OctreeNodeGPU{});  // placeholders

		// Serialize each direct child into its reserved slot.
		for (size_t i = 0; i < directChildren.size(); ++i) {
			int childIndex = currentGPU.firstChildIndex + static_cast<int>(i);
			SerializeIntoPlaceholder(directChildren[i], gpuNodes, childIndex, gpuElements, elementMap);
		}
	}

	// Write the current node’s data into its reserved slot.
	gpuNodes[currentIndex] = currentGPU;
	return currentIndex;
}

template<typename T, typename GetAABB, typename GetCenter, typename GetDensity>
void Octree<T, GetAABB, GetCenter, GetDensity>::SerializeIntoPlaceholder(const OctreeNode<T>* node,
	std::vector<OctreeNodeGPU>& gpuNodes,
	int index,
	std::vector<T>& gpuElements,
	std::unordered_map<Vec3, int, Vec3Hasher>& elementMap) const {
	if (!node)
		return;

	OctreeNodeGPU nodeGPU;
	nodeGPU.minBounds = node->boundingBox.m_mins;
	nodeGPU.maxBounds = node->boundingBox.m_maxs;
	nodeGPU.densitySum = node->densitySum;
	nodeGPU.depth = node->depth;

	if (node->isLeaf) {
		nodeGPU.firstElementIndex = static_cast<int>(gpuElements.size());
		nodeGPU.numElements = 0;
		for (const T* element : node->elements) {
			Vec3 key = getCenter(*element);
			if (elementMap.find(key) == elementMap.end()) {
				elementMap[key] = static_cast<int>(gpuElements.size());
				gpuElements.push_back(*element);
			}
			nodeGPU.numElements++;
		}
		if (nodeGPU.numElements == 0)
			nodeGPU.firstElementIndex = -1;

		nodeGPU.firstChildIndex = -1;
		nodeGPU.numChildren = 0;
	}
	else {
		std::vector<const OctreeNode<T>*> directChildren;
		for (const auto& child : node->children) {
			if (child)
				directChildren.push_back(child);
		}
		nodeGPU.numChildren = static_cast<int>(directChildren.size());
		nodeGPU.firstChildIndex = static_cast<int>(gpuNodes.size());
		for (size_t i = 0; i < directChildren.size(); ++i)
			gpuNodes.push_back(OctreeNodeGPU{});
		for (size_t i = 0; i < directChildren.size(); ++i) {
			int childIndex = nodeGPU.firstChildIndex + static_cast<int>(i);
			SerializeIntoPlaceholder(directChildren[i], gpuNodes, childIndex, gpuElements, elementMap);
		}
	}
	gpuNodes[index] = nodeGPU;
}