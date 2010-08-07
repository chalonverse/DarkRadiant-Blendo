#ifndef SHADERSELECTOR_H_
#define SHADERSELECTOR_H_

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>
#include "gtkutil/GLWidget.h"

#include <gtkmm/box.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treeselection.h>

/* FORWARD DECLS */
class Material;
typedef boost::shared_ptr<Material> MaterialPtr;

namespace Gtk
{
	class Widget;
	class TreeView;
}

namespace ui
{

/** A widget that allows the selection of a shader. The widget contains
 * three elements - a tree view displaying available shaders as
 * identified by the specified prefixes, an OpenGL widget displaying a 
 * preview of the currently-selected shader, and a table containing certain
 * information about the shader.
 * 
 * Use the GtkWidget* operator to incorporate this class into a dialog window.
 * 
 * This widget populates its list of shaders automatically, and offers a method
 * that allows calling code to retrieve the user's selection. The set of 
 * displayed textures can be defined by passing a list of texture prefixes to
 * the constructor (comma-separated, e.g. "fog,light"). 
 * 
 * The client class has to derive from the abstract ShaderSelector::Client class
 * providing an interface to allow the update of the info liststore.
 * 
 * For convenience purposes, this class provides two static members that
 * allow populating the infostore widget with common lightshader/shader information.
 * 
 */
class ShaderSelector :
	public Gtk::VBox
{
public:
	
	// Derive from this class to update the info widget
	class Client
	{
	public:
	    /// destructor
		virtual ~Client() {}
		/** greebo: This gets invoked upon selection changed to allow the client
		 * 			class to display custom information in the selector's liststore.  
		 */
		virtual void shaderSelectionChanged(const std::string& shader, 
											const Glib::RefPtr<Gtk::ListStore>& listStore) = 0;
	};

	struct ShaderTreeColumns : 
		public Gtk::TreeModel::ColumnRecord
	{
		ShaderTreeColumns() { add(displayName); add(shaderName); add(icon); }

		Gtk::TreeModelColumn<Glib::ustring> displayName;
		Gtk::TreeModelColumn<Glib::ustring> shaderName;
		Gtk::TreeModelColumn< Glib::RefPtr<Gdk::Pixbuf> > icon;
	};
	
private:
	ShaderTreeColumns _shaderTreeColumns;
	
	// Tree view and selection object
	Gtk::TreeView* _treeView;
	Glib::RefPtr<Gtk::TreeSelection> _selection;
	
	// GL preview widget
	gtkutil::GLWidget* _glWidget;
	
	// The client of this class.
	Client* _client;
	
	// TRUE, if the first light layers are to be rendered instead of the editorimages
	bool _isLightTexture;

	struct InfoStoreColumns : 
		public Gtk::TreeModel::ColumnRecord
	{
		InfoStoreColumns() { add(attribute); add(value); }

		Gtk::TreeModelColumn<Glib::ustring> attribute;
		Gtk::TreeModelColumn<Glib::ustring> value;
	};

	InfoStoreColumns _infoStoreColumns;

	// List store for info table
	Glib::RefPtr<Gtk::ListStore> _infoStore;
	
public:
	// This is where the prefixes are stored (needed to filter the possible shaders)
	typedef std::vector<std::string> PrefixList;
	PrefixList _prefixes;

	/** Constructor.
	 * 
	 * @client: The client class that gets notified on selection change
	 * @prefixes: A comma-separated list of shader prefixes.
	 * @isLightTexture: set this to TRUE to render the light textures instead of the editor images
	 */
	ShaderSelector(Client* client, const std::string& prefixes, bool isLightTexture = false);
	
	/** Return the shader selected by the user, or an empty string if there
	 * was no selection.
	 */
	std::string getSelection();
	
	/** Set the given shader name as the current selection, highlighting it
	 * in the tree view.
	 * 
	 * @param selection
	 * The fullname of the shader to select, or the empty string if there 
	 * should be no selection.
	 */
	void setSelection(const std::string& selection);
	
	// Get the selected Material
	MaterialPtr getSelectedShader();
	
	/** greebo: Static info display function (can be used by other UI classes as well
	 * 			to allow code reuse).
	 */
	static void displayShaderInfo(const MaterialPtr& shader, 
								  const Glib::RefPtr<Gtk::ListStore>& listStore,
								  int attrCol = 0,   // attribute column number 
								  int valueCol = 1); // value column number
	
	/** greebo: Populates the given listStore with the light shader information.
	 */
	static void displayLightShaderInfo(const MaterialPtr& shader, 
									   const Glib::RefPtr<Gtk::ListStore>& listStore,
									   int attrCol = 0,   // attribute column number 
									   int valueCol = 1); // value column number
	
private:

	// Create GUI elements
	Gtk::Widget& createTreeView();
	Gtk::Widget& createPreview();
	
	// Update the info in the table (passes the call to the client class)
	void updateInfoTable();
	
	// gtkmm callbacks
	bool _onExpose(GdkEventExpose*);
	void _onSelChange();
};

} // namespace ui

#endif /*SHADERSELECTOR_H_*/
