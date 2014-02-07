#pragma once

#include <wx/dataview.h>
#include <vector>
#include <memory>
#include <boost/noncopyable.hpp>

namespace wxutil
{

/**
 * Implements a wxwidgets DataViewModel, similar to wxDataViewTreeStore
 * but with more versatility of the stored columns.
 */
class TreeModel :
	public wxDataViewModel
{
public:
	/**
	 * Represents a column in the wxutil::TreeModel
	 * Use the Type string to instantiate a Column and arrange
	 * multiple columns into a ColumnRecord structure.
	 */
	class Column
	{
	public:
		enum Type
		{
			String = 0,
			Integer,
			Double,
			Bool,
			Icon,
			NumTypes
		};

		Type type;
		std::string name;

	private:
		// column index in the treemodel - this member is assigned
		// by the TreeModel itself on attachment
		int _col;

	public:
		Column(Type type_, const std::string& name_ = "") :
			type(type_),
			name(name_),
			_col(-1)
		{}

		// Returns the index of this column
		int getColumnIndex() const
		{
			if (_col == -1) throw std::runtime_error("Cannot query column index of unattached column.");

			return _col;
		}

		// Only for internal use by the TreeModel - didn't want to use a friend declaration
		void _setColumnIndex(int index)
		{
			_col = index;
		}

		// Returns the wxWidgets type string of this column
		wxString getWxType() const
		{
			static std::vector<wxString> types(NumTypes);

			if (types.empty())
			{
				types[String] = "string";
				types[Integer] = "long";
				types[Double] = "double";
				types[Bool] = "bool";
				types[Icon] = "icon";
			}

			return types[type];
		}
	};

	/**
	 * Use this record to declare the column order of the TreeModel.
	 * Subclasses should call Add() for each of their Column members.
	 */
	class ColumnRecord
	{
	public:
		// A list of column references, these point to members of subclasses
		typedef std::vector<Column> List;

	private:
		List _columns;

	protected:
		// Only to be instantiated by subclasses
		ColumnRecord() {}

	public:
		Column& Add(Column::Type type)
		{
			_columns.push_back(Column(type));
			return _columns.back();
		}

		List::iterator begin()
		{ 
			return _columns.begin();
		}

		List::const_iterator begin() const
		{ 
			return _columns.begin();
		}

		List::iterator end()
		{ 
			return _columns.end();
		}

		List::const_iterator end() const
		{ 
			return _columns.end();
		}

		List::size_type size() const
		{ 
			return _columns.size();
		}

		const List::value_type& operator[](std::size_t index) const
		{
			return _columns[index];
		}

		List::value_type& operator[](std::size_t index)
		{
			return _columns[index];
		}
	};

	// An assignment helper, for use in TreeModel::Row
	class ItemValueProxy :
		public boost::noncopyable
	{
	private:
		wxDataViewItem _item;
		const Column& _column;
		wxDataViewModel& _model;

	public:
		ItemValueProxy(const wxDataViewItem& item, const Column& column, wxDataViewModel& model) :
			_item(item),
			_column(column),
			_model(model)
		{}

		// get/set operators
		ItemValueProxy& operator=(const wxVariant& data)
		{
			_model.SetValue(data, _item, _column.getColumnIndex());
			return *this;
		}

		operator wxVariant() const
		{
			wxVariant variant;

			_model.GetValue(variant, _item, _column.getColumnIndex());

			return variant;
		}
	};

	/**
	 * A convenience representation of a single wxDataViewItem,
	 * allowing to set column values using the operator[].
	 */
	class Row
	{
	private:
		wxDataViewItem _item;
		wxDataViewModel& _model;

	public:
		Row(const wxDataViewItem& item, wxDataViewModel& model) :
			 _item(item),
			 _model(model)
		{}

		const wxDataViewItem& getItem() const
		{
			return _item;
		}

		const ItemValueProxy operator[](const Column& column) const
		{
			return ItemValueProxy(_item, column, _model);
		}

		ItemValueProxy operator[](const Column& column)
		{
			return ItemValueProxy(_item, column, _model);
		}
	};

private:
	struct Node;
	typedef std::shared_ptr<Node> NodePtr;

private:
	ColumnRecord _columns;

	NodePtr _rootNode;

	int _sortColumn;
public:
	TreeModel(const ColumnRecord& columns);

	virtual ~TreeModel();

	virtual Row AddItem(wxDataViewItem& parent);

	// Base class implementation / overrides

	virtual bool HasDefaultCompare() const;
	virtual unsigned int GetColumnCount() const;

    // return type as reported by wxVariant
    virtual wxString GetColumnType(unsigned int col) const;

    // get value into a wxVariant
    virtual void GetValue( wxVariant &variant,
                           const wxDataViewItem &item, unsigned int col ) const;
	virtual bool SetValue(const wxVariant &variant,
                          const wxDataViewItem &item,
                          unsigned int col);
	virtual wxDataViewItem GetParent( const wxDataViewItem &item ) const;
    virtual bool IsContainer(const wxDataViewItem& item) const;

	virtual unsigned int GetChildren(const wxDataViewItem& item, wxDataViewItemArray& children) const;
	virtual wxDataViewItem GetRoot();

	virtual int Compare(const wxDataViewItem& item1, const wxDataViewItem& item2, unsigned int column, bool ascending) const;
};


} // namespace

#include <string>
#include <gtkmm/treesortable.h>
#include <gtkmm/treemodel.h>

namespace Gtk { class TreeView; }

namespace gtkutil
{

/**
 * Utility class for operation on Gtk::TreeModels.
 */
class TreeModel
{
public:
	/**
	 * greebo: Utility callback for use in Gtk::TreeView::set_search_equal_func, which
	 * enables some sort of "full string" search in treeviews.
	 *
	 * The equalFuncStringContains returns "match" as soon as the given key occurs
	 * somewhere in the string in question, not only at the beginning (GTK default).
	 *
	 * Prerequisites: The search column must contain a string.
	 */
	static bool equalFuncStringContains(const Glib::RefPtr<Gtk::TreeModel>& model,
										int column,
										const Glib::ustring& key,
										const Gtk::TreeModel::iterator& iter);

	class SelectionFinder
	{
	private:
		// String containing the name to highlight
		std::string _selection;

		// An integer to search for (alternative to the string above)
		int _needle;

		// The found iterator
		Gtk::TreeModel::iterator _foundIter;

		// The column index to be searched
		int _column;

		// TRUE, if this should search for an integer instead of a string
		bool _searchForInt;

	public:

		// Constructor to search for strings
		SelectionFinder(const std::string& selection, int column);

		// Constructor to search for integers
		SelectionFinder(int needle, int column);

		/** greebo: Get the GtkTreeIter corresponding to the found path.
		 * 			The returnvalue can be invalid if the internal found path is NULL.
		 */
		const Gtk::TreeModel::iterator getIter() const;

		// Callback for gtkmm
		bool forEach(const Gtk::TreeModel::iterator& iter);
	};

	/**
	 * greebo: Tries to lookup the given string in the given column of the given view.
	 * Returns TRUE if the lookup and the selection was successful, FALSE otherwise.
	 */
	static bool findAndSelectString(Gtk::TreeView* view,
									const std::string& needle,
									int column);

	/**
	 * greebo: Tries to lookup the given string in the given column of the given view.
	 * Returns TRUE if the lookup and the selection was successful, FALSE otherwise.
	 */
	static bool findAndSelectInteger(Gtk::TreeView* view, int needle, int column);

	/**
	 * greebo: Tries to lookup the given string in the given column of the given view.
	 * Returns TRUE if the lookup and the selection was successful, FALSE otherwise.
	 */
	static bool findAndSelectString(Gtk::TreeView* view,
									const std::string& needle,
									const Gtk::TreeModelColumn<Glib::ustring>& column);

	/**
	 * greebo: Tries to lookup the given string in the given column of the given view.
	 * Returns TRUE if the lookup and the selection was successful, FALSE otherwise.
	 */
	static bool findAndSelectString(Gtk::TreeView* view,
									const std::string& needle,
									const Gtk::TreeModelColumn<std::string>& column);

	/**
	 * greebo: Tries to lookup the given string in the given column of the given view.
	 * Returns TRUE if the lookup and the selection was successful, FALSE otherwise.
	 */
	static bool findAndSelectInteger(Gtk::TreeView* view, int needle,
									 const Gtk::TreeModelColumn<int>& column);

	/**
	 * greebo: Takes care that the given tree model is sorted such that
	 * folders are listed before "regular" items.
	 *
	 * @model: The tree model to sort, must implement GtkTreeSortable.
	 * @nameCol: the column number containing the name
	 * @isFolderColumn: the column number containing a boolean flag: "is folder"
	 */
	static void applyFoldersFirstSortFunc(const Glib::RefPtr<Gtk::TreeSortable>& model,
										  const Gtk::TreeModelColumn<Glib::ustring>& nameColumn,
										  const Gtk::TreeModelColumn<bool>& isFolderColumn);

	static void applyFoldersFirstSortFunc(const Glib::RefPtr<Gtk::TreeSortable>& model,
										  const Gtk::TreeModelColumn<std::string>& nameColumn,
										  const Gtk::TreeModelColumn<bool>& isFolderColumn);

private:
	static int sortFuncFoldersFirstStd(const Gtk::TreeModel::iterator& a,
									const Gtk::TreeModel::iterator& b,
									const Gtk::TreeModelColumn<std::string>& nameColumn, // columns are bound
									const Gtk::TreeModelColumn<bool>& isFolderColumn);	 // by applyFoldersFirstSortFunc

	static int sortFuncFoldersFirst(const Gtk::TreeModel::iterator& a,
									const Gtk::TreeModel::iterator& b,
									const Gtk::TreeModelColumn<Glib::ustring>& nameColumn, // columns are bound
									const Gtk::TreeModelColumn<bool>& isFolderColumn);	 // by applyFoldersFirstSortFunc
};

} // namespace gtkutil
