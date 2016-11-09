#include "ParticlesManager.h"

#include "ParticleDef.h"
#include "StageDef.h"
#include "ParticleNode.h"
#include "RenderableParticle.h"

#include "icommandsystem.h"
#include "ieventmanager.h"
#include "itextstream.h"
#include "ifilesystem.h"
#include "iarchive.h"
#include "igame.h"
#include "i18n.h"

#include "parser/DefTokeniser.h"
#include "math/Vector4.h"
#include "os/fs.h"

#include "debugging/ScopedDebugTimer.h"

#include <fstream>
#include <iostream>
#include <functional>
#include <boost/regex.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace fs = boost::filesystem;

namespace particles
{

namespace
{
    inline void writeParticleCommentHeader(std::ostream& str)
    {
        // The comment at the head of the decl
        str << "/*" << std::endl
                    << "\tGenerated by DarkRadiant's Particle Editor."
                    << std::endl <<	"*/" << std::endl;
    }
}

ParticlesManager::ParticlesManager() :
    _defLoader(std::bind(&ParticlesManager::reloadParticleDefs, this))
{}

sigc::signal<void> ParticlesManager::signal_particlesReloaded() const
{
    return _particlesReloadedSignal;
}

// Visit all of the particle defs
void ParticlesManager::forEachParticleDef(const ParticleDefVisitor& v)
{
    ensureDefsLoaded();

	// Invoke the visitor for each ParticleDef object
	for (const ParticleDefMap::value_type& pair : _particleDefs)
	{
		v(*pair.second);
	}
}

IParticleDefPtr ParticlesManager::getDefByName(const std::string& name)
{
    ensureDefsLoaded();

	ParticleDefMap::const_iterator found = _particleDefs.find(name);

	return found != _particleDefs.end() ? found->second : IParticleDefPtr();
}

IParticleNodePtr ParticlesManager::createParticleNode(const std::string& name)
{
	std::string nameCleaned = name;

	// Cut off the ".prt" from the end of the particle name
	if (boost::algorithm::ends_with(nameCleaned, ".prt"))
	{
		nameCleaned = nameCleaned.substr(0, nameCleaned.length() - 4);
	}

    ensureDefsLoaded();

	ParticleDefMap::const_iterator found = _particleDefs.find(nameCleaned);

	if (found == _particleDefs.end())
	{
		return IParticleNodePtr();
	}

	RenderableParticlePtr renderable(new RenderableParticle(found->second));
	return ParticleNodePtr(new ParticleNode(renderable));
}

IRenderableParticlePtr ParticlesManager::getRenderableParticle(const std::string& name)
{
    ensureDefsLoaded();

	ParticleDefMap::const_iterator found = _particleDefs.find(name);

	if (found != _particleDefs.end())
	{
		return RenderableParticlePtr(new RenderableParticle(found->second));
	}
	else
	{
		return IRenderableParticlePtr();
	}
}

ParticleDefPtr ParticlesManager::findOrInsertParticleDef(const std::string& name)
{
    ensureDefsLoaded();

    return findOrInsertParticleDefInternal(name);
}

ParticleDefPtr ParticlesManager::findOrInsertParticleDefInternal(const std::string& name)
{
    ParticleDefMap::iterator i = _particleDefs.find(name);

    if (i != _particleDefs.end())
    {
        // Particle def is already existing in the map
        return i->second;
    }

    // Not existing, add a new ParticleDef to the map
    std::pair<ParticleDefMap::iterator, bool> result = _particleDefs.insert(
        ParticleDefMap::value_type(name, ParticleDefPtr(new ParticleDef(name))));

    // Return the iterator from the insertion result
    return result.first->second;
}

void ParticlesManager::removeParticleDef(const std::string& name)
{
    ensureDefsLoaded();

	ParticleDefMap::iterator i = _particleDefs.find(name);

	if (i != _particleDefs.end())
	{
		_particleDefs.erase(i);
	}
}

void ParticlesManager::ensureDefsLoaded()
{
    _defLoader.ensureFinished();
}

// Parse particle defs from string
void ParticlesManager::parseStream(std::istream& contents, const std::string& filename)
{
	// Usual ritual, get a parser::DefTokeniser and start tokenising the DEFs
	parser::BasicDefTokeniser<std::istream> tok(contents);

	while (tok.hasMoreTokens())
	{
		parseParticleDef(tok, filename);
	}
}

// Parse a single particle def
void ParticlesManager::parseParticleDef(parser::DefTokeniser& tok, const std::string& filename)
{
	// Standard DEF, starts with "particle <name> {"
	std::string declName = tok.nextToken();

	// Check for a valid particle declaration, some .prt files contain materials
	if (declName != "particle")
	{
		// No particle, skip name plus whole block
		tok.skipTokens(1);
		tok.assertNextToken("{");

		for (std::size_t level = 1; level > 0;)
		{
			std::string token = tok.nextToken();

			if (token == "}")
			{
				level--;
			}
			else if (token == "{")
			{
				level++;
			}
		}

		return;
	}

	// Valid particle declaration, go ahead parsing the name
	std::string name = tok.nextToken();
	tok.assertNextToken("{");

    // Find the particle def (use the non-blocking, internal lookup)
	ParticleDefPtr pdef = findOrInsertParticleDefInternal(name);

	pdef->setFilename(filename);

	// Let the particle construct itself from the token stream
	pdef->parseFromTokens(tok);
}

const std::string& ParticlesManager::getName() const
{
	static std::string _name(MODULE_PARTICLESMANAGER);
	return _name;
}

const StringSet& ParticlesManager::getDependencies() const
{
	static StringSet _dependencies;

	if (_dependencies.empty())
	{
		_dependencies.insert(MODULE_VIRTUALFILESYSTEM);
		_dependencies.insert(MODULE_COMMANDSYSTEM);
		_dependencies.insert(MODULE_EVENTMANAGER);
	}

	return _dependencies;
}

void ParticlesManager::initialiseModule(const ApplicationContext& ctx)
{
	rMessage() << "ParticlesManager::initialiseModule called" << std::endl;

	// Load the .prt files in a new thread, public methods will block until
    // this has been completed
    _defLoader.start();

	// Register the "ReloadParticles" commands
	GlobalCommandSystem().addCommand("ReloadParticles", std::bind(&ParticlesManager::reloadParticleDefs, this));
	GlobalEventManager().addCommand("ReloadParticles", "ReloadParticles");
}

void ParticlesManager::reloadParticleDefs()
{
	ScopedDebugTimer timer("Particle definitions parsed: ");

    GlobalFileSystem().forEachFile(PARTICLES_DIR, PARTICLES_EXT, [&](const std::string& filename)
    {
        // Attempt to open the file in text mode
        ArchiveTextFilePtr file = GlobalFileSystem().openTextFile(PARTICLES_DIR + filename);

        if (file != NULL)
        {
            // File is open, so parse the tokens
            try 
            {
                std::istream is(&(file->getInputStream()));
                parseStream(is, filename);
            }
            catch (parser::ParseException& e)
            {
                rError() << "[particles] Failed to parse " << filename
                    << ": " << e.what() << std::endl;
            }
        }
        else
        {
            rError() << "[particles] Unable to open " << filename << std::endl;
        }
    }, 1); // depth == 1: don't search subdirectories

    rMessage() << "Found " << _particleDefs.size() << " particle definitions." << std::endl;

	// Notify observers about this event
    _particlesReloadedSignal.emit();
}

void ParticlesManager::saveParticleDef(const std::string& particleName)
{
    ensureDefsLoaded();

	ParticleDefMap::const_iterator found = _particleDefs.find(particleName);

	if (found == _particleDefs.end())
	{
		throw std::runtime_error(_("Cannot save particle, it has not been registered yet."));
	}

	ParticleDefPtr particle = found->second;

	std::string relativePath = PARTICLES_DIR + particle->getFilename();

	fs::path targetPath = GlobalGameManager().getModPath();

	if (targetPath.empty())
	{
		targetPath = GlobalGameManager().getUserEnginePath();

		rMessage() << "No mod base path found, falling back to user engine path to save particle file: " << 
			targetPath.string() << std::endl;
	}

	targetPath /= PARTICLES_DIR;

	// Ensure the particles folder exists
	fs::create_directories(targetPath);

	fs::path targetFile = targetPath / particle->getFilename();

	// If the file doesn't exist yet, let's check if we need to inherit stuff first from the VFS
	if (!fs::exists(targetFile))
	{
		ArchiveTextFilePtr inheritFile = GlobalFileSystem().openTextFile(relativePath);

		if (inheritFile)
		{
			// There is a file with that name already in the VFS, copy it to the target file
			TextInputStream& inheritStream = inheritFile->getInputStream();

			std::ofstream outFile(targetFile.string().c_str());

			if (!outFile.is_open())
			{
				throw std::runtime_error(
					(boost::format(_("Cannot open file for writing: %s")) % targetFile.string()).str());
			}

			char buf[16384];
			std::size_t bytesRead = inheritStream.read(buf, sizeof(buf));

			while (bytesRead > 0)
			{
				outFile.write(buf, bytesRead);

				bytesRead = inheritStream.read(buf, sizeof(buf));
			}

			outFile.close();
		}
	}

	// Open a temporary file
	fs::path tempFile = targetFile;

	tempFile.remove_filename();
	tempFile /= "_" + os::filename_from_path(targetFile);

	std::ofstream tempStream(tempFile.string().c_str());

	if (!tempStream.is_open())
	{
		throw std::runtime_error(
				(boost::format(_("Cannot open file for writing: %s")) % tempFile.string()).str());
	}

	std::string tempString;

	// If a previous file exists, open it for reading and filter out the particle def we'll be writing
	if (fs::exists(targetFile))
	{
		std::ifstream inheritStream(targetFile.string().c_str());

		if (!inheritStream.is_open())
		{
			throw std::runtime_error(
					(boost::format(_("Cannot open file for reading: %s")) % targetFile.string()).str());
		}

		// Write the file to the output stream, up to the point the particle def should be written to
		stripParticleDefFromStream(inheritStream, tempStream, particleName);

		if (inheritStream.eof())
		{
			// Particle def was not found in the inherited stream, write our comment
			tempStream << std::endl << std::endl;

			writeParticleCommentHeader(tempStream);
		}

		// We're at the insertion point (which might as well be EOF of the inheritStream)

		// Write the particle declaration
		tempStream << *particle << std::endl;

		tempStream << inheritStream.rdbuf();

		inheritStream.close();
	}
	else
	{
		// Just put the particle def into the file and that's it, leave a comment at the head of the decl
		writeParticleCommentHeader(tempStream);

		// Write the particle declaration
		tempStream << *particle << std::endl;
	}

	tempStream.close();

	// Move the temporary stream over the actual file, removing the target first
	if (fs::exists(targetFile))
	{
		try
		{
			fs::remove(targetFile);
		}
		catch (fs::filesystem_error& e)
		{
			rError() << "Could not remove the file " << targetFile.string() << std::endl
				<< e.what() << std::endl;

			throw std::runtime_error(
				(boost::format(_("Could not remove the file %s")) % targetFile.string()).str());
		}
	}

	try
	{
		fs::rename(tempFile, targetFile);
	}
	catch (fs::filesystem_error& e)
	{
		rError() << "Could not rename the temporary file " << tempFile.string() << std::endl
			<< e.what() << std::endl;

		throw std::runtime_error(
			(boost::format(_("Could not rename the temporary file %s")) % tempFile.string()).str());
	}
}

void ParticlesManager::stripParticleDefFromStream(std::istream& input,
	std::ostream& output, const std::string& particleName)
{
	std::string line;
	boost::regex pattern("^[\\s]*particle[\\s]+" + particleName + "\\s*(\\{)*\\s*$");

	while (std::getline(input, line))
	{
		boost::smatch matches;

		// See if this line contains the particle def in question
		if (boost::regex_match(line, matches, pattern))
		{
			// Line matches, march from opening brace to the other one
			std::size_t openBraces = 0;
			bool blockStarted = false;

			if (!matches[1].str().empty())
			{
				// We've had an opening brace in the first line
				openBraces++;
				blockStarted = true;
			}

			while (std::getline(input, line))
			{
				for (std::size_t i = 0; i < line.length(); ++i)
				{
					if (line[i] == '{')
					{
						openBraces++;
						blockStarted = true;
					}
					else if (line[i] == '}')
					{
						openBraces--;
					}
				}

				if (blockStarted && openBraces == 0)
				{
					break;
				}
			}

			return; // stop right here, return to caller
		}
		else
		{
			// No particular match, add line to output
			output << line;
		}

		// Append a newline in any case
		output << std::endl;
	}
}

} // namespace particles
