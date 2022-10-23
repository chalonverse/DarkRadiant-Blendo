#pragma once

#include <memory>
#include "ui/imainframelayout.h"

#include "camera/CamWnd.h"

#include "AuiManager.h"
#include "PropertyNotebook.h"

namespace ui
{

constexpr const char* const AUI_LAYOUT_NAME = "Dockable";

class AuiFloatingFrame;

/// Layout based on wxWidgets AUI (dock widget interface)
class AuiLayout : public IMainFrameLayout
{
    // Main AUI manager
    AuiManager _auiMgr;
    PropertyNotebook* _propertyNotebook;

    struct PaneInfo
    {
        std::string paneName;
        std::string controlName;
        wxWindow* control;
    };

    // List of panes managed by the AUI manager
    std::list<PaneInfo> _panes;

    std::map<std::string, IMainFrame::ControlSettings> _defaultControlSettings;

    // Stored floating window locations
    std::map<std::string, std::string> _floatingPaneLocations;

public:
    AuiLayout();

	// IMainFrameLayout implementation
	std::string getName() override;
	void activate() override;
	void deactivate() override;
	void toggleFullscreenCameraView() override;
	void saveStateToRegistry();
	void restoreStateFromRegistry() override;
    void createFloatingControl(const std::string& controlName) override;

    void registerControl(const std::string& controlName, const IMainFrame::ControlSettings& defaultSettings);

    // Creates the named control at its registered default location
    void createControl(const std::string& controlName);
    void focusControl(const std::string& controlName);
    void toggleControl(const std::string& controlName);

    void ensureControlIsActive(wxWindow* control);
    void ensureControlIsInactive(wxWindow* control);

	// The creation function, needed by the mainframe layout manager
	static std::shared_ptr<AuiLayout> CreateInstance();

    // Internally used by the AuiManager implementation
    void convertFloatingPaneToPropertyTab(AuiFloatingFrame* floatingWindow);

private:
    // Add a pane to the wxAuiManager and store it in the list
    void addPane(const std::string& controlName, wxWindow* window, const wxAuiPaneInfo& info);
    void addPane(const std::string& controlName, const std::string& paneName, wxWindow* window, const wxAuiPaneInfo& info);

    void createPane(const std::string& controlName, const std::string& paneName,
        const std::function<void(wxAuiPaneInfo&)>& setupPane);

    void onPaneClose(wxAuiManagerEvent& ev);
    bool paneNameExists(const std::string& name) const;

    // Returns true if the control is loaded in the notebook or in a pane
    bool controlExists(const std::string& controlName) const;

    std::string generateUniquePaneName(const std::string& controlName);

    void convertPaneToPropertyTab(const std::string& paneName);
};

} // namespace
