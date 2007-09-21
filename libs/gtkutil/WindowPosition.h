#ifndef WINDOWPOSITION_H_
#define WINDOWPOSITION_H_

#include "gtk/gtkwindow.h"
#include "math/Vector2.h"
#include "xmlutil/Node.h"

/* greebo: A WindowPosition object keeps track of the window's size and position. 
 * 
 * Use the connect() method to connect a GtkWindow to this object.
 * 
 * Use the loadFromNode() and saveToNode() methods to save the internal
 * size info into the given xml::Node
 * 
 * This is used by the XYWnd classes to save/restore the window state upon restart.
 */
namespace gtkutil {

	namespace {
		typedef BasicVector2<int> PositionVector;
		typedef BasicVector2<int> SizeVector;
	}

class WindowPosition {
	// The size and position of this object
	PositionVector _position;
	SizeVector _size;
	
	// The connected window
	GtkWindow* _window;

public:
	WindowPosition();

	// Connect the passed window to this object
	void connect(GtkWindow* window);

	const PositionVector getPosition() const;
	const SizeVector getSize() const;

	void setPosition(const int& x, const int& y);
	void setSize(const int& width, const int& height);
	
	void saveToNode(xml::Node& node);
	void loadFromNode(const xml::Node& node);
	
	// Applies the internally stored size/position info to the GtkWindow
	// The algorithm was adapted from original GtkRadiant code (window.h) 
	void applyPosition();

	// Reads the position from the GtkWindow
	void readPosition();
	
private:

	// The static GTK callback that gets invoked on window size/position changes
	static gboolean onConfigure(GtkWidget* widget, GdkEventConfigure *event, WindowPosition* self);

}; // class WindowPosition

} // namespace gtkutil

#endif /*WINDOWPOSITION_H_*/
