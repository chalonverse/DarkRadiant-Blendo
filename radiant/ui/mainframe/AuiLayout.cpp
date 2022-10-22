#include "AuiLayout.h"

#include "i18n.h"
#include "ui/imenumanager.h"
#include "ui/imainframe.h"
#include "ui/iuserinterface.h"

#include <wx/sizer.h>
#include <wx/aui/auibook.h>

#include "camera/CameraWndManager.h"
#include "command/ExecutionFailure.h"
#include "ui/texturebrowser/TextureBrowser.h"
#include "wxutil/Bitmap.h"
#include "xyview/GlobalXYWnd.h"

namespace ui
{

namespace
{
    const std::string RKEY_ROOT = "user/ui/mainFrame/aui/";
    const std::string RKEY_AUI_PERSPECTIVE = RKEY_ROOT + "perspective";
    const std::string RKEY_AUI_PANES = RKEY_ROOT + "panes";
    const std::string RKEY_AUI_LAYOUT_VERSION = RKEY_ROOT + "layoutVersion";
    const std::string PANE_NODE_NAME = "pane";
    const std::string PANE_NAME_ATTRIBUTE = "paneName";
    const std::string CONTROL_NAME_ATTRIBUTE = "controlName";
    constexpr int AuiLayoutVersion = 1;

    // Minimum size of docked panels
    const wxSize MIN_SIZE(128, 128);

    // Return a pane info with default options
    wxAuiPaneInfo DEFAULT_PANE_INFO(const std::string& caption,
                                    const wxSize& minSize)
    {
        return wxAuiPaneInfo().Caption(caption).CloseButton(false).MaximizeButton()
                              .BestSize(minSize).MinSize(minSize).DestroyOnClose(true);
    }

    void setupFloatingPane(wxAuiPaneInfo& pane)
    {
        pane.Float().CloseButton(true).MinSize(128, 128);
    }
}

AuiLayout::AuiLayout() :
    _auiMgr(nullptr, wxAUI_MGR_ALLOW_FLOATING | wxAUI_MGR_VENETIAN_BLINDS_HINT | wxAUI_MGR_LIVE_RESIZE),
    _propertyNotebook(nullptr)
{
    _auiMgr.GetArtProvider()->SetMetric(wxAUI_DOCKART_GRADIENT_TYPE, wxAUI_GRADIENT_NONE);
    _auiMgr.Bind(wxEVT_AUI_PANE_CLOSE, &AuiLayout::onPaneClose, this);
}

bool AuiLayout::paneNameExists(const std::string& name) const
{
    for (const auto& info : _panes)
    {
        if (info.paneName == name)
        {
            return true;
        }
    }

    return false;
}

bool AuiLayout::controlExists(const std::string& controlName) const
{
    // Check the notebook first
    if (_propertyNotebook->controlExists(controlName))
    {
        return true;
    }

    for (const auto& info : _panes)
    {
        if (info.controlName == controlName)
        {
            return true;
        }
    }

    return false;
}

std::string AuiLayout::generateUniquePaneName(const std::string& controlName)
{
    auto paneName = controlName;
    auto index = 1;

    while (paneNameExists(paneName))
    {
        paneName += fmt::format("{0}{1}", controlName, ++index);
    }

    return paneName;
}

void AuiLayout::addPane(const std::string& controlName, wxWindow* window, const wxAuiPaneInfo& info)
{
    // Give the pane a unique name so we can restore perspectives
    addPane(controlName, generateUniquePaneName(controlName), window, info);
}

void AuiLayout::addPane(const std::string& controlName, const std::string& paneName, wxWindow* window, const wxAuiPaneInfo& info)
{
    wxAuiPaneInfo paneInfo = info;
    paneInfo.Name(paneName);

    // Add and store the pane
    _auiMgr.AddPane(window, paneInfo);
    // Remember this pane
    _panes.push_back({ paneName, controlName, window });
}

void AuiLayout::convertPaneToPropertyTab(const std::string& paneName)
{
    for (auto i = _panes.begin(); i != _panes.end(); ++i)
    {
        if (i->paneName != paneName) continue;

        // Close the pane if it's present
        if (auto paneInfo = _auiMgr.GetPane(i->paneName); paneInfo.IsOk())
        {
            _auiMgr.ClosePane(paneInfo);
        }

        _propertyNotebook->addControl(i->controlName);
        _panes.erase(i);
        break;
    }
}

void AuiLayout::onPaneClose(wxAuiManagerEvent& ev)
{
    // For floating panes, memorise the last size and position
    auto closedPane = ev.GetPane();

    if (closedPane->IsFloating())
    {
        // Store the position of this pane before closing
        _floatingPaneLocations[closedPane->name.ToStdString()] = _auiMgr.SavePaneInfo(*closedPane).ToStdString();
    }

    // This is a desperate work around to let undocked property windows
    // return to the property notebook when they're closed
    // I failed finding any other way to have floating windows dragged into
    // the notebook, or adding a custom pane button - I'm open to ideas

    for (auto i = _panes.begin(); i != _panes.end(); ++i)
    {
        if (i->paneName != closedPane->name) continue;

        _propertyNotebook->addControl(i->controlName);
        _panes.erase(i);
        break;
    }
}

std::string AuiLayout::getName()
{
    return AUI_LAYOUT_NAME;
}

void AuiLayout::saveStateToRegistry()
{
    registry::setValue(RKEY_AUI_LAYOUT_VERSION, AuiLayoutVersion);

    // Save the pane perspective
    registry::setValue(RKEY_AUI_PERSPECTIVE, _auiMgr.SavePerspective().ToStdString());

    // Save tracked panes, we need to create all named panes before we can load the perspective
    GlobalRegistry().deleteXPath(RKEY_AUI_PANES);

    auto panesKey = GlobalRegistry().createKey(RKEY_AUI_PANES);

    for (const auto& pane : _panes)
    {
        auto& paneInfo = _auiMgr.GetPane(pane.control);

        // Skip hidden panels
        if (!paneInfo.IsShown())
        {
            continue;
        }

        auto paneNode = panesKey.createChild(PANE_NODE_NAME);
        paneNode.setAttributeValue(CONTROL_NAME_ATTRIBUTE, pane.controlName);
        paneNode.setAttributeValue(PANE_NAME_ATTRIBUTE, pane.paneName);
    }

    // Save property notebook stae
    _propertyNotebook->saveState();
}

void AuiLayout::activate()
{
    auto topLevelParent = GlobalMainFrame().getWxTopLevelWindow();

    // AUI manager can't manage a Sizer, we need to create an actual wxWindow
    // container
    auto managedArea = new wxWindow(topLevelParent, wxID_ANY);
    _auiMgr.SetManagedWindow(managedArea);
    GlobalMainFrame().getWxMainContainer()->Add(managedArea, 1, wxEXPAND);

    _propertyNotebook = new PropertyNotebook(managedArea, *this);

    auto orthoViewControl = GlobalUserInterface().findControl(UserControl::OrthoView);
    auto cameraControl = GlobalUserInterface().findControl(UserControl::Camera);
    assert(cameraControl);
    assert(orthoViewControl);

    // Add the camera and notebook to the left, as with the Embedded layout, and
    // the 2D view on the right
    wxSize size = topLevelParent->GetSize();
    size.Scale(0.5, 1.0);

    addPane(cameraControl->getControlName(), cameraControl->createWidget(managedArea),
            DEFAULT_PANE_INFO(cameraControl->getDisplayName(), size).Left().Position(0));
    addPane("PropertiesPanel", _propertyNotebook,
            DEFAULT_PANE_INFO(_("Properties"), size).Left().Position(1));
    addPane(orthoViewControl->getControlName(), orthoViewControl->createWidget(managedArea),
            DEFAULT_PANE_INFO(orthoViewControl->getDisplayName(), size).CenterPane());
    _auiMgr.Update();

    // Hide the camera toggle option for non-floating views
    GlobalMenuManager().setVisibility("main/view/cameraview", false);
    // Hide the console/texture browser toggles for non-floating/non-split views
    GlobalMenuManager().setVisibility("main/view/textureBrowser", false);
}

void AuiLayout::deactivate()
{
    // Delete all active views
    GlobalXYWndManager().destroyViews();

    // Get a reference to the managed window, it might be cleared by UnInit()
    auto managedWindow = _auiMgr.GetManagedWindow();

    // Unregister the AuiMgr from the event handlers of the managed window
    // otherwise we run into crashes during shutdown (#5586)
    _auiMgr.UnInit();

    managedWindow->Destroy();
}

void AuiLayout::createPane(const std::string& controlName, const std::string& paneName,
    const std::function<void(wxAuiPaneInfo&)>& setupPane)
{
    auto control = GlobalUserInterface().findControl(controlName);

    if (!control)
    {
        rError() << "Cannot find named control: " << controlName << std::endl;
        return;
    }

    auto managedWindow = _auiMgr.GetManagedWindow();

    auto pane = DEFAULT_PANE_INFO(control->getDisplayName(), MIN_SIZE);
    pane.name = paneName;

    // Run client code to setup the pane properties
    setupPane(pane);

    if (!control->getIcon().empty())
    {
        pane.Icon(wxutil::GetLocalBitmap(control->getIcon()));
    }

    auto widget = control->createWidget(managedWindow);

    // Fit and determine the floating size automatically
    widget->Fit();

    // Some additional height for the window title bar
    pane.FloatingSize(widget->GetSize().x, widget->GetSize().y + 30);

    _auiMgr.AddPane(widget, pane);
    _panes.push_back({ paneName, controlName, widget });
}

void AuiLayout::createFloatingControl(const std::string& controlName)
{
    createPane(controlName, generateUniquePaneName(controlName), [this](auto& paneInfo)
    {
        setupFloatingPane(paneInfo);

        // Check if we got existing size information for this floating pane
        auto existingSizeInfo = _floatingPaneLocations.find(paneInfo.name.ToStdString());

        if (existingSizeInfo != _floatingPaneLocations.end())
        {
            _auiMgr.LoadPaneInfo(existingSizeInfo->second, paneInfo);
        }
    });

    _auiMgr.Update();
}

void AuiLayout::addControl(const std::string& controlName, const IMainFrame::ControlSettings& defaultSettings)
{
    _defaultControlSettings[controlName] = defaultSettings;

    if (defaultSettings.visible)
    {
        createControl(controlName);
    }
}

void AuiLayout::createControl(const std::string& controlName)
{
    auto defaultSettings = _defaultControlSettings.find(controlName);

    if (defaultSettings == _defaultControlSettings.end())
    {
        return;
    }

    switch (defaultSettings->second.location)
    {
    case IMainFrame::Location::PropertyPanel:
        _propertyNotebook->addControl(controlName);
        break;

    case IMainFrame::Location::FloatingWindow:
        createFloatingControl(controlName);
        break;
    }
}

void AuiLayout::focusControl(const std::string& controlName)
{
    // Locate the control, is it loaded anywhere
    if (!controlExists(controlName))
    {
        // Control is not loaded anywhere, check the named settings
        // Check the default settings if there's a control
        if (_defaultControlSettings.count(controlName) == 0)
        {
            throw cmd::ExecutionFailure(fmt::format(_("Cannot focus unknown control {0}"), controlName));
        }

        // Create the control in its default location
        createControl(controlName);
    }

    // Focus matching controls in the property notebook
    _propertyNotebook->focusControl(controlName);

    // Unset the focus of any wxPanels in floating windows
    for (auto p = _panes.begin(); p != _panes.end(); ++p)
    {
        if (p->controlName != controlName) continue;

        auto& paneInfo = _auiMgr.GetPane(p->control);

        // Show hidden panes
        if (!paneInfo.IsShown())
        {
            paneInfo.Show();
            _auiMgr.Update();
        }

        if (auto panel = wxDynamicCast(p->control, wxPanel); panel != nullptr)
        {
            panel->SetFocusIgnoringChildren();
        }
        break;
    }
}

void AuiLayout::toggleControl(const std::string& controlName)
{
    // Locate the control, is it loaded anywhere?
    if (!controlExists(controlName))
    {
        // Control is not loaded anywhere, check the named settings
        // Check the default settings if there's a control
        if (_defaultControlSettings.count(controlName) == 0)
        {
            throw cmd::ExecutionFailure(fmt::format(_("Cannot toggle unknown control {0}"), controlName));
        }

        // Create the control in its default location
        createControl(controlName);
        // Bring it to front
        focusControl(controlName);
        return;
    }

    // Control exists, we can toggle
    // If the control is a property tab, then just focus it
    if (_propertyNotebook->controlExists(controlName))
    {
        // Focus matching controls in the property notebook
        _propertyNotebook->focusControl(controlName);
        return;
    }

    // If it's a docked pane, then do nothing, otherwise toggle its visibility
    for (auto p = _panes.begin(); p != _panes.end(); ++p)
    {
        if (p->controlName == controlName)
        {
            auto& paneInfo = _auiMgr.GetPane(p->control);

            if (!paneInfo.IsDocked())
            {
                paneInfo.Show(!paneInfo.IsShown());
                _auiMgr.Update();
            }
            break;
        }
    }
}

void AuiLayout::restoreStateFromRegistry()
{
    // Check the saved version
    if (registry::getValue<int>(RKEY_AUI_LAYOUT_VERSION) != AuiLayoutVersion)
    {
        rMessage() << "No compatible AUI layout state information found in registry" << std::endl;
        return;
    }

    // Restore all missing panes, this has to be done before the perspective is restored
    for (const auto& node : GlobalRegistry().findXPath(RKEY_AUI_PANES + "//*"))
    {
        if (node.getName() != PANE_NODE_NAME) continue;

        auto controlName = node.getAttributeValue(CONTROL_NAME_ATTRIBUTE);
        auto paneName = node.getAttributeValue(PANE_NAME_ATTRIBUTE);

        if (paneNameExists(paneName)) continue; // this one already exists

        createPane(controlName, paneName, setupFloatingPane);
    }

    // Restore the property notebook state
    _propertyNotebook->restoreState();

    // Nasty hack to get the panes sized properly. Since BestSize() is
    // completely ignored (at least on Linux), we have to add the panes with a
    // large *minimum* size and then reset this size after the initial addition.
    for (const auto& info : _panes)
    {
        _auiMgr.GetPane(info.control).MinSize(MIN_SIZE);
    }

    _auiMgr.Update();

    // If we have a stored perspective, load it
    auto storedPersp = registry::getValue<std::string>(RKEY_AUI_PERSPECTIVE);

    if (!storedPersp.empty())
    {
        _auiMgr.LoadPerspective(storedPersp);
    }

    // Restore all floating XY views
    GlobalXYWnd().restoreState();
}

void AuiLayout::toggleFullscreenCameraView()
{
}

std::shared_ptr<AuiLayout> AuiLayout::CreateInstance()
{
    return std::make_shared<AuiLayout>();
}

} // namespace ui
