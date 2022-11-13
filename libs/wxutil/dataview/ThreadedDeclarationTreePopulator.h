#pragma once

#include "idecltypes.h"
#include "ifavourites.h"

#include "../Bitmap.h"
#include "../Icon.h"
#include "DeclarationTreeView.h"
#include "ThreadedResourceTreePopulator.h"
#include "TreeViewItemStyle.h"
#include "VFSTreePopulator.h"
#include "os/path.h"

namespace wxutil
{

/**
 * Shared ThreadedResourceTreePopulator implementation specialising on populating
 * trees of IDeclaration elements.
 */
class ThreadedDeclarationTreePopulator :
    public ThreadedResourceTreePopulator
{
private:
    static constexpr const char* const DEFAULT_DECL_ICON = "decl.png";
    static constexpr const char* const DEFAULT_FOLDER_ICON = "folder16.png";

    decl::Type _type;
    const DeclarationTreeView::Columns& _columns;

    std::set<std::string> _favourites;

    wxutil::Icon _folderIcon;
    wxutil::Icon _declIcon;

public:
    ThreadedDeclarationTreePopulator(decl::Type type, const DeclarationTreeView::Columns& columns) :
        ThreadedDeclarationTreePopulator(type, columns, DEFAULT_DECL_ICON, DEFAULT_FOLDER_ICON)
    {}

    ThreadedDeclarationTreePopulator(decl::Type type, const DeclarationTreeView::Columns& columns, 
        const std::string& declIcon) :
        ThreadedDeclarationTreePopulator(type, columns, declIcon, DEFAULT_FOLDER_ICON)
    {}

    ThreadedDeclarationTreePopulator(decl::Type type, const DeclarationTreeView::Columns& columns, 
        const std::string& declIcon, const std::string& folderIcon) :
        ThreadedResourceTreePopulator(columns),
        _type(type),
        _columns(columns),
        _declIcon(GetLocalBitmap(declIcon)),
        _folderIcon(GetLocalBitmap(folderIcon))
    {
        // Assemble the set of favourites for the given declaration type
        _favourites = GlobalFavouritesManager().getFavourites(decl::getTypeName(type));
    }

    ~ThreadedDeclarationTreePopulator() override
    {
        EnsureStopped();
    }

    // Default implementation creates a plain tree using the mod name as first path element
    // Subclasses should override the default implementation (without calling the base) if not suitable
    void PopulateModel(const TreeModel::Ptr& model) override
    {
        VFSTreePopulator populator(model);

        GlobalDeclarationManager().foreachDeclaration(_type, [&](const decl::IDeclaration::Ptr& decl)
        {
            ThrowIfCancellationRequested();

            // Some names contain backslashes, sort them in the tree by replacing the backslashes
            auto nameForwardSlashes = os::standardPath(decl->getDeclName());

            auto fullPath = decl->getModName() + "/" + nameForwardSlashes;

            // Sort the shader into the tree and set the values
            populator.addPath(fullPath, [&](TreeModel::Row& row,
                const std::string& path, const std::string& leafName, bool isFolder)
            {
                AssignValuesToRow(row, path, isFolder ? path : decl->getDeclName(), leafName, isFolder);
            });
        });
    }

protected:
    // Default sorting behaviour is to sort the tree alphabetically with folders on top
    void SortModel(const TreeModel::Ptr& model) override
    {
        model->SortModelFoldersFirst(_columns.leafName, _columns.isFolder);
    }

    /**
     * Populates the given row with values matching for a certain declaration or folder
     *
     * @fullPath: The path to the row, mainly used for internal storage.
     * @declName: The name of the declaration (including any folders and slashes)
     * @leafName: The name part after the rightmost slash
     * @isFolder: Whether this row is a folder (the folder icon will be assigned)
     */
    void AssignValuesToRow(TreeModel::Row& row, const std::string& fullPath, 
        const std::string& declName, const std::string& leafName, bool isFolder)
    {
        ThrowIfCancellationRequested();

        auto isFavourite = IsFavourite(declName);

        row[_columns.iconAndName] = wxVariant(wxDataViewIconText(leafName, !isFolder ? _declIcon : _folderIcon));
        row[_columns.iconAndName].setAttr(TreeViewItemStyle::Declaration(isFavourite));
        row[_columns.fullName] = fullPath;
        row[_columns.leafName] = leafName;
        row[_columns.declName] = declName;
        row[_columns.isFolder] = isFolder;
        row[_columns.isFavourite] = isFavourite;

        row.SendItemAdded();
    }

    const std::set<std::string>& GetFavourites() const
    {
        return _favourites;
    }

    bool IsFavourite(const std::string& declName)
    {
        return _favourites.count(declName) > 0;
    }
};

}
