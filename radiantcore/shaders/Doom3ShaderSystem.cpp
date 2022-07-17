#include "Doom3ShaderSystem.h"
#include "MaterialSourceGenerator.h"

#include "i18n.h"
#include "ideclmanager.h"

#include "iregistry.h"
#include "ifilesystem.h"
#include "ifiletypes.h"
#include "igame.h"

#include "ShaderExpression.h"

#include "debugging/ScopedDebugTimer.h"
#include "module/StaticModule.h"

#include "decl/DeclarationCreator.h"
#include "stream/TemporaryOutputStream.h"
#include "materials/ParseLib.h"
#include <functional>

namespace
{
    const char* TEXTURE_PREFIX = "textures/";

    // Default image maps for optional material stages
    const std::string IMAGE_FLAT = "_flat.bmp";
    const std::string IMAGE_BLACK = "_black.bmp";

    inline std::string getBitmapsPath()
    {
        return module::GlobalModuleRegistry().getApplicationContext().getBitmapsPath();
    }

}

namespace shaders
{

Doom3ShaderSystem::Doom3ShaderSystem() :
    _enableActiveUpdates(true)
{}

void Doom3ShaderSystem::construct()
{
    _library = std::make_shared<ShaderLibrary>();
    _textureManager = std::make_shared<GLTextureManager>();
}

void Doom3ShaderSystem::destroy()
{
    // Don't destroy the GLTextureManager, it's called from
    // the CShader destructors.
}

void Doom3ShaderSystem::freeShaders() {
    _library->clear();
    _textureManager->checkBindings();
    activeShadersChangedNotify();
}

void Doom3ShaderSystem::refresh() {
}

MaterialPtr Doom3ShaderSystem::getMaterial(const std::string& name)
{
    return _library->findShader(name);
}

bool Doom3ShaderSystem::materialExists(const std::string& name)
{
    return _library->definitionExists(name);
}

bool Doom3ShaderSystem::materialCanBeModified(const std::string& name)
{
    if (!_library->definitionExists(name))
    {
        return false;
    }

    auto decl = _library->getTemplate(name);
    const auto& fileInfo = decl->getBlockSyntax().fileInfo;
    return fileInfo.name.empty() || fileInfo.getIsPhysicalFile();
}

void Doom3ShaderSystem::foreachShaderName(const ShaderNameCallback& callback)
{
    // Pass the call to the Library
    _library->foreachShaderName(callback);
}

void Doom3ShaderSystem::setLightingEnabled(bool enabled)
{
    if (CShader::m_lightingEnabled != enabled)
    {
        // First unrealise the lighting of all shaders
        _library->foreachShader([](const CShaderPtr& shader)
        {
            shader->unrealiseLighting();
        });

        // Set the global (greebo: Does this really need to be done this way?)
        CShader::m_lightingEnabled = enabled;

        // Now realise the lighting of all shaders
        _library->foreachShader([](const CShaderPtr& shader)
        {
            shader->realiseLighting();
        });
    }
}

const char* Doom3ShaderSystem::getTexturePrefix() const
{
    return TEXTURE_PREFIX;
}

GLTextureManager& Doom3ShaderSystem::getTextureManager()
{
    return *_textureManager;
}

// Get default textures
TexturePtr Doom3ShaderSystem::getDefaultInteractionTexture(IShaderLayer::Type type)
{
    TexturePtr defaultTex;

    // Look up based on layer type
    switch (type)
    {
    case IShaderLayer::DIFFUSE:
    case IShaderLayer::SPECULAR:
        defaultTex = _textureManager->getBinding(getBitmapsPath() + IMAGE_BLACK);
        break;

    case IShaderLayer::BUMP:
        defaultTex = _textureManager->getBinding(getBitmapsPath() + IMAGE_FLAT);
        break;
    default:
        break;
    }

    return defaultTex;
}

sigc::signal<void> Doom3ShaderSystem::signal_activeShadersChanged() const
{
    return _signalActiveShadersChanged;
}

void Doom3ShaderSystem::activeShadersChangedNotify()
{
    if (_enableActiveUpdates)
    {
        _signalActiveShadersChanged.emit();
    }
}

void Doom3ShaderSystem::foreachMaterial(const std::function<void(const MaterialPtr&)>& func)
{
    _library->foreachShader(func);
}

TexturePtr Doom3ShaderSystem::loadTextureFromFile(const std::string& filename)
{
    // Remove any unused Textures before allocating new ones.
    _textureManager->checkBindings();

    // Get the binding (i.e. load the texture)
    return _textureManager->getBinding(filename);
}

sigc::signal<void, const std::string&>& Doom3ShaderSystem::signal_materialCreated()
{
    return _sigMaterialCreated;
}

sigc::signal<void, const std::string&, const std::string&>& Doom3ShaderSystem::signal_materialRenamed()
{
    return _sigMaterialRenamed;
}

sigc::signal<void, const std::string&>& Doom3ShaderSystem::signal_materialRemoved()
{
    return _sigMaterialRemoved;
}

IShaderExpression::Ptr Doom3ShaderSystem::createShaderExpressionFromString(const std::string& exprStr)
{
    return ShaderExpression::createFromString(exprStr);
}

std::string Doom3ShaderSystem::ensureNonConflictingName(const std::string& name)
{
    auto candidate = name;
    auto i = 0;

    while (_library->definitionExists(candidate))
    {
        candidate += fmt::format("{0:02d}", ++i);
    }

    return candidate;
}

MaterialPtr Doom3ShaderSystem::createEmptyMaterial(const std::string& name)
{
    // Find a non-conflicting name and create an empty declaration
    auto candidate = ensureNonConflictingName(name);
    auto decl = GlobalDeclarationManager().findOrCreateDeclaration(decl::Type::Material, name);

    auto material = _library->findShader(candidate);
    material->setIsModified();

    _sigMaterialCreated.emit(candidate);

    return material;
}

bool Doom3ShaderSystem::renameMaterial(const std::string& oldName, const std::string& newName)
{
    auto result = _library->renameDefinition(oldName, newName);

    if (result)
    {
        _sigMaterialRenamed(oldName, newName);
    }

    return result;
}

void Doom3ShaderSystem::removeMaterial(const std::string& name)
{
    if (!_library->definitionExists(name))
    {
        rWarning() << "Cannot remove non-existent material " << name << std::endl;
        return;
    }

    _library->removeDefinition(name);

    _sigMaterialRemoved.emit(name);
}

MaterialPtr Doom3ShaderSystem::copyMaterial(const std::string& nameOfOriginal, const std::string& nameOfCopy)
{
    if (nameOfCopy.empty())
    {
        rWarning() << "Cannot copy, the new name must not be empty" << std::endl;
        return MaterialPtr();
    }

    auto candidate = ensureNonConflictingName(nameOfCopy);

    if (!_library->definitionExists(nameOfOriginal))
    {
        rWarning() << "Cannot copy non-existent material " << nameOfOriginal << std::endl;
        return MaterialPtr();
    }

    _library->copyDefinition(nameOfOriginal, candidate);

    _sigMaterialCreated.emit(candidate);

    auto material = _library->findShader(candidate);
    material->setIsModified();

    return material;
}

void Doom3ShaderSystem::saveMaterial(const std::string& name)
{
    auto material = _library->findShader(name);

    if (!material->isModified())
    {
        rMessage() << "Material is not modified, nothing to save." << std::endl;
        return;
    }

    if (!materialCanBeModified(material->getName()))
    {
        throw std::runtime_error("Cannot save this material, it's read-only.");
    }

    // Store the modifications in our actual template and un-mark the file
    material->commitModifications();

    // Write the declaration to disk
    GlobalDeclarationManager().saveDeclaration(material->getTemplate());
}

ITableDefinition::Ptr Doom3ShaderSystem::getTable(const std::string& name)
{
    return std::static_pointer_cast<ITableDefinition>(
        GlobalDeclarationManager().findDeclaration(decl::Type::Table, name)
    );
}

const std::string& Doom3ShaderSystem::getName() const
{
    static std::string _name(MODULE_SHADERSYSTEM);
    return _name;
}

const StringSet& Doom3ShaderSystem::getDependencies() const
{
    static StringSet _dependencies
    {
        MODULE_DECLMANAGER,
        MODULE_VIRTUALFILESYSTEM,
        MODULE_XMLREGISTRY,
        MODULE_GAMEMANAGER,
        MODULE_FILETYPES,
    };

    return _dependencies;
}

void Doom3ShaderSystem::initialiseModule(const IApplicationContext& ctx)
{
    rMessage() << getName() << "::initialiseModule called" << std::endl;

    GlobalDeclarationManager().registerDeclType("table", std::make_shared<decl::DeclarationCreator<TableDefinition>>(decl::Type::Table));
    GlobalDeclarationManager().registerDeclType("material", std::make_shared<decl::DeclarationCreator<ShaderTemplate>>(decl::Type::Material));
    GlobalDeclarationManager().registerDeclFolder(decl::Type::Material, "materials/", ".mtr");

    construct();

#if 0
    testShaderExpressionParsing();
#endif

    // Register the mtr file extension
    GlobalFiletypes().registerPattern("material", FileTypePattern(_("Material File"), "mtr", "*.mtr"));
}

// Horrible evil macro to avoid assertion failures if expr is NULL
#define GET_EXPR_OR_RETURN expr = createShaderExpressionFromString(exprStr);\
                                  if (!expr) return;

void Doom3ShaderSystem::testShaderExpressionParsing()
{
    // Test a few things
    std::string exprStr = "3";
    IShaderExpression::Ptr expr;
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "3+4";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "(3+4)";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "(4.2)";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "3+5+6";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "3+(5+6)";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "3 * 3+5";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "3+3*5";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "(3+3)*5";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "(3+3*7)-5";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "3-3*5";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "blinktable[0]";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "blinktable[1]";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "blinktable[0.3]";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "blinksnaptable[0.3]";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "xianjittertable[0]";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "xianjittertable[time]";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "3-3*xianjittertable[2]";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "3+xianjittertable[3]*7";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "(3+xianjittertable[3])*7";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "2.3 % 2";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "2.0 % 0.5";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "2 == 2";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "1 == 2";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "1 != 2";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "1.2 != 1.2";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "1.2 == 1.2*3";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "1.2*3 == 1.2*3";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "3 == 3 && 1 != 0";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "1 != 1 || 3 == 3";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "4 == 3 || 1 != 0";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "time";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(2) << std::endl;

    exprStr = "-3 + 5";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "3 * -5";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "3 * -5 + 4";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "3 + -5 * 4";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "3 * 5 * -6";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;

    exprStr = "decalFade[(time - Parm3)/(parm4 - parm3)]";
    GET_EXPR_OR_RETURN;
    rMessage() << "Expression " << exprStr << ": " << expr->getValue(0) << std::endl;
}

void Doom3ShaderSystem::shutdownModule()
{
    rMessage() << "Doom3ShaderSystem::shutdownModule called" << std::endl;

    destroy();
    _library->clear();
    _library.reset();
}

// Accessor function encapsulating the static shadersystem instance
Doom3ShaderSystemPtr GetShaderSystem()
{
    // Acquire the moduleptr from the module registry
    RegisterableModulePtr modulePtr(module::GlobalModuleRegistry().getModule(MODULE_SHADERSYSTEM));

    // static_cast it onto our shadersystem type
    return std::static_pointer_cast<Doom3ShaderSystem>(modulePtr);
}

GLTextureManager& GetTextureManager()
{
    return GetShaderSystem()->getTextureManager();
}

// Static module instance
module::StaticModuleRegistration<Doom3ShaderSystem> d3ShaderSystemModule;

} // namespace shaders
