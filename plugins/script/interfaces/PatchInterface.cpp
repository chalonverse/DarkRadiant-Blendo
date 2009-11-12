#include "PatchInterface.h"

#include "ipatch.h"

namespace script {

class ScriptPatchNode :
	public ScriptSceneNode
{
	static const std::string _emptyShader;
	static PatchControl _emptyPatchControl;
public:
	ScriptPatchNode(const scene::INodePtr& node) :
		ScriptSceneNode((node != NULL && Node_isPatch(node)) ? node : scene::INodePtr())
	{}

	// Checks if the given SceneNode structure is a PatchNode
	static bool isPatch(const ScriptSceneNode& node) {
		return Node_isPatch(node);
	}

	// "Cast" service for Python, returns a ScriptPatchNode. 
	// The returned node is non-NULL if the cast succeeded
	static ScriptPatchNode getPatch(const ScriptSceneNode& node) {
		// Try to cast the node onto a patch
		IPatchNodePtr patchNode = boost::dynamic_pointer_cast<IPatchNode>(
			static_cast<scene::INodePtr>(node)
		);
		
		// Construct a patchNode (contained node may be NULL)
		return (patchNode != NULL) ? ScriptPatchNode(node) : ScriptPatchNode(scene::INodePtr());
	}

	// Resizes the patch to the given dimensions
	void setDims(std::size_t width, std::size_t height) 
	{
		IPatchNodePtr patchNode = boost::dynamic_pointer_cast<IPatchNode>(_node.lock());
		if (patchNode == NULL) return;

		patchNode->getPatch().setDims(width, height);
	}
	
	// Get the patch dimensions
	std::size_t getWidth() const 
	{
		IPatchNodePtr patchNode = boost::dynamic_pointer_cast<IPatchNode>(_node.lock());
		if (patchNode == NULL) return -1;

		return patchNode->getPatch().getWidth();
	}

	std::size_t getHeight() const 
	{
		IPatchNodePtr patchNode = boost::dynamic_pointer_cast<IPatchNode>(_node.lock());
		if (patchNode == NULL) return -1;

		return patchNode->getPatch().getHeight();
	}

	// Return a defined patch control vertex at <row>,<col>
	PatchControl& ctrlAt(std::size_t row, std::size_t col) 
	{
		IPatchNodePtr patchNode = boost::dynamic_pointer_cast<IPatchNode>(_node.lock());
		if (patchNode == NULL) return _emptyPatchControl;

		IPatch& patch = patchNode->getPatch();

		if (row > patch.getHeight() || col > patch.getWidth()) 
		{
			globalErrorStream() << "One or more patch control indices out of bounds: " << row << "," << col << std::endl;
			return _emptyPatchControl;
		}
		
		return patchNode->getPatch().ctrlAt(row, col);
	}

 	void insertColumns(std::size_t colIndex)
	{
		IPatchNodePtr patchNode = boost::dynamic_pointer_cast<IPatchNode>(_node.lock());
		if (patchNode == NULL) return;

		return patchNode->getPatch().insertColumns(colIndex);
	}
 	
 	void insertRows(std::size_t rowIndex) 
	{
		IPatchNodePtr patchNode = boost::dynamic_pointer_cast<IPatchNode>(_node.lock());
		if (patchNode == NULL) return;

		return patchNode->getPatch().insertRows(rowIndex);
	}

 	void removePoints(bool columns, std::size_t index)
	{
		IPatchNodePtr patchNode = boost::dynamic_pointer_cast<IPatchNode>(_node.lock());
		if (patchNode == NULL) return;

		return patchNode->getPatch().removePoints(columns, index);
	}
 	
 	void appendPoints(bool columns, bool beginning) 
	{
		IPatchNodePtr patchNode = boost::dynamic_pointer_cast<IPatchNode>(_node.lock());
		if (patchNode == NULL) return;

		return patchNode->getPatch().appendPoints(columns, beginning);
	}

	// Check if the patch has invalid control points or width/height are zero
	bool isValid() const 
	{
		IPatchNodePtr patchNode = boost::dynamic_pointer_cast<IPatchNode>(_node.lock());
		if (patchNode == NULL) return false;

		return patchNode->getPatch().isValid();
	}

	// Check whether all control vertices are in the same 3D spot (with minimal tolerance)
	bool isDegenerate() const 
	{
		IPatchNodePtr patchNode = boost::dynamic_pointer_cast<IPatchNode>(_node.lock());
		if (patchNode == NULL) return true;

		return patchNode->getPatch().isDegenerate();
	}

	// Shader handling
	const std::string& getShader() const 
	{
		IPatchNodePtr patchNode = boost::dynamic_pointer_cast<IPatchNode>(_node.lock());
		if (patchNode == NULL) return _emptyShader;

		return patchNode->getPatch().getShader();
	}

	void setShader(const std::string& name) 
	{
		IPatchNodePtr patchNode = boost::dynamic_pointer_cast<IPatchNode>(_node.lock());
		if (patchNode == NULL) return;

		patchNode->getPatch().setShader(name);
	}

	bool subdivionsFixed() const 
	{
		IPatchNodePtr patchNode = boost::dynamic_pointer_cast<IPatchNode>(_node.lock());
		if (patchNode == NULL) return false;

		return patchNode->getPatch().subdivionsFixed();
	}
	
	Subdivisions getSubdivisions() const 
	{
		IPatchNodePtr patchNode = boost::dynamic_pointer_cast<IPatchNode>(_node.lock());
		if (patchNode == NULL) return Subdivisions();

		return patchNode->getPatch().getSubdivisions();
	}
	
	void setFixedSubdivisions(bool isFixed, const Subdivisions& divisions) 
	{
		IPatchNodePtr patchNode = boost::dynamic_pointer_cast<IPatchNode>(_node.lock());
		if (patchNode == NULL) return;

		patchNode->getPatch().setFixedSubdivisions(isFixed, divisions);
	}
};

// Initialise static members
const std::string ScriptPatchNode::_emptyShader;
PatchControl ScriptPatchNode::_emptyPatchControl;

ScriptSceneNode PatchInterface::createPatchDef2() {
	// Create a new patch and return the script scene node
	return ScriptSceneNode(GlobalPatchCreator(DEF2).createPatch());
}

ScriptSceneNode PatchInterface::createPatchDef3() {
	// Create a new patch and return the script scene node
	return ScriptSceneNode(GlobalPatchCreator(DEF3).createPatch());
}

void PatchInterface::registerInterface(boost::python::object& nspace) {
	nspace["PatchControl"] = boost::python::class_<PatchControl>("PatchControl")
		.def_readwrite("vertex", &PatchControl::vertex)
		.def_readwrite("texcoord", &PatchControl::texcoord)
	;

	nspace["Subdivisions"] = boost::python::class_<Subdivisions>("Subdivisions", 
		boost::python::init<unsigned int, unsigned int>())
		.def(boost::python::init<const Subdivisions&>())
		// greebo: Pick the correct overload - this is hard to read, but it is necessary
		.def("x", static_cast<unsigned int& (Subdivisions::*)()>(&Subdivisions::x), 
			boost::python::return_value_policy<boost::python::copy_non_const_reference>())
		.def("y", static_cast<unsigned int& (Subdivisions::*)()>(&Subdivisions::y), 
			boost::python::return_value_policy<boost::python::copy_non_const_reference>())
	;

	// Define a PatchNode interface
	nspace["PatchNode"] = boost::python::class_<ScriptPatchNode, 
		boost::python::bases<ScriptSceneNode> >("PatchNode", boost::python::init<const scene::INodePtr&>() )
		.def("setDims", &ScriptPatchNode::setDims)
		.def("getWidth", &ScriptPatchNode::getWidth)
		.def("getHeight", &ScriptPatchNode::getHeight)
		.def("ctrlAt", &ScriptPatchNode::ctrlAt, 
			boost::python::return_value_policy<boost::python::copy_non_const_reference>())
		.def("insertColumns", &ScriptPatchNode::insertColumns)
		.def("insertRows", &ScriptPatchNode::insertRows)
		.def("removePoints", &ScriptPatchNode::removePoints)
		.def("appendPoints", &ScriptPatchNode::appendPoints)
		.def("isValid", &ScriptPatchNode::isValid)
		.def("isDegenerate", &ScriptPatchNode::isDegenerate)
		.def("getShader", &ScriptPatchNode::getShader, 
			boost::python::return_value_policy<boost::python::copy_const_reference>())
		.def("setShader", &ScriptPatchNode::setShader)
		.def("subdivionsFixed", &ScriptPatchNode::subdivionsFixed)
		.def("getSubdivisions", &ScriptPatchNode::getSubdivisions)
		.def("setFixedSubdivisions", &ScriptPatchNode::setFixedSubdivisions)
	;

	// Add the "isPatch" and "getPatch" method to all ScriptSceneNodes
	boost::python::object sceneNode = nspace["SceneNode"];

	boost::python::objects::add_to_namespace(sceneNode, 
		"isPatch", boost::python::make_function(&ScriptPatchNode::isPatch));

	boost::python::objects::add_to_namespace(sceneNode, 
		"getPatch", boost::python::make_function(&ScriptPatchNode::getPatch));
	
	// Define the GlobalPatchCreator interface
	nspace["GlobalPatchCreator"] = boost::python::class_<PatchInterface>("GlobalPatchCreator")
		.def("createPatchDef2", &PatchInterface::createPatchDef2)
		.def("createPatchDef3", &PatchInterface::createPatchDef3)
	;

	// Now point the Python variable "GlobalPatchCreator" to this instance
	nspace["GlobalPatchCreator"] = boost::python::ptr(this);
}

} // namespace script
