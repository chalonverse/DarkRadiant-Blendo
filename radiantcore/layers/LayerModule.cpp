#include "ilayer.h"
#include "icommandsystem.h"
#include "itextstream.h"
#include "imapinfofile.h"
#include "imap.h"

#include "module/StaticModule.h"
#include "LayerManager.h"
#include "LayerInfoFileModule.h"

namespace scene
{

namespace
{

const char* const COMMAND_CREATELAYER("CreateLayer");
const char* const COMMAND_ADDTOLAYER("AddSelectionToLayer");
const char* const COMMAND_MOVETOLAYER("MoveSelectionToLayer");
const char* const COMMAND_REMOVEFROMLAYER("RemoveSelectionFromLayer");
const char* const COMMAND_SHOWLAYER("ShowLayer");
const char* const COMMAND_HIDELAYER("HideLayer");

inline void DoWithMapLayerManager(const std::function<void(scene::ILayerManager&)>& func)
{
	if (!GlobalMapModule().getRoot())
	{
		rError() << "No map loaded, cannot do this." << std::endl;
		return;
	}

	func(GlobalMapModule().getRoot()->getLayerManager());
}

}

class LayerModule :
	public ILayerModule
{
public:
	const std::string& getName() const override
	{
		static std::string _name(MODULE_LAYERS);
		return _name;
	}

	const StringSet& getDependencies() const override
	{
		static StringSet _dependencies;

		if (_dependencies.empty())
		{
			_dependencies.insert(MODULE_COMMANDSYSTEM);
			_dependencies.insert(MODULE_MAPINFOFILEMANAGER);
		}

		return _dependencies;
	}

	void initialiseModule(const IApplicationContext& ctx) override
	{
		rMessage() << getName() << "::initialiseModule called." << std::endl;

		GlobalCommandSystem().addCommand(COMMAND_ADDTOLAYER,
			std::bind(&LayerModule::addSelectionToLayer, this, std::placeholders::_1),
			{ cmd::ARGTYPE_INT });

		GlobalCommandSystem().addCommand(COMMAND_MOVETOLAYER,
			std::bind(&LayerModule::moveSelectionToLayer, this, std::placeholders::_1),
			{ cmd::ARGTYPE_INT });
        
        GlobalCommandSystem().addCommand(COMMAND_REMOVEFROMLAYER,
			std::bind(&LayerModule::removeSelectionFromLayer, this, std::placeholders::_1),
			{ cmd::ARGTYPE_INT });

		GlobalCommandSystem().addCommand(COMMAND_SHOWLAYER,
			std::bind(&LayerModule::showLayer, this, std::placeholders::_1),
			{ cmd::ARGTYPE_INT });

		GlobalCommandSystem().addCommand(COMMAND_HIDELAYER,
			std::bind(&LayerModule::hideLayer, this, std::placeholders::_1),
			{ cmd::ARGTYPE_INT });

        GlobalCommandSystem().addCommand(COMMAND_CREATELAYER,
            std::bind(&LayerModule::createLayer, this, std::placeholders::_1),
            { cmd::ARGTYPE_STRING });

		GlobalMapInfoFileManager().registerInfoFileModule(
			std::make_shared<LayerInfoFileModule>()
		);
	}

	ILayerManager::Ptr createLayerManager() override
	{
		return std::make_shared<LayerManager>();
	}

private:
    void createLayer(const cmd::ArgumentList& args)
    {
        if (args.size() != 1)
        {
            rError() << "Usage: " << COMMAND_CREATELAYER << " <LayerName> " << std::endl;
            return;
        }

        DoWithMapLayerManager([&](ILayerManager& manager)
        {
            manager.createLayer(args[0].getString());
            GlobalMapModule().setModified(true);
        });
    }

	void addSelectionToLayer(const cmd::ArgumentList& args)
	{
		if (args.size() != 1)
		{
			rError() << "Usage: " << COMMAND_ADDTOLAYER << " <LayerID> " << std::endl;
			return;
		}

		DoWithMapLayerManager([&](ILayerManager& manager)
		{
			manager.addSelectionToLayer(args[0].getInt());
            GlobalMapModule().setModified(true);
		});
	}

	void moveSelectionToLayer(const cmd::ArgumentList& args)
	{
		if (args.size() != 1)
		{
			rError() << "Usage: " << COMMAND_MOVETOLAYER << " <LayerID> " << std::endl;
			return;
		}

		DoWithMapLayerManager([&](ILayerManager& manager)
		{
			manager.moveSelectionToLayer(args[0].getInt());
            GlobalMapModule().setModified(true);
		});
	}

    void removeSelectionFromLayer(const cmd::ArgumentList& args)
	{
		if (args.size() != 1)
		{
			rError() << "Usage: " << COMMAND_REMOVEFROMLAYER << " <LayerID> " << std::endl;
			return;
		}

		DoWithMapLayerManager([&](ILayerManager& manager)
		{
			manager.removeSelectionFromLayer(args[0].getInt());
            GlobalMapModule().setModified(true);
		});
	}

	void showLayer(const cmd::ArgumentList& args)
	{
		if (args.size() != 1)
		{
			rError() << "Usage: " << COMMAND_SHOWLAYER << " <LayerID> " << std::endl;
			return;
		}

		DoWithMapLayerManager([&](ILayerManager& manager)
		{
			manager.setLayerVisibility(args[0].getInt(), true);
		});
	}

	void hideLayer(const cmd::ArgumentList& args)
	{
		if (args.size() != 1)
		{
			rError() << "Usage: " << COMMAND_HIDELAYER << " <LayerID> " << std::endl;
			return;
		}

		DoWithMapLayerManager([&](ILayerManager& manager)
		{
			manager.setLayerVisibility(args[0].getInt(), false);
		});
	}
};

module::StaticModule<LayerModule> layerManagerFactoryModule;

}
