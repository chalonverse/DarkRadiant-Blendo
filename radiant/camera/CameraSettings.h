#pragma once

#include <string>
#include "iregistry.h"

#include <cmath>
#include "igl.h"
#include <sigc++/signal.h>

namespace ui
{

/* greebo: This is the home of all the camera settings. As this class observes
 * the registry it can be connected to the according registry keys and gets
 * notified if any of the observed keys are changed.*/

namespace 
{
	const int MAX_CUBIC_SCALE = 23;
	const int MAX_CAMERA_SPEED = 300;

	const std::string RKEY_CAMERA_ROOT = "user/ui/camera";
	const std::string RKEY_MOVEMENT_SPEED = RKEY_CAMERA_ROOT + "/movementSpeed";
	const std::string RKEY_ROTATION_SPEED = RKEY_CAMERA_ROOT + "/rotationSpeed";
	const std::string RKEY_INVERT_MOUSE_VERTICAL_AXIS = RKEY_CAMERA_ROOT + "/invertMouseVerticalAxis";
	const std::string RKEY_DISCRETE_MOVEMENT = RKEY_CAMERA_ROOT + "/discreteMovement";
	const std::string RKEY_CUBIC_SCALE = RKEY_CAMERA_ROOT + "/cubicScale";
	const std::string RKEY_ENABLE_FARCLIP = RKEY_CAMERA_ROOT + "/enableCubicClipping";
	const std::string RKEY_DRAWMODE = RKEY_CAMERA_ROOT + "/drawMode";
	const std::string RKEY_SOLID_SELECTION_BOXES = "user/ui/xyview/solidSelectionBoxes";
	const std::string RKEY_TOGGLE_FREE_MOVE = RKEY_CAMERA_ROOT + "/toggleFreeMove";
	const std::string RKEY_CAMERA_WINDOW_STATE = RKEY_CAMERA_ROOT + "/window";
    const std::string RKEY_SHOW_CAMERA_TOOLBAR = RKEY_CAMERA_ROOT + "/showToolbar";
    const std::string RKEY_CAMERA_FONT_SIZE = RKEY_CAMERA_ROOT + "/fontSize";
    const std::string RKEY_CAMERA_FONT_STYLE = RKEY_CAMERA_ROOT + "/fontStyle";
    const std::string RKEY_CAMERA_GRID_ENABLED = RKEY_CAMERA_ROOT + "/gridEnabled";
    const std::string RKEY_CAMERA_GRID_SPACING = RKEY_CAMERA_ROOT + "/gridSpacing";
    const std::string RKEY_CAMERA_COLOR_IN_FULL_BRIGHT = RKEY_CAMERA_ROOT + "/colorInFullBright";
}

inline float calculateFarPlaneDistance(int cubicScale)
{
    return pow(2.0, (cubicScale + 7) / 2.0);
}

enum CameraDrawMode 
{
	RENDER_MODE_WIREFRAME,
	RENDER_MODE_SOLID,
    RENDER_MODE_TEXTURED,
    RENDER_MODE_LIGHTING
};

class CameraSettings: public sigc::trackable
{
	bool _callbackActive;

	int _movementSpeed;
	int _angleSpeed;

	bool _invertMouseVerticalAxis;
	bool _discreteMovement;

	CameraDrawMode _cameraDrawMode;

	int _cubicScale;
	bool _farClipEnabled;
	bool _solidSelectionBoxes;
	// This is TRUE if the mousebutton must be held to stay in freelook mode
	// instead of enabling it by clicking and clicking again to disable
	bool _toggleFreelook;

	bool _gridEnabled;
	int _gridSpacing;

    bool _colorInFullBright;

    // Signals
    sigc::signal<void> _sigRenderModeChanged;

private:
    void observeKey(const std::string& key);
	void keyChanged();

public:
	CameraSettings();

	int movementSpeed() const;
	int angleSpeed() const;

	// Returns true if cubic clipping is on
	bool farClipEnabled() const;
	bool invertMouseVerticalAxis() const;
	bool discreteMovement() const;
	bool solidSelectionBoxes() const;
	bool toggleFreelook() const;

    /// Whether to show the camera toolbar
    bool showCameraToolbar() const;

    bool gridEnabled() const;
    int gridSpacing() const;

    bool colorInFullBright() const;

	// Sets/returns the draw mode (wireframe, solid, textured, lighting)
	CameraDrawMode getRenderMode() const;
	void setRenderMode(const CameraDrawMode& mode);
	void toggleLightingMode();

	// Gets/Sets the cubic scale member variable (is automatically constrained [1..MAX_CUBIC_SCALE])
	int cubicScale() const;
	void setCubicScale(int scale);

	// Enables/disables the cubic clipping
	void toggleFarClip(bool newState);
	void setFarClip(bool farClipEnabled);

    int fontSize() const;
    IGLFont::Style fontStyle() const;

	// Adds the elements to the "camera" preference page
	void constructPreferencePage();

    /* SIGNALS */

    /// Emitted when the render mode is changed, e.g. by the F3 key.
    sigc::signal<void> signalRenderModeChanged()
    {
        return _sigRenderModeChanged;
    }

private:
	void importDrawMode(const int mode);
}; // class CameraSettings

} // namespace

ui::CameraSettings* getCameraSettings();
