#include "SelectionGroupManager.h"

#include "i18n.h"
#include "imap.h"
#include "itextstream.h"
#include "imapinfofile.h"
#include "icommandsystem.h"
#include "iselection.h"
#include "ieventmanager.h"
#include "imainframe.h"
#include "modulesystem/StaticModule.h"

#include "wxutil/dialog/MessageBox.h"
#include "selectionlib.h"
#include "SelectionGroup.h"
#include "SelectableNode.h"
#include "SelectionGroupInfoFileModule.h"

namespace selection
{

const std::string& SelectionGroupManager::getName() const
{
	static std::string _name(MODULE_SELECTIONGROUP);
	return _name;
}

const StringSet& SelectionGroupManager::getDependencies() const
{
	static StringSet _dependencies;

	if (_dependencies.empty())
	{
		_dependencies.insert(MODULE_SELECTIONSYSTEM);
		_dependencies.insert(MODULE_EVENTMANAGER);
		_dependencies.insert(MODULE_COMMANDSYSTEM);
		_dependencies.insert(MODULE_RADIANT);
		_dependencies.insert(MODULE_MAP);
		_dependencies.insert(MODULE_MAPINFOFILEMANAGER);
	}

	return _dependencies;
}

void SelectionGroupManager::initialiseModule(const ApplicationContext& ctx)
{
	rMessage() << getName() << "::initialiseModule called." << std::endl;

	GlobalCommandSystem().addCommand("GroupSelected",
		std::bind(&SelectionGroupManager::groupSelectedCmd, this, std::placeholders::_1));
	GlobalCommandSystem().addCommand("UngroupSelected",
		std::bind(&SelectionGroupManager::ungroupSelectedCmd, this, std::placeholders::_1));
	GlobalCommandSystem().addCommand("DeleteAllSelectionGroups",
		std::bind(&SelectionGroupManager::deleteAllSelectionGroupsCmd, this, std::placeholders::_1));

	GlobalEventManager().addCommand("GroupSelected", "GroupSelected");
	GlobalEventManager().addCommand("UngroupSelected", "UngroupSelected");
	GlobalEventManager().addCommand("DeleteAllSelectionGroups", "DeleteAllSelectionGroups");

	GlobalMapModule().signal_mapEvent().connect(
		sigc::mem_fun(*this, &SelectionGroupManager::onMapEvent)
	);

	GlobalMapInfoFileManager().registerInfoFileModule(
		std::make_shared<SelectionGroupInfoFileModule>()
	);
}

void SelectionGroupManager::onMapEvent(IMap::MapEvent ev)
{
	if (ev == IMap::MapUnloaded)
	{
		deleteAllSelectionGroups();
	}
}

ISelectionGroupPtr SelectionGroupManager::createSelectionGroup()
{
	// Create a new group ID
	std::size_t id = generateGroupId();

	SelectionGroupPtr group = std::make_shared<SelectionGroup>(id);
	_groups[id] = group;

	return group;
}

void SelectionGroupManager::setGroupSelected(std::size_t id, bool selected)
{
	SelectionGroupMap::iterator found = _groups.find(id);

	if (found == _groups.end())
	{
		rError() << "Cannot find the group with ID " << id << std::endl;
		return;
	}

	found->second->setSelected(selected);
}

void SelectionGroupManager::deleteSelectionGroup(std::size_t id)
{
	SelectionGroupMap::iterator found = _groups.find(id);

	if (found == _groups.end())
	{
		rError() << "Cannot delete the group with ID " << id << " as it doesn't exist." << std::endl;
		return;
	}

	found->second->removeAllNodes();

	_groups.erase(found);
}

void SelectionGroupManager::deleteAllSelectionGroups()
{
	for (SelectionGroupMap::iterator g = _groups.begin(); g != _groups.end(); )
	{
		deleteSelectionGroup((g++)->first);
	}

	assert(_groups.empty());
}

void SelectionGroupManager::foreachSelectionGroup(const std::function<void(ISelectionGroup&)>& func)
{
	for (SelectionGroupMap::value_type& pair : _groups)
	{
		func(*pair.second);
	}
}

ISelectionGroupPtr SelectionGroupManager::createSelectionGroupInternal(std::size_t id)
{
	if (_groups.find(id) != _groups.end())
	{
		rWarning() << "Cannot create group with ID " << id << ", as it's already taken." << std::endl;
		throw std::runtime_error("Group ID already taken");
	}

	SelectionGroupPtr group = std::make_shared<SelectionGroup>(id);
	_groups[id] = group;

	return group;
}

void SelectionGroupManager::deleteAllSelectionGroupsCmd(const cmd::ArgumentList& args)
{
	deleteAllSelectionGroups();
}

void SelectionGroupManager::groupSelectedCmd(const cmd::ArgumentList& args)
{
	if (GlobalSelectionSystem().Mode() != SelectionSystem::ePrimitive)
	{
		rError() << "Must be in primitive selection mode to form groups." << std::endl;
		wxutil::Messagebox::ShowError(_("Groups can be formed in Primitive selection mode only"));
		return;
	}

	if (GlobalSelectionSystem().getSelectionInfo().totalCount == 0)
	{
		rError() << "Nothing selected, cannot group anything." << std::endl;
		wxutil::Messagebox::ShowError(_("Nothing selected, cannot group anything"));
		return;
	}
	
	if (GlobalSelectionSystem().getSelectionInfo().totalCount == 1)
	{
		rError() << "Select more than one element to form a group." << std::endl;
		wxutil::Messagebox::ShowError(_("Select more than one element to form a group"));
		return;
	}

	// Check if the current selection already is member of the same group
	std::set<std::size_t> groupIds;
	bool hasUngroupedNode = false;

	GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
	{
		std::shared_ptr<IGroupSelectable> selectable = std::dynamic_pointer_cast<IGroupSelectable>(node);

		if (!selectable) return;

		if (!selectable->getGroupIds().empty())
		{
			groupIds.insert(selectable->getMostRecentGroupId());
		}
		else
		{
			hasUngroupedNode = true;
		}
	});

	if (!hasUngroupedNode && groupIds.size() == 1)
	{
		rError() << "The selected elements already form a group" << std::endl;
		wxutil::Messagebox::ShowError(_("The selected elements already form a group"));
		return;
	}

	ISelectionGroupPtr group = createSelectionGroup();

	GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
	{
		group->addNode(node);
	});

	GlobalMainFrame().updateAllWindows();
}

void SelectionGroupManager::ungroupSelectedCmd(const cmd::ArgumentList& args)
{
	// Collect all the latest group Ids from all selected nodes
	std::set<std::size_t> ids;

	GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
	{
		std::shared_ptr<scene::SelectableNode> selectable = std::dynamic_pointer_cast<scene::SelectableNode>(node);

		if (!selectable) return;

		if (selectable->isGroupMember())
		{
			ids.insert(selectable->getMostRecentGroupId());
		}
	});

	// Now remove the found group by ID (maybe convert them to a selection set before removal?)
	std::for_each(ids.begin(), ids.end(), [this](std::size_t id)
	{
		deleteSelectionGroup(id);
	});

	GlobalMainFrame().updateAllWindows();
}

std::size_t SelectionGroupManager::generateGroupId()
{
	for (std::size_t i = 0; i < std::numeric_limits<std::size_t>::max(); ++i)
	{
		if (_groups.find(i) == _groups.end())
		{
			// Found a free ID
			return i;
		}
	}

	throw std::runtime_error("Out of group IDs.");
}

module::StaticModule<SelectionGroupManager> staticSelectionGroupManagerModule;

SelectionGroupManager& getSelectionGroupManagerInternal()
{
	return *staticSelectionGroupManagerModule.getModule();
}

}
